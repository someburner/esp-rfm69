/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> and Jeroen Domburg <jeroen@spritesmods.com>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If we meet some day, and you think this stuff is
 * worth it, you can buy us a beer in return.
 * ----------------------------------------------------------------------------
 */
#include "user_interface.h"
#include "json/cJSON.h"
#include "osapi.h"
#include "sntp.h"
#include "mem.h"
#include "espconn.h"

#include "cgi.h"
#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"
#include "http_client.h"

#include "user_config.h"

//WiFi access point data
typedef struct {
	char ssid[32];
	sint8 rssi;
	char enc;
	char channel;
} ap;

//Scan result
typedef struct {
	ap **ap;
	int ap_count;
} scan_result_data;

typedef struct {
	uint8_t scanning;
	uint8_t connecting;
	scan_result_data scan_result;
	uint8_t mode;
	uint8_t station_status;
	struct station_config station_config;

} wifi_status_t;

typedef struct {
	uint8_t state;
	ETSTimer timer;
} api_cgi_status;

typedef struct {
	uint8_t state;
	ETSTimer timer;
	char ssid[32];
	char pwd[64];
} api_cgi_connect_status;

typedef struct {
	uint8_t state;
	http_connection *http_client;
} api_cgi_check_internet_status;

#ifdef CHANGE_TO_STA
#define MODECHANGE "yes"
#else
#define MODECHANGE "no"
#endif

static char *wifiMode[] = { 0, "STA", "AP", "AP+STA" };
static char *wifiPhy[]  = { 0, "11b", "11g", "11n" };

static char *connStatuses[] =
{
	"idle", "connecting", "wrong password", "AP not found", "failed", "got IP address"
};

static char *wifiWarn[] =
{ 	0,
	"Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(3)\\\">STA+AP mode</a>",
	"<b>Can't scan in this mode!</b> Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(3)\\\">STA+AP mode</a>",
	"Switch to <a href=\\\"#\\\" onclick=\\\"changeWifiMode(1)\\\">STA mode</a>",
};

static wifi_status_t wifi_status;

struct station_config *wifi_st_config;

int http_wifi_api_get_info(http_connection *c)
{
	CGI_WIFI_DBG("http_wifi_api_get_info\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	api_cgi_status * status = c->cgi.data;

	if(status==NULL){ //first call, send headers

		status = (api_cgi_status*)os_malloc(sizeof(api_cgi_status));
		status->state=1;
		c->cgi.data=status;

		http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
		http_response_OK(c);

		return HTTPD_CGI_MORE;
	}
	else if(status->state==1)
	{
		//json data
		char mac[20];
		wifi_station_get_config(&wifi_status.station_config);
		char *wifistatus = "unknown";
		uint8_t c_status = wifi_station_get_connect_status();
		if (c_status >= 0 && c_status < sizeof(connStatuses)) wifistatus = connStatuses[c_status];
		int p = wifi_get_phy_mode();
		char *phy = wifiPhy[p&3];
		sint8 rssi = wifi_station_get_rssi();
		if (rssi > 0) rssi = 0;
		uint8_t op = wifi_get_opmode() & 0x3;
		char *warn = wifiWarn[op];
		uint8 mac_addr[6];
		wifi_get_macaddr(0, mac_addr);
		uint8_t chan = wifi_get_channel();

		char *hostname = wifi_station_get_hostname();
		os_sprintf(mac,"%02x:%02x:%02x:%02x:%02x:%02x", mac_addr[0], mac_addr[1],
			mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

		cJSON *root = cJSON_CreateObject();
		cJSON_AddNumberToObject(root,"mode",wifi_get_opmode());
		cJSON_AddStringToObject(root,"modechange", MODECHANGE);
		cJSON_AddStringToObject(root,"ssid", (const char *)wifi_status.station_config.ssid);
		cJSON_AddStringToObject(root,"status", (const char *)wifistatus);
		cJSON_AddStringToObject(root,"phy", (const char *)phy);
		cJSON_AddNumberToObject(root,"rssi", rssi);
		cJSON_AddStringToObject(root,"warn", (const char *)warn);
		cJSON_AddStringToObject(root,"mac", (const char *)mac);
		cJSON_AddNumberToObject(root,"chan", chan);
		cJSON_AddStringToObject(root,"hostname", (const char *)hostname);
		cJSON_AddStringToObject(root,"domain", (const char *)INTERFACE_DOMAIN);

		// got ip
		if(c_status==5)
		{
			struct ip_info ip;
			wifi_get_ip_info(0x0,&ip);
			char *ip_str = (char*)ipaddr_ntoa(&ip.ip);
			cJSON_AddStringToObject(root,"ip",ip_str);
		}
		else { cJSON_AddStringToObject(root,"ip",""); }

		http_write_json(c,root);
		cJSON_Delete(root);

		status->state=99;
		return HTTPD_CGI_MORE;
	}
	else
	{
		os_free(c->cgi.data);
		return HTTPD_CGI_DONE;
	}
}

int http_wifi_api_get_status(http_connection *c)
{
	CGI_WIFI_DBG("wifi get_status\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	api_cgi_status * status = c->cgi.data;

	//first call, send headers
	if(status==NULL)
	{
		status = (api_cgi_status*)os_malloc(sizeof(api_cgi_status));
		status->state=1;
		c->cgi.data=status;

		http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
		http_response_OK(c);

		return HTTPD_CGI_MORE;
	}
	else if(status->state==1)
	{
		wifi_station_get_config(&wifi_status.station_config);
		uint8_t c_status = wifi_station_get_connect_status();

		//return JSON
		cJSON *root = cJSON_CreateObject();
		cJSON_AddBoolToObject(root,"scanning",wifi_status.scanning);
		cJSON_AddStringToObject(root,"ssid",(const char *)wifi_status.station_config.ssid);
		cJSON_AddNumberToObject(root,"mode",wifi_get_opmode());
		cJSON_AddNumberToObject(root,"station_status",c_status);

		//got ip
		if(c_status==5)
		{
			struct ip_info ip;
			wifi_get_ip_info(0x0,&ip);
			char *ip_str = (char*)ipaddr_ntoa(&ip.ip);
			cJSON_AddStringToObject(root,"ip",ip_str);
		}
		else { cJSON_AddStringToObject(root,"ip",""); }

		http_write_json(c,root);
		cJSON_Delete(root);

		status->state=99;
		return HTTPD_CGI_MORE;
	}
	else
	{
		os_free(c->cgi.data);
		return HTTPD_CGI_DONE;
	}
}

static void http_wifi_api_scan_callback(void *arg, STATUS status)
{
	int n;
	struct bss_info *bss_link = (struct bss_info *)arg;
	CGI_WIFI_DBG("Wifi Scan Done, status: %d\n", status);
	if (status!=OK)
	{
		wifi_status.scanning=0;
		return;
	}

	//Clear prev ap data if needed.
	if (wifi_status.scan_result.ap!=NULL)
	{
		for (n=0; n<wifi_status.scan_result.ap_count; n++)
		{
			os_free(wifi_status.scan_result.ap[n]);
		}
		os_free(wifi_status.scan_result.ap);
	}

	//Count amount of access points found.
	n=0;
	while (bss_link != NULL)
	{
		bss_link = bss_link->next.stqe_next;
		n++;
	}

	//Allocate memory for access point data
	wifi_status.scan_result.ap=(ap **)os_malloc(sizeof(ap *)*n);
	wifi_status.scan_result.ap_count=n;
	CGI_WIFI_DBG("Scan done: found %d APs\n", n);

	//Copy access point data to the static struct
	n=0;
	bss_link = (struct bss_info *)arg;
	while (bss_link != NULL)
	{
		if (n>=wifi_status.scan_result.ap_count)
		{
			//This means the bss_link changed under our nose. Shouldn't happen!
			//Break because otherwise we will write in unallocated memory.
			CGI_WIFI_DBG("Huh? I have more than the allocated %d aps!\n", wifi_status.scan_result.ap_count);
			break;
		}
		//Save the ap data.
		if(strlen(bss_link->ssid)>0)
		{
			wifi_status.scan_result.ap[n]=(ap *)os_malloc(sizeof(ap));
			wifi_status.scan_result.ap[n]->rssi=bss_link->rssi;
			wifi_status.scan_result.ap[n]->enc=bss_link->authmode;
			wifi_status.scan_result.ap[n]->channel=bss_link->channel;
			strncpy(wifi_status.scan_result.ap[n]->ssid, (char*)bss_link->ssid, 32);
			n++;
		}
		else { wifi_status.scan_result.ap_count--; }

		bss_link = bss_link->next.stqe_next;
	}

	//We're done.
	wifi_status.scanning=0;
}

typedef struct {
	uint8_t state;
	ETSTimer timer;
	int ap_index;
} api_cgi_scan_status;

int http_wifi_api_scan(http_connection *c)
{
	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	api_cgi_scan_status * status = c->cgi.data;
	//first call, create status
	if(status==NULL)
	{
		//create status
		status = (api_cgi_scan_status*)os_malloc(sizeof(api_cgi_scan_status));
		status->state=1;
		status->ap_index=0;
		c->cgi.data=status;

		//if not already scanning, request scan
		if(!wifi_status.scanning)
		{
			// CGI_WIFI_DBG("Starting scan\n");
			wifi_station_scan(NULL,http_wifi_api_scan_callback);
			wifi_status.scanning=1;
		}

		//write headers
		http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
		http_response_OK(c);

		//set state to 1 - waiting
		status->state=1;

		return HTTPD_CGI_MORE;
	}
	else
	{
		if (wifi_status.scanning)
		{
			// CGI_WIFI_DBG("Waiting scan done\n");

			//set timer to check again
			os_timer_disarm(&status->timer);
			os_timer_setfn(&status->timer, (os_timer_func_t *)http_execute_cgi, c);
			os_timer_arm(&status->timer, 500, 0);

			return HTTPD_CGI_MORE;
		}
		else if (status->state==1)
		{
			//clear timer
			os_timer_disarm(&status->timer);

			CGI_WIFI_DBG("Scan complete %d\n",status->ap_index);

			//create json
			cJSON *root = cJSON_CreateObject();
			cJSON_AddNumberToObject(root,"ap_count",wifi_status.scan_result.ap_count);

			cJSON * array;
			cJSON * item;
			cJSON_AddItemToObject(root, "ap", array = cJSON_CreateArray());

			int i;
			for(i=0;i< wifi_status.scan_result.ap_count;i++)
			{
				cJSON_AddItemToArray(array,item=cJSON_CreateObject());
				cJSON_AddStringToObject(item,"ssid",(const char *)wifi_status.scan_result.ap[i]->ssid);
				cJSON_AddNumberToObject(item,"rssi",wifi_status.scan_result.ap[i]->rssi);
				cJSON_AddNumberToObject(item,"enc",wifi_status.scan_result.ap[i]->enc);
				cJSON_AddNumberToObject(item,"channel",wifi_status.scan_result.ap[i]->channel);
			}

			http_write_json(c,root);

			//delete json struct
			cJSON_Delete(root);

			status->state=99;
			return HTTPD_CGI_MORE;
		}
		else  //free resources
		{
			CGI_WIFI_DBG("Freeing alloced memory\n");
			os_free(c->cgi.data);

			return HTTPD_CGI_DONE;
		}
	}
}

int http_wifi_api_disconnect(http_connection *c)
{
	CGI_WIFI_DBG("http_wifi_disconnect\n");

	//wait for whole body
	if (c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	//reset wifi cfg
	strcpy(wifi_status.station_config.ssid,"");
	strcpy(wifi_status.station_config.password,"");
	wifi_status.station_config.bssid_set=0;

	wifi_station_set_config(&wifi_status.station_config);
	wifi_station_disconnect();
	wifi_station_set_auto_connect(false);

	http_response_OK(c);
	return HTTPD_CGI_DONE;
}

int http_wifi_api_connect_ap(http_connection *c)
{
	CGI_WIFI_DBG("http_wifi_api_connect_ap\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	api_cgi_connect_status * status = c->cgi.data;

	if(status==NULL)
	{
		// CGI_WIFI_DBG("http_wifi_api_connect_ap status NULL\n");

		status = (api_cgi_connect_status*)os_malloc(sizeof(api_cgi_connect_status));
		status->state=1;
		c->cgi.data=status;

		//parse json and validate
		cJSON * root = cJSON_Parse(c->body.data);
		if(root==NULL) goto badrequest;

		cJSON * ssid = cJSON_GetObjectItem(root,"ssid");
		if(ssid==NULL) goto badrequest;
		else if(ssid->type != cJSON_String) goto badrequest;

		cJSON * pwd = cJSON_GetObjectItem(root,"pwd");
		if(pwd==NULL) goto badrequest;
		else if(pwd->type!=cJSON_String) goto badrequest;

		//parse ok
		strncpy(status->ssid,ssid->valuestring,32);
		strncpy(status->pwd,pwd->valuestring,64);

		//set timer to connect
		os_timer_disarm(&status->timer);
		os_timer_setfn(&status->timer, (os_timer_func_t *)http_execute_cgi, c);
		os_timer_arm(&status->timer, 10, 0);

		return HTTPD_CGI_MORE;
	}
	else if (status->state==1)
	{
		CGI_WIFI_DBG("connect_ap status %d\n",status->state);
		//try connect

		if(strlen(status->ssid)>32 || strlen(status->pwd)>64)
			goto badrequest;

		CGI_WIFI_DBG("ap connect ssid: %s, pwd: %s\n",status->ssid,status->pwd);

		strcpy(wifi_status.station_config.ssid,status->ssid);
		strcpy(wifi_status.station_config.password,status->pwd);
		wifi_status.station_config.bssid_set=0;

		wifi_station_disconnect();
		wifi_station_set_config(&wifi_status.station_config);
		wifi_station_connect();

		//set timer to check status
		os_timer_disarm(&status->timer);
		os_timer_setfn(&status->timer, (os_timer_func_t *)http_execute_cgi, c);
		os_timer_arm(&status->timer, 500, 0);

		status->state=2;

		return HTTPD_CGI_MORE;
	}
	else if (status->state==2)
	{
		CGI_WIFI_DBG("connect_ap status %d\n",status->state);
		uint8_t c_status = wifi_station_get_connect_status();

		CGI_WIFI_DBG("wifi sta status %d\n",c_status);
		if (c_status>=2 && c_status <= 4 )
		{
			wifi_station_disconnect();
			strcpy(wifi_status.station_config.ssid,"");
			strcpy(wifi_status.station_config.password,"");
			wifi_station_set_config(&wifi_status.station_config);
		}

		if (c_status==1)
		{
			//set timer to check status
			os_timer_disarm(&status->timer);
			os_timer_setfn(&status->timer, (os_timer_func_t *)http_execute_cgi, c);
			os_timer_arm(&status->timer, 500, 0);
			return HTTPD_CGI_MORE;
		}
		else
		{
			//write headers
			http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
			http_response_OK(c);

			//create json
			cJSON *root = cJSON_CreateObject();
			cJSON_AddNumberToObject(root,"status",c_status);

			//got ip
			if(c_status==5)
			{
				struct ip_info ip;
				wifi_get_ip_info(0x0,&ip);
				char *ip_str = (char*)ipaddr_ntoa(&ip.ip);
				cJSON_AddStringToObject(root,"ip",ip_str);
			}
			else
			{
				cJSON_AddStringToObject(root,"ip","");
			}

			http_write_json(c,root);

			//delete json struct
			cJSON_Delete(root);

			status->state=99;

			return HTTPD_CGI_MORE;
		}
	}
	else //status=99
	{
		CGI_WIFI_DBG("connect_ap status %d\n",status->state);
		//clean
		os_free(c->cgi.data);
		return HTTPD_CGI_DONE;
	}

badrequest:
	http_response_BAD_REQUEST(c);
	status->state=99;
	return HTTPD_CGI_MORE;

	//shut up compiler
	return HTTPD_CGI_DONE;
}

int http_wifi_api_check_internet_cb(http_connection *c)
{
	CGI_WIFI_DBG("check_internet_cb state: %d\n",c->state);

	http_connection *request=c->reverse;

	if(request->espConnection==NULL)
	{
		//client request has been aborted
		return HTTP_CLIENT_CGI_DONE;
	}

	api_cgi_check_internet_status * status = (api_cgi_check_internet_status *)request->cgi.data;

	if (c->state==HTTP_CLIENT_DNS_NOT_FOUND)
	{
		status->state=3;
		http_execute_cgi(request);
		return HTTP_CLIENT_CGI_DONE;
	}

	//wait whole body
	if(c->state==HTTPD_STATE_BODY_END)
	{
		if (c->parser.status_code==200) { status->state=2; }
		else { status->state=3; }

		http_execute_cgi(request);
		return HTTP_CLIENT_CGI_DONE;
	}
	return HTTP_CLIENT_CGI_MORE;
}

int http_wifi_api_check_internet(http_connection *c)
{
	CGI_WIFI_DBG("check_internet\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE;}

	api_cgi_check_internet_status * status = (api_cgi_check_internet_status *)c->cgi.data;
	//first call, send headers
	if(status==NULL)
	{
		CGI_WIFI_DBG("check_internet begin\n");
		status = (api_cgi_check_internet_status*)os_malloc(sizeof(api_cgi_check_internet_status));
		status->state=1;
		c->cgi.data=status;

		http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
		http_response_OK(c);

		status->http_client = http_client_new(http_wifi_api_check_internet_cb);
		http_client_GET(status->http_client,"http://www.msftncsi.com/ncsi.txt");

		status->http_client->reverse = c; //mark reverse so we can find on callback
		c->reverse=&status->http_client; //reverse other way around

		return HTTPD_CGI_MORE;
	}
	else if (status->state==1)
	{
		// just signal we aren't finished
		CGI_WIFI_DBG("check_internet 1\n");
		return HTTPD_CGI_MORE;
	}
	else if(status->state==2)
	{
		//DNS FOUND
		CGI_WIFI_DBG("check_internet 2\n");

		status->state=99;

		//create json
		cJSON *root = cJSON_CreateObject();
		cJSON_AddNumberToObject(root,"status",1);

		http_write_json(c,root);
		//delete json struct
		cJSON_Delete(root);

		return HTTPD_CGI_MORE;
	}
	else if (status->state==3)
	{
		//DNS NOT FOUND
		status->state=99;

		//create json
		cJSON *root = cJSON_CreateObject();
		cJSON_AddNumberToObject(root,"status",0);

		http_write_json(c,root);
		//delete json struct
		cJSON_Delete(root);

		return HTTPD_CGI_MORE;
	}
	else
	{
		CGI_WIFI_DBG("check_internet done\n");
		os_free(c->cgi.data);
		return HTTPD_CGI_DONE;
	}
}
