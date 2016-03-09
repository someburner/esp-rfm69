#ifndef __STATUS_H__
#define __STATUS_H__
#include "c_types.h"

uint8_t wifiStatus_mq;
uint8_t lastwifiStatus_mq;
enum { wifiIsDisconnected, wifiIsConnected, wifiGotIP };
enum { rfm_unknown, rfm_timed_out, rfm_connected };

typedef void(*WifiStateChangeCb)(uint8_t wifiStatus);
void statusWifiUpdate(uint8_t state);
void statusRfmUpdate(uint8_t state);
void statusInit(void);

#endif
