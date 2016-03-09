/*
Some random cgi routines.
*/
#include "user_interface.h"
#include "c_stdlib.h"
#include "c_stdio.h"
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
#include "http_client.h"
#include "user_config.h"

#include "config.h"
#include "rfm/radiohandler.h"
#include "cgi_console.h"

#define CONS_CMDS_CT 4

static const char * console_json[CONS_CMDS_CT] =
{
	"start", "pwr", "to_node", "disp"
};

extern char console_buf[];
extern int console_wr;
extern int console_rd;
extern int console_pos;

uint8_t sendTxBuff[15];

//external reference to cgiRadioStatus, radiostate
extern RadioStatus radioStatus;
extern uint8_t radioState;

int http_console_api_clear(http_connection *c)
{
	CGI_CONS_DBG("api_clear\n");
	http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
	http_response_OK(c);
	console_rd = console_wr = console_pos = 0;
	return HTTPD_CGI_DONE;
}

int http_console_api(http_connection *c)
{
	CGI_CONS_DBG("http_console_api\n");

	//wait for whole body
	if(c->state <HTTPD_STATE_BODY_END)
		return HTTPD_CGI_MORE;

	http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
	// http_response_OK(c);

	//parse json and validate
   cJSON * root = cJSON_Parse(c->body.data);
   if(root==NULL) goto badrequest;

	cJSON * first = NULL;
	unsigned i;
	for (i=0; i < CONS_CMDS_CT; i++)
	{
		if (cJSON_HasObjectItem(root, console_json[i]))
		{
			first = cJSON_GetObjectItem(root,console_json[i]);
			break;
		}
	}

	if ((first==NULL) || i == CONS_CMDS_CT) { goto badrequest; }
	if ((first->type != cJSON_String) && (i != cons_start_t) && (i != cons_to_node_t)) { goto badrequest; }

	int tx = 0;
	int btn_val = 99;

	cJSON * cmd_ct = NULL;
	if (i > 0)
	{
		cmd_ct = cJSON_GetObjectItem(root,"ctcnt");
		if (cmd_ct==NULL) { goto badrequest; }
		if (cmd_ct->type != cJSON_Number) { goto badrequest; }
		btn_val = cmd_ct->valueint;
	}

	CGI_CONS_DBG("BTN Value: %d index: %d\n", btn_val, i);

	switch (i)
	{
		case cons_start_t:
		{
			char buff[256];
			if (first->type != cJSON_Number)
				{ goto badrequest; }

			int start = first->valueint;
			int len = 0; // length of text in buff
			int console_len = (console_wr+BUF_MAX-console_rd) % BUF_MAX; // num chars in console_buf

			if (start < console_pos) 							{ start = 0; }
			else if (start >= console_pos+console_len) 	{ start = console_len; }
			else { start = start - console_pos; }
			int rd = (console_rd+start) % BUF_MAX;
			while (len < BUF_MAX && rd != console_wr)
			{
				uint8_t c = console_buf[rd];
				buff[len++] = c;
				rd = (rd + 1) % BUF_MAX;
			}
			buff[len] = '\0'; //null terminate

			cJSON_AddNumberToObject(root,"len",console_len-start);
			cJSON *newSt = cJSON_CreateNumber(console_pos+start);
			cJSON_ReplaceItemInObject(root, "start", newSt);
			cJSON_AddStringToObject(root,"text", (const char *)buff);

			http_write_json(c,root);
			cJSON_Delete(root);
			return HTTPD_CGI_DONE;
		} break;

		case cons_pwr_t:
		{
			sendTxBuff[0] = (uint8_t) (49-btn_val);
         tx = 1;
			NODE_DBG("sendtxbuff[0]==%02x\n", sendTxBuff[0]);
			NODE_DBG("Sending %d bytes to node %d\n",tx, 99);
			generateRfmTxMsg(99, sendTxBuff, tx, false, false);
      } break;

		case cons_to_node_t:
		{
			/* Get requested payload */
			cJSON * dataJson = cJSON_GetObjectItem(root,"data");
		   if(dataJson==NULL) { goto badrequest; }
		   else if (dataJson->type != cJSON_String) { goto badrequest; }
			bool reqAck = false;

			/* Get ack checkbox value */
			cJSON * ackJson = cJSON_GetObjectItem(root,"ack");
			if ((ackJson!=NULL) && (ackJson->type == cJSON_Number))
				{ reqAck = (bool) ackJson->valueint; }

			int len, nodeid;
			len = strlen(dataJson->valuestring);
			NODE_DBG("send len = %d\n", len);
			nodeid = first->valueint;

			if ((len > 0) && (nodeid>0) && (nodeid < 255))
			{
				CGI_CONS_DBG("Sending %s to node %d\n", dataJson->valuestring, nodeid);
				generateRfmTxMsg((uint8_t)nodeid, dataJson->valuestring, len, reqAck, false);
			}

		} break;

		case cons_disp_t:
		{
			rfm_toggle_all_output();
		} break;

	} /* End cons_method_t Switch() */
	http_response_OK(c);
	cJSON_Delete(root);
	return HTTPD_CGI_DONE;

badrequest:
	http_response_BAD_REQUEST(c);
	cJSON_Delete(root);
	return HTTPD_CGI_DONE;

	//shut up compiler
	return HTTPD_CGI_DONE;
}
