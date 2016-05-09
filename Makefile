#  copyright (c) 2010 Espressif System
#
ifndef PDIR
endif

# Base directory for the compiler
OPENSDK_ROOT 		?= YOUR-PATH/esp-open-sdk-1.5.2
XTENSA_TOOLS_ROOT ?= $(OPENSDK_ROOT)/xtensa-lx106-elf/bin/
SDK_ROOT 			?= $(OPENSDK_ROOT)/sdk/
PROJ_ROOT = $(abspath $(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))/

# Base directory of the ESP8266 SDK package, absolute
SDK_BASE				?= $(abspath $(OPENSDK_ROOT)/esp_iot_sdk_v1.5.2)

#############################################################
# User Configuration
#############################################################
#Flash Spiffs?
FLASH_SPIFFS			?= yes
SPIFFS_ADDR				?= 0x80000

# Build time Wifi Cfg
STA_SSID 				?= YOUR_SSID
STA_PASS 				?= yourpassword

#If MQTT_ENABLE is set to 0, it will not be initialized
MQTT_ENABLE				= 1

MQTT_SUB_COUNT			= 2				#number of subs
MQTT_SUB_TOPIC1		?= esp-rfm69-sub1		#sub topic 1
MQTT_SUB_TOPIC2		?= esp-rfm69-sub2		#sub topic 2

MQTT_PUB_COUNT			= 1				#number of pubs
MQTT_PUB_TOPIC1		?= esp-rfm69-topic1		#pub topic 1

#If MQTT_USE_IP is set to 1, it will init MQTT with DEFAULT_MQTT_IP
#if MQTT_USE_IP is set to 0, it will first find the ip for MQTT_HOST_NAME
# and initialize MQTT using the IP found (if it is found)
MQTT_USE_IP				= 0
DEFAULT_MQTT_IP		?= 198.41.30.241
MQTT_HOST_NAME			?= iot.eclipse.org #public broker

#Pin number for RFM69 interrupts
RFM_INTR_PIN        ?= 2
#Rfm frequency 91 = 915MHz; 43 = 433MHz
RFM69_FREQ          ?= 91
#Network ID of both units
RFM69_NET_ID        ?= 100

#915 MHz ID of bridge rf chip
RFM69_NODE_ID       ?= 1
#915 MHz ID of device in pit
RFM69_DEV_ID        ?= 3

RFM69_IS_HW         ?= 1
RFM69_ENCRYPT_KEY   ?= \"Rfm69_EncryptKey\"

#Flash esp_init_data_default?
FLASH_INIT_DEFAULT	?= no

#Flash blank.bin?
FLASH_BLANK				?= no
#############################################################
# Spiffy Compressor
# 		Usage: make spiffy_img.o
#############################################################
VERBOSE 							= 1

COMPRESS_W_HTMLCOMPRESSOR 	?= yes
GZIP_COMPRESSION				 = 1
HTML_COMPRESSOR 				?= htmlcompressor-1.5.3.jar
YUI_COMPRESSOR 				?= yuicompressor-2.4.8.jar

SPFY_DIR 		= $(PROJ_ROOT)tools/spiffy-compressor/
SPFY_SRC_DIR 	= $(SPFY_DIR)src/
SPFY_BLD_DIR 	= $(SPFY_DIR)build/
HTML_PATH 		= $(SPFY_DIR)html/
WIFI_PATH 		= $(HTML_PATH)wifi/

SPFY_MKDIR 	= mkdir -p

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
MAKEPDIR :=
else
Q := @
vecho := @echo
MAKEPDIR := --no-print-directory -s
endif

# ifeq ($(GZIP_COMPRESSION), 1)
# SPIFFY_DEFINES += -DGZIP_COMPRESSION=$(GZIP_COMPRESSION)
# endif

#############################################################
# Esptool flash options:
# 		Configured for esp-12e (4MByte Windbond)
#############################################################
ESPBAUD 				  	?= 230400 	# 115200, 230400, 460800
ESP_FLASH_MODE      	?= 0			# 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  	?= 15			# 15->80Mhz
ET_FS               	?= 32m     	# 32Mbit flash size in esptool flash command
ET_FF               	?= 80m     	# 80Mhz flash speed in esptool flash command
ET_BLANK            	?= 0x3FE000 # where to flash blank.bin to erase wireless settings
ESP_INIT  			  	?= 0x3FC000 # flash init data provided by espressif

#relative path of esptool script
ESPTOOL					?= ../tools/esptool.py
#absolute path of sdk bin folder
ESPTOOLMAKE				:= $(XTENSA_TOOLS_ROOT)

#############################################################
# Version Info
#############################################################
DATE    := $(shell date '+%F %T')
BRANCH  := $(shell if git diff --quiet HEAD; then git describe --tags; \
                   else git symbolic-ref --short HEAD; fi)
SHA     := $(shell if git diff --quiet HEAD; then git rev-parse --short HEAD | cut -d"/" -f 3; \
                   else echo "development"; fi)
VERSION ?=esp-rfm69 $(BRANCH) - $(DATE) - $(SHA)

#############################################################
# Select compile (Windows build removed)
#############################################################
# Can we use -fdata-sections?
ifndef COMPORT
	ESPPORT = /dev/ftdi_esp
else
	ESPPORT = $(COMPORT)
endif
CCFLAGS += -Os -ffunction-sections -fno-jump-tables -fdata-sections
AR			:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
CC 		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc -I$(PROJ_ROOT)sdk-overrides/include -I$(SDK_ROOT)include
NM 		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-nm
CPP 		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-cpp
OBJCOPY 	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
BINDIR  	 = ../bin/
 UNAME_S := $(shell uname -s)
 ifeq ($(UNAME_S),Linux)
# LINUX
 endif
 UNAME_P := $(shell uname -p)
 ifeq ($(UNAME_P),x86_64)
# ->AMD64
 endif
 ifneq ($(filter %86,$(UNAME_P)),)
# ->IA32
 endif
 ifneq ($(filter arm%,$(UNAME_P)),)
# ->ARM
 endif
#############################################################

#############################################################
# Compiler flags
#############################################################
CSRCS 	?= $(wildcard *.c)
ASRCs 	?= $(wildcard *.s)
ASRCS 	?= $(wildcard *.S)
SUBDIRS 	?= $(filter-out %build,$(patsubst %/,%,$(dir $(wildcard */Makefile))))

ODIR 		:= .output
OBJODIR 	:= $(ODIR)/$(TARGET)/$(FLAVOR)/obj

OBJS := $(CSRCS:%.c=$(OBJODIR)/%.o) \
        $(ASRCs:%.s=$(OBJODIR)/%.o) \
        $(ASRCS:%.S=$(OBJODIR)/%.o)

DEPS := $(CSRCS:%.c=$(OBJODIR)/%.d) \
        $(ASRCs:%.s=$(OBJODIR)/%.d) \
        $(ASRCS:%.S=$(OBJODIR)/%.d)

LIBODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/lib
OLIBS := $(GEN_LIBS:%=$(LIBODIR)/%)

IMAGEODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/image
OIMAGES := $(GEN_IMAGES:%=$(IMAGEODIR)/%)

BINODIR := $(ODIR)/$(TARGET)/$(FLAVOR)/bin
OBINS := $(GEN_BINS:%=$(BINODIR)/%)

#
# Note:
# https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
# If you add global optimize options like "-O2" here
# they will override "-Os" defined above.
# "-Os" should be used to reduce code size
#
CCFLAGS += 						\
	-g								\
	-Wpointer-arith			\
	-Wundef						\
	-Werror						\
	-Wl,-EL						\
	-fno-inline-functions	\
	-nostdlib       			\
	-mlongcalls					\
	-mtext-section-literals
#	-Wall

CCFLAGS 					?= -DBUILD_MQTT=$(BUILD_MQTT)

CFLAGS = $(CCFLAGS) $(DEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)
DFLAGS = $(CCFLAGS) $(DDEFINES) $(EXTRA_CCFLAGS) $(STD_CFLAGS) $(INCLUDES)

FLASH_BINS = 0x00000 $(BINDIR)0x00000.bin 0x10000 $(BINDIR)0x10000.bin

ifeq ("$(FLASH_SPIFFS)","yes")
FLASH_BINS += $(SPIFFS_ADDR) $(BINDIR)spiffy_rom.bin
endif

ifeq ("$(FLASH_INIT_DEFAULT)","yes")
FLASH_BINS += $(ESP_INIT) $(BINDIR)esp_init_data_default.bin
endif

ifeq ("$(FLASH_BLANK)","yes")
FLASH_BINS += $(ET_BLANK) $(BINDIR)blank.bin
endif

#############################################################
# Functions
#############################################################
define ShortcutRule
$(1): .subdirs $(2)/$(1)
endef

define MakeLibrary
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(LIBODIR)/$(1).a: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(LIBODIR)
	$$(if $$(filter %.a,$$?),mkdir -p $$(EXTRACT_DIR)_$(1))
	$$(if $$(filter %.a,$$?),cd $$(EXTRACT_DIR)_$(1); $$(foreach lib,$$(filter %.a,$$?),$$(AR) xo $$(UP_EXTRACT_DIR)/$$(lib);))
	$$(AR) ru $$@ $$(filter %.o,$$?) $$(if $$(filter %.a,$$?),$$(EXTRACT_DIR)_$(1)/*.o)
	$$(if $$(filter %.a,$$?),$$(RM) -r $$(EXTRACT_DIR)_$(1))
endef

define MakeImage
DEP_LIBS_$(1) = $$(foreach lib,$$(filter %.a,$$(COMPONENTS_$(1))),$$(dir $$(lib))$$(LIBODIR)/$$(notdir $$(lib)))
DEP_OBJS_$(1) = $$(foreach obj,$$(filter %.o,$$(COMPONENTS_$(1))),$$(dir $$(obj))$$(OBJODIR)/$$(notdir $$(obj)))
$$(IMAGEODIR)/$(1).out: $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1)) $$(DEPENDS_$(1))
	@mkdir -p $$(IMAGEODIR)
	$$(CC) $$(LDFLAGS) $$(if $$(LINKFLAGS_$(1)),$$(LINKFLAGS_$(1)),$$(LINKFLAGS_DEFAULT) $$(OBJS) $$(DEP_OBJS_$(1)) $$(DEP_LIBS_$(1))) -o $$@
endef

$(BINODIR)/%.bin: $(IMAGEODIR)/%.out
	@mkdir -p $(BINODIR)
	$(ESPTOOL) elf2image $< -o $(BINDIR) --path $(ESPTOOLMAKE) -fs $(ET_FS) -ff $(ET_FF)
	@echo "Build Complete!"

#############################################################
# Rules base
# Should be done in top-level makefile only
#############################################################
.PHONY: all .subdirs spiffy

ifndef PDIR
# targets for top level only
OSPIFFY=spiffy spiffy_img.o
endif

all: .subdirs $(OSPIFFY) $(OBJS) $(OLIBS) $(OIMAGES) $(OBINS) $(SPECIAL_MKTARGETS)

clean:
	$(Q) $(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clean;)
	$(Q) $(RM) -r $(ODIR)/$(TARGET)/$(FLAVOR)

clobber: $(SPECIAL_CLOBBER)
	$(foreach d, $(SUBDIRS), $(MAKE) -C $(d) clobber;)
	$(RM) -r $(ODIR)

flash:
ifndef PDIR
	$(Q) $(MAKE) -C ./app flash
else
	$(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) \
		-ff $(ET_FF) $(FLASH_BINS)
endif


.subdirs:
	@set -e; $(foreach d, $(SUBDIRS), $(MAKE) $(MAKEPDIR) -C $(d);)

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clobber)
ifdef DEPS
sinclude $(DEPS)
endif
endif
endif

$(OBJODIR)/%.o: %.c
	@mkdir -p $(OBJODIR);
	$(CC) $(if $(findstring $<,$(DSRCS)),$(DFLAGS),$(CFLAGS)) $(COPTS_$(*F)) -o $@ -c $<

$(OBJODIR)/%.d: %.c
	@mkdir -p $(OBJODIR);
	@echo DEPEND: $(CC) -M $(CFLAGS) $<
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.s
	@mkdir -p $(OBJODIR);
	$(CC) $(CFLAGS) -o $@ -c $<

$(OBJODIR)/%.d: %.s
	@mkdir -p $(OBJODIR); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(OBJODIR)/%.o: %.S
	@mkdir -p $(OBJODIR);
	$(CC) $(CFLAGS) -D__ASSEMBLER__ -o $@ -c $<

$(OBJODIR)/%.d: %.S
	@mkdir -p $(OBJODIR); \
	set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\.o\)[ :]*,$(OBJODIR)/\1 $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(foreach lib,$(GEN_LIBS),$(eval $(call ShortcutRule,$(lib),$(LIBODIR))))
$(foreach image,$(GEN_IMAGES),$(eval $(call ShortcutRule,$(image),$(IMAGEODIR))))
$(foreach bin,$(GEN_BINS),$(eval $(call ShortcutRule,$(bin),$(BINODIR))))
$(foreach lib,$(GEN_LIBS),$(eval $(call MakeLibrary,$(basename $(lib)))))
$(foreach image,$(GEN_IMAGES),$(eval $(call MakeImage,$(basename $(image)))))


utils/$(HTML_COMPRESSOR):
	$(Q) mkdir -p ${SPFY_DIR}utils
	$(Q) cd ${SPFY_DIR}utils; wget -nc https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI_COMPRESSOR)
	$(Q) cd ${SPFY_DIR}utils; wget -nc https://htmlcompressor.googlecode.com/files/$(HTML_COMPRESSOR)

ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
spiffy_img.o: utils/$(HTML_COMPRESSOR)
endif

spiffy_img.o: ${SPFY_DIR}html/ ${SPFY_DIR}html/wifi/ ${SPFY_DIR}spiffy/build/spiffy
	$(Q) rm -rf ${SPFY_DIR}html_compressed; mkdir ${SPFY_DIR}html_compressed;
	$(Q) cp -r ${SPFY_DIR}html/*.css ${SPFY_DIR}html_compressed;
	$(Q) cp -r ${SPFY_DIR}html/*.js ${SPFY_DIR}html_compressed;
	$(Q) cp -r ${SPFY_DIR}html/wifi/*.png ${SPFY_DIR}html_compressed;
	$(Q) cp -r ${SPFY_DIR}html/wifi/*.js ${SPFY_DIR}html_compressed;
	$(Q) cp -r ${SPFY_DIR}html/*.ico ${SPFY_DIR}html_compressed;
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) echo "Compression assets with htmlcompressor..."
		$(Q) java -jar ${SPFY_DIR}utils/$(HTML_COMPRESSOR) \
		-t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
		-o $(abspath $(SPFY_DIR)html_compressed)/ \
		$(HTML_PATH) \
		$(HTML_PATH)*.html
	$(Q) java -jar ${SPFY_DIR}utils/$(HTML_COMPRESSOR) \
		-t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
		-o $(abspath $(SPFY_DIR)html_compressed)/ \
		$(WIFI_PATH)*.html
	$(Q) echo "Compression assets with yui-compressor..."
	$(Q) for file in `find ${SPFY_DIR}html_compressed -type f -name "*.js"`; do \
			java -jar ${SPFY_DIR}utils/$(YUI_COMPRESSOR) $$file --line-break 0 -o $$file; \
		done
	$(Q) for file in `find ${SPFY_DIR}html_compressed -type f -name "*.css"`; do \
			java -jar ${SPFY_DIR}utils/$(YUI_COMPRESSOR) $$file -o $$file; \
		done
endif
	$(Q) for file in `find ${SPFY_DIR}html_compressed -type f -name "*.htm-"`; do \
		mv $$file $$file; \
	done
	$(Q) cd ${SPFY_DIR}html_compressed; find . \! -name \*- | ${SPFY_DIR}spiffy/build/spiffy > ${SPFY_DIR}build/spiff_rom.bin;
	$(Q) cd ${SPFY_DIR}html_compressed;
	$(Q) cp ${SPFY_DIR}html_compressed/spiffy_rom.bin ${PROJ_ROOT}/bin/spiffy_rom.bin

spiffy:
	$(Q) $(MAKE) -C ${SPFY_DIR}spiffy

mkdirs:
	-@${SPFY_MKDIR} ${SPFY_BLD_DIR}

clean-spiffy:
	@echo ... removing build files in ${SPFY_DIR}
	@rm -f ${SPFY_DIR}spiffy/build/*.o
	@rm -f ${SPFY_DIR}spiffy/build/*.d
	@rm -f ${SPFY_DIR}spiffy/build/spiffy
	@rm -f ${SPFY_DIR}*.o
	@rm -f ${SPFY_DIR}*.d
	@rm -f ${SPFY_DIR}*.elf

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#############################################################

INCLUDES := $(INCLUDES) -I $(PDIR)include -I $(PDIR)include/$(TARGET)
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile
