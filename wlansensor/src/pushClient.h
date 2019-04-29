#ifndef PUSHCLIENT_H
#define PUSHCLIENT_H

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <time.h>

enum Status { Idle, Success, Fail, Progress };

class PushClient {

  WiFiClient wClient;
  Status status = Idle;
  const char* _pushServerAddress = "api.pushingbox.com";
  const char *deviceId;
  time_t startTime,lockTime,failTime;
  int timeOut = 20; // Sekunden
  uint16_t _lock = 3600;
  String msg;

  public:

  boolean setId(const char *id);

  Status getStatus() { return status; };

  void handle();

  boolean pushMessage(const char *ss = NULL);


};

#endif
