#ifndef RFM69_H
#define RFM69_H
#include "rfm.h"

#define RF69_MAX_DATA_LEN       61 // to take advantage of the built in AES/CRC we want to limit the frame size to the internal FIFO size (66 bytes - 3 bytes overhead - 2 bytes crc)
#define CSMA_LIMIT             -90 // upper RX signal sensitivity threshold in dBm for carrier sense access

// available frequency bands
#define RF69_315MHZ             31 // non trivial values to avoid misconfiguration
#define RF69_433MHZ             43
#define RF69_868MHZ             86
#define RF69_915MHZ             91

#define null                    0
#define COURSE_TEMP_COEF       -90 // puts the temperature reading in the ballpark, user can fine tune the returned value
#define RF69_BROADCAST_ADDR     255
#define RF69_CSMA_LIMIT_MS      1000
#define RF69_CSMA_LIMIT_US      1000000UL
#define RF69_TX_LIMIT_MS        1000
#define RF69_FSTEP              61.03515625 // == FXOSC / 2^19 = 32MHz / 2^19 (p13 in datasheet)

#define RFM69_CTL_SENDACK       0x80
#define RFM69_CTL_REQACK        0x40

const uint8_t CONFIG[24][2];

rfm_retcode_t rfm69_spi_init(void);
rfm_retcode_t rfm69_init(RFM_Handle *handle, uint8_t ID, uint8_t networkID);

void rfm69_exec_isr();

bool rfm69_receiveDone();
void rfm69_receiveBegin();
void rfm69_setMode(RFM69_OP_MODE newMode);
bool rfm69_canSend();
bool rfm69_ACKRequested();
void rfm69_sendACK();
void rfm69_setFrequency(uint32_t freqHz);
void rfm69_setEncryptKey(char* key);
void rfm69_sleep();

void rfm69_setHighPowerRegs(bool onOff);
void rfm69_setHighPower(bool onOFF); // has to be called after initialize() for RFM69HW  -def onOFF=true
int16_t rfm69_readRSSI(bool forceTrigger); //def forceTrigger=false
void rfm69_setAddress(uint8_t addr);

void rfm69_setNetwork(uint8_t networkID);
void rfm69_setPowerLevel(uint8_t level); // reduce/increase transmit power level
/*
uint8_t readTemperature(uint8_t calFactor); // get CMOS temperature (8bit) -def calFactor=0
void rcCalibration(); // calibrate the internal RC oscillator for use in wide temperature variations - see datasheet section [4.3.5. RC Timer Accuracy]
*/
void ICACHE_RAM_ATTR rfm69_writeToFIFO32(uint8* outbuf, uint8_t datalen);
uint8_t ICACHE_RAM_ATTR rfm69_readReg(uint8_t regAddr);
void ICACHE_RAM_ATTR rfm69_writeReg(uint8_t regAddr, uint8_t value);
// uint8_t rfm69_readReg(uint8_t regAddr);
// void rfm69_writeReg(uint8_t regAddr, uint8_t value);
uint32_t ICACHE_RAM_ATTR rfm69_readReg32(uint8_t regAddr);
void rfm69_readAllRegs();

#endif
