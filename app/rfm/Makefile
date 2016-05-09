
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
GEN_LIBS = rfm.a
endif

STD_CFLAGS=-std=gnu11 -Wimplicit

#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
#DEFINES +=
EXTRA_CCFLAGS += \
	-DPING_RATE=$(PING_RATE) -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
	-DRFM69_NODE_ID=$(RFM69_NODE_ID) -DRFM69_NET_ID=$(RFM69_NET_ID) \
	-DRFM69_FREQ=$(RFM69_FREQ) -DRFM69_DEV_ID=$(RFM69_DEV_ID) \
	-DMQTT_ENABLE=$(MQTT_ENABLE)

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
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I ./
INCLUDES += -I ../libc
INCLUDES += -I ../
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile
