
#ifndef WEBSERVER_H
#define WEBSERVER_H

extern void setupWebServer();
extern String getModulInfo();
extern String formatBytes(size_t bytes);
extern String getNetworkInfo();
extern String ipToString(IPAddress ip);

#endif


