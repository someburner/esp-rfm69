# --------------- Spiffy options ---------------

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
# GZIP_COMPRESSION = $(GZIP_COMPRESSION)
SPIFFY_OUTPUT	  ?= 0
# VERBOSE=1

#default Endpoint

# Build time Wifi Cfg
STA_SSID 				?= STA_SSID
STA_PASS 				?= STA_PASS

############
#
# OUTPUT
#
############
BINARY = spiffy

############
#
# Paths
#
############

sourcedir = src
builddir = build
BUILD_BASE	= build


#############
#
# Build tools
#
#############

CC = gcc $(COMPILEROPTIONS)
LD = ld
GDB = gdb
OBJCOPY = objcopy
OBJDUMP = objdump
MKDIR = mkdir -p


###############
#
# Files and libs
#
###############

FLAGS	+= -DCONFIG_BUILD_SPIFFS -I.. -std=gnu99 -lz
CFILES = main.c
CFILES	+= spiffs_nucleus.c
CFILES	+= spiffs_gc.c
CFILES	+= spiffs_hydrogen.c
CFILES	+= spiffs_cache.c
CFILES	+= spiffs_check.c

INCLUDE_DIRECTIVES = -I./${sourcedir}
COMPILEROPTIONS = $(INCLUDE_DIRECTIVES)

# ifeq ($(GZIP_COMPRESSION),1)
# CCFLAGS		?= -DGZIP_COMPRESSION=$(GZIP_COMPRESSION)
# endif

ifeq ($(SPIFFY_OUTPUT),1)
CCFLAGS		?= -DSPIFFY_DEBUG
endif

CCFLAGS +=  -DSTA_PASS=$(STA_PASS) -DSTA_SSID=$(STA_SSID)


V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif


############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/default ${sourcedir}/test

OBJFILES = $(CFILES:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES)

DEPENDENCIES = $(DEPFILES)

# link object files, create binary
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS) -lz

-include $(DEPENDENCIES)

# compile c filesf
$(OBJFILES) : ${builddir}/%.o:%.c
		$(vecho) "... compile $@"
		@${CC} ${CCFLAGS} -g -c -o $@ $<

# make dependencies
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(COMPILEROPTIONS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY)

mkdirs:
	-@${MKDIR} ${builddir}


clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.elf
	@rm -f ${builddir}/spiffy
	@rm -f ${builddir}/spiffy.exe
