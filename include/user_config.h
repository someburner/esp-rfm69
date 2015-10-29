#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_
#include <c_types.h>
#ifdef __WIN32__
#include <_mingw.h>
#endif

#undef SHOW_HEAP_USE
#define DEBUGIP
#define SDK_DBG

// #define TCP_DBG
#undef ESPFS_DBG
#undef CGI_DBG
#define CGIFLASH_DBG
#define CGIPINS_DBG
#define CGIWIFI_DBG
#define CONFIG_DBG
#define LOG_DBG
#define STATUS_DBG
//#define SHOW_HEAP_USE
//#define SOCK_DBG
//#define RFM_DBG
//#define HTTPD_DBG
#undef PKTBUF_DBG
//#define UART_DBG

// If defined, the default hostname for DHCP will include the chip ID to make it unique
#undef CHIP_IN_HOSTNAME

extern char* esp_rfm_version;
extern uint8_t UTILS_StrToIP(const char* str, void *ip);

#endif
