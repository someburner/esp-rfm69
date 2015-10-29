#include <esp8266.h>
#include "uart.h"
#include "cgi.h"
#include "config.h"
#include "queue.h"
#include "log.h"

#define LOG_QUEUE_LEN         10

#define BUF_MAX (128)
static char log_buf[BUF_MAX];
static int log_wr, log_rd;
static int log_pos;
static bool log_newline; // at start of a new line
static int sock_logsent = 0;

static int logLen = 0;
static ETSTimer websockLogTimer;

void ICACHE_FLASH_ATTR logPush(char *buffer, short length)
{
  if (length != 0) {
		/* Create new log_st struct and add to queue */
    struct log_st *msg = NULL;
    msg = (struct log_st *)os_zalloc(sizeof(struct log_st));
    msg->buf = (char*)os_zalloc(length);
    msg->len = length;
    if (os_memcpy(msg->buf, buffer, length) != 0) {
		STAILQ_INSERT_TAIL(&log_list, msg, next);
		logLen++;
    }
  }

	/* Clean up old Tx messages */
  if (logLen > LOG_QUEUE_LEN) {
	struct log_st *np = STAILQ_FIRST(&log_list);
	char* npBuf = np->buf;
	os_free(npBuf);
	STAILQ_REMOVE(&log_list, np, log_st, next);
	os_free(np);
	logLen--;
   sock_logsent--;
  }
}

static void ICACHE_FLASH_ATTR websockLogTimerCb(void *arg)
{
	os_timer_disarm(&websockLogTimer);

	if (sock_logsent == logLen)
	{
		cgiWebsockBroadcast("/ws/logws.cgi", ".", 1, WEBSOCK_FLAG_NONE);
		return;
	}
	if (sock_logsent > logLen) sock_logsent = logLen;

	int queuePos = sock_logsent;

	if (queuePos < logLen)
	{
		struct log_st *sock_out;
		int pos = 0;
		STAILQ_FOREACH(sock_out, &log_list, next)
		{
			if (sock_out == NULL) {
				os_printf(" ERROR: No sock_out buff found\n");
				return;
			} else if (pos == queuePos) { //found next msg
				sock_logsent++;
				cgiWebsockBroadcast("/ws/logws.cgi", sock_out->buf, sock_out->len, WEBSOCK_FLAG_NONE);
				break;
			}
			pos++;
		}
	}
}

//On reception of a message, send "You sent: " plus whatever the other side sent
void logWsRecv(Websock *ws, char *data, int len, int flags) {
	os_timer_disarm(&websockLogTimer);
	if (atoi(data) == 1) {
		os_timer_arm(&websockLogTimer, 10003, 0);
      os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
	} else {
		os_timer_arm(&websockLogTimer, 499, 0);
	}
}

//Websocket connected. Install reception handler and send welcome message.
void logWsConnect(Websock *ws) {
	ws->recvCb=logWsRecv;
	sock_logsent = 0;
	os_timer_arm(&websockLogTimer, 502, 0);
}

static void ICACHE_FLASH_ATTR
log_write(char c) {
	log_buf[log_wr] = c;
	if ((log_wr+1) == BUF_MAX) {
		logPush(log_buf, log_wr);
		log_wr = 0;
	} else {
		log_wr++;
	}
	if (log_wr == log_rd) { //empty
		log_rd = (log_rd+1) % BUF_MAX; // full, eat first char
		log_pos++;
	}
}

static void ICACHE_FLASH_ATTR
log_write_char(char c) {
	// Uart output unless disabled
	if (log_newline) {
		uart0_write_char('>');
		log_newline = false;
	}
	uart0_write_char(c);
	if (c == '\n') {
		log_newline = true;
		uart0_write_char('\r');
	}
	// Store in log buffer
	if (c == '\n') log_write('\r');
	log_write(c);
}

void ICACHE_FLASH_ATTR logInit() {
	log_wr = 0;
	log_rd = 0;
 	os_install_putc1((void *)log_write_char);
	STAILQ_INIT(&log_list);

	os_timer_disarm(&websockLogTimer);
	os_timer_setfn(&websockLogTimer, websockLogTimerCb, NULL);
}
