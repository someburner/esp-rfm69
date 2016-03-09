#ifndef __RFM_H
#define __RFM_H

#include "c_types.h"
#include "osapi.h"
#include "ets_sys.h"
#include "user_interface.h"

#define RFM_OUTPUT_ALL			(1<<7)
#define RFM_PROMISCUOUS			(1<<6)

#define RF69_MAX_DATA_LEN       61

/* callback function (currently unused) */
typedef void (*RfmMsgCallback)(uint32_t *args);

void rfm_set_promiscuous(bool onoff);
void rfm_toggle_all_output(void);

/* Various init return codes */
typedef enum {
	RFM_INVALID,
	RFM_SPI_OK,
	RFM_INIT_OK,
	RFM_KEY_OK,
	RFM_GPIO_SET_INTR_ERR,
	RFM_GPIO_SET_ERR,
	RFM_SPI_ERR,
	RFM_INV_KEY_LEN,
	RFM_KEY_ERR
} rfm_retcode_t;

/* RFM69 driver operating modes */
typedef enum {
	RF69_OP_SLEEP,		// XTAL OFF
	RF69_OP_STANDBY,	// XTAL ON
	RF69_OP_SYNTH,		// PLL ON
	RF69_OP_RX,			// RX MODE
	RF69_OP_TX,			// TX MODE
	RF69_OP_NONE		// NONE
} RFM69_OP_MODE;

/* Simplifies accessing pkt headers */
typedef union {
	uint32 	data;
   struct {
		uint8 	CTLBYTE;
		uint8 	SENDERID;
		uint8 	TARGETID;
      uint8 	PAYLOADLEN;
   } get;
} RFM_HEADER_T;

/* App state */
typedef enum {
	RFM_IDLE,
	RFM_RX_BUFF_READY,
	RFM_TX_BEGIN,
	RFM_TX_WAIT,
	RFM_FIFO_WRITE,
	RFM_TX_SENT,
	RFM_ERROR
} RFM_APP_STATE;

/* OTA state */
typedef enum {
	RFM_OTA_IDLE,
	RFM_OTA_INIT,
	RFM_OTA_NEXT,
	RFM_OTA_LAST,
	RFM_OTA_DONE,
	RFM_OTA_ERROR
} RFM_OTA_STATE;

/* Driver structure
 * Contains almost everything needed by driver to operate            *
 * with the expection of rx buffer aka PAYLOAD which is shared        *
 * and located in RFM_Handle struct. OUTBUFF is present instead of    *
 * using txbuff to allow for send retries without having to roll back *
 * txbuff, as a retry is a fairly common occurence.                   */
typedef struct {
	RFM69_OP_MODE curr_mode;
	RFM69_OP_MODE next_mode; //TODO use this to impl. timers

	uint8_t DATALEN;
	uint8_t SENDERID;
	uint8_t TARGETID;     // should match ress
	uint8_t PAYLOADLEN;
	uint8_t ACK_REQUESTED;
	uint8_t ACK_RECEIVED; // should be polled immediately after sending a packet with ACK request
	int16_t RSSI;

	uint8_t OUTBUFF[66];  // Contains entire packet (data+headers)
	uint8_t OUTBUFFLEN;   // Length of entire packet, including payloadlen, so this
								 // should always be == OUTBUFF[0] + 1
} RFM69_DRIVER;

/* OTA structure
 * Contains OTA Update info, including SPIFFS object and              *
 * with the expection of rx buffer aka PAYLOAD which is shared        *
 * and located in RFM_Handle struct. OUTBUFF is present instead of    *
 * using txbuff to allow for send retries without having to roll back *
 * txbuff, as a retry is a fairly common occurence.                   */
typedef struct {
	uint8_t 					state;
	uint8_t 					outBuffFilled;
	uint8_t 					nodeid;
	uint8_t 					isEOF;
	uint8_t 					retries;
	uint16_t 				seq;
	uint16_t 				prevSeq;
	uint32_t					timeout;
	uint32_t 				fsPos;
	struct fs_file_st *  fs_st;
	uint8_t					fs_buff[256];
} RFM69_OTA_T;

/* Handle structure
 * Main data structure for driver  */
typedef struct {
	RFM_APP_STATE 		state;
	RFM69_DRIVER		driver;
	RFM69_OTA_T	*		ota;
	uint8_t 				nodeId;
	uint32_t 			options;
	//RfmFwCallback 	fwCb;
	RfmMsgCallback 	msgCb;
	os_timer_t 			tickTimer;
	uint32_t 			keepAliveTick;
	uint32_t 			sendTimeout;
	int 					error_msg;

	/* Cbuff refs */
	unsigned int		txBuffNum;
	unsigned int		rxBuffNum;
} RFM_Handle;


#endif
