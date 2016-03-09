#ifndef CGI_CONSOLE_H
#define CGI_CONSOLE_H

#define BUF_MAX (128)


typedef enum {
	cons_start_t,
	cons_pwr_t,
	cons_to_node_t,
	cons_disp_t,
} cons_method_t;

typedef enum {
	power_on_btn, //0
	power_off_btn,
} console_btn_t;

int http_console_api(http_connection *c);
int http_console_api_clear(http_connection *c);
int http_console_api_rfm69_info(http_connection *c);

int http_console_api_server(http_connection *c);

#endif
