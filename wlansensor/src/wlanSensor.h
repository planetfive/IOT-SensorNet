
#ifndef WLANSENSOR_H
#define WLANSENSOR_H

#include <Arduino.h>
#include <IPAddress.h>

// #define SPINDELCODE

#ifdef SPINDELCODE
extern float spTilt,spTemp,spPlato;
extern String do_spindel();
#endif

#define EEPROM_MAXCHARS 1024 // EEprom-Speicher für alle Modulparameter
#define PARAMETER_MAX 30

enum MODULPARAMETER { ModulName,Ssid,Passphrase,ModulMode,RemoteIP,RemotePort,
                      DeepSleep,TempAlias,TempSensor,Condition1,Action1,Condition2,
                      Action2,Condition3,Action3,Condition4,Action4,APpassphrase,
                      APip,APgateway,PSid1,PSid2,PSid3,PSid4,
                      OUT1_MODE,OUT2_MODE,VCC_Offset,Bssid,LocalIP,Gateway };

#define UDP_MAX_SIZE 300  // Maximale Anzahl Zeichen für UDP-Datagramm
// LED-Pin und Zustaende
#define LED_ON 1
#define LED_OFF 0
#define LED_PIN 5 // 2 --> Modul-LED
#define IN_PIN 12
#define OUT2_PIN 14
#define OUT1_PIN 4 // Reset-Pin für Logger-und Sensormode

#define NTP_TRIGGER_TIME 43200   // alle 12 Stunden wird ntp-time aktualisiert

// zum externen Messen, bitte auskommentieren und ALLES neu compilieren !
#define ADCMODE_VCC  // --> Betriebsspannung wird intern gemessen

extern char *parameter[PARAMETER_MAX];
extern IPAddress rIp;
//extern char*  mode;
extern int rP;

extern String localIp;
extern String udpEvent;

extern String sensorDaten;

extern String do_dht();
extern float dhtTemp,dhtHum;

//extern int seconds;
extern unsigned long ntpTime,ntpStartTime,verglTime,bootTime;

enum TEMP_SENSOR_MODEL { NO_SENSOR, DS1820, DHT_SENSOR, Spindel };
extern uint8_t tempSensorModel;

enum OUTPUT_MODE { NO_OUTPUT, SCHALTER, TASTER, DIMMER };
extern uint8_t out1_mode, out2_mode;
extern int8_t out1_wert, out2_wert;

extern boolean lifetimeFlag;

extern boolean debugFlag;
extern void debug(String msg);

extern void connectAsStation();



#endif
