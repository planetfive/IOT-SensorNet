
#include "pushClient.h"
#include "wlanSensor.h"
#include "setup.h"

// char* pushServerAddress = "api.pushingbox.com";

boolean PushClient::setId(const char *id) {
  status = Idle;
  if(strlen(id)==0)
    return false;
  deviceId = id;
  return true;
}

boolean PushClient::pushMessage(const char *ss) {
  if(ss != NULL) {
    msg = String(ss);
  }
  if(status == Progress) { // I'm working
    return false;
  }
  if(ntpTime < PushClient::lockTime) {
    return false;
  }
  if(ntpTime < failTime) {
    debug(F("PushService im Fail-Delay<br/>"));
    return false;
  }
  if(wClient.connect(_pushServerAddress,80) == 0) {
     debug(F("Connection to PushService failed"));
     PushClient::status = Fail;
     wClient.stop();
     failTime = ntpTime + 30; // 30 Sekunden warten
     return false;
  }
  // Connect and send deviceId
  PushClient::startTime = ntpTime;
  wClient.print(F("GET /pushingbox?devid="));
  wClient.print(deviceId);
  Serial.print(deviceId);
  if(msg.length() > 0) {
    wClient.print(F("&var="));
    wClient.print(msg);
  }
  wClient.println(F(" HTTP/1.1"));
  wClient.print(F("Host: "));
  wClient.println(_pushServerAddress);
  wClient.println(F("User-Agent: ESP8266"));
  wClient.println();
  PushClient::status = Progress;
  PushClient::lockTime = ntpTime + PushClient::_lock;
  return true;
}


void PushClient::handle() {
  if(ntpTime < failTime)
    return;
  if(PushClient::status == Fail) {
    PushClient::status = Idle;
    pushMessage();
    return;
  }
  if(PushClient::status != Progress) // nothing to do
     return;
  if((ntpTime - PushClient::startTime) > PushClient::timeOut) { // TimeOut
    debug(F("TimeOut:No answer from pushServer"));
    PushClient::status = Fail;
    wClient.stop();
    // hier kann gleich pushMessage() getriggert werden, da die TimeOut-Zeit verstrichen ist
    return;
  }
  // no answer available
  if(wClient.available() == false) {
    return;
  }

  // ok, pushServer answer
  while(wClient.available()) {
    char c = wClient.read();
    // toDo --> auswerten ob antwort ok und status entsprechend setzen
  }
  wClient.stop();
  PushClient::status = Success;
  return;
}
