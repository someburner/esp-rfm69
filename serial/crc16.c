/** \addtogroup crc16
 * @{ */

/**
 * \file
 *         Implementation of the CRC16 calculcation
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

/* CITT CRC16 polynomial ^16 + ^12 + ^5 + 1 */
/*---------------------------------------------------------------------------*/
unsigned short
crc16_add(unsigned char b, unsigned short acc)
{
  /*
    acc  = (unsigned char)(acc >> 8) | (acc << 8);
    acc ^= b;
    acc ^= (unsigned char)(acc & 0xff) >> 4;
    acc ^= (acc << 8) << 4;
    acc ^= ((acc & 0xff) << 4) << 1;
  */

  acc ^= b;
  acc  = (acc >> 8) | (acc << 8);
  acc ^= (acc & 0xff00) << 4;
  acc ^= (acc >> 8) >> 4;
  acc ^= (acc & 0xff00) >> 5;
  return acc;
}
/*---------------------------------------------------------------------------*/
unsigned short
crc16_data(const unsigned char *data, int len, unsigned short acc)
{
  int i;
  
  for(i = 0; i < len; ++i) {
    acc = crc16_add(*data, acc);
    ++data;
  }
  return acc;
}
/*---------------------------------------------------------------------------*/

/** @} */
