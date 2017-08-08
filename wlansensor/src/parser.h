#ifndef PARSER_H
#define PARSER_H

#include "onewire.h"
#include "wlanSensor.h"

// Condition und Actionverwaltung

/* ******   Variablensubstitution für fogenden Variablen: *********
   $in  --> GPIO12 def IN_PIN
   $vcc --> Die intern gemessene VCC-Spannung des Moduls
   $dht_t --> Die Temperatur des DHT22
   $dht_f --> Die Feuchte des DHT22
   $romcode --> Die Temperatur des benannten DS1820-Temperatursensors
   $lifetime --> Falls die Lifetime eines Sensors überschritten wird

   Actiondefinitionen:
   actionPushService --> ruft den Pushservice auf


*******************************************************************/
class Parser {

private:

long stapel[20];
uint8_t ptr = 0, errNo = 0;
long op1,op2;

void push(long w);
long pop();

public:

long parse(String s); // neg Rückgabewert ist Fehlerfall
//int action(void (*action)()) { action(); };

boolean getErr() { return errNo; }

};




// **************** Temperaturaliase parsen   **********

#define ALIASE_MAX MAX_SENSORS

class AliasParser {

private:

  struct AliasDict {
    char* key;
    char* value;
  } aliase[ALIASE_MAX];

  uint8_t dictCount = 0;

public:

  void parseAliase(String aliase);
  String substitute(String s);
  String getValue(String key);

};

#endif
