/*
 * Adaptation of Paul Stoffregen's One wire library to the ESP8266 and
 * Necromant's Frankenstein firmware by Erland Lewin <erland@lewin.nu>
 *
 * Paul's original library site:
 *   http://www.pjrc.com/teensy/td_libs_OneWire.html
 *
 * See also http://playground.arduino.cc/Learning/OneWire
 *
 * modified by R.Kreisel to use in Projekt
 *          https://github.com/planetfive/IOT-SensorNet
 */

 /*
Copyright (c) 2007, Jim Studt  (original old version - many contributors since)
The latest version of this library may be found at:
  http://www.pjrc.com/teensy/td_libs_OneWire.html
OneWire has been maintained by Paul Stoffregen (paul@pjrc.com) since
January 2010.
DO NOT EMAIL for technical support, especially not for ESP chips!
All project support questions must be posted on public forums
relevant to the board or chips used.  If using Arduino, post on
Arduino's forum.  If using ESP, post on the ESP community forums.
There is ABSOLUTELY NO TECH SUPPORT BY PRIVATE EMAIL!
Github's issue tracker for OneWire should be used only to report
specific bugs.  DO NOT request project support via Github.  All
project and tech support questions must be posted on forums, not
github issues.  If you experience a problem and you are not
absolutely sure it's an issue with the library, ask on a forum
first.  Only use github to report issues after experts have
confirmed the issue is with OneWire rather than your project.
Back in 2010, OneWire was in need of many bug fixes, but had
been abandoned the original author (Jim Studt).  None of the known
contributors were interested in maintaining OneWire.  Paul typically
works on OneWire every 6 to 12 months.  Patches usually wait that
long.  If anyone is interested in more actively maintaining OneWire,
please contact Paul (this is pretty much the only reason to use
private email about OneWire).
OneWire is now very mature code.  No changes other than adding
definitions for newer hardware support are anticipated.
Version 2.3:
  Unknown chip fallback mode, Roger Clark
  Teensy-LC compatibility, Paul Stoffregen
  Search bug fix, Love Nystrom
Version 2.2:
  Teensy 3.0 compatibility, Paul Stoffregen, paul@pjrc.com
  Arduino Due compatibility, http://arduino.cc/forum/index.php?topic=141030
  Fix DS18B20 example negative temperature
  Fix DS18B20 example's low res modes, Ken Butcher
  Improve reset timing, Mark Tillotson
  Add const qualifiers, Bertrik Sikken
  Add initial value input to crc16, Bertrik Sikken
  Add target_search() function, Scott Roberts
Version 2.1:
  Arduino 1.0 compatibility, Paul Stoffregen
  Improve temperature example, Paul Stoffregen
  DS250x_PROM example, Guillermo Lovato
  PIC32 (chipKit) compatibility, Jason Dangel, dangel.jason AT gmail.com
  Improvements from Glenn Trewitt:
  - crc16() now works
  - check_crc16() does all of calculation/checking work.
  - Added read_bytes() and write_bytes(), to reduce tedious loops.
  - Added ds2408 example.
  Delete very old, out-of-date readme file (info is here)
Version 2.0: Modifications by Paul Stoffregen, January 2010:
http://www.pjrc.com/teensy/td_libs_OneWire.html
  Search fix from Robin James
    http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27
  Use direct optimized I/O in all cases
  Disable interrupts during timing critical sections
    (this solves many random communication errors)
  Disable interrupts during read-modify-write I/O
  Reduce RAM consumption by eliminating unnecessary
    variables and trimming many to 8 bits
  Optimize both crc8 - table version moved to flash
Modified to work with larger numbers of devices - avoids loop.
Tested in Arduino 11 alpha with 12 sensors.
26 Sept 2008 -- Robin James
http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295/27#27
Updated to work with arduino-0008 and to include skip() as of
2007/07/06. --RJL20
Modified to calculate the 8-bit CRC directly, avoiding the need for
the 256-byte lookup table to be loaded in RAM.  Tested in arduino-0010
-- Tom Pollard, Jan 23, 2008
Jim Studt's original library was modified by Josh Larios.
Tom Pollard, pollard@alum.mit.edu, contributed around May 20, 2008
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:
The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
Much of the code was inspired by Derek Yerger's code, though I don't
think much of that remains.  In any event that was..
    (copyleft) 2006 by Derek Yerger - Free to distribute freely.
The CRC code was excerpted and inspired by the Dallas Semiconductor
sample code bearing this copyright.
//---------------------------------------------------------------------------
// Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Dallas Semiconductor
// shall not be used except as stated in the Dallas Semiconductor
// Branding Policy.
//--------------------------------------------------------------------------
*/

#include <Arduino.h>

#include "onewire.h"
#include "FS.h"
#include "parser.h"

extern "C" {

#include "user_interface.h"
#include "eagle_soc.h"
#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "os_type.h"

// *** static c-function prototypes ****

void os_delay_us(int);
void wdt_feed();
};

struct DS1820_DEF ds1820[MAX_SENSORS];

static int ds_search( uint8_t *addr );
static void reset_search();
static void write_bit( int v );
static void write( uint8_t v, int parasitePower );
static inline int read_bit(void);
static inline void write_bit( int v );
static void select(const uint8_t rom[8]);
void ds_init( int gpio );
static uint8_t reset(void);
static uint8_t read();
static uint8_t crc8(const uint8_t *addr, uint8_t len);


boolean getDs1820Temp(const char* romcode, float* t) {
  // Serial.println("Romcode:" + String(romcode));
  for(uint8_t i=0; i < MAX_SENSORS; i++) {
    if(strcmp(romcode,ds1820[i].romcode)==0) {
      *t = ds1820[i].temperatur;
      Serial.println(String(ds1820[i].romcode) + String(" - ") + String(*t));
      return true;
    }
  }
  return false;
}

/*
String getDs1820TempFromArr() {
  String s = "";
  String alias,romcode;
  AliasParser p;
  for(uint8_t i=0; i < MAX_SENSORS; i++) {
    if(ds1820[i].romcode[0] == 0)
      break;
    alias = p.getValue(String(ds1820[i].romcode));
    if(alias.length() > 0)
      romcode = alias;
    else
      romcode = ds1820[i].romcode;
    if(s.length() > 0)
      s = s + String('\n'); // + romcode + String(" - ") + String(ds1820[i].temperatur);
    s = s + romcode + String("-") + String(ds1820[i].temperatur);
  }
  return s;
}
*/

/*******************************************************************************************
 * DS 18x20 stuff
 *******************************************************************************************/
#ifndef DS_PIN
String do_ds18b20(boolean getall) {
  return "DS_PIN ist nicht definiert";
}
#endif

#ifdef DS_PIN
String do_ds18b20(boolean getall) {
  //Serial.print("array:");Serial.println((String(sizeof(ds1820))));
  memset(ds1820,0,sizeof(ds1820));
  char temperatur[MAX_T_CHARS];
  int gpio = DS_PIN;
  int r, i;
  uint8_t addr[8], data[12], arrayPos = 0;
  bool first = true;
  String t = "";
  //Serial.println( "Starte Temperaturwandlung" );
  ds_init( gpio );

  reset();

  write( DS1820_SKIP_ROM, 1 );
  write( DS1820_CONVERT_T, 1 );


  //750ms 1x, 375ms 0.5x, 188ms 0.25x, 94ms 0.12x
  os_delay_us( 750*1000 );
  wdt_feed();

  reset_search();
  do{
    r = ds_search( addr );
    if( r )
    {

       if( crc8( addr, 7 ) != addr[7] ) {
          sprintf(temperatur, "CRC mismatch, crc=%xd, addr[7]=%xd\n", crc8( addr, 7 ), addr[7] );
          // Serial.println(temperatur);
       }
      switch( addr[0] )
      {
        case DS18S20:
          //Serial.println( "Device is DS18S20 family." );
          break;

        case DS18B20:
          //Serial.println( "Device is DS18B20 family." );
          break;

        default:
          //Serial.println( "Device is unknown family." );
          return t;
      }

    }
    else {
      if(!getall){
        //Serial.println( "No more DS18x20 detected, sorry" );
        return t;
      }
      else {
        //Serial.println("Temperaturmessung fails !");
        break;
      }
    }
    reset();
    select( addr );
    write( DS1820_READ_SCRATCHPAD, 0 );

    for( i = 0; i < 9; i++ )
    {
      data[i] = read();
    }

    // float arithmetic isn't really necessary, tVal and tFract are in 1/10 °C
    uint16_t tVal, tFract;
    char* tSign;

    tVal = (data[1] << 8) | data[0];
    if (tVal & 0x8000) {
      tVal = (tVal ^ 0xffff) + 1;       // 2's complement
      tSign = (char*)"-";
    } else
      tSign = (char*)"";

    // datasize differs between DS18S20 and DS18B20 - 9bit vs 12bit
    if (addr[0] == DS18S20) {
      tFract = (tVal & 0x01) ? 50 : 0;    // 1bit Fract for DS18S20
      tVal >>= 1;
    } else {
      tFract = (tVal & 0x0f) * 100 / 16;    // 4bit Fract for DS18B20
      tVal >>= 4;
    }
    sprintf(ds1820[arrayPos].romcode,"%02x%02x%02x%02x%02x%02x%02x%02x",addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
    sprintf(temperatur,"%s%d.%02d",tSign, tVal, tFract);
    ds1820[arrayPos].temperatur = atof(temperatur);
    //Serial.println("DS1820:" + String(ds1820[arrayPos].romcode) + " - " + String(ds1820[arrayPos].temperatur));
    if(arrayPos < (MAX_SENSORS -1))
      arrayPos++;
    sprintf(temperatur,"%02x%02x%02x%02x%02x%02x%02x%02x:%s%d.%02d°C",addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],tSign, tVal, tFract); // 30 Zeichen incl 0
    // Serial.println(temperatur);
    if(first) {
      t = String(temperatur);
      first = false;
      yield();
    }
    else {
      t +=  String(" ") + String(temperatur);
      yield();
    }
    if(!getall){
       return t;
    }
   } while(getall);

  return t;
}
#endif

// global search state
static unsigned char ROM_NO[8];
static uint8_t LastDiscrepancy;
static uint8_t LastFamilyDiscrepancy;
static uint8_t LastDeviceFlag;
static int gpioPin;


void ds_init( int gpio ) {
  pinMode(gpio, INPUT_PULLUP); // toDo: testen mit INPUT
  gpioPin = gpio;
}


static void reset_search()
{
  // reset the search state
  LastDiscrepancy = 0;
  LastDeviceFlag = FALSE;
  LastFamilyDiscrepancy = 0;
  for(int i = 7; ; i--) {
    ROM_NO[i] = 0;
    if ( i == 0) break;
  }
}

// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
static uint8_t reset(void)
{
  int r;
  uint8_t retries = 125;

  GPIO_DIS_OUTPUT( gpioPin );

  // wait until the wire is high... just in case
  do {
    if (--retries == 0) return 0;
    os_delay_us(2);
  } while ( !GPIO_INPUT_GET( gpioPin ));

  GPIO_OUTPUT_SET( gpioPin, 0 );
  os_delay_us(480);
  GPIO_DIS_OUTPUT( gpioPin );
  os_delay_us(70);
  r = !GPIO_INPUT_GET( gpioPin );
  os_delay_us(410);

  return r;
}

/* pass array of 8 bytes in */
static int ds_search( uint8_t *newAddr )
{
  uint8_t id_bit_number;
  uint8_t last_zero, rom_byte_number;
  uint8_t id_bit, cmp_id_bit;
  int search_result;

  unsigned char rom_byte_mask, search_direction;

  // initialize for search
  id_bit_number = 1;
  last_zero = 0;
  rom_byte_number = 0;
  rom_byte_mask = 1;
  search_result = 0;

  // if the last call was not the last one
  if (!LastDeviceFlag)
  {
    // 1-Wire reset
    if (!reset())
    {
      // reset the search
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      return FALSE;
    }

    // issue the search command
    write(DS1820_SEARCHROM, 0);

    // loop to do the search
    do
    {
      // read a bit and its complement
      id_bit = read_bit();
      cmp_id_bit = read_bit();

      // check for no devices on 1-wire
      if ((id_bit == 1) && (cmp_id_bit == 1))
        break;
      else
      {
        // all devices coupled have 0 or 1
        if (id_bit != cmp_id_bit)
          search_direction = id_bit;  // bit write value for search
        else
        {
          // if this discrepancy if before the Last Discrepancy
          // on a previous next then pick the same as last time
          if (id_bit_number < LastDiscrepancy)
            search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
          else
            // if equal to last pick 1, if not then pick 0
            search_direction = (id_bit_number == LastDiscrepancy);

          // if 0 was picked then record its position in LastZero
          if (search_direction == 0)
          {
            last_zero = id_bit_number;

            // check for Last discrepancy in family
            if (last_zero < 9)
              LastFamilyDiscrepancy = last_zero;
          }
        }

        // set or clear the bit in the ROM byte rom_byte_number
        // with mask rom_byte_mask
        if (search_direction == 1)
          ROM_NO[rom_byte_number] |= rom_byte_mask;
        else
          ROM_NO[rom_byte_number] &= ~rom_byte_mask;

        // serial number search direction write bit
        write_bit(search_direction);

        // increment the byte counter id_bit_number
        // and shift the mask rom_byte_mask
        id_bit_number++;
        rom_byte_mask <<= 1;

        // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
        if (rom_byte_mask == 0)
        {
          rom_byte_number++;
          rom_byte_mask = 1;
        }
      }
    }
    while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

    // if the search was successful then
    if (!(id_bit_number < 65))
    {
      // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
      LastDiscrepancy = last_zero;

      // check for last device
      if (LastDiscrepancy == 0)
        LastDeviceFlag = TRUE;

      search_result = TRUE;
    }
  }

  // if no device found then reset counters so next 'search' will be like a first
  if (!search_result || !ROM_NO[0])
  {
    LastDiscrepancy = 0;
    LastDeviceFlag = FALSE;
    LastFamilyDiscrepancy = 0;
    search_result = FALSE;
  }
  for (int i = 0; i < 8; i++) newAddr[i] = ROM_NO[i];
  return search_result;
}

//
// Write a byte. The writing code uses the active drivers to raise the
// pin high, if you need power after the write (e.g. DS18S20 in
// parasite power mode) then set 'power' to 1, otherwise the pin will
// go tri-state at the end of the write to avoid heating in a short or
// other mishap.
//
static void write( uint8_t v, int power ) {
  uint8_t bitMask;

  for (bitMask = 0x01; bitMask; bitMask <<= 1) {
    write_bit( (bitMask & v)?1:0);
  }
  if ( !power) {
    GPIO_DIS_OUTPUT( gpioPin );
    GPIO_OUTPUT_SET( gpioPin, 0 );
  }
}

//
// Write a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
static inline void write_bit( int v )
{
  GPIO_OUTPUT_SET( gpioPin, 0 );
  if( v ) {
    os_delay_us(10);
    GPIO_OUTPUT_SET( gpioPin, 1 );
    os_delay_us(55);
  } else {
    os_delay_us(65);
    GPIO_OUTPUT_SET( gpioPin, 1 );
    os_delay_us(5);
  }
}

//
// Read a bit. Port and bit is used to cut lookup time and provide
// more certain timing.
//
static inline int read_bit(void)
{
  int r;
  GPIO_OUTPUT_SET( gpioPin, 0 );
  os_delay_us(3);
  GPIO_DIS_OUTPUT( gpioPin );
  os_delay_us(10);
  r = GPIO_INPUT_GET( gpioPin );
  os_delay_us(53);
  return r;
}

//
// Do a ROM select
//
static void select(const uint8_t *rom)
{
  uint8_t i;
  write(DS1820_MATCHROM, 0); // Choose ROM
  for (i = 0; i < 8; i++)
     write(rom[i], 0);
}

//
// Read a byte
//
static uint8_t read() {
  uint8_t bitMask;
  uint8_t r = 0;
  for (bitMask = 0x01; bitMask; bitMask <<= 1) {
    if ( read_bit()) r |= bitMask;
  }
  return r;
}

//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but much smaller, than the lookup table.
//
static uint8_t crc8(const uint8_t *addr, uint8_t len)
{
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix)
         crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}
