/*
Some flash handling cgi routines. Used for reading the existing flash and updating the ESPFS image.
*/
#include <esp8266.h>
#include "cgiflash.h"
#include <osapi.h>
#include "cgiflash.h"
#include "espfsformat.h"
#include <osapi.h>
#include "cgiflash.h"
#include "espfs.h"

// hack: this from LwIP
extern uint16_t inet_chksum(void *dataptr, uint16_t len);

/* allows us access to individual bytes of 32bit data.
 * set to 64 as our data streams are 64*16 = 1024         */
FwHeader fwHeader = {
	0,         //cursorpo
	0, 123, 0, 0, //seq, magic, crc, len uint16
};

/* pointer to a struck containing 1024 fw bytes to be send */
static struct FwPart *fwP;

// magic number for Moteino Firmware
#define FLASH_MAGIC  (0x4BAE)
#define FW_HEADER_ADDR   (FIRMWARE_SIZE - 2*ATMEGA_FLASH_MAX) //flash at the end of this partition 962560 -> sector 235
// static int flash_pri  = 0; // primary flash sector (0 or 1, or -1 for error)

// #define FIRMWARE_SIZE 0
const char termStr[5] = { '\r', '\n', '\r', '\n', '\0' }; // \r\n\r\n

bool ICACHE_FLASH_ATTR savePart(void) {
	if ((fwHeader.cur <= 100) || (fwHeader.cur > FIRMWARE_SIZE)) {
		#ifdef CGIFLASH_DBG
		os_printf("Invalid fw Addr (0x%05x)\n", (unsigned int)fwHeader.cur);
		#endif
		goto fail;
	}

	uint16 curseq = fwHeader.seq;
	curseq +=1;
  // erase secondary
  uint32_t addr = fwHeader.cur;
	if (curseq == 1) {
		//#ifdef CGIFLASH_DBG
		os_printf("seq %d: erasing next sector at %d\n", curseq, 235);
		//#endif
		if (spi_flash_erase_sector(235) != SPI_FLASH_RESULT_OK) {
			goto fail;
		}
	} else if (curseq%4 == 0) {
		os_printf("seq %d: erasing next sector at %d\n", curseq, (235+(curseq)/4));
		if (spi_flash_erase_sector((235+(curseq)/4)) != SPI_FLASH_RESULT_OK) {
			goto fail;
		}
	}
	fwHeader.seq = curseq;
	os_printf("seq %d: \n", (fwHeader.seq));
  // calculate CRC
	fwHeader.magic = FLASH_MAGIC;
	// fwPart.crc = inet_chksum((void *)&fwPart.buf, sizeof(fwPart.buf));
  // os_printf("cksum is now %04x\n", fwPart.crc);
  // write primary with incorrect seq
  if (spi_flash_write(addr, (uint32 *)fwP->buf.bytes, 1024) != SPI_FLASH_RESULT_OK) {
		goto fail; // no harm done, give up
	}
  // spi_flash_write(addr, (void *)&fwPart, sizeof(uint32_t));
  return true;
fail:
  os_printf("*** Failed to save part data ***\n");
	// memDump(&fwPart, sizeof(fwPart));
  return false;
}

static int clickCount = 0;
int ICACHE_FLASH_ATTR cgiFlashTest(HttpdConnData *connData)
{
	uint32 tmp_buf[64];
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	uint32 addr = FW_HEADER_ADDR + (clickCount*32);
	spi_flash_read(addr, (uint32*)tmp_buf, 32);
	memDump(&tmp_buf, 32);
	clickCount++;
	if (clickCount == 1024) {
		clickCount = 0;
	}
	return HTTPD_CGI_DONE;
}

static int lenToSkip = 0;
static int payloadSeq = 0;
//Cgi that allows the firmware to be replaced via http POST
int ICACHE_FLASH_ATTR cgiUploadFlash(HttpdConnData *connData)
{
	static int buffPos;
	int buff_offset = 0;
	char *b = NULL;
	int skip = 0;

	if (connData->conn==NULL)
	{
		payloadSeq = 0;
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	// assume no error yet...
	char *err = NULL;
	int code = 400;

	if (connData->post==NULL) err="No POST request."; // check if POST
	if (err==NULL && connData->post->len > ATMEGA_FLASH_MAX) err = "Firmware image too large"; // check overall size
	//if (err == NULL && offset == 0 && def->type==CGIFLASH_TYPE_FW) err = checkBinHeader(connData->post->buff);

	int offset = connData->post->received - connData->post->buffLen;
	// make sure we're buffering in 1024 byte chunks
	if (err == NULL && (offset%1024 != 0)) {
		err = "Buffering problem";
		os_printf("Bad offset. offset = %d, buff_offset = %d\n", offset, buff_offset);
		code = 500;
	}

	if (err == NULL && (payloadSeq == 0))
	{
		// os_memcpy(&fwPart.fwHeader, &fwHeaderDefault, sizeof(fwHeaderDefault));
		#ifdef CGIFLASH_DBG
		os_printf("Setting up. payloadseq = 0\n");
		#endif
		fwP = (struct FwPart*) os_malloc(sizeof(struct FwPart));
		fwHeader.len = 0;
		fwHeader.cur = FW_HEADER_ADDR;
		fwHeader.seq = 0;
	}


	// return an error if there is one
	if (err != NULL)
	{
		payloadSeq = 0;
		os_printf("Error %d: %s\n", code, err);
		httpdStartResponse(connData, code);
		httpdHeader(connData, "Content-Type", "text/plain");
		httpdEndHeaders(connData);
		httpdSend(connData, "Firmware image error:\r\n", -1);
		httpdSend(connData, err, -1);
		httpdSend(connData, "\r\n", -1);
		connData->cgiPrivData = (void *)1;
		return HTTPD_CGI_DONE;
	}

	if ((b = os_strstr(connData->post->buff, connData->post->multipartBoundary)) != NULL)
	{
		char *term = NULL;
		if ((term = os_strstr(b, termStr)) != NULL)
		{
			skip = ((unsigned int)term - (unsigned int)connData->post->buff) + 4; //acount for \r\n
			fwHeader.len -= skip;
		}
	}
	buff_offset = connData->post->received - connData->post->buffLen + lenToSkip;

	/* Make sure we actually have data */
	if (buff_offset == 0) {
		connData->cgiPrivData = NULL;
	} else if (connData->cgiPrivData != NULL) {
		os_printf("error!\n");
		payloadSeq = 0;
		return HTTPD_CGI_DONE;
	}

	#ifdef CGIFLASH_DBG
	os_printf("PayloadSeq[%d] : ", payloadSeq);
	#endif
	if (connData->post->received == connData->post->len)
	{
		char endBuff[100];
		int endBoundLen = os_sprintf(endBuff, "\r\n\r\n%s--", connData->post->multipartBoundary);
		int lenLast = connData->post->buffLen - endBoundLen;// - 1;
		int remLast = (lenLast%4);
		if (remLast) {
			lenLast += (4-remLast);
			fwHeader.len -= (4-remLast); //need aligned for memcpy, but don't want rfm to send
		}

		#ifdef CGIFLASH_DBG
		os_printf("lenLast = %d. Mis-aligned by %d bytes\n", lenLast, remLast);
		#endif
		os_memcpy(fwP->buf.dword+((1024-lenToSkip)/4), connData->post->buff, lenToSkip);
		if (savePart()) {
			fwHeader.cur += 1024;
			fwHeader.len += 1024;
			os_printf("lenLast Aligned = %d\n", lenLast );
			os_memcpy(fwP->buf.dword, connData->post->buff+lenToSkip, lenLast);
			os_memset(fwP->buf.dword+((lenLast+lenToSkip)/4), 0, (1024-lenLast));//-lenToSkip));
		}

		if (!(savePart()))
		{
			os_printf("savePart() failed..\n");
		}
		os_printf("sizeof(fwHeader) = %d\n", sizeof(fwHeader));
		fwHeader.len += lenLast;
		// os_memcpy(&fwHeader, &fwPart.fwh, sizeof(fwPart.fwh));
		fwHeader.cur = FW_HEADER_ADDR;// + sizeof(fwHeader); //reset cursor to beginning
		fwHeader.seq = 0;
		fwP->crc = 0;
		buffPos = 0;
		payloadSeq = 0;
		lenToSkip = 0;
		if (fwP!=NULL) {
			os_printf("freeing fwp..\n");
			os_free(fwP);
			fwP = NULL;
		}
		httpdStartResponse(connData, 200);
		httpdEndHeaders(connData);
		os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
		return HTTPD_CGI_DONE;
	}
	else
	{
		lenToSkip += skip;
		/* Fill rest of buffer and flash */
		if ((lenToSkip + buffPos) == 1024)
		{
			// os_memcpy(fwPart->buf.dword[((1024-lenToSkip)/4)], connData->post->buff, lenToSkip);
			os_memcpy(fwP->buf.dword+((1024-lenToSkip)/4), connData->post->buff, lenToSkip);
			#ifdef CGIFLASH_DBG
			os_printf("Buffer filled. Writing to SPI Flash\n");
			#endif
			if (savePart())
			{
				fwHeader.cur += 1024;
				fwHeader.len += 1024;
				buffPos = 0;
			} else {
				#ifdef CGIFLASH_DBG
				os_printf("savePart() failed..\n");
				#endif
			}
			/* now fill the buffer with the next */
			os_memcpy(fwP->buf.dword, connData->post->buff+lenToSkip, 1024 - lenToSkip);
			buffPos = 1024 - lenToSkip;
		}
		else /* Fill beginning of buffer*/
		{
			if (buffPos == 0) {
				os_memcpy(fwP->buf.dword, connData->post->buff+lenToSkip,1024 - lenToSkip);
				buffPos = 1024 - lenToSkip;
			} else if (buffPos%4 == 0) {
				os_printf("Filling buf at %d\n", (buffPos/4));
				os_memcpy(fwP->buf.dword, connData->post->buff+lenToSkip,1024 - lenToSkip);
				// os_memcpy(&fwPart.buf.dword[(buffPos/4)], &connData->post->buff[lenToSkip], connData->post->buffLen - lenToSkip);
				buffPos = 1024 - lenToSkip;
			} else {
				#ifdef CGIFLASH_DBG
				os_printf("uh oh, buffer is not aligned\n");
				#endif
			}
		}
		payloadSeq++;
		return HTTPD_CGI_MORE;
	}
}

// Check that the header of the firmware blob looks like actual firmware...
static char* ICACHE_FLASH_ATTR check_header(void *buf) {
	uint8_t *cd = (uint8_t *)buf;
	#ifdef CGIFLASH_DBG
	uint32_t *buf32 = buf;
	os_printf("%p: %08lX %08lX %08lX %08lX\n", buf, buf32[0], buf32[1], buf32[2], buf32[3]);
	#endif
	if (cd[0] != 0xEA) return "IROM magic missing";
	if (cd[1] != 4 || cd[2] > 3 || (cd[3]>>4) > 6) return "bad flash header";
	if (((uint16_t *)buf)[3] != 0x4010) return "Invalid entry addr";
	if (((uint32_t *)buf)[2] != 0) return "Invalid start offset";
	return NULL;
}

// Cgi to query which firmware needs to be uploaded next
int ICACHE_FLASH_ATTR cgiGetFirmwareNext(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}
	uint8 id = system_upgrade_userbin_check();
	httpdStartResponse(connData, 200);
	httpdHeader(connData, "Content-Type", "text/plain");
	httpdHeader(connData, "Content-Length", "9");
	httpdEndHeaders(connData);
	char *next = id == 1 ? "user1.bin" : "user2.bin";
	httpdSend(connData, next, -1);
	os_printf("Next firmware: %s (got %d)\n", next, id);
	return HTTPD_CGI_DONE;
}

//Cgi that reads the SPI flash. Assumes 4096KByte flash.
//ToDo: Figure out real flash size somehow?
int ICACHE_FLASH_ATTR cgiReadFlash(HttpdConnData *connData) {
	int *pos=(int *)&connData->cgiData;
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	if (*pos==0) {
		os_printf("Start flash download.\n");
		httpdStartResponse(connData, 200);
		httpdHeader(connData, "Content-Type", "application/bin");
		httpdEndHeaders(connData);
		*pos=0x40200000;
		return HTTPD_CGI_MORE;
	}
	//Send 1K of flash per call. We will get called again if we haven't sent 512K yet.
	espconn_sent(connData->conn, (uint8 *)(*pos), 1024);
	*pos+=1024;
	if (*pos>=0x40200000+(4096*1024)) return HTTPD_CGI_DONE; else return HTTPD_CGI_MORE;
}

//Cgi that allows the firmware to be replaced via http POST
int ICACHE_FLASH_ATTR cgiUploadFirmware(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

	int offset = connData->post->received - connData->post->buffLen;
	if (offset == 0) {
		connData->cgiPrivData = NULL;
	} else if (connData->cgiPrivData != NULL) {
		// we have an error condition, do nothing
		os_printf("ERR: cgiPrivData != NULL\n");
		return HTTPD_CGI_DONE;
	}

	// assume no error yet...
	char *err = NULL;
	int code = 400;

	// check overall size
	//os_printf("FW: %d (max %d)\n", connData->post->len, FIRMWARE_SIZE);
	if (connData->post->len > ATMEGA_FLASH_MAX) err = "Moteino Firmware image too large";
	if (connData->post->buff == NULL || connData->requestType != HTTPD_METHOD_POST ||
			connData->post->len < 1024) err = "Invalid request";

	// check that data starts with an appropriate header
	if (err == NULL && offset == 0) err = check_header(connData->post->buff);

	// make sure we're buffering in 1024 byte chunks
	if (err == NULL && offset % 1024 != 0) {
		err = "Buffering problem";
		code = 500;
	}

	// return an error if there is one
	if (err != NULL) {
		#ifdef CGIFLASH_DBG
		os_printf("Error %d: %s\n", code, err);
		#endif
		httpdStartResponse(connData, code);
		httpdHeader(connData, "Content-Type", "text/plain");
		//httpdHeader(connData, "Content-Length", strlen(err)+2);
		httpdEndHeaders(connData);
		httpdSend(connData, err, -1);
		httpdSend(connData, "\r\n", -1);
		connData->cgiPrivData = (void *)1;
		return HTTPD_CGI_DONE;
	}

	// let's see which partition we need to flash and what flash address that puts us at
	uint8 id = system_upgrade_userbin_check();
	int address = id == 1 ? 4*1024                   // either start after 4KB boot partition
	    : 4*1024 + FIRMWARE_SIZE + 16*1024 + 4*1024; // 4KB boot, fw1, 16KB user param, 4KB reserved
	address += offset;

	// erase next flash block if necessary
	if (address % SPI_FLASH_SEC_SIZE == 0){
		#ifdef CGIFLASH_DBG
		os_printf("Flashing 0x%05x (id=%d)\n", address, 2-id);
		#endif
		spi_flash_erase_sector(address/SPI_FLASH_SEC_SIZE);
	}

	// Write the data
	//os_printf("Writing %d bytes at 0x%05x (%d of %d)\n", connData->post->buffSize, address,
	//		connData->post->received, connData->post->len);
	spi_flash_write(address, (uint32 *)connData->post->buff, connData->post->buffLen);

	if (connData->post->received == connData->post->len){
		httpdStartResponse(connData, 200);
		httpdEndHeaders(connData);
		return HTTPD_CGI_DONE;
	} else {
		return HTTPD_CGI_MORE;
	}
}

//static ETSTimer flash_reboot_timer;

// Handle request to reboot into the new firmware
int ICACHE_FLASH_ATTR cgiRebootFirmware(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	// TODO: sanity-check that the 'next' partition actually contains something that looks like
	// valid firmware

	// This should probably be forked into a separate task that waits a second to let the
	// current HTTP request finish...
	system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
	system_upgrade_reboot();
	httpdStartResponse(connData, 200);
	httpdEndHeaders(connData);
	return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR cgiFlashInit() {
	// os_memcpy(&fwPart.fwh, &fwHeaderDefault, sizeof(fwHeaderDefault));
	// os_memcpy(&fwHeader, &fwHeaderDefault, sizeof(fwHeaderDefault));
	#ifdef CGIFLASH_DBG
	os_printf("cgiFlashInit\n");
	#endif
}

void memDump(void *addr, int len) {
  for (int i=0; i<len; i++) {
		if (i%16 == 0) {
			os_printf("\n");
		}
    os_printf("%02x", ((uint8_t *)addr)[i]);
  }
  os_printf("\n");
}
