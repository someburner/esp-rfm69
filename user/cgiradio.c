/*
Cgi/template routines for the /radio url.
*/
#include "cgiradio.h"
#include "cgi.h"
#include "config.h"
#include "log.h"

// ===== radio status change callback
//set Moteino UART status to disconnected

uint8_t radioState = radioUnknown;

RadioStatus radioStatus = {
  0, //freq
  0, //rssi
  0, //voltage
  110, //axis
  0, //totalPeaks
};

void (*radioStatusCb)(uint8_t); // callback when radio status changes

// reset timer changes back to STA+AP if we can't associate
#define RESET_TIMEOUT (15000) // 15 seconds

// Change special settings
int ICACHE_FLASH_ATTR cgiRadioSpecial(HttpdConnData *connData) {
	char dhcp[8];
	char hostname[32];
	char staticip[20];
	char netmask[20];
	char gateway[20];

	if (connData->conn==NULL) return HTTPD_CGI_DONE;

	// get args and their string lengths
	int dl = httpdFindArg(connData->getArgs, "dhcp", dhcp, sizeof(dhcp));
	int hl = httpdFindArg(connData->getArgs, "hostname", hostname, sizeof(hostname));
	int sl = httpdFindArg(connData->getArgs, "staticip", staticip, sizeof(staticip));
	int nl = httpdFindArg(connData->getArgs, "netmask", netmask, sizeof(netmask));
	int gl = httpdFindArg(connData->getArgs, "gateway", gateway, sizeof(gateway));

	if (!(dl > 0 && hl >= 0 && sl >= 0 && nl >= 0 && gl >= 0)) {
		jsonHeader(connData, 400);
		httpdSend(connData, "Request is missing fields", -1);
		return HTTPD_CGI_DONE;
	}

	char url[64]; // redirect URL
	jsonHeader(connData, 200);
	httpdSend(connData, url, -1);
	return HTTPD_CGI_DONE;
}

static char *radioStatuses[] = {"Unknown", "Disconnected", "Connected"};

// static char *radioWarn[] = { 0,
// 	"Switch to <a href=\\\"#\\\" onclick=\\\"changeradioMode(3)\\\">STA+AP mode</a>",
// 	"<b>Can't scan in this mode!</b> Switch to <a href=\\\"#\\\" onclick=\\\"changeradioMode(3)\\\">STA+AP mode</a>",
// 	"Switch to <a href=\\\"#\\\" onclick=\\\"changeradioMode(1)\\\">STA mode</a>",
// };

// print various radio radiormation into json buffer
int ICACHE_FLASH_ATTR printRadioStatus(char *buff) {
	int len;
  #ifdef CGI_DBG
	os_printf("doing printRadio... \n");
  #endif
	len = os_sprintf(buff,
		"\"link\": \"%s\", \"id\": \"%d\", \"freq\": \"%dMHz\", "
		"\"rssi\":\"%ddB\", \"batt\": \"%dV\", \"axis\": \"%d\", \"peaks\":%lu",
		radioStatuses[radioState], flashConfig.nodeID, radioStatus.freq,
    radioStatus.rssi, radioStatus.batt, radioStatus.axis, radioStatus.totalPeaks);
	return len;
}

int ICACHE_FLASH_ATTR cgiRadioConnStatus(HttpdConnData *connData) {
	char buff[1024];
	int len;

	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
	jsonHeader(connData, 200);

	len = os_sprintf(buff, "{");
	len += printRadioStatus(buff+len);
	len += os_sprintf(buff+len, ", ");

	// if (radioReason != 0) {
	// 	len += os_sprintf(buff+len, "\"reason\": \"%s\", ", radioGetReason());
	// }

#if 0
	// commented out 'cause often the client that requested the change can't get a request in to
	// find out that it succeeded. Better to just wait the std 15 seconds...
	// int st=radio_station_get_connect_status();
	// if (st == STATION_GOT_IP) {
	// 	if (radio_get_opmode() != 1) {
	// 		// Reset into AP-only mode sooner.
	// 	}
	}
#endif

	len += os_sprintf(buff+len, "\"x\":0}\n");

	os_printf("  -> %s\n", buff);

	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

// Cgi to return various radio radiormation
int ICACHE_FLASH_ATTR cgiRadioStatus(HttpdConnData *connData) {
	char buff[1024];

	if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.

	os_strcpy(buff, "{");
	printRadioStatus(buff+1);
	os_strcat(buff, "}");

	jsonHeader(connData, 200);
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

// Init the wireless, which consists of setting a timer if we expect to connect to an AP
// so we can revert to STA+AP mode if we can't connect.
void ICACHE_FLASH_ATTR cgiRadioInit() {
	// radio_set_phy_mode(2);
	os_printf("cgiRadio init, mode=%s\n", radioStatuses[radioState]);

	// radioInfo.status = 0;
	// radio_set_event_handler_cb(radioHandleEventCb);
	// check on the radio in a few seconds to see whether we need to switch mode
}
