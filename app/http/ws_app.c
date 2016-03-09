#include "user_interface.h"
#include "c_stdio.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "queue.h"

#include "user_config.h"
#include "flash_fs.h"

#include "../rfm/rfm_parser.h"

#include "cgi.h"
#include "cgi_wifi.h"
#include "http.h"
#include "http_process.h"

#include "websocket.h"
#include "http_websocket_server.h"

#define HTTP_PORT 80

#define WS_FS_STREAM_START "fsStreamStart"
#define WS_FS_STREAM_SIZE "fsStreamSize"
#define WS_FS_STREAM_DONE "fsStreamDone"
#define WS_FS_STREAM_ABORT "fsStreamAbort"

static os_timer_t testws_timer;

struct ws_app_context
{
	uint8_t stream_data;
	int packet_requested_size;
	http_connection *conn;
	uint8_t waiting_sent;
	char * packet;
	int packet_size;
};

static struct ws_app_context * ws_getset_context(struct ws_app_context *context)
{
	static struct ws_app_context *save_context = NULL;
	if (context) { save_context = context; NODE_DBG("\tContext Set! = %p\n", context); return NULL;}
	else if (save_context != NULL){ return save_context;}
	else { NODE_DEBUG("no context avail!\n"); }
}

static void live_data_cb(int x,int y, int z)
{
	static struct ws_app_context *context = NULL;
	if (context == NULL) context = ws_getset_context(NULL);

	if (context)
	{
		context->packet_size = os_sprintf(context->packet, "{\"x\":%d,\"y\":%d,\"z\":%d}", x, y, z);
		http_ws_push_text(context->conn,context->packet,context->packet_size);
		context->waiting_sent=1;
	}
}

static void ws_app_send_packet(struct ws_app_context *context)
{
	WS_DBG("Webscoket app send packet size %d, requested: %d",context->packet_size,context->packet_requested_size);

	if ( (!context->waiting_sent) && context->stream_data==1)
	{
		//send packet
		if(context->packet_requested_size != context->packet_size )
		{
			WS_DBG("Webscoket app changing packet size %p",context);

			//free previous packet
			if(context->packet!=NULL) { os_free(context->packet); }

			context->packet=NULL;
			context->packet_size=0;
		}

		if(context->packet==NULL)
		{
			WS_DBG("Webscoket allocating packet %p",context);
			context->packet = (char *)os_zalloc(context->packet_requested_size);
			context->packet_size=context->packet_requested_size;
			//fill with trash data
			int i;
			for(i=0; i < context->packet_size;i++)
				context->packet[i]=i%0xFF;
		}

		// http_ws_push_bin(context->conn,context->packet,context->packet_size);
		// context->waiting_sent=1;
	}
}


static int  ws_app_msg_sent(http_connection *c)
{
	WS_DBG("Ws app msg sent %p",c);

	struct ws_app_context *context = (struct ws_app_context*)c->reverse;

	if (context!=NULL)
	{
		WS_DBG("\tcontext found %p",context);

		context->waiting_sent=0;

		if(context->stream_data==1){
			//no requet to stop made, send next packet
			ws_app_send_packet(context);
		}
	}
}

static int  ws_app_client_disconnected(http_connection *c)
{
	NODE_DBG("Webscoket app client disconnected %p",c);

	//clean up
	struct ws_app_context *context = (struct ws_app_context*)c->reverse;

	if (context!=NULL)
	{
		if (context->packet!=NULL)
		{
			os_free(context->packet);
		}
		context->packet=NULL;
	}
	os_free(context);
}

static int  ws_app_client_connected(http_connection *c)
{
	NODE_DBG("Websocket app client connected %p",c);

	//create config
	struct ws_app_context *context = (struct  ws_app_context*)os_zalloc(sizeof(struct ws_app_context));
	context->conn=c;
	c->reverse = context; //so we may find it on callbacks

	WS_DBG("\tcontext %p",context);
}


static int  ws_app_msg_received(http_connection *c)
{
	WS_DBG("Websocket app msg received %p",c);
	struct ws_app_context *context = (struct ws_app_context*)c->reverse;
	static uint32_t total_len = 0;
	static struct fs_file_st * fs_st = NULL;
	static int fs_fd = -1;

	ws_frame *msg = (ws_frame *)c->cgi.argument;
	//just ignore and move on
	if (msg==NULL) { return HTTP_WS_CGI_MORE; }
	//just ignore and move on
	if (msg->SIZE <=0) { return HTTP_WS_CGI_MORE; }

	char * s = msg->DATA;
	char *last = s+msg->SIZE;

	//make a null terminated string
	char * str = (char *)os_zalloc(msg->SIZE + 1);
	memcpy(str,s,msg->SIZE);
	str[msg->SIZE]=0;
	total_len+= msg->SIZE;

	context->waiting_sent=0;

	if(strstr(str,WS_FS_STREAM_START)==str){
		//request to start stream
		NODE_DBG("ws: stream start\n");
		char * s_filename = str + strlen(WS_FS_STREAM_START);

		char * s_size_head = strstr(s_filename, WS_FS_STREAM_SIZE);

		if (s_size_head)
		{
			uint32_t s_filename_len = (s_size_head - s_filename);
			NODE_DBG("ws: filename len = %d, name = ", s_filename_len);
			char * fs_name = (char*) os_zalloc(sizeof(char)*s_filename_len + 1);
			memcpy(fs_name, s_filename, s_filename_len);
			// unsigned i =0;
			fs_name[s_filename_len] = '\0';
			// for (i; i<s_filename_len;i++) { NODE_DBG("%c", s_filename[i]); }
			NODE_DBG("%s\n", fs_name);

			int ret = fs_new_file(&fs_st, fs_name, true);
			NODE_DBG("ws ret: %d\n", ret);
			os_free(fs_name);

			char * s_size = s_size_head + strlen(WS_FS_STREAM_SIZE);
			int wsfs_size = atoi(s_size);
			NODE_DBG("ws: file size = %d\n", wsfs_size);
			fs_st->size = wsfs_size;
			total_len = 0;
			// fs_st->size = wsfs_size;
			// context->packet_size = os_sprintf(context->packet, "%s", "ok");
			http_ws_push_text(c,"ok",2);
			// http_ws_push_text(context->conn,context->packet,context->packet_size);

			// context->packet_requested_size=WS_FS_BUF_MAX;
			// if (wsfs_size < context->packet_requested_size)
			// {
			// 	context->packet_requested_size = wsfs_size;
			// }
			// context->stream_data = 1;
			// if(!context->waiting_sent)
			// {
			// 	ws_getset_context(context);
			// 	ws_app_send_packet(context); //send first pkt
			// 	// live_data_register_listener(live_data_cb);
			// }

		}

		// NODE_DBG("\trequest stream packet size %d",pSize);
		//
		// if(pSize>0 && pSize <= 1024)
		// {
		//
		// 	context->stream_data =1;
		// 	if(!context->waiting_sent)
		// 	{
		// 		ws_getset_context(context);
		// 		ws_app_send_packet(context); //send first pkt
		// 		live_data_register_listener(live_data_cb);
		// 	}
		// }

	}
	else if (strstr(str,WS_FS_STREAM_DONE)==str)
	{
		int pixres = fs_update_pix(fs_st);
		NODE_DBG("\tfs stream done pixres = %d\n", pixres);
		total_len -= strlen(WS_FS_STREAM_DONE);
		if (fs_st->size != total_len)
		{
			NODE_DBG("\tfs mismatch: got %d, expected %d\n",total_len, fs_st->size);
		}
		context->stream_data=0;
		fs_st = NULL;
	}
	else if (strstr(str,WS_FS_STREAM_ABORT)==str)
	{
		NODE_DBG("\trequest stream abort - delet file\n");
		context->stream_data=0;
	} else {
		if (fs_st)
		{
			int ret = fs_append_to_file(fs_st, msg->DATA, msg->SIZE);
			WS_DBG("\tfs_append = %d\n", ret);
		}

		http_ws_push_text(c,"ok",2);
	}

	os_free(str);

	return HTTP_WS_CGI_MORE;
}



void init_ws_server(void)
{
	WS_DBG("Websocket app init");

	//ws
	http_ws_server_init(ws_app_client_connected,ws_app_client_disconnected,ws_app_msg_received,ws_app_msg_sent);
	http_ws_server_start();

}
