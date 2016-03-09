#ifndef CGI_FS_H
#define CGI_FS_H

#define CGI_FS_EOF				(1<<1)
#define CGI_FS_STATICFS			(1<<2) //>>3 == 0
#define CGI_FS_DYNFS				(1<<3) //>>3 == 1

#define CGI_PAR_GETFS(x)			(x & (1>>3))

typedef struct {
	size_t size;
	uint8_t flags;
	char * name; //ptr to const chars
	int pix;
} CGI_SPIFFS_ENTRY;

typedef struct {
	CGI_SPIFFS_ENTRY *file;
	uint32_t readPos;
	uint8_t fs_flags;
} CGI_FILE_STATE_T;

typedef struct
{
	uint8_t api_state;
	CGI_FILE_STATE_T *f;
	char *buff;
} cgi_fs_state;

typedef struct {
	uint8_t state;
	char * bStr;
	char * filename;
	int total_size;
	int seq;
} api_cgi_upload_status;

int http_static_api(http_connection *c);
int http_fs_api(http_connection *c);

int http_fs_api_list(http_connection *c);
int http_fs_api_rename(http_connection *c);
int http_fs_api_delete(http_connection *c);
int http_fs_api_download(http_connection *c);
int http_fs_api_upload(http_connection *c);
int http_fs_api_fw(http_connection *c);

int http_fs_api_uploadfile(http_connection *c);

char * getFilename(char * buff);
char * getDataBegin(char * buff);
char * getBoundaryStr(char * buff);
int checkForLastPkt(char * buff, char * boundStr);

#endif
