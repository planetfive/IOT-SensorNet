#ifndef LOGGER_H
#define LOGGER_H
#include <Arduino.h>

#define LOGGERFILE "/logger.txt"
#define LOGGERFILE_MAX_LENGTH 20000  // Filelen in byte

// **** RTC-maximale Größe ist 512 Byte

struct RTC_DEF {
  uint16_t resetValue;
  char id[10];
  uint16_t rtcPos;
  unsigned long time;
  size_t filepos;
  float mem[100];
} ;

extern void read_rtc();
extern void loggeDatenInFlash(RTC_DEF *rtc);
extern void runLoggerMode();
extern boolean loggeDaten();

#endif
