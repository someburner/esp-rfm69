/*
Rfm69 related cgi routines.
*/
#include "user_interface.h"
#include "osapi.h"
#include "mem.h"
#include "platform.h"
#include "json/cJSON.h"

#include "cgi.h"
#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"
#include "user_config.h"

#include "rfm/radiohandler.h"
#include "cgi_rfm69.h"

const char *radioStatuses[] = { "Unknown", "Disconnected", "Connected" };

extern RadioStatus radioStatus;
extern uint8_t radioState;
extern int txTotal;
extern int rxTotal;
extern uint32 lastFreeHeap;

int http_rfm69_api_resetvals(http_connection *c)
{
	CGI_RFM_DBG("http_rfm69_api_resetvals\n");
   radioStatus.batt = 0; //testing
   radioStatus.totalct = 0;
   txTotal = 0;
   rxTotal = 0;

	http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
	http_response_OK(c);

	return HTTPD_CGI_DONE;
}

int http_rfm69_api_status(http_connection *c)
{
	CGI_RFM_DBG("http_rfm69_api_status\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END)
		return HTTPD_CGI_MORE;

	http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
	http_response_OK(c);

	int thisNodeId = 0;
	thisNodeId = 1;

	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root,"id", thisNodeId);
	cJSON_AddStringToObject(root,"link", radioStatuses[radioState]);
	cJSON_AddNumberToObject(root,"freq", radioStatus.freq);

	http_write_json(c,root);
	cJSON_Delete(root);

	return HTTPD_CGI_DONE;
}

int http_rfm69_api_update(http_connection *c)
{
	CGI_RFM_DBG("http_rfm69_api_update\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END)
		return HTTPD_CGI_MORE;

	http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
	http_response_OK(c);

   radioStatus.batt = radioStatus.batt + 1; //testing
	cJSON *root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root,"rssi", radioStatus.rssi);
	cJSON_AddNumberToObject(root,"batt", radioStatus.batt);
	cJSON_AddNumberToObject(root,"ct", radioStatus.totalct);
   cJSON_AddNumberToObject(root,"tx", txTotal);
   cJSON_AddNumberToObject(root,"rx", rxTotal);
	if (lastFreeHeap != 0)
		cJSON_AddNumberToObject(root,"heap", lastFreeHeap);

	http_write_json(c,root);
	cJSON_Delete(root);

	return HTTPD_CGI_DONE;
}
