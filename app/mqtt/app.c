#include "c_types.h"
#include "c_stdio.h"
#include "json/cJSON.h"
#include "user_interface.h"
#include "mem.h"
#include "osapi.h"
#include "sntp.h"
#include "espconn.h"

#include "user_config.h"
#include "../util/bitwise_utils.h"

#include "mqtt.h"
#include "mqtt_msg.h"
#include "mqtt_helper.h"
#include "mqtt_api.h"

#define dt_id_bridge			1
#define DEVICE1_TYPEID 		dt_id_bridge

#define MQTT_MAX_LEN_CHECK(len) \
	if (len > MQTT_BUF_USABLE) { \
		client->rfmon->status |= RFMON_FULLQ; \
		goto add_new_json; \
	}

#define MAX_STRLEN_OF_TYPE(msg, msg_type) \
		((msg_type==samp_msg_t) ? (MAX_SAMPLE_STRLEN+msg->datalen+4) 	: \
		((msg_type==evt_msg_t)  ? (MAX_EVENT_STRLEN+msg->datalen+4) 	: \
		((msg_type==meta_msg_t) ? (MAX_META_STRLEN+msg->datalen+4) : 125)))

#define CHECK_NEXT_STRLEN(current_len, next_len, next_msg1, next_msg_type1, next_msg_status) \
	MQTT_MAX_LEN_CHECK(current_len); \
	next_len = MAX_STRLEN_OF_TYPE((next_msg1), (next_msg_type1)); \
	if ((next_len + current_len) > MQTT_BUF_USABLE) {\
		client->rfmon->status |= RFMON_FULLQ; \
		MQTT_DBG("Queue full. \n"); goto add_new_json; \
	} else { next_msg_status = client->rfmon->status &= ~(RFMON_FULLQ); current_len += next_len; }

static MQTT_Client mqtt_client;
static rfm69_mon_t rfm69_mon;

static uint8_t device_type_id_list[2] = {0, dt_id_bridge };
static uint32_t device_id_list[RFM_MAX_DEVICE_CT] = { 0, BRIDGE_ID, 0 };

static os_timer_t mqtt_timer;
static os_timer_t testmq_timer;
static os_timer_t rfm_msg_timer;

extern uint8_t wifiStatus_mq;
extern uint8_t lastwifiStatus_mq;

uint32_t rfm_buff_sent_sz = 0;

static char bootmsg[20];

typedef struct
{
	uint8_t id;
	char topic[15];
}  mqtt_topic_t;

// mqtt_topic_t rfm_subs[MQTT_TOPIC_COUNT];
mqtt_topic_t rfm_test_pub = { 5, "/0000000"};
mqtt_topic_t rfm_test_sub = { 6, "/f00000/0000"};

static void my_queue_cb(cJSON * json_obj, void *data, uint8_t msg_type)
{
	rfm_queue_msg_t * q_msg = (rfm_queue_msg_t *)data;
	cJSON * d_values = NULL;
	uint16_t i;

	uint16_t pts = (uint16_t)q_msg->datalen;
	MQTT_DBG("msg len = %d\n", pts);

	uint8_t datatype = (uint8_t)q_msg->datatype;

	if (json_obj == NULL)
	{
		NODE_ERR(" json_obj null?\n");
		return;
	}
	//
	// if (msg_type == samp_msg_t)
	// {
	//    cJSON_AddNumberToObject(json_obj, KEYSTR_SAMPLE_TYPE, datatype);
	//
	//    switch(datatype)
	//    {
	//       case rfm_sample_double:
	// 		{
	//          if (pts == 1)
	//             { cJSON_AddNumberToObject(json_obj, KEYSTR_SAMPLE_VALUE, *(double*)(q_msg->data)); break; }
	//
	// 			d_values = cJSON_CreateArray();
	//          for (i=0; i<pts; i++)
   //          {
	// 				// NODE_DBG("evt_data[double][%d]: %f\n",i,  *(double*)q_msg->data+(i*sizeof(double)));
   //          	cJSON_AddItemToArray(d_values, cJSON_CreateNumber(*((double*)q_msg->data+(i*sizeof(double)))));
   //          }
   //          cJSON_AddItemToObject(json_obj, KEYSTR_SAMPLE_VALUE, d_values);
	//       } break;
	//
	//       case rfm_sample_char: {
	//          if (pts == 1)
	//             { cJSON_AddNumberToObject(json_obj, KEYSTR_SAMPLE_VALUE, *(uint8_t*)(q_msg->data)); break; }
	//
	// 			d_values = cJSON_CreateArray();
	//          for (i=0; i<pts; i++)
	//             {
	// 				NODE_DBG("evt_data[char][%d]: %c\n",i,*(uint8_t*)q_msg->data+(i*sizeof(uint8_t)));
	//             cJSON_AddItemToArray(d_values, cJSON_CreateNumber(*((uint8_t*)q_msg->data+(i*sizeof(uint8_t)))));
	//             }
	//             cJSON_AddItemToObject(json_obj, KEYSTR_SAMPLE_VALUE, d_values);
	//       } break;
	//
	//       case rfm_sample_int:
	// 		{
	//          if (pts == 1)
	//             { cJSON_AddNumberToObject(json_obj, KEYSTR_SAMPLE_VALUE, *(int*)(q_msg->data)); break; }
	//
	// 			d_values = cJSON_CreateArray();
	//          for (i=0; i<pts; i++)
	//             {
	// 					NODE_DBG("evt_data[int][%d]: %d\n",i,*(int*)q_msg->data+(i*sizeof(int)));
	//             cJSON_AddItemToArray(d_values, cJSON_CreateNumber(*((int*)q_msg->data+(i*sizeof(int)))));
	//             }
	//             cJSON_AddItemToObject(json_obj, KEYSTR_SAMPLE_VALUE, d_values);
	//       } break;
	//    }
	//
	// } else if (msg_type == evt_msg_t) {
	//
	// 	char * msg_str = (char*)q_msg->data;
	//
	// 	cJSON_AddNumberToObject(json_obj, KEYSTR_EVENT_TYPE, datatype);
	// 	if (msg_str)
	// 	{
	// 		cJSON_AddStringToObject(json_obj, KEYSTR_DATA, msg_str);
	// 	}
	// } else if (msg_type== meta_msg_t) {
	// 	char * msg_str2 = (char*)q_msg->data;
	//
	// 	if (msg_str2)
	// 	{
	// 		cJSON_AddStringToObject(json_obj, KEYSTR_DATA, msg_str2);
	// 	}
	// }
}

static void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	MQTT_DBG("MQTT: Connected\n");
	MQTT_DBG("mqttConnectedCb: rfmon status = %02x, pub_buf_len = %d\n", client->rfmon->status, client->rfmon->pub_buff_len);

	os_timer_disarm(&rfm_msg_timer);
	client->rfmon->pub_buff_len = 0;
	os_timer_arm(&rfm_msg_timer, 14004, 1);

	client->rfmon->status |= RFMON_CONN;			//set connected flag
	client->rfmon->status &= ~(RFMON_DISCONN);  //clear disconn flag
	client->rfmon->status &= ~(RFMON_INIT);		//just in case?

	char * setTopic = (char *)os_zalloc(strlen(rfm_test_sub.topic)+1);
	strcpy(setTopic,rfm_test_sub.topic);
	MQTT_Subscribe(client, setTopic, 0);
	os_free(setTopic);

	int i;
	for(i=0;i<MQTT_DEVICE_COUNT;i++)
	{
		// char * setTopic = (char *)os_zalloc(strlen(rfm_subs[i].topic)+1);
		// strcpy(setTopic,rfm_subs[i].topic);
		// MQTT_Subscribe(client, setTopic, 0);
		// NODE_DBG("subscripted to topic %d = %s\n", i, rfm_subs[i].topic);
		//
		// os_free(setTopic);
	}
	os_timer_disarm(&testmq_timer);
	os_timer_arm(&testmq_timer, 22033, 0);

	NODE_DBG("Connected. Arming testmq_timer\n");
}

static void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	MQTT_DBG("MQTT: Disconnected\n");
	NODE_DBG("Disarming testmq_timer\n");
	client->rfmon->status |= RFMON_DISCONN;		//set disconnected flag
	client->rfmon->status &= ~(RFMON_CONN);  //clear connected flag
	os_timer_disarm(&testmq_timer);
}

static void updateRfmQueueStatus(MQTT_Client* client, int caller)
{
	MQTT_DBG("updateRfmQueueStatus, caller = %d\n", caller);
	uint8_t bitstate = 0;
	struct rfmqtt_msg_st * fq_msg = NULL;
	rfm_queue_msg_t * getmsg = NULL;
	struct rfmqtt_msg_st * fq_temp;

	fq_msg = STAILQ_FIRST(&(client->rfmon->rfmqtt_msg_list));
	STAILQ_FOREACH_SAFE(fq_msg, &(client->rfmon->rfmqtt_msg_list), next, fq_temp)
	{
		// fq_temp = STAILQ_NEXT(fq_msg, next);

		bitstate = Byte_GetHighestOrderBit(fq_msg->msg_status);
		bitstate = (1<<bitstate);

		switch (bitstate)
		{
			case RFMON_MSG_DELETE: //highest mask order - always perform this
			{
				//marked for deletion
				STAILQ_REMOVE(&(client->rfmon->rfmqtt_msg_list), fq_msg, rfmqtt_msg_st, next);
				if (fq_msg->msg != NULL)
				{
					getmsg = (rfm_queue_msg_t*) fq_msg->msg;
					if (getmsg)
					{
						if (getmsg->data) { os_free(getmsg->data); }
						// os_free(getmsg);
						os_free(fq_msg->msg);
						getmsg = NULL;
					}
					MQTT_DBG("\tdeleted an fq_msg->msg\n");
				} else {
					MQTT_DBG("fq_msg->msg == null?\n");
				}
				os_free(fq_msg);
				MQTT_DBG("\t\tdeleted an fq_msg\n");
			} break;

			//waiting state.
			case RFMON_MSG_WAIT:
			{
				//If puback was called, msg was sent. Sits here until puback is called
				if (caller == mqtt_puback_caller_id)
					{ fq_msg->msg_status |= RFMON_MSG_DELETE; }
			} break;

			// if called by puback, pub was sent off. otherwise it's still in queue
			case RFMON_MSG_PUB_SENT:
			{
				if (caller == mqtt_puback_caller_id) //msg timed out, but still in buffer. Sent to waiting.
					{ fq_msg->msg_status |= RFMON_MSG_DELETE; }

				if (caller == rfm_timer_pub_caller_id) //msg timed out, but still in buffer. Sent to waiting.
					{ fq_msg->msg_status |= RFMON_MSG_WAIT; }
			} break;

			//pub has been added to queue
			case RFMON_MSG_PUB_ADD:
			{
				if (caller == rfm_timer_pub_caller_id)
					{ fq_msg->msg_status |= RFMON_MSG_PUB_SENT; }

				//if msg is empty, just delete
				if (caller == rfm_timer_empty_caller_id)
					{ fq_msg->msg_status |= RFMON_MSG_DELETE; }
			} break;

			case RFMON_MSG_PUB_GEN:
			{
				//do nothing
			} break;

			case RFMON_MSG_PUB_INIT:
			{
				//reset message state
				fq_msg->msg_status = 0;
			} break;

		} /* End msg_state Switch() */

	} /* End STAILQ loop */
}

static void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	MQTT_DBG("mqttPublishedCb\n");

	client->rfmon->pub_buff_len = 0; 				//fixed mqtt header size
	client->rfmon->status &= ~(RFMON_FULLQ); 	//clear full buff flag
	client->rfmon->status &= ~(RFMON_DISCONN);	//clear disconn flag
	client->rfmon->status |= RFMON_CONN;			//set connected flag

	/* Update queue statuses - sent messages will be marked for deletion */
	updateRfmQueueStatus(client, mqtt_puback_caller_id);
}

/* MQTT Data received CB.
 * Checks if message is valid JSON and calls appropriate parser/handler
 */
static void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	NODE_DBG("Receive topic: %s, data: %d bytes\n", topicBuf, data_len);

	cJSON * root = cJSON_Parse(dataBuf);
	if(root==NULL) { NODE_DBG("MQTT: cJSON parser returned NULL\n"); goto clean_mqtt; }

	cJSON * j_devid = cJSON_GetObjectItem(root,KEYSTR_DEVICE_ID);
	if ((j_devid==NULL) || (j_devid->type != cJSON_Number) || (j_devid->valueint != 1)) { goto clean_json; }

	cJSON * j_arr_item = NULL;

clean_json:
	cJSON_Delete(root);

clean_mqtt:
	os_free(topicBuf);
	os_free(dataBuf);

}

static void mqttTimeoutCb(uint32_t *args)
{
	MQTT_DBG("mqttTimeoutCb\n");
	MQTT_Client* client = (MQTT_Client*)args;

	/* Last sent message timed out. Update queue to mark as in send buff */
	updateRfmQueueStatus(client, mqtt_timeout_caller_id);
}

/* Simple method to set MQTT connection */
void mqtt_setconn(uint8_t state)
{
	// check if this is the first connection. Don't allow if RFMON_DISCONN set
	if (state == 1) {
		if ((rfm69_mon.status & RFMON_INIT) || (rfm69_mon.status & RFMON_DISCONN))
		{
			rfm69_mon.status &= ~(RFMON_INIT); //clear init flag
			NODE_DBG("MQTT: Detected wifi network up\n");
			MQTT_Connect(&mqtt_client);
		}
	} else {
		NODE_DBG("MQTT: Detected wifi network down\n");
	}
}

static void rfm_msg_timer_cb(void *arg)
{
	MQTT_DBG("rfm_msg_timer_cb\n");
	MQTT_Client* client = (MQTT_Client*)&mqtt_client;

	// Check if keepalive has timed-out (for debug purposes only)
	if (mqtt_client.keepAliveTick > MQTT_KEEPALIVE)
	{
		MQTT_DBG("tick = %d! status = %d. Sending ping\n", mqtt_client.keepAliveTick, client->rfmon->status);
	}

	//clean up any deleted messages
	updateRfmQueueStatus(client, null_msg_caller_id);

	// if queue is full or is client disconnectd, exit.
	if ((client->rfmon->status & RFMON_FULLQ) || (client->rfmon->status & RFMON_DISCONN))
	{
		NODE_DBG("MQTT: Output buffer is full.\nWait to post or clear buffer first.\n");
		return;
	}

	static uint32_t now = 0;
   struct rfmqtt_msg_st * fq_msg = NULL;
	rfm_queue_msg_t * getmsg = NULL;

	struct rfmqtt_msg_st * fq_temp;
	char * outstr = NULL;
	cJSON * gen_obj = NULL;
	cJSON * ds_root = NULL;

	uint32_t current_json_strlen = 0;
	uint32_t next_len = 0;

	/* Bridge info section */
	if (wifiStatus_mq==2)
	{
		now = sntp_get_current_timestamp();
		MQTT_DBG("now = %u\n", now);
	}

	NODE_DBG("rfmon->pub_buff_len = %u\n", client->rfmon->pub_buff_len);
	if ((client->rfmon->pub_buff_len + MAX_BRIDGE_STRLEN) > MQTT_BUF_USABLE)
	{
		client->rfmon->status |= RFMON_FULLQ;
		NODE_DBG("pub buf limit reached! RFMON_FULLQ set\n");
		return;
	}

	/* Setup root object and add Bridge headers */
	ds_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(ds_root, KEYSTR_DATETIME, now);
	cJSON_AddNumberToObject(ds_root, KEYSTR_DEVICE_ID, device_id_list[dt_id_bridge]);
	// cJSON_AddNumberToObject(ds_root, KEYSTR_DEVICE_TYPE, device_type_id_list[dt_id_bridge]);

	/* init json_srlen to Bridge strlen unless pub_buff contains data still */
	if (client->rfmon->pub_buff_len == 0)
		{ current_json_strlen = MAX_BRIDGE_STRLEN; }
	else
		{ current_json_strlen = client->rfmon->pub_buff_len; }

	/* Device specfic section */
	cJSON * sample_arr 	= cJSON_CreateArray();
	cJSON * evt_arr 		= cJSON_CreateArray();
	cJSON * meta_arr 		= cJSON_CreateArray();

	int nonEmptyMsgCt = 0;
	STAILQ_FOREACH_SAFE(fq_msg, &(client->rfmon->rfmqtt_msg_list), next, fq_temp)
	{
		CHECK_NEXT_STRLEN(current_json_strlen, next_len, (rfm_queue_msg_t *)fq_msg->msg, fq_msg->msg_type, fq_msg->msg_status);
		// fq_temp = STAILQ_NEXT(fq_msg, next);

		fq_msg->msg_status |= RFMON_MSG_PUB_GEN;
		uint8_t bitorder = Byte_GetHighestOrderBit(fq_msg->msg_status);

		if (bitorder<=3)
		{
			gen_obj = cJSON_CreateObject();

			fq_msg->cb(gen_obj, fq_msg->msg, fq_msg->msg_type);

			cJSON_AddNumberToObject(gen_obj, KEYSTR_DATETIME, fq_msg->stamp);
			cJSON_AddNumberToObject(gen_obj, KEYSTR_DEVICE_ID, device_id_list[fq_msg->dev_num]);
			// cJSON_AddNumberToObject(gen_obj, KEYSTR_DEVICE_TYPE, fq_msg->dev_num);


			if (fq_msg->msg_type == samp_msg_t)
				{ cJSON_AddItemToArray(sample_arr, gen_obj);}
			else if (fq_msg->msg_type == evt_msg_t)
				{ cJSON_AddItemToArray(evt_arr, gen_obj); }
			else if (fq_msg->msg_type == meta_msg_t)
				{ cJSON_AddItemToArray(meta_arr, gen_obj); }

			fq_msg->msg_status |= RFMON_MSG_PUB_ADD;
			// if (fq_temp == NULL)
			// 	{ MQTT_DBG("end of msgs\n"); }
		}
	}
	updateRfmQueueStatus(client, rfm_timer_init_caller_id);


add_new_json:
	MQTT_DBG("Publish\n");
	char * astr = cJSON_PrintUnformatted(ds_root);
	if (astr == NULL) { return;}
	int jlen = strlen(astr);
	MQTT_DBG("Publish %d bytes to %s\n", jlen, rfm_test_pub.topic);
	if (jlen <= MQTT_BUF_USABLE)
	{
		if (nonEmptyMsgCt > 0)
			{ updateRfmQueueStatus(client, rfm_timer_pub_caller_id); }
		else
			{ updateRfmQueueStatus(client, rfm_timer_empty_caller_id); }
		bool pubret = MQTT_Publish(&mqtt_client, rfm_test_pub.topic, astr, jlen, 0, 1);
		if (pubret)
		{
			client->rfmon->pub_buff_len += jlen;
			NODE_DBG("Pub OK. buff_len = %d\n", client->rfmon->pub_buff_len);
		} else {
			NODE_DBG("Pub NOT OK\n");
		}
	} else {
		MQTT_DBG("\n\nsize > MQTT_BUF_USABLE ->  jlen = %d\n\n", jlen);
	}
	os_free(astr);

	//keep this spacing
	if (ds_root)
	{
		cJSON_Delete(ds_root);
	} else {
		NODE_DBG("ds_root already freed?\n");
	}
	return;
}

void push_data(uint8_t q_msgtype, uint8_t dev, void * buff, uint16_t len, uint8_t typeofdata, uint32_t stamp)
{
	static uint32_t dtstamp = 0;
   if (len < 1) return;

   struct rfmqtt_msg_st * f_msg = (struct rfmqtt_msg_st *)os_zalloc(sizeof(struct rfmqtt_msg_st));

	if (stamp != 0) 					{ dtstamp = stamp; }
	else if (wifiStatus_mq==2)		{ dtstamp = sntp_get_current_timestamp(); }

   rfm_queue_msg_t * queue_msg = (rfm_queue_msg_t *)os_zalloc(sizeof(rfm_queue_msg_t));

   f_msg->dev_num = dev;
   f_msg->stamp = dtstamp;
	f_msg->msg_status = 0;
	f_msg->msg_type = q_msgtype;

	queue_msg->datalen = len;
	queue_msg->datatype = typeofdata;

	// if (q_msgtype == samp_msg_t)
	// {
	//    switch(typeofdata)
	//    {
	//       case rfm_sample_double: {
	// 			queue_msg->data = (double*)os_zalloc(sizeof(double)*len);
	//          memcpy(queue_msg->data, buff, len);
	//       } break;
	//
	//       case rfm_sample_char: {
	// 			queue_msg->data = (char*)os_zalloc(sizeof(char)*len);
	//          memcpy(queue_msg->data, buff, len);
	//       } break;
	//
	//       case rfm_sample_int: {
	// 			queue_msg->data = (int*)os_zalloc(sizeof(int)*len);
	//          memcpy(queue_msg->data, buff, len);
	//       } break;
	//    }
	// }
	// else if (q_msgtype == evt_msg_t)
	// {
	// 	queue_msg->data = (char*)os_zalloc(sizeof(char)*len+1);
	// 	memcpy(queue_msg->data, buff, len+1);
	// }
	// else if (q_msgtype == meta_msg_t)
	// {
	// 	queue_msg->data = (char*)os_zalloc(sizeof(char)*len+1);
	// 	memcpy(queue_msg->data, buff, len+1);
	// }

	f_msg->msg = queue_msg;
	f_msg->cb=my_queue_cb;

	/* Add this msg to rf queue */
	STAILQ_INSERT_HEAD(&(mqtt_client.rfmon->rfmqtt_msg_list), f_msg, next);
}

/* Test timer that pubs a simple "boot-x" string on an interval*/
static void testmq_timer_cb(void *arg)
{
	os_timer_disarm(&testmq_timer);
	MQTT_DBG("Posting rf Test msg\n");
	static int x = 0;
	memset(bootmsg, 0, 20);
	int len = os_sprintf(bootmsg, "boot-%d", x);
	push_data(evt_msg_t, dt_id_bridge, (char*)bootmsg, len, 0, 0);
	x++;

	os_timer_arm(&testmq_timer, 7033, 0);
}

void mqtt_app_init()
{
	//init rf mon structure
	memset(&rfm69_mon, 0, sizeof(rfm69_mon_t));
	rfm69_mon.status |= RFMON_INIT;
	rfm69_mon.block = 0;
	rfm69_mon.pub_buff_len = 0;
	STAILQ_INIT(&(rfm69_mon.rfmqtt_msg_list)); //init STAILQ

	//set subs
	int i, j;
	for(i=0,j=0;i<MQTT_DEVICE_COUNT;i++)
	{

	}

	//set pub
	os_sprintf(rfm_test_pub.topic, "%s", MQTT_PUBSUB_PREFIX);

	// Init MQTT
	MQTT_InitConnection(&mqtt_client, MQTT_SERVER_DOMAIN, 1883, 0);

	/* Attach rfm69 MQTT Monitor struct to MQTT Client */
	mqtt_client.rfmon = &rfm69_mon;

	MQTT_InitClient(&mqtt_client, "rfm69_dev0", NULL, NULL, 20, 1); //last bit sets cleanSession flag
	MQTT_InitLWT(&mqtt_client, "/lwt", "offline-1235", 0, 0);
	MQTT_OnConnected(&mqtt_client, mqttConnectedCb);
	MQTT_OnDisconnected(&mqtt_client, mqttDisconnectedCb);
	MQTT_OnPublished(&mqtt_client, mqttPublishedCb);
	MQTT_OnData(&mqtt_client, mqttDataCb);
	MQTT_OnTimeout(&mqtt_client, mqttTimeoutCb);

	//arm test send timer
	os_memset(&testmq_timer,0,sizeof(os_timer_t));
	os_timer_disarm(&testmq_timer);
	os_timer_setfn(&testmq_timer, (os_timer_func_t *)testmq_timer_cb, NULL);

	//arm rfm_msg_timer timer - performs necessary queue management checks/methods
	os_memset(&rfm_msg_timer,0,sizeof(os_timer_t));
	os_timer_disarm(&rfm_msg_timer);
	os_timer_setfn(&rfm_msg_timer, (os_timer_func_t *)rfm_msg_timer_cb, NULL);
	os_timer_arm(&rfm_msg_timer, 14004, 1);
}
