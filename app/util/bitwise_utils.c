#include "c_types.h"

#include "bitwise_utils.h"

uint8_t Byte_GetHighestOrderBit(uint8_t input)
{
	uint8_t output = 0;
	while (input >>= 1) {
		output++;
	}
	return output;
}
