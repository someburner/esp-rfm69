#include "user_interface.h"
#include "user_config.h"
#include "mem.h"

#include "flash_fs.h"
#include "config.h"

// Filenames
#define RFM_CONF_NAME "rfm.conf"
#define WIFI_CONF_NAME "wifi.conf"

#define GET_STR_LIT(V) #V
#define STR_LIT(V) GET_STR_LIT(V)

RFM_CONF_T rfmConf =
{
   RFM69_NODE_ID,       //uint8_t bridgeId;
   RFM69_DEV_ID,        //uint8_t deviceId;
   RFM69_NET_ID,        //uint8_t netId;
   RFM69_FREQ,          //uint8_t freq;
   RFM69_ENCRYPT_KEY"\0",   // char[16] key
   {0,0,0},             //alignment block
   0,                   //magic
};

WIFI_CONF_T wifiConf =
{
   STR_LIT(STA_SSID)"\0",
   STR_LIT(STA_PASS)"\0",
   0           //magic
};

// Function pointer type + array
typedef int (*conf_w_t)(void * root, void* conf, int fd);

static conf_w_t c_fn[3] = {
   fill_Rfm_Struct,
   fill_Wifi_Struct,
   fill_Http_Struct
};

// JSON check macros
#define CONFIG_CHECK_NULL_VAL(v) \
if ((v)==NULL) { \
  NODE_DBG("ERR: NULL Value\n"); \
 return -1; \
}

int open_config_fd(config_type_t type, char rw)
{
   int fd = -1;
   int flags = 0;
   if       (rw == 'r') { flags = FS_RDONLY; }
   else if  (rw == 'w') { flags = FS_CREAT | FS_TRUNC | FS_RDWR; }
   else if  (rw == 'a') { flags = FS_RDWR; }

   switch (type)
   {
      case rfm_conf_type:  { fd = fs_open(RFM_CONF_NAME,  flags); } break;
      case wifi_conf_type: { fd = fs_open(WIFI_CONF_NAME, flags); } break;
      default: { fd = -1; }
   }
   return fd;
}


int write_config_to_fs(void * conf, config_type_t type)
{
   int fd = open_config_fd(type, 'w');
   if (fd < 0) { return -1; }
   int ret = 1;

   switch (type)
   {
      case rfm_conf_type:
      {
         RFM_CONF_T * newRfmConf = (RFM_CONF_T* )conf;
         ret = fs_write(fd, newRfmConf, sizeof(RFM_CONF_T));
         if (ret >= 0) { rfmConf = *newRfmConf; }
      } break;
      case wifi_conf_type:
      {
         WIFI_CONF_T * newWifiConf = (WIFI_CONF_T* )conf;
         ret = fs_write(fd,newWifiConf, sizeof(WIFI_CONF_T));
      } break;

      default: { fd = -1; }
   }

   if (ret < 0) { NODE_ERR("fs write err %i\n", fs_errno()); }
   if (ret >= 0) { NODE_DBG("Wrote %d bytes\n", ret); }

   fs_close(fd);

   return ret;
}


int read_conf_from_fs(config_type_t type)
{
   int fd = open_config_fd(type, 'r');
   if (fd <= 0)
   {
      NODE_DBG("%d does not exist, creating...\n", type);
      int writeOK = 0;
      switch (type)
      {
         case rfm_conf_type: {
            writeOK = write_config_to_fs(&rfmConf, rfm_conf_type);
            if (writeOK) { rfmConf.magic = MAGIC_NUM; }
            return writeOK;
         } break;
         case wifi_conf_type: {
            writeOK = write_config_to_fs(&wifiConf, wifi_conf_type);
            if (writeOK) { wifiConf.magic = MAGIC_NUM; }
            return writeOK;
         } break;
         default: return -1;
      }
   }

   /* Config exists. Read in Json */
   NODE_DBG("file exists\n");
   int res = -1;

   res = fs_seek(fd, 0, FS_SEEK_END);
   if (res < 0)     { fs_close(fd); fd = 0; return res; }

   int size = fs_tell(fd);
   if (size <= 0)   { fs_close(fd); fd = 0; return size; }

   res = fs_seek(fd, 0, FS_SEEK_SET);
   if (res < 0)     { fs_close(fd); fd = 0; return res; }

   NODE_DBG("found. size = %d\n", size);
   uint8_t * file_in = (uint8_t*)os_malloc(size);
   int ret = fs_read(fd, file_in, size);
   fs_close(fd);

   size_t expected = 0;
   switch (type) {
      case rfm_conf_type:
      {
         expected = sizeof(RFM_CONF_T);
         if (ret == expected)
            { memcpy(&rfmConf, file_in, expected); os_free(file_in); return 1; }
      } break;
      case wifi_conf_type:
      {
         expected = sizeof(WIFI_CONF_T);
         if (ret == expected)
            { memcpy(&wifiConf, file_in, expected); os_free(file_in); return 1; }
      } break;
   }
   os_free(file_in);
   NODE_DBG("filesize mismatch: expected %d, got %d\n",(int) expected, ret);
   return -1;
}

int fill_Rfm_Struct(void* rt, void* conf, int fd)
{
   RFM_CONF_T * newRfmConf = (RFM_CONF_T* )conf;
   rfmConf.bridgeId = newRfmConf->bridgeId;
   rfmConf.deviceId = newRfmConf->deviceId;
   rfmConf.netId = newRfmConf->netId;
   memcpy(rfmConf.key, newRfmConf->key, 16);
   return 1;
}

int fill_Wifi_Struct(void* rt, void* conf, int fd)
{
   WIFI_CONF_T * newWifiConf = (WIFI_CONF_T* )conf;

   memset(wifiConf.ssid, 0, 32);
   memset(wifiConf.pass, 0, 64);
   memcpy(wifiConf.ssid, newWifiConf->ssid, 32);
   memcpy(wifiConf.pass, newWifiConf->pass, 64);
   wifiConf.magic = MAGIC_NUM;

   return 1;
}

int get_config_ptr(void * conf, config_type_t type)
{
   int ret = -1;
   switch (type) {
      case rfm_conf_type:
      {
         RFM_CONF_T * rtemp;
         RFM_CONF_T * rtemp2;
         if (rfmConf.magic != MAGIC_NUM)
         {
            ret = read_conf_from_fs(rfm_conf_type);
            if (ret) {
               rtemp = &rfmConf;
               rtemp2 = (RFM_CONF_T*)conf;
               *rtemp2 =  *rtemp;
            }
         } else {
            rtemp = &rfmConf;
            rtemp2 = (RFM_CONF_T*)conf;
            *rtemp2 =  *rtemp;
            ret = 1;
         }
      } break;
      case wifi_conf_type:
      {
         WIFI_CONF_T * wtemp;
         WIFI_CONF_T * wtemp2;
         if (wifiConf.magic != MAGIC_NUM)
         {
            ret = read_conf_from_fs(wifi_conf_type);
            if (ret) {
               wtemp = &wifiConf;
               wtemp2 = (WIFI_CONF_T*)conf;
               *wtemp2 =  *wtemp;
            }
         } else {
            wtemp = &wifiConf;
            wtemp2 = (WIFI_CONF_T*)conf;
            *wtemp2 =  *wtemp;
            ret = 1;
         }
      } break;
   }
   if (ret > 0)
   {
      NODE_DBG("fs ok!\n");
   } else {
      NODE_DBG("error opening\n");
   }
   return ret;
}

RFM_CONF_T * get_rfm_conf(void)
{
   return &rfmConf;
}

WIFI_CONF_T * get_wifi_conf(void)
{
   return &wifiConf;
}
