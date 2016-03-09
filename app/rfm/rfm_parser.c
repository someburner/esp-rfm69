#include "c_stdio.h"
#include "user_config.h"
#include "mem.h"
#include "../util/cbuff.h"

#include "driver/rfm69.h"
#include "radiohandler.h"
#include "rfm_parser.h"
#include "rfm.h"

#include "platform/status.h"

#include "../mqtt/mqtt_api.h"

#define EVENT_COUNT 2
#define MAX_LIVE_DATA_LISTENERS 1

static HCBUFF hrxBuff;

extern RadioStatus radioStatus;

typedef void (*parse_t)(uint32 stamp, uint8* buf, int len);

live_data_callback listeners[MAX_LIVE_DATA_LISTENERS];

void live_data_register_listener(live_data_callback f)
{
	NODE_DBG("RFM Add live data listener %p\n",f);
	int i=0;
	while (listeners[i]!=NULL) i++;

	if(i>=MAX_LIVE_DATA_LISTENERS)
		{ NODE_DBG("No more listener space %d\n",i); return; }

	NODE_DBG("Relay listener add index %d\n",i);
	listeners[i]=f;
}

static void live_data_notify_listeners(int x, int y, int z)
{
	int i;
	for(i=0;i<MAX_LIVE_DATA_LISTENERS;i++)
	{
		if(listeners[i]!=NULL)
			{ listeners[i](x, y, z); }
	}
}

static parse_t p_fn[EVENT_COUNT] = {
	live_event,				// 5 - 'l' - 0x6C - 108
	default_event			// 6 - else - 0x00
};

void process_rfm_msg(RFM_Handle * rfm, uint32 stamp)
{
	uint8_t msgBuff[61];
	uint8_t enumSelector = 0xff;
	char fn_type = 'z';
   unsigned i;
	unsigned int cbuffret;
	CBUFF dataIn;
	uint8_t pkt_len;

	hrxBuff = cbuffOpen(rfm->rxBuffNum);
	if (cbuffGetByte(hrxBuff,&dataIn) != CBUFF_GET_OK) goto parse_err;

	pkt_len = dataIn;
	RFM_DBG("pkt len = %d\n", pkt_len);

	/* Eat next 3 bytes (header data), keep 4th */
	// i = 2;
	// do {
	// 	if (cbuffGetByte(hrxBuff,&dataIn) != CBUFF_GET_OK) goto parse_err;
	// 	NODE_DBG("ate byte %02x\n", dataIn);
	// 	i--;
	// } while (i);
	// if (cbuffGetByte(hrxBuff,&dataIn) != CBUFF_GET_OK) goto parse_err;

	if (cbuffPeekHead(hrxBuff,&dataIn) == CBUFF_GET_OK) {
		fn_type = dataIn;
	}

	cbuffret = cbuffGetArray(hrxBuff, msgBuff, pkt_len-3);
	if (cbuffret == 0) goto parse_err;

	switch (fn_type)
	{
		case 'l':  //live data
		{
			p_fn[0](stamp, msgBuff, cbuffret);
		} break;
		default:
		{
			enumSelector = 0xff;
			p_fn[1](0, msgBuff, cbuffret);
    	} break;
	}

	radioState = radioIsConnected;


	cbuffret = cbuffGetFill(hrxBuff);
	if (cbuffret != 0)
	{
		NODE_DBG("cbuff not empty?? == %d\n", cbuffret);
		cbuffClearBuffer(hrxBuff);
	}


	rfm->rxBuffNum = cbuffClose(hrxBuff);
	return;

release:
	rfm->rxBuffNum = cbuffClose(hrxBuff);
	return;

parse_err:
	NODE_DBG("prcess_rfm_msg: cbuff err\n");
	goto release;

}

void live_event(uint32 stamp, uint8* buf, int len)
{
	uint8_t seq = len/6;
	unsigned i;
	int16_t x, y, z;
	for (i=0;i<seq;i++)
	{
		x = (int16_t)(buf[(i*6)]<<8 | buf[(i*6+1)]);
		y = (int16_t)(buf[(i*6+2)]<<8 | buf[(i*6+3)]);
		z = (int16_t)(buf[(i*6+4)]<<8 | buf[(i*6+5)]);
		live_data_notify_listeners(x, y, z);
		RFM_DBG("Seq %d: x=%d, y=%d, z=%d\n", i, x, y, z);
	}
}

void default_event(uint32 stamp, uint8* buf, int len)
{
	NODE_ERR("defaukt event\n");

	char lineBuff[128];
	unsigned int lineLen = 0;
	unsigned i = 0;

	lineLen = os_sprintf(lineBuff,"PKT[%u]===", stamp);
	if (lineLen > 0) console_write_string(lineBuff, lineLen);
	for (i=0; i<len; i++)
	{
		lineLen = os_sprintf(lineBuff,"| x%02x ",buf[i]);
		if (lineLen > 0) console_write_string(lineBuff, lineLen);
	}
	lineLen = os_sprintf(lineBuff,"|\r\n");
	if (lineLen > 0) console_write_string(lineBuff, lineLen);
	// NODE_DBG("Buffer Contents:\n");
	// unsigned i;
	//
	// for (i=0; i<len; i++) {
	// 	NODE_DBG("%02x ", buf[i]);
	// }
	// NODE_DBG("\n");
}
