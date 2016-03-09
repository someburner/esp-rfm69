// Platform-dependent functions
#include "platform.h"
#include "common.h"
#include "c_stdio.h"
#include "c_stdlib.h"
#include "gpio.h"
#include "user_interface.h"
#include "driver/uart.h"

void output_redirect(const char *str){

  #ifdef DEVELOP_VERSION
     uart_write_string(0,str);
  #endif
}

int platform_init()
{
  cmn_platform_init();
  // All done
  return PLATFORM_OK;
}

// ****************************************************************************
// UART
// TODO: Support timeouts.

// Send: version with and without mux
void platform_uart_send( unsigned id, u8 data)
{
   uart_write_char(id, data);
}

// ****************************************************************************
// Flash access functions

/*
 * Assumptions:
 * > toaddr is INTERNAL_FLASH_WRITE_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_WRITE_UNIT_SIZE
 */
uint32_t platform_s_flash_write( const void *from, uint32_t toaddr, uint32_t size )
{
  SpiFlashOpResult r;
  const uint32_t blkmask = INTERNAL_FLASH_WRITE_UNIT_SIZE - 1;
  uint32_t *apbuf = NULL;
  uint32_t fromaddr = (uint32_t)from;
  if( (fromaddr & blkmask ) || (fromaddr >= INTERNAL_FLASH_MAPPED_ADDRESS)) {
    apbuf = (uint32_t *)c_malloc(size);
    if(!apbuf)
      return 0;
    memcpy(apbuf, from, size);
  }
  system_soft_wdt_feed ();
  r = flash_write(toaddr, apbuf?(uint32 *)apbuf:(uint32 *)from, size);
  if(apbuf)
    c_free(apbuf);
  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_write: r=%d at %08X\n", ( int )r, ( unsigned )toaddr);
    return 0;
  }
}

/*
 * Assumptions:
 * > fromaddr is INTERNAL_FLASH_READ_UNIT_SIZE aligned
 * > size is a multiple of INTERNAL_FLASH_READ_UNIT_SIZE
 */
uint32_t platform_s_flash_read( void *to, uint32_t fromaddr, uint32_t size )
{
  if (size==0)
    return 0;

  SpiFlashOpResult r;
  system_soft_wdt_feed ();

  const uint32_t blkmask = (INTERNAL_FLASH_READ_UNIT_SIZE - 1);
  if( ((uint32_t)to) & blkmask )
  {
    uint32_t size2=size-INTERNAL_FLASH_READ_UNIT_SIZE;
    uint32* to2=(uint32*)((((uint32_t)to)&(~blkmask))+INTERNAL_FLASH_READ_UNIT_SIZE);
    r = flash_read(fromaddr, to2, size2);
    if(SPI_FLASH_RESULT_OK == r)
    {
      os_memmove(to,to2,size2);
      char back[ INTERNAL_FLASH_READ_UNIT_SIZE ] __attribute__ ((aligned(INTERNAL_FLASH_READ_UNIT_SIZE)));
      r=flash_read(fromaddr+size2,(uint32*)back,INTERNAL_FLASH_READ_UNIT_SIZE);
      os_memcpy((uint8_t*)to+size2,back,INTERNAL_FLASH_READ_UNIT_SIZE);
    }
  }
  else
    r = flash_read(fromaddr, (uint32 *)to, size);

  if(SPI_FLASH_RESULT_OK == r)
    return size;
  else{
    NODE_ERR( "ERROR in flash_read: r=%d at %08X\n", ( int )r, ( unsigned )fromaddr);
    return 0;
  }
}

int platform_flash_erase_sector( uint32_t sector_id )
{
  system_soft_wdt_feed ();
  return flash_erase( sector_id ) == SPI_FLASH_RESULT_OK ? PLATFORM_OK : PLATFORM_ERR;
}

uint32_t platform_flash_mapped2phys (uint32_t mapped_addr)
{
  uint32_t cache_ctrl = READ_PERI_REG(CACHE_FLASH_CTRL_REG);
  if (!(cache_ctrl & CACHE_FLASH_ACTIVE))
    return -1;
  bool b0 = (cache_ctrl & CACHE_FLASH_MAPPED0) ? 1 : 0;
  bool b1 = (cache_ctrl & CACHE_FLASH_MAPPED1) ? 1 : 0;
  uint32_t meg = (b1 << 1) | b0;
  return mapped_addr - INTERNAL_FLASH_MAPPED_ADDRESS + meg * 0x100000;
}
