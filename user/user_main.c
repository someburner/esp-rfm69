#include <esp8266.h>
#include <sntp.h>
#include <gpio.h>
#include "httpd.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "cgiradio.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"
#include "uart.h"
#include "console.h"
#include "config.h"
#include "log.h"
#include "radiohandler.h"
#include "driver/spi.h"
#include "driver/spi_register.h"
#include "driver/rfm69.h"

//Function that tells the authentication system what users/passwords live on the system.
//This is disabled in the default build; if you want to try it, enable the authBasic line in
//the builtInUrls below.
int myPassFn(HttpdConnData *connData, int no, char *user, int userLen, char *pass, int passLen) {
	if (no==0) {
		os_strcpy(user, "admin");
		os_strcpy(pass, "s3cr3t");
		return 1;
//Add more users this way. Check against incrementing no for each user added.
//	} else if (no==1) {
//		os_strcpy(user, "user1");
//		os_strcpy(pass, "something");
//		return 1;
	}
	return 0;
}

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[]={
	{"/", cgiRedirect, "/index.html"},
	{"/menu", cgiMenu, NULL},
	{"/flash.bin", cgiReadFlash, NULL},
	{"/flash/download", cgiReadFlash, NULL},
	{"/flash/next", cgiGetFirmwareNext, NULL},
	{"/flash/upload", cgiUploadFlash, NULL},
	{"/flash/test", cgiFlashTest, NULL},
	{"/flash/uploadfw", cgiUploadFirmware, NULL},
	{"/flash/reboot", cgiRebootFirmware, NULL},
	{"/console/clear", ajaxConsoleClear, NULL},
	{"/console/senddata", ajaxConsoleSendData, NULL},

	{"/console/text", ajaxConsole, NULL},

	// {"/wifi/*", authBasic, myPassFn},
	{"/wifi", cgiRedirect, "/wifi/wifi.html"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.html"},
	{"/wifi/info", cgiWifiInfo, NULL},
	{"/wifi/scan", cgiWiFiScan, NULL},
	{"/wifi/connect", cgiWiFiConnect, NULL},
	{"/wifi/connstatus", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode", cgiWiFiSetMode, NULL},
	{"/wifi/special", cgiWiFiSpecial, NULL},
	{"/ws/logws.cgi", cgiWebsocket, logWsConnect},
	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;

static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
	os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

void user_rf_pre_init(void) {
}

// address of espfs binary blob
extern uint32_t _binary_espfs_img_start;

static char *rst_codes[] = {
	"normal", "wdt reset", "exception", "soft wdt", "restart", "deep sleep", "external",
};

#define SSID "rfm69-Base"
#define PASSWORD ""

# define VERS_STR_STR(V) #V
# define VERS_STR(V) VERS_STR_STR(V)
char *esp_rfm69_version = VERS_STR(VERSION);

void ICACHE_FLASH_ATTR
set_softAP_conf(void)
{
 	wifi_station_set_auto_connect(1);
	wifi_set_opmode(0x03);

    struct softap_config config;
    wifi_softap_get_config(&config);

    char ssid[32] = SSID;
    char password[64] = PASSWORD;
    config.ssid_len = 10;
    config.channel = 6;
    config.max_connection = 2;
    config.authmode = AUTH_OPEN;
    config.beacon_interval = 100;
    config.ssid_hidden = 0;

    os_memcpy(&config.ssid, ssid, 32);
    os_memcpy(&config.password, password, 64);
    wifi_softap_set_config(&config);
    //wifi_softap_set_config_current(&softConf);
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) {
	// get the flash config so we know how to init things
	configWipe(); // uncomment to reset the config for testing purposes
	bool restoreOk = configRestore();

	// init gpio pin registers
	gpio_init();
	// init UART
	uart_init(flashConfig.baud_rate, 115200);
	logInit(); // must come after init of uart
	// say hello (leave some time to cause break in TX after boot loader's msg
	os_delay_us(10000L);
	os_printf("\n\n** %s\n", esp_rfm69_version);
	os_printf("Flash config restore %s\n", restoreOk ? "ok" : "*FAILED*");

#if defined(STA_SSID) && defined(STA_PASS)
// int x = wifi_get_opmode() & 0x3;
// if (x == 2) {
	// we only force the STA settings when a full flash of the module has been made, which
	// resets the wifi settings not to have anything configured
	struct station_config stconf;
	wifi_station_get_config(&stconf);

	if (os_strlen((char*)stconf.ssid) == 0 && os_strlen((char*)stconf.password) == 0) {
		os_strncpy((char*)stconf.ssid, VERS_STR(STA_SSID), 32);
		os_strncpy((char*)stconf.password, VERS_STR(STA_PASS), 64);
	  os_printf("WiFi Makefile setting connect: %s pw %s\n",
				(char*)stconf.ssid, (char*)stconf.password);
		wifi_set_opmode(3); // sta+ap, will switch to sta-only 15 secs after connecting
		stconf.bssid_set = 0;
		wifi_station_set_config(&stconf);
		}
	// }
#else
	set_softAP_conf();
#endif


	// init the flash filesystem with the html stuff
	espFsInit(&_binary_espfs_img_start);

	// mount the http handlers
	httpdInit(builtInUrls, 80);

	if (rfm69_spi_init()) {
		os_printf("RFM69 SPI Initialized\n");
		if (rfm69_init(RFM69_NODE_ID, RFM69_NET_ID)) {
			os_printf("RFM69 Configured. Listening ...\n");
			radioHandlerInit();
		} else {
			os_printf("RFM69 configuration failure.\n");
		}
	} else {
		os_printf("RFM69 SPI failure.\n");
	}

	#ifdef SHOW_HEAP_USE
		os_timer_disarm(&prHeapTimer);
		os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
		os_timer_arm(&prHeapTimer, 10000, 1);
	#endif

	struct rst_info *rst_info = system_get_rst_info();
	char resetMessage[600];
	os_sprintf(resetMessage,"Reset cause: %d=%s\nexccause=%d"
								"epc1=0x%x epc2=0x%x epc3=0x%x excvaddr=0x%x depc=0x%x\n",
								rst_info->reason, rst_codes[rst_info->reason],
								rst_info->exccause, rst_info->epc1, rst_info->epc2,
								rst_info->epc3, rst_info->excvaddr, rst_info->depc);
	os_printf("%s", resetMessage);
	os_printf("Flash map %d, chip %08X\n", system_get_flash_size_map(), spi_flash_get_id());

	cgiFlashInit();

	os_printf("** esp-rfm69 ready\n");
}
