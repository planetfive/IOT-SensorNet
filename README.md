# IOT-SensorNet - universelle Firmware für den ESP8266 (vorzugsweise 12F)

Achtung: An der Software wird noch eifrig gebastelt.

Kurzer Abriss der Features:
 - Temperaturmessungen mit max 4 DS1820,Temp-und Feuchtemessung mit DHT22, PIR-Bewegungsmelder, Fensterüberwachung, Alarmgeber, Logging usw.
 - Anzeige einzeln oder mehrere Sensoren gesammelt über Webserver.
 - Eigenes Sensor-WLan-Netz konfigurierbar.
 - Hardware über 5V-Netzteil oder 2 Batterien. ( Kann evtl. über uns zum Selbstkostenpreis bezogen werden )
   Vieles ist auch mit z.B. dem Wemos-mini-Modul machbar.Siehe https://wiki.wemos.cc/products:d1:d1_mini
 - Passiv ( nur Anzeige ) oder aktiv programmierbar zum Ausführen von Aktionen ( Pushservice, 2 Ausgänge schaltbar als Taster, Schalter oder Dimmen ).

Der schnelle Weg zum Erfolg:
----------------------------
Dieses Projekt wurde mit der IDE Atom/PlatformIO erstellt.
Die Firmware befindet sich im Verzeichniss "wlansensor/firmware.bin".
Die erste Firmware muß mit einem Flash-Tool (z. B. esptool.py) geflasht werde.

Befehl: "esptool.py --port /dev/ttyUSB0 write_flash --flash_mode qio --flash_size 32m 0x00000 /home/???/wlanSensor/firmware.bin".
Alle weiteren Firmware-Updates oder Spiffs-Dateien(html und andere Steuerdateien) können über WLan und Browser eingespielt werden.
Nach dem initialen Firmware-flashen müssen noch die Spiffs-Dateien für den Webserver eingespielt werden.
Wenn eine ntp.txt-Datei mit gültiger IP-Adresse existiert, wird die Zeit lokal von dieser Adresse (z.B. die Routeradresse) abgeholt.

Eine ausführliche Dokumentation ist in Arbeit.


