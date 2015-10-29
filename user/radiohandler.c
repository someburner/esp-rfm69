#include <esp8266.h>
#include "c_types.h"
#include "user_interface.h"
#include "osapi.h"
#include "cgiradio.h"
#include "queue.h"
#include "config.h"
#include "radiohandler.h"
#include "console.h"
#include "config.h"

#define MOT_FW_ADDR   (FIRMWARE_SIZE - 2*ATMEGA_FLASH_MAX)

#define radioTaskPrio        1
#define radioTaskQueueLen    10

#define TX_QUEUE_LEN         3
#define RX_QUEUE_LEN         3
os_event_t radioTaskQueue[radioTaskQueueLen];

STAILQ_HEAD(rx_msg_head, radio_msg_st) rx_msg_list = STAILQ_HEAD_INITIALIZER(rx_msg_list);
struct rx_msg_head *rxheadp;
static int txCount, rxCount;
extern FwHeader fwHeader;


uint8_t FLXEOF[]  = { 0x46, 0x4C, 0x58, 0x3F, 0x45, 0x4F, 0x46 }; //FLX?EOF
uint8_t FLXOK[]   = { 0x46, 0x4C, 0x58, 0x3F, 0x4F, 0x4B };       //FLX?OK
uint8_t FLX[]     = { 0x46, 0x4C, 0x58, 0x3F };                   //FLX?
uint8_t FLXHEAD[] = { 0x46, 0x4C, 0x58, 0x3A};                    //FLX:

/* Timers */
static ETSTimer ackRequestedTimer;
static ETSTimer retrySendTimer;
static ETSTimer canSendTimer;
static ETSTimer rfmFwTimeoutTimer;

/* FW OTA Variables */
static uint16_t seq         = 0; //current sequence # of FW OTA
static uint16_t prevSeq     = 0; //previous sequence # of FW OTA
static uint8_t isEOF        = false;

static uint8_t ackRequestedOnTimer = false;
static uint32 canSendT1   = 0; //canSend timer. Expires after 1000ms
static uint32 retryWaitT1 = 0; //retry timer. Set between 20-50 ms
static uint32 fwWaitT1    = 0; //timer to nix fw update at. Set to 4000ms
static uint8_t retries    = 0; //set this before posting sendWithRetry
static uint8_t ackNodeId  = 0; //set this to choose what node to ACK

extern RadioStatus radioStatus; //external reference to cgiRadioStatus
extern ETSTimer peakCheckTimer;
static uint8_t handlerState;

void ICACHE_FLASH_ATTR process_rfm_msg() {
}

void ICACHE_FLASH_ATTR radioMsgTxPush(uint8_t id, uint8_t *buf, short len, bool requestACK, bool sendACK)
{
	if (len != 0)
	{
		/* Create new radio_msg struct and add to tx queue */
		struct radio_msg_st *msg = NULL;
		msg = (struct radio_msg_st *)os_zalloc(sizeof(struct radio_msg_st));
		msg->buff = (uint8_t*)os_zalloc(len);
		msg->buffSize = len;
		msg->nodeId = id;
		msg->ctlbyte = 0;
		if (sendACK)
			{ msg->ctlbyte = RFM69_CTL_SENDACK; }
		else if (requestACK)
			{ msg->ctlbyte = RFM69_CTL_REQACK; }

		if (os_memcpy(msg->buff, buf, len) != 0)
			{ STAILQ_INSERT_HEAD(&tx_msg_list, msg, next); txCount++; }

		#ifdef RFM_DBG
		for (uint8_t i=0; i<msg->buffSize; i++ ) { os_printf("%02x", msg->buff[i]); }
		os_printf("\n");
		#endif
	}

	/* Clean up old Tx messages */
	if (txCount > TX_QUEUE_LEN)
	{
		struct radio_msg_st *np = STAILQ_LAST(&tx_msg_list, radio_msg_st , next);
		uint8_t* npBuff = np->buff;
		os_free(npBuff);
		STAILQ_REMOVE(&tx_msg_list, np, radio_msg_st, next);
		os_free(np);
		txCount--;
	}
}

void ICACHE_FLASH_ATTR radioMsgRxPush(uint8_t id, uint8_t *buf, short len, uint8_t ctl)
{
	if (len != 0)
	{
		/* Create new radio_msg struct and add to rx queue */
		struct radio_msg_st *msg = NULL;
		msg = (struct radio_msg_st *)os_zalloc(sizeof(struct radio_msg_st));
		msg->buff = (uint8_t*)os_zalloc(len);
		msg->buffSize = len;
		msg->nodeId = id;
		msg->ctlbyte = ctl;
		if (os_memcpy(msg->buff, buf, len) != 0) {
			STAILQ_INSERT_HEAD(&rx_msg_list, msg, next);
			rxCount++;
		}
	}

	/* Clean up old Rx messages */
	if (rxCount > RX_QUEUE_LEN)
	{
		struct radio_msg_st *np = STAILQ_LAST(&rx_msg_list, radio_msg_st , next);
		uint8_t* npBuff = np->buff;
		os_free(npBuff);
		STAILQ_REMOVE(&rx_msg_list, np, radio_msg_st, next);
		os_free(np);
		rxCount--;
	}
}

void ICACHE_FLASH_ATTR createRadioTask(uint8_t type, uint8_t arg)
{
	uint32_t task = 0;
	task = (uint32_t) type;
	task |= ((uint32_t)arg) << 8;
	system_os_post(radioTaskPrio, 0, task);
}

void ICACHE_FLASH_ATTR rfmFwTimeoutTimerCb(void *v)
{
  if ((system_get_time() - fwWaitT1) < 4000000UL) {
		retries = 3;
    os_timer_disarm(&rfmFwTimeoutTimer);
		os_timer_arm(&rfmFwTimeoutTimer, 80, 0);
		createRadioTask(rfm_sendWithRetry, rfm_request_ack);
	} else {
    fwWaitT1 = 0;
    os_printf("rfmFwTimeoutTimer: Timeout limit reached. Aborting\n");
    os_timer_disarm(&rfmFwTimeoutTimer);
		handlerState = rfm_standby_state;
	}
}

static void ICACHE_FLASH_ATTR radioTask(os_event_t *events)
{
	uint32 params = events->par;
	uint8_t eventType = (uint8_t) (params & 0x000000FF);
	uint8_t param = (uint8_t) ((params>>8) & 0x000000FF);
	if ((eventType == rfm_handle_data) && (handlerState == rfm_fw_started))
	{
		eventType = rfm_handle_fw_data;
	}
	switch(eventType) {
		/* Handler for regular data*/
		case rfm_handle_data:
		{
			struct radio_msg_st *rx2 = STAILQ_FIRST(&rx_msg_list);

			if ((rx2 == NULL) || (rxCount == 0)) /* make sure we have something */
				{ os_printf(" ERROR: No rx buffers found\n"); return; }

			if (rx2->ctlbyte & RFM69_CTL_REQACK) {
				radioMsgTxPush(3, (uint8_t*)"ACK", 3, false, true);
				createRadioTask(rfm_sendFrame, rfm_event_default);
			}

			if (rx2->ctlbyte & RFM69_CTL_SENDACK)
				{ os_printf("- Ack\n"); }

			//use this to filter data
			if ((rx2->buff[0] == 0x70) || (rx2->buff[0] == 0x75) || (rx2->buff[0] == 0x72) || (rx2->buff[0] == 0x76))
				{ process_rfm_msg(); } 
			else
				{ console_write_string("Data received: \"", 16);
					for (int i=0; i<rx2->buffSize;i++) {
						console_write_char(rx2->buff[i]);
					}
					console_write_string("\"\n", 2); }
				}
		break;

		/* Handler for firmware data*/
		case rfm_handle_fw_data:
		{
			/* make sure we have something */
      	struct radio_msg_st *rx = STAILQ_FIRST(&rx_msg_list);
	      if (rx == NULL) { os_printf(" ERROR: No rx buffers found\n"); return; }

			/* Ack received - disable retry timers */
			if (rx->ctlbyte & RFM69_CTL_SENDACK)
				{ os_timer_disarm(&ackRequestedTimer); os_timer_disarm(&retrySendTimer); }

			/* Check for FLX?OK handshake */
			if (ets_memcmp(rx->buff, FLXOK, 6) == 0)
			{
        		if (isEOF)
				{
					os_timer_disarm(&ackRequestedTimer);
					os_timer_disarm(&retrySendTimer);
					os_timer_disarm(&rfmFwTimeoutTimer);
					os_timer_disarm(&canSendTimer);
					#ifdef RFM_DBG
					os_printf("MOT > ESP : FLX?EOF OK! Flashing Done!\n");
					#endif
					handlerState = rfm_standby_state;
					break;
				}

				fwWaitT1 = system_get_time();
        		os_timer_disarm(&rfmFwTimeoutTimer);
				prevSeq = 0;
				seq = 0;
				createRadioTask(rfm_fw_cont, rfm_fw_0);

			/* Check for FLX:xx: sequence response */
			} else if (ets_memcmp(rx->buff, FLXHEAD, 4) == 0) {
				char seqChars[5]; uint8_t z = 0;
				for (uint8_t i=4; i < rx->buffSize-2; i++) {
					if ((rx->buff[i] >= 48) && (rx->buff[i] <= 57)) {
    					seqChars[i-4] = rx->buff[i]; z++;
					}
				}
  				seqChars[z] = '\0';
				uint16_t ackedSeq = (uint16_t) atol(seqChars);

				if (seq == ackedSeq)
				{
					#ifdef RFM_DBG
					os_printf("Acked Seq #: %u ---> MATCH\n", ackedSeq);
					#endif
					fwWaitT1 = system_get_time();
					prevSeq = seq;
					seq++;
					createRadioTask(rfm_fw_cont, rfm_event_default);
				} else {
					os_printf("ERROR: Acked Seq #: %u, Expected #: %u\n", ackedSeq, prevSeq);
				}
			} else {
				os_printf("unknown rx type\n");
				handlerState = rfm_standby_state;
			}
		}
		break;
		case rfm_fw_cont:
		{
			uint8_t tempBuff[16];
			uint32 temp32[16];
			char numTemp[5];
			/* Check if this is seq 0 */
			if (param == rfm_fw_0)
			{
				seq = 0;
			}

			os_memcpy(tempBuff, FLXHEAD, 4);
			int numLen = os_sprintf(numTemp, "%u", seq);
	    	for (int i=0; i<numLen; i++)
			{
	      	tempBuff[4+i] = (uint8_t) numTemp[i];
	    	}
	    	tempBuff[4+numLen] = 0x3A; //set next char to ':'
	    	int headerLen = 4+numLen+1; //set update pointer
			uint32 addr = MOT_FW_ADDR + (16*seq);
			spi_flash_read(addr, (uint32*)temp32, 16);
			for (int i=0;i<16;i++) { tempBuff[i+headerLen] = ((uint8_t*)temp32)[i]; }

			if (((seq)*16) > fwHeader.len)
			{
				isEOF = true;
				os_printf("FW EOF Detected. seqLen = %d\n", seq);
				createRadioTask(rfm_fw_end, rfm_event_default);
				break;
			}
			if (seq%16 == 0)
			{
 				os_printf("seq[%d] -> %02x - %02x\n", seq, tempBuff[headerLen], tempBuff[15+headerLen]);
			}

			radioMsgTxPush(3, tempBuff, 16+headerLen, true, false);
			retries = 3;
			createRadioTask(rfm_sendWithRetry, rfm_event_default);

			/* reset fw timeout */
			fwWaitT1 = system_get_time();
			os_timer_disarm(&rfmFwTimeoutTimer);
			os_timer_setfn(&rfmFwTimeoutTimer, rfmFwTimeoutTimerCb, NULL);
			os_timer_arm(&rfmFwTimeoutTimer, 120, 0);
		}
		break;
		case rfm_fw_begin:
		{
			if (handlerState == rfm_fw_started)
			{ os_printf("FW OTA process already running.\n"); break;
			} else if (fwHeader.len == 0) { os_printf("No firmware loaded!\n");
			} else {
				handlerState = rfm_fw_started;
				os_printf("RFM69-ATMega OTA: %u bytes\n", fwHeader.len);
				isEOF = false;
				retries = 0;
				seq = 0;
				prevSeq = 0;
				radioMsgTxPush(3, FLX, 4, true, false);
				createRadioTask(rfm_sendWithRetry, rfm_event_default);
			}
		}
		break;
		case rfm_fw_end:
		{
      	os_printf("... Done! Seq = %d\n", seq);
      	retries = 0;
			seq = 0;
			prevSeq = 0;
      	radioMsgTxPush(3, FLXEOF, 7, true, false);
			createRadioTask(rfm_sendWithRetry, rfm_event_default);
			// handlerState = rfm_standby_state;
		}
		break;
	  	case rfm_send:
		{
      rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
      if (rfm69_canSend())
		{
			createRadioTask(rfm_sendFrame, rfm_event_default);
      }
		else
		{
			if (param == rfm_request_ack)
				{ ackRequestedOnTimer = true; }
			else
				{ ackRequestedOnTimer = false; }
     		canSendT1 = system_get_time();
         os_timer_disarm(&canSendTimer);
         os_timer_arm(&canSendTimer, 1, 0);
      	}
		}
		break;
    	case rfm_sendWithRetry:
		{
      	retryWaitT1 = system_get_time();
			createRadioTask(rfm_send, rfm_event_default);
			retries = 0;
			os_timer_disarm(&retrySendTimer);
			os_timer_arm(&retrySendTimer, 20, 0);
			ackNodeId = 3;
      	os_timer_disarm(&ackRequestedTimer);
      	os_timer_arm(&ackRequestedTimer, 1, 0);
    	}
		break;
    	case rfm_sendFrame:
		{
			rfm69_sendFrame();
    	}
		break;
  	}
}

static void ICACHE_FLASH_ATTR canSendTimerCb(void *v)
{
	if (((system_get_time() - canSendT1) > RF69_CSMA_LIMIT_US) || (rfm69_canSend()))
	{
		createRadioTask(rfm_sendFrame, rfm_event_default);
		os_timer_disarm(&canSendTimer);
	} else {
		rfm69_receiveDone();
		os_timer_disarm(&canSendTimer);
		os_timer_arm(&canSendTimer, 1, 0);
  }
}

static void ICACHE_FLASH_ATTR retrySendTimerCb(void *v)
{
	if (retries > 0)
	{
		retries--;
		#ifdef RFM_DBG
		os_printf("retrying send\n");
		#endif
		os_timer_disarm(&retrySendTimer);
		os_timer_arm(&retrySendTimer, 40, 0); // 40ms roundtrip req for 61byte packets
		createRadioTask(rfm_sendFrame, rfm_event_default);
	}
	else { os_timer_disarm(&retrySendTimer); }
}

static void ICACHE_FLASH_ATTR ackRequestedTimerCb(void *v)
{
	if (rfm69_ACKReceived(ackNodeId))
	{
		retries = 0;
		ackRequestedOnTimer = false;
		os_timer_disarm(&ackRequestedTimer);
		os_timer_disarm(&retrySendTimer);
	}
}

void ICACHE_FLASH_ATTR disableSendTimers()
{
	os_timer_disarm(&ackRequestedTimer);
	os_timer_disarm(&retrySendTimer);
}

void ICACHE_FLASH_ATTR radioHandlerInit()
{
	handlerState = rfm_inititial_state;
	STAILQ_INIT(&rx_msg_list);

	os_timer_disarm(&rfmFwTimeoutTimer);
	os_timer_setfn(&rfmFwTimeoutTimer, rfmFwTimeoutTimerCb, NULL);

	os_timer_disarm(&retrySendTimer);
	os_timer_setfn(&retrySendTimer, retrySendTimerCb, NULL);

	os_timer_disarm(&ackRequestedTimer);
	os_timer_setfn(&ackRequestedTimer, ackRequestedTimerCb, NULL);

	os_timer_disarm(&canSendTimer);
	os_timer_setfn(&canSendTimer, canSendTimerCb, NULL);

	system_os_task(radioTask, radioTaskPrio, radioTaskQueue, radioTaskQueueLen);
	handlerState = rfm_standby_state;
}
