#include <Arduino.h>
#include <EEPROM.h>
#include <IPAddress.h>
#include "setup.h"
#include "wlanSensor.h"
#include "split.h"
#include "logger.h"
#include "DHT.h"
#include "onewire.h"

extern "C" {
#include "user_interface.h"
#include "eagle_soc.h"
#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "os_type.h"

extern struct rst_info resetInfo;
}


#define ERR_AREA_LEN 1024 // wegen maskierung des ringspeichers immer nur 128 oder 256 oder 512 oder 1024

uint8_t errArea[ERR_AREA_LEN];
uint16_t errPtr = 0;

void putErr(const char* err) {
  for(int i=0; i < ERR_AREA_LEN; i++) {
    if(*(err + i) == 0)
       break;
    errArea[errPtr & (ERR_AREA_LEN -1)] = (uint8_t)(*(err + i));
    errPtr++;
  }
}

String getErr(void) {
  String errString = "";
  uint16_t ptr = errPtr;
  for(int i=0; i < ERR_AREA_LEN; i++) {
    if(errArea[ptr & (ERR_AREA_LEN -1)] != 0)
       errString += (char)errArea[ptr & (ERR_AREA_LEN -1)];
    ptr++;
  }
  return errString;
}

double getVCC() {
  int v = ESP.getVcc();
  double offset = atof(parameter[VCC_Offset]);
  return (double)v / (double)1024 + offset;
}

void writeResetValue(uint16_t v) {
  system_rtc_mem_write(64,&v,sizeof(v)); // ResetValue ist erster Wert in rtc
}

uint16_t readResetValue() {
  uint16_t resetValue;;
  system_rtc_mem_read(64,&resetValue,sizeof(resetValue));  // ResetValue ist erster Wert in rtc
  return resetValue;
}

char *toCharArr(String s) {
  uint8_t len = s.length();
  char* string = new char[len + 1];
  strcpy(string,s.c_str());
  return string;
}


void printWelcome() {
  Serial.println(F("\r\n"));
  Serial.println(F("Folgende Aktionen lassen sich mit Tastendruck ausloesen:"));
  Serial.println(F("Press '?' --> Diese Hilfe"));
  Serial.println(F("      'p' --> Modulparameter eingeben"));
  Serial.println(F("      'e' --> EEprom loeschen mit 0xFF"));
  Serial.println(F("      'u' --> Dateiupload von einem Webserver in's SPIFFS-Filesystem"));
  Serial.println(F("      'q' --> Vorhandene Dateien im SPIFFS-Filesystem anzeigen"));
  Serial.println(F("      't' --> SPIFFS-Datei anzeigen"));
  Serial.println(F("      'x' --> SPIFFS-Datei loeschen"));
  Serial.println(F("      'd' --> Temperatur messen"));
  Serial.println(F("      'v' --> Batteriespannung messen"));
  Serial.println(F("      's' --> Tiefschlaf 30 Sekunden"));
  Serial.println(F("      'r' --> Modul neu starten (Restart nicht Reset)"));
  Serial.println(F("\r\n"));
}

void clearEeprom() {
  for (int i = 0; i < EEPROM_MAXCHARS; i++) {
    EEPROM.write(i,255);
  }
  EEPROM.commit();
}


void writeEeprom(String data) {
  int len = data.length();
  if(len >= (EEPROM_MAXCHARS - 1)) {
    Serial.println(F("EEPROM-Write abgebrochen, Daten zu gross"));
    debug(F("EEPROM-Write abgebrochen, Daten zu gross"));
    return;
  }
  for (int i = 0; i < len; i++) {
    EEPROM.write(i,data.charAt(i));
  }
  EEPROM.write(len,0);
  EEPROM.commit();
  // Serial.println("EEPROM geschrieben");
}


void writeModulParameter() {
  String s = "";
  for(int i=0; i< PARAMETER_MAX;i++){
    if(parameter[i] != NULL) {
       s += String(parameter[i]) + ',';
    }
    else {
      s += ',';
    }
  }
  s.remove(s.length()-1); // letztes komma entfernen
  writeEeprom(s);
}

boolean readModulParameter() {
  char mem[EEPROM_MAXCHARS+2],c;
  int i;
  for (i = 0; i < (EEPROM_MAXCHARS - 1); i++) {
    c = EEPROM.read(i);
    if(c >= ' ' && c <= 'z') {
      mem[i] = c;
    }
    else {
       mem[i] = 0;
       break;
    }
  }
  mem[EEPROM_MAXCHARS - 1] = 0;
  Split split(mem);
  i = 0;
  while(split.next()){
    String s = split.getNext();
    uint8_t len = s.length();
    parameter[i] = new char[len + 1];
    strcpy(parameter[i],s.c_str());
    i++;
  }
  for(uint8_t x=0; x < PARAMETER_MAX; x++) { // alle NULL-Pointer initialisieren
    if(parameter[x] == NULL) {
      parameter[x] = (char*)"";
    }
  }
  if(i < 4) //  name,ssid,passwort,mode müssen vorhanden sein
     return false;
  return true;
}

extern DHT dht;
void setTemperaturModel() {
  String tModel = String(parameter[TempSensor]);
  if(tModel.equals("DS1820")) {
    tempSensorModel = DS1820;
    return;
  }
  if(tModel.equals("DHT22")) {
    if(resetInfo.reason == 0) { // PowerON
      while(millis() < 1050) // Spannung muß mindestens 1 Sek anliegen, bevor der DHT22 betriebsbereit ist
        delay(10);
    }
    tempSensorModel = DHT_SENSOR;
    dht.setup(DS_PIN,DHT::AUTO_DETECT); // AUTO_DETECT ruft readSensor() auf, das heisst, beim nächsten lesen nach 1 sec gibt's gültige werte
    return;
  }
  if(tModel.equals("Spindel")) {
    tempSensorModel = Spindel;
    return;
  }
  if(tModel.equals("HH10D")) {
    tempSensorModel = HH10D_SENSOR;
    return;
  }
  tempSensorModel = NO_SENSOR;
}


void setOutputMode() {
  String oMode = String(parameter[OUT1_MODE]);
  if(oMode.equals("SCHALTER"))
    out1_mode = SCHALTER;
  else
    if(oMode.equals("TASTER"))
      out1_mode = TASTER;
    else
      if(oMode.equals("DIMMER"))
        out1_mode = DIMMER;
      else
        out1_mode = NO_OUTPUT;

  oMode = String(parameter[OUT2_MODE]);
  if(oMode.equals("SCHALTER"))
    out2_mode = SCHALTER;
  else
    if(oMode.equals("TASTER"))
      out2_mode = TASTER;
    else
      if(oMode.equals("DIMMER"))
        out2_mode = DIMMER;
      else
        out2_mode = NO_OUTPUT;
}


char* readSeriellParameter(char* name) { // Eingabe über die serielle Schnittstelle
   char *p;
   char c,arr[82];
   int len = 0;
   while (true) {
      while (!Serial.available())
         yield();
      c = Serial.read();
      if(c >= ' ' && c <='z'){ // gueltiges Zeichen
        Serial.write(c);      //Echo
        arr[len++] = c;
      }
      else {
         if (c == 13) {  // Enter, Parameter wird übernommen
            if(len == 0)
               return name;
            arr[len] = 0;
            p = new char[len+2];
            strcpy(p,arr);
            return p;
         }
         else {
            return NULL;
         }
      }
   }
}
