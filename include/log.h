#ifndef LOG_H
#define LOG_H

#include "httpd.h"
#include "cgiwebsocket.h"
#include "webpages-espfs.h"

struct log_st {
	STAILQ_ENTRY(log_st) next;
	uint8_t  len;
	char*    buf;
};

STAILQ_HEAD(log_head, log_st) log_list;

void logInit(void);
void logPush(char *buffer, short length);
void logWsRecv(Websock *ws, char *data, int len, int flags);
void logWsConnect(Websock *ws);

#endif
