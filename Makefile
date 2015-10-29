#
# Makefile for esp-rfm69
#
# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `make wiflash` will flash the esp over wifi
# `VERBOSE=1 make ...` will print debug info
# `ESP_HOSTNAME=my.esp.example.com make wiflash` is an easy way to override a variable
# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
XTENSA_TOOLS_ROOT ?= $(abspath /home/jeffrey/esp-open-sdk/xtensa-lx106-elf/bin)/

# Base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= $(abspath ../esp_iot_sdk_v1.4.0)

# Esptool.py path and port
# https://github.com/themadinventor/esptool
#ESPTOOL		?= $(abspath /home/jeffrey/esp-open-sdk/esptool/esptool.py)
ESPTOOL		?= ./tools/esptool/esptool.py
ESPPORT		?= /dev/ftdi_esp
ESPBAUD		?= 460800

# --------------- chipset configuration   ---------------

# Pick your flash size: "512KB", "1MB", or "4MB"
FLASH_SIZE ?= 4MB

# Winbond 25Q32 4MB flash, typ for esp-12
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 6       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
# ESP_FLASH_MAX       ?= 503808  # max bin file for 512KB flash partition: 492KB
ESP_FLASH_MAX       ?= 1028096 # max bin file for 1MB flash partition: 1004KB
ET_FS               ?= 32m     # 32Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command

# where to flash blank.bin to erase wireless settings
# Basically take 7F000 - 400*4 (aka 1024*4) = 7E000
ET_BLANK            ?= 0x7E000

# Build time Wifi Cfg
#STA_SSID ?= WiFiSSID
#STA_PASS ?= wifipassword

# hostname or IP address for wifi flashing
ESP_HOSTNAME        ?= esp-rfm69

# RFM69 settings
#RFM69_FREQ          ?= 43
RFM69_FREQ          ?= 91
RFM69_NET_ID        ?= 100
RFM69_NODE_ID       ?= 1
RFM69_IS_HW         ?= 0
RFM69_ENCRYPT_KEY   ?= \"Rfm69_EncryptKey\"
# Rate at which the ESP 'pings' the node device, in ms
PING_RATE           ?= 600000

ATMEGA_FLASH_MAX    ?= 32768  # max bin file for atmega328p

# The pin assignments below are used when the settings in flash are invalid, they
# can be changed via the web interface
# GPIO pin used to reset attached microcontroller, acative low
MCU_RESET_PIN       ?= -1
# GPIO pin used with reset to reprogram MCU (ISP=in-system-programming, unused with AVRs), active low
MCU_ISP_PIN         ?= -1
# GPIO pin used for "connectivity" LED, active low
LED_CONN_PIN        ?= 0

# --------------- esp-rfm69 version        ---------------

# This queries git to produce a version string like "esp-rfm69 v0.9.0 2015-06-01 34bc76"
# If you don't have a proper git checkout or are on windows, then simply swap for the constant
# Steps to release: create release on github, git pull, git describe --tags to verify you're
# on the release tag, make release, upload esp-rfm69.tgz into the release files
#VERSION ?= "esp-rfm69 custom version"
DATE    := $(shell date '+%F %T')
BRANCH  := $(shell if git diff --quiet HEAD; then git describe --tags; \
                   else git symbolic-ref --short HEAD; fi)
SHA     := $(shell if git diff --quiet HEAD; then git rev-parse --short HEAD | cut -d"/" -f 3; \
                   else echo "development"; fi)
VERSION ?=esp-rfm69 $(BRANCH) - $(DATE) - $(SHA)

# --------------- esp-rfm69 config options ---------------

# If CHANGE_TO_STA is set to "yes" the esp-rfm69 module will switch to station mode
# once successfully connected to an access point. Else it will stay in AP+STA mode.

CHANGE_TO_STA ?= yes

# --------------- esphttpd config options ---------------

# If GZIP_COMPRESSION is set to "yes" then the static css, js, and html files will be compressed
# with gzip before added to the espfs image and will be served with gzip Content-Encoding header.
# This could speed up the downloading of these files, but might break compatibility with older
# web browsers not supporting gzip encoding because Accept-Encoding is simply ignored.
# Enable this option if you have large static files to serve (for e.g. JQuery, Twitter bootstrap)
# If you have text based static files with different extensions what you want to serve compressed
# then you will need to add the extension to the following places:
# - Add the extension to this Makefile at the webpages.espfs target to the find command
# - Add the extension to the gzippedFileTypes array in the user/httpd.c file
#
# Adding JPG or PNG files (and any other compressed formats) is not recommended, because GZIP
# compression does not work effectively on compressed files.

#Static gzipping is disabled by default.
GZIP_COMPRESSION ?= yes

# If COMPRESS_W_HTMLCOMPRESSOR is set to "yes" then the static css and js files will be compressed with
# htmlcompressor and yui-compressor. This option works only when GZIP_COMPRESSION is set to "yes".
# https://code.google.com/p/htmlcompressor/#For_Non-Java_Projects
# http://yui.github.io/yuicompressor/
# enabled by default.
COMPRESS_W_HTMLCOMPRESSOR ?= yes
HTML_COMPRESSOR ?= htmlcompressor-1.5.3.jar
YUI_COMPRESSOR ?= yuicompressor-2.4.8.jar

# -------------- End of config options -------------

HTML_PATH = $(abspath ./html)/
WIFI_PATH = $(HTML_PATH)wifi/

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE	  	= firmware

# name for the target project
TARGET		= httpd

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

# which modules (subdirectories) of the project to include in compiling
MODULES		= espfs httpd user serial cmd driver rfm69
EXTRA_INCDIR	= include .

# libraries used in this project, mainly provided by the SDK
LIBS		= c gcc hal phy pp net80211 wpa main lwip

# compiler flags using during compilation of source files
CFLAGS		= -Os -ggdb -std=c99 -Werror -Wpointer-arith -Wundef -Wall -Wl,-EL \
	-fno-inline-functions -mno-serialize-volatile -nostdlib -mlongcalls \
	-mtext-section-literals -ffunction-sections -fdata-sections -D__ets__ \
	-DICACHE_FLASH -D_STDINT_H -Wno-address  -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
	-DMCU_RESET_PIN=$(MCU_RESET_PIN) -DMCU_ISP_PIN=$(MCU_ISP_PIN) \
	-DLED_CONN_PIN=$(LED_CONN_PIN) -DLED_SERIAL_PIN=$(LED_SERIAL_PIN) \
	-DRFM69_FREQ=$(RFM69_FREQ) -DRFM69_NET_ID=$(RFM69_NET_ID) \
	-DRFM69_NODE_ID=$(RFM69_NODE_ID) -DRFM69_IS_HW=$(RFM69_IS_HW) \
	-DRFM69_ENCRYPT_KEY=$(RFM69_ENCRYPT_KEY) -DVERSION="$(VERSION)" \
	-DPING_RATE=$(PING_RATE) -DATMEGA_FLASH_MAX=$(ATMEGA_FLASH_MAX)

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# linker script used for the above linker step
LD_SCRIPT 	:= build/eagle.esphttpd.v6.ld
LD_SCRIPT1	:= build/eagle.esphttpd1.v6.ld
LD_SCRIPT2	:= build/eagle.esphttpd2.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR		= lib
SDK_LDDIR			= ld
SDK_INCDIR		= include include/json
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump


####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS		:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC)) $(BUILD_BASE)/espfs_img.o
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

ifneq ($(strip $(STA_SSID)),)
CFLAGS		+= -DSTA_SSID="$(STA_SSID)"
endif

ifneq ($(strip $(STA_PASS)),)
CFLAGS		+= -DSTA_PASS="$(STA_PASS)"
endif

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DGZIP_COMPRESSION
endif

ifeq ("$(CHANGE_TO_STA)","yes")
CFLAGS          += -DCHANGE_TO_STA
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean webpages.espfs wiflash

all: echo_version checkdirs $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

echo_version:
	@echo VERSION: $(VERSION)

$(USER1_OUT): $(APP_AR) $(LD_SCRIPT1)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	@echo Dump  : $(OBJDP) -x $(USER1_OUT)
	@echo Disass: $(OBJDP) -d -l -x $(USER1_OUT)
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(USER2_OUT): $(APP_AR) $(LD_SCRIPT2)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(FW_BASE):
	$(vecho) "FW $@"
	$(Q) mkdir -p $@

$(FW_BASE)/user1.bin: $(USER1_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	ls -ls eagle*bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "** user1.bin uses $$(stat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available"
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(FW_BASE)/user2.bin: $(USER2_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

wiflash: all
	./wiflash $(ESP_HOSTNAME) $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

baseflash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash 0x01000 $(FW_BASE)/user1.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) -ff $(ET_FF) \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.4(b1).bin" 0x01000 $(FW_BASE)/user1.bin \
	  $(ET_BLANK) $(SDK_BASE)/bin/blank.bin

tools/$(HTML_COMPRESSOR):
	$(Q) mkdir -p tools
  ifeq ($(OS),Windows_NT)
	cd tools; wget --no-check-certificate https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI_COMPRESSOR) -O $(YUI_COMPRESSOR)
	cd tools; wget --no-check-certificate https://htmlcompressor.googlecode.com/files/$(HTML_COMPRESSOR) -O $(HTML_COMPRESSOR)
  else
	cd tools; wget https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI_COMPRESSOR)
	cd tools; wget https://htmlcompressor.googlecode.com/files/$(HTML_COMPRESSOR)
  endif

ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
$(BUILD_BASE)/espfs_img.o: tools/$(HTML_COMPRESSOR)
endif

# fw/
# $(Q) cp -r fw/*.bin html_compressed;
$(BUILD_BASE)/espfs_img.o: html/ html/wifi/ espfs/mkespfsimage/mkespfsimage
	$(Q) rm -rf html_compressed; mkdir html_compressed; mkdir html_compressed/wifi;
	$(Q) cp -r html/*.ico html_compressed;
	$(Q) cp -r html/*.css html_compressed;
	$(Q) cp -r html/*.js html_compressed;
	$(Q) cp -r html/wifi/*.png html_compressed/wifi;
	$(Q) cp -r html/wifi/*.js html_compressed/wifi;
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) echo "Compression assets with htmlcompressor. This may take a while..."
		$(Q) java -jar tools/$(HTML_COMPRESSOR) \
		-t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
		-o $(abspath ./html_compressed)/ \
		$(HTML_PATH)head- \
		$(HTML_PATH)*.html
	$(Q) java -jar tools/$(HTML_COMPRESSOR) \
		-t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
		-o $(abspath ./html_compressed)/wifi/ \
		$(WIFI_PATH)*.html
	$(Q) echo "Compression assets with yui-compressor. This may take a while..."
	$(Q) for file in `find html_compressed -type f -name "*.js"`; do \
			java -jar tools/$(YUI_COMPRESSOR) $$file -o $$file; \
		done
	$(Q) for file in `find html_compressed -type f -name "*.css"`; do \
			java -jar tools/$(YUI_COMPRESSOR) $$file -o $$file; \
		done
endif
ifeq (,$(findstring mqtt,$(MODULES)))
	$(Q) rm -rf html_compressed/mqtt.html
	$(Q) rm -rf html_compressed/mqtt.js
endif
	$(Q) for file in `find html_compressed -type f -name "*.htm*"`; do \
		cat html_compressed/head- $$file >$${file}-; \
		mv $$file- $$file; \
	done
	$(Q) rm html_compressed/head-
	$(Q) cd html_compressed; find . \! -name \*- | ../espfs/mkespfsimage/mkespfsimage > ../build/espfs.img; cd ..;
	$(Q) ls -sl build/espfs.img
	$(Q) cd build; $(OBJCP) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.espfs \
			espfs.img espfs_img.o; cd ..

# edit the loader script to add the espfs section to the end of irom with a 4 byte alignment.
# we also adjust the sizes of the segments 'cause we need more irom0
# in the end the only thing that matters wrt size is that the whole shebang fits into the
# 236KB available (in a 512KB flash)
ifeq ("$(FLASH_SIZE)","512KB")
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld >$@
else
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/6B000/7C000/' \
			$(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld >$@
endif

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/
	$(Q) $(MAKE) -C espfs/mkespfsimage GZIP_COMPRESSION="$(GZIP_COMPRESSION)"

release: all
	$(Q) rm -rf release; mkdir -p release/esp-rfm69-$(BRANCH)
	$(Q) egrep -a 'esp-rfm69 [a-z0-9.]+ - 201' $(FW_BASE)/user1.bin | cut -b 1-80
	$(Q) egrep -a 'esp-rfm69 [a-z0-9.]+ - 201' $(FW_BASE)/user2.bin | cut -b 1-80
	$(Q) cp $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin $(SDK_BASE)/bin/blank.bin \
		   "$(SDK_BASE)/bin/boot_v1.4(b1).bin" wiflash release/esp-rfm69-$(BRANCH)
	$(Q) tar zcf esp-rfm69-$(BRANCH).tgz -C release esp-rfm69-$(BRANCH)
	$(Q) echo "Release file: esp-rfm69-$(BRANCH).tgz"
	$(Q) rm -rf release

clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) make -C espfs/mkespfsimage/ clean
	$(Q) rm -rf $(FW_BASE)
	$(Q) rm -f webpages.espfs
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) rm -rf html_compressed
endif

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
