/*******************************************************************************
*
* Net Utilities: Header
*
*******************************************************************************/
/*******************************************************************************
*Application-level interface for accessing various net-related utlities.
*******************************************************************************/
#ifndef NETUTIL_h
#define NETUTIL_h

#include "espconn.h"

typedef enum {
   PING_SEQ_INVALID,
   PING_SEQ_READY,
   PING_SEQ_STARTED,
   PING_SEQ_DONE,
} ping_state_t;

/* Struct for accessing ping response data */
typedef struct {
   uint8_t state;
   uint8_t success;
   uint8_t error;
   uint8_t expected;

   uint32_t avg_time;

} PING_RESP_T;

/*******************************************************************************
 * netutil_ping_ip_string:
 * Description: Pings an IP
 * Parameters:
 * - ip: String representation of IP Address
 * - appresp: Pointer to app-level ping response data to print out
 * Returns: PING_RESP_T - pointer to struct containing response info/metrics
*******************************************************************************/
void netutil_ping_ip_string(const char * ip, void * appresp);
void netutil_nslookup(const char *domain, dns_found_callback ns_cb);















#endif
