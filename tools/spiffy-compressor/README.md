spiffy
======

execute: make spiffy_img.o 

Build a [spiffs](https://github.com/pellepl/spiffs) file system binary for embedding/writing
onto the [Sming](https://github.com/anakod/Sming) ESP8266 spiffs file system.
This code forked from https://github.com/xlfe/spiffy and changed for bigger fs and file sizes and stability.

### What is it

spiffy builds a binary spiffs image for you to write_flash to a esp8266 runing Sming so you can
get all the files onto your cool IoT device in one fell swoop.

### usage
Basic usage is "spiffy 196608 webFiles" after build. You can build spiffy like this.

#### Clone the repo and build spiffy

```bash
git clone https://github.com/alonewolfx2/spiffy.git
cd spiffy
make
```

#### create a folder with the webFiles you'd like to embed

#### run spiffy to build the rom

```bash
C:\Users\Suskun\Documents\GitHub\spiffy\build>spiffy 196608 webFiles
Creating rom spiff_rom.bin of size 196608 bytes
Adding files in directory webFiles
Unable to read file .
Unable to read file ..
bootstrap.css.gz added to spiffs (15615 bytes)
index.html added to spiffs (12466 bytes)
jquery.js.gz added to spiffs (30153 bytes)
settings.html added to spiffs (4210 bytes)
style.css added to spiffs (7710 bytes)
wifi-sprites.png added to spiffs (1769 bytes)

```

#### Done!
