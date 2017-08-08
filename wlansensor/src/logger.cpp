#include <Arduino.h>
#include <string.h>
#include <ESP8266WiFi.h>

#include "logger.h"
#include "DHT.h"
#include "wlanSensor.h"
#include "onewire.h"
#include "spiffs.h"
#include "setup.h"
#include "ntpClient.h"
#include "FS.h"
#include "parser.h"

extern "C" {
#include "user_interface.h"
#include "eagle_soc.h"
#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "os_type.h"
}

extern DHT dht;

void read_rtc(){
  struct RTC_DEF rtc;
  system_rtc_mem_read(64,&rtc,sizeof(rtc));
  String s;
  for(int i=0; i < 100;i++){
    s += String(rtc.mem[i]) + ' ';
  }
  //Serial.println(String(rtc.id)+String(F(" - Pos:"))+String(rtc.rtcPos)+String(F(" - Time:"))+
  //      String(rtc.time)+String(F(" - Filepos:"))+String(rtc.filepos));
}


void loggeDatenInFlash(RTC_DEF *rtc) {
  File file;
  if(SPIFFS.exists(LOGGERFILE))
    file = SPIFFS.open(LOGGERFILE, "r+");
  else
    file = SPIFFS.open(LOGGERFILE, "w");
  if(!file) {
    //Serial.println(F("Loggerfile oeffnen fehlgeschlagen"));
    return;
  }
  //if(file.seek(rtc->filepos,SeekSet)==false)
  //  Serial.println(F("Konnte Dateizeiger Loggerfile nicht positionieren"));
  String data = "";
  for(uint8_t i=0;i < rtc->rtcPos;i++) {
    // toDo: durch probieren einen offset von 3,6 Sekunden ermittelt -> noch einprogrammieren!
    data += printDate(rtc->time + (i*atoi(parameter[DeepSleep])) + 3) + String(" - ") + String(rtc->mem[i],1) + String("\n");
  }
  file.write((const uint8_t*)data.c_str(),data.length());
  rtc->filepos = file.position();
  if(rtc->filepos >= LOGGERFILE_MAX_LENGTH)
    rtc->filepos = 0;
  rtc->rtcPos = 0;
  system_rtc_mem_write(64,rtc,sizeof(*rtc));
  //Serial.print(F("Loggerfilegroesse:")); Serial.println(String(file.size()));
  file.close();
}

boolean loggeDaten() {
  struct RTC_DEF rtc;
  float t;
  const char* rtcId = "!ESP-RTC!";
  system_rtc_mem_read(64,&rtc,sizeof(rtc));
  if(strcmp(rtc.id,rtcId)) {  // Speicher zum 1.mal beschreiben
    //Serial.println(F("RTC-Speicher zum ersten mal beschreiben"));
    strcpy(rtc.id,rtcId);
    rtc.time = 0;
    rtc.rtcPos = 0;
    rtc.filepos = 0;
    for(int i=0;i < 100;i++){
      rtc.mem[i]=0;
    }
    system_rtc_mem_write(64,&rtc,sizeof(rtc));
    // toDo: LOGGERFILE nicht automatisch hier löschen
    // SPIFFS.remove(LOGGERFILE);
    return true; // nächstes Mal zeit holen
  }
  // Temperatur lesen
  if(tempSensorModel == DHT_SENSOR) {
    t = dht.getTemperature();
    // float h = dht.getHumidity();
  }
  else {
    do_ds18b20(true);
    t = ds1820[0].temperatur;
  }
  // Daten speichern
  if(rtc.rtcPos == 0){
    // Einwahl einleiten
    connectAsStation();
    uint8_t count = 0;
    boolean connectedFlag = true;
    while (WiFi.status() != WL_CONNECTED) {  // Warten auf Verbindung
      count++;
      if(count > 25) {
        connectedFlag = false;
        break;
      }
      delay(500);
    }
    unsigned long time = 0;
    if(connectedFlag) {
      time = getNtpTime();
      if(time == 0)
        time = getNtpTime();
    }
    if(time > 0)
      rtc.time = time;
    //Serial.print(F("NTP-Time:"));Serial.println(printDate(rtc.time));
  }
  rtc.mem[rtc.rtcPos++] = t;
  if(rtc.rtcPos >= 100) {
    loggeDatenInFlash(&rtc); // hier erfolgt auch ein mem_write()
    return true; // nächstes mal zeit holen
  }
  //rtc.rtcPos++;
  system_rtc_mem_write(64,&rtc,sizeof(rtc));
  return false;
}

void runLoggerMode() {
  // toDo --> VCC prüfen ?
  switch(tempSensorModel) {
    case DHT_SENSOR:
      dht.setup(DS_PIN,DHT::DHT22);
      delay(1000);
      break;
    case DS1820:
      break;
    default:
      //Serial.println(F("Kein Sensor konfiguriert - Bitte konfigurieren\r\n"));
      parameter[ModulMode][0] = 'M';
      parameter[ModulMode][1] = 0;
      writeModulParameter();
      delay(1000);
      WiFi.disconnect();
      ESP.restart();
      delay(5000);
  }
  //Serial.println(F("\r\nLogge Daten\r\n"));
  if(loggeDaten()==true)
    // 100 Werte wurden ins Flash geschrieben, RTC-Zyklus beginnt neu mit NTP-Zeit lesen
    ESP.deepSleep(atol(parameter[DeepSleep])*1000000);
  else
    ESP.deepSleep(atol(parameter[DeepSleep])*1000000, WAKE_RF_DISABLED);
  delay(5000);
}
