// Common platform functions

#ifndef __CONFIG_H__
#define __CONFIG_H__

#define MAGIC_NUM    0x1234

typedef struct {
	uint8_t bridgeId;
	uint8_t deviceId;
	uint8_t netId;
	uint8_t freq;
	char key[17]; //hashable
	uint8_t block[3];
	uint32_t magic;
} RFM_CONF_T;

typedef struct {
	char ssid[32]; //hashable
	char pass[64]; //hashable
	uint32_t magic;
} WIFI_CONF_T;

typedef struct {
	uint32_t endpt;
	char urlprefix[24];//hashable
	char urldomain[16];//hashable
	uint32_t magic;
} HTTP_CONF_T;

typedef enum {
	rfm_conf_type,
	wifi_conf_type,
	http_conf_type
} config_type_t;

int open_config_fd(config_type_t type, char rw);

int fill_Rfm_Struct(void * root, void* conf, int fd);
int fill_Wifi_Struct(void * root, void* conf, int fd);
int fill_Http_Struct(void * root, void* conf, int fd);


// RFM69 Configuration methods
// RFM_CONF_T*  get_rfm_conf(void);
// WIFI_CONF_T* get_wifi_conf(void);
// HTTP_CONF_T* get_http_conf(void);
int get_config_ptr(void * conf, config_type_t type);

int read_conf_from_fs(config_type_t type);

// int read_rfmconf(int fd, char * jsonStr);
// int read_wificonf(int fd, char * jsonStr);
// int read_httpconf(int fd, char * jsonStr);
int write_config_to_fs(void * conf, config_type_t type);

// Timer-specific functions

// System timer generic implemenation

// Filesystem-related functions

#endif // #ifndef __COMMON_H__
