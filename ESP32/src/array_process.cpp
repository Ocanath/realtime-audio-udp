#include "array_process.h"

/*
non-unit tested helper function
*/
void process_32bit_to_16bit(unsigned char * parr, size_t arr_size, size_t * newlen)
{
	size_t bidx_16b = 0;
	size_t bidx_32b = 0;
	for(int i = 0; i < arr_size / sizeof(int32_t); i++)
	{
		int32_t * pi32 = (int32_t*)(parr) + bidx_32b*sizeof(int32_t);
		bidx_32b += sizeof(int32_t);
		int16_t val_div = (*pi32 / 0x10000);
		int16_t * pi16 = (int16_t*)(parr) + bidx_16b*sizeof(int16_t);
		*pi16 = val_div;
	}
	*newlen = (arr_size*sizeof(int16_t))/sizeof(int32_t);
}
