#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <string.h>
// #include <AES.h>

#include "wlanSensor.h"
#include "version.h"
#include "split.h"
#include "webServer.h"
#include "onewire.h"
#include "spiffs.h"
#include "setup.h"
#include "ntpClient.h"
#include "ESP8266WiFiScan.h"
#include "ESP8266WiFiMulti.h"
#include "FS.h"
#include "pushClient.h"
#include "parser.h"
#include "Esp.h"
#include "logger.h"
#include "hh10d.h"
#include "options.h"

extern "C" {
#include "user_interface.h"
#include "eagle_soc.h"
#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "os_type.h"

// void os_delay_us(int);
// void wdt_feed();
};


extern ESP8266WebServer server;
extern PushClient pushClient1,pushClient2,pushClient3,pushClient4;
extern AliasParser aliasParser;

/*
const char* uploadButton = "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

const char* upgradeButton = "<form method='POST' action='/upgrade' enctype='multipart/form-data'><input type='file' name='upgrade'><input type='submit' value='Firmware-Upgrade'></form>";

const char *htmlHeader = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>ESP8266</title>"
                         "<style type=\"text/css\"> div.rahmen { width: 700px;margin: 10px auto;padding: 10px;border: 1px solid #000; } </style>"
                         "</head><body><div class=\"rahmen\">";

const char *noSpiff = "<h3>Datei im SPIFFS-Filesystem nicht vorhanden</h3><p>Hier kann man eine Datei uploaden !</p>";

const char *upgrade = "<h3>Firmware-Upgrade</h3><p>Hier kann man eine Datei uploaden !</p>";

const char* link = "<p><a href=\"/\">zurück</a></p>";

const char *htmlEnd = "</div></body></html>";
*/

String version = VERSION;

File fsUploadFile;
bool firmwareUpgradeFlag = false;

/*** AES-Stuff *********************************
AES aes; // = AES() ;
int bits = 128;
byte *key = (unsigned char*)"01a-qqqq6789010123";
//real iv = iv x2 ex: 01234567 = 0123456701234567
//unsigned long long int my_iv = 36753562;
unsigned long long int my_iv = 302;
//byte cipher [50] ;
//byte check [50] ;


byte* aesEncrypt(char* plain) {
  //byte iv [N_BLOCK] ;
  aes.calc_size_n_pad(strlen(plain)+1);
  int size = aes.get_size();
  putErr("<br>cipherSize:");
  putErr(String(size).c_str());
  putErr("<br>");
  byte* cipher = new byte[size];
  aes.set_IV(my_iv);
  // aes.iv_inc();
  // aes.get_IV(iv);
  // for(uint8_t i=0; i<N_BLOCK;i++){
  //   putErr(String(iv[i],HEX).c_str());
  //   putErr(" ");
  // }

  aes.do_aes_encrypt((byte*)plain,strlen(plain)+1,cipher,key,bits);
  // aes.get_IV(iv);
  // putErr("<br>I-Vektor:");
  // for(uint8_t i=0; i<N_BLOCK;i++){
  //   putErr(String(iv[i],HEX).c_str());
  //   putErr(" ");
  // }
  putErr("<br>");
  return cipher;
}

byte* aesDecrypt(byte* cipher) {
  // byte iv [N_BLOCK] ;
  int size = aes.get_size();
  byte* plain = new byte[size];
  // memset(plain,0,size);
  aes.set_IV(my_iv);
  // aes.get_IV(iv);
  // for(uint8_t i=0; i<N_BLOCK;i++){
  //   putErr(String(iv[i],HEX).c_str());
  //   putErr(" ");
  // }
  aes.do_aes_decrypt(cipher,size,plain,key,bits);
  // aes.get_IV(iv);
  // putErr("<br>I-Vektor:");
  // for(uint8_t i=0; i<N_BLOCK;i++){
  //   putErr(String(iv[i],HEX).c_str());
  //   putErr(" ");
  // }
  putErr("<br>Cipher:");
  for(uint8_t i=0; i<size;i++){
    putErr(String(cipher[i],HEX).c_str());
    putErr(" ");
  }
  putErr("<br>");
  delete cipher;
  return plain;
}


void testAes() {
  byte* plainText;
  byte* cipher;
  //aes.iv_inc();
  char text[] = "encrypt !";

  cipher = aesEncrypt(text); // gibt array per new() zurueck
  plainText = aesDecrypt(cipher); // gibt array per new() zurueck und delete array aus aesEncrypt
  putErr("<br>Plain:");
  putErr((char*)plainText);
  putErr("<br>");
  delete plainText;
}

*********************************************************************/

const String newFileUploadString(){
  String p = F("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>ESP8266</title>"
                 "<style type=\"text/css\"> div.rahmen { width: 700px;margin: 10px auto;padding: 10px;border: 1px solid #000; } </style>"
                 "</head><body><div class=\"rahmen\">"
                 "<h3>Datei im SPIFFS-Filesystem nicht vorhanden</h3><p>Hier kann man eine Datei uploaden !</p>"
                 "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>"
                 "<p><a href=\"/\">zurück</a></p>"
                 "</div></body></html>");
  return p;
}

const String newHtmlPage(String msg){
  String p = F("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>ESP8266</title>"
                 "<style type=\"text/css\"> div.rahmen { width: 700px;margin: 10px auto;padding: 10px;border: 1px solid #000; } </style>"
                 "</head><body><div class=\"rahmen\"><h3>");
  p = p + msg;
  p = p + F("</h3><p><a href=\"/\">zurück</a></p></div></body></html>");
  return p;
}

const String schalterDef(const char *_name, uint8_t zustand){
  String p = F("<p><h3>Ausgang:$name</h3><input class='button' type='submit' name='$name' value='Schalter'>$zustand</p>");
  if(zustand)
    p.replace("$zustand","<label> ist Ein</label>");
  else
    p.replace("$zustand","<lavel> ist Aus</label>");
  p.replace("$name",_name);
  return p;
}

const String tasterDef(const char *_name) {
  String p = F("<p><h3>Ausgang:$name</h3><input class='button' type='submit' name='$name' value='Taster'></p>");
  p.replace("$name",_name);
  return p;
}

const String dimmerDef(char sliderNr, int8_t dimmwert){
  char _dimmwert[20];
  itoa(dimmwert,_dimmwert,10);
  if(dimmwert < 0 || dimmwert > 100) {
    _dimmwert[0] = '0';
    _dimmwert[1] = 0;
  }
  char ueberschrift[] =  "<h3>Slider Ausgang X</h3>";
  ueberschrift[19] = sliderNr;
  Serial.println(String("Dimmwert:") + String(_dimmwert));
  String html = F("<table>	<tr> <td>"
			         "<script type='text/javascript'> new Slider('$slider_name', 800, 40, 0, 100, $eingabewert, 1, 5,"
						     "\"document.getElementById('$eingabefeld_id').value=Slider.instanz['$slider_name'].wert;\");"
					       "</script>	<td> <input name='$eingabefeld_name' id='$eingabefeld_id' value='$eingabewert'"
                 "maxlength='4' size='5' onChange=\"Slider.instanz['_slider_name'].neuerWert(this.value);\">"
					       "</td> </tr> </table>"
			           "<p><input class='button' type='submit' name='$button_name' value='jetzt Dimmen'></p>" );
  String p = String(ueberschrift) + html;
  char slider[] = "sliderX";
  slider[6]=sliderNr;
  char sliderbutton[] = "sliderbuttonX";
  sliderbutton[12]=sliderNr;
  char eingabefeld[] = "eingabefeldX";
  eingabefeld[11]=sliderNr;
  p.replace("$slider_name",slider);
  p.replace("$button_name",sliderbutton);
  p.replace("$eingabefeld_id",String(eingabefeld)+String("_id"));
  p.replace("$eingabefeld_name",eingabefeld);
  p.replace("$eingabewert",_dimmwert);
  return p;
}


//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String modulLaufzeit() {
  uint32_t diff = ntpTime - bootTime;
  uint16_t days = diff / 86400;
  uint8_t hours = (diff % 86400) / 3600;
  uint8_t min = (diff % 3600) / 60;
  char s[50];
  sprintf(s,"%d:%02d:%02d",days,hours,min);
  return String(s);
}

/*
extern "C" {
#include "user_interface.h"

extern struct rst_info resetInfo;
}
*/

String getModulInfo() {
     String info;
    info = String(F("Firmware-Version:")) + version +
           String(F(" <br>\r\nChip-ID: ")) + String(ESP.getChipId()) +  String(F(" <br>\r\n")) +
           String(F("Flash-Groesse (Real): ")) + formatBytes(ESP.getFlashChipRealSize()) + String(F(" <br>\r\n")) +
           String(F("Heap-Groesse: ")) + formatBytes(ESP.getFreeHeap()) + String(F(" <br>\r\n")) +
           String(F("Sketch-Groesse: ")) + formatBytes(ESP.getSketchSize()) + String(F(" <br>\r\n")) +
           String(F("Freie Sketch-Groesse: ")) + formatBytes(ESP.getFreeSketchSpace()) + String(F(" <br>\r\n")) +
           String(F("SDK-Version: ")) + String(ESP.getSdkVersion()) + String(F(" <br>\r\n")) +
           String(F("Modul-Laufzeit (Tage:Std:Min) ")) + modulLaufzeit() + String(F(" <br>\r\n")) +
           // String(millis()) + String("--") + String(resetInfo.reason) + String(F(" <br>\r\n")) +
           String(F("Bootgrund:")) + ESP.getResetReason() + String(F(" <br>\r\n"));
   return info;
}

String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

/*
toDo: braucht diese Version mit buff weniger heap-speicher -- ausprobieren!!!
String EspClass::getResetReason(void) {
    char buff[32];
    if (resetInfo.reason == REASON_DEFAULT_RST) { // normal startup by power on
      strcpy_P(buff, PSTR("Power on"));
    } else if (resetInfo.reason == REASON_WDT_RST) { // hardware watch dog reset
      strcpy_P(buff, PSTR("Hardware Watchdog"));
    } else if (resetInfo.reason == REASON_EXCEPTION_RST) { // exception reset, GPIO status won’t change
      strcpy_P(buff, PSTR("Exception"));
    } else if (resetInfo.reason == REASON_SOFT_WDT_RST) { // software watch dog reset, GPIO status won’t change
      strcpy_P(buff, PSTR("Software Watchdog"));
    } else if (resetInfo.reason == REASON_SOFT_RESTART) { // software restart ,system_restart , GPIO status won’t change
      strcpy_P(buff, PSTR("Software/System restart"));
    } else if (resetInfo.reason == REASON_DEEP_SLEEP_AWAKE) { // wake up from deep-sleep
      strcpy_P(buff, PSTR("Deep-Sleep Wake"));
    } else if (resetInfo.reason == REASON_EXT_SYS_RST) { // external system reset
      strcpy_P(buff, PSTR("External System"));
    } else {
      strcpy_P(buff, PSTR("Unknown"));
    }
    return String(buff);
}

String getResetInfo() {
  String s;
  struct rst_info * rPtr = ESP.getResetInfoPtr();
  switch(rPtr->reason) {
    case REASON_DEFAULT_RST: // normal startup by power on
      s = F("Power on");
      break;
    case REASON_WDT_RST: // hardware watch dog reset
      s = F("Hardware Watchdog");
      break;
    case REASON_EXCEPTION_RST: // exception reset, GPIO status won’t change
      s = F("Exception");
      break;
    case REASON_SOFT_WDT_RST: // software watch dog reset, GPIO status won’t change
      s = F("Software Watchdog");
      break;
    case REASON_SOFT_RESTART: // software restart ,system_restart , GPIO status won’t change
      s = F("Software/System restart");
      break;
    case REASON_DEEP_SLEEP_AWAKE: // wake up from deep-sleep
      s = F("Deep-Sleep Wake");
      break;
    case REASON_EXT_SYS_RST: // external system reset
      s = F("External System");
      break;
    default:
      s = F("unbekannter Resetgrund");
  }
  return s;
}
*/

String getNetworkInfo() {
  String info = String(F("Modulname: ")) + String(parameter[ModulName]) + String(F(" <br>\r\n")) +
                String(F("Mode: ")) + String(parameter[ModulMode]) + String(F(" <br>\r\n")) +
                String(F("SSID: ")) + String(WiFi.SSID()) + String(F("  BSSID: ")) + WiFi.BSSIDstr() + String(F(" <br>\r\n")) +
                String(F("Eigene MAC-Addr: ")) + WiFi.macAddress() + String(F("<br>\r\n")) +
                String(F("Local-IP: ")) + localIp + String(F(" Gateway: ")) + ipToString(WiFi.gatewayIP()) + String(F(" <br>\r\n")) +
                String(F("AccessPoint-IP: ")) + ipToString(WiFi.softAPIP())+ String(F(" <br>\r\n")) +
                String(F("Channel: ")) + String(WiFi.channel()) + String(F(" <br><br>\r\n\r\n")) +
                String(F("RemoteIP: ")) + String(parameter[RemoteIP]) + String(F(" <br>\r\n")) +
                String(F("RemotePort: ")) + String(rP) + String(F(" <br>\r\n")) +
                String(F("DeepSleep: ")) + String(parameter[DeepSleep]) + String(F(" <br>\r\n"));

  return info;
}


String getWebStartPage() {
  // Serial.print("Handle getWebStartPage(): ");
  String page = getFileContents("/startPage.htm");
  if (page.length() == 0) {
     // Serial.println("open 'startPage.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  String info;
  info = String(F("Modul-Laufzeit: ")) + modulLaufzeit() + String(F("  (Tage:Std:Min)<br>")) +
         String(F("Modul-Mode: ")) + String(parameter[ModulMode]) + String(F("<br>")) +
         String(F("Letzter Boot: ")) + ESP.getResetReason() + String(F("<br>")) +
         String(F("Version: ")) + version + String(F("<br>"));
  page.replace("$Info",info);
  return page;
}

String getParameterPage() {
  // Serial.print("Handle getParameterPage(): ");
  String page =  getFileContents("/parameter.htm");
  if (page.length() == 0) {
     // Serial.println("open 'parameter.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  page.replace("$Name",String(parameter[ModulName]));
  page.replace("$Ssid",String(parameter[Ssid]));
  page.replace("$Localip",String(parameter[LocalIP]));
  page.replace("$Gateway",String(parameter[Gateway]));
  page.replace("$Bssid",String(parameter[Bssid]));
  page.replace("$Passphrase","****");
  page.replace("$Mode",getModulMode(parameter[ModulMode]));
  /*
  switch(parameter[ModulMode][0]){
    case 'L':
      page.replace("$Mode",F("<option selected>L - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      break;
    case 'S':
      page.replace("$Mode",F( "<option>L  - Loggermode</option>"
                                "<option selected>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      break;
    case 'M':
      if(parameter[ModulMode][1] == '+')
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option selected>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      else
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option selected>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      break;
    case 'W':
      if(parameter[ModulMode][1] == '+')
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option selected>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      else
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option selected>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      break;
    case 'A':
      if(parameter[ModulMode][1] == '+')
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option selected>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
      else
        page.replace("$Mode",F("<option>L  - Loggermode</option>"
                              "<option>S  - Sensormode</option>"
                              "<option>W  - Station-Mode</option>"
                              "<option>W+ - Station-Mode + Sensoranzeige</option>"
                              "<option selected>A  - AccesPoint-Mode</option>"
                              "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                              "<option>M  - AccessPoint + Station-Mode</option>"
                              "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                              ));
      break;
    default:
      page.replace("$Mode",F( "<option>L  - Loggermode</option>"
                                "<option>S  - Sensormode</option>"
                                "<option>W  - Station-Mode</option>"
                                "<option>W+ - Station-Mode + Sensoranzeige</option>"
                                "<option>A  - AccesPoint-Mode</option>"
                                "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                                "<option>M  - AccessPoint + Station-Mode</option>"
                                "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>"
                                ));
  }
  */
  page.replace("$Rip",String(parameter[RemoteIP]));
  page.replace("$Rp",String(parameter[RemotePort]));
  page.replace("$Dsleep",String(parameter[DeepSleep]));
  page.replace("$VCC_Offset",String(parameter[VCC_Offset]));
  page.replace("$Alias",String(parameter[TempAlias]));
  String tModel = String(parameter[TempSensor]);
  page.replace(F("$tModel"),getSensorOptions(tModel));
  /*
  if(tModel.equals("DS1820"))
    page.replace("$tModel",F("<option>NO</option><option selected>DS1820</option><option>DHT22</option><option>Spindel</option>"));
  else
    if(tModel.equals("DHT22"))
      page.replace("$tModel",F("<option>NO</option><option>DS1820</option><option selected>DHT22</option><option>Spindel</option>"));
    else
      if(tModel.equals("Spindel"))
        page.replace("$tModel",F("<option>NO</option><option>DS1820</option><option>DHT22</option><option selected>Spindel</option>"));
      else
        page.replace("$tModel",F("<option selected>NO</option><option>DS1820</option><option>DHT22</option><option>Spindel</option>"));
  */
  page.replace("$Ap_pw",String(parameter[APpassphrase]));
  page.replace("$Ap_ip",String(parameter[APip]));
  //page.replace("$Ap_gw",String(parameter[APgateway]));
  page.replace("$Ps1",String(parameter[PSid1]));
  page.replace("$Ps2",String(parameter[PSid2]));
  page.replace("$Ps3",String(parameter[PSid3]));
  page.replace("$Ps4",String(parameter[PSid4]));
  page.replace("$Condition1",String(parameter[Condition1]));
  page.replace("$Action1",String(parameter[Action1]));
  page.replace("$Condition2",String(parameter[Condition2]));
  page.replace("$Action2",String(parameter[Action2]));
  page.replace("$Condition3",String(parameter[Condition3]));
  page.replace("$Action3",String(parameter[Action3]));
  page.replace("$Condition4",String(parameter[Condition4]));
  page.replace("$Action4",String(parameter[Action4]));
  //String out1_mode = String(parameter[OUT1_MODE]);
  //Serial.println("Output1 - Mode:"+out1_mode);
  setOutputMode();
  page.replace("$oMode1",getOutputMode(out1_mode));
  page.replace("$oMode2",getOutputMode(out2_mode));
  /*
  switch(out1_mode){
    case SCHALTER:
      page.replace("$oMode1",F("<option>NO_OUTPUT</option><option selected>SCHALTER</option><option>TASTER</option><option>DIMMER</option>"));
      break;
    case TASTER:
      page.replace("$oMode1",F("<option>NO_OUTPUT</option><option>SCHALTER</option><option selected>TASTER</option><option>DIMMER</option>"));
      break;
    case DIMMER:
      page.replace("$oMode1",F("<option>NO_OUTPUT</option><option>Schalter</option><option>Taster</option><option selected>Dimmer</option>"));
      break;
    default:
      page.replace("$oMode1",F("<option selected>NO_OUTPUT</option><option>SCHALTER</option><option>TASTER</option><option>DIMMER</option>"));
    }
  switch(out2_mode){
    case SCHALTER:
      page.replace("$oMode2",F("<option>NO_OUTPUT</option><option selected>SCHALTER</option><option>TASTER</option><option>DIMMER</option>"));
      break;
    case TASTER:
      page.replace("$oMode2",F("<option>NO_OUTPUT</option><option>SCHALTER</option><option selected>TASTER</option><option>DIMMER</option>"));
      break;
    case DIMMER:
      page.replace("$oMode2",F("<option>NO_OUTPUT</option><option>SCHALTER</option><option>TASTER</option><option selected>DIMMER</option>"));
      break;
    default:
      page.replace("$oMode2",F("<option selected>NO_OUTPUT</option><option>SCHALTER</option><option>TASTER</option><option>DIMMER</option>"));
    }
  */
  return page;
}

String getErrPage() {
  String page = getFileContents(F("/err.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'help.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  page.replace("$Info",getModulInfo());
  page.replace("$NetInfo",getNetworkInfo());
  page.replace("$Error",getErr());
  return page;
}


String getHelpPage() {
  // Serial.print("Handle getHelpPage(): ");
  String page = getFileContents(F("/help.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'help.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  return page;
}


String getFirmwarePage() {
  // Serial.print("Handle getFirmwarePage(): ");
  String page = getFileContents(F("/firmware.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'firmware.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  return page;
}

String getGpioPage() {
  // Serial.print("Handle getGpioPage(): ");
  String page = getFileContents(F("/gpio.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'gpio.htm' failed, return uploadPage");
     return newFileUploadString();
  }
#ifdef ADCMODE_VCC
    double volt = getVCC();
#endif
#ifndef ADCMODE_VCC
    uint16_t v = system_adc_read();
    double volt = 3.6 / 1024 * v;
#endif
  String s = String(F("<p>Die momentane Betriebsspannung beträgt:")) + String(volt,3) + " V</p>";
  String t;
  switch(tempSensorModel) {
      case DS1820:
         t = do_ds18b20(true);
         break;
      case DHT_SENSOR:
         t = do_dht();
         break;
      case HH10D_SENSOR:
         t = do_hh10d();
         break;
#ifdef SPINDELCODE
      case SPINDEL:
         t = do_spindel();
         break;
#endif
      default:
         t = F("kein Sensor vorhanden");
  }
  String input = F("<p>Input-Pin:");
  // pinMode(IN_PIN,INPUT_PULLUP);
  if(digitalRead(IN_PIN))
     input += F("HIGH</p>");
  else
     input += F("LOW</p>");

  String out1 = F("<p>Out1-Pin:");
  if(out1_mode == DIMMER) {
    out1 += String(out1_wert) + String("</p>");
  }
  else {
     if(digitalRead(OUT1_PIN))
        out1 += F("HIGH</p>");
     else
        out1 += F("LOW</p>");
  }

  String out2 = F("<p>Out2-Pin:");
  if(out2_mode == DIMMER) {
    out2 += String(out2_wert) + String("</p>");
  }
  else {
     if(digitalRead(OUT2_PIN))
        out2 += F("HIGH</p>");
     else
        out2 += F("LOW</p>");
  }
  page.replace("$Temperatur",String(F("<p>Der momentane Sensorwert beträgt:")) + t + String("</p>"));
  page.replace("$Vcc",s + input + out1 + out2);
  return page;
}

String getSensorPage() {
  String daten,wert;
  uint8_t idx;
  int h,m,s;
  char time[10];

  String page = getFileContents(F("/sensor.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'gpio.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  Split split(sensorDaten,'\n');
  while(split.next()) {
    idx = 0;
    daten += "<tr>";
    Split z(split.getNext());
    while(z.next()) {
      wert = z.getNext();
      if(idx == 0) { // Zeitfeld
        getTime((unsigned long)wert.toInt(),&h,&m,&s);
        sprintf(time,"%02d:%02d:%02d",h,m,s);
        wert = String(time);
      }
      /*
      if(idx == 3) { // Temperatursensorfeld
        putErr("Temperatursensorfeld:");
        putErr(wert.c_str());
        putErr("<br/>");
        Split tempfeld(wert,' ');
        wert = "";
        while(tempfeld.next()) {
          String w = tempfeld.getNext();
          Split t(w,':');
            if(t.getCount()==2) {
              putErr("Fuege Alias ein:");
              char* alias = aliasParser.getValue(t.getNext());
              if(alias != NULL) {
                 w = String(alias) + String(':') + t.getNext();
                 putErr(alias);
              }
              else
                putErr("NULL");
              putErr("<br/>");
            }
           if(wert.length()==0)
              wert = w;
           else
              wert += ' ' + w;
        }
      }
      */
      if(idx++ != 1) // alle, außer Modulname
         daten += String(F("<td align='center'>")) + wert + String(F("</td>"));
      else { // Modulname
          daten += String(F("<td align='center'><input class='sensor_button' type='submit' name='")) +
                wert + String(F("' value='")) + wert + String(F("'></td>"));
      }
    }
    daten += F("</tr>");
  }
  page.replace("$Sensor",daten);
  return page;
}

String getScanPage() {
  String page = getFileContents(F("/scan.htm"));
  if (page.length() == 0) {
     return newFileUploadString();
  }
  return page;
}

String getSchalterPage() {
  // Serial.print("Handle getSchalterPage(): ");
  String page = getFileContents(F("/schalter.htm"));
  if (page.length() == 0) {
     // Serial.println("open 'schalter.htm' failed, return uploadPage");
     return newFileUploadString();
  }
  //String out1_mode = String(parameter[OUT1_MODE]);
  //Serial.println("Output1 - Mode:"+out1_mode);
  switch(out1_mode){
    case SCHALTER:
      page.replace("$OUT1",schalterDef("schalter1",out1_wert));
      break;
    case TASTER:
      page.replace("$OUT1",tasterDef("taster1"));
      break;
    case DIMMER:
      page.replace("$OUT1",dimmerDef('1',out1_wert));
      break;
    default:
      page.replace("$OUT1","");
    }
  //String out2_mode = String(parameter[OUT2_MODE]);
  switch(out2_mode){
    case SCHALTER:
      page.replace("$OUT2",schalterDef("schalter2",out2_wert));
      break;
    case TASTER:
      page.replace("$OUT2",tasterDef("taster2"));
      break;
    case DIMMER:
      page.replace("$OUT2",dimmerDef('2',out2_wert));
      break;
    default:
      page.replace("$OUT2","");
    }
  return page;
}


void webRoot(){
  Serial.print(F("\r\nRootPage - "));
  switch(server.method()) {
    case HTTP_GET:
       server.send(200, F("text/html"),getWebStartPage());
       Serial.println(F("GET"));
       break;
    case HTTP_POST:
       Serial.println(F("POST"));
       // Serial.println("Reset-Button:" + server.arg("reboot"));
       // Serial.println("Test-Button:" + server.arg("testbutton"));
       if(server.arg("reboot").length() > 0) {
          server.send(200, F("text/html"), F("Reboot wird durchgefuehrt .... Den Zurueck-Button des Browsers benutzen"));
          delay(1000);
          WiFi.disconnect();
          delay(1000);
          ESP.restart();
          delay(5000);
          // rebootet !
       }
        if(server.arg("testbutton").length() > 0) {

          Serial.println(F("Testbutton gedrueckt, get Ntp-Time"));
          ntpTime = getNtpTime();
          ntpStartTime = ntpTime;
          if(ntpTime)
             Serial.println(F("got Ntp-Time"));
          else
             Serial.println(F("no Ntp-Time"));
          server.send(200, F("text/html"),getWebStartPage());
          return;


          /*
          uint32_t start = 1048576; // ab 1MB löschen
          for (int i=0; i < 768;i++) { // LOESCHT EEprom,SPIFFS-Filesystem und SDK-Parameter
            spi_flash_erase_sector( (start / 4096)+i );
            if (0 ==(i % 16))
              Serial.print(".");
          }

          uint32_t start = 4194304 - 16384; // ab 4MB -16K <-- SDK-Konfiguration
          for (int i=0; i < 4;i++) { // 4 Blobs a 4KB
            spi_flash_erase_sector( (start / 4096)+i );
            Serial.print(".");
          }
          Serial.println("\r\nSDK-Erase complete, rebooting\r\n");
          system_restart();
          delay(5000);
          */
       }
       if(server.arg("erasebutton").length() > 0) {
         // toDo --> die Parameterdaten löschen oder doch besser ab 1MB aufwärts??????????
         debug(F("Erasebutton aufgerufen zur Zeit ohne Funktion"));
         server.send(200, F("text/html"),getWebStartPage());
         return;
         /*
         Serial.println(F("Loesche alles ab erstem MB (nach Programmspeicher)"));
         uint32_t start = 1048576; // ab 1MB löschen
         for (int i=0; i < 768;i++) { // LOESCHT EEprom,SPIFFS-Filesystem und SDK-Parameter
           spi_flash_erase_sector( (start / 4096)+i );
           if (0 ==(i % 16))
             Serial.print(".");
           }
         */
         /*
         uint32_t start = 4194304 - 16384; // ab 4MB -16K <-- SDK-Konfiguration
         for (int i=0; i < 4;i++) { // 4 Blobs a 4KB
           SpiFlashOpResult result = spi_flash_erase_sector( (start / 4096)+i );
           switch(result){
             case SPI_FLASH_RESULT_OK:
              Serial.print(".");
              break;
            case SPI_FLASH_RESULT_ERR:
              Serial.print("E");
              break;
            case SPI_FLASH_RESULT_TIMEOUT:
              Serial.print("T");
              break;
            default:
              Serial.print("?");
           }
         }
         */
         /*
         Serial.println(F("\r\nLöschung durchgeführt, bitte warten ... Restart wird durchgeführt!\r\n"));
         server.send(200, F("text/html"), F("Gelöscht! Bitte warten ... Reboot wird durchgefuehrt .... Den Zurueck-Button des Browsers benutzen"));
         delay(1000);
         WiFi.disconnect();
         delay(1000);
         ESP.restart();
         delay(5000);
         // Ausführung stoppt hier
         */
       }
    default:
      server.send(200, F("text/html"),getWebStartPage());
      Serial.println(String(F("ServerMethode:")) + String(server.method()));
      debug(String(F("ServerMethode:")) + String(server.method()));
  }
}


void uploadPage() {
  server.send(200, F("text/html"),newFileUploadString());
  // Serial.println("uploadPage gesendet");
}

void sendAnswerPage(String answer,String returnLink = "") {
  // toDo --> Seite noch etwas aufhübschen
  String p = F("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>ESP8266</title>"
                         "<style type=\"text/css\"> div.rahmen { width: 700px;margin: 10px auto;padding: 10px;border: 1px solid #000; } </style>"
                         "</head><body><div class=\"rahmen\">");
  p += answer;
  if(returnLink.length() > 0) {
    p += F("<p><a href=\"");
    p += returnLink;
    p += F("\">zurück</a></p>");
  }
  else
    p += F("<p><a href=\"/\">zurück</a></p>");
  p += F("</div></body></html>");
  server.send(200, F("text/html"),p);
  // Serial.println("uploadAnswerPage gesendet");
}

void errPage() {
  server.send(200, F("text/html"),getErrPage());
  // Serial.println("help.htm gesendet");
}


void helpPage() {
  server.send(200, F("text/html"), getHelpPage());
  // Serial.println("help.htm gesendet");
}

void firmwarePage() {
  server.send(200, F("text/html"), getFirmwarePage());
  // Serial.println("firmware.htm gesendet");
}

void gpioPage() {
  server.send(200, F("text/html"), getGpioPage());
  // Serial.println("gpio.htm gesendet");
}

void sensorPage() {
  if(server.method()==HTTP_GET) {
     server.send(200, F("text/html"), getSensorPage());
     return;
  }
  // HTTP_POST
  String s = server.arg(0) + String(F(",modulMode=W"));
  if(udpEvent.length() > 0) {
    if(strstr(udpEvent.c_str(),s.c_str())==NULL) // Verhindern, das Event mehrfach angelegt wird !
      udpEvent += ";" + s;
    else
      Serial.println(F("Event vorhanden"));
  }
  else
     udpEvent = s;
  server.send(200, F("text/html"), getSensorPage());
}

String getHtmlTemplate(String file) {
  String htmlTemplate = F("<form action='files.htm' method='post'>"
                          "<p><input type='text' name='file_input' value='$filename' >"
                          "<input type='submit' class='button' name='file_button' value='Löschen' /></p>"
                          "</form>");
  htmlTemplate.replace("$filename",file);
  return htmlTemplate;
}

String getFilesPage() {
  // Serial.print("Handle getSchalterPage(): ");
  String page = getFileContents(F("/files.htm"));
  if (page.length() == 0) {
     return newFileUploadString();
  }
  // Dateiliste einlesen
  Serial.println(F("SPIFFS-Dateien lesen"));
  Dir dir = SPIFFS.openDir(F("/"));
  String list;
  while(dir.next()){
    File file = dir.openFile("r");
    // Serial.println(file);
    list += getHtmlTemplate(file.name());
    file.close();
  }
  page.replace("$Moduldateien", list);
  return page;
}

void filesPage() {
  if(server.method()==HTTP_POST) {
    // HTTP_POST
    String s = server.arg("file_input");
    Serial.println(String(F("Delete File:")) + s);
    deleteFile(s.c_str());
  }
  server.send(200, F("text/html"), getFilesPage());
}

void hh10dPage() {
  String kalibriermeldung = "";
  String feuchtewert = "";
  String page = getFileContents(F("/hh10d.htm"));
  if (page.length() == 0) {
     page = newFileUploadString();
  }
  if(server.method()==HTTP_POST) {
    // HTTP_POST
    if(server.arg("calibrate").length() > 0) {
      kalibriermeldung = writeCalibratedValuesInFile();
    }
    else if(server.arg("humidity").length() > 0) {
      feuchtewert = do_hh10d();
    }
  }
  page.replace(F("$Kalibriermeldung"),kalibriermeldung);
  page.replace(F("$Feuchtewert"),feuchtewert);
  server.send(200, F("text/html"), page);
}

void scanPage() {
  String page = getScanPage();
  if(server.method()==HTTP_GET) {
    server.send(200, F("text/html"), page);
    return;
  }
  // HTTP_POST
  String ssid_scan;int32_t rssi_scan;uint8_t sec_scan;uint8_t* BSSID_scan;int32_t chan_scan;bool hidden_scan;
  if(server.arg("scan").length() > 0) {
    uint8_t count = WiFi.scanNetworks();
    String info = "";
    if(count > 0) {
        for(uint8_t i = 0; i < count; i++) {
          WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
          Serial.println(ssid_scan);
          String bssid = String(BSSID_scan[0], HEX) + String(":") + String(BSSID_scan[1], HEX) + String(":") + String(BSSID_scan[2], HEX) + String(":") +
                         String(BSSID_scan[3], HEX) + String(":") + String(BSSID_scan[4], HEX) + String(":") + String(BSSID_scan[5], HEX);
          info += String(F("<tr><td align='center'>")) + ssid_scan + String(F("</td>"));
          info += String(F("<td align='center'>")) + bssid + String(F("</td>"));
          info += String(F("<td align='center'>")) + String(rssi_scan) + String(F("</td>"));
          info += String(F("<td align='center'>")) + String(chan_scan) + String(F("</td>"));
          info += String(F("<td align='center'><input class='button' type='submit' name='n_")) +
                String(i) + String(F("' value='uebernehmen' ></td></tr>"));
        }
    }
    page.replace("$SCAN_RESULT",info);
  }
  else if(server.arg("free").length() > 0) {
    WiFi.scanDelete();
    page.replace("$SCAN_RESULT","");
  }
  else {
     page.replace("$SCAN_RESULT","");
    for(uint8_t i=0; i < server.args(); i++) {
      if(server.argName(i).length() > 0) {
         Serial.println("Name:" + server.argName(i));
         if(server.argName(i).startsWith("n_")) {  // toDo: muß überarbeitet werden, siehe unten
          String number = server.argName(i).substring(2);
          Serial.println("nummer:"+number);
          uint8_t idx = number.toInt();
          Serial.println("Index:" + String(idx));
          WiFi.getNetworkInfo(idx, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
          String bssid = String(BSSID_scan[0], HEX) + String(":") + String(BSSID_scan[1], HEX) + String(":") + String(BSSID_scan[2], HEX) + String(":") +
                         String(BSSID_scan[3], HEX) + String(":") + String(BSSID_scan[4], HEX) + String(":") + String(BSSID_scan[5], HEX);
          Serial.println("BSSID:" + bssid);
          parameter[Bssid] = toCharArr(bssid);
          writeModulParameter();
          // toDo: nicht ausgereift, weil passwort nicht abgefragt wird und nicht neu gestartet !!!!!
          // ausserdem sollte vorher überprüft werden, ob eine Einwahl mit den neuen Parametern möglich ist !!!!
         }
      }
    }
  }
  server.send(200, F("text/html"), page);
}

void schalterPage() {
  int w = 0;
  // Serial.println("help.htm gesendet");
  if(server.method() == HTTP_POST) { // Antwortseite Webserver
    if(server.arg("schalter1").length() > 0) {
      Serial.print(F("Schalter:"));
      if(digitalRead(OUT1_PIN)) {
         digitalWrite(OUT1_PIN,LOW);
         out1_wert = 0;
         Serial.println(F("LOW"));
      }
      else {
         digitalWrite(OUT1_PIN,HIGH);
         out1_wert = -1;
         Serial.println(F("HIGH"));
      }
    }
    else {
      if(server.arg("taster1").length() > 0) {
        Serial.println(F("Taster einschalten"));
        digitalWrite(OUT1_PIN,HIGH);
        out1_wert = 3;
      }
      else {
        if(server.arg("sliderbutton1").length() > 0) {
          Serial.println(F("Dimmen"));
          String wert = server.arg("eingabefeld1");
          w = wert.toInt();
          Serial.println("Eingabefeldwert:"+String(w));
          out1_wert = w;
          analogWrite(OUT1_PIN,w);
        }
      }
    }
    if(server.arg("schalter2").length() > 0) {
      Serial.print(F("Schalter:"));
      if(digitalRead(OUT2_PIN)) {
         digitalWrite(OUT2_PIN,LOW);
         out2_wert = 0;
         Serial.println(F("LOW"));
      }
      else {
         digitalWrite(OUT2_PIN,HIGH);
         out2_wert = -1;
         Serial.println(F("HIGH"));
      }
    }
    else {
      if(server.arg("taster2").length() > 0) {
        Serial.println(F("Taster einschalten"));
        digitalWrite(OUT2_PIN,HIGH);
        out2_wert = 3;
      }
      else {
        if(server.arg("sliderbutton2").length() > 0) {
          Serial.println(F("Dimmen"));
          String wert = server.arg("eingabefeld2");
          w = wert.toInt();
          out2_wert = w;
          analogWrite(OUT2_PIN,w);
        }
      }
    }
  server.send(200, F("text/html"), getSchalterPage());
  }
  else
    server.send(200, F("text/html"), getSchalterPage());
}


void parameterPage() {
  String p;
  if(server.method() == HTTP_POST) { // Antwortseite Webserver
    String _ps1 = server.arg("ps1");
    String _ps2 = server.arg("ps2");
    String _ps3 = server.arg("ps3");
    String _ps4 = server.arg("ps4");
    if(server.arg("ps1_button").length() > 0){
      if(pushClient1.pushMessage() == true) {
        sendAnswerPage("PushService1 gestartet","/parameter.htm");
      }
      else {
        sendAnswerPage("PushService1 fails","/parameter.htm");
      }
      return;
    }
    if(server.arg("ps2_button").length() > 0){
      if(pushClient2.pushMessage() == true) {
        sendAnswerPage("PushService2 gestartet","/parameter.htm");
      }
      else {
        sendAnswerPage("PushService2 fails","/parameter.htm");
      }
      return;
    }
    if(server.arg("ps3_button").length() > 0){
      if(pushClient3.pushMessage() == true) {
        sendAnswerPage("PushService3 gestartet","/parameter.htm");
      }
      else {
        sendAnswerPage("PushService3 fails","/parameter.htm");
      }
      return;
    }
    if(server.arg("ps4_button").length() > 0){
      if(pushClient4.pushMessage() == true) {
        sendAnswerPage("PushService4 gestartet","/parameter.htm");
      }
      else {
        sendAnswerPage("PushService4 fails","/parameter.htm");
      }
      return;
    }
    String _name = server.arg("name");
    String _ssid = server.arg("ssid");
    String _localip = server.arg("localip");
    String _gateway = server.arg("gateway");
    String _bssid = server.arg("bssid");
    String _pw = server.arg("pw");
    String _mode = server.arg("mode");
    String _rip = server.arg("rip");
    String _rp = server.arg("rp");
    String _dsleep = server.arg("dsleep");
    String _vcc_offset = server.arg("vcc_offset");
    String _alias = server.arg("alias");
    String _model = server.arg("model");
    String _ap_pw = server.arg("ap_pw");
    String _ap_ip = server.arg("ap_ip");
    //String _ap_gw = server.arg("ap_gw");
    String _cond1 = server.arg("condition1");
    String _action1 = server.arg("action1");
    String _cond2 = server.arg("condition2");
    String _action2 = server.arg("action2");
    String _cond3 = server.arg("condition3");
    String _action3 = server.arg("action3");
    String _cond4 = server.arg("condition4");
    String _action4 = server.arg("action4");
    String _out1_mode = server.arg("output1_mode");
    String _out2_mode = server.arg("output2_mode");
    parameter[ModulName] = toCharArr(_name);
    parameter[Ssid] = toCharArr(_ssid);
    parameter[LocalIP] = toCharArr(_localip);
    parameter[Gateway] = toCharArr(_gateway);
    parameter[Bssid] = toCharArr(_bssid);
    if(! _pw.equals("****"))
      parameter[Passphrase] = toCharArr(_pw);
    char *m;
    if(_mode[1] == '+') {
      m = new char[3];
      m[0] = _mode[0];
      m[1] = '+';
      m[2] = 0;
    }
    else {
      m = new char[2];
      m[0] = _mode[0];
      m[1] = 0;
    }
    parameter[ModulMode] = m;
    //Serial.print("ModulMode:");Serial.println(m);
    //parameter[ModulMode] = toCharArr(_mode);
    parameter[RemoteIP] = toCharArr(_rip);
    parameter[RemotePort] = toCharArr(_rp);
    parameter[DeepSleep] = toCharArr(_dsleep);
    parameter[VCC_Offset] = toCharArr(_vcc_offset);
    parameter[TempAlias] = toCharArr(_alias);
    parameter[TempSensor] = toCharArr(_model);
    //Serial.print("Speichere:");Serial.println(_model);
    parameter[APpassphrase] = toCharArr(_ap_pw);
    parameter[APip] = toCharArr(_ap_ip);
    //parameter[APgateway] = toCharArr(_ap_gw);
    parameter[PSid1] = toCharArr(_ps1);
    parameter[PSid2] = toCharArr(_ps2);
    parameter[PSid3] = toCharArr(_ps3);
    parameter[PSid4] = toCharArr(_ps4);
    parameter[Condition1] = toCharArr(_cond1);
    parameter[Action1] = toCharArr(_action1);
    parameter[Condition2] = toCharArr(_cond2);
    parameter[Action2] = toCharArr(_action2);
    parameter[Condition3] = toCharArr(_cond3);
    parameter[Action3] = toCharArr(_action3);
    parameter[Condition4] = toCharArr(_cond4);
    parameter[Action4] = toCharArr(_action4);
    parameter[OUT1_MODE] = toCharArr(_out1_mode);
    parameter[OUT2_MODE] = toCharArr(_out2_mode);
    writeModulParameter();
    p = newHtmlPage(F("Parameter aktualisiert! - Restarte Modul - dauert ca. 30 Sekunden, bis Modul bereit."));
    //p.replace("$Parameter",F("Parameter aktualisiert! - Restarte Modul - dauert ca. 30 Sekunden, bis Modul bereit."));
    server.send(200, F("text/html"), p);
    delay(2000);
    WiFi.disconnect();
    delay(2000);
    ESP.restart();
    delay(5000);
  }
  else { // Startseite Webserver
    p = getParameterPage();
    p.replace("$Parameter","");
    server.send(200, F("text/html"),p);
  }
  Serial.println(F("ParameterPage gesendet"));
}


void handleSpiffsFileUpload(){
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(filename.length() < 2) {
      Serial.println(String(F("File ungueltig:")) + filename);
      return;
    }
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print(F("handleFileUpload Name: ")); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
    Serial.println(F("Upload file"));
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.print(F("Upload closed - File-Size: ")); Serial.println(upload.totalSize);
  }
}

const __FlashStringHelper * getSupportedFileType(String filename){
  if(filename.endsWith(".css")) return F("text/css");
  else if(filename.endsWith(".js")) return F("application/javascript");
  else if(filename.endsWith(".htm")) return F("text/html");
  else if(filename.endsWith(".html")) return F("text/html");
  else if(filename.endsWith(".png")) return F("image/png");
  else if(filename.endsWith(".gif")) return F("image/gif");
  else if(filename.endsWith(".jpg")) return F("image/jpeg");
  else if(filename.endsWith(".ico")) return F("image/x-icon");
  else if(filename.endsWith(".txt")) return F("text/plain");
  else if(filename.endsWith(".pdf")) return F("application/pdf");
  else return F("");
}

/*
bool sendSpiffsFile(String path){
  Serial.println(String(F("sendSpiffsFile():")) + path);
  String contentType = getSupportedFileType(path);
  if(contentType.length()==0)
    return false;
  if(SPIFFS.exists(path)){
    Serial.println(String(F("File exists:")) + path);
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}
*/

//************  AccelGyro-Stuff *******************************
/*
#define MEDIANROUNDS 7
#define ACCINTERVAL 200
#define MEDIANAVRG 3

RunningMedian samples = RunningMedian(MEDIANROUNDS);
MPU6050 accelgyro;
int16_t ax, ay, az;
float Volt, Temperatur, Tilt;


bool initAccel() {
  if((out1_mode == NO_OUTPUT)&&(out2_mode == NO_OUTPUT)) {
    // join I2C bus (I2Cdev library doesn't do this automatically)
    Wire.begin(12, 14);  // (SDA,SCL) lt. AI-Thinker_ESP-12F.pdf GPIO2-SDA und GPIO14-SCL
    Wire.setClock(100000);
    Wire.setClockStretchLimit(2*230);

    int res = Wire.status();
    if(res != I2C_OK) {
      Serial.println(String(F("I2C ERROR: ")) + String(res));
      return false;
    }
    if(accelgyro.testConnection() == false) {
      Serial.println(F("Verbindungsaufbau zu MPU6050 gescheitert"));
      return false;
    }

    // init the Accel
    accelgyro.initialize();
    accelgyro.setDLPFMode(MPU6050_DLPF_BW_5);
    accelgyro.setTempSensorEnabled(true);
    return true;
  }
  else {
    Serial.println(F("AccelGyro kann nicht initialisiert werden --> OutputModes gesetzt"));
    return false;
  }
}


void getAccSample() {
  uint8_t res = Wire.status();
  uint8_t con = accelgyro.testConnection();
  if (res == I2C_OK && con == true)
    accelgyro.getAcceleration(&ax, &az, &ay);
  else {
    Serial.println(String("I2C ERROR: ") + res + String(" con:") + con);

  }
}


float calculateTilt() {
  float _ax = ax;
  float _ay = ay;
  float _az = az;
  float pitch = (atan2(_ay, sqrt(_ax * _ax + _az * _az))) * 180.0 / M_PI;
  float roll = (atan2(_ax, sqrt(_ay * _ay + _az * _az))) * 180.0 / M_PI;
  return sqrt(pitch * pitch + roll * roll);
}

float getTilt() {

  // make sure enough time for Acc to start
  uint32_t start = ACCINTERVAL;

  // das warten mit millis() ist umständlich
  // hier runningMedian zu bemühen sieht für mich absolut unnötig aus - kann's nicht nachvollziehen
  for (uint8_t i = 0; i < MEDIANROUNDS; i++) {
    while (millis() - start < ACCINTERVAL)
      yield();
    start = millis();
    //getAccSample();
    accelgyro.getAcceleration(&ax, &az, &ay);
    float _tilt = calculateTilt();
    Serial.print("Spl ");
    Serial.print(i);
    Serial.print(F(": "));
    Serial.println(_tilt);
    samples.add(_tilt);
  }
  return samples.getAverage(MEDIANAVRG);
}

*/
//************  Ende AccelGyro-Stuff **************************

void spindelPage() {
  String data = "";

  String page = getFileContents(F("/spindel.htm"));
  if (page.length() == 0) {
     page = newFileUploadString();
  }
  else {
    if(server.method() == HTTP_POST) { // Antwortseite Webserver
      if(server.arg("data").length() > 0){
#ifdef SPINDELCODE
        data = do_spindel();
#endif
      }
    }
    page.replace("$Daten",data);
  }
  server.send(200, F("text/html"),page);
}

void testPage() {
  if(server.method() == HTTP_POST) { // Antwortseite Webserver
    if(server.arg("test1").length() > 0){
      WiFi.disconnect();
      Serial.println("Wifi disconnected");
      connectAsStation();
      uint8_t count = 0;
      while (WiFi.status() != WL_CONNECTED) {  // Warten auf Verbindung
        count++;
        if(count > 25) {
          Serial.println(F("\r\nKonnte keine Verbindung herstellen"));
          WiFi.disconnect();
          delay(1000);
          system_deep_sleep(1*1000000); // Warte mal ne Minute ab
          delay(5000);
        }
        delay(2);
        Serial.print(".");
        delay(498);
      }
      // eigene IP
      localIp = ipToString(WiFi.localIP());
      Serial.println(F("Wlan neu verbunden"));
      server.send(200, F("text/html"),F("Wlan wurde neu gestartet"));
      return;
    }
    else {
      if(server.arg("test2").length() > 0){
        /*
        putErr(String(F("Logge Daten<br>")).c_str());
        loggeDaten();
        */
        WiFi.scanDelete();
        server.send(200, F("text/html"),F("Scan deleted"));
        return;
      }
      else {
        if(server.arg("test3").length() > 0){
          server.send(200, F("text/html"),F("test3 gedrueckt"));
        }
      }
    }
  }
  String page = getFileContents(F("/test.htm"));
  if (page.length() == 0) {
     page = newFileUploadString();
  }
  server.send(200, F("text/html"),page);
}

void loggerPage() {
  String page = getFileContents(F("/logger.htm"));
  if (page.length() == 0) {
     page = newFileUploadString();
  }
  server.send(200, F("text/html"),page);
}

void setupWebServer(){

  server.on("/logger.htm",loggerPage);

  server.on("/test.htm",testPage);

  server.on("/spindel.htm",spindelPage);

  server.on("/err.htm",errPage);

  server.on("/schalter.htm",schalterPage);

  server.on("/",webRoot);

  server.on("/upload",HTTP_GET,uploadPage);
   // 1.Callbackfunktion wird nach Upload aufgerufen
   // 2.Callbackfunktion behandelt den Upload
  server.on("/upload", HTTP_POST, [](){
          //server.sendHeader("Connection", "close");
          //server.sendHeader("Access-Control-Allow-Origin", "*");
          if(Update.hasError())
             sendAnswerPage(F("<h2>Fucking module ... Update FAIL's</h2>"));
          else
             sendAnswerPage(F("<h2>Update OK -- Have fun ;)</h2>"));
       }, handleSpiffsFileUpload);

  server.on("/parameter.htm",parameterPage);

  server.on("/gpio.htm",gpioPage);

  server.on("/help.htm",helpPage);

  server.on("/sensor.htm",sensorPage);

  server.on("/scan.htm",scanPage);

  server.on("/files.htm",filesPage);

  server.on("/hh10d.htm",hh10dPage);

  server.onNotFound([](){
    /*
    if(server.uri().equals("/")) {
      Serial.println(F("URI=/"));
      server.send(200, F("text/html"),getWebStartPage());
      return;
    }
    */
    /*
    if(!sendSpiffsFile(server.uri())){
      Serial.println(String(F("Datei nicht gefunden!")));
      server.send(404, F("text/plain"), F("FileNotFound"));
      return;
    }
    */
    String path = server.uri();
    Serial.println(String(F("URI nicht vordefiniert (FileNotFound):")) + path);
    if(SPIFFS.exists(path)){
      // File found
      Serial.println(String(F("File exists:")) + path);
      String contentType = getSupportedFileType(path);

      if(contentType.length()==0) {
        // not supported filetype !!!
        Serial.println(String(F("Filetyp is not supported:")) + path);
        server.send(404, F("text/html"),"Filetype is not supported!");
      }
      else {
        // supported filetype
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
      }
    }
    else {
      // FileNotFound
      Serial.println(String(F("File existiert auch nicht:")) + path);
      server.send(404, F("text/html"),"File not Found!");
    }
  });

  server.on("/firmware.htm", HTTP_GET,firmwarePage);

  // ****** Firmware-Upload *****************************************************
  server.on("/upgrade", HTTP_POST, [](){  // Wird nach Upload aufgerufen
       /*
       if(Update.hasError())
           sendAnswerPage(F("<h2>Fucking module ... Update FAIL's</h2>"));
        else
           sendAnswerPage(F("<h2>Update OK -- Have fun ;)</h2>"));
        */
        // Nach Upload ist Reset-Neustart erforderlich oder Restart ?????
        // ESP.reset();
        if(firmwareUpgradeFlag == false || Update.hasError() ) {
          debug(F("Firmwareupdate fehlgeschlagen"));
          sendAnswerPage(F("<h2>Fucking module ... Update FAIL's</h2>"));
          return;
        }
        sendAnswerPage(F("<h2>Update OK -- Have fun ;)</h2>"));
        if(server.arg("erase_config").length() > 0) {
          Serial.println(F("Checkbox gesetzt"));
          uint32_t start = 1048576; // ab 1MB
          for (int i=0; i < 768;i++) { // LOESCHT EEprom,SPIFFS-Filesystem und SDK-Parameter
            spi_flash_erase_sector( (start / 4096)+i );
            if (0 ==(i % 16))
            Serial.print(".");
          }
          Serial.println(F("Erase done"));
        }
        else
          Serial.println(F("Checkbox nicht gesetzt"));
        delay(1000);  // evtl yield() benutzen ?????????
        ESP.restart();
        delay(5000);
        },

        []() {
        HTTPUpload& upload = server.upload();
        if(upload.status == UPLOAD_FILE_START) { // behandle Upload
          firmwareUpgradeFlag = true;
          if(upload.filename.length() < 2){
            firmwareUpgradeFlag = false;
            yield();
            return;
          }
          WiFiUDP::stopAll();
          Serial.printf("Update: %s\r\n", upload.filename.c_str());
          uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          if(!Update.begin(maxSketchSpace)) { //start with max available size
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_WRITE){
          if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            Update.printError(Serial);
          }
        } else if(upload.status == UPLOAD_FILE_END){
          if(Update.end(true)){ //true to set the size to the current progress
            Serial.printf("Update Success: %u\r\nPrepare rebooting...\r\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
        yield();
        });
     // ***********************************************************************************
}


/*

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  Serial.println("--handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
      for (uint8_t i=0; i<server.args(); i++){
      Serial.println(server.argName(i) + ": " + server.arg(i) + "\r\n");
    }
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}

  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  server.send(200, "text/json", output);
}



  server.on("/", HTTP_POST, [](){ server.send(200, "text/plain",sa ); }, handleSpiffsFileUpload);

  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "404 - FileNotFound");
  });
  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });

  */
