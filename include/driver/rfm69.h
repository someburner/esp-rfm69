#ifndef RFM69_H
#define RFM69_H

#include "driver/spi.h"
#include "driver/rfm69_register.h"
#include "radiohandler.h"

#define RF69_MAX_DATA_LEN       61 // to take advantage of the built in AES/CRC we want to limit the frame size to the internal FIFO size (66 bytes - 3 bytes overhead - 2 bytes crc)
#define CSMA_LIMIT             -90 // upper RX signal sensitivity threshold in dBm for carrier sense access
#define RF69_MODE_SLEEP         0 // XTAL OFF
#define RF69_MODE_STANDBY       1 // XTAL ON
#define RF69_MODE_SYNTH         2 // PLL ON
#define RF69_MODE_RX            3 // RX MODE
#define RF69_MODE_TX            4 // TX MODE

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

bool rfm69_spi_init();
bool rfm69_init(uint8_t ID, uint8_t networkID);
void rfm69_encrypt(const char* key);
void rfm69_setMode(uint8_t mode);
void rfm69_setHighPowerRegs(bool onOff);
void rfm69_setHighPower(bool onOFF); // has to be called after initialize() for RFM69HW  -def onOFF=true
void rfm69_sleep();
bool rfm69_receiveDone();
void rfm69_receiveBegin();
int16_t rfm69_readRSSI(bool forceTrigger); //def forceTrigger=false
void rfm69_interruptHandler();
void rfm69_setAddress(uint8_t addr);
void rfm69_setNetwork(uint8_t networkID);
bool rfm69_canSend();
bool rfm69_ACKReceived(uint8_t fromNodeID);
bool rfm69_ACKRequested();
void rfm69_sendACK();
void rfm69_sendFrame();
uint32_t rfm69_getFrequency();
void rfm69_setFrequency(uint32_t freqHz);
void rfm69_promiscuous(bool onOff); //def onOff=true
void rfm69_setPowerLevel(uint8_t level); // reduce/increase transmit power level
/*
uint8_t readTemperature(uint8_t calFactor); // get CMOS temperature (8bit) -def calFactor=0
void rcCalibration(); // calibrate the internal RC oscillator for use in wide temperature variations - see datasheet section [4.3.5. RC Timer Accuracy]
*/
void rfm69_writeReg(uint8_t regAddr, uint8_t value);
void rfm69_writeToFIFO(uint8_t ctlByte);
uint8_t rfm69_readReg(uint8_t regAddr);
uint32_t rfm69_readReg32(uint8_t regAddr);
void rfm69_readAllRegs();

#endif
