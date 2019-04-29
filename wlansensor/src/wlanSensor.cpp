#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>
#include <time.h>

#include "DHT.h"
#include "webServer.h"
#include "pushClient.h"
#include "FS.h"
#include "split.h"
#include "wlanSensor.h"
#include "setup.h"
#include "onewire.h"
#include "webServer.h"
#include "ntpClient.h"
#include "spiffs.h"
#include "parser.h"
#include "logger.h"


#include "hh10d.h"


extern "C" {
#include "user_interface.h"
#include "eagle_soc.h"
#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "os_type.h"

//void os_delay_us(int);
void wdt_feed();
};

// bewirkt, dass Pin ADC nicht mehr verwendet wird
// statt dessen wird intern mit ESP.getVcc() die Betriebsspannung gemessen
#ifdef ADCMODE_VCC
ADC_MODE(ADC_VCC);
#endif

PushClient pushClient1;
PushClient pushClient2;
PushClient pushClient3;
PushClient pushClient4;


uint8_t tempSensorModel = NO_SENSOR; // legt den Typ des Temperatursensors fest, entweder DHT22 oder DS1820 ist möglich
uint8_t out1_mode = NO_OUTPUT;
uint8_t out2_mode = NO_OUTPUT;
int8_t out1_wert = 0;
int8_t out2_wert = 0;
boolean pulseOut1 = false, pulseOut2 = false;
boolean debugFlag = true;

// Klasse für DHT-Temperatur und Feuchtesensor
DHT dht;

// hält den letzten Output des jeweiligen Sensoraufrufes bereit ( z.B. doSpindel(),doDSxxxx(). doDHT() )
String sensorMessage;

Parser actionParser; // Parst die Bedingung und Aktions-Paare

char *parameter[PARAMETER_MAX];  // alle Modulparameter, siehe wlanSensor.h

// Laufzeit-Modulvariablen
IPAddress rIp; // remoteIP als Array
int rP; // remotePort als int
String localIp; // eigene Modul-IP, wie per DHCP erhalten ( im WiFi-Station-Mode )

boolean actionFlag; // wird alle paar Minuten zum Triggern der Aktions-Funktion gesetzt
boolean triggerLifetimeFlag;

String sensorDaten; // enthalten die empfangenen Sensordaten
String udpEvent;  // wird als Befehl beim Melden eines Sensor-Moduls an selbiges abgesetzt
boolean lifetimeFlag = false; // Wird beim Überschreiben auf true gesetzt

/* DNS-Gedöns
#define DNS_PORT 53
boolean dns_server; // falls DNS-Server aktiv, dann true
*/

// WiFi-Gedöns
WiFiUDP udp;

// Udp-Gedöns
boolean sensorFlag;  // true, wenn das Modul als sensor arbeitet
char packetBuffer[UDP_MAX_SIZE + 2]; //  UDP_TX_PACKET_MAX_SIZE buffer to hold incoming packet

// Time-Gedöns
//unsigned long ntpTime,ntpStartTime,verglTime,bootTime;
time_t ntpTime,ntpStartTime,verglTime,bootTime;

// Die Klasse AliasParser ersetzt den Romcode der Temperatursensoren durch das festgelegte Alias
AliasParser aliasParser;

// Web-Server initialisieren
ESP8266WebServer server(80);    // Serverport  hier einstellen

// DNS-Server initialisieren
// DNSServer dnsServer;

// ********************************* functions *******************************************************

void debug(String msg) {
  if(debugFlag == false)
    return;
  putErr(msg.c_str());
  putErr("<br>");
}

void testeSensorDataLifetime() {
  if(sensorDaten.length()== 0) {  // nix zu tun
      return;
  }
  String line,tempDaten;
  Split split(sensorDaten,'\n'); // einzelne Moduleinträge
  while(split.next()) {
    line = split.getNext();
    String tm = Split(line).getIndex(0); // Zeitfeld
    String nm = Split(line).getIndex(1); // Namefeld
    String sleeptime = Split(line).getIndex(6);  // Lifetime
    unsigned long t = (unsigned long)tm.toInt();
    unsigned long lt = (unsigned long)sleeptime.toInt();
    if(ntpTime < (t + (lt + lt + (lt/10)))) { // doppelte sleeptime + 10% sleeptime = lifetime
      // lifetime wurde noch nicht überschritten
      if(tempDaten.length() > 0)
        tempDaten += "\n";
      tempDaten += line; // deshalb Eintrag übernommen
    }
    else {
      // Lifetime überschritten toDo: evtl. präziser formulieren
      lifetimeFlag = true;
      debug(printDate(ntpTime) + " " + nm + String(F(":Lifetime ueberschritten")));
    }
  }
  yield();
  sensorDaten = tempDaten;
}

void insertSensorData(String sw) { // empfangene Sensordaten sammeln in globalem String "sensorDaten"
  boolean newModulFlag = true;
  if(sensorDaten.length()== 0) {  // 1.Sensoreintrag, deshalb gleich schreiben
      sensorDaten = sw;
      return;
  }
  String newName = Split(sw).getIndex(1); // Modulname vom neuen Eintrag (Standard-delimiter ',')
  String modulname;
  String line,tempDaten;
  Split split(sensorDaten,'\n'); // einzelne Moduleinträge
  while(split.next()) {
    line = split.getNext();
    modulname = Split(line).getIndex(1);
    if(tempDaten.length() > 0)
      tempDaten += "\n";
    if(newName.equals(modulname)) { // Modulname ist vorhanden, deshalb neuen Eintrag übernehmen
      tempDaten += sw; // neuer Eintrag wird geschrieben
      newModulFlag = false;
    }
    else { //
      tempDaten += line;
    }
    yield();
  }
  if(newModulFlag == true) {
    tempDaten += "\n" + sw;
  }
  sensorDaten = tempDaten;
}

/*
void insertSensorData(String sw) { // empfangene Sensordaten sammeln in globalem String "sensorDaten"
  boolean flag = false;
  if(sensorDaten.length()== 0) {  // 1.Sensoreintrag schreiben
      sensorDaten = sw;
      return;
  }
  String newName = Split(sw).getIndex(1); // Modulname vom neuen Eintrag (Standard-delimiter ',')
  String modulname,sleeptime,tm;
  String line,tempDaten;
  Split split(sensorDaten,'\n');
  while(split.next()) {
    line = split.getNext();
    modulname = Split(line).getIndex(1);
    if(newName.equals(modulname) && (flag == false)) { // Modulname ist vorhanden, deshalb neuen Eintrag übernehmen
      if(tempDaten.length() > 0)
        tempDaten += "\n";
      tempDaten += sw; // neuer Eintrag wird geschrieben
      flag = true; // jetzt werden nur noch die vorhandenen Werte übernommen
    }
    else { //
      // hier noch Lifetime überprüfen
      tm = Split(line).getIndex(0); // Zeitfeld
      sleeptime = Split(line).getIndex(6);  // Lifetime
      unsigned long t = (unsigned long)tm.toInt();
      unsigned long lt = (unsigned long)sleeptime.toInt();
      if(ntpTime < (t + (lt + lt + (lt/10)))) { // doppelte sleeptime + 10% sleeptime = lifetime
        // lifetime wurde noch nicht überschritten
        if(tempDaten.length() > 0)
          tempDaten += "\n";
        tempDaten += line; // deshalb Eintrag übernommen
      }
      else {
        // Lifetime überschritten toDo: evtl. präziser formulieren
        lifetimeFlag = true;
      }
    }
    yield();
  }
  if(flag == false) {
     if(tempDaten.length() > 0)
        tempDaten += "\n";
     tempDaten += sw; // Kein Eintrag mit diesem Modulnamen vorhanden, deshalb schreiben
  }
  sensorDaten = tempDaten;

}
*/

String testUdpEvent(String modulname) { // testet, ob für dieses Sensor-Modul ein Event vorliegt
  String event;
  String tmpEvent;
  String retEvent = "";
  if(udpEvent.length() == 0)
     return "";
  Split split(udpEvent,';');
  while(split.next()) {
    event = split.getNext();
    if(Split(event).getIndex(0).equals(modulname))
       retEvent = Split(event).getIndex(1);
    else {
       if(tmpEvent.length() > 0)
          tmpEvent += ";" + event;
       else
          tmpEvent = event;
    }
  }
  udpEvent = tmpEvent;
  return retEvent;
}

String getSensorDaten() { // ermittelt die Sensordaten des Moduls
    yield();
    switch(tempSensorModel) {
      case DS1820:
         sensorMessage = do_ds18b20(true);
         sensorMessage = aliasParser.substitute(sensorMessage); // ersetzt romcode durch alias
         break;
      case DHT_SENSOR:
         while(millis() < 3100) // 1 Sekunde für Stabilisierung und 2 Sek. für 2.Polling
          delay(10);
         sensorMessage = do_dht();
         break;
      case HH10D_SENSOR:
        sensorMessage = do_hh10d();
        break;
#ifdef SPINDELCODE
      case Spindel:
         sensorMessage = do_spindel();
         break;
#endif
      default:
         sensorMessage = String(F("kein Sensor vorhanden"));
    }
    yield();
#ifdef ADCMODE_VCC
    double volt = getVCC();
    String s = String(volt,2);
#else
// #ifndef ADCMODE_VCC
    uint16_t v = system_adc_read();
    double volt = 3.6 / 1024 * v;
    String s = String(volt,2);
#endif
    String msg = String(parameter[ModulName]) + String(",") + String(localIp) + String(",") + sensorMessage + String(",") + s + " V";
    if(digitalRead(IN_PIN))
      msg += ",HI,";
    else
      msg += ",LOW,";
    int sleeptime = atoi(parameter[DeepSleep]);
    msg += String(sleeptime);
    if(msg.length() > (UDP_MAX_SIZE -20))
      msg = msg.substring(0,UDP_MAX_SIZE -20); // Empfangsmodul muß noch Zeit hinzufügen
    return msg;
}

// *********** Action-Stuff ***********************

void doPushService(PushClient &client, const char *ss) {
  //Serial.println(F("Action PushService aufgerufen:"));
  //String t = "";
  if(ss != NULL) {
    String msg = ss;
    if(msg.equals("Vcc")) {  // Betriebsspannung
      #ifdef ADCMODE_VCC
          double volt = getVCC();
      #endif
      #ifndef ADCMODE_VCC
          uint16_t v = system_adc_read();
          double volt = 3.6 / 1024 * v;
      #endif
      msg = String(volt,3);
    }
    else {  // Lifetime
      if(msg.equals("Lifetime")) {
        msg = "Sensor_hat_Lifetime_ueberschritten";
      }
      else {
        if(msg.equals("Dht")) { // DHTXX-Sensor
          //msg = do_dht();
          msg = sensorMessage;
        }
        else {
          if(msg.equals("Spindel")) { // DHTXX-Sensor
            msg = sensorMessage;
          }
          else {  // DS1820-Sensoren
            float t;
            if(getDs1820Temp(msg.c_str(), &t)) {
              String alias = aliasParser.getValue(msg); // gibt alias vom romcode zurück
              if(alias.length() > 0)
                msg = alias + "=" + String(t);
              else
                msg += "=" + String(t);
            }
            else
              //Serial.println("Sensor nicht gefunden");
              debug(String(F("PushService:keine gültige Message-Variable")));
              // toDo:  hier könnte auch der Messagestring einfach so ohne Interpretation übernommen werden
          }
        }
      }
    }
    //debug(msg);
    client.pushMessage(msg.c_str());
    }
    else
      client.pushMessage();
    if(parameter[ModulMode][0] == 'S') { // im Sensormode muß der Pushservice direkt gehandled werden
      while(1) {
        yield();
        client.handle();
        if(client.getStatus() != Progress) {
          break;
        }
      }
    }
}

void parseAction(const char *condition, const char *action, const char * ss = NULL) {
    int cond = actionParser.parse(condition);
    if(! actionParser.getErr()) { // !0 ist Fehlerfall
      if(strcmp(action,"PushService1")==0 && cond) {
        if( (pushClient2.getStatus() != Progress) &&
            (pushClient3.getStatus() != Progress) &&
            (pushClient4.getStatus() != Progress) ) {
          doPushService(pushClient1,ss);
        }
        return;
      }
      if(strcmp(action,"PushService2")==0 && cond) {
        if( (pushClient1.getStatus() != Progress) &&
            (pushClient3.getStatus() != Progress) &&
            (pushClient4.getStatus() != Progress) ) {
          doPushService(pushClient2,ss);
        }
        return;
      }
      if(strcmp(action,"PushService3")==0 && cond) {
        if( (pushClient1.getStatus() != Progress) &&
            (pushClient2.getStatus() != Progress) &&
            (pushClient4.getStatus() != Progress) ) {
          doPushService(pushClient3,ss);
        }
        return;
      }
      if(strcmp(action,"PushService4")==0 && cond) {
        if( (pushClient1.getStatus() != Progress) &&
            (pushClient2.getStatus() != Progress) &&
            (pushClient3.getStatus() != Progress) ) {
          doPushService(pushClient4,ss);
        }
        return;
      }
      // **************** test test test ***********************************
      // abhängig vom input pin , udp-befehl an ESP-Server senden
      if(strncmp(action,"pulseOut",8) == 0) { // pulseOutX schaltet outX auf dem RemoteServer im Sekundentakt um
        IPAddress uip(rIp);  // IP-Sendeadresse
        // ! --> kennzeichnet das UDP-Datagramm als Befehl aus
        // P --> Befehl für Pulse
        // 11 --> 1.Zahl = out1 ( 2 = out2 ) , 2.Zahl ist Einschalten ( 0 = Ausschalten )
        char udp_befehl[5];
        udp_befehl[0] = '!';
        udp_befehl[1] = 'P';
        udp_befehl[2] = *(action+8);
        udp_befehl[4] = 0;
        if(cond > 0) {
          udp.beginPacket(uip,rP);
          udp_befehl[3] = '1';
          udp.print(udp_befehl); // schaltet out1 auf dem RemoteServer auf puslseOn
          udp.endPacket();
        }
        else {
          udp.beginPacket(uip,rP);
          udp_befehl[3] = '0';
          udp.print(udp_befehl); // schaltet out1 auf puslseOff
          udp.endPacket();
        }
        return;
      }
      // ***************** ende test ****************************************
      if(parameter[ModulMode][0] == 'S') // im sensormode nur pushnachricht und pulseOut vor dem schlafen sinnvoll
        return;
      // outXH und outXL implementieren
      if(strcmp(action,"out1H") == 0) {
        if(cond > 0){  // nur wenn cond > 0 wird high ausgegeben
          switch(out1_mode){
            case SCHALTER:
              out1_wert = -1;
              digitalWrite(OUT1_PIN,1);
              break;
            case TASTER:
              out1_wert = 3;
              digitalWrite(OUT1_PIN,1);
              break;
            case DIMMER:
              out1_wert = (cond < 100) ? cond : 100;
              if(out1_wert)
                analogWrite(OUT1_PIN,out1_wert);
              break;
          }
        }
        return;
      }
      if(strcmp(action,"out1L") == 0) {
        if(cond > 0){  // nur wenn cond > 0 wird low ausgegeben
          out1_wert = 0;
          digitalWrite(OUT1_PIN,0);
        }
        return;
      }
      if(strcmp(action,"out2H") == 0) {
        if(cond > 0){  // nur wenn cond > 0 wird high ausgegeben
          switch(out2_mode){
            case SCHALTER:
              out2_wert = -1;
              digitalWrite(OUT2_PIN,1);
              break;
            case TASTER:
              out2_wert = 3;
              digitalWrite(OUT2_PIN,1);
              break;
            case DIMMER:
              out2_wert = (cond < 100) ? cond : 100;
              if(out2_wert)
                analogWrite(OUT2_PIN,out2_wert);
              break;
          }
        }
        return;
      }
      if(strcmp(action,"out2L") == 0) {
        if(cond > 0){  // nur wenn cond > 0 wird low ausgegeben
          out2_wert = 0;
          digitalWrite(OUT2_PIN,0);
        }
        return;
      }
      if(strcmp(action,"out1") == 0) {
        switch(out1_mode){
          case SCHALTER:
            out1_wert = (cond > 0) ? -1 : 0;
            digitalWrite(OUT1_PIN,out1_wert != 0);
            break;
          case TASTER:
            out1_wert = (cond > 0) ? 3 : 0;
            digitalWrite(OUT1_PIN,out1_wert != 0);
            break;
          case DIMMER:
            if(cond < 0)
              out1_wert = 0;
            else if(cond > 100)
              out1_wert = 100;
            else
              out1_wert = cond;
            analogWrite(OUT1_PIN,out1_wert);
            break;
          default:
            out1_wert = 0;
            digitalWrite(OUT1_PIN,LOW);
        }
        return;
      }
      if(strcmp(action,"out2") == 0) {
         //digitalWrite(OUT1_PIN,cond > 0);
        switch(out2_mode){
          case SCHALTER:
            out2_wert = (cond > 0) ? -1 : 0;
            digitalWrite(OUT2_PIN,out2_wert != 0);
            break;
          case TASTER:
            out2_wert = (cond > 0) ? 3 : 0;
            digitalWrite(OUT2_PIN,out2_wert != 0);
            break;
          case DIMMER:
            if(cond < 0)
              out2_wert = 0;
            else if(cond > 100)
              out2_wert = 100;
            else
              out2_wert = cond;
            analogWrite(OUT2_PIN,out2_wert);
            break;
          default:
            out2_wert = 0;
            digitalWrite(OUT2_PIN,LOW);
        }
        return;
      }
    }
    else
      debug(F("parseAction:Bedingung enthaelt Fehler!"));
       //Serial.println(F("Condition = Fehlerfall"));

}

// ************************************************

void handleAction() {
  // Serial.println("Teste Conditions");
  /*
  if((strlen(parameter[Condition1]) > 0) && (strlen(parameter[Action1]) > 0)) {
    Split split(parameter[Action1],':');
    switch(split.getCount()) {
      case 1:
        parseAction(parameter[Condition1],parameter[Action1]);
        break;
      case 2:
        parseAction(parameter[Condition1],split.getIndex(0).c_str(),split.getIndex(1).c_str());
    }
    yield();
  }
  if((strlen(parameter[Condition2]) > 0) && (strlen(parameter[Action2]) > 0)) {
    Split split(parameter[Action2],':');
    switch(split.getCount()) {
      case 1:
        parseAction(parameter[Condition2],parameter[Action2]);
        break;
      case 2:
        parseAction(parameter[Condition2],split.getIndex(0).c_str(),split.getIndex(1).c_str());
    }
    yield();
  }
  if((strlen(parameter[Condition3]) > 0) && (strlen(parameter[Action3]) > 0)) {
    Split split(parameter[Action3],':');
    switch(split.getCount()) {
      case 1:
        parseAction(parameter[Condition3],parameter[Action3]);
        break;
      case 2:
        parseAction(parameter[Condition3],split.getIndex(0).c_str(),split.getIndex(1).c_str());
    }
    yield();
  }
  if((strlen(parameter[Condition4]) > 0) && (strlen(parameter[Action4]) > 0)) {
    Split split(parameter[Action4],':');
    switch(split.getCount()) {
      case 1:
        parseAction(parameter[Condition4],parameter[Action4]);
        break;
      case 2:
        parseAction(parameter[Condition4],split.getIndex(0).c_str(),split.getIndex(1).c_str());
    }
    yield();
  }
  */

  char *condition, *action;
  for(uint8_t i=0; i < 4; i++) {
    switch(i) {
      case 0:
        condition = parameter[Condition1];
        action = parameter[Action1];
        break;
      case 1:
        condition = parameter[Condition2];
        action = parameter[Action2];
        break;
      case 2:
        condition = parameter[Condition3];
        action = parameter[Action3];
        break;
      case 3:
        condition = parameter[Condition4];
        action = parameter[Action4];
        break;
    }
    if((strlen(condition) > 0) && (strlen(action) > 0)) {
    Split split(action,':');
    switch(split.getCount()) {
      case 1:
        parseAction(condition,action);
        break;
      case 2:
        parseAction(condition,split.getIndex(0).c_str(),split.getIndex(1).c_str());
        break;
      default:
        debug(String(F("Fehler in handleAction() "))+String(action));
    }
    yield();
    }
  }



}


void runSensorMode() {
  //double volt;
  //int vcc;
  if(udp.begin(rP)==0) {
     // ("\r\nScheisse, Fehler beim UdpStart <udp.beginn()> - mach nen reboot");
     Serial.println(F("Fehler beim UDP-Start"));
     WiFi.disconnect();
     delay(1000);
     system_deep_sleep(120*1000000); // 2 min
     delay(5000);
  }
  yield();
    // Serial.println("Send a UDP-Packet");
    String msg = getSensorDaten();
    IPAddress uip(rIp);  // IP-Sendeadresse
    udp.beginPacket(uip,rP);
    udp.print(msg);
    Serial.println("Sendestring:" + msg);
    udp.endPacket();
    yield();
    String answer = "";
    for(int j=0; j < 200; j++) { // eine eventuelle Antwort vom Webserver abwarten
       int packetSize = udp.parsePacket();
       if(packetSize) {
          // read the packet into packetBufffer
          udp.read(packetBuffer,UDP_MAX_SIZE);
          /*
          Serial.print("Received packet of size ");
          Serial.println(packetSize);
          Serial.print("From ");
          IPAddress remote = udp.remoteIP();
          for (int i = 0; i < 4; i++) {
             Serial.print(remote[i], DEC);
             if (i < 3) {
                Serial.print(".");
             }
          }
          Serial.print(", port ");
          Serial.println(udp.remotePort());
          Serial.print("Contents: ");
          */
          answer = packetBuffer;
          //Serial.println(String(packetBuffer));
          //Serial.println(answer);
          if(answer.startsWith("modulMode=")) {    //  <------  toDo Modulmode kann aus mehreren Zeichen bestehen !!!!
            String mode = answer.substring(10);
            uint8_t len = mode.length();
            parameter[ModulMode] = new char[len + 1];
            strcpy(parameter[ModulMode],mode.c_str());
            // Serial.println(F("Aendere ModulMode:"));
            // Serial.println(c);
            // parameter[ModulMode][0]=c;
            writeModulParameter();
            WiFi.disconnect();
            delay(1000);
            system_deep_sleep(1000000);
            delay(5000);
          }
          break;
       }
       delay(10);
     }
     /* ******** zu testzwecken verwendet ***********
     if(answer.length()==0)
      Serial.println(F("Keine Antwort vom Webserver erhalten"));
     */
     handleAction();
     // Serial.println("Now sleep");
     WiFi.disconnect();
     delay(1000);
     system_deep_sleep(atol(parameter[DeepSleep])*1000000);
     // ESP.deepSleep(uint32_t time_us, RFMode mode = RF_DEFAULT); // time=0 --> aufwachen nur durch pulse reset
     delay(5000);
}


void configStation() {
  if((strlen(parameter[LocalIP]) > 0) && (strlen(parameter[Gateway]) > 0)) {
      // Modul mit statischen Adressen konfigurieren
        IPAddress lip;
        lip.fromString(parameter[LocalIP]);
        IPAddress gw;
        gw.fromString(parameter[Gateway]);
        IPAddress msk = { 255,255,255,0 };
        WiFi.config(lip,gw,msk);
      }
  WiFi.hostname(parameter[ModulName]);
  if(strlen(parameter[Bssid]) == 0)
    WiFi.begin(parameter[Ssid], parameter[Passphrase]);
  else {
    // erweiterte Konfiguration mit BSSID
    Serial.print(F("BSSID:"));
    Split split(parameter[Bssid],':');
    uint8_t bssid[6];
    uint8_t i = 0;
    while(split.next()) {
      String s = split.getNext();
      uint8_t n = (uint8_t)strtol(s.c_str(),(char **)NULL,16);
      Serial.print(n);Serial.print("-");
      if(i < 6) {
        bssid[i] = n;
        i++;
      }
      else
        break;
    }
    Serial.println();
    WiFi.begin(parameter[Ssid], parameter[Passphrase],0,bssid);
  }
}

void connectAsStation() {
  WiFi.mode(WIFI_STA);
  configStation();
}

void connectAsAP(WiFiMode m) {
  debug(F("Starte Modul im AccessPoint-Mode"));
  IPAddress APIp,gw;
  if(strlen(parameter[APip]) == 0)
    APIp = { 192,168,4,1 };
  else
    APIp.fromString(parameter[APip]);
  if(strlen(parameter[Gateway]) == 0)
    gw = { 0,0,0,0 };
  else
    gw.fromString(parameter[Gateway]);
  IPAddress msk = { 255,255,255,0 };
  WiFi.mode(m);
  if(m == WIFI_AP_STA) { // MixedMode verwenden
    configStation();
  }
  WiFi.softAPConfig(APIp,gw,msk);
  if(strlen(parameter[APpassphrase]) == 0) {
    debug(F("AP-SSID = ESP-AP-Free"));
    WiFi.softAP("ESP-AP-Free");
  }
  else {
    debug(F("AP-SSID = ESP-AP"));
    WiFi.softAP("ESP-AP",parameter[APpassphrase]);
  }
  /*
  if(m == WIFI_AP_STA) { // MixedMode verwenden
    configStation();
  }
  */
  delay(500);
}

// ************  DHT-Sensordaten lesen und Vars setzen ***************************
float dhtTemp,dhtHum;
String do_dht() {
  dhtTemp = dht.getTemperature();
  dhtHum = dht.getHumidity();
  String s = String(F("Temp:")) + String(dhtTemp) + String(F("__Feuchte:")) + String(dhtHum);
  return s;
}
// *********** Ende DHT *************************************************************


// ************ Ticker-Stuff ***********************
void setSec() {
  ntpTime++;
}

void setSensorFlag() {
  sensorFlag = true;
}

void setActionFlag() {
  actionFlag = true;
}

void setLifetimeTriggerFlag() {
  triggerLifetimeFlag = true;
}

Ticker secTicker;
Ticker sensorTicker;
Ticker actionTicker;
Ticker lifetimeTicker;
// **************************************************


void setup() {

  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LED_OFF);
  // **** temporär umgestellt !!!!!! pinMode(IN_PIN,INPUT_PULLUP);
  pinMode(IN_PIN,INPUT);

  sensorFlag = false; // aktiviert im W-Mode die Anzeige der eigenen Sensordaten

  Serial.begin(115200);         // Serielle Schnittstelle initialisieren

  //system_set_os_print(1);
  //Serial.setDebugOutput(true);

  Serial.print(F("\r\n\r\nBoote ....\r\n\r\n"));
  debug(F("Boote ...."));

  boolean doubleReset;
  if(readResetValue() == RESET_VALUE) {
    // das Modul wurde doppelt resetet
    Serial.println(F("2.Modulreset"));
    doubleReset = true;
    writeResetValue(0);
  }
  else {
    Serial.print(F("Resetdetektion..."));
    writeResetValue(RESET_VALUE);
    delay(700); // warte, ob ein reset eintritt
    doubleReset = false; // nix, dann mach mer weiter
    writeResetValue(0);
    Serial.println(F(" fertig\r\n"));
  }

  if(wifi_station_get_auto_connect()) {
    Serial.println(F("Autoconnect ist gesetzt! - deaktivere es\r\n"));
    wifi_station_set_auto_connect(0);
  }

  EEPROM.begin(EEPROM_MAXCHARS); // EEPROM initialisieren
  //Serial.println("Read EEPROM ....");

  if(readModulParameter()==false) { // noch nicht konfiguriert !
     parameter[ModulMode] = (char*)"A"; // temporäre Einstellung - konfiguriert das Modul als AccessPoint
     parameter[RemotePort] = (char*)"15000";
     Serial.print(F("EEPROM noch nicht konfiguriert\r\n"));
     debug(F("EEPROM noch nicht konfiguriert"));
  }

  if(doubleReset) {
    // ein doppelter Reset setzt Mixed-Mode
    parameter[ModulMode] = (char*)"M";
  }

  setTemperaturModel(); // setzt Variable "tempSetModel" und initialisiert bei bedarf dht-sensor

  if(SPIFFS.begin() == false) {
      Serial.println(F("SPIFFS fail\r\n"));
      debug(F("SPIFFS-Filesystem einrichten fehlgeschlagen!"));
  }
  // Testen, ob Mode-Reset-Taster des Sensormodules gedrückt wurde
  // und ob Spannung im Grenzbereich
  if((parameter[ModulMode][0] == 'L')||(parameter[ModulMode][0] == 'S')) {
    pinMode(OUT1_PIN,INPUT_PULLUP);
    if(digitalRead(OUT1_PIN)==LOW) {
      // Falls out1 als Ausgang beschaltet ist, geht das Modul höchstwahrscheinlich
      // auch in den M-Mode
      digitalWrite(LED_PIN,LED_ON);
      if(parameter[ModulMode][0] == 'L') { // letzten Daten des RTC schreiben
        struct RTC_DEF rtc;
        system_rtc_mem_read(64,&rtc,sizeof(rtc));
        loggeDatenInFlash(&rtc);
      }
      parameter[ModulMode][0] = 'M';
      parameter[ModulMode][1] = 0;
      writeModulParameter();
      ESP.deepSleep(10,RF_DEFAULT);
      delay(5000);
    }
    #ifdef ADCMODE_VCC
    double volt = getVCC();
    if(volt < 2.9) {
      Serial.println(F("Spannung im Grenzbereich - schlafe ewig"));
      // toDo --> ist 2.9V der richtige wert oder wie bestimme ich das????
      system_deep_sleep(0); // unendlich schlafen wegen niedriger spannung
      delay(5000);
    }
    #endif
  }
  else {
    // evtl. bestehendes Sensor-Steuer-File "Fail" löschen
    SPIFFS.remove(F("/Fail"));
  }

  if(parameter[ModulMode][0] == 'L') {
    runLoggerMode();
    // hier gehts nicht weiter
  }

  // **** temporär deaktiviert um Bootgrund zu überwachen
  //if(parameter[ModulMode][0] != 'S')
  Serial.println(getModulInfo());

  // Push-Service initialisieren
  if(pushClient1.setId(parameter[PSid1]))
    debug(F("PushClient1 initialisiert"));
  if(pushClient2.setId(parameter[PSid2]))
    debug(F("PushClient2 initialisiert"));
  if(pushClient3.setId(parameter[PSid3]))
    debug(F("PushClient3 initialisiert"));
  if(pushClient4.setId(parameter[PSid4]))
    debug(F("PushClient4 initialisiert"));

  rP = atoi(parameter[RemotePort]); // IP + Portstrings für Programm-Laufzeit umsetzen
  rIp.fromString(parameter[RemoteIP]);

  // Output-Pins konfigurieren
  setOutputMode(); // setzt die variablen outx_mode
  if(out1_mode != NO_OUTPUT) {
    pinMode(OUT1_PIN,OUTPUT); // GPIO4
    digitalWrite(OUT1_PIN,LOW);
    debug(F("Output1 initialisiert (GPIO4)"));
  }
  if(out2_mode != NO_OUTPUT) {
    pinMode(OUT2_PIN,OUTPUT); // GPIO14
    digitalWrite(OUT2_PIN,LOW);
    debug(F("Output2 initialisiert (GPIO14)"));
  }

  // dns_server = false; // DNS-Server wird momentan nicht verwendet

  switch(parameter[ModulMode][0]) {
    case 'M':
       connectAsAP(WIFI_AP_STA); // MixedMode
       break;
    case 'A':
      connectAsAP(WIFI_AP); // nur AP-Mode
      break;
    default: // W + S
      connectAsStation();
  }


  // WiFi.printDiag(Serial);       // Wlan Daten seriell ausgeben
  if(parameter[ModulMode][0] != 'A') { // alle Modes außer reinem AP, der keine Einwahl benötigt
    Serial.print(F("\r\n\r\n!!!!!!!! Achtung: Mit Taste 'k' Wifi-Einwahl abbrechen und in AP-Modus wechseln\r\n"));
    uint8_t count = 0;
    while (WiFi.status() != WL_CONNECTED) {  // Warten auf Verbindung
      count++;
      if(count > 25) {
         if(parameter[ModulMode][0] == 'S') { // Als Sensor
            // Anzahl Abbrüche zählen und dann 8 Std schlafen
            digitalWrite(LED_PIN,LED_OFF);
            Serial.println(F("\r\nKonnte keine Verbindung herstellen"));
            String cont = getFileContents(F("/Fail"));
            if(cont.length() > 0) {
              Serial.println(String(F("Fileinhalt:"))+cont);
              if(cont.charAt(0) == '1') {
                 Serial.println(F("3.Versuch erfolgt in 1 Minute"));
                 writeFile(String(F("/Fail")).c_str(),"2");
                 WiFi.disconnect();
                 delay(1000);
                 system_deep_sleep(60*1000000); // Warte mal ne Minute ab
                 delay(5000);
              }
              else {
                // schlafe 1 Std, viel mehr ist technisch nicht möglich
                Serial.println(F("Wieder erfolglos, probiers in 1 Std nochmal"));
                deleteFile(String(F("/Fail")).c_str());
                WiFi.disconnect();
                delay(1000);
                system_deep_sleep((uint32_t)3600000000);
                delay(5000);
              }
            }
            Serial.println(F("2.Versuch erfolgt in 1 Minute"));
            writeFile(String(F("/Fail")).c_str(),"1");
            WiFi.disconnect();
            delay(1000);
            system_deep_sleep(60*1000000); // Warte mal ne Minute ab
            delay(5000);
         }
         else {
          if(parameter[ModulMode][0] == 'W') { // nur im Station-Mode auf AP-Mode umschalten nötig
            parameter[ModulMode][0] = 'A';
            parameter[ModulMode][1] = 0;
            connectAsAP(WIFI_AP);
          }
         }
         break;
      }
      digitalWrite(LED_PIN,LED_ON);
      if(Serial.available()) {
        char c;
        c = Serial.read();
        if(c == 'k') {
          parameter[ModulMode][0] = 'A'; // zum Konfigurieren AP_Mode
          connectAsAP(WIFI_AP);
          break;
        }
      }
      delay(2);
      Serial.print(".");
      digitalWrite(LED_PIN,LED_OFF);
      delay(498);
    }
  }

  // eigene IP
  localIp = ipToString(WiFi.localIP());

  Serial.println();
  Serial.println(getNetworkInfo());

  secTicker.attach(1,setSec); // Sekunden hochzählen

  /* DHT-Sensor initialisieren Achtung: Sensor muß vor der initialisierung mind. 1 sek Spannung haben (evtl. delay(1000))
  if(tempSensorModel == DHT_SENSOR)
     dht.setup(DS_PIN,DHT::AUTO_DETECT);
  */

  aliasParser.parseAliase(parameter[TempAlias]);

  if(parameter[ModulMode][0]=='S') {
      // WLAN-Sensormode
      runSensorMode();
      // hier gehts nicht weiter, weil Sensor legt sich schlafen
  }

  // ab hier nur noch W,A,M - Modes
  if( (parameter[ModulMode][0]!='W')&&(parameter[ModulMode][0]!='A')&&(parameter[ModulMode][0]!='M'))
     parameter[ModulMode][0]='W';

  /*
  if(dns_server == true) {  // toDo dns_server nur im AP_Mode ? oder evtl. ganz entfernen ?
    dnsServer.start(DNS_PORT, "*", { 192,168,4,1 }); // <-- Config-AccessPoint hat als default-Adresse 192.168.4.1
    Serial.println(F("DNSServer gestartet"));
  }
  */

  actionTicker.attach(60,setActionFlag); // Actionabarbeitung triggern
  lifetimeTicker.attach(510,setLifetimeTriggerFlag); // triggert die Lifetimeüberprüfung

  setupWebServer();
  server.begin();
  Serial.println(F("\r\nWebserver gestartet ..."));
  debug(F("Webserver gestartet ..."));
  boolean udpFlag = false;
  if((strlen(parameter[ModulMode])==2) && (parameter[ModulMode][1]=='+')) { // Sensoranzeige aktiviert
    debug(F("Sensoranzeige aktiviert"));
    ntpTime = getNtpTime();
    ntpStartTime = ntpTime;
    bootTime = ntpTime;
    udpFlag = true;
  }
  if(strlen(parameter[RemoteIP]) > 0) { // falls remoteIp gesetzt, wird der ticker aktiviert
    int t = atoi(parameter[DeepSleep]);
    if(t > 0) {
      udpFlag = true;
      sensorTicker.attach(t,setSensorFlag);
      debug(F("Eigene Sensordaten senden"));
    }
  }
  if(udpFlag == true) {
    //Setup UDP
    if(udp.begin(rP))
      debug(F("Starte UDP"));
  }

  printWelcome();
  Serial.print(F("\r\n> "));

  if(parameter[ModulMode][0]!='W')
   digitalWrite(LED_PIN,LED_ON);
}


void loop() {

  if(verglTime != ntpTime) { // generiert Sekundentakt
    verglTime = ntpTime;
    // **** Taster wieder ausschalten
    if((out1_mode == TASTER) && (out1_wert > 0)) {
      out1_wert--;
      if(out1_wert == 0)
        digitalWrite(OUT1_PIN,LOW);
    }
    if((out2_mode == TASTER) && (out2_wert > 0)) {
      out2_wert--;
      if(out2_wert == 0)
        digitalWrite(OUT2_PIN,LOW);
    }
    // **** Ende Taster ausschalten

    // handelt pulsierend Ausgabe - Ausgang muß als Schalter deklariert sein
    if((pulseOut1 == true) && (out1_mode == SCHALTER)) {
      if(digitalRead(OUT1_PIN))
        digitalWrite(OUT1_PIN,LOW);
      else
        digitalWrite(OUT1_PIN,HIGH);
    }
    if((pulseOut2 == true) && (out2_mode == SCHALTER)) {
      if(digitalRead(OUT2_PIN))
        digitalWrite(OUT2_PIN,LOW);
      else
        digitalWrite(OUT2_PIN,HIGH);
    }
  }



  /*
  if(dns_server == true) {  // ist nur im Access-Mode aktiv
    dnsServer.processNextRequest();
  }
  */

  String sw;
  server.handleClient();
  if(sensorFlag) {  // localIp ist gesetzt, also Sensordaten erfassen und ggf. senden
    if(localIp.equals(parameter[RemoteIP])) { // Sensordaten im eigenen Modul anzeigen
      sw = String(ntpTime) + "," + getSensorDaten();
      insertSensorData(sw);
    }
    else {  // Sensordaten an externe Station übertragen
      IPAddress uip(rIp);  // IP-Sendeadresse
      udp.beginPacket(uip,rP);
      udp.print(getSensorDaten());
      udp.endPacket();

    }
    yield();
    // Serial.println(F("Sensordaten verschickt"));
    sensorFlag = false; // Triggerflag zurücksetzen
  }

  if((strlen(parameter[ModulMode])==2) && (parameter[ModulMode][1]=='+')) { // Externe Sensoren anzeigen
        // Zeit-Stuff
        if((ntpTime - ntpStartTime) > NTP_TRIGGER_TIME) {  // alle 12 Std
          uint32_t t = getNtpTime();
          if(t > 0) {
            ntpTime = t;
          }
          ntpStartTime = ntpTime;
        }
        // Höre am UDP-Port
        int packetSize = udp.parsePacket();
        if (packetSize) { // jepp, Sensordaten stehen an
          if(packetSize > (UDP_MAX_SIZE -2)) // Paket begrenzen
             packetSize = UDP_MAX_SIZE -2;
          udp.read(packetBuffer, packetSize);
          packetBuffer[packetSize] = 0;
          if(packetBuffer[0]=='!') {  // kein normaler Sensordatenstring, sondern Befehl zum Schalten der Ausgänge des Servers
            if(packetBuffer[1] == 'P') {  //  P --> Befehl zum Pulsen des entsprechenden Ausgangs im Sekundentakt
              if(packetBuffer[2] == '1') { // packetBuffer[2] --> Ausgang 1 oder 2
                 pulseOut1 = (packetBuffer[3]=='1');
                 digitalWrite(OUT1_PIN,(packetBuffer[3]=='1')); // stellt sicher, daß wenn pulseOut false ist, auch der Ausgang ausgeschaltet wird
              }
              else {
                pulseOut2 = (packetBuffer[3]=='1');
                digitalWrite(OUT2_PIN,(packetBuffer[3]=='1'));
              }
             }
            debug(packetBuffer);
            yield();
          }
          else {  // normaler Sensorstring für die Anzeige in sensor.htm
          sw = String(ntpTime) + "," + String(packetBuffer); // Sensordaten mit Zeit versehen
          String event = testUdpEvent(Split(sw).getIndex(1)); // Sensor-Modulname extrahieren und testen, ob dafür Event anliegt
          if(event.length() > 0) { // Jepp, Event senden
            IPAddress remote = udp.remoteIP();
            uint16_t port = udp.remotePort();
            udp.beginPacket(remote,port);
            Serial.println(event);
            udp.print(event);
            udp.endPacket();
          }
          yield();
          insertSensorData(sw); // Sensordaten abspeichern
          // Serial.println(sensorDaten);
          }
        }
  }

    // handle PushServices
  pushClient1.handle();
  pushClient2.handle();
  pushClient3.handle();
  pushClient4.handle();

  if(actionFlag) { // 1 mal pro min
    switch(tempSensorModel) {
      case DS1820:
         sensorMessage = do_ds18b20(true);
         sensorMessage = aliasParser.substitute(sensorMessage); // ersetzt romcode durch alias
         break;
      case DHT_SENSOR:
         sensorMessage = do_dht();
         break;
      case HH10D_SENSOR:
          sensorMessage = do_hh10d();
          break;
#ifdef SPINDELCODE
      case Spindel:
         sensorMessage = do_spindel();
         break;
#endif
      default:
         sensorMessage = String(F("kein Sensor vorhanden"));
    }
    // ******* dirtyHack **************
    if(parameter[ModulMode][0] == 'W') {
      if(WiFi.status()!= WL_CONNECTED) {
        Serial.println(F("Verbindung abgebrochen... Boote neu!"));
        ESP.restart();
        delay(5000);
      }
    }
    // ******* end dirtyHack **********
    handleAction(); // handle Condition and Actions
    actionFlag = false;
  }

  if(triggerLifetimeFlag) {
    testeSensorDataLifetime();
    triggerLifetimeFlag = false;
  }

  if(Serial.available()) {
      const __FlashStringHelper * cancelAction = F("\r\nAktion abgebrochen\r\n\r\n>");
      char c;
      char* filename = NULL;
      char* url = NULL;
      String s;
      double volt;
      c = Serial.read();
      switch(c){
        case '?':Serial.println(getModulInfo());
                 printWelcome();
                 break;
        /*
        case 'u':Serial.println(F("\r\nDateiupload"));
                 Serial.print(F("Upload-URL eingeben: "));
                 url = readSeriellParameter(url);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 Serial.print(F("\r\nDateinamen eingeben: "));
                 filename = readSeriellParameter(filename);
                 if(filename == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 if(uploadFile(url,filename)==false) {
                    Serial.println(F("Fehler beim Dateiupload"));
                    break;
                 }
                 Serial.println(F("Upload erfolgreich ausgefuehrt"));
                 break;
        */
        case 'q':showFiles();
                 break;
        case 't':Serial.println(F("\r\nSPIFFS-Datei anzeigen:"));
                 Serial.print(F("Dateinamen eingeben: "));
                 filename = readSeriellParameter(filename);
                 if(filename == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 Serial.println(getFileContents(filename));
                 break;

        case 'x':Serial.println(F("\r\nSPIFFS-Datei löschen:"));
                 Serial.print(F("Dateinamen eingeben: "));
                 filename = readSeriellParameter(filename);
                 if(filename == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 SPIFFS.remove(filename);
                 break;
        case 'p':Serial.println(F("\r\nModulparameter ins EPROM speichern"));
                 Serial.println(F("Zum Bestaetigen <Enter> eingeben, fuer Abbruch irgend ein Sonderzeichen ( z.B. <ESC> ) eingeben"));
                 Serial.println(F("<Enter> ohne Eingabe uebernimmt die Vorgabe\r\n"));
                 Serial.print(F("Modulname eingeben: "));
                 url = readSeriellParameter(parameter[ModulName]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[ModulName] = url;

                 Serial.print(F("\r\nSSID eingeben: "));
                 url = readSeriellParameter(parameter[Ssid]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[Ssid] = url;

                 Serial.print(F("\r\nPasswort eingeben: "));
                 url = readSeriellParameter(parameter[Passphrase]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[Passphrase] = url;

                 Serial.print(F("\r\nModulMode eingeben: "));
                 url = readSeriellParameter(parameter[ModulMode]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[ModulMode] = url;

                 Serial.print(F("\r\nRemoteIP eingeben: "));
                 url = readSeriellParameter(parameter[RemoteIP]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[RemoteIP] = url;

                 Serial.print(F("\r\nRemotePort eingeben: "));
                 url = readSeriellParameter(parameter[RemotePort]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[RemotePort] = url;

                 Serial.print(F("\r\nDeepSleep in Sek. eingeben: "));
                 url = readSeriellParameter(parameter[DeepSleep]);
                 if(url == NULL) {
                     Serial.println(cancelAction);
                     break;
                 }
                 else
                  parameter[DeepSleep] = url;

                 writeModulParameter();
                 Serial.println(F("\r\nModulparameter gespeichert!\r\n"));
                 break;
        case 'e':Serial.print(F("\r\nLoesche EEprom ... "));
                 clearEeprom();
                 Serial.println(F("fertig"));
                 break;
        case 's':Serial.println(F("\r\nGoing sleep for 30 seconds"));
                 WiFi.disconnect();
                 delay(1000);
                 system_deep_sleep(30*1000000);
                 Serial.println(F("Now sleep"));
                 delay(5000);
                 break;
        case 'r':Serial.println(F("\r\nGoing restart now"));
                 WiFi.disconnect();
                 delay(1000);
                 ESP.restart();
                 delay(5000);
                 //system_restart();
                 break;
        case 'R':Serial.println(F("\r\nGoing reset now"));
                 WiFi.disconnect();
                 delay(1000);
                 ESP.reset();
                 delay(5000);
                 break;
        case '5': Serial.println(F("\r\ndeep sleep - rf_enabled"));
                  WiFi.disconnect();
                  delay(1000);
                  ESP.deepSleep(10,WAKE_RF_DEFAULT);
                  delay(5000);
                  //system_restart();
                  break;
        case '6': Serial.println(F("\r\ndeep sleep - rf_disabled"));
                  WiFi.disconnect();
                  delay(1000);
                  ESP.deepSleep(10,WAKE_RF_DISABLED);
                  delay(5000);
                  //system_restart();
                  break;
        case '7': Serial.print(F("\r\nwifi status:"));
                  Serial.println(wifi_station_get_connect_status());
                  break;
        case 'n':Serial.println(String(F("Ntp-Time:")) + String(ntpTime));
                 break;
#ifdef SPINDELCODE
        case 'b':if(tempSensorModel != Spindel)
                    break;
                 Serial.println(do_spindel());
                 break;
#endif
        case 'h':do_hh10d();
                  break;
        case 'H':writeCalibratedValuesInFile();
                  break;
        case 'd':if(tempSensorModel != DS1820)
                    break;
                 Serial.print(F("Temperatur:"));
                 Serial.println(do_ds18b20(true));
                 break;
        case 'f':if(tempSensorModel != DHT_SENSOR)
                    break;
                 Serial.println(do_dht());
                 break;
        case '1':WiFi.printDiag(Serial);
                 break;
        case 'D':Serial.println(F("AP-Disconnect!"));
                 // WiFi.softAPdisconnect();
                 break;
        case 'v':Serial.println(F("\r\nStarte AD-Wandlung"));
#ifdef ADCMODE_VCC
                 volt = getVCC();
                 s = String(F("VCC=")) + String(volt,3);
#endif
#ifndef ADCMODE_VCC
                 volt = 3.6 / 1024 * system_adc_read();
                 s = String(F("Extern-V=")) + String(volt,3);
#endif
                 Serial.println(s);
                 break;
      }
      Serial.print(F("\r\n> "));
  }

}
