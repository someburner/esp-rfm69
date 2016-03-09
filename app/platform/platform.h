// Platform-specific functions
#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "cpu_esp8266.h"

#include "c_types.h"
// Error / status codes
enum
{
  PLATFORM_ERR,
  PLATFORM_OK,
  PLATFORM_UNDERFLOW = -1
};

// Platform initialization
int platform_init(void);
void platform_int_init(void);

// *****************************************************************************
// Timer subsection

// Timer data type
typedef uint32_t timer_data_type;

// *****************************************************************************
// CAN subsection

// Maximum length for any CAN message
#define PLATFORM_CAN_MAXLEN                   8

// eLua CAN ID types
enum
{
  ELUA_CAN_ID_STD = 0,
  ELUA_CAN_ID_EXT
};

int platform_can_exists( unsigned id );
uint32_t platform_can_setup( unsigned id, uint32_t clock );
int platform_can_send( unsigned id, uint32_t canid, uint8_t idtype, uint8_t len, const uint8_t *data );
int platform_can_recv( unsigned id, uint32_t *canid, uint8_t *idtype, uint8_t *len, uint8_t *data );

// *****************************************************************************
// UART subsection

// There are 3 "virtual" UART ports (UART0...UART2).
#define PLATFORM_UART_TOTAL                   3
// TODO: PLATFORM_UART_TOTAL is not used - figure out purpose, or remove?
// Note: Some CPUs (e.g. LM4F/TM4C) have more than 3 hardware UARTs

// Parity
enum
{
  PLATFORM_UART_PARITY_EVEN,
  PLATFORM_UART_PARITY_ODD,
  PLATFORM_UART_PARITY_NONE,
  PLATFORM_UART_PARITY_MARK,
  PLATFORM_UART_PARITY_SPACE
};

// Stop bits
enum
{
  PLATFORM_UART_STOPBITS_1,
  PLATFORM_UART_STOPBITS_1_5,
  PLATFORM_UART_STOPBITS_2
};

// Flow control types (this is a bit mask, one can specify PLATFORM_UART_FLOW_RTS | PLATFORM_UART_FLOW_CTS )
#define PLATFORM_UART_FLOW_NONE               0
#define PLATFORM_UART_FLOW_RTS                1
#define PLATFORM_UART_FLOW_CTS                2

// The platform UART functions
int platform_uart_exists( unsigned id );
uint32_t platform_uart_setup( unsigned id, uint32_t baud, int databits, int parity, int stopbits );
int platform_uart_set_buffer( unsigned id, unsigned size );
void platform_uart_send( unsigned id, uint8_t data );
void platform_s_uart_send( unsigned id, uint8_t data );
int platform_uart_recv( unsigned id, unsigned timer_id, timer_data_type timeout );
int platform_s_uart_recv( unsigned id, timer_data_type timeout );
int platform_uart_set_flow_control( unsigned id, int type );
int platform_s_uart_set_flow_control( unsigned id, int type );

// *****************************************************************************
// Ethernet specific functions

void platform_eth_send_packet( const void* src, uint32_t size );
uint32_t platform_eth_get_packet_nb( void* buf, uint32_t maxlen );
void platform_eth_force_interrupt(void);
uint32_t platform_eth_get_elapsed_time(void);

// *****************************************************************************
// Internal flash erase/write functions

uint32_t platform_flash_get_first_free_block_address( uint32_t *psect );
uint32_t platform_flash_get_sector_of_address( uint32_t addr );
uint32_t platform_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size );
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size );
uint32_t platform_flash_get_num_sectors(void);
int platform_flash_erase_sector( uint32_t sector_id );

/**
 * Translated a mapped address to a physical flash address, based on the
 * current flash cache mapping.
 * @param mapped_addr Address to translate (>= INTERNAL_FLASH_MAPPED_ADDRESS)
 * @return the corresponding physical flash address, or -1 if flash cache is
 *  not currently active.
 * @see Cache_Read_Enable.
 */
uint32_t platform_flash_mapped2phys (uint32_t mapped_addr);

// *****************************************************************************
// Allocator support

void* platform_get_first_free_ram( unsigned id );
void* platform_get_last_free_ram( unsigned id );

#endif
