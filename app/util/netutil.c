/*******************************************************************************
*
* Net Utilities: Application-level interface for accessing various net-related
* utlities.
*
*******************************************************************************/
#include "user_interface.h"
#include "ping.h"
#include "mem.h"
#include "osapi.h"
#include "espconn.h"

#include "c_stdio.h"

#include "netutil.h"
#include "user_config.h"

// #define TEST_NSLOOKUP

static struct ping_option ping_opt;

static PING_RESP_T * app_pingresp = NULL;

static void netutil_ping_cb(void *opt, void *resp)
{
   struct ping_resp *  ping_resp = (struct ping_resp *)resp;
   struct ping_option * ping_opt  = (struct ping_option *)opt;

   if (app_pingresp == NULL)
   {
      NODE_ERR("app_pingresp NULL?\n");
      return;
   }

   if (ping_resp->ping_err == -1)
   {
      app_pingresp->error++;
      NODE_ERR("Error\n");
   } else {
      app_pingresp->success++;
      app_pingresp->avg_time += ping_resp->resp_time;
   }

   PING_DBG(
   "DEBUG: ping reply\n"
   "\ttotal_count = %d \n"
   "\tresp_time = %d \n"
   "\tseqno = %d \n"
   "\ttimeout_count = %d \n"
   "\tbytes = %d \n"
   "\ttotal_bytes = %d \n"
   "\ttotal_time = %d \n"
   "\tping_err = %d \n",
   ping_resp->total_count, ping_resp->resp_time, ping_resp->seqno,
   ping_resp->timeout_count, ping_resp->bytes, ping_resp->total_bytes,
   ping_resp->total_time, ping_resp->ping_err
   );

   PING_DBG("ping: success=%d, fail=%d, expect=%d\n", app_pingresp->success, app_pingresp->error, app_pingresp->expected);

   // Is it time to end?
   // Don't using seqno because it does not increase on error
   if (app_pingresp->success + app_pingresp->error == app_pingresp->expected)
   {
      app_pingresp->state = PING_SEQ_DONE;
      app_pingresp->avg_time = app_pingresp->avg_time / app_pingresp->expected;
   }
}

void netutil_ping_ip_string(const char * ip, void * response)
{
   /* Check for valid input */
   if (response == NULL) return;

   app_pingresp = (PING_RESP_T *)response;

   /* Return if already started, or done has not been set */
   if ((app_pingresp->state == PING_SEQ_STARTED) || (app_pingresp->state == PING_SEQ_DONE)) {
      return;
   }

   /* Check for valid IP */
   u32_t tempIp = ipaddr_addr(ip);
   if (tempIp == IPADDR_NONE)
   {
      NODE_ERR("Invalid IP\n");
      return;
   }

   /* Clear stats if complete */
   // if (app_pingresp->state == PING_SEQ_DONE) {}
   /* Clear Struct & Metrics */
   app_pingresp->success = app_pingresp->error = app_pingresp->expected = 0;

   os_memset(&ping_opt, 0, sizeof(struct ping_option));
   ping_opt.ip = tempIp;
   ping_opt.count = 5;
   ping_opt.coarse_time = 1;


   /* Attach Callbacks */
   ping_opt.recv_function = netutil_ping_cb;
   ping_opt.sent_function = NULL;

   if (ping_start(&ping_opt)) {
      app_pingresp->state = PING_SEQ_STARTED;
      app_pingresp->expected = 5;
      PING_DBG("Ping Start: IP=%s\n", ip);
   } else {
      app_pingresp->state = PING_SEQ_READY;
   }

}

static void nslookup_cb(const char *name, ip_addr_t *ip, void *arg) {
	char ipstr[17];
	if (ip == NULL) {
      NODE_DBG("FAILED");
	} else {
      ipaddr_ntoa_r(ip, ipstr, sizeof(ipstr));
      NODE_DBG("Host: %s -> ip: %s\n", name, ipstr);
	}
}

void netutil_nslookup(const char *domain, dns_found_callback ns_cb)
{
	static ip_addr_t ip;
	static struct espconn connector = {0};
   #ifdef TEST_NSLOOKUP
	err_t r = espconn_gethostbyname(&connector, domain, &ip, nslookup_cb);
   #else
   err_t r = espconn_gethostbyname(&connector, domain, &ip, ns_cb);
   #endif
	NODE_DBG("Resolving %s...\n", domain);
	if(r == ESPCONN_OK) {
      nslookup_cb(domain, &ip, &connector);
	} else if(r != ESPCONN_INPROGRESS) {
      NODE_DBG("dns in progress\n");
	}
}










/* End netutil.c */
