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
#include "c_stdio.h"
#include "osapi.h"
#include "mem.h"
#include "espconn.h"
#include "platform.h"
#include "json/cJSON.h"
#include "lwip/dns.h"

#include "http.h"
#include "http_client.h"
#include "http_parser.h"
#include "http_helper.h"
#include "http_process.h"
#include "user_config.h"

void http_client_request_execute(http_connection *c)
{
	HTTP_DBG("http_client_request_execute");

	//we should send request headers
	if (c->state==HTTP_CLIENT_CONNECT_OK)
	{
		char * path = http_url_get_path(c);
		char * query = http_url_get_query(c);

		char * requestPath;
		if(query!=NULL)
		{
			requestPath = (char *)os_zalloc(strlen(path)+strlen(query)+2);
			strcpy(requestPath,path);
			os_strcat(requestPath,"?");
			os_strcat(requestPath,query);
		}
		else
		{
			requestPath = (char *)os_zalloc(strlen(path)+1);
			strcpy(requestPath,path);
		}

		if (path==NULL) { path = "/"; }

		http_SET_HEADER(c,HTTP_HOST,http_url_get_host(c));
		http_SET_HEADER(c,HTTP_USER_AGENT,HTTP_DEFAULT_SERVER);

		if (c->method==HTTP_GET)
		{
			http_request_GET(c,requestPath);
			os_free(requestPath);
		}
		else if (c->method==HTTP_POST)
		{
			// http_SET_HEADER(c,HTTP_CONTENT_LENGTH,"42");
			NODE_DBG("c->method==HTTP_POST");
			http_request_POST(c,requestPath);
			os_free(requestPath);
		}
		else
		{
			return;
		}

		c->state=HTTP_CLIENT_REQUEST_HEADERS_SENT;
		http_execute_cgi(c);
		// http_transmit(c); // handled in app client.c
	}
	else if(c->state==HTTP_CLIENT_REQUEST_HEADERS_SENT)
	{
		NODE_DBG("c->state==HTTP_CLIENT_REQUEST_HEADERS_SENT");

		//we can now send the body
		if(c->request_body!=NULL)
		{
			NODE_DBG("c->request_body!=NULL");
			//body is already here, send
			int bodyLen = strlen(c->request_body);
			if(bodyLen>=HTTP_BUFFER_SIZE)
			{
				http_nwrite(c,c->request_body,HTTP_BUFFER_SIZE);
				c->request_body+=HTTP_BUFFER_SIZE;
			}
			else
			{
				http_write(c,c->request_body);
			}
			http_transmit(c);
		}
		else
		{
			//callback so body can be sent elsewhere
			http_execute_cgi(c);
		}
		c->state=HTTP_CLIENT_REQUEST_BODY_SENT;
	}
}

void http_client_dns_found_cb(const char *name, ip_addr_t *ipaddr, void *arg)
{
	HTTP_DBG("http_client_dns_callback conn=%p",arg);

	http_connection *c = (http_connection*)arg;

	if(c==NULL)
		return;

	os_timer_disarm(&c->timeout_timer);
	c->host_ip.addr=ipaddr->addr;

	if(name!=NULL && ipaddr!=NULL)
	{
		#ifdef DEVELOP_VERSION
		HTTP_DBG("http_client_dns_callback: %s %d.%d.%d.%d",
			name,
			ip4_addr1(&ipaddr->addr),
			ip4_addr2(&ipaddr->addr),
			ip4_addr3(&ipaddr->addr),
			ip4_addr4(&ipaddr->addr));
		#endif

		//set ip on tcp
		c->espConnection->proto.tcp->remote_ip[0]=ip4_addr1(&ipaddr->addr);
		c->espConnection->proto.tcp->remote_ip[1]=ip4_addr2(&ipaddr->addr);
		c->espConnection->proto.tcp->remote_ip[2]=ip4_addr3(&ipaddr->addr);
		c->espConnection->proto.tcp->remote_ip[3]=ip4_addr4(&ipaddr->addr);

		//DNS found
		c->state = HTTP_CLIENT_DNS_FOUND;
	}
	else
	{
		//DNS found
		c->state = HTTP_CLIENT_DNS_NOT_FOUND;
	}

	http_execute_cgi(c);
}

void http_client_dns_timeout(void *arg)
{
	HTTP_DBG("http_client_dns_timeout");
	http_connection *c = (http_connection*)arg;

	// put null so if dns query response arrives, it wont trigger
	// execute again
	c->espConnection->reverse=NULL;

	c->state = HTTP_CLIENT_DNS_NOT_FOUND;
	http_execute_cgi(c);
}

void http_client_dns(http_connection *c)
{
	char * host = http_url_get_host(c);
	HTTP_DBG("http_client_dns: %s",host);

	memset(&c->host_ip,0,sizeof(ip_addr_t));
	dns_gethostbyname(host, &c->host_ip, &http_client_dns_found_cb,(void*)c);

    os_timer_disarm(&c->timeout_timer);
    os_timer_setfn(&c->timeout_timer, (os_timer_func_t *)http_client_dns_timeout, c);
    os_timer_arm(&c->timeout_timer, 1000, 0);

}

void http_client_connect_callback(void *arg)
{
	struct espconn *conn=arg;

	HTTP_DBG("http_client_connect_callback: %d",conn->state==ESPCONN_CONNECT?1:0);

	http_connection *c = (http_connection *)conn->reverse;

	if (conn->state==ESPCONN_CONNECT)
		{ c->state=HTTP_CLIENT_CONNECT_OK; }
	else
		{ c->state=HTTP_CLIENT_CONNECT_FAIL; }

	http_execute_cgi(c);

}

int http_client_cgi_execute(http_connection *c)
{

	if(c->cgi.function==NULL)
		return 0;

	int ret = c->cgi.function(c);

	if(c->state==HTTP_CLIENT_DNS_NOT_FOUND || (ret== HTTP_CLIENT_CGI_DONE && c->state==HTTP_CLIENT_DNS_FOUND))
	{
		//this was a dns request only
		HTTP_DBG("Client conn %p done after DNS",c);
		http_process_free_connection(c);
		return 0;
	}
	else if (ret== HTTP_CLIENT_CGI_DONE)
	{
		HTTP_DBG("http_client: abort");
		//abort
		espconn_disconnect(c->espConnection);
		http_process_free_connection(c);
		return 0;
	}

	//connect
	if (c->state==HTTP_CLIENT_DNS_FOUND)
	{
		espconn_connect(c->espConnection);
	}

	//we are ready to make the request
	if (c->state==HTTP_CLIENT_CONNECT_OK)
	{
		http_client_request_execute(c);
	}

	//free
	if (c->state==HTTP_CLIENT_CONNECT_FAIL)
	{
		espconn_disconnect(c->espConnection);
		http_process_free_connection(c);
	}
}

http_connection * http_client_new(http_callback callback)
{
	http_connection * client = http_new_connection(0,NULL);

	if(client==NULL)
		return NULL;

	client->cgi.execute=http_client_cgi_execute;
	client->cgi.function = callback;

	espconn_regist_connectcb(client->espConnection, http_client_connect_callback);

	return client;
}

int http_client_open_url(http_connection *c,char *url)
{
	NODE_DBG("http_client_open_url: %s",url);

	strcpy(c->url,url);
	http_parse_url(c);

	//set port
	if(c->url_parsed.port>0)
		c->client_connection.proto.tcp->remote_port=c->url_parsed.port;
	else
		c->client_connection.proto.tcp->remote_port=80;

	http_client_dns(c);

	return 1;
}

int http_client_GET(http_connection *c,char *url)
{
	c->method=HTTP_GET;
	return http_client_open_url(c,url);
}

int http_client_POST(http_connection *c,char *url)
{
	c->method=HTTP_POST;
	return http_client_open_url(c,url);
}
