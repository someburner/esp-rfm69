#ESP-RFM69

##Overview
-----------------------

RFM69 driver and web interface for the ESP8288.


##Features:
-----------------------

* RFM69 driver for ESP8266
* NGINX-style HTTP server, serving files from SPIFFS
* RFM webapp for sending basic text, viewing received messages
* RFM69 OTA update utility, Moteino compatible
* SPIFFFS webapp:
   - Drag-and-drop file uploader based on websockets
   - Styled file viewer
   - Download files
   - Delete files
* SPIFFY-based ROM creator with gzip, html, css, js compression
* Web interface for WiFi
* Built with Espressif's NONOS SDK v1.5.2 and NONOS lwip 1.5.2 module


##Bugs & Limitations:
-----------------------

* Have not tested anything other than 66-byte encrypted packet mode. YMMV
* File System uploader crashes on occasion. Probably not too difficult to
   fix but I haven't had time.
* Static Javascript is a bit messy and probably a bit buggy. Still getting the
   hang of web development.
* WiFi change and event monitor needs to be fixed or perhaps re-written
* This is not compatible with esp-arduino, and I do not personally have plans
   to work on a port, but I have included enough basic functionality that you
   should be able to look at the API make changes to suit your needs


##Credits:
-----------------------

Any and all contributers to/authors of the various modules used in this project
(see list below/LICENSE file). But particularly:

* Felix Rusu at LowPowerLab.com for creating the Moteino framework, RFM69 driver,
   and in particular all the work he has put in to Wireless Programming.
* Spritetm for his contribution of esp-httpd, and Israel Lot for his
   NGINX-flavor rework of esp-httpd.
* SPIFFS author [pellepl](https://github.com/pellepl/spiffs) for creating SPIFFS and making
   himself more than available for help and support.
* Espressif for the awesome, cheap chip and English documentation
* Max Filippov and Paul Sokolovsky (pfalcon) for open-sourcing this bad boy.


##Requirements
-----------------------

1. ESP-12e. This release is written for a 4MB flash chip size. If you're using
   an earlier module with less space, feel free to re-configure SPIFFS and
   user config. Or, you know, just spend $2 and get an esp-12e

2. RFM69W, RFM69HW, or RFM12B. RFM12B is NOT supported with this release, but
   should be able to work with a little effort.

3. Linux to build stuff. None of this has been tested under Windows.



##Getting Started
-----------------------

1. Build Toolchain:

   Clone and build the latest [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk) if you haven't already.
   Make sure to install the stand-alone version (STANDALONE=y)

2. Edit makefiles

   **main project makefile**:

   Replace YOUR-PATH with the path to esp-open-sdk:

```
   XTENSA_TOOLS_ROOT ?= $(abspath /YOUR-PATH/esp-open-sdk-1.5.2/xtensa-lx106-elf/bin)/
   SDK_ROOT 			?= $(abspath /YOUR-PATH/esp-open-sdk-1.5.2/sdk)/
   PROJ_ROOT 			?= $(abspath /YOUR-PATH/esp-rfm69)/
```

   In addition to the above, configure the following as desired:
   - **STA_SSID**: Pre-configure a WiFi SSID to connect to
   - **STA_PASS**: password for above
   - **RFM_INTR_PIN**: ESP pin # for DIO0 on RFM69 (uses GPIO4 by default)
   - **RFM69_FREQ**: 91 = 915MHz; 43 = 433MHz
   - **RFM69_NET_ID**: RFM69 network ID, 1-254
   - **RFM69_NODE_ID**: RFM69 ID to use for this ESP8266
   - **RFM69_DEV_ID**: Default Node ID to send packets to
   - **RFM69_IS_HW**: 1 == HW (20dB), 0 == W (13dB) - MUST BE SET CORRECTLY
   - **RFM69_ENCRYPT_KEY**: 16 character encryption key for RFM69 AES. - MUST BE 16 CHARS


   **app/makefile**:

   For the makefile in the app/ directory:

   Replace YOUR-ESP-RFM69-PATH with the path to this project:
   ```
   LDDIR = $(abspath /YOUR-ESP-RFM69-PATH/esp-rfm69/ld)/
   ```


   **spiffy-compressor makefile**:

   I have modified SPIFFY to go into the tools/spiffy-compressor/html folder,
   and look for css, js, and html files. It then compresses them, using gzip
   compression where appropriate, and then adds them to a 64KByte SPIFFS rom.

   This is done each time a 'make' is issued. If you want to change that, just
   change the 'FLASH_SPIFFS' option in the makefile.

   The result is 'spiffy_rom.bin', which then gets copied to the /bin folder
   and flashed when 'make flash' is issued with 'FLASH_SPIFFS' set.

   **To make the compressed HTML image you will need to first compile 'spiffy-compressor'
   located in the tools folder.** This folder contains all the static FS files
   for the webapp and may be changed as desired.

   *Usage*:
   ```
   make clean-spiffy
   make spiffy
   ```


2. Build the project

   Make sure spiffy has been built, then run:

   ```
   make
   ```

   This should compile everything and copy '0x00000.bin', '0x10000.bin',
   and 'spiffy_rom.bin' to the /bin directory.

   If that's all good, then put the ESP8266 in flash mode and run

   ```
   make flash
   ```

##Moteino OTA:
-----------------------

See Moteino_OTA.md

##Serial Monitor:
-----------------------

Default baud rate is 74880. After user_init is called, it goes to 115200.
If platformIO is installed, serial monitor can be invoked as such
(replace /dev/ftdi_esp with your com port):

```
platformio serialports monitor -b 115200 -p /dev/ftdi_esp
```

##MQTT
-----------------------

More to Come..

##SPI Flash Info
-----------------------

The flash chips packaged with esp-xx modules can be of various sizes and come
from various manufacturers. To get flash ID (as hex):

```
uint32 flash_id = spi_flash_get_id();
NODE_DBG("Flash_ID = %x", flash_id);
```

**Layout:**

- `0x000000 - 0x010000`:   64KB - Bootloader, SDK
- `0x010000 - 0x080000`:  448KB - Rom0
- `0x080000 - 0x090000`:   64KB - Static FS (HTML, initial config)
- `0x090000 - 0x1DC000`: 1328KB - Dynamic FS (All other files)
- `0x1DC000 - 0x1EC000`:   64KB - ATMega328p rom0
- `0x1EC000 - 0x1FC000`:   64KB - ATMega328p rom1
- `0x1FC000 - 0x200000`:  Unused (SDK config)


##SPIFFS:
-----------------------

The address to flash SPIFFS must (I think) be on a proper multiple of the
logical block size, and must not overlap with irom at all.

So if *PHYS_ERASE_BLK_SZ* = 32x1024 = **0x8000**, and
*LOG_BLK_SZ* = 64x1024 = **0x10000**, and
*irom0_0_seg* is **0x52000** in size (i.e. 0x62000 region), then
*SPIFFS_START_ADDR* should be at **0x70000**.
This is to allow proper header data to be written cleanly without being
overwritten upon reflash.


**From SPIFFS author pellepl**:

* Try keeping the erase size as big as your block size. Having a smaller
erase size will add up to more erase call / block inducing more overhead.
I'd go for 32/64kbyte block and erase size, with a 256 page size.
Especially if you got ram to spend.

* Try imagining the largest size your app would ever need, multiply by 1.5
and there you have it :) If you have loads of files being less than the
page size, you should decrease page size.

* But, for large files, there are boundaries when a new "inode table"
(metadata, indirection stuff) must be loaded at a certain file offsets.
I don't know what typedef sizes you have on your obj_id, page_ix etc, but
the vanilla configuration (u16_t typedefs, 256 bytes page size) would force
a table refresh at ~(240/sizeof(u16_t)=2 entries per inode table) * 256 bytes
= ~30KByte intervals. A table refresh is basically a file system scan for
a specific obj_id with correct span index. If this sounds like mumbo-jumbo
to you, see TECH_SPEC

[TECH SPEC](https://github.com/pellepl/spiffs/blob/master/docs/TECH_SPEC)


##Known Flash chips:
-----------------------

`BergMicro BG25Q32`:

* ID: 0x1640E0
* Size: 4MB
* Known shipments:
    - AI-Thinker ESP-12-E QIO L2 [Dec '15]
    - AI-Thinker (blue esp, yellow board) [Sept '15]

`Other`:


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


##Debug Tools
-----------------------

* [Allow COORS in Chrome](https://chrome.google.com/webstore/detail/allow-control-allow-origi/nlfbmbojpeacfghkpbjhddihlkkiljbi/related?hl=en)
* [Brackets HTML/CSS/JS Editor](http://brackets.io/)


##Module List
-----------------------

* [MQTT](https://github.com/tuanpmt/esp_mqtt)
* [SPIFFS](https://github.com/pellepl/spiffs)
* [NODEMCU](https://github.com/nodemcu/nodemcu-firmware/tree/dev)
* [cJSON](https://github.com/kbranigan/cJSON)
* [cbuff-module](https://github.com/codinghead/cbuff-module)
* [ESP-Ginx](https://github.com/israellot/esp-ginx)
* [HTTP Parser](https://github.com/nodejs/http-parser)
* [Espressif SDK](http://bbs.espressif.com/viewforum.php?f=46&sid=542472662ff3f73241d8301c4c2d7f15)
* [SPI Driver](https://github.com/MetalPhreak/ESP8266_SPI_Driver)


##Screenies
-----------------------

###Console
![Console](doc/console.png?raw=true "Console")

###File System
![File System](doc/filesys.png?raw=true "File System")

###Uploader
![Uploader](doc/uploader.png?raw=true "Uploader")

###OTA Updater
![OTA Updater](doc/ota.png "OTA Updater")
