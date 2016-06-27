#include "user_config.h"

#include "c_types.h"
#include "mem.h"
#include "rfm.h"

#include "../util/cbuff.h"
#include "../libc/c_stdio.h"

#include "driver/rfm69_register.h"
#include "driver/spi.h"
#include "driver/rfm69.h"
#include "driver/gpio16.h"

#define SPI_DEV HSPI

LOCAL void ICACHE_RAM_ATTR rfm69_interruptHandler(unsigned pin, unsigned level);

extern uint8_t pin_num[GPIO_PIN_NUM];

const char *gpio_type_desc[] =
{
  "(DISABLE INTERRUPT)",
  "(UP)",
  "(DOWN)",
  "(BOTH)",
  "(LOW LEVEL)",
  "(HIGH LEVEL)"
};

static const uint32_t MASK32_BIT0 = 0xFF000000;
static const uint32_t MASK32_BIT1 = 0x00FF0000;
static const uint32_t MASK32_BIT2 = 0x0000FF00;
static const uint32_t MASK32_BIT3 = 0x000000FF;

static uint8_t _isRFM69HW = RFM69_IS_HW;
static uint8_t _powerLevel = 31;
static char* _key = RFM69_ENCRYPT_KEY;
RFM_Handle * rfmptr = NULL;

static HCBUFF hrxBuff;

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

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_writeReg
//   Description: Set register value
//    Parameters: regAddr - control register to set
//                value - value to set
// uint32 spi_transaction(uint8 spi_no, uint8 cmd_bits, uint16 cmd_data,
// uint32 addr_bits, uint32 addr_data, uint32 dout_bits, uint32 dout_data,
// uint32 din_bits, uint32 dummy_bits)
////////////////////////////////////////////////////////////////////////////////
void ICACHE_RAM_ATTR rfm69_writeReg(uint8_t regAddr, uint8_t value)
{
   regAddr |= 0x80;
   spi_transaction(SPI_DEV, 8, regAddr, 8, value, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readReg
//   Description: Read single byte from regAddr.
//    Parameters: regAddr - control register to get
////////////////////////////////////////////////////////////////////////////////
uint8_t ICACHE_RAM_ATTR rfm69_readReg(uint8_t regAddr)
{
   regAddr &= 0x7F;
   return spi_transaction(SPI_DEV, 8, regAddr, 0, 0, 8, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readReg32
//   Description: Read 32 bits from regAddr. Probably only useful for FIFO reads
//                or encrypt key reads.
//    Parameters: regAddr - control register to read from.
////////////////////////////////////////////////////////////////////////////////
uint32_t ICACHE_RAM_ATTR rfm69_readReg32(uint8_t regAddr)
{
   regAddr &= 0x7F;
   return spi_transaction(SPI_DEV, 8, regAddr, 0, 0, 32, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setMode
//   Description: Change RFM69 operating mode.
//    Parameters: newMode - RFM69_OP_MODE to switch to.
////////////////////////////////////////////////////////////////////////////////
void rfm69_setMode(RFM69_OP_MODE newMode)
{
   if (newMode == rfmptr->driver.curr_mode) { return; }

   switch (newMode) {
      case RF69_OP_TX:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_TRANSMITTER);
         rfm69_setHighPowerRegs(true);
      break;
      case RF69_OP_RX:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_RECEIVER);
         rfm69_setHighPowerRegs(false);
      break;
      case RF69_OP_SYNTH:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SYNTHESIZER);
      break;
      case RF69_OP_STANDBY:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_STANDBY);
      break;
      case RF69_OP_SLEEP:
         rfm69_writeReg(REG_OPMODE, (rfm69_readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SLEEP);
      break;
      default: //RF69_OP_NONE		// NONE
   return;
   }

   if (rfmptr->driver.curr_mode == RF69_OP_SLEEP)
   {
      // we are using packet mode, so this check is not really needed
      // but waiting for mode ready is necessary when going from sleep because the FIFO may not be immediately available from previous mode
      if (!(rfm69_readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY))
      {
         //wait for mode ready sleepmode
      }
   }

   rfmptr->driver.curr_mode = newMode;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setHighPowerRegs
//   Description: Set high power registers
//    Parameters: onOff - true: 20dBm (HW); false: 13dBm (W)
////////////////////////////////////////////////////////////////////////////////
void rfm69_setHighPowerRegs(bool onOff)
{
   rfm69_writeReg(REG_TESTPA1, onOff ? 0x5D : 0x55);
   rfm69_writeReg(REG_TESTPA2, onOff ? 0x7C : 0x70);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setHighPower
//   Description: Additional power registers that must be set on init
//    Parameters: onOff - true: 20dBm (HW); false: 13dBm (W)
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_writeToFIFO32
//   Description: Writes to FIFO in 32 bit chunks until remaining length is less
//                than 32 bits, writes remainder in 8 bit chunks. The ESP8266
//                requires 32 bit writes, so remainder must be padded.
//    Parameters: outbuf  - pointer to uint8 buffer
//                datalen - number of bytes to write
//          TODO: Write remaining bytes using single spi_transaction() call
//                instead of individual 8 bit calls.
////////////////////////////////////////////////////////////////////////////////
void ICACHE_RAM_ATTR rfm69_writeToFIFO32(uint8* outbuf, uint8_t datalen)
{
   unsigned i;
   uint8 regAddr = (REG_FIFO | 0x80); //set write command
   uint8 writeCycles = datalen/4;
   uint8 remainder   = datalen%4;
   static uint32 outData = 0; //matches datatype of spi_transaction

   for (i=0; i < writeCycles; i++)
   {
      outData = (uint32) (outbuf[(i*4)])<<24 | (outbuf[(i*4+1)])<<16 | (outbuf[(i*4+2)])<<8 | (outbuf[(i*4+3)]);
      spi_transaction(SPI_DEV, 8, regAddr, 32, outData, 0, 0);
   }

   for (i=0; i < remainder; i++)
   {
      spi_transaction(SPI_DEV, 8, regAddr, 8, outbuf[4*writeCycles+i], 0, 0);
   }
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_receiveDone
//   Description: Called continuously to put RFM69 in RX mode, flush buffers if
//                needed, re-enable DIO0 interrupt if needed.
////////////////////////////////////////////////////////////////////////////////
bool rfm69_receiveDone()
{
	if (rfmptr->driver.curr_mode != RF69_OP_RX)
      { rfm69_receiveBegin(); }
   else if (rfmptr->driver.PAYLOADLEN > 0)
   {
      rfm69_setMode(RF69_OP_STANDBY); // enables interrupts
		return true;
   }
   return false;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_receiveBegin
//   Description: Prepare driver for packet receive.
////////////////////////////////////////////////////////////////////////////////
void rfm69_receiveBegin()
{
   rfmptr->driver.DATALEN = 0;
   rfmptr->driver.SENDERID = 0;
   rfmptr->driver.TARGETID = 0;
   rfmptr->driver.PAYLOADLEN = 0;
   rfmptr->driver.ACK_REQUESTED = 0;
   rfmptr->driver.ACK_RECEIVED = 0;
   rfmptr->driver.RSSI = 0;
   if (rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)
   {
      rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
   }
   rfm69_writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01); // set DIO0 to "PAYLOADREADY" in receive mode
   rfm69_setMode(RF69_OP_RX);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_interruptHandler
//   Description: RFM69 interrupt routine. Called whenever DIO0/GPIO5 goes high.
//                This occurs when RFM69 is in RX mode and payload is ready.
//    Parameters: Unused, but potentially helpful. Left for compatibility with
//                gpio16.c interrupt attach method.
//                pin   - interrupt pin
//                level - pin level type
////////////////////////////////////////////////////////////////////////////////
void rfm69_interruptHandler(unsigned pin, unsigned level)
{
   system_os_post(RFM_TASK_PRIO, (os_signal_t)RFM_SIG_ISR0, (os_param_t)rfmptr);
}

void rfm69_exec_isr()
{
   unsigned i;
   static RFM_HEADER_T h;
   static uint32_t rxData;
   static uint8_t readCycles, remainder;
   unsigned int        dataInBuffer = 0;
   uint8_t testbuf[8];

   if (rfmptr->driver.curr_mode == RF69_OP_RX && (rfm69_readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY))
   {
      rfm69_setMode(RF69_OP_STANDBY);
      h.data = rfm69_readReg32(REG_FIFO);

      rfmptr->driver.TARGETID = h.get.TARGETID; //(uint8_t) ((h.data & MASK32_BIT1) >> 16);
      rfmptr->driver.PAYLOADLEN = h.get.PAYLOADLEN;

      if(!((rfmptr->options & RFM_PROMISCUOUS) || h.get.TARGETID == rfmptr->nodeId
         || h.get.TARGETID == RF69_BROADCAST_ADDR) || h.get.PAYLOADLEN < 3)
      {
         rfmptr->driver.PAYLOADLEN = 0;
         rfm69_receiveBegin();
         return;
      }

      rfmptr->driver.DATALEN = rfmptr->driver.PAYLOADLEN - 3;
      rfmptr->driver.SENDERID = h.get.SENDERID;
      uint8_t CTLbyte = h.get.CTLBYTE;
      rfmptr->driver.ACK_RECEIVED = CTLbyte & RFM69_CTL_SENDACK; // extract ACK-received flag
      rfmptr->driver.ACK_REQUESTED = CTLbyte & RFM69_CTL_REQACK; // extract ACK-requested flag

      #if 0
      NODE_DBG("rx ackrx =%d dl = %d\n", rfmptr->driver.ACK_RECEIVED, rfmptr->driver.DATALEN);
      dataInBuffer = cbuffGetArray(hrxBuff, testbuf, 1);
      for (i=0;i<dataInBuffer;i++)
         { NODE_DBG("%02x ", testbuf[i]); }
      NODE_DBG("\n");
      #endif

      if (rfmptr->driver.DATALEN > 0)
      {
         hrxBuff = cbuffOpen(rfmptr->rxBuffNum);
         dataInBuffer = cbuffPutByte(hrxBuff, h.get.PAYLOADLEN);

         readCycles  = rfmptr->driver.DATALEN/4;
         remainder   = rfmptr->driver.DATALEN%4;

         for (i = 0; i < readCycles; i++)
         {
            rxData = rfm69_readReg32(REG_FIFO);
            dataInBuffer = cbuffPutByte(hrxBuff, (uint8_t) (rxData >> 24));
            dataInBuffer = cbuffPutByte(hrxBuff, (uint8_t) (rxData >> 16));
            dataInBuffer = cbuffPutByte(hrxBuff, (uint8_t) (rxData >> 8));
            dataInBuffer = cbuffPutByte(hrxBuff, (uint8_t) (rxData));

            #if 0
            dataInBuffer = cbuffGetArray(hrxBuff, testbuf, 4);
            for (i=0;i<dataInBuffer;i++) { NODE_DBG("%02x ", testbuf[i]); }
            NODE_DBG("\n");
            #endif
         }

         if (remainder)
         {
            rxData = rfm69_readReg32(REG_FIFO);

            for (i=0;i<remainder;i++)
            {
               dataInBuffer = cbuffPutByte(hrxBuff, (uint8_t) (rxData >> (24 -(i*8))));
            }

            #if 0
            dataInBuffer = cbuffGetArray(hrxBuff, testbuf, remainder);
            for (i=0;i<dataInBuffer;i++) { NODE_DBG("%02x ", testbuf[i]); }
            NODE_DBG("\n");
            #endif
         }

         rfmptr->rxBuffNum = cbuffClose(hrxBuff);
      }

      // rfm69_setMode(rfmptr, RF69_OP_RX);
      rfmptr->state = RFM_RX_BUFF_READY;

      system_os_post(RFM_TASK_PRIO, 0, (os_param_t)rfmptr);
   }
   rfmptr->driver.RSSI = rfm69_readRSSI(false);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readRSSI
//   Description: get the received signal strength indicator (RSSI)
//    Parameters: forceTrigger : force new reading?
//       Returns: RSSI (16 bit signed)
////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setAddress
//   Description: set this node's address
//    Parameters: addr: New address (1-255)
//          TODO: Is 0 allowed?
////////////////////////////////////////////////////////////////////////////////
void rfm69_setAddress(uint8_t addr)
{
   rfmptr->nodeId = addr;
   rfm69_writeReg(REG_NODEADRS, rfmptr->nodeId);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setNetwork
//   Description: set the network id (if not in promiscuous)
//    Parameters: addr: New address (1-255)
//          TODO: Is 0 allowed?
////////////////////////////////////////////////////////////////////////////////
void rfm69_setNetwork(uint8_t networkID)
{
   rfm69_writeReg(REG_SYNCVALUE2, networkID);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_canSend
//   Description: Checks current RSSI level to see if network is 'busy'
//       Returns: true: RSSI strength is < -100 dBm and payload is empty
//                false: activity is detected
////////////////////////////////////////////////////////////////////////////////
bool rfm69_canSend()
{
   // if signal stronger than -100dBm is detected assume channel activity
   if ((rfmptr->driver.curr_mode == RF69_OP_RX) && (rfmptr->driver.PAYLOADLEN == 0) && (rfm69_readRSSI(false) < CSMA_LIMIT))
      { rfm69_setMode(RF69_OP_STANDBY); return true; }

   return false;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_ACKReceived
//   Description: Checks driver to see if an ACK was received. Should be polled
//                immediately after sending a packet with ACK request
//    Parameters: fromNodeID: Node address to check for ack
//       Returns: true: Ack was received
//                false: No ack found
////////////////////////////////////////////////////////////////////////////////
bool rfm69_ACKReceived(uint8_t fromNodeID)
{
   if (rfm69_receiveDone())
   {
      return (rfmptr->driver.SENDERID == fromNodeID || fromNodeID == RF69_BROADCAST_ADDR) && rfmptr->driver.ACK_RECEIVED;
   }
   return false;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_ACKRequested
//   Description: Checks driver to see if an ACK was requested in the last
//                received packet. (non-broadcasted packets only)
//                immediately after sending a packet with ACK request
//       Returns: true: Ack was requested
//                false: No ack requested
////////////////////////////////////////////////////////////////////////////////
bool rfm69_ACKRequested()
{
   return rfmptr->driver.ACK_REQUESTED && (rfmptr->driver.TARGETID != RF69_BROADCAST_ADDR);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_sendACK
//   Description: Should be called immediately after reception in case sender
//                wants ACK. Clears ACK_REQUESTED flag.
////////////////////////////////////////////////////////////////////////////////
void rfm69_sendACK()
{
   rfmptr->driver.ACK_REQUESTED = 0;
   uint8_t sender = rfmptr->driver.SENDERID;
   int16_t _RSSI = rfmptr->driver.RSSI; // save payload received RSSI value
   // rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
   // uint32_t now = millis();
   // while (!canSend() && millis() - now < RF69_CSMA_LIMIT_MS) receiveDone();
   // SENDERID = sender;    // TWS: Restore SenderID after it gets wiped out by receiveDone()
   // sendFrame(sender, buffer, bufferSize, false, true);

   rfmptr->driver.RSSI = _RSSI; // restore payload RSSI
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_getFrequency
//   Description: Reads the frequency registers
//       Returns: the frequency (in Hz) as uint32
////////////////////////////////////////////////////////////////////////////////
uint32_t rfm69_getFrequency()
{
  return RF69_FSTEP * (((uint32_t) rfm69_readReg(REG_FRFMSB) << 16) + ((uint16_t) rfm69_readReg(REG_FRFMID) << 8) + rfm69_readReg(REG_FRFLSB));
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setFrequency
//   Description: Writes the frequency registers
//    Parameters: freqHz: the frequency (in Hz)
////////////////////////////////////////////////////////////////////////////////
void rfm69_setFrequency(uint32_t freqHz)
{
   uint8_t oldMode = rfmptr->driver.curr_mode;
   if (oldMode == RF69_OP_TX) { rfm69_setMode(RF69_OP_RX); }

   freqHz /= RF69_FSTEP; // divide down by FSTEP to get FRF
   rfm69_writeReg(REG_FRFMSB, freqHz >> 16);
   rfm69_writeReg(REG_FRFMID, freqHz >> 8);
   rfm69_writeReg(REG_FRFLSB, freqHz);

   if (oldMode == RF69_OP_RX) { rfm69_setMode(RF69_OP_SYNTH); }
   rfm69_setMode(oldMode);
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_setEncryptKey
//   Description: Writes the REG_AESKEY registers starting at REG_AESKEY1. Must
//                be 16 bytes to be enabled.
//    Parameters: key - pointer to key string. If < 16 or NULL, encryption will
//                      be disabled.
////////////////////////////////////////////////////////////////////////////////
void rfm69_setEncryptKey(char* key)
{
   unsigned i;
   bool enable = false;
   rfm69_setMode(RF69_OP_STANDBY);

   if (key != NULL)
   {
      enable = true;
      if (strlen(key) != 16) {
         enable = false;
      } else {
         for (i = 0; i < 16; i++) { rfm69_writeReg((REG_AESKEY1+i), key[i]); }
      }
   }

   rfm69_writeReg(REG_PACKETCONFIG2, (rfm69_readReg(REG_PACKETCONFIG2) & 0xFE) | (enable ? 1 : 0));
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_sleep
//   Description: Small wrapper to set RF69_OP_SLEEP
////////////////////////////////////////////////////////////////////////////////
void rfm69_sleep()
{
   rfm69_setMode(RF69_OP_SLEEP);
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

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_readAllRegs
//   Description: Read all registers and print to stdout. Useful for debugging/
//                making sure SPI is working properly.
////////////////////////////////////////////////////////////////////////////////
void rfm69_readAllRegs()
{
   unsigned regAddr;
   uint8_t regVal;
   for (regAddr = 1; regAddr <= 0x4F; regAddr++) {
      regVal = rfm69_readReg(regAddr);
      RFM_DBG("Register[%x] = %x\n", (regAddr & 0x7F), regVal);
   }
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_spi_init
//   Description: Initialises SPI hardware for 4MHz operation. RFM69 datasheet
//                says 10MHz is supported, but Moteino users have found it to be
//                less reliable.
//          TODO: Is reliability an ATMega issue or RFM69 issue?
////////////////////////////////////////////////////////////////////////////////
rfm_retcode_t rfm69_spi_init()
{
   spi_init_gpio(SPI_DEV, SPI_CLK_USE_DIV);
   spi_clock(SPI_DEV, 4, 5); //10MHz - 2 || 4 MHz - 5
   spi_tx_byte_order(SPI_DEV, SPI_BYTE_ORDER_HIGH_TO_LOW);
   spi_rx_byte_order(SPI_DEV, SPI_BYTE_ORDER_HIGH_TO_LOW);

   SET_PERI_REG_MASK(SPI_USER(SPI_DEV), SPI_CS_SETUP|SPI_CS_HOLD);
   CLEAR_PERI_REG_MASK(SPI_USER(SPI_DEV), SPI_FLASH_MODE);

   GPIO_INT_TYPE gpio_type;
   gpio_type = GPIO_PIN_INTR_POSEDGE;
   if (set_gpio_mode(RFM_INTR_PIN, GPIO_PULLUP, GPIO_INT))
   {
      NODE_DBG("GPIO%d set interrupt mode\r\n", pin_num[RFM_INTR_PIN]);
      if (gpio_intr_init(RFM_INTR_PIN, gpio_type)) {
         NODE_DBG("GPIO%d enable %s mode\r\n", pin_num[RFM_INTR_PIN], gpio_type_desc[gpio_type]);
         gpio_intr_attach(rfm69_interruptHandler);
      } else {
         return RFM_GPIO_SET_INTR_ERR;
      }
   } else {
      os_printf("Error: GPIO%d not set interrupt mode\r\n", pin_num[RFM_INTR_PIN]);
      return RFM_GPIO_SET_ERR;
   }
   return RFM_SPI_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Function Name: rfm69_init
//   Description: Configure RFM69 and enable interrupts
//    Parameters: handle - pointer to RFM_Handle struct initialized in rfm/app.c
//                    ID - node ID (1-255)
//             networkID - network ID (1-255)
////////////////////////////////////////////////////////////////////////////////
rfm_retcode_t rfm69_init(RFM_Handle *handle, uint8_t ID, uint8_t networkID)
{
   rfmptr = handle;
   rfmptr->state = RFM_IDLE;
   unsigned i;
   for (i = 0; CONFIG[i][0] != 255; i++) {
      rfm69_writeReg(CONFIG[i][0], CONFIG[i][1]);
   }
   if (RFM69_NET_ID != networkID) { rfm69_setNetwork(networkID); }

   /* Initialize with encrypt key */
   rfm69_setEncryptKey(_key);
   /* Left as a quick check to make sure SPI is working */
   NODE_DBG("Encrypt Key: ");
   for (i = 0; i < 16; i++)
   {
      uint8_t keyVal = rfm69_readReg((REG_AESKEY1+i));
      if ((uint8_t) _key[i] != keyVal)
         { NODE_DBG("Invalid Key = %02x", keyVal); return RFM_KEY_ERR; }
      else
         { NODE_DBG("%c", keyVal); }
   }
   NODE_DBG("\n");

   rfm69_setHighPower(_isRFM69HW); // called regardless if it's a RFM69W or RFM69HW
   rfm69_setMode(RF69_OP_STANDBY);

   rfmptr->nodeId = ID;

   return RFM_INIT_OK;
}
