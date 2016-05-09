#include "user_config.h"
#include "led.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_types.h"
#include "user_interface.h"

#include "driver/gpio16.h"

#define WIFI_LED_PIN_NUM    3 //GPIO0
#define RFM_LED_PIN_NUM     1 //GPIO5

// Timers
static ETSTimer wifiLedTimer;
static ETSTimer rfmLedTimer;

static uint8_t wifiLedState = 0;
static uint8_t rfmLedState = 0;


// Set the LED on or off, respecting the defined polarity
static void setLed(int on, int8_t pin)
{
   if (pin < 0) return; // disabled
   // LED is active-low
   if (on) { gpio_write(pin, on); }
   else { gpio_write(pin, 0); }
}

// Timer callback to update WiFi status LED
static void wifiLedTimerCb(void *v)
{
   int time = 1000;
   #if 0
   if (wifiState == wifi_gotip)
   {
      // connected, all is good, solid light with a short dark blip every 3 seconds
      wifiLedState = 1;
      time = 15000;
   }
   else if (wifiState == wifi_isconn)
   {
      // waiting for DHCP, go on/off every second
      wifiLedState = 1 - wifiLedState;
      time = 1000;
   }
   else
   {
      // not connected
      switch (wifi_get_opmode())
      {
         case 1: // STA
            wifiLedState = 0;
            break;
         case 2: // AP
            wifiLedState = 1-wifiLedState;
            time = wifiLedState ? 50 : 1950;
            break;
         case 3: // STA+AP
            wifiLedState = 1-wifiLedState;
            time = wifiLedState ? 50 : 950;
            break;
      }
   }
   #endif
   setLed(wifiLedState, WIFI_LED_PIN_NUM);
   os_timer_arm(&wifiLedTimer, time, 0);
}

// Timer callback to update RFM69 status LED
static void rfmLedTimerCb(void *v)
{
   int time = 1000;
   #if 0
   if (rfmState == rfm_unknown)
   {
      // connected, all is good, solid light with a short dark blip every 3 seconds
      rfmLedState = 1-rfmLedState;
      time = rfmLedState ? 2900 : 1200;
   }
   else if (rfmState == rfm_connected)
   {
      // waiting for DHCP, go on/off every second
      rfmLedState = 1;
      time = 10001;
   }
   else
   {
      // not connected
      rfmLedState = 1-rfmLedState;
      time = rfmLedState ? 950 : 1950;
   }
   #endif
   setLed(rfmLedState, RFM_LED_PIN_NUM);
   os_timer_arm(&rfmLedTimer, time, 0);
}

void ledInit(void)
{
   STATUS_DBG("CONN led=%d\n", 1);

   os_timer_disarm(&wifiLedTimer);
   os_timer_setfn(&wifiLedTimer, wifiLedTimerCb, NULL);
   os_timer_arm(&wifiLedTimer, 2003, 0);

   os_timer_disarm(&rfmLedTimer);
   os_timer_setfn(&rfmLedTimer, rfmLedTimerCb, NULL);
   os_timer_arm(&rfmLedTimer, 5517, 0);

}
