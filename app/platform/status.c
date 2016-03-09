#include "user_config.h"
#include "status.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_types.h"
#include "user_interface.h"

#include "driver/gpio16.h"

#ifdef RFM_MQTT_EN
#include "mqtt_api.h"
#endif

#define WIFI_LED_PIN_NUM    3 //GPIO0
#define RFM_LED_PIN_NUM     1 //GPIO5

typedef enum {
   wifi_disconn,
   wifi_isconn,
   wifi_gotip
} config_type_t;

// wifi status change callbacks
static WifiStateChangeCb wifi_state_change_cb[4];

uint8_t wifiStatus_mq = 0; 		//STATION_IDLE
uint8_t lastwifiStatus_mq = 0; 	//STATION_IDLE

// initial state
uint8_t wifiState = wifiIsDisconnected;
uint8_t rfmState =  rfm_unknown;

// Timers
static ETSTimer wifiLedTimer;
static ETSTimer rfmLedTimer;

static uint8_t wifiLedState = 0;
static uint8_t rfmLedState = 0;

// reasons for which a connection failed
uint8_t wifiReason = 0;
static char *wifiReasons[] =
{
   "", "unspecified", "auth_expire", "auth_leave", "assoc_expire",
   "assoc_toomany", "not_authed", "not_assoced", "assoc_leave",
   "assoc_not_authed", "disassoc_pwrcap_bad", "disassoc_supchan_bad",
   "ie_invalid", "mic_failure",

   "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
   /*
   "4way_handshake_timeout",
   "group_key_update_timeout", "ie_in_4way_differs", "group_cipher_invalid",
   "pairwise_cipher_invalid", "akmp_invalid", "unsupp_rsn_ie_version",
   "invalid_rsn_ie_cap", "802_1x_auth_failed", "cipher_suite_rejected",
   */
   "beacon_timeout", "no_ap_found"
};

// callback when wifi status changes
// void (*wifiStatusCb)(uint8_t);

static char* wifiGetReason(void)
{
   if (wifiReason <= 24) return wifiReasons[wifiReason];
   if (wifiReason >= 200 && wifiReason <= 201) return wifiReasons[wifiReason-200+24];
   return wifiReasons[1];
}

// Set the LED on or off, respecting the defined polarity
static void setLed(int on, int8_t pin)
{
   if (pin < 0) return; // disabled
   // LED is active-low
   if (on) { gpio_write(pin, on); }
   else { gpio_write(pin, 0); }
}

// Timer callback to update WiFi status LED
static void wifiLedTimerCb(void *v)
{
   int time = 1000;
   if (wifiState == wifi_gotip)
   {
      // connected, all is good, solid light with a short dark blip every 3 seconds
      wifiLedState = 1;
      time = 15000;
   }
   else if (wifiState == wifi_isconn)
   {
      // waiting for DHCP, go on/off every second
      wifiLedState = 1 - wifiLedState;
      time = 1000;
   }
   else
   {
      // not connected
      switch (wifi_get_opmode())
      {
         case 1: // STA
            wifiLedState = 0;
            break;
         case 2: // AP
            wifiLedState = 1-wifiLedState;
            time = wifiLedState ? 50 : 1950;
            break;
         case 3: // STA+AP
            wifiLedState = 1-wifiLedState;
            time = wifiLedState ? 50 : 950;
            break;
      }
   }
   setLed(wifiLedState, WIFI_LED_PIN_NUM);
   os_timer_arm(&wifiLedTimer, time, 0);
}

// Timer callback to update RFM69 status LED
static void rfmLedTimerCb(void *v)
{
   int time = 1000;
   if (rfmState == rfm_unknown)
   {
      // connected, all is good, solid light with a short dark blip every 3 seconds
      rfmLedState = 1-rfmLedState;
      time = rfmLedState ? 2900 : 1200;
   }
   else if (rfmState == rfm_connected)
   {
      // waiting for DHCP, go on/off every second
      rfmLedState = 1;
      time = 10001;
   }
   else
   {
      // not connected
      rfmLedState = 1-rfmLedState;
      time = rfmLedState ? 950 : 1950;
   }
   setLed(rfmLedState, RFM_LED_PIN_NUM);
   os_timer_arm(&rfmLedTimer, time, 0);
}

// change the wifi state indication
void statusWifiUpdate(uint8_t state)
{
   uint32 time_next = 523;
   wifiState = state;
   if (wifiState == wifiIsDisconnected)
   {
      time_next = 10933;
   }
   // schedule an update (don't want to run into concurrency issues)
   os_timer_disarm(&wifiLedTimer);
   os_timer_setfn(&wifiLedTimer, wifiLedTimerCb, NULL);
   os_timer_arm(&wifiLedTimer, time_next, 0);
}

// change the rfm69 state indication
void statusRfmUpdate(uint8_t state)
{
   rfmState = state;
   // schedule an update (don't want to run into concurrency issues)
   os_timer_disarm(&rfmLedTimer);
   os_timer_setfn(&rfmLedTimer, rfmLedTimerCb, NULL);
   os_timer_arm(&rfmLedTimer, 527, 0);
}

// handler for wifi status change callback coming in from espressif library
static void wifiHandleEventCb(System_Event_t *evt) {
   static int retryCount = 0;
   lastwifiStatus_mq = wifiStatus_mq;
   switch (evt->event)
   {
      case EVENT_STAMODE_CONNECTED:
      {
         retryCount = 0;
         wifiState = wifiIsConnected;
         wifiReason = 0;
         NODE_DBG("Wifi connected to ssid %s, ch %d\n", evt->event_info.connected.ssid,
         evt->event_info.connected.channel);
         statusWifiUpdate(wifiState);
      } break;
      case EVENT_STAMODE_DISCONNECTED:
      {
         retryCount++;
         wifiState = wifiIsDisconnected;
         wifiReason = evt->event_info.disconnected.reason;
         NODE_DBG("Wifi disconnected from ssid %s, reason %s (%d)\n",
         evt->event_info.disconnected.ssid, wifiGetReason(), evt->event_info.disconnected.reason);
         wifiStatus_mq = wifiIsDisconnected;
         #ifdef RFM_MQTT_EN
         mqtt_setconn(0);
         #endif
         statusWifiUpdate(wifiState);
      } break;
      case EVENT_STAMODE_AUTHMODE_CHANGE:
      {
         NODE_DBG("Wifi auth mode: %d -> %d\n",
         evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
      } break;
      case EVENT_STAMODE_GOT_IP:
      {
         retryCount = 0;
         wifiState = wifiGotIP;
         wifiReason = 0;
         NODE_DBG("Wifi got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
         IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask),
         IP2STR(&evt->event_info.got_ip.gw));
         statusWifiUpdate(wifiState);
         wifiStatus_mq = 2;
         #ifdef RFM_MQTT_EN
         mqtt_setconn(1);
         #endif

         // wifiStartMDNS(evt->event_info.got_ip.ip);
      } break;
      case EVENT_SOFTAPMODE_STACONNECTED:
      {
         NODE_DBG("Wifi AP: station " MACSTR " joined, AID = %d\n",
         MAC2STR(evt->event_info.sta_connected.mac), evt->event_info.sta_connected.aid);
      } break;
      case EVENT_SOFTAPMODE_STADISCONNECTED:
      {
         NODE_DBG("Wifi AP: station " MACSTR " left, AID = %d\n",
         MAC2STR(evt->event_info.sta_disconnected.mac), evt->event_info.sta_disconnected.aid);
      } break;
      default: break;
   }

   int i;
   for (i = 0; i < 4; i++)
   {
      if (wifi_state_change_cb[i] != NULL) (wifi_state_change_cb[i])(wifiState);
   }
}

void statusInit(void)
{
   STATUS_DBG("CONN led=%d\n", 1);

   wifi_set_event_handler_cb(wifiHandleEventCb);

   os_timer_disarm(&wifiLedTimer);
   os_timer_setfn(&wifiLedTimer, wifiLedTimerCb, NULL);
   os_timer_arm(&wifiLedTimer, 2003, 0);

   os_timer_disarm(&rfmLedTimer);
   os_timer_setfn(&rfmLedTimer, rfmLedTimerCb, NULL);
   os_timer_arm(&rfmLedTimer, 5517, 0);

}
