#include <Arduino.h>
#include "FS.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "wlanSensor.h"
#include "webServer.h"
#include "setup.h"

String dirFiles() {
  String files = "";
  int i;
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    files +=dir.fileName();
    File f = dir.openFile("r");
    files += String(" - ") + formatBytes(f.size());
    files += F("\r\n");
    f.close();
  }
  return files;
}

boolean deleteFile(const char* fName) {
  boolean ok = SPIFFS.remove(fName);
  if(!ok)
    debug(String(fName) + String(F(" kann nicht geloescht werden")));
  return ok;
}

boolean writeFile(const char* fName, const char* txt){
  File f = SPIFFS.open(fName, "w");
  if (!f) {
     debug(String(fName) + String(F(" kann nicht geoeffnet werden")));
     return false;
  }
  f.write((uint8_t*)txt,strlen(txt));
  f.close();
  return true;
}


String _getFileContents(String filename) {
  String page = "";
  File f = SPIFFS.open(filename, "r");
  if (!f) {
    debug(filename + String(F(" kann nicht geoeffnet werden")));
    return page;
  }
  while(f.available()){
    page += (char) f.read();
  }
  f.close();
  page.replace("$MODULNAME",parameter[ModulName]);
  return page;
}

String getFileContents(const char* filename) {
  return _getFileContents(String(filename));
}

String getFileContents(const __FlashStringHelper *filename) {
  return _getFileContents(String(filename));
}

void showFiles(){
  Serial.println(dirFiles());
}

//****************************************************************************************


boolean uploadFile(char* url,char* file) {  // Upload von einer Webserver-URL
  HTTPClient http;

  File f = SPIFFS.open(file, "w");
  if (!f) {
     Serial.println(F("file open failed"));
     return false;
  }

   // configure server and url
   http.begin(url);
   Serial.println(F("[HTTP] GET..."));
   // start connection and send HTTP header
   int httpCode = http.GET();
   if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\r\n", httpCode);
      // file found at server
      if(httpCode == HTTP_CODE_OK) {
        // get lenght of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();
        // create buffer for read
        uint8_t buff[128] = { 0 };
        // get tcp stream
        WiFiClient * stream = http.getStreamPtr();
        // read all data from server
        while(http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();
          if(size) {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            // write it to Serial
            //Serial.write(buff, c);
            f.write(buff,c);
              if(len > 0) {
                len -= c;
              }
            }
            delay(1);
          }
          Serial.println();
          Serial.print(F("[HTTP] connection closed or file end.\n"));
      }
   }
   else {
    Serial.printf("[HTTP] GET... failed, error: %s\r\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
  f.close();
  http.end();
  return true;
}
