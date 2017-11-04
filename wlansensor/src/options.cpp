#include <Arduino.h>

#include "wlanSensor.h"
#include "version.h"
#include "split.h"
#include "webServer.h"
#include "onewire.h"
#include "spiffs.h"
#include "setup.h"
#include "ntpClient.h"
#include "pushClient.h"
#include "parser.h"
#include "logger.h"
#include "hh10d.h"

String getSensorOptions(String sensortyp) {
  String options = "<option>NO</option><option>DS1820</option><option>DHT22</option><option>HH10D</option><option>Spindel</option>";
  String selection = " selected>"+sensortyp;
  options.replace(">" + sensortyp,selection);
  return options;
}

String getModulMode(String mode) {
  String options = F("<option>L - Loggermode</option>"
                     "<option>S  - Sensormode</option>"
                     "<option>W  - Station-Mode</option>"
                     "<option>W+ - Station-Mode + Sensoranzeige</option>"
                     "<option>A  - AccesPoint-Mode</option>"
                     "<option>A+ - AccessPoint-Mode + Sensoranzeige</option>"
                     "<option>M  - AccessPoint + Station-Mode</option>"
                     "<option>M+ - AccessPoint + Station-Mode + Sensoranzeige</option>" );
  if(mode.length()==1)
    mode += ' ';
  String selection = " selected>"+mode;
  options.replace(">" + mode,selection);
  return options;
}

String getOutputMode(uint8_t mode) {
  String options = F( "<option>NO_OUTPUT</option>"
                      "<option>SCHALTER</option>"
                      "<option>TASTER</option>"
                      "<option>DIMMER</option>");
  String _mode;
  switch(mode){
    case SCHALTER:
      _mode = "SCH";
      break;
    case TASTER:
      _mode = "TA";
      break;
    case DIMMER:
      _mode = "DI";
      break;
    default:
      _mode = "NO";
    }
  String selection = " selected>" + _mode;
  options.replace(">" + _mode,selection);
  return options;
}
