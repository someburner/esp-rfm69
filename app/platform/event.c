#include "user_config.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_types.h"
#include "user_interface.h"

#include "driver/gpio16.h"

#include "mqtt/mqtt_api.h"
#include "util/netutil.h"
#include "event.h"

typedef enum {
   wifi_disconn,
   wifi_isconn,
   wifi_gotip
} config_type_t;

#define HOST_STR_STR(V) #V
#define HOST_STR(V) HOST_STR_STR(V)

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

/* WiFi Failure reasons. See user_interface.h */
uint8_t wifiReason = 0;
static char *wifiReasons[] =
{
   "", "unspecified", "auth_expire", "auth_leave", "assoc_expire",            //4
   "assoc_toomany", "not_authed", "not_assoced", "assoc_leave",               //8
   "assoc_not_authed", "disassoc_pwrcap_bad", "disassoc_supchan_bad", "",     //12
   "ie_invalid", "mic_failure", "4way_handshake_timeout",                     //15
   "group_key_update_timeout", "ie_in_4way_differs", "group_cipher_invalid",  //18
   "pairwise_cipher_invalid", "akmp_invalid", "unsupp_rsn_ie_version",        //21
   "invalid_rsn_ie_cap", "802_1x_auth_failed", "cipher_suite_rejected",       //24
   "beacon_timeout", "no_ap_found"                                            //26
};

// callback when wifi status changes
// void (*wifiStatusCb)(uint8_t);

static char* wifiGetReason(void)
{
   if (wifiReason <= 24) return wifiReasons[wifiReason];
   if (wifiReason >= 200 && wifiReason <= 201) return wifiReasons[wifiReason-200+24];
   return wifiReasons[1];
}

void user_mqtt_timeout_cb(uint32_t *arg)
{
   NODE_DBG("user_mqtt_timeout\n");
}

static void nslookup_cb(const char *name, ip_addr_t *ip, void *arg) {
	char ipstr[17];
	if (ip == NULL) {
      NODE_DBG("Dns Failed\n");
	} else {
      ipaddr_ntoa_r(ip, ipstr, sizeof(ipstr));
      NODE_DBG("Host: %s -> ip: %s\n", name, ipstr);
      #ifdef MQTT_EXPECT_IP
      if (os_strcmp(ipstr, DEFAULT_MQTT_EXPECT_IP) != 0) NODE_DBG("Dns mismatch?\n");
      #endif
      if (MQTT_ENABLE == 0) return;
      if (MQTT_USE_IP == 0)
      {
         mqtt_app_init(ipstr);
         mqtt_setconn(1);
      }
	}
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
}

// change the rfm69 state indication
void statusRfmUpdate(uint8_t state)
{
   rfmState = state;
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
         if (MQTT_ENABLE == 1)
         {
            mqtt_setconn(0);
         }

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
         if (MQTT_ENABLE == 0) break;

         if (MQTT_USE_IP == 0)
         {
            netutil_nslookup(HOST_STR(MQTT_HOST_NAME), nslookup_cb);
         } else {
            mqtt_app_init(HOST_STR(DEFAULT_MQTT_IP));
            mqtt_setconn(1);
         }
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

void user_event_init()
{
   NODE_DBG("user_event_init\n");

   wifi_set_event_handler_cb(wifiHandleEventCb);

}
