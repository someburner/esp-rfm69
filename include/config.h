#ifndef CONFIG_H
#define CONFIG_H

// Flash configuration settings. When adding new items always add them at the end and formulate
// them such that a value of zero is an appropriate default or backwards compatible. Existing
// modules that are upgraded will have zero in the new fields. This ensures that an upgrade does
// not wipe out the old settings.

typedef struct {
  uint32_t seq; // flash write sequence number
  uint16_t magic, crc;
  int8_t   reset_pin, isp_pin, conn_led_pin; //conn_led = gpio0
  int32_t  baud_rate;
  char     hostname[33];               // if using DHCP
  uint32_t staticip, netmask, gateway; // using DHCP if staticip==0
  uint8_t  tcp_enable, rssi_enable;    // TCP client settings
  //char     api_key[48];                // RSSI submission API key (Grovestreams for now)
  uint8_t  nodeID;
} FlashConfig;
extern FlashConfig flashConfig;

bool configSave(void);
bool configRestore(void);
void configWipe(void);


#endif
