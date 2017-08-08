
#ifndef NTPCLIENT_H
#define NTPCLIENT_H

#include <Arduino.h>


extern unsigned long getNtpTime();
extern void getTime(unsigned long unixTime, int* hour, int* min, int* sec);
extern String printDate (unsigned long epoch);

#endif
