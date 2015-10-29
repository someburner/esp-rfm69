#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "uart.h"
#include "driver/rfm69.h"
#include "radiohandler.h"
#include "config.h"
#include "console.h"

// Microcontroller console capturing the last 1024 characters received on the uart so
// they can be shown on a web page

// Buffer to hold concole contents.
// Invariants:
// - console_rd==console_wr <=> buffer empty
// - *console_rd == next char to read
// - *console_wr == next char to write
// - 0 <= console_xx < BUF_MAX
// - (console_wr+1)%BUF_MAX) == console_rd <=> buffer full
#define radioTaskPrio        1
#define BUF_MAX (1024)

char sendTxBuff[20];
static char console_buf[BUF_MAX];
static int console_wr, console_rd;
static int console_pos; // offset since reset of buffer

static void ICACHE_FLASH_ATTR
console_write(char c) {
  console_buf[console_wr] = c;
  console_wr = (console_wr+1) % BUF_MAX;
  if (console_wr == console_rd) {
    // full, we write anyway and loose the oldest char
    console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
    console_pos++;
  }
}

void ICACHE_FLASH_ATTR
console_write_char(char c) {
  //if (c == '\n' && console_prev() != '\r') console_write('\r'); // does more harm than good
  console_write(c);
}

void ICACHE_FLASH_ATTR
console_write_string(char* buff, int len) {
	for (int k=0; k < len; k++) {
		console_write_char(buff[k]);
	}
}

int ICACHE_FLASH_ATTR
ajaxConsoleClear(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  jsonHeader(connData, 200);
  console_rd = console_wr = console_pos = 0;
  return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsoleFwUpdate(HttpdConnData *connData) {
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	os_printf("Attempting to flash node %d...\n", 3);
	system_os_post(radioTaskPrio, 0, rfm_fw_begin);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsoleSendData(HttpdConnData *connData) {
	char nodebuff[128];
	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	int len = 0;
	int nodeid = 0;
	int nodelen = httpdFindArg(connData->getArgs, "nodeid", nodebuff, sizeof(nodebuff));
	if (nodelen > 0) {
		nodeid = atoi(nodebuff);
	}
	if (nodeid == 0) {
		os_printf("invalid nodeid\n");
		return HTTPD_CGI_DONE;
	}
	char buff[128];
	len = httpdFindArg(connData->getArgs, "txt", buff, sizeof(buff));
	if (len > 0) {
		os_printf("Sending %s to node %d\n", buff, nodeid);
		for (int i=0; i<len; i++) {
			sendTxBuff[i] = buff[i];
		}
		radioMsgTxPush(nodeid, (uint8_t*)sendTxBuff, len, false, false);
		createRadioTask(rfm_send, rfm_event_default);
	}
	int status = 200;
	jsonHeader(connData, status);
	//httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR
ajaxConsole(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  char buff[2048];
  int len; // length of text in buff
  int console_len = (console_wr+BUF_MAX-console_rd) % BUF_MAX; // num chars in console_buf
  int start = 0; // offset onto console_wr to start sending out chars

  jsonHeader(connData, 200);

  // figure out where to start in buffer based on URI param
  len = httpdFindArg(connData->getArgs, "start", buff, sizeof(buff));
  if (len > 0) {
    start = atoi(buff);
    if (start < console_pos) {
      start = 0;
    } else if (start >= console_pos+console_len) {
      start = console_len;
    } else {
      start = start - console_pos;
    }
  }

  // start outputting
  len = os_sprintf(buff, "{\"len\":%d, \"start\":%d, \"text\": \"",
      console_len-start, console_pos+start);

  int rd = (console_rd+start) % BUF_MAX;
  while (len < 2040 && rd != console_wr) {
    uint8_t c = console_buf[rd];
    if (c == '\\' || c == '"') {
      buff[len++] = '\\';
      buff[len++] = c;
    } else if (c == '\r') {
      // this is crummy, but browsers display a newline for \r\n sequences
    } else if (c < ' ') {
      len += os_sprintf(buff+len, "\\u%04x", c);
    } else {
      buff[len++] = c;
    }
    rd = (rd + 1) % BUF_MAX;
  }
  os_strcpy(buff+len, "\"}"); len+=2;
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

void ICACHE_FLASH_ATTR consoleInit() {
  console_wr = 0;
  console_rd = 0;
}
