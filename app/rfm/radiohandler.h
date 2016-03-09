#ifndef RADIOHANDLER_H
#define RADIOHANDLER_H

#include "user_config.h"
#include "osapi.h"
#include "rfm.h"
#include "rfm_parser.h"

#define BUF_MAX (128) //max size of console.html log

typedef struct {
  uint16_t freq;
  int16_t  rssi;
  int16_t  batt;
  uint8_t  axis;
  uint32_t totalct;
} RadioStatus;

enum { radioUnknown, radioIsDisconnected, radioIsConnected};
uint8_t radioState;

char console_buf[BUF_MAX];
int console_wr;
int console_rd;
int console_pos;

void console_write_char(char c);
void console_write_string(char* buff, int len);

// void generateRfmTxMsg(int toId, int requestAck, void* data, int len);
void generateRfmTxMsg(int toId, void* data, int len, bool requestAck, bool sendACK);
void disableSendTimers();

void rfm_begin_ota(char * binName, uint8_t nid);
void rfm_end_ota();

void process_rfm_ota_msg(RFM_Handle * r);
void generate_next_OTA_msg();

void rfmSendTimerCb(void *v);
void radioHandlerInit(RFM_Handle * r);


#endif
