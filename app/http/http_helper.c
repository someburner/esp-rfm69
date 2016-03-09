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
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "user_config.h"
#include "espconn.h"

#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"

#include "json/cJSON.h"

//Struct to keep extension->mime data in
typedef struct {
	const char *ext;
	const char *mimetype;
} MimeMap;

//If you need an extra mime type, add it here.
static const MimeMap mimeTypes[]=
{
	{"htm", "text/htm"},
	{"html", "text/html"},
	{"css", "text/css"},
	{"js", "text/javascript"},
	{"txt", "text/plain"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"json","application/json"},
	{"svg","image/svg+xml"},
	{"ico", "image/x-icon"},
	{NULL, "text/html"}, //default value
};

int http_reset_buffer(http_connection *c)
{
	//reset buffer
	memset(c->output.buffer,0,HTTP_BUFFER_SIZE);
	c->output.bufferPos = c->output.buffer;
	return 1;
}

int http_write_json(http_connection *c,cJSON *root)
{
	char * json_string;
	json_string = cJSON_Print(root);

	int ret = http_write(c,json_string);
	os_free(json_string);
	return ret;
}

int http_write_json_end_headers(http_connection *c,cJSON *root)
{
	char * json_string;
	json_string = cJSON_PrintUnformatted(root);

	//retrieve payload len, format as string
	int payloadLen = strlen(json_string);

	//finish headers
	http_SET_HEADER(c, HTTP_CONTENT_TYPE, JSON_CONTENT_TYPE);
	http_SET_CONTENT_LENGTH(c, payloadLen);
	http_end_headers(c);

	int ret = http_nwrite(c,json_string, payloadLen);
	os_free(json_string);
	return ret;
}

int http_end_line(http_connection *c)
{
	return http_write(c,"\r\n");
}

int http_SET_HEADER(http_connection *c,const char * header,const char * value)
{
	HTTP_HELPER_DBG("Setting header: %s : %s\n",header,value);

	int j=0;
	while(c->output.headers[j].key!=NULL && j< MAX_HEADERS)
	{
		if(strcmp(c->output.headers[j].key,header)==0)
		{
			//header already on the list, overwrite
			if(c->output.headers[j].value!=NULL)
			{
				os_free(c->output.headers[j].value);
			}
			break;
		}
		j++;
	}
	if(j==MAX_HEADERS) return 0;

	c->output.headers[j].key=header;
	c->output.headers[j].value=(char *) os_malloc(strlen(value)+1);

	if(c->output.headers[j].value==NULL)
	{
		HTTP_HELPER_DBG("Failed to alloc header memory\n");
		return 0;
	}
	strcpy(c->output.headers[j].value,value);
	return 1;
}

int http_SET_CONTENT_LENGTH(http_connection *c,int len)
{
	char buff[10];
	os_sprintf(buff,"%d",len);
	return http_SET_HEADER(c,HTTP_CONTENT_LENGTH,buff);
}

int http_HEADER(http_connection *c,const char *header,const char *value)
{
	HTTP_HELPER_DBG("Writing  header: %s : %s\n",header,value);
	return http_write(c,header)
	&& http_write(c,": ")
	&& http_write(c,value)
	&& http_end_line(c);
}

int http_end_headers(http_connection *c)
{
	//write cached headers
	int j=0;
	while(j< MAX_HEADERS)
	{
		if(c->output.headers[j].key!=NULL)
		{
			//write header
			http_HEADER(c,c->output.headers[j].key,c->output.headers[j].value);

			//free header
			os_free(c->output.headers[j].value);
			c->output.headers[j].value=NULL;
			c->output.headers[j].key=NULL;
		}
		j++;
	}

	//write final empty line
	return http_end_line(c);
}

//REQUEST
int http_request_start(http_connection *c,const char *method,const char *path)
{
	http_reset_buffer(c);

	int ret = http_write(c,method)
	&& http_write(c," ")
	&& http_write(c,path)
	&& http_write(c," HTTP/1.0")
	&& http_end_line(c)
	&& http_HEADER(c,HTTP_CONNECTION,"Close");
}

//REQUEST2
int http_request_start_post(http_connection *c,const char *method,const char *path)
{
	http_reset_buffer(c);

	int ret = http_write(c,method)
	&& http_write(c," ")
	&& http_write(c,path)
	&& http_write(c," HTTP/1.1")
	&& http_end_line(c)
	&& http_HEADER(c,HTTP_CONNECTION,"keep-alive");
}

int http_request_GET(http_connection *c,const char *path)
{
	return http_request_start(c,"GET",path)
	&& http_end_headers(c);
}

int http_request_POST(http_connection *c,const char *path)
{
	return http_request_start_post(c,"POST",path);
}

//RESPONSE
int http_response_start(http_connection *c)
{
	return http_write(c,"HTTP/")
	&& http_write(c,HTTP_VERSION)
	&& http_write(c," ");
}

int http_response_STATUS(http_connection *c,const char * status)
{
	return
	http_response_start(c) &&
	http_write(c,status) &&
	http_end_line(c) &&
	//default headers
	http_HEADER(c,HTTP_SERVER,HTTP_DEFAULT_SERVER) &&
	http_HEADER(c,HTTP_CONNECTION,"Close");
}

int http_response_OK(http_connection *c)
{
	return http_response_STATUS(c,HTTP_OK) &&
	http_end_headers(c);
}

int http_response_BAD_REQUEST(http_connection *c)
{
	return http_response_STATUS(c,HTTP_BAD_REQUEST) &&
	http_end_headers(c);
}

int http_response_NOT_FOUND(http_connection *c)
{
	return
	http_response_STATUS(c,HTTP_NOT_FOUND)&&
	http_end_headers(c);
}

int http_response_NOT_MODIFIED(http_connection *c)
{
	return
	http_response_STATUS(c,HTTP_NOT_MODIFIED)&&
	http_end_headers(c);
}

int http_response_REDIRECT(http_connection *c,const char* destination)
{
	return
	http_response_STATUS(c,HTTP_REDIRECT) &&
	http_HEADER(c,HTTP_LOCATION,destination) &&
	http_end_headers(c);
}

//ws
int http_websocket_HANDSHAKE(http_connection *c)
{
	http_reset_buffer(c);

	int ret = http_write(c,"HTTP/1.1 101 Switching Protocols")
	&& http_end_line(c)
	&& http_end_headers(c);
	return ret;
}

//Returns a static char* to a mime type for a given url to a file.
const char *http_get_mime(char *url)
{
	int i=0;
	//Go find the extension
	char *ext=url+(strlen(url)-1);
	while (ext!=url && *ext!='.') ext--;
	if (*ext=='.') ext++;

	//ToDo: os_strcmp is case sensitive; we may want to do case-intensive matching here...
	while (mimeTypes[i].ext!=NULL && strcmp(ext, mimeTypes[i].ext)!=0) i++;
	return mimeTypes[i].mimetype;
}
