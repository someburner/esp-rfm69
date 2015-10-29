#ifndef CONSOLE_H
#define CONSOLE_H

#include "httpd.h"

void consoleInit(void);
void ICACHE_FLASH_ATTR console_write_char(char c);
void ICACHE_FLASH_ATTR console_write_string(char* buff, int len);
int ajaxConsole(HttpdConnData *connData);
int ajaxConsoleClear(HttpdConnData *connData);
int ajaxConsoleFwUpdate(HttpdConnData *connData);
int ajaxConsoleSendData(HttpdConnData *connData);
int tplConsole(HttpdConnData *connData, char *token, void **arg);


#endif
