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

#ifndef __RFX_BITSTREAM_H
#define __RFX_BITSTREAM_H

#include <freerdp/codec/rfx.h>

struct _RFX_BITSTREAM
{
	uint8* buffer;
	int nbytes;
	int byte_pos;
	int bits_left;
};
typedef struct _RFX_BITSTREAM RFX_BITSTREAM;

#define rfx_bitstream_attach(bs, _buffer, _nbytes) do { \
	bs->buffer = (uint8*) (_buffer); \
	bs->nbytes = (_nbytes); \
	bs->byte_pos = 0; \
	bs->bits_left = 8; } while (0)

#define rfx_bitstream_get_bits(bs, _nbits, _r) do { \
	int nbits = _nbits; \
	int b; \
	uint16 n = 0; \
	while (bs->byte_pos < bs->nbytes && nbits > 0) \
	{ \
		b = nbits; \
		if (b > bs->bits_left) \
			b = bs->bits_left; \
		if (n) \
			n <<= b; \
		n |= (bs->buffer[bs->byte_pos] >> (bs->bits_left - b)) & ((1 << b) - 1); \
		bs->bits_left -= b; \
		nbits -= b; \
		if (bs->bits_left == 0) \
		{ \
			bs->bits_left = 8; \
			bs->byte_pos++; \
		} \
	} \
	_r = n; } while (0)

#define rfx_bitstream_put_bits(bs, _bits, _nbits) do { \
	uint16 bits = (_bits); \
	int nbits = (_nbits); \
	int b; \
	while (bs->byte_pos < bs->nbytes && nbits > 0) \
	{ \
		b = nbits; \
		if (b > bs->bits_left) \
			b = bs->bits_left; \
		bs->buffer[bs->byte_pos] &= ~(((1 << b) - 1) << (bs->bits_left - b)); \
		bs->buffer[bs->byte_pos] |= ((bits >> (nbits - b)) & ((1 << b) - 1)) << (bs->bits_left - b); \
		bs->bits_left -= b; \
		nbits -= b; \
		if (bs->bits_left == 0) \
		{ \
			bs->bits_left = 8; \
			bs->byte_pos++; \
		} \
	} } while (0)

#define rfx_bitstream_eos(_bs) ((_bs)->byte_pos >= (_bs)->nbytes)
#define rfx_bitstream_left(_bs) ((_bs)->byte_pos >= (_bs)->nbytes ? 0 : ((_bs)->nbytes - (_bs)->byte_pos - 1) * 8 + (_bs)->bits_left)
#define rfx_bitstream_get_processed_bytes(_bs) ((_bs)->bits_left < 8 ? (_bs)->byte_pos + 1 : (_bs)->byte_pos)

#endif /* __RFX_BITSTREAM_H */
