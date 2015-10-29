#include "driver/rfm69.h"
#include "driver/gpio16.h"
#include "queue.h"

#define SPI_DEV HSPI
#define RFM_INTR_PIN         1
#define radioTaskPrio        1

extern uint8_t pin_num[GPIO_PIN_NUM];

const char *gpio_type_desc[] =
{
  "GPIO (DISABLE INTERRUPT)",
  "GPIO (UP)",
  "GPIO (DOWN)",
  "GPIO (BOTH)",
  "GPIO (LOW LEVEL)",
  "GPIO (HIGH LEVEL)"
};

const uint32_t MASK32_BIT0 = 0xFF000000;
const uint32_t MASK32_BIT1 = 0x00FF0000;
const uint32_t MASK32_BIT2 = 0x0000FF00;
const uint32_t MASK32_BIT3 = 0x000000FF;

static ETSTimer receiveDoneTimer;

static uint8_t _mode;
static uint8_t _isRFM69HW = RFM69_IS_HW;
static uint8_t _address = RFM69_NODE_ID;
static uint8_t _powerLevel = 31;
static char* _key = RFM69_ENCRYPT_KEY;

static uint8_t DATA[RF69_MAX_DATA_LEN];
static uint8_t DATALEN;
static uint8_t SENDERID;
static uint8_t TARGETID;     // should match ress
static uint8_t PAYLOADLEN;
static uint8_t ACK_REQUESTED;
static uint8_t ACK_RECEIVED; // should be polled immediately after sending a packet with ACK request
static int16_t RSSI;

static bool _promiscuousMode = false;

const uint8_t CONFIG[][2] =
{
   { REG_OPMODE, RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY }, /* 0x01 */
   { REG_DATAMODUL, RF_DATAMODUL_DATAMODE_PACKET | RF_DATAMODUL_MODULATIONTYPE_FSK | RF_DATAMODUL_MODULATIONSHAPING_00 },/* 0x02 */ // no shaping
   { REG_BITRATEMSB, RF_BITRATEMSB_55555}, /* 0x03 */ // default: 4.8 KBPS
   { REG_BITRATELSB, RF_BITRATELSB_55555}, /* 0x04 */
   { REG_FDEVMSB, RF_FDEVMSB_50000}, /* 0x05 */  // default: 5KHz, (FDEV + BitRate / 2 <= 500KHz)
   { REG_FDEVLSB, RF_FDEVLSB_50000}, /* 0x06 */
   { REG_FRFMSB, (uint8_t) (RFM69_FREQ==RF69_315MHZ ? RF_FRFMSB_315 : (RFM69_FREQ==RF69_433MHZ ? RF_FRFMSB_433 : (RFM69_FREQ==RF69_868MHZ ? RF_FRFMSB_868 : RF_FRFMSB_915))) }, /* 0x07 */
   { REG_FRFMID, (uint8_t) (RFM69_FREQ==RF69_315MHZ ? RF_FRFMID_315 : (RFM69_FREQ==RF69_433MHZ ? RF_FRFMID_433 : (RFM69_FREQ==RF69_868MHZ ? RF_FRFMID_868 : RF_FRFMID_915))) }, /* 0x08 */
   { REG_FRFLSB, (uint8_t) (RFM69_FREQ==RF69_315MHZ ? RF_FRFLSB_315 : (RFM69_FREQ==RF69_433MHZ ? RF_FRFLSB_433 : (RFM69_FREQ==RF69_868MHZ ? RF_FRFLSB_868 : RF_FRFLSB_915))) }, /* 0x09 */
   { REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_2 }, /* 0x19 */ // (BitRate < 2 * RxBw)
   { REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01 }, /* 0x25 */  // DIO0 is the only IRQ we're using
   { REG_DIOMAPPING2, RF_DIOMAPPING2_CLKOUT_OFF },// | RF_DIOMAPPING2_DIO5_11 }, /* 0x26 */// DIO5 ClkOut disable for power saving
   { REG_IRQFLAGS2, RF_IRQFLAGS2_FIFOOVERRUN },/* 0x28 */ // writing to this bit ensures that the FIFO & status flags are reset
   { REG_RSSITHRESH, 220 }, /* 0x29 */// must be set to dBm = (-Sensitivity / 2), default is 0xE4 = 228 so -114dBm
   { REG_SYNCCONFIG, RF_SYNC_ON | RF_SYNC_FIFOFILL_AUTO | RF_SYNC_SIZE_2 | RF_SYNC_TOL_0 },/* 0x2E */
   { REG_SYNCVALUE1, 0x2D },  /* 0x2F */    // attempt to make this compatible with sync1 byte of RFM12B lib
   { REG_SYNCVALUE2, RFM69_NET_ID }, /* 0x30 */ // NETWORK ID
   { REG_PACKETCONFIG1, RF_PACKET1_FORMAT_VARIABLE | RF_PACKET1_DCFREE_OFF | RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON | RF_PACKET1_ADRSFILTERING_OFF },/* 0x37 */
   { REG_PAYLOADLENGTH, 66 },/* 0x38 */ // in variable length mode: the max frame size, not used in TX
   { REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTART_FIFONOTEMPTY | RF_FIFOTHRESH_VALUE },/* 0x3C */  // TX on FIFO not empty
   { REG_PACKETCONFIG2, RF_PACKET2_RXRESTARTDELAY_2BITS | RF_PACKET2_AUTORXRESTART_ON | RF_PACKET2_AES_OFF },/* 0x3D */ // RXRESTARTDELAY must match transmitter PA ramp-down time (bitrate dependent)
   { REG_TESTDAGC, RF_DAGC_IMPROVED_LOWBETA0 },/* 0x6F */  // run DAGC continuously in RX mode for Fading Margin Improvement, recommended default for AfcLowBetaOn=0
   // looks like PA1 and PA2 are not implemented on RFM69W, hence the max output power is 13dBm
   // +17dBm and +20dBm are possible on RFM69HW
   // +13dBm formula: Pout = -18 + OutputPower (with PA0 or PA1**)
   // +17dBm formula: Pout = -14 + OutputPower (with PA1 and PA2)**
   // +20dBm formula: Pout = -11 + OutputPower (with PA1 and PA2)** and high power PA settings (section 3.3.7 in datasheet)
   // { REG_PALEVEL, RF_PALEVEL_PA0_ON | RF_PALEVEL_PA1_OFF | RF_PALEVEL_PA2_OFF | RF_PALEVEL_OUTPUTPOWER_11111},/* 0x11 */
   // { REG_OCP, RF_OCP_ON | RF_OCP_TRIM_95 }, // over current protection (default is 95mA)/* 0x13 */
   // RXBW defaults are { REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_24 | RF_RXBW_EXP_5} (RxBw: 10.4KHz)
   //for BR-19200: /* 0x19 */ { REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_24 | RF_RXBW_EXP_3 },
   //{ REG_PREAMBLELSB, RF_PREAMBLESIZE_LSB_VALUE } /* 0x2D */ // default 3 preamble bytes 0xAAAAAA
   //{ REG_NODEADRS, nodeID }, /* 0x39 */// turned off because we're not using address filtering
   //for BR-19200: { REG_PACKETCONFIG2, RF_PACKET2_RXRESTARTDELAY_NONE | RF_PACKET2_AUTORXRESTART_ON | RF_PACKET2_AES_OFF }, /* 0x3D */ // RXRESTARTDELAY must match transmitter PA ramp-down time (bitrate dependent)
   {255, 0}
};

static void ICACHE_FLASH_ATTR receiveDoneTimerCb(void *v)
{
	if (rfm69_receiveDone()) {
  }
	os_timer_disarm(&receiveDoneTimer);
	os_timer_arm(&receiveDoneTimer, 1, 0);
}

void ICACHE_FLASH_ATTR intr_callback(unsigned pin, unsigned level)
{
   rfm69_interruptHandler();
}

// checks if a packet was received and/or puts transceiver in receive (ie RX or listen) mode
bool rfm69_receiveDone()
{
	if (_mode != RF69_MODE_RX) {
      rfm69_receiveBegin();
   } else if (PAYLOADLEN > 0) {
      rfm69_setMode(RF69_MODE_STANDBY); // enables interrupts
		return true;
   }
   return false;
}

// internal function
void rfm69_receiveBegin()
{
   DATALEN = 0;
   SENDERID = 0;
   TARGETID = 0;
   PAYLOADLEN = 0;
   ACK_REQUESTED = 0;
   ACK_RECEIVED = 0;
   RSSI = 0;
   if (rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)
   {
      rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
   }
   rfm69_writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01); // set DIO0 to "PAYLOADREADY" in receive mode
   rfm69_setMode(RF69_MODE_RX);
}

// internal function - interrupt gets called when a packet is received
void rfm69_interruptHandler()
{
   static uint32_t rxData;
   static uint8_t readCycles, remainder;

   if (_mode == RF69_MODE_RX && (rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY))
   {
      disableSendTimers();
      rfm69_setMode(RF69_MODE_STANDBY);

      rxData = rfm69_readReg32(REG_FIFO);
      PAYLOADLEN = (uint8_t) ((rxData & MASK32_BIT0) >> 24) ;
      PAYLOADLEN = PAYLOADLEN > 66 ? 66 : PAYLOADLEN; // precaution
      TARGETID = (uint8_t) ((rxData & MASK32_BIT1) >> 16);

      if(!(_promiscuousMode || TARGETID == _address || TARGETID == RF69_BROADCAST_ADDR) || PAYLOADLEN < 3)
      {
         PAYLOADLEN = 0;
         rfm69_receiveBegin();
         return;
      }

      DATALEN = PAYLOADLEN - 3;
      SENDERID = (uint8_t) ((rxData & MASK32_BIT2) >> 8);

      uint8_t CTLbyte = (uint8_t) (rxData & MASK32_BIT3);
      ACK_RECEIVED = CTLbyte & RFM69_CTL_SENDACK; // extract ACK-received flag
      ACK_REQUESTED = CTLbyte & RFM69_CTL_REQACK; // extract ACK-requested flag

      readCycles  = DATALEN/4;
      remainder   = DATALEN%4;

      for (uint8_t i = 0; i < readCycles; i++) {
         rxData = rfm69_readReg32(REG_FIFO);
         DATA[(4*i)] = (uint8_t) ((rxData & MASK32_BIT0) >> 24);
         DATA[(4*i)+1] = (uint8_t) ((rxData & MASK32_BIT1) >> 16);
         DATA[(4*i)+2] = (uint8_t) ((rxData & MASK32_BIT2) >> 8);
         DATA[(4*i)+3] = (uint8_t)  (rxData & MASK32_BIT3);
      }

      if (remainder)
      {
         rxData = rfm69_readReg32(REG_FIFO);
         switch(remainder)
         {
            case 4: DATA[4*readCycles+3] = (uint8_t) (rxData & MASK32_BIT3);
            case 3: DATA[4*readCycles+2] = (uint8_t) ((rxData & MASK32_BIT2) >> 8);
            case 2: DATA[4*readCycles+1] = (uint8_t) ((rxData & MASK32_BIT1) >> 16);
            case 1: DATA[4*readCycles]   = (uint8_t) ((rxData & MASK32_BIT0) >> 24);
         }
      }

      if (DATALEN < RF69_MAX_DATA_LEN) DATA[DATALEN] = 0; // add null at end of string

      radioMsgRxPush(SENDERID, DATA, DATALEN, CTLbyte);
      createRadioTask(rfm_handle_data, rfm_event_default);

      rfm69_setMode(RF69_MODE_RX);
   }
   RSSI = rfm69_readRSSI(false);
}

// get the received signal strength indicator (RSSI)
int16_t rfm69_readRSSI(bool forceTrigger)
{
   int16_t rssi = 0;
   if (forceTrigger)
   {
      // RSSI trigger not needed if DAGC is in continuous mode
      rfm69_writeReg(REG_RSSICONFIG, RF_RSSI_START);
      while ((rfm69_readReg(REG_RSSICONFIG) & RF_RSSI_DONE) == 0x00); // wait for RSSI_Ready
   }
   rssi = -rfm69_readReg(REG_RSSIVALUE);
   rssi >>= 1;
   return rssi;
}

//set this node's address
void rfm69_setAddress(uint8_t addr)
{
   _address = addr;
   rfm69_writeReg(REG_NODEADRS, _address);
}

//set this node's network id
void rfm69_setNetwork(uint8_t networkID)
{
   rfm69_writeReg(REG_SYNCVALUE2, networkID);
}

bool rfm69_canSend()
{
   // if signal stronger than -100dBm is detected assume channel activity
   if ((_mode == RF69_MODE_RX) && (PAYLOADLEN == 0) && (rfm69_readRSSI(false) < CSMA_LIMIT))
      { rfm69_setMode(RF69_MODE_STANDBY); return true; }

   return false;
}

void rfm69_sendFrame()
{
   struct radio_msg_st *tx = STAILQ_FIRST(&tx_msg_list);

   rfm69_setMode(RF69_MODE_STANDBY);
   while ((rfm69_readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00); // wait for ModeReady
   rfm69_writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00); // DIO0 is "Packet Sent"
   if (tx->buffSize > RF69_MAX_DATA_LEN) tx->buffSize = RF69_MAX_DATA_LEN;

   // write to FIFO
   rfm69_writeToFIFO(tx->ctlbyte);
   // no need to wait for transmit mode to be ready since its handled by the radio

   rfm69_setMode(RF69_MODE_TX);
   while ((rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00); // wait for ModeReady
   rfm69_setMode(RF69_MODE_STANDBY);
}

// should be polled immediately after sending a packet with ACK request
bool rfm69_ACKReceived(uint8_t fromNodeID)
{
   if (rfm69_receiveDone()) {
      return (SENDERID == fromNodeID || fromNodeID == RF69_BROADCAST_ADDR) && ACK_RECEIVED;
   }
   return false;
}

// check whether an ACK was requested in the last received packet (non-broadcasted packet)
bool rfm69_ACKRequested()
{
   return ACK_REQUESTED && (TARGETID != RF69_BROADCAST_ADDR);
}

// should be called immediately after reception in case sender wants ACK
void rfm69_sendACK()
{
   ACK_REQUESTED = 0;
   // uint8_t sender = SENDERID;
   int16_t _RSSI = RSSI; // save payload received RSSI value
   rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
   RSSI = _RSSI; // restore payload RSSI
}

// return the frequency (in Hz)
uint32_t rfm69_getFrequency()
{
  return RF69_FSTEP * (((uint32_t) rfm69_readReg(REG_FRFMSB) << 16) + ((uint16_t) rfm69_readReg(REG_FRFMID) << 8) + rfm69_readReg(REG_FRFLSB));
}

// set the frequency (in Hz)
void rfm69_setFrequency(uint32_t freqHz)
{
   uint8_t oldMode = _mode;
   if (oldMode == RF69_MODE_TX) { rfm69_setMode(RF69_MODE_RX); }

   freqHz /= RF69_FSTEP; // divide down by FSTEP to get FRF
   rfm69_writeReg(REG_FRFMSB, freqHz >> 16);
   rfm69_writeReg(REG_FRFMID, freqHz >> 8);
   rfm69_writeReg(REG_FRFLSB, freqHz);

   if (oldMode == RF69_MODE_RX) { rfm69_setMode(RF69_MODE_SYNTH); }
   rfm69_setMode(oldMode);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_spi_init
//   Description: Initialises SPI hardware for 10MHz operation
//    Parameters: none
////////////////////////////////////////////////////////////////////////////////
bool rfm69_spi_init()
{
   spi_init_gpio(SPI_DEV, SPI_CLK_USE_DIV);
   spi_clock(SPI_DEV, 4, 5); //10MHz  2 - 4 MHz  5
   spi_tx_byte_order(SPI_DEV, SPI_BYTE_ORDER_HIGH_TO_LOW);
   spi_rx_byte_order(SPI_DEV, SPI_BYTE_ORDER_HIGH_TO_LOW);

   SET_PERI_REG_MASK(SPI_USER(SPI_DEV), SPI_CS_SETUP|SPI_CS_HOLD);
   CLEAR_PERI_REG_MASK(SPI_USER(SPI_DEV), SPI_FLASH_MODE);

   GPIO_INT_TYPE gpio_type;
   gpio_type = GPIO_PIN_INTR_POSEDGE;
   if (set_gpio_mode(RFM_INTR_PIN, GPIO_PULLUP, GPIO_INT))
   {
      os_printf("GPIO%d set interrupt mode\r\n", pin_num[RFM_INTR_PIN]);
      if (gpio_intr_init(RFM_INTR_PIN, gpio_type)) {
         os_printf("GPIO%d enable %s mode\r\n", pin_num[RFM_INTR_PIN], gpio_type_desc[gpio_type]);
         gpio_intr_attach(intr_callback);
      } else {
         os_printf("Error: GPIO%d not enable %s mode\r\n", pin_num[RFM_INTR_PIN], gpio_type_desc[gpio_type]);
         return false;
      }
   } else {
      os_printf("Error: GPIO%d not set interrupt mode\r\n", pin_num[RFM_INTR_PIN]);
      return false;
   }
   return true;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_init
//   Description: Configure RFM69 and enable interrupts
//    Parameters: ID - node number; networkID - network number
////////////////////////////////////////////////////////////////////////////////
bool rfm69_init(uint8_t ID, uint8_t networkID)
{
   for (uint8_t i = 0; CONFIG[i][0] != 255; i++) {
      rfm69_writeReg(CONFIG[i][0], CONFIG[i][1]);
   }
   if (RFM69_NET_ID != networkID) { rfm69_setNetwork(networkID); }

   /* Initialize with encrypt key */
   if (_key != 0)
   {
      rfm69_encrypt(_key);
      /* Left as a quick check to make sure SPI is working */
      for (uint8_t i = 0; i < 16; i++) {
         uint8_t keyVal = rfm69_readReg((REG_AESKEY1+i));
         if ((uint8_t)_key[i] != keyVal) {
            os_printf("EncryptKey error. Check SPI?\n");
            return false;
         }
      }
   }

   rfm69_setHighPower(_isRFM69HW); // called regardless if it's a RFM69W or RFM69HW
   rfm69_setMode(RF69_MODE_STANDBY);

   _address = ID;

   /* Initialize Tx Queue */
   STAILQ_INIT(&tx_msg_list);

   os_timer_disarm(&receiveDoneTimer);
   os_timer_setfn(&receiveDoneTimer, receiveDoneTimerCb, NULL);
   os_timer_arm(&receiveDoneTimer, 1000, 0);
   return true;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_writeReg
//   Description: Set register value
//    Parameters: regAddr - control register to set
//                value - value to set
//uint32 spi_transaction(uint8 spi_no, uint8 cmd_bits, uint16 cmd_data,
// uint32 addr_bits, uint32 addr_data, uint32 dout_bits, uint32 dout_data,
// uint32 din_bits, uint32 dummy_bits)
////////////////////////////////////////////////////////////////////////////////
void rfm69_writeReg(uint8_t regAddr, uint8_t value)
{
   regAddr |= 0x80;
   spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 8, value, 0, 0);
}

void rfm69_writeToFIFO(uint8_t ctlByte)
{
   struct radio_msg_st *tx = STAILQ_FIRST(&tx_msg_list);
   uint8_t length = tx->buffSize;
   uint8_t toAddress = tx->nodeId;
   uint8_t regAddr = (REG_FIFO | 0x80); //set write command
   uint8_t writeCycles = length/4;
   uint8_t remainder   = length%4;
   static uint32 outData;

   // ETS_GPIO_INTR_DISABLE();
   outData = 0;
   outData = (uint32) (((length+3)<<24) | (toAddress<<16) | (_address<<8) | (ctlByte));
   spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 32, outData, 0, 0);

   for (uint8_t i=0; i < writeCycles; i++) {
      outData = 0;
      outData = (uint32) (tx->buff[(i*4)])<<24 | (tx->buff[(i*4+1)])<<16 | (tx->buff[(i*4+2)])<<8 | (tx->buff[(i*4+3)]);
      spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 32, outData, 0, 0);
   }

   for (uint8_t i=0; i < remainder; i++) {
      outData = 0;
      outData = tx->buff[4*writeCycles+remainder];
      spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 8, tx->buff[4*writeCycles+i], 0, 0);
   }
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readReg
//   Description: Get register value
//    Parameters: regAddr - control register to get
////////////////////////////////////////////////////////////////////////////////
uint8_t rfm69_readReg(uint8_t regAddr)
{
   regAddr &= 0x7F;
   return spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 0, 0, 8, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readReg
//   Description: Get register value
//    Parameters: regAddr - control register to get
////////////////////////////////////////////////////////////////////////////////
uint32_t rfm69_readReg32(uint8_t regAddr)
{
   regAddr &= 0x7F;
   return spi_transaction(SPI_DEV, 0, 0, 8, regAddr, 0, 0, 32, 0);
}

void rfm69_setMode(uint8_t newMode)
{
   if (newMode == _mode) { return; }

   switch (newMode) {
      case RF69_MODE_TX:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_TRANSMITTER);
         if (_isRFM69HW) rfm69_setHighPowerRegs(true);
      break;
      case RF69_MODE_RX:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_RECEIVER);
         if (_isRFM69HW) rfm69_setHighPowerRegs(false);
      break;
      case RF69_MODE_SYNTH:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SYNTHESIZER);
      break;
      case RF69_MODE_STANDBY:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_STANDBY);
      break;
      case RF69_MODE_SLEEP:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SLEEP);
      break;
      default:
   return;
   }

   // we are using packet mode, so this check is not really needed
   // but waiting for mode ready is necessary when going from sleep because the FIFO may not be immediately available from previous mode
   while (_mode == RF69_MODE_SLEEP && (rfm69_readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00); // wait for ModeReady

   _mode = newMode;
}

void rfm69_setHighPowerRegs(bool onOff)
{
   rfm69_writeReg(REG_TESTPA1, onOff ? 0x5D : 0x55);
   rfm69_writeReg(REG_TESTPA2, onOff ? 0x7C : 0x70);
}

// To enable encryption: radio.encrypt("ABCDEFGHIJKLMNOP");
// To disable encryption: radio.encrypt(null) or radio.encrypt(0)
// KEY HAS TO BE 16 bytes !!!
void rfm69_encrypt(const char* key)
{
   rfm69_setMode(RF69_MODE_STANDBY);
   if (key != 0) {
      for (uint8_t i = 0; i < 16; i++) { rfm69_writeReg((REG_AESKEY1+i), key[i]); }
   }
   rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFE) | (key ? 1 : 0));
}

// for RFM69HW only: you must call setHighPower(true) after initialize() or else transmission won't work
void rfm69_setHighPower(bool onOff)
{
   _isRFM69HW = onOff;
   rfm69_writeReg(REG_OCP, _isRFM69HW ? RF_OCP_OFF : RF_OCP_ON);
   if (_isRFM69HW) {
      rfm69_writeReg(REG_PALEVEL, (rfm69_readReg(REG_PALEVEL) & 0x1F) | RF_PALEVEL_PA1_ON | RF_PALEVEL_PA2_ON); // enable P1 & P2 amplifier stages
   } else {
      rfm69_writeReg(REG_PALEVEL, RF_PALEVEL_PA0_ON | RF_PALEVEL_PA1_OFF | RF_PALEVEL_PA2_OFF | _powerLevel); // enable P0 only
   }
}

void rfm69_sleep()
{
   rfm69_setMode(RF69_MODE_SLEEP);
}

// true  = disable filtering to capture all frames on network
// false = enable node/broadcast filtering to capture only frames sent to this/broadcast address
void rfm69_promiscuous(bool onOff)
{
   _promiscuousMode = onOff;
   //writeReg(REG_PACKETCONFIG1, (readReg(REG_PACKETCONFIG1) & 0xF9) | (onOff ? RF_PACKET1_ADRSFILTERING_OFF : RF_PACKET1_ADRSFILTERING_NODEBROADCAST));
}

// set *transmit/TX* output power: 0=min, 31=max
// this results in a "weaker" transmitted signal, and directly results in a lower RSSI at the receiver
// the power configurations are explained in the SX1231H datasheet (Table 10 on p21; RegPaLevel p66): http://www.semtech.com/images/datasheet/sx1231h.pdf
// valid powerLevel parameter values are 0-31 and result in a directly proportional effect on the output/transmission power
// this function implements 2 modes as follows:
//       - for RFM69W the range is from 0-31 [-18dBm to 13dBm] (PA0 only on RFIO pin)
//       - for RFM69HW the range is from 0-31 [5dBm to 20dBm]  (PA1 & PA2 on PA_BOOST pin & high Power PA settings - see section 3.3.7 in datasheet, p22)
void rfm69_setPowerLevel(uint8_t powerLevel)
{
   _powerLevel = (powerLevel > 31 ? 31 : powerLevel);
   if (_isRFM69HW) _powerLevel /= 2;
   rfm69_writeReg(REG_PALEVEL, (rfm69_readReg(REG_PALEVEL) & 0xE0) | _powerLevel);
}

void rfm69_readAllRegs()
{
   uint8_t regVal;
   for (uint8_t regAddr = 1; regAddr <= 0x4F; regAddr++) {
      regVal = rfm69_readReg(regAddr);
      os_printf("Register[%x] = %x\n", (regAddr & 0x7F), regVal);
   }
}
