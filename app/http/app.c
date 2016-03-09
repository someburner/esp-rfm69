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
#include "c_types.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"

#include "cgi.h"
#include "cgi_menu.h"
#include "cgi_wifi.h"
#include "cgi_console.h"
#include "cgi_fs.h"
#include "cgi_rfm69.h"
#include "user_config.h"
#include "http_server.h"
#include "ws_app.h"

#define HTTP_PORT 80

static http_server_url api_urls[]=
{//-------URL----------------------CGI------------- ARGUMENT-------METHOD-------------FLAGS-----
	{"/menu",               http_menu_api_get,			NULL,      HTTP_GET,				NEED_BODY},
	{"/console/clear", 		http_console_api_clear,		NULL,      HTTP_POST,			NO_FLAG},
	{"/console", 				http_console_api, 			NULL,      HTTP_POST,      	NEED_BODY},
	{"/fs*",						http_fs_api,					NULL,      HTTP_ANY_METHOD,  	NO_FLAG},
	{"/rfm69/resetvals",		http_rfm69_api_resetvals,	NULL,      HTTP_POST,  			NO_FLAG},
	{"/rfm69/status",			http_rfm69_api_status,		NULL,      HTTP_POST,  			NO_FLAG},
	{"/rfm69/update",			http_rfm69_api_update,		NULL,      HTTP_POST,  			NO_FLAG},
	{"/wifi/connect",		   http_wifi_api_connect_ap, 	NULL,		  HTTP_POST,			NEED_BODY},
	{"/wifi/dc",		   	http_wifi_api_disconnect, 	NULL,		  HTTP_POST,			NO_FLAG},
	{"/wifi/info",	       	http_wifi_api_get_info,		NULL,		  HTTP_POST,			NEED_BODY},
	{"/wifi/status",			http_wifi_api_get_status,	NULL,		  HTTP_POST,			NO_FLAG},
	{"/wifi/scan",		   	http_wifi_api_scan, 			NULL,		  HTTP_POST,			NO_FLAG},
	{NULL,						NULL,		 						NULL,		  HTTP_ANY_METHOD,	NO_FLAG},
};

static url_rewrite rewrites[]=
{//----PATH---------REWRITE-------
	{"/"			,"/index.html"},
	{NULL			,NULL}
};

void init_http_server()
{
	//general max tcp conns
	espconn_tcp_set_max_con(20);

	http_server_init();
	http_server_bind_domain(INTERFACE_DOMAIN);
	http_server_enable_captive_portal();
	http_server_enable_cors();
	http_server_rewrite(rewrites);
	http_server_bind_urls((http_server_url *)&api_urls);
	http_server_start();

	//ws
	init_ws_server();
}
