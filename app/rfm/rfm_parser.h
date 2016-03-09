#ifndef RFM_PARSER_H
#define	RFM_PARSER_H
#include "c_types.h"
#include "rfm.h"
#include "radiohandler.h"

typedef void (*live_data_callback)(int x, int y, int z);  //callback function

void live_data_register_listener(live_data_callback f);

void live_event(uint32 stamp, uint8* buf, int len);
void default_event(uint32 stamp, uint8* buf, int len);

uint8_t serverCodeLUT(char fn_type, uint8_t indexedVal);

void process_rfm_msg(RFM_Handle *rfm, uint32 stamp);

extern uint8_t rfm_queue_t;
int txTotal;
int rxTotal;

#endif
