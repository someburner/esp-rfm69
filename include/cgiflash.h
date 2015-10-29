#ifndef CGIFLASH_H
#define CGIFLASH_H

#include "httpd.h"
#include "radiohandler.h"

#define CGIFLASH_TYPE_FW 0
#define CGIFLASH_TYPE_ESPFS 1

typedef struct {
	int type;
	int fw1Pos;
	int fw2Pos;
	int fwSize;
} CgiUploadFwDef;


typedef union
{
	uint8_t  byte[1024];
	uint8_t  bytes[256][4];
  uint32_t dword[256];
} AlignedBuffer;

typedef struct {
	uint32_t cur;
  uint16_t seq, magic, crc, len;
} FwHeader;


struct FwPart {
	// FwHeader            fwh;
	uint16_t        seq, crc;
	AlignedBuffer   buf;
};

void cgiFlashInit();
int cgiReadFlash(HttpdConnData *connData);
int cgiGetFirmwareNext(HttpdConnData *connData);
int cgiUploadFirmware(HttpdConnData *connData);
int cgiUploadFlash(HttpdConnData *connData);
int cgiRebootFirmware(HttpdConnData *connData);
int cgiFlashTest(HttpdConnData *connData);

void memDump(void *addr, int len);

#endif
