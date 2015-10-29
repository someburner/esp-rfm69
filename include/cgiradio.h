#ifndef CGIRADIO_H
#define CGIRADIO_H

#include <esp8266.h>
#include "httpd.h"

enum { radioUnknown, radioIsDisconnected, radioIsConnected};

typedef struct {
  uint16_t freq;
  int16_t  rssi;
  int16_t batt;
  uint8_t  axis;
  uint32_t totalPeaks;
} RadioStatus;

extern RadioStatus radioStatus;

void ICACHE_FLASH_ATTR printRfmData();
int cgiRadioStatus(HttpdConnData *connData);
int cgiRadioConnStatus(HttpdConnData *connData);
int cgiRadioSpecial(HttpdConnData *connData);
int cgiServerGet(HttpdConnData *connData);
void cgiRadioInit(void);

extern uint8_t radioState;

extern void (*radioStatusCb)(uint8_t); // callback when radio status changes

#endif
