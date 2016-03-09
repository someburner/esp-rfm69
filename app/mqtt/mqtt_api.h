#ifndef __MQTT_API_H__
#define __MQTT_API_H__

#include "mqtt_helper.h"
#include "queue.h"
#include "json/cJSON.h"

/* Max number of messages in rfmqtt_msg_list
 * size should be 28 Bytes per msg + data pointed to by (void *)
 * If message size is ~10 bytes, guses around 40 bytes ea
 * 25*40 = 1Kbytes. After this, we should store on SPI Flash.
 */
#define MAX_RFMQTT_MSG_CT   25

#define BRIDGE_ID             1234

/* Internal flags to keep track of mqtt state */
#define RFMON_INIT          (1<<7)
#define RFMON_EMPTYQ        (1<<6)
#define RFMON_FULLQ         (1<<5)
#define RFMON_DISCONN       (1<<4)
#define RFMON_CONN          (1<<3)

/* Internal flags to keep track of mqtt message sent states */
#define RFMON_MSG_PUB_INIT    0
#define RFMON_MSG_PUB_GEN     (1<<3)
#define RFMON_MSG_PUB_ADD     (1<<4)
#define RFMON_MSG_PUB_SENT    (1<<5)
#define RFMON_MSG_WAIT        (1<<6)
#define RFMON_MSG_DELETE      (1<<7)

/* Internal message caller ids */
typedef enum {
   null_msg_caller_id	 		    	= 0,
   rfm_timer_init_caller_id			= 1,
   rfm_timer_pub_caller_id			= 2,
   rfm_timer_empty_caller_id      = 3,
   mqtt_timeout_caller_id         	= 4,
   mqtt_puback_caller_id         	= 5
} rfm_caller_id_t;

/* early declaration */
struct msg_cb;

/* Message callback */
typedef void (*msg_cb_t)(cJSON * json_obj, void *data, uint8_t msg_type);

/* Message callback struct */
struct msg_cb
{
   msg_cb_t cb;
   void *data;
   uint8_t msg_type;
};

/* Internal message queue type */
typedef enum {
	null_msg_t	 			= 0,
   samp_msg_t         	= 1,
   evt_msg_t 			   = 2,
   meta_msg_t         	= 3,
   conf_msg_t         	= 4,
	max_msg_t				= 5
} rfm_queue_type_t;

/* Internal message struct */
typedef struct {
   uint16_t    datalen;
   uint8_t     datatype;
   uint8_t     block;

   void*       data;
} rfm_queue_msg_t;


 /* RFM message queue struct:
  * stamp      - time when received on external interface (i.e. RFM69)
  * dev_num    - device number for external sensor device
  * msg_status - bitsetting showing current msg state in relation to MQTT
  * cb         - callback to unpack data for json creation
  * msg        - void pointer to data object to be passed in to CB
  */
struct rfmqtt_msg_st {
   /* Required Queue info */
   STAILQ_ENTRY(rfmqtt_msg_st) next;

   /* General msg info */
   uint32_t       stamp;
   uint16_t       dev_num;
   uint8_t        msg_status;
   uint8_t        msg_type;

   /* msg callback data */
	msg_cb_t       cb;
	void *         msg;
};

/* RFM Queue Monitor struct:
 * Contains message queue and keeps track of MQTT status
 * status         - RFMMON_INIT/EMPTYQ/FULLQ/DISCONN/CONN
 * block          - alignment padding
 * pub_buff_len   - keeps track of MQTT out-buffer size.
 */
typedef struct {
   uint8_t     status;
   uint8_t     block;
   uint16_t 	pub_buff_len;

   STAILQ_HEAD(rfmqtt_msg_head, rfmqtt_msg_st) rfmqtt_msg_list;
} rfm69_mon_t;

/* set MQTT to conn state or disconn state */
void mqtt_setconn(uint8_t state);

/*
 * Data Sample Push API: Add a data sample to rfm Queue
 * buff: pointer to data
 * len: length of string, or number of data points
 * typeofdata: 0 for evt right now, for data: rfm_sample_double, int, char, etc
 * stamp: timestamp of when received
 */
void push_data(uint8_t q_msgtype, uint8_t dev, void * buff, uint16_t len, uint8_t typeofdata, uint32_t stamp);

#endif
