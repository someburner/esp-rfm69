#ifndef __HTTP_WS_SERVER_H
#define __HTTP_WS_SERVER_H


typedef struct {

	//Listening connection data
	struct espconn server_conn;
	esp_tcp server_tcp;

	const char * host_domain;
	int port;


} http_ws_config;

void http_ws_push_bin(http_connection *c,char *msg,size_t msgLen);
void http_ws_push_text(http_connection *c,char *msg,size_t msgLen);
void http_ws_server_init();
void http_ws_server_start();

#endif
