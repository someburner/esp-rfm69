#include "osapi.h"
#include "sntp.h"
#include "user_interface.h"
#include "c_stdint.h"
#include "c_stddef.h"
#include "c_stdio.h"

#include "user_config.h"
#include "../util/cbuff.h"
#include "platform/flash_fs.h"

#include "driver/rfm69_register.h"
#include "driver/rfm69.h"

#include "rfm.h"
#include "rfm_parser.h"
#include "radiohandler.h"
#include "mem.h"

static RFM_Handle *  rfm_inst;
static ETSTimer      fwTimer;

static HCBUFF htxBuff;
static HCBUFF hrxBuff;

// Uncomment if you want to save received packets in the msg queue
// #define RFM_SAVE_PKTS        10

#define RFM_QUEUE_MAX        10
os_event_t RFM_TASK_QUEUE[RFM_TASK_QUEUE_SIZE];

struct rx_msg_head *rxheadp;
static int txCount, rxCount = 0;

int txTotal, rxTotal = 0;

static uint8_t FLXEOF[]  = { 0x46, 0x4C, 0x58, 0x3F, 0x45, 0x4F, 0x46 }; //FLX?EOF
static uint8_t FLXOK[]   = { 0x46, 0x4C, 0x58, 0x3F, 0x4F, 0x4B };       //FLX?OK
static uint8_t FLX[]     = { 0x46, 0x4C, 0x58, 0x3F };                   //FLX?
static uint8_t FLXHEAD[] = { 0x46, 0x4C, 0x58, 0x3A };                    //FLX:

RadioStatus radioStatus =
{
  0, //freq
  0, //rssi
  0, //voltage
  110, //axis
  0, //total test ct
};

char console_buf[BUF_MAX];
int console_wr, console_rd = 0;
int console_pos = 0; // offset since reset of buffer

static uint32 lastGoodStamp = 0;

uint8_t radioState = radioUnknown;

// static RfmMsgCallback rfm_callback;

static void rfm_callback(uint32_t *args)
{
	RFM_Handle* rfm = (RFM_Handle*)args;
	RFM_DBG("rfm_callback");
}

void generateRfmTxMsg(int toId, void* data, int len, bool requestAck, bool sendACK)
{
   /* Check Datalen */
	if ((data == NULL) || (len > 61)) { NODE_ERR("Invalid rfm msg\n"); return; }

   unsigned int dataInBuffer = 0;
   uint8_t ctl_byte = 0;

   /* Open CBuff for writing */
   htxBuff = cbuffOpen(rfm_inst->txBuffNum);

   dataInBuffer += cbuffPutByte(htxBuff, (uint8_t) (len+3));
   dataInBuffer += cbuffPutByte(htxBuff, (uint8_t) (toId));
   dataInBuffer += cbuffPutByte(htxBuff, (uint8_t) (rfm_inst->nodeId));
   if (sendACK)
      { ctl_byte = RFM69_CTL_SENDACK; }
   else if (requestAck)
      { ctl_byte = RFM69_CTL_REQACK; }
   dataInBuffer += cbuffPutByte(htxBuff, (uint8_t) (ctl_byte));

   /* Now copy payload, if there is one */
   if (len) { dataInBuffer += cbuffPutArray(htxBuff, data, len); }

   RFM_DBG("outbuff tot: %d\n", dataInBuffer);
   rfm_inst->txBuffNum = cbuffClose(htxBuff);

   #if 0
   NODE_DBG("outbuff:\n");
   int k;
   for(k=0;k<len+4;k++)
   {
      NODE_DBG("%02x ",newMsg->pkt.buff[k]);
   }
   NODE_DBG("\n");
   #endif

   rfm_inst->state = RFM_TX_BEGIN;
   system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm_inst);
}

static void RFM_Task(os_event_t *e)
{
   RFM_Handle* rfm = (RFM_Handle*)e->par;

   switch(rfm->state)
   {
      /* -------  Default / Do Nothing case --------
       * Used to reset/end a routine that is in-progress. By definition, *
       * any other case indicates a routine is in progress               */
      case RFM_IDLE: break;

      /* -------  RFM69 packet received --------
       * RFM69 driver interrupt has been handled and a packet has been   *
       * placed in rxBuff.                                               */
      case RFM_RX_BUFF_READY:
      {
         NODE_DBG("buf ready\n");

			rxTotal++;
			radioStatus.rssi = rfm->driver.RSSI;
         uint32 newStamp = sntp_get_current_timestamp();
         if (newStamp == 0)
         {
            if (lastGoodStamp != 0) { newStamp = lastGoodStamp; }
            else { newStamp = 1448437682; }
         }

         #ifdef RFM_SAVE_PKTS
         queuedMsg->stamp = newStamp;
         #endif

         if ((rfm->ota != NULL) && (rfm->ota->state >= RFM_OTA_INIT))
         {
            NODE_DBG("ota rx\n");
            os_timer_disarm(&fwTimer);
            process_rfm_ota_msg(rfm);
         } else {
            /* Check ACK_REQUESTED flag */
            if (rfm69_ACKRequested())
            {
               rfm->driver.ACK_REQUESTED = 0;
               uint8_t sender = rfm->driver.SENDERID;
               int16_t _RSSI = rfm->driver.RSSI; // save payload received RSSI value
               RFM_DBG("ACK rq'd. Sending ACK\n");
               generateRfmTxMsg(sender, "", 0, false, true);
            } else {
               rfm69_setMode(RF69_OP_RX);
            }
            process_rfm_msg(rfm, newStamp);
         }


         #ifdef RFM_SAVE_PKTS
         os_free(queuedMsg->pkt.buff);
         os_free(queuedMsg);
			STAILQ_REMOVE(&(rfm->rfm_msg_list), queuedMsg, rfm_msg_t, next);
         #endif
      } break;

      /* -------  Initiate TX --------
       * Use this to begin the Transmit process. Fills the driver OUTBUFF     *
       * and checks canSend() -> true, go to next state, else init CSMA timer *
       * Should only be called -after- a NEW message has been put in txBuff   *
       * If you need to resend a packet, use FIFO_WRITE state                 */
      case RFM_TX_BEGIN:
      {
         RFM_DBG("tx begin\n");
         CBUFF dataIn;
         unsigned int cbufret = 0;

         /* Grab first message in queue */
         htxBuff = cbuffOpen(rfm->txBuffNum);
         if (cbuffPeekTail(htxBuff, &dataIn) != CBUFF_GET_OK)
         {
            NODE_ERR("Invalid RFM69 msg\n");
            rfm->state = RFM_IDLE;
            return;
         }

         RFM_DBG("tx_begin pkt len = %d\n", dataIn);

         /* Fill outbuffer */
         rfm->driver.OUTBUFFLEN = dataIn+1;
         cbufret = cbuffGetArray(htxBuff, rfm->driver.OUTBUFF, dataIn+1);

         #if 0
         unsigned i;
         for (i=0; i<dataIn+1;i++)
            { NODE_DBG("%02x ", rfm->driver.OUTBUFF[i]); }
         NODE_DBG("\n");
         cbufret = cbuffGetFill(htxBuff);
         NODE_DBG("tx fill=%d\n", cbufret);
         #endif

         rfm->txBuffNum = cbuffClose(htxBuff);

         rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
         if (rfm69_canSend())
   		{
            rfm->state = RFM_FIFO_WRITE;
            system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm);
         }
         rfm->state = RFM_TX_WAIT;
         rfm->sendTimeout = system_get_time();
         os_timer_arm(&(rfm->tickTimer), 3, 0);
      } break;

      /* ------- TX wait --------
       * Do nothing case that allows us to wait while tickTimer checks RSSI   *
       * for activity or for CSMA expiration.                                 */
      case RFM_TX_WAIT: break;

      /* -------  FIFO Write - aka sendFrame --------
       * Writes whatever is in OUTBUFF to FIFO. RFM69 should be in a valid    *
       * TX state at this point, so make sure all wait conditions have been   *
       * met and that OUTBUFF/OUTBUFFLEN are correct                          */
      case RFM_FIFO_WRITE:
      {
         RFM_DBG("fifo write\n");
         rfm69_setMode(RF69_OP_STANDBY);

         while ((rfm69_readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);//wait modeready
         rfm69_writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00); // DIO0 is "Packet Sent"

         rfm69_writeToFIFO32(rfm->driver.OUTBUFF, rfm->driver.OUTBUFFLEN);
         rfm69_setMode(RF69_OP_TX);

         while ((rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00); // wait for ModeReady
         rfm69_setMode(RF69_OP_STANDBY);

         rfm->state = RFM_TX_SENT;
         // rfm->driver.OUTBUFFLEN = 0;

			txTotal++;
         system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm);
      } break;

      /* -------  TX Sent --------
       * Notifies that the packet was sent.       *
       * Safe to put into RX to listen for ACKs.  */
      case RFM_TX_SENT:
      {
         RFM_DBG("pkt sent\n");
         rfm69_setMode(RF69_OP_RX);
         rfm->state = RFM_IDLE;
         system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm);
      } break;

      /* -------  Error case --------
       * Stub for error notifications. Not really implemented currently, but  *
       * could be used to push errors to MQTT, HTTP, log to SPIFFS, etc       */
      case RFM_ERROR:
      {
         NODE_ERR("RFM error: %d\n", rfm->error_msg);
      } break;

   }
}

static void rfm69_timer_cb(void *arg)
{
   RFM_Handle* rfm = (RFM_Handle*)arg;
	os_timer_disarm(&(rfm->tickTimer));

	if (rfm->state == RFM_TX_WAIT)
	{
      RFM_DBG("fifo wait\n");
		if (((system_get_time() - rfm->sendTimeout) > RF69_CSMA_LIMIT_US) || (rfm69_canSend()))
		{
         if ((rfm->ota != NULL) && (rfm->ota->state >= RFM_OTA_INIT))
            { os_timer_disarm(&fwTimer); os_timer_arm(&fwTimer, 40, 0); }
	      rfm->state = RFM_FIFO_WRITE;
	      system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm);
		} else {
			if (rfm69_receiveDone())
            { RFM_DBG("rx1\n"); }
	      os_timer_arm(&(rfm->tickTimer), 20, 0);
			return;
	  }
	}

   if (rfm->state != RFM_FIFO_WRITE)
   {
      if (rfm69_receiveDone())
         { RFM_DBG("receiveDone!\n"); }
   }

	os_timer_arm(&rfm->tickTimer, 1, 0);
}

static void fwTimerCb(void *arg)
{
   if ((system_get_time() - rfm_inst->ota->timeout) < 4000000UL) {
      rfm_inst->ota->retries = 3;
      os_timer_disarm(&fwTimer);
      RFM_DBG("fwTimer cb\n");

      if (rfm_inst->ota->outBuffFilled)
      {
         rfm_inst->state =  RFM_FIFO_WRITE;
         system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfm_inst);
      }

      os_timer_arm(&fwTimer, 40, 0);

   } else {
      rfm_inst->ota->timeout = 0;
      NODE_DBG("fwTimer: Timeout limit reached. Aborting\n");
      os_timer_disarm(&fwTimer);
      rfm_inst->ota->state = RFM_OTA_IDLE;
   }
}

void process_rfm_ota_msg(RFM_Handle * r)
{
   RFM_DBG("proc_ota\n");
   CBUFF dataIn = 0;
   uint8_t buff[66];
   uint8_t pkt_len = 0;
   unsigned int cbuffret;

   /* Ack received - disable retry timers */
   // if (rx->ctlbyte & RFM69_CTL_SENDACK)
   //    { os_timer_disarm(&ackRequestedTimer); os_timer_disarm(&retrySendTimer); }

   hrxBuff = cbuffOpen(rfm_inst->rxBuffNum);
	if (cbuffGetByte(hrxBuff,&dataIn) != CBUFF_GET_OK) goto parse_err;

   pkt_len = dataIn - 3;
   NODE_DBG("proc pkt len = %d\n", pkt_len);

   cbuffret = cbuffGetArray(hrxBuff, buff, pkt_len);
	if (cbuffret == 0) goto parse_err;
   rfm_inst->rxBuffNum = cbuffClose(hrxBuff);

   #if 0
   unsigned i;
   for (i=0; i<cbuffret; i++)
      { NODE_DBG("%02x", buff[i]); }
   NODE_DBG("\n");
   #endif

   if (memcmp(buff, FLXOK, 6) == 0)
   {
      NODE_DBG("\tFLXOK rx\n");
      if (rfm_inst->ota->isEOF)
      {
         NODE_DBG("MOT > ESP : FLX?EOF OK! Flashing Done!\n");
         rfm_inst->ota->state = RFM_OTA_IDLE;
         rfm_end_ota();
         return;
      }

      rfm_inst->ota->timeout = system_get_time();
      os_timer_disarm(&fwTimer);
      rfm_inst->ota->prevSeq = 0;
      rfm_inst->ota->seq = 0;
      rfm_inst->ota->outBuffFilled = 0;

      rfm_inst->ota->state = RFM_OTA_NEXT;
      generate_next_OTA_msg();

   /* Check for FLX:xx: sequence response */
   } else if (memcmp(buff, FLXHEAD, 4) == 0) {
      char seqChars[5];
      uint8_t z = 0;
      for (uint8_t i=4; i < pkt_len-2; i++) {
         if ((buff[i] >= 48) && (buff[i] <= 57)) {
            seqChars[i-4] = buff[i];
            z++;
         }
      }
      seqChars[z] = '\0';
      uint16_t ackedSeq = (uint16_t) atoi(seqChars);

      if (rfm_inst->ota->seq == ackedSeq)
      {
         RFM_DBG("Acked Seq #: %u ---> MATCH\n", ackedSeq);
         rfm_inst->ota->timeout = system_get_time();
         rfm_inst->ota->prevSeq = rfm_inst->ota->seq;
         rfm_inst->ota->seq ++;
         generate_next_OTA_msg();
      } else {
         NODE_ERR("ERROR: Acked Seq #: %u, Expected #: %u\n", ackedSeq, rfm_inst->ota->seq);
      }
   } else {
      NODE_ERR("err\n");
      unsigned er;
      for (er=0;er<pkt_len; er++)
      {
         NODE_ERR("%02x ", buff[er]);
      }
      NODE_DBG("\n");
      rfm_inst->ota->state = RFM_OTA_NEXT;
   }

	return;

release:
   rfm_inst->rxBuffNum = cbuffClose(hrxBuff);
	return;

parse_err:
	NODE_DBG("process_rfm_ota: cbuff err\n");
	goto release;
}


void generate_next_OTA_msg()
{
   int curr_st = rfm_inst->ota->state;
   RFM_DBG("generate_next_OTA_msg: state %d\n", curr_st);

   switch (curr_st) {
      case RFM_OTA_INIT: {
         NODE_DBG("\tRFM_OTA_INIT\n");
         rfm_inst->ota->seq = 0;
         rfm_inst->ota->fsPos = 0;
         rfm_inst->ota->state = RFM_OTA_NEXT;
         generateRfmTxMsg(rfm_inst->ota->nodeid, FLX, 4, true, false);
      } break;

      case RFM_OTA_NEXT: {
         RFM_DBG("\tRFM_OTA_NEXT\n");
         rfm_inst->ota->outBuffFilled = 0;
         int res = -1;
         uint8_t  next_payload_len = 16;
         uint8_t  tempBuff[16];
			char numTemp[5];

         if ((rfm_inst->ota->seq)%16 == 0)
         {
            uint16_t  next_fs_len = 256;
            int fs_readbytes_rem = rfm_inst->ota->fs_st->size - rfm_inst->ota->fsPos;

            // if there is less than 256 bytes remaining...
            if (fs_readbytes_rem < next_fs_len)
            {
               next_fs_len = fs_readbytes_rem;
               NODE_DBG("next_fs_len = %d\n", next_fs_len);
               memset(rfm_inst->ota->fs_buff, 0, 256); //0 out our buffer
            }

            /* Read data into buffer */
            res = fs_read_file(rfm_inst->ota->fs_st, rfm_inst->ota->fs_buff, next_fs_len, rfm_inst->ota->fsPos);
            if (res < 0)
            {
               NODE_ERR("fs read err!\n");
               rfm_inst->ota->state = RFM_OTA_IDLE;
               return;
            }

            RFM_DBG("seq[%d] -> fs_read res = %d\n", rfm_inst->ota->seq, res);
            rfm_inst->ota->fsPos += res;
         }

			memcpy(tempBuff, FLXHEAD, 4);
			int numLen = os_sprintf(numTemp, "%u", rfm_inst->ota->seq);
	    	for (int i=0; i<numLen; i++)
			{
	      	tempBuff[4+i] = (uint8_t) numTemp[i];
	    	}
	    	tempBuff[4+numLen] = 0x3A; //set next char to ':'
          uint8_t headerLen = 4+numLen+1; //set update pointer

         /* fs_buffer divided into 16 parts of 16 bytes each, for ex:
          * seq  0: offset = (0%16)*16 = 0    | seq  1: offset = (1%16)*16 = 16
          * seq 15: offset = (15%16)*16 = 240 | seq  16: offset = (16%16)*16 = 0
          * seq 17: offset = (17%16)*16 = 16  | seq  18: offset = (18%16)*16 = 32
          * ... And so on.
         */
         uint8_t buff_offset = (uint8_t) ((rfm_inst->ota->seq)%16)*16;
         RFM_DBG("buff_offset = %d, headerLen = %d\n", buff_offset, headerLen);

         // if we're in the last buffer sequence..
         if (rfm_inst->ota->seq == (rfm_inst->ota->fs_st->size /16))
         {
            int tot_payload_rem = rfm_inst->ota->fs_st->size - ((rfm_inst->ota->seq)*16);
            if (tot_payload_rem%16 != 0)
            {
               // next_payload_len = tot_payload_rem%16; //truncate next_payload_len
               next_payload_len = tot_payload_rem;

               rfm_inst->ota->isEOF = true;
            }
         }

         RFM_DBG("next_payload_len = %d\n", next_payload_len);
         memcpy(tempBuff+headerLen, rfm_inst->ota->fs_buff + buff_offset, next_payload_len);

         if (rfm_inst->ota->isEOF)
         {
            NODE_DBG("FW EOF Detected.\n");
            rfm_inst->ota->state = RFM_OTA_LAST;
         }

         generateRfmTxMsg(rfm_inst->ota->nodeid, tempBuff, next_payload_len+headerLen, true, false);
         rfm_inst->ota->outBuffFilled = 1;
         rfm_inst->ota->retries = 3;

         /* reset fw timeout */
         rfm_inst->ota->timeout = system_get_time();
         os_timer_disarm(&fwTimer);
			os_timer_setfn(&fwTimer, fwTimerCb, NULL);
			os_timer_arm(&fwTimer, 60, 0);
      } break;

      case RFM_OTA_LAST: {
         NODE_DBG("... Done! Seq = %d\n", rfm_inst->ota->seq);
         rfm_inst->ota->seq = 0;
         rfm_inst->ota->prevSeq = 0;
         rfm_inst->ota->retries = 0;
         rfm_inst->ota->state = RFM_OTA_DONE;
         os_timer_disarm(&fwTimer);

         generateRfmTxMsg(rfm_inst->ota->nodeid, FLXEOF, 7, true, false);
      } break;

      case RFM_OTA_DONE: {
         rfm_inst->ota->state = RFM_OTA_IDLE;
         rfm_inst->state = RFM_IDLE;
      } break;
   }
}

void rfm_end_ota()
{
   NODE_DBG("OTA Complete. Cleaning up\n");
   rfm_inst->state = RFM_IDLE;
   if (rfm_inst->ota != NULL)
   {
      os_free(rfm_inst->ota);
   }
}

void rfm_begin_ota(char * binName, uint8_t nid)
{
   struct fs_file_st * fs_st = NULL;
   int res = -1;
   NODE_DBG("rfm_begin_ota\n");
   if (binName == NULL) {
      NODE_ERR("invalid name\n");
      return;
   } else if ((nid > 255) || (nid == 0)) {
      NODE_ERR("invalid node id\n");
      return;
   }

   /* Make sure file exists */
   res = fs_exists(binName, &fs_st);

   if (res < 0) { NODE_ERR("File DNE!? Aborting. \n"); return; }

   rfm_inst->ota = (RFM69_OTA_T *)os_zalloc(sizeof(RFM69_OTA_T));
   rfm_inst->ota->state = RFM_OTA_IDLE;

   // memset(&(rfm_inst->ota), 0, sizeof(RFM69_OTA_T));

   rfm_inst->ota->fs_st = fs_st;
   rfm_inst->ota->state = RFM_OTA_INIT;    //set ota struct status to init
   rfm_inst->ota->nodeid = nid;

   generate_next_OTA_msg();
}

void rfm_toggle_all_output(void)
{
   static uint8_t toggle = 1;
   if (toggle)
   {
      rfm_inst->options |= RFM_OUTPUT_ALL;
      toggle = 0;
   }
   else
   {
      rfm_inst->options &= ~RFM_OUTPUT_ALL;
      toggle = 1;
   }
}

void rfm_set_promiscuous(bool on)
{
   if (on) { rfm_inst->options |= RFM_PROMISCUOUS; }
   else { rfm_inst->options &= ~(RFM_PROMISCUOUS); }
}

void console_write(char c)
{
	console_buf[console_wr] = c;
	console_wr = (console_wr+1) % BUF_MAX;
	if (console_wr == console_rd)
	{
		// full, we write anyway and loose the oldest char
		console_rd = (console_rd+1) % BUF_MAX; // full, eat first char
		console_pos++;
	}
}

void console_write_char(char c)
{
	console_write(c);
}

void console_write_string(char* buff, int len)
{
	int i;
	for (i=0; i < len; i++)
	{
		console_write_char(buff[i]);
	}
}

void radioHandlerInit(RFM_Handle * r)
{
   rfm_inst = r;
   rfm_inst->state = RFM_IDLE;
   // rfm_inst->msgCb = rfm_callback;
   rfm_inst->ota = NULL;

	radioState = radioUnknown;

	os_timer_disarm(&rfm_inst->tickTimer);
	os_timer_setfn(&rfm_inst->tickTimer, (os_timer_func_t *)rfm69_timer_cb, (void *)rfm_inst);
	os_timer_arm(&rfm_inst->tickTimer, 1000, 1);

   os_timer_disarm(&fwTimer);
   os_timer_setfn(&fwTimer, (os_timer_func_t *)fwTimerCb, (void *)rfm_inst);
   // os_timer_arm(&fwTimer, 1000, 0);

	system_os_task(RFM_Task, RFM_TASK_PRIO, RFM_TASK_QUEUE, RFM_TASK_QUEUE_SIZE);
}
