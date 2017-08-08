#include <Arduino.h>
#include "parser.h"
#include "wlanSensor.h"
#include "split.h"
#include "setup.h"
#include "DHT.h"
#include "ntpClient.h"

extern DHT dht;

boolean isZahl(String s){
  uint8_t len = s.length();
  for(uint8_t i=0; i < len; i++){
    if(isAlphaNumeric(s.charAt(i))==false)
       return false;
  }
  return true;
}

void Parser::push(long w) {
  if(ptr > 19){
     errNo = 1;
     debug(F("push():Error - Stackspitze erreicht"));
     return;
  }
  stapel[ptr++] = w;
  errNo = 0;
  return ;
}

long Parser::pop() {
  if(ptr == 0) {
    errNo = 2;
    debug(F("pop():Error - Stackbasis erreicht"));
    return 0;
  }
  int w = stapel[--ptr];
  errNo = 0;
  return w;
}


long Parser::parse(String s) {
  long w;
  String token;
  Split tokens(s,' ');
  while(tokens.next()) {
    token = tokens.getNext();
    if(token.charAt(0) == '$') {  // Substitution der Sensoren bzw des digitalen Eingang
      if(token.equals("$in")) {
        push(digitalRead(IN_PIN));
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$vcc")) {
        w = getVCC()*100;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$dht_t")) {
        w = dhtTemp*10;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$dht_f")) {
        w = dhtHum*10;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
#ifdef SPINDELCODE
      if(token.equals("$spTilt")) {
        w = spTilt*100;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$spTemp")) {
        w = spTemp*100;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$spPlato")) {
        w = spPlato*100;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
#endif            
      if(token.equals("$time")) {
        if(ntpTime < 50000) { // nehme mal an, dass keine Zeit geholt wurde
          ntpTime = getNtpTime(); // Zeit setzen
        }
        w = ntpTime % 86400L; // Rückgabewert ist der angefangene Tag in Sekunden
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      if(token.equals("$lifetime")) {
        push(lifetimeFlag != 0);
        if(errNo) {
          return 0;
        }
        lifetimeFlag = false;
        continue;
      }
      // ds1820-temperaturen setzen sich zusammen aus: $ und romcode
      float temp;
      //yield();
      //do_ds18b20(true);
      //yield();
      if(getDs1820Temp(token.c_str()+1, &temp)) {
        w = temp *10;
        push(w);
        if(errNo) {
          return 0;
        }
        continue;
      }
      debug(F("parse() Error: Variable ist nicht bekannt"));
      return 0;
    }
    if(isZahl(token)) { // eine Zahl wurde erkannt
      push(token.toInt());
      if(errNo) {
        return 0;
      }
      continue;
    }
    // hier kommen die Aktionen
    if(token.equals("+")) {
        // addieren
        op2 = pop();
        if(errNo)
           return 0;
        op1 = pop();
        if(errNo)
           return 0;
        push(op1 + op2);
        if(errNo)
           return 0;
        continue;
    }
    if(token.equals("-")) {
          // subtrahieren
          op2 = pop();
          if(errNo)
           return 0;
          op1 = pop();
          if(errNo)
           return 0;
          push(op1 - op2);
          if(errNo)
           return 0;
          continue;
    }
    if(token.equals(">")) {
            // groesser als
            op2 = pop();
            if(errNo)
              return 0;
            op1 = pop();
            if(errNo)
              return 0;
            push(op1 > op2);
            if(errNo)
              return 0;
            continue;
    }
    if(token.equals("<")) {
              // kleiner als
              op2 = pop();
              if(errNo)
                return 0;
              op1 = pop();
              if(errNo)
                return 0;
              push(op1 < op2);
              if(errNo)
                return 0;
              continue;
    }
    if(token.equals("==")) {
              // gleich
              op2 = pop();
              if(errNo)
                return 0;
              op1 = pop();
              if(errNo)
                return 0;
              push(op1 == op2);
              if(errNo)
                return 0;
              continue;
    }
    if(token.equals("!=")) {
             // nicht gleich
             op2 = pop();
             if(errNo)
               return 0;
             op1 = pop();
             if(errNo)
               return 0;
             push(op1 != op2);
             if(errNo)
               return 0;
             continue;
    }
    if(token.equals("&&")) {
             // logisches und
             op2 = pop();
             if(errNo)
               return 0;
             op1 = pop();
             if(errNo)
               return 0;
             push(op1 && op2);
             if(errNo)
               return 0;
             continue;
    }
    if(token.equals("||")) {
             // logisches or
             op2 = pop();
             if(errNo)
               return 0;
             op1 = pop();
             if(errNo)
               return 0;
             push(op1 || op2);
             if(errNo)
               return 0;
             continue;
    }
    if(token.equals("!")) {
      op1 = pop();
      if(errNo)
        return 0;
      push(! op1);
      if(errNo)
        return 0;
      continue;
    }
    debug(String(F("parse() Error: Token unbekannt")) + String(token));
    errNo = 3;
    return 0;
    }
    w = pop();
    if(errNo)
      return 0;
    return w;
}


// **************** Temperaturaliase parsen   **********

void AliasParser::parseAliase(String s) {
  if(s.length()==0)
     return;
  String dictS;
  Split split(s,';');
  uint8_t count = split.getCount();
  //Serial.println("\r\nCount:" + String(count));
  if(count == 0) {
    // sollte nie auftreten, weil der String oben schon auf Länge null geprüft wurde ;)
     return;
   }
  uint8_t pos = 0;
  while(split.next()) {
    dictS = split.getNext();
    Split dict(dictS,':');
    if(dict.getCount() != 2) {
      debug(F("Alias und Romadresse bitte durch \":\" trennen"));
      return;
    }
    aliase[pos].key = toCharArr(dict.getNext());
    aliase[pos].value = toCharArr(dict.getNext());
    //Serial.println(String(aliase[pos].key));
    //Serial.println(String(aliase[pos].value));
    pos++;
    if(pos ==  ALIASE_MAX)
       break;
  }
  dictCount = pos;
}

String AliasParser::substitute(String s) {
  for(uint8_t i=0; i < dictCount; i++) {
    s.replace(aliase[i].key,aliase[i].value);
  }
  return s;
}

String AliasParser::getValue(String key) {
  String s = "";
  if(dictCount == 0)
     return s;
  for(uint8_t i=0; i < dictCount; i++) {
    if(key.equals(aliase[i].key)) {
      s = aliase[i].value;
      return s;
    }
  }
  return s;
}
