#ifndef RADIOHANDLER_H
#define RADIOHANDLER_H

#include <c_types.h>
#include <espconn.h>
#include "osapi.h"
#include "espfs.h"
#include "driver/rfm69.h"
#include "cgiflash.h"

struct radio_msg_st {
	STAILQ_ENTRY(radio_msg_st) next;
	uint8_t  nodeId;
	uint8_t  buffSize;
	uint8_t* buff;
	uint8_t  ctlbyte;
};

STAILQ_HEAD(tx_msg_head, radio_msg_st) tx_msg_list;// = STAILQ_HEAD_INITIALIZER(tx_msg_list);

void ICACHE_FLASH_ATTR rfmFwTimerCb(void *v);
void ICACHE_FLASH_ATTR radioMsgTxPush(uint8_t id, uint8_t *buf, short len, bool requestACK, bool sendACK);
void ICACHE_FLASH_ATTR radioMsgRxPush(uint8_t id, uint8_t *buf, short len, uint8_t ctl);
void ICACHE_FLASH_ATTR createRadioTask(uint8_t type, uint8_t arg);

void ICACHE_FLASH_ATTR disableSendTimers();
void ICACHE_FLASH_ATTR closeFwFile();

void ICACHE_FLASH_ATTR process_rfm_msg();
uint8_t ICACHE_FLASH_ATTR serverCodeLUT(const char fn_type, uint8_t indexedVal);
void ICACHE_FLASH_ATTR rfmSendTimerCb(void *v);
void ICACHE_FLASH_ATTR radioHandlerInit();
void ICACHE_FLASH_ATTR printRfmData();

enum rfm_event_t {
	rfm_invalid,
	rfm_handle_data,
	rfm_handle_fw_data,
	rfm_prepare_data,
	rfm_send,
	rfm_sendWithRetry,
	rfm_sendFrame,
	rfm_fw_begin,
	rfm_fw_cont,
	rfm_fw_end,
};

enum rfm_event_param_t {
	rfm_event_default,
	rfm_request_ack,
	rfm_send_ack,
	rfm_ack_requested,
	rfm_ack_received,
	rfm_fw_0,
};

enum rfm_handler_state_t {
	rfm_inititial_state,
	rfm_standby_state,
  rfm_fw_started,
	rfm_fw_error,
	rfm_fw_clear
};

extern uint8_t rfm_queue_t;

/* LISD3MDL Settings --> 'l'
 *  Get or Set settings reside on LIS3MDL chip.
 */
enum lis_setting_t {
  lis_invalid,
  lis_reg1,
  lis_reg2,
  lis_reg3,
  lis_reg4,
  lis_reg5,
  lis_status_reg,
  lis_int_cfg,
  lis_threshold_lb,
  lis_threshold_hb,
  lis_offset1,
  lis_offset2,
  lis_offset3,
  lis_offset4,
  lis_offset5,
  lis_offset6
};

/* Moteino Node Settings --> 'm'
 *  Get or Set settings that reside on Moteino.
 */
enum node_settings_t {
  node_set_invalid,
  node_axis,
  node_cal_time,
  node_delta,
  node_interrupt,
  node_percent,
  node_period,
  node_retry_period,
  node_sleepmode
};

#endif
