/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Bit Stream
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>

#include "rfx_bitstream.h"

void rfx_bitstream_attach(RFX_BITSTREAM* bs, uint8* buffer, int nbytes)
{
	bs->buffer = buffer;
	bs->nbytes = nbytes;
	bs->byte_pos = 0;
	bs->bits_left = 8;
}

uint16 rfx_bitstream_get_bits(RFX_BITSTREAM* bs, int nbits)
{
	int b;
	uint16 n = 0;

	while (bs->byte_pos < bs->nbytes && nbits > 0)
	{
		b = nbits;

		if (b > bs->bits_left)
			b = bs->bits_left;

		if (n)
			n <<= b;

		n |= (bs->buffer[bs->byte_pos] >> (bs->bits_left - b)) & ((1 << b) - 1);
		bs->bits_left -= b;
		nbits -= b;

		if (bs->bits_left == 0)
		{
			bs->bits_left = 8;
			bs->byte_pos++;
		}
	}

	return n;
}

void rfx_bitstream_put_bits(RFX_BITSTREAM* bs, uint16 bits, int nbits)
{
	int b;

	while (bs->byte_pos < bs->nbytes && nbits > 0)
	{
		b = nbits;

		if (b > bs->bits_left)
			b = bs->bits_left;

		bs->buffer[bs->byte_pos] |= ((bits >> (nbits - b)) & ((1 << b) - 1)) << (bs->bits_left - b);
		bs->bits_left -= b;
		nbits -= b;

		if (bs->bits_left == 0)
		{
			bs->bits_left = 8;
			bs->byte_pos++;
		}
	}
}
