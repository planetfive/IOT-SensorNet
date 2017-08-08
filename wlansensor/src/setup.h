#include <IPAddress.h>
#ifndef SETUP_H
#define SETUP_H

// doubleReset-detection
#define RESET_VALUE 0xA00A // magic-number
extern void writeResetValue(uint16_t v);
extern uint16_t readResetValue();

extern void writeEeprom(String data);
extern void clearEeprom();

extern boolean readModulParameter();
extern void writeModulParameter();

extern void setTemperaturModel();
extern void setOutputMode();

char* readSeriellParameter(char* name);

extern void printWelcome();

extern void putErr(const char* err);
extern String getErr(void);

extern double getVCC();

extern char *toCharArr(String s);

#endif
