/*
Menu and general index CGI routines
*/
#include "user_interface.h"
#include "osapi.h"
#include "mem.h"
#include "platform.h"
#include "user_config.h"

#include "cgi.h"
#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"
#include "http_client.h"
#include "json/cJSON.h"

#include "cgi_menu.h"

extern char *esp_rfm69_version; // in user_main.c

int http_menu_api_get(http_connection *c) {
   CGI_MENU_DBG("http_menu_api_get\n");
   //wait for whole body
	if(c->state <HTTPD_STATE_BODY_END)
      return HTTPD_CGI_MORE;

   // set cache to 1 hour
   http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
   http_SET_HEADER(c,HTTP_CACHE_CONTROL,HTTP_DEFAULT_CACHE);
   http_response_OK(c);

   //create json
   cJSON *root, *array;
   root = cJSON_CreateObject();
   cJSON_AddItemToObject(root, "menu", array = cJSON_CreateArray());
   cJSON_AddStringToObject(array,"Index","Index");
   cJSON_AddStringToObject(array,"Index","/index.html");
   cJSON_AddStringToObject(array,"Wifi","Wifi");
   cJSON_AddStringToObject(array,"Wifi","/wifi.html");
   cJSON_AddStringToObject(array,"Console","Console");
   cJSON_AddStringToObject(array,"Console","/console.html");
   cJSON_AddStringToObject(array,"RFM OTA","RFM OTA");
   cJSON_AddStringToObject(array,"RFM OTA","/fw.html");

    cJSON_AddStringToObject(root,"version", esp_rfm69_version);

    http_write_json(c,root);

    //delete json struct
    cJSON_Delete(root);

    return HTTPD_CGI_DONE;
}
