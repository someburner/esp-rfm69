#include "user_interface.h"
#include "c_types.h"
#include "c_stdio.h"
#include "mem.h"
#include "osapi.h"

#include "user_config.h"
#include "platform/config.h"
#include "driver/rfm69.h"
#include "radiohandler.h"
#include "util/cbuff.h"

/* RX Buffer Size - 128 == 2+ packets */
#define RFM_RX_BUFFSIZE        128

/* TX Buffer Size - 256 == 4+ packets */
#define RFM_TX_BUFFSIZE        256

static RFM_Handle rfm_handle_base;

CBUFFOBJ    rxBuffObj;
CBUFF       rxBuff[RFM_RX_BUFFSIZE];

CBUFFOBJ    txBuffObj;
CBUFF       txBuff[RFM_TX_BUFFSIZE];

bool init_rfm_handler()
{
	int ret = 0;
	RFM_Handle * rfm_ptr = &rfm_handle_base;
	cbuffInit();

	memset(rfm_ptr, 0, sizeof(rfm_handle_base));

	rfm_handle_base.rxBuffNum = cbuffCreate(rxBuff, RFM_RX_BUFFSIZE, &rxBuffObj);
	rfm_handle_base.txBuffNum = cbuffCreate(txBuff, RFM_TX_BUFFSIZE, &txBuffObj);

	if (rfm_handle_base.rxBuffNum == 0 || rfm_handle_base.txBuffNum == 0) {
		NODE_ERR("Error creating rfm cbuff(s)\n");
	} else {
		NODE_DBG("cbuff tx Id=%d, rx = %d\n", rfm_handle_base.txBuffNum, rfm_handle_base.rxBuffNum);
	}

	uint8_t thisNodeId, thisNetId;
	RFM_CONF_T dummy;
	RFM_CONF_T * rfmConfPtr = &dummy;
	ret = get_config_ptr((void *)rfmConfPtr, rfm_conf_type);
	if (ret > 0)
	{
		NODE_DBG("Loaded rfm info: Id=%d, netId = %d\n", rfmConfPtr->bridgeId, rfmConfPtr->netId);
		thisNodeId = rfmConfPtr->bridgeId;
		thisNetId = rfmConfPtr->netId;
	} else {
		thisNodeId = RFM69_NODE_ID;
		thisNetId = RFM69_NET_ID;
		NODE_DBG("Error loading rfm.conf\n");
	}

	// rfm_handle_base.msgCb = rfm_callback;
	rfm_handle_base.nodeId = thisNodeId;
	rfm_handle_base.msgCb = NULL;
	rfm_handle_base.state = RFM_IDLE;
	rfm_handle_base.keepAliveTick = 20; //20 ms default
	rfm_handle_base.sendTimeout = 1000; //1 sec default
	rfm_handle_base.options = RFM_OUTPUT_ALL;

	ret = rfm69_spi_init();
   if (ret == RFM_SPI_OK)
   {
      NODE_DBG("RFM69 SPI Initialized\n");
		ret = rfm69_init(rfm_ptr, thisNodeId, thisNetId);
      if (ret == RFM_INIT_OK)
      {
         NODE_DBG("RFM69 listening on address %d...\n", thisNodeId);
			radioHandlerInit(rfm_ptr);
			return true;
		}
	}

	NODE_DBG("Error: RFM init returned %d\n", ret);
	return false;
}
