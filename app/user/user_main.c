/******************************************************************************
 * FileName: user_main.c
 * Description: entry file of user application
 *
*******************************************************************************/
#include "platform.h"
#include "c_stdlib.h"
#include "c_stdio.h"

#include "user_interface.h"
#include "user_exceptions.h"
#include "user_config.h"
#include "ets_sys.h"
#include "mem.h"
#include "dns.h"

#include "driver/uart.h"
#include "flash_fs.h"
#include "flash_api.h"

#include "mqtt/app.h"
#include "platform/event.h"

static os_timer_t heapTimer;

static char *rst_codes[] = {
  "normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};

/* Important: no_init_data CAN NOT be left as zero initialised, as that
 * initialisation will happen after user_start_trampoline, but before
 * the user_init, thus clobbering our state!
 */
static uint8_t no_init_data = 0xff;
// uint32_t flash_end1 = 0xffffffff;
// uint32_t init_data_hdr1= 0xffffffff;

// Uncomment to erase WiFi Config
//#define FLASH_BLANK
uint32 lastFreeHeap = 0;

#define VERS_STR_STR(V) #V
#define VERS_STR(V) VERS_STR_STR(V)
char *esp_rfm69_version = VERS_STR(VERSION);

/* Note: the trampoline *must* be explicitly put into the .text segment, since
 * by the time it is invoked the irom has not yet been mapped. This naturally
 * also goes for anything the trampoline itself calls.
 */
void TEXT_SECTION_ATTR user_start_trampoline (void)
{
   __real__xtos_set_exception_handler (
   EXCCAUSE_LOAD_STORE_ERROR, load_non_32_wide_handler);

   /* Minimal early detection of missing esp_init_data.
   * If it is missing, the SDK will write its own and thus we'd end up
   * using that unless the flash size field is incorrect. This then leads
   * to different esp_init_data being used depending on whether the user
   * flashed with the right flash size or not (and the better option would
   * be to flash with an *incorrect* flash size, counter-intuitively).
   * To avoid that mess, we read out the flash size and do a test for
   * esp_init_data based on that size. If it's missing, flag for later.
   * If the flash size was incorrect, we'll end up fixing it all up
   * anyway, so this ends up solving the conundrum. Only remaining issue
   * is lack of spare code bytes in iram, so this is deliberately quite
   * terse and not as readable as one might like.
   */
   SPIFlashInfo sfi;
   SPIRead (0, &sfi, sizeof (sfi)); // Cache read not enabled yet, safe to use
   if (sfi.size < 2) // Compensate for out-of-order 4mbit vs 2mbit values
      sfi.size ^= 1;
   uint32_t flash_end_addr = (256 * 1024) << sfi.size;
   // flash_end1 = flash_end_addr;
   // init_data_hdr1 = sfi.size;
   uint32_t init_data_hdr = 0xffffffff;
   SPIRead (flash_end_addr - 4 * SPI_FLASH_SEC_SIZE, &init_data_hdr, sizeof (init_data_hdr));
   no_init_data = (init_data_hdr == 0xffffffff);

   call_user_start();
}

static void heapTimerCb(void *arg)
{
   #ifdef system_show_malloc
   os_timer_disarm(&heapTimer);
   system_show_malloc();
   os_timer_arm(&heapTimer, 2000, 1);
   #else
   lastFreeHeap = system_get_free_heap_size();
   NODE_DBG("FREE HEAP: %d\n",lastFreeHeap);
   #endif

}

static void config_wifi()
{
   NODE_DBG("Putting AP UP");
   wifi_set_opmode(STATIONAP_MODE);  //Set softAP + station mode

   struct softap_config config;
   wifi_softap_get_config(&config);

   uint32_t random_number = os_random();
   char ssid[32];
   os_sprintf(ssid, "rfm69_%x", random_number);

   strcpy(config.ssid,ssid);
   memset(config.password,0,64);
   config.ssid_len=strlen(ssid);
   config.channel=11;
   config.authmode=AUTH_OPEN;
   config.max_connection=4;
   config.ssid_hidden=0;
   wifi_softap_set_config(&config);

#if defined(STA_SSID) && defined(STA_PASS)
   char sta_ssid[32] = VERS_STR(STA_SSID);
   char sta_password[64] = VERS_STR(STA_PASS);

   struct station_config stconf;
   wifi_station_get_config(&stconf);
   stconf.bssid_set = 0;

   os_memcpy(&stconf.ssid, sta_ssid, 32);
   os_memcpy(&stconf.password, sta_password, 64);
   NODE_DBG("WiFi Makefile setting: \n");
   NODE_DBG("ap: %s pw %s\n",(char*)stconf.ssid, (char*)stconf.password);
   wifi_station_set_config(&stconf);
   if (wifi_station_dhcpc_status() != DHCP_STARTED)
      { wifi_station_dhcpc_start(); }
#else
   struct station_config stconf;
   wifi_station_get_config(&stconf);

   stconf.bssid_set = 0;
   memset(stconf.ssid,0,32);
   memset(stconf.password,0,64);
   wifi_station_set_config(&stconf);

#endif
}

void esp_rfm69_init(void)
{
   system_update_cpu_freq(160); //overclock :)

	uart_init(BIT_RATE_115200,BIT_RATE_115200);

   user_event_init();

   #if defined(FLASH_SAFE_API)
       if( flash_safe_get_size_byte() != flash_rom_get_size_byte()) {
           NODE_ERR("Self adjust flash size.\n");
           // Fit hardware real flash size.
           flash_rom_set_size_byte(flash_safe_get_size_byte());
           // Write out init data at real location.
           no_init_data = true;

           fs_unmount();   // mounted by format.
       }
   #endif // defined(FLASH_SAFE_API)

	// if (no_init_data)
	// {
	// 	NODE_ERR("Restore init data.\n");
	// 	// Flash init data at FLASHSIZE - 0x04000 Byte.
	// 	flash_init_data_default();
	// 	// Flash blank data at FLASHSIZE - 0x02000 Byte.
	// 	flash_init_data_blank();
	// 	// Reboot to make the new data come into effect
	// 	system_restart ();
	// }

   /* Both should return 0x400000 for esp12-e */
   #ifdef FS_DEBUG
   uint32_t fs_size;
   fs_size = flash_safe_get_size_byte();
   FS_DBG("\nflash safe sz = 0x%x\n", fs_size);
   fs_size = flash_rom_get_size_byte();
   FS_DBG("\nflash rom sz = 0x%x\n", fs_size);
   FS_DBG("platform_flash_get_first_free_block_address = 0x%08x\n", platform_flash_get_first_free_block_address(NULL));
   uint32 flash_id = spi_flash_get_id();
   FS_DBG("Flash_ID = %x\n", flash_id);
   #endif

   dynfs_mount();
   // myspiffs_format(1);
   if (dynfs_mounted() != 1)
   {
      NODE_ERR("dynfs not mounted?\n");
   }
   else
   {
      // int tret = myfs_check(FS1);
      // NODE_ERR("dynfs ok tret = %d\n", tret);
   }

   fs_mount();
   int res = 0;
   if (fs_mounted() != 1)
   {
      NODE_ERR("fs not mounted?\n");
   }
   else
   {
      if (read_conf_from_fs(0)) { NODE_DBG("RFM Conf OK\n"); }
      if (read_conf_from_fs(1)) { NODE_DBG("Wifi Conf OK\n"); }
      if (read_conf_from_fs(2)) { NODE_DBG("Http Conf OK\n"); }
   }
   fs_init_info();


   sntp_setservername(0, "time-a.timefreq.bldrdoc.gov");
   sntp_setservername(1, "time-c.nist.gov");
   sntp_set_timezone(0);
	sntp_init();

   wifi_station_set_reconnect_policy(true);
   wifi_station_set_auto_connect(1); //for auto-connect on boot
   config_wifi();

   init_dns();
   init_http_server();

   res = init_rfm_handler();
   NODE_DBG("RFM69 Handler Init %s!\n", (res==1) ? "OK":"Error");

   #ifdef DEVELOP_VERSION
   //arm timer
   os_memset(&heapTimer,0,sizeof(os_timer_t));
   os_timer_disarm(&heapTimer);
   os_timer_setfn(&heapTimer, (os_timer_func_t *)heapTimerCb, NULL);
   os_timer_arm(&heapTimer, 5000, 1);
   #endif

   struct rst_info *ri = system_get_rst_info();
   char rst_string[25];
   os_sprintf(rst_string, "Rst: %s", rst_codes[ri->reason]);
   // mqtt_debug_publish(BRIDGE_ID, 0, rst_string, strlen(rst_string));
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
   system_init_done_cb(esp_rfm69_init);
}
