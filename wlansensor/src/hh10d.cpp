/*
 HH10D humidity sensor
 Achtung, die Adresse des Moduls ist dez. 81. Fehlerhaft im Datenblatt.
*/

#include <Arduino.h>
#include "FS.h"
#include <Wire.h>
#include <Ticker.h>
#include "hh10d.h"
#include "wlanSensor.h"
#include "onewire.h"
#include "split.h"
#include "spiffs.h"

int offset_value, sensitiv_value;
volatile unsigned long freq_counter;


// ************* Offset und Sensitive-Werte des Moduls aus Datei einlesen ***********
String getHh10dFilename() {
  return String(F("/hh10dCalc.txt"));
}

void readHh10dFile() {
  String filename = getHh10dFilename();
  File f = SPIFFS.open(filename, "r");
  String line;
  if (!f) {
      Serial.println(filename + F(" kann nicht geoeffnet werden"));
  }
  else {
    while(f.available()){
      line = f.readStringUntil('\n');
      // Serial.println(line);
      if(line.length() == 0)
        continue;
      if(line.charAt(0)=='#')
        continue;
      if(line.charAt(0) >= '0') {
        Split split(line,',');
        uint8_t count = split.getCount();
        if(count == 2) {
          String s = split.getNext();
          sensitiv_value = s.toInt();
          // Serial.println(sensitiv_value);
          s = split.getNext();
          offset_value = s.toInt();
          // Serial.println(offset_value);
        }
        else
          Serial.println(filename + F(" fehlerhaft"));
        break;
      }
    }
    f.close();
  }
}
// *******************************************************************


boolean hh10dSetupFlag = false;
void setupInputPin() {
  if(hh10dSetupFlag == true) // Setup nur 1 Mal ausführen
    return;
  pinMode(IN_PIN, INPUT);
  // Serial.println(F("Pinsetup HH10D durchgefuehrt"));
  hh10dSetupFlag = true;
  readHh10dFile();
}

// ************ Offset und Sensitive-Wert aus Modul per i2c auslesen

int i2cRead2bytes(int deviceaddress, byte address) {
 // SET ADDRESS
 Wire.beginTransmission(deviceaddress);
 Wire.write(address); // address for sensitivity
 Wire.endTransmission();
 // REQUEST RETURN VALUE
 Wire.requestFrom(deviceaddress, 2);
 // COLLECT RETURN VALUE
 int rv = 0;
 for (int c = 0; c < 2; c++ )
  if (Wire.available())
    rv = rv * 256 + Wire.read();
 return rv;
}

boolean getCalibratedValues(){
  Wire.begin(IN_PIN, OUT2_PIN);  // (SDA,SCL) lt. AI-Thinker_ESP-12F.pdf GPIO2-SDA und GPIO14-SCL
  Wire.setClock(100000);
  Wire.setClockStretchLimit(2*230);
  int res = Wire.status();
  String s;
  if(res != I2C_OK) {
      s = String(F("I2C-ERROR:")) + String(res);
      Serial.println(s);
      return false;
    }
  sensitiv_value   =  i2cRead2bytes(81, 10); //Read sensitivity from EEPROM
  s = String(F("Sensitive=")) + String(sensitiv_value) + " \r\n";
  offset_value =  i2cRead2bytes(81, 12); //Same for offset
  s += String(F("Offset=")) + String(offset_value);
  Serial.println(s);
  return true;
}

String writeCalibratedValuesInFile() {
  String values = F("Sensortyp muß in Parametern auf <HH10D> gesetzt werden ... breche ab");
  if(tempSensorModel != HH10D_SENSOR) {
    Serial.println(values);
    return values;
  }
  if(getCalibratedValues()) {
    values = String(sensitiv_value) + ',' + String(offset_value);
    if(writeFile(getHh10dFilename().c_str(),values.c_str()) == false) {
      values = (F("Konnte Kalibrierdatei nicht erstellen"));
      Serial.println(values);
      return values;
    }
    return String(F("SensitiveWert=")) + String(sensitiv_value) + String(F(" OffsetWert=")) + String(offset_value);
  }
  return String(F("I2C-Fehler aufgetreten"));
}

// ****************************************************************

// **********   FOUT-Frequenz des Moduls bestimmen ***************

// Interruptroutine
void ISRcountFreq() {
  freq_counter++;
}

float calcHumidity(long freq){  // gibt relative Feuchte in Prozent zurück
  if(sensitiv_value < 1 || offset_value < 1) {
    Serial.println(F("Feuchtesensor HH10D: Offset -oder Sensitive-Wert nicht gesetzt"));
    return 0.0;
  }
  return (offset_value-freq)*sensitiv_value/4096.0; // Formel zur Berechnung des Feuchtewertes
}

Ticker hh10dTicker;
volatile boolean hh10dTimeFlag = false;
void setHh10dTimeFlag(){  // wird zur Beendigung der Messperiode gefeuert
  hh10dTimeFlag = true;
}

String do_hh10d(){
  if(tempSensorModel == HH10D_SENSOR){
    setupInputPin();
    attachInterrupt(digitalPinToInterrupt(DS_PIN), ISRcountFreq, RISING);
    unsigned long freq1 = freq_counter; //getHh10dFreq();
    hh10dTicker.once_ms(1000,setHh10dTimeFlag);
    while(hh10dTimeFlag == false) {
      yield(); // Warten bis Messperiode beendet ist
    }
    hh10dTimeFlag = false;
    unsigned long freq2 = freq_counter;
    detachInterrupt(digitalPinToInterrupt(DS_PIN));
    long freq = freq2 -freq1;
    //Serial.print(F("Frequenz:")); Serial.println(freq);
    float humidity = calcHumidity(freq);
    //Serial.print(F("Feuchtewert:")); Serial.println(humidity);
    return String("Rel._Feuchte:") + String(humidity);
  }
  Serial.println(F("Sensortyp muß in Parametern auf <HH10D> gesetzt werden ... breche ab"));
  hh10dSetupFlag = false;
  return String(F("Sensortyp H10D in Parameter einstellen"));
}
