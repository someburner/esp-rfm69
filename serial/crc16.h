/**
 * \file
 *         Header file for the CRC16 calculcation
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

/** \addtogroup lib
 * @{ */

/**
 * \defgroup crc16 Cyclic Redundancy Check 16 (CRC16) calculation
 *
 * The Cyclic Redundancy Check 16 is a hash function that produces a
 * checksum that is used to detect errors in transmissions. The CRC16
 * calculation module is an iterative CRC calculator that can be used
 * to cumulatively update a CRC checksum for every incoming byte.
 *
 * @{
 */

#ifndef CRC16_H_
#define CRC16_H_
#ifdef	__cplusplus
extern "C" {
#endif
/**
 * \brief      Update an accumulated CRC16 checksum with one byte.
 * \param b    The byte to be added to the checksum
 * \param crc  The accumulated CRC that is to be updated.
 * \return     The updated CRC checksum.
 *
 *             This function updates an accumulated CRC16 checksum
 *             with one byte. It can be used as a running checksum, or
 *             to checksum an entire data block.
 *
 *             \note The algorithm used in this implementation is
 *             tailored for a running checksum and does not perform as
 *             well as a table-driven algorithm when checksumming an
 *             entire data block.
 *
 */
unsigned short crc16_add(unsigned char b, unsigned short crc);

/**
 * \brief      Calculate the CRC16 over a data area
 * \param data Pointer to the data
 * \param datalen The length of the data
 * \param acc  The accumulated CRC that is to be updated (or zero).
 * \return     The CRC16 checksum.
 *
 *             This function calculates the CRC16 checksum of a data area.
 *
 *             \note The algorithm used in this implementation is
 *             tailored for a running checksum and does not perform as
 *             well as a table-driven algorithm when checksumming an
 *             entire data block.
 */
unsigned short crc16_data(const unsigned char *data, int datalen,
			  unsigned short acc);
#ifdef	__cplusplus
}
#endif
#endif /* CRC16_H_ */

/** @} */
/** @} */
