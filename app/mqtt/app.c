#include "c_types.h"
#include "c_stdio.h"
#include "user_interface.h"
#include "mem.h"
#include "osapi.h"
#include "sntp.h"
#include "espconn.h"

#include "user_config.h"
#include "util/bitwise_utils.h"

#include "mqtt.h"
#include "mqtt_msg.h"
#include "mqtt_api.h"

// #define USER_TEST_MSG //<--Uncomment to send the test message below on timer

static MQTT_Client mqtt_client;
static USER_MQTT_T * user_mqtt = NULL;

static os_timer_t mqtt_timer;
static bool isConnected = false;

#ifdef USER_TEST_MSG
static os_timer_t testmq_timer;
static char testmsg[] = "{\"message\":\"test1234\",\"severity\":\"INFO\"}";
#endif

extern uint8_t wifiStatus_mq;
extern uint8_t lastwifiStatus_mq;

#define MQTT_STR_STR(V) #V
#define MQTT_STR(V) MQTT_STR_STR(V)


/* Simple method to set MQTT connection */
void mqtt_setconn(uint8_t state)
{
	// check if this is the first connection. Don't allow if FMON_DISCONN set
	if (state == 1) {
		MQTT_Connect(&mqtt_client);
	} else {
		NODE_DBG("MQTT: Detected wifi network down\n");
	}
}

bool mqttIsConnected()
{
	return isConnected;
}


static void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	NODE_DBG("MQTT: Connected\n");
	isConnected = true;

	unsigned i;

	for (i=0; i< MQTT_SUB_COUNT ; i++)
	{
		if (user_mqtt->subs[i]->topic != NULL) {
			if (MQTT_Subscribe(client, user_mqtt->subs[i]->topic, 1))
				NODE_DBG("+sub: %s, QoS %d\n", user_mqtt->subs[i]->topic, 1);
		}
	}
}

static void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	NODE_DBG("MQTT: Disconnected\n");
	isConnected = false;

#ifdef USER_TEST_MSG
	NODE_DBG("Disarming testmq_timer\n");
	os_timer_disarm(&testmq_timer);
#endif
}

static void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	NODE_DBG("mqttPublishedCb\n");
}

/* MQTT Data received CB.
 * Checks if message is valid JSON and calls appropriate parser/handler
 */
static void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	MQTT_Client* client = (MQTT_Client*)args;
	unsigned i;

	char *topicBuf = (char*)os_zalloc(topic_len+1);
	char *dataBuf = (char*)os_zalloc(data_len+1);

	memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	NODE_DBG("Receive topic: %s, data: %d bytes\n", topicBuf, data_len);

	os_free(topicBuf);

	#ifdef MQTT_SUB_TOPIC1
	if (os_strncmp(topic, user_mqtt->subs[0]->topic, topic_len) == 0)
	{
		NODE_DBG("Msg on topic1:\n");
		for (i=0; i<data_len; i++)
		{
			NODE_DBG("%c", dataBuf[i]);
		}
		NODE_DBG("\n");
	}
	#endif

	#ifdef MQTT_SUB_TOPIC2
	if (os_strncmp(topic, user_mqtt->subs[1]->topic, topic_len) == 0)
	{
		NODE_DBG("Msg on topic2:\n");
		for (i=0; i<data_len; i++)
		{
			NODE_DBG("%c", dataBuf[i]);
		}
		NODE_DBG("\n");
	}
	#endif

	os_free(dataBuf);
}

static void mqttTimeoutCb(uint32_t *args)
{
	NODE_DBG("mqttTimeoutCb\n");
	MQTT_Client* client = (MQTT_Client*)args;
}

#ifdef USER_TEST_MSG
/* Test timer that pubs a simple "boot-x" string on an interval*/
static void testmq_timer_cb(void *arg)
{
	os_timer_disarm(&testmq_timer);
	MQTT_DBG("Posting Test msg\n");

	mqtt_api_pub(0, testmsg, strlen(testmsg));

	os_timer_arm(&testmq_timer, 7033, 0);
}
#endif

bool mqtt_api_pub(unsigned topicNum, char * msg, int len)
{
	if (topicNum >= MQTT_PUB_COUNT) return false;
	if ((user_mqtt->pubs[topicNum]->topic == NULL) || (msg == NULL) || (len <= 0) )
		return false;

	return MQTT_Publish(&mqtt_client, user_mqtt->pubs[topicNum]->topic, msg, len, 1, 0);
}


void mqtt_app_update_handle(USER_MQTT_T * user_mqtt_ptr)
{
	user_mqtt = user_mqtt_ptr;
}

void mqtt_setup_topics()
{
   /* SUBSCRIBE */
   USER_TOPIC_T ** new_subs = (USER_TOPIC_T **) os_zalloc(sizeof(USER_TOPIC_T*)*MQTT_SUB_COUNT);
	#ifdef MQTT_SUB_TOPIC1
   new_subs[0] = (USER_TOPIC_T *) os_zalloc(sizeof(USER_TOPIC_T));
	new_subs[0]->topic = (char*)os_zalloc(sizeof(char) * MQTT_MAX_TOPIC_LEN);
	new_subs[0]->topic_offset = os_sprintf(new_subs[0]->topic,"%s", MQTT_STR(MQTT_SUB_TOPIC1));
	NODE_DBG("MQTT: sub[0] = %s\n", new_subs[0]->topic);
	#endif
	#ifdef MQTT_SUB_TOPIC2
   new_subs[1] = (USER_TOPIC_T *) os_zalloc(sizeof(USER_TOPIC_T));
	new_subs[1]->topic = (char*)os_zalloc(sizeof(char) * MQTT_MAX_TOPIC_LEN);
	new_subs[1]->topic_offset = os_sprintf(new_subs[1]->topic,"%s", MQTT_STR(MQTT_SUB_TOPIC2));
	NODE_DBG("MQTT: sub[1] = %s\n", new_subs[1]->topic);
	#endif

	/* Attach to user settings */
   user_mqtt->subs = new_subs;


   /* PUBLISH */
   USER_TOPIC_T ** new_pubs = (USER_TOPIC_T **) os_zalloc(sizeof(USER_TOPIC_T*)*MQTT_PUB_COUNT);

	#ifdef MQTT_PUB_TOPIC1
	new_pubs[0] = (USER_TOPIC_T *) os_zalloc(sizeof(USER_TOPIC_T));
	new_pubs[0]->topic = (char*)os_zalloc(sizeof(char) * MQTT_MAX_TOPIC_LEN);
	new_pubs[0]->topic_offset = os_sprintf(new_pubs[0]->topic,"%s", MQTT_STR(MQTT_PUB_TOPIC1));
	NODE_DBG("MQTT: pub[0] = %s\n", new_pubs[0]->topic);
	#endif

	/* Attach to user settings */
   user_mqtt->pubs = new_pubs;

   NODE_DBG("Pub[0] = %s\n", user_mqtt->pubs[0]->topic);

}

void mqtt_app_init(char * ip)
{
	/* Setup user MQTT settings */
	if (user_mqtt == NULL) user_mqtt = (USER_MQTT_T *) os_zalloc(sizeof(USER_MQTT_T));
	mqtt_setup_topics();

	NODE_DBG("mqtt init with IP=%s\n", ip);

	/* Init MQTT */
	MQTT_InitConnection(&mqtt_client, ip, 1883, 0);
	MQTT_InitClient(&mqtt_client, "user_dev0", NULL, NULL, 120, 1); //last bit sets cleanSession flag
	MQTT_InitLWT(&mqtt_client, "/lwt", "offline-1235", 0, 0);

	/* Attach MQTT callbacks */
	MQTT_OnConnected(&mqtt_client, mqttConnectedCb);
	MQTT_OnData(&mqtt_client, mqttDataCb);
	MQTT_OnDisconnected(&mqtt_client, mqttDisconnectedCb);
	MQTT_OnPublished(&mqtt_client, mqttPublishedCb);
	MQTT_OnTimeout(&mqtt_client, mqttTimeoutCb);


#ifdef USER_TEST_MSG
	/* arm test send timer */
	os_memset(&testmq_timer,0,sizeof(os_timer_t));
	os_timer_disarm(&testmq_timer);
	os_timer_setfn(&testmq_timer, (os_timer_func_t *)testmq_timer_cb, NULL);
	os_timer_arm(&testmq_timer, 7033, 0);
#endif
}
