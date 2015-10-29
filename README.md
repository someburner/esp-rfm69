#ESP8266 RFM69W Driver

##Overview
-----------------------

* ESP8266 (Esp-12e) + RFM69HW

##Description
-----------------------
  RFM69 driver for ESP8266 SoC. Tested with Espressif's 1.4 SDK. Driver based
on Felix Rusu's RFM69 library for Moteino. ESP8266 interface based on [esp-link](https://github.com/jeelabs/esp-link)


##Features
-----------------------

* Compatible with Moteino.
* Supports OTA FW updating.


##Usage:
-----------------------
   To build the project:

* Update the makefile with your user's location of the esp-open-sdk as well as
Espressif's IOT sdk.
* Configure the RFM69 settings as desired.

   Then:
```
make clean
make flash
```

##Firmware updating:
-----------------------
First, grab the .hex file compiled with your Arduino compiler. Using platformio grabbing is simple, just go
into the .pioenvs folder and it's in there.

Then convert it to a binary file using hex2bin (what I used on Linux) or whatever method is available to you.
On the ESP8266, go to the uC Console page, choose the .bin file, and click upload. It will ask for confirmation,
so check that it's the correct filename and say ok.

Then click the upload fw button. This will initiate the fw update process. If you have a serial monitor open you
should see it logging out the first and last bytes (as hex) every group of 64 bytes. At the end is will say EOF
detected.


##Pin Configuration
-----------------------

**Bold** == for flashing only

###RFM69
-----------------------

| RFM69HW | ESP-12E |
| ------- | --------|
| MISO    | GPIO12  |
| MOSI    | GPIO13  |
| SCK     | GPIO14  |
| CS/SS   | GPIO15  |
| DIO0    | GPIO5   |

###GPIO
-----------------------

| I/O          | ESP-12E  |
| ------------ | -------- |
| 10K PULLUP   | CH_PD/EN |
| 10K PULLDOWN | GPIO15   |
| **10K PULLDOWN** | GPIO0    |
| BUTTON-GND   | RST      |

##List of libraries used

- [MetalPhreak's SPI Library](https://github.com/MetalPhreak/ESP8266_SPI_Driver):
Used to communicate with RFM69
- [CHERT's GPIO Library](https://github.com/CHERTS/esp8266-gpio16):
Used to configure interrupts and IO pins
- [httpd Server by Sprite](http://git.spritesserver.nl/esphttpd.git/): serves
webpages, files, and responds to requests, etc
- [RFM69 Library](https://github.com/LowPowerLab/RFM69): Arduino driver for
RFM69, which is what I based mine on.

##Changelog
-----------------------

`1.0.0`

* First release
