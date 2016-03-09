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
#include "c_stdio.h"

#include "cgi.h"
#include "cgi_fs.h"
#include "http_parser.h"
#include "http_process.h"
#include "http_server.h"
#include "http_helper.h"
#include "user_config.h"

static http_connection connection_poll[MAX_CONNECTIONS];

void http_execute_cgi(http_connection *conn)
{
	if (conn->espConnection==NULL) { return; }

	if (conn->cgi.execute==NULL) { return; }

	conn->cgi.execute(conn);
}

int http_transmit(http_connection *c)
{
	HTTP_PROC_DBG("Transmit Buffer\n");

	int len = (c->output.bufferPos - c->output.buffer);
	if (len>0 && len <= HTTP_BUFFER_SIZE)
	{
		espconn_send(c->espConnection, (uint8_t*)(c->output.buffer),len);
	}
	else
	{
		HTTP_PROC_DBG("Wrong transmit size %d\n",len);
	}

	//free buffer
	http_reset_buffer(c);

	return len;
}

int http_nwrite(http_connection *c,const char * message,size_t size)
{
	int rem = c->output.buffer + HTTP_BUFFER_SIZE - c->output.bufferPos;
	//HTTP_PROC_DBG("Response write %d, Buffer rem %d , buffer add : %p",size,rem,c->response.buffer);

	if(rem < size && c->cgi.function!=cgi_transmit)
	{
		HTTP_DBG("Buffer Overflow\n");

		//copy what's possible
		memcpy(c->output.bufferPos,message,rem);
		message+=rem; //advance message
		size-=rem; //adjust size
		c->output.bufferPos+=rem; //mark buffer pos

		struct cgi_transmit_arg *transmit_cgi = (struct cgi_transmit_arg*)os_malloc(sizeof(struct cgi_transmit_arg));
		memcpy(&transmit_cgi->previous_cgi,&c->cgi,sizeof(cgi_struct));
		c->cgi.function=cgi_transmit;

		transmit_cgi->data = (uint8_t*)os_malloc(size);
		memcpy(transmit_cgi->data,message,size);
		transmit_cgi->len=size;
		transmit_cgi->dataPos=transmit_cgi->data;

		c->cgi.data = transmit_cgi;
		c->cgi.done=0;

		//http_transmit(c);
		//goto process; //restart;
	}
	else
	{
		// int i;
		// HTTP_DBG("writing size[%d]:", size);
		// for (i=0; i<size; i++) { os_printf("%c", message[i]); }
		memcpy(c->output.bufferPos,message,size);
		c->output.bufferPos+=size;
	}
	return 1;
}

int http_write(http_connection *c,const char * message)
{
	size_t len = strlen(message);
	return http_nwrite(c,message,len);
}

// HEADER RELATED FUNCTIONS -------------------------------------------------------
header * http_get_header(http_connection *conn,const char* header)
{
	int i=0;
	while(conn->headers[i].key!=NULL)
	{
		if (strcmp(conn->headers[i].key,header)==0)
		{
			if (conn->headers[i].value!=NULL) { return &(conn->headers[i]); }
			else { return NULL; }
		}
		i++;
	}
	return NULL;
}

void http_set_save_header(http_connection *conn,const char* header)
{
	HTTP_PROC_DBG("http_parser will save header :%s\n",header);

	int j=0;
	while(conn->headers[j].key!=NULL && j< MAX_HEADERS) j++;
	if(j==MAX_HEADERS) return;

	conn->headers[j].key=(char*)header;
	conn->headers[j].save=0;
}

void http_set_save_body(http_connection *conn)
{
	HTTP_DBG("http_parser will save body\n");
	conn->body.save=1;

	//make sure body is free
	if(conn->body.data!=NULL)
	{
		os_free(conn->body.data);
		conn->body.data=NULL;
	}
}

char * http_url_get_field(http_connection *c,enum http_parser_url_fields field)
{
	if (c->url_parsed.field_set & (1<<field))
	{
		char * start = c->url + c->url_parsed.field_data[field].off;
		char * end = start + c->url_parsed.field_data[field].len -1;
		end++;
		*end=0;

		if (*start==0)
			{ if (field==UF_PATH) { *start='/'; } }
		return start;

	} else { return NULL; }
}

char * http_url_get_query_param(http_connection *c,char* param)
{
	HTTP_PROC_DBG("http_url_get_query_param\n");

	//return null if there's no query at all
	if(!(c->url_parsed.field_set & (1<<UF_QUERY))) { return NULL; }

	//find field
	char *start = c->url + c->url_parsed.field_data[UF_QUERY].off;
	char *end = start + c->url_parsed.field_data[UF_QUERY].len-1;

	char *param_search = (char*) os_malloc(strlen(param)+2);
	strcpy(param_search,param);
	strcat(param_search,"=");

	HTTP_QUERY_DBG("search for : %s in %s\n",param_search, start);

	char *ret=NULL;

	start--; //to start at ?
	while(start<=end)
	{
		HTTP_QUERY_DBG("char : %c\n",*start);

		if (*start == '?' || *start == '&' || *start=='\0')
		{
			HTTP_QUERY_DBG("Is match?\n");
			start++;
			HTTP_QUERY_DBG("param_search: %s start: %s\n", param_search, start);
			//check param name, case sensitive -- fix later
			if ((ret=strstr(start,param_search))!=NULL)
			{
				HTTP_QUERY_DBG("yes\n");
				//match
				start +=strlen(param_search);
				ret = start;

				//0 end string
				while (*start!='\0' && *start!='&') { start++; }

				*start='\0';
				break;
			}
		}
		start++;
	}
	os_free(param_search);
	return ret;
}

char * http_url_get_host(http_connection *c)
{
	return http_url_get_field(c,UF_HOST);
}

char * http_url_get_path(http_connection *c)
{
	return http_url_get_field(c,UF_PATH);
}

char * http_url_get_query(http_connection *c)
{
	return http_url_get_field(c,UF_QUERY);
}

void http_parse_url(http_connection *c)
{
	memset(&c->url_parsed,0,sizeof(struct http_parser_url));

	http_parser_parse_url((const char *)c->url, strlen(c->url),0 ,&c->url_parsed);

	#ifdef DEVELOP_VERSION
		HTTP_DBG("Parse URL : %s\n",c->url);

		HTTP_PROC_DBG("\tPORT: %d \n",c->url_parsed.port);

		if(c->url_parsed.field_set & (1<<UF_SCHEMA)){
			// HTTP_DBG("\tSCHEMA: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_SCHEMA].off,c->url_parsed.field_data[UF_SCHEMA].len);
		}
		if(c->url_parsed.field_set & (1<<UF_HOST)){
			// HTTP_DBG("\tHOST: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_HOST].off,c->url_parsed.field_data[UF_HOST].len);
		}
		if(c->url_parsed.field_set & (1<<UF_PORT)){
			// HTTP_DBG("\tPORT: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_PORT].off,c->url_parsed.field_data[UF_PORT].len);
		}
		if(c->url_parsed.field_set & (1<<UF_PATH)){
			// HTTP_DBG("\tPATH: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_PATH].off,c->url_parsed.field_data[UF_PATH].len);
		}
		if(c->url_parsed.field_set & (1<<UF_QUERY)){
			// HTTP_DBG("\tQUERY: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_QUERY].off,c->url_parsed.field_data[UF_QUERY].len);
		}
		if(c->url_parsed.field_set & (1<<UF_FRAGMENT)){
			// HTTP_DBG("\tFRAGMENT: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_FRAGMENT].off,c->url_parsed.field_data[UF_FRAGMENT].len);
		}
		if(c->url_parsed.field_set & (1<<UF_USERINFO)){
			// HTTP_DBG("\tUSER INFO: ");
			//nprintf(c->url + c->url_parsed.field_data[UF_USERINFO].off,c->url_parsed.field_data[UF_USERINFO].len);
		}
	#endif
}

// PARSER CALLBACKS -------------------------------------------------------
static int on_message_begin(http_parser *parser)
{
	HTTP_DBG("http_parser message begin\n");

	//nothing to do here
	return 0;
}

static int on_url(http_parser *parser, const char *url, size_t length)
{
	HTTP_PROC_DBG("http_parser url: \n");
	//nprintf(url,length);

	HTTP_PROC_DBG("http_parser method: %d\n",parser->method);

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	conn->state=HTTPD_STATE_ON_URL; //set state

	memcpy(conn->url,url,length); //copy url to connection info
	conn->url[length]=0; //null terminate string

	http_parse_url(conn);

	//execute cgi
	http_execute_cgi(conn);

	return 0;
}

static int on_status(http_parser *parser, const char *url, size_t length)
{
	HTTP_DBG("http_parser status: \n");
	//nprintf(url,length);

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	conn->state=HTTPD_STATE_ON_STATUS; //set state

	//execute cgi again
	http_execute_cgi(conn);

	return 0;
}


static int on_header_field(http_parser *parser, const char *at, size_t length)
{
	// HTTP_DBG("http_parser header: \n");
	//nprintf(at,length);

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	int i=0;
	while(conn->headers[i].key!=NULL){
		if(strncmp(conn->headers[i].key,at,length)==0){
			HTTP_DBG("marking header to save\n");
			//match header
			//turn on save header
			conn->headers[i].save=1;
			break;
		}
		i++;
	}
	return 0;
}

static int on_header_value(http_parser *parser, const char *at, size_t length)
{
	//HTTP_DBG("http_parser header value:\n");
	//nprintf(at,length);

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	int i=0;
	while (conn->headers[i].key!=NULL)
	{
		if (conn->headers[i].save==1)
		{
			HTTP_PROC_DBG("saving header\n");

			conn->headers[i].value=(char *) os_malloc(length+1);
			memcpy(conn->headers[i].value,at,length);
			conn->headers[i].value[length]=0; //terminate string;

			conn->headers[i].save=0;
			break;
		}
		i++;
	}
	return 0;
}

static int on_headers_complete(http_parser *parser)
{
	HTTP_PROC_DBG("http_parser headers complete\n");

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	conn->state = HTTPD_STATE_HEADERS_END; //set state

	//execute cgi again
	http_execute_cgi(conn);
}

static int on_body(http_parser *parser, const char *at, size_t length)
{
	HTTP_DBG("http_parser body:\n");

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;
	conn->state = HTTPD_STATE_ON_BODY; //set state

	if (conn->body.save)
	{
		if (conn->body.data==NULL)
		{
			HTTP_DBG("saving body len %d\n",length);

			conn->body.data = (char *) os_malloc(length+1);
			memcpy(conn->body.data,at,length);
			conn->body.len = length;
			conn->body.data[length]=0;
		}
		else
		{//assuming body can come in different tcp packets, this callback will be called
		//more than once
			HTTP_DBG("appending body len %d\n",length);

			size_t newLenght = conn->body.len+length;
			char * newBuffer = (char *) os_malloc(newLenght+1);
			memcpy(newBuffer,conn->body.data,conn->body.len); //copy previous data
			memcpy(newBuffer+conn->body.len,at,length); //copy new data
			os_free(conn->body.data); //free previous
			conn->body.data=newBuffer;
			conn->body.len=newLenght;
			conn->body.data[newLenght]=0;
		}
	}
	//execute cgi again
	http_execute_cgi(conn);

	return 0;
}

static int on_multipart_body(http_parser *parser, const char *at, size_t length)
{
	HTTP_DBG("\non_multipart_body:\n");

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;

	conn->state = HTTPD_STATE_ON_BODY; //set state

	if(conn->body.data==NULL)
	{
		NODE_DBG("saving body len %d\n",length);
		conn->body.data = (uint8_t *) os_malloc(length+1);
		memcpy(conn->body.data,at,length);
		conn->body.len = length;
		conn->body.data[length]=0;
	}
	else
	{
		NODE_DBG("http multipart, data not null. length = %d\n", length);
		os_free(conn->body.data);
		conn->body.data = (uint8_t *) os_malloc(length+1);
		memcpy(conn->body.data,at,length);
		conn->body.len = length;
		conn->body.data[length]=0;
	}
	conn->cgi.execute=http_fs_api_uploadfile;
	http_execute_cgi(conn);

	return 0;
}


void http_use_multipart_body(http_connection *conn)
{
	HTTP_DBG("http_process will save using multipart\n");
	conn->parser_settings.on_body=on_multipart_body;
}


// SPECIAL CALLBACKS -------------------------------------------------------
// special callback like function to pass ws data to cgi
static int on_websocket_data(http_parser *parser, char *data, size_t length)
{
	HTTP_DBG("on_websocket_data: \n");
	//int i;
	//for(i=0;i<length;i++)
	//	os_printf("%02X",data[i]);
	//os_printf("\r\n");

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;
	conn->state = HTTPD_STATE_WS_DATA; //set state
	conn->body.data = (char *)data;
	conn->body.len = length;

	//execute cgi again
	http_execute_cgi(conn);

	conn->body.data=NULL;
	conn->body.len=0;

	return 0;
}

static int on_message_complete(http_parser *parser)
{
	HTTP_DBG("http_parser message complete\n");

	//grab the connection
	http_connection * conn = (http_connection *)parser->data;
	conn->state = HTTPD_STATE_BODY_END; //set state

	//execute cgi again
	http_execute_cgi(conn);

	//free body
	if (conn->body.save==1 && conn->body.data!=NULL)
	{
		HTTP_DBG("freeing body memory\n");
		os_free(conn->body.data);
		conn->body.len=0;
	}

	return 0;
}

//Looks up the connection for a specific esp connection
static http_connection *http_process_find_connection(void *arg)
{
	int i;

	if (arg==0)
	{
		HTTP_DBG("http find: Couldn't find connection for %p\n", arg);
		return NULL;
	}

	for(i=0;i<MAX_CONNECTIONS;i++) if(connection_poll[i].espConnection==arg) break;

	if (i<MAX_CONNECTIONS)
	{
		return &connection_poll[i];
	} else {
		HTTP_DBG("http_find wtf? %p\n", arg);
		return NULL; //WtF?
	}
}

void http_process_free_connection(http_connection *conn)
{
	http_reset_buffer(conn);
	conn->espConnection=NULL;

	conn->cgi.function=NULL;
	conn->cgi.data=NULL;
	conn->cgi.argument=NULL;

	//free headers
	int j=0;
	while (j<MAX_HEADERS)
	{
		if (conn->headers[j].value!=NULL && (conn->headers[j].value!=conn->headers[j].key))
		{
			os_free(conn->headers[j].value);
			conn->headers[j].value=NULL;
		}
		conn->headers[j].key=NULL;
		j++;
	}

	//free buffer
	os_free(conn->output.buffer);
}

// ESP CONN CALLBACKS -------------------------------------------------------
static void http_process_sent_cb(void *arg)
{
	HTTP_DBG("\nhttp_process_sent_cb, conn %p\n",arg);

	http_connection *conn = http_process_find_connection(arg);

	if(conn==NULL) { return; }

	if (conn->cgi.done==1)  //Marked for destruction?
	{
		HTTP_PROC_DBG("Conn %p is done. Closing.\n", conn->espConnection);
		espconn_disconnect(conn->espConnection);
		http_process_free_connection(conn);
		return; //No need to execute cgi again
	}

	if(conn->parser.upgrade)
	{
		conn->state = HTTPD_STATE_WS_DATA_SENT; //set state
	}
	http_execute_cgi(conn);
}

// SOCKET DATA AVAILABLE CALLBACKS -------------------------------------------------------
static void http_process_received_cb(void *arg, char *data, unsigned short len)
{
	HTTP_DBG("\nhttp rcv_cb, len: %d\n",len);

	http_connection *conn = http_process_find_connection(arg);
	if (conn==NULL)
	{
		espconn_disconnect(arg);
		return;
	}

	//pass data to http_parser
	size_t nparsed = http_parser_execute(
		&(conn->parser),
		&(conn->parser_settings),
		data,
		len);

	if (conn->parser.upgrade)
	{
  		/* handle new protocol */
		on_websocket_data(&conn->parser,data,len);
	}
	else if (nparsed != len)
	{
	  /* Handle error. Usually just close the connection. */
		espconn_disconnect(conn->espConnection);
		http_process_free_connection(conn);
	}
}

static void http_process_disconnect_cb(void *arg)
{
	HTTP_PROC_DBG("http_process_disconnect_cb: %p\n",arg);

	http_connection *conn = http_process_find_connection(arg);
	if (conn!=NULL)
	{
		//is it a client connection?
		if (conn->espConnection== &conn->client_connection)
		{
			//tell parser about EOF
			http_parser_execute(
				&(conn->parser),
				&(conn->parser_settings),
				NULL,
				0);
		}

		if (conn->parser.upgrade)
		{
			conn->state=HTTPD_STATE_WS_CLIENT_DISCONNECT;
			http_execute_cgi(conn);
		}
		http_process_free_connection(conn);
	}
	else
	{
		//find connections that should be closed
		int i;
		for(i=0;i<MAX_CONNECTIONS;i++)
		{
			struct espconn *conn = connection_poll[i].espConnection;

			if(conn!=NULL)
			{
				if (conn->state==ESPCONN_NONE || conn->state >=ESPCONN_CLOSE)
				{
				//should close
					//is it a client connection? If yes, don't free
					if(&connection_poll[i].client_connection != connection_poll[i].espConnection)
					{
						http_process_free_connection(&connection_poll[i]);
					}
				}
			}
		}
	}
}

static void http_process_reconnect_cb(void *arg, sint8 err)
{
	//some error
	HTTP_DBG("Reconnect conn=%p err %d\n",arg,err);

	struct espconn * conn = (struct espconn *)arg;
	conn->state=ESPCONN_CLOSE; // make sure of that

	http_process_disconnect_cb(arg);
}
LOCAL uint16_t server_timeover = 60*60*12; // yes. 12h timeout. so what? :)
http_connection * http_new_connection(uint8_t in,struct espconn *conn)
{
	HTTP_DBG("http_new_connection\n");
	int i;
	//Find empty connection in pool
	for (i=0; i<MAX_CONNECTIONS; i++) if (connection_poll[i].espConnection==NULL) break;

	if (i>=MAX_CONNECTIONS)
	{
		HTTP_PROC_DBG("Connection pool overflow!\n");

		if(conn!=NULL)
		{
			espconn_disconnect(conn);
		}
		return NULL;
	}

	HTTP_PROC_DBG("\nNew connection, conn=%p, pool slot %d\n", conn, i);

	if (conn!=NULL)
	{
		connection_poll[i].espConnection=conn;
		connection_poll[i].espConnection->reverse=&connection_poll[i];
	}

	//allocate buffer
	connection_poll[i].output.buffer = (uint8_t *)os_zalloc(HTTP_BUFFER_SIZE);

	//zero headers again- for sanity
	int j=0;
	while (j<MAX_HEADERS)
	{
		if (connection_poll[i].headers[j].value!=NULL
		&& (connection_poll[i].headers[j].value!=connection_poll[i].headers[j].key))
		{
			os_free(connection_poll[i].headers[j].value);
			connection_poll[i].headers[j].value=NULL;
		}
		connection_poll[i].headers[j].key=NULL;
		j++;
	}

	//mark cgi as not done
	connection_poll[i].cgi.done=0;

	//free response buffer again
	http_reset_buffer(&connection_poll[i]);

	//init body
	connection_poll[i].body.len=0;
	connection_poll[i].body.save=0;
	connection_poll[i].body.data=NULL;

	//reset parser
	http_parser_settings_init(&(connection_poll[i].parser_settings));
	connection_poll[i].parser_settings.on_message_begin=on_message_begin;
	connection_poll[i].parser_settings.on_url=on_url;
	connection_poll[i].parser_settings.on_header_field=on_header_field;
	connection_poll[i].parser_settings.on_header_value=on_header_value;
	connection_poll[i].parser_settings.on_headers_complete=on_headers_complete;
	connection_poll[i].parser_settings.on_body=on_body;
	connection_poll[i].parser_settings.on_message_complete=on_message_complete;

	//attach httpd connection to data (socket info) so we may retrieve it easily inside parser callbacks
	connection_poll[i].parser.data=(&connection_poll[i]);

	//init parser
	if (in)
	{
		http_parser_init(&(connection_poll[i].parser),HTTP_REQUEST);

		//register espconn callbacks
		espconn_regist_recvcb(conn, http_process_received_cb);
		espconn_regist_reconcb(conn, http_process_reconnect_cb);
		espconn_regist_disconcb(conn, http_process_disconnect_cb);
		espconn_regist_sentcb(conn, http_process_sent_cb);
		// espconn_regist_write_finish(conn, http_process_sent_cb);
	}
	else
	{
		http_parser_init(&(connection_poll[i].parser),HTTP_RESPONSE);

		connection_poll[i].espConnection = &connection_poll[i].client_connection;

		connection_poll[i].espConnection->reverse=&connection_poll[i]; //set reverse object

		connection_poll[i].espConnection->type=ESPCONN_TCP;
		connection_poll[i].espConnection->state=ESPCONN_NONE;
		connection_poll[i].espConnection->proto.tcp = &connection_poll[i].client_tcp;

		//register espconn callbacks
		espconn_regist_recvcb(connection_poll[i].espConnection, http_process_received_cb);
		espconn_regist_reconcb(connection_poll[i].espConnection, http_process_reconnect_cb);
		espconn_regist_disconcb(connection_poll[i].espConnection, http_process_disconnect_cb);

		espconn_regist_sentcb(connection_poll[i].espConnection, http_process_sent_cb);
		// espconn_regist_write_finish(connection_poll[i].espConnection, http_process_sent_cb);
	}

	espconn_regist_time(conn,server_timeover,0);

	return &connection_poll[i];
}

static void http_process_init()
{
	int i;
	//init connection pool
	for (i=0; i<MAX_CONNECTIONS; i++)
	{
		//init with 0s
		memset(&connection_poll[i],0,sizeof(http_connection));
	}
}
