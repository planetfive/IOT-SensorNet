
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "wlanSensor.h"
#include "setup.h"
#include "spiffs.h"

#define NTP_PORT 2390       // local port to listen for UDP packets
#define NTP_PACKET_SIZE 48    // NTP time stamp is in the first 48 bytes of the message

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
// IPAddress timeServerIP; // time.nist.gov NTP server address

// const char* ntpServerName = "time.nist.gov";  // 128.138.141.172

byte pBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udpNtp;

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  // set all bytes in the buffer to 0
  memset(pBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  pBuffer[0] = 0b11100011;   // LI, Version, Mode
  pBuffer[1] = 0;     // Stratum, or type of clock
  pBuffer[2] = 6;     // Polling Interval
  pBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pBuffer[12]  = 49;
  pBuffer[13]  = 0x4E;
  pBuffer[14]  = 49;
  pBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udpNtp.beginPacket(address, 123); //NTP requests are to port 123
  udpNtp.write(pBuffer, NTP_PACKET_SIZE);
  udpNtp.endPacket();
}

int doNtp(IPAddress& address) {
  sendNTPpacket(address); // send an NTP packet to a time server
  // wait to see if a reply is available
  int cb;
  // Serial.print(F("Wait for reply"));
  for(int j=0; j < 300; j++) {
    cb = udpNtp.parsePacket();
    // Serial.print('.');
    yield();
    if (cb) {
      break;
    }
    delay(10);
  }
  return cb;
}

unsigned long getNtpTime() {
  udpNtp.begin(NTP_PORT);
  int cb;
  IPAddress localIp,timeServerIP;
  timeServerIP[0]= 128;
  timeServerIP[1]= 138;
  timeServerIP[2]= 141;
  timeServerIP[3]= 172;
  String localNtpAddress = getFileContents(F("/ntp.txt"));
  if(localNtpAddress.length() > 0) {
    if(localIp.fromString(localNtpAddress)) {
      debug(F("got local NTP-Address"));
      cb = doNtp(localIp);
    }
    else
      cb = doNtp(timeServerIP);
  }
  else
    cb = doNtp(timeServerIP);

  // Serial.println("Get ntp-Time");
  if(!cb) {
    debug(F("Cant get Time by IP-Address"));
    debug(F("Try by name ... "));
    if( WiFi.hostByName(String(F("time.nist.gov")).c_str(), timeServerIP) == 0) {
      timeServerIP[0]= 128;
      timeServerIP[1]= 138;
      timeServerIP[2]= 141;
      timeServerIP[3]= 172;
      debug(F("Fail"));
    }
    else
       debug(F("OK - got IP-Address"));
    debug(F("Now, try it again"));
    cb = doNtp(timeServerIP);
  }
  if(cb) {
    //debug(F("NTP-Request: packet received"));
    // We've received a packet, read the data from it
    udpNtp.read(pBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(pBuffer[40], pBuffer[41]);
    unsigned long lowWord = word(pBuffer[42], pBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - 2208988800UL;
    // print Unix time:
    debug(String(F("Unix-Time: ")) + String(epoch));
    udpNtp.stop();
    return epoch;
  }
  debug(F("NTP-Request: no packet received"));
  return 0;
}

void getTime(unsigned long unixTime, int  *hour, int* min, int* sec) {
    // print the hour, minute and second:
    *hour = (unixTime % 86400L) / 3600;
    *min = (unixTime  % 3600) / 60;
    *sec = unixTime % 60;
    //Serial.println("The UTC time is " + String(*hour) + ":" + String(*min) + ":" + String(*sec));       // UTC is the time at Greenwich Meridian (GMT)
}

#define LEAP_YEAR(Y)     ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

String printDate (unsigned long epoch)
{
  String DateString = "";
  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;
  time = (uint32_t)epoch;
  time /= 60; // now it is minutes
  time /= 60; // now it is hours
  time /= 24; // now it is days
  // the same ? time = epoch / (60*60*24);
  year = 0;
  days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) year++;
  DateString += String(year + 1970); // year is offset from 1970
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0
  days = 0;
  month = 0;
  monthLength = 0;
  for (month = 0; month < 12; month++)
  {
    if (month == 1)
    { // february
      if (LEAP_YEAR(year)) monthLength = 29;
      else  monthLength = 28;
    } else  monthLength = monthDays[month];
    if (time >= monthLength)time -= monthLength;
    else break;
  }
  DateString += ".";
  if((month + 1)<10)DateString += "0";
  DateString += String(month + 1);  // jan is month 1
  DateString += ".";
  if((time + 1)<10)DateString += "0";
  DateString += String(time + 1); // day of month
  uint8_t h = (epoch % 86400L) / 3600;
  uint8_t m = (epoch  % 3600) / 60;
  char s[8];
  sprintf(s," %02d:%02d",h,m);
  DateString += String(s);
  return DateString;
}
