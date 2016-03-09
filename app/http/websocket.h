#ifndef __WEBSOCKET_H
#define __WEBSOCKET_H

typedef void (*write_function)(const char *data,size_t len,void * arg);  //callback function

enum ws_frame_type {
   WS_CONTINUATION=0x00,
   WS_TEXT        =0x01,
   WS_BINARY      =0x02,
   WS_PING        =0x09,
   WS_PONG        =0x0A,
   WS_CLOSE       =0x08,
   WS_INVALID     =0xFF
};

typedef struct {
   uint8_t FIN;
   uint8_t MASKED;
   enum ws_frame_type TYPE;

   uint64_t SIZE;
   char * DATA;
} ws_frame;

void ws_parse_frame(ws_frame *frame,char * data,size_t dataLen);
void ws_output_frame(ws_frame *frame,enum ws_frame_type type,char * payload,size_t payloadSize);
void ws_write_frame(ws_frame *frame,write_function w,void *arg);

 #endif
