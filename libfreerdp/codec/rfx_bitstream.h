/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CODEC_RFX_BITSTREAM_H
#define FREERDP_LIB_CODEC_RFX_BITSTREAM_H

#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/codec/rfx.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct
	{
		BYTE* buffer;
		uint32_t nbytes;
		uint32_t byte_pos;
		uint32_t bits_left;
	} RFX_BITSTREAM;

	static inline void rfx_bitstream_attach(RFX_BITSTREAM* bs, BYTE* WINPR_RESTRICT buffer,
	                                        size_t nbytes)
	{
		WINPR_ASSERT(bs);
		bs->buffer = (buffer);

		WINPR_ASSERT(nbytes <= UINT32_MAX);
		bs->nbytes = WINPR_ASSERTING_INT_CAST(uint32_t, nbytes);
		bs->byte_pos = 0;
		bs->bits_left = 8;
	}

	static inline uint32_t rfx_bitstream_get_bits(RFX_BITSTREAM* bs, uint32_t nbits)
	{
		UINT16 n = 0;
		while (bs->byte_pos < bs->nbytes && nbits > 0)
		{
			uint32_t b = nbits;
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

	static inline void rfx_bitstream_put_bits(RFX_BITSTREAM* bs, uint32_t _bits, uint32_t _nbits)
	{
		UINT16 bits = WINPR_ASSERTING_INT_CAST(UINT16, _bits);

		uint32_t nbits = (_nbits);
		while (bs->byte_pos < bs->nbytes && nbits > 0)
		{
			uint32_t b = nbits;
			if (b > bs->bits_left)
				b = bs->bits_left;
			bs->buffer[bs->byte_pos] |= ((bits >> (nbits - b)) & ((1 << b) - 1))
			                            << (bs->bits_left - b);
			bs->bits_left -= b;
			nbits -= b;
			if (bs->bits_left == 0)
			{
				bs->bits_left = 8;
				bs->byte_pos++;
			}
		}
	}

	static inline void rfx_bitstream_flush(RFX_BITSTREAM* bs)
	{
		WINPR_ASSERT(bs);
		if (bs->bits_left != 8)
		{
			uint32_t _nbits = 8 - bs->bits_left;
			rfx_bitstream_put_bits(bs, 0, _nbits);
		}
	}

	static inline BOOL rfx_bitstream_eos(RFX_BITSTREAM* bs)
	{
		WINPR_ASSERT(bs);
		return ((bs)->byte_pos >= (bs)->nbytes);
	}

	static inline uint32_t rfx_bitstream_left(RFX_BITSTREAM* bs)
	{
		WINPR_ASSERT(bs);

		if ((bs)->byte_pos >= (bs)->nbytes)
			return 0;

		return ((bs)->nbytes - (bs)->byte_pos - 1) * 8 + (bs)->bits_left;
	}

	static inline uint32_t rfx_bitstream_get_processed_bytes(RFX_BITSTREAM* bs)
	{
		WINPR_ASSERT(bs);
		if ((bs)->bits_left < 8)
			return (bs)->byte_pos + 1;
		return (bs)->byte_pos;
	}

#ifdef __cplusplus
}
#endif
#endif /* FREERDP_LIB_CODEC_RFX_BITSTREAM_H */
