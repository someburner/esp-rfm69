#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define DEVELOP_VERSION
#define BIT_RATE_DEFAULT         BIT_RATE_115200 // BIT_RATE_74880
#define FLASH_4M
#define GPIO_INTERRUPT_ENABLE    1

#define INTERFACE_DOMAIN         "smart.rfm.com"

// #define MQTT_SERVER_IP           "10.42.0.1"
#define RFM_MAX_DEVICE_CT      4

enum {
   rfm_device_null          = 0,
   rfm_device1              = 1,
   rfme_device2              = 2,
   rfm_device_max           = 3
};

#define PEAK_QUEUE_MAX           5

#define RFM_TASK_PRIO            USER_TASK_PRIO_1
#define RFM_TASK_QUEUE_SIZE      5

// #define RFM_MQTT_EN              1


#define MQTT_SERVER_DOMAIN       "your.server.com"
/* Number of MQTT Topics: 1 for each MQTT topic route */
#define MQTT_TOPIC_COUNT         MQTT_DEVICE_COUNT
/* Num Devices: Minimum 1 (for Bridge device) + 1 for each associated device */
#define MQTT_DEVICE_COUNT        2

#define MQTT_TASK_PRIO        	USER_TASK_PRIO_2
#define MQTT_TASK_QUEUE_SIZE    	1
#define MQTT_ERROR

#ifdef MQTT_ERROR
#define MQTT_ERR(a)           c_printf("MQTT ERR# %d\n", a)
#else
#define MQTT_ERR
#endif


#ifdef DEVELOP_VERSION
#define NODE_DEBUG
#define NODE_ERROR
#endif	/* DEVELOP_VERSION */

// #define MEMLEAK_DEBUG

// #define MQTT_DEBUG
// #define FS_DEBUG
// #define SPIFFS_DEBUG

// #define HTTP_DEBUG
// #define HTTP_CGI_DEBUG
// #define HTTP_HELPER_DEBUG
// #define HTTP_PROC_DEBUG
// #define HTTP_QUERY_DBG

// #define CGI_DEBUG
// #define CGI_CONS_DEBUG
// #define CGI_MENU_DEBUG
// #define CGI_RFM_DEBUG
// #define CGI_SPIFFS_DEBUG
// #define CGI_WIFI_DEBUG

// #define STATUS_DEBUG
// #define WS_DEBUG


// #ifdef MQTT_ERROR
// #define MQTT_ERR(a)           c_printf("MQTT ERR# %d\n", a)
// #else
// #define MQTT_ERR
// #endif


#ifdef NODE_ERROR
#define NODE_ERR              c_printf
#else
#define NODE_ERR
#endif	/* NODE_ERROR */

#ifdef NODE_DEBUG
#define NODE_DBG              c_printf
#else
#define NODE_DBG
#endif	/* NODE_DEBUG */

#ifdef FS_DEBUG
#define FS_DBG                c_printf
#else
#define FS_DBG
#endif

#ifdef PING_DEBUG
#define PING_DBG                c_printf
#else
#define PING_DBG
#endif /* PING_DEBUG */

#ifdef HTTP_CGI_DEBUG
#define HTTP_CGI_DBG          c_printf
#else
#define HTTP_CGI_DBG
#endif

#ifdef MQTT_DEBUG
#define MQTT_DBG              c_printf
#else
#define MQTT_DBG
#endif

#ifdef SMART_DEBUG
#define SMART_DBG             c_printf
#else
#define SMART_DBG
#endif	/* Smart */

#ifdef HTTP_DEBUG
#define HTTP_DBG              c_printf
#else
#define HTTP_DBG
#endif	/* HTTP general */

#ifdef CGI_DEBUG
#define CGI_DBG               c_printf
#else
#define CGI_DBG
#endif	/* cgi.c debug */

#ifdef CGI_CONS_DEBUG
#define CGI_CONS_DBG          c_printf
#else
#define CGI_CONS_DBG
#endif	/* cgi_console.c debug */

#ifdef CGI_MENU_DEBUG
#define CGI_MENU_DBG          c_printf
#else
#define CGI_MENU_DBG
#endif	/* cgi_console.c debug */

#ifdef CGI_RFM_DEBUG
#define CGI_RFM_DBG           c_printf
#else
#define CGI_RFM_DBG
#endif	/* cgi_rfm69.c debug */

#ifdef CGI_SPIFFS_DEBUG
#define CGI_FS_DBG        c_printf
#else
#define CGI_FS_DBG
#endif

#ifdef CGI_WIFI_DEBUG
#define CGI_WIFI_DBG          c_printf
#else
#define CGI_WIFI_DBG
#endif	/* cgi_wifi.c debug */

#ifdef DNS_DBG_DEBUG
#define DNS_DBG               c_printf
#else
#define DNS_DBG
#endif	/* dns.c debug */

#ifdef RFM_DEBUG
#define RFM_DBG               os_printf
#else
#define RFM_DBG
#endif	/* RFM69 stuff */

#ifdef STATUS_DEBUG
#define STATUS_DBG            c_printf
#else
#define STATUS_DBG
#endif

#ifdef WS_DEBUG
#define WS_DBG                c_printf
#else
#define WS_DBG
#endif	/* Websockets */


#ifdef HTTP_HELPER_DEBUG
#define HTTP_HELPER_DBG       c_printf
#else
#define HTTP_HELPER_DBG
#endif

#ifdef HTTP_PROC_DEBUG
#define HTTP_PROC_DBG         c_printf
#else
#define HTTP_PROC_DBG
#endif

#ifdef HTTP_QUERY_DBG
#define HTTP_QUERY_DBG         c_printf
#else
#define HTTP_QUERY_DBG
#endif

#define ICACHE_STORE_TYPEDEF_ATTR   __attribute__((aligned(4),packed))
#define ICACHE_STORE_ATTR           __attribute__((aligned(4)))
#define ICACHE_RAM_ATTR             __attribute__((section(".iram0.text")))

#define DEBUG_UART 0 //debug on uart 0 or 1

#define BUILD_SPIFFS	1

// Byte 107 of esp_init_data_default, only one of these 3 can be picked
// #define ESP_INIT_DATA_ENABLE_READVDD33
// #define ESP_INIT_DATA_ENABLE_READADC
// #define ESP_INIT_DATA_FIXED_VDD33_VALUE 33



// #define CLIENT_SSL_ENABLE
// #define MD2_ENABLE
// #define SHA2_ENABLE

#endif	/* __USER_CONFIG_H__ */
