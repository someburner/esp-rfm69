#ifndef __MQTT_API_H
#define __MQTT_API_H

// #define MQTT_EXPECT_IP

#ifdef MQTT_EXPECT_IP
#define DEFAULT_MQTT_EXPECT_IP 0.0.0.0
#endif

#define NULL_STR "null"

/* MQTT-Related Structures */
typedef struct
{
	uint8_t    topic_offset; //length of sub topic
	char *     topic;
} USER_TOPIC_T;

typedef struct
{
	char *      		mqtt_host; //MQTT host IP
   USER_TOPIC_T**   	subs;  //list of sub topics
   USER_TOPIC_T**   	pubs;  //list of pub topics

} USER_MQTT_T;

void mqtt_app_init(char * ip);

int set_mqtt_host(char * host);

bool mqttIsConnected();
void mqtt_setconn(uint8_t state);

void mqtt_app_update_handle(USER_MQTT_T * user_mqtt_ptr);
void mqtt_app_update_topics();

bool mqtt_api_pub(unsigned topicNum, char * msg, int len);

#endif
