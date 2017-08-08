#ifndef ONEWIRE_H
#define ONEWIRE_H

#define MAX_SENSORS 4
#define MAX_T_CHARS 130  // Maximale Anzahl Zeichen für Temperaturübertragung entspricht 4 Sensoren maximal

#define DS1820_WRITE_SCRATCHPAD  0x4E
#define DS1820_READ_SCRATCHPAD  0xBE
#define DS1820_COPY_SCRATCHPAD  0x48
#define DS1820_READ_EEPROM    0xB8
#define DS1820_READ_PWRSUPPLY 0xB4
#define DS1820_SEARCHROM    0xF0
#define DS1820_SKIP_ROM     0xCC
#define DS1820_READROM      0x33
#define DS1820_MATCHROM     0x55
#define DS1820_ALARMSEARCH    0xEC
#define DS1820_CONVERT_T    0x44
#define FALSE 0
#define TRUE 1

// DS18x20 family codes
#define DS18S20   0x10
#define DS18B20   0x28

// Die Temperaturmessung erfolgt nur wenn der DS_PIN definiert ist !!!!!
#define DS_PIN 13   // GPIO13

struct DS1820_DEF {
  char romcode[17];
  float temperatur;
};

extern String do_ds18b20(boolean all);
extern struct DS1820_DEF ds1820[MAX_SENSORS];
extern boolean getDs1820Temp(const char* romcode, float* t);
extern String getDs1820TempFromArr();

#endif
