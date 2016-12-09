/**
 * WinPR: Windows Portable Runtime
 * BitStream
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/print.h>
#include <winpr/bitstream.h>
#include "../trio/trio.h"

const char* BYTE_BIT_STRINGS_LSB[256] =
{
	"00000000", "00000001", "00000010", "00000011",
	"00000100", "00000101", "00000110", "00000111",
	"00001000", "00001001", "00001010", "00001011",
	"00001100", "00001101", "00001110", "00001111",
	"00010000", "00010001", "00010010", "00010011",
	"00010100", "00010101", "00010110", "00010111",
	"00011000", "00011001", "00011010", "00011011",
	"00011100", "00011101", "00011110", "00011111",
	"00100000", "00100001", "00100010", "00100011",
	"00100100", "00100101", "00100110", "00100111",
	"00101000", "00101001", "00101010", "00101011",
	"00101100", "00101101", "00101110", "00101111",
	"00110000", "00110001", "00110010", "00110011",
	"00110100", "00110101", "00110110", "00110111",
	"00111000", "00111001", "00111010", "00111011",
	"00111100", "00111101", "00111110", "00111111",
	"01000000", "01000001", "01000010", "01000011",
	"01000100", "01000101", "01000110", "01000111",
	"01001000", "01001001", "01001010", "01001011",
	"01001100", "01001101", "01001110", "01001111",
	"01010000", "01010001", "01010010", "01010011",
	"01010100", "01010101", "01010110", "01010111",
	"01011000", "01011001", "01011010", "01011011",
	"01011100", "01011101", "01011110", "01011111",
	"01100000", "01100001", "01100010", "01100011",
	"01100100", "01100101", "01100110", "01100111",
	"01101000", "01101001", "01101010", "01101011",
	"01101100", "01101101", "01101110", "01101111",
	"01110000", "01110001", "01110010", "01110011",
	"01110100", "01110101", "01110110", "01110111",
	"01111000", "01111001", "01111010", "01111011",
	"01111100", "01111101", "01111110", "01111111",
	"10000000", "10000001", "10000010", "10000011",
	"10000100", "10000101", "10000110", "10000111",
	"10001000", "10001001", "10001010", "10001011",
	"10001100", "10001101", "10001110", "10001111",
	"10010000", "10010001", "10010010", "10010011",
	"10010100", "10010101", "10010110", "10010111",
	"10011000", "10011001", "10011010", "10011011",
	"10011100", "10011101", "10011110", "10011111",
	"10100000", "10100001", "10100010", "10100011",
	"10100100", "10100101", "10100110", "10100111",
	"10101000", "10101001", "10101010", "10101011",
	"10101100", "10101101", "10101110", "10101111",
	"10110000", "10110001", "10110010", "10110011",
	"10110100", "10110101", "10110110", "10110111",
	"10111000", "10111001", "10111010", "10111011",
	"10111100", "10111101", "10111110", "10111111",
	"11000000", "11000001", "11000010", "11000011",
	"11000100", "11000101", "11000110", "11000111",
	"11001000", "11001001", "11001010", "11001011",
	"11001100", "11001101", "11001110", "11001111",
	"11010000", "11010001", "11010010", "11010011",
	"11010100", "11010101", "11010110", "11010111",
	"11011000", "11011001", "11011010", "11011011",
	"11011100", "11011101", "11011110", "11011111",
	"11100000", "11100001", "11100010", "11100011",
	"11100100", "11100101", "11100110", "11100111",
	"11101000", "11101001", "11101010", "11101011",
	"11101100", "11101101", "11101110", "11101111",
	"11110000", "11110001", "11110010", "11110011",
	"11110100", "11110101", "11110110", "11110111",
	"11111000", "11111001", "11111010", "11111011",
	"11111100", "11111101", "11111110", "11111111"
};

const char* BYTE_BIT_STRINGS_MSB[256] =
{
	"00000000", "10000000", "01000000", "11000000",
	"00100000", "10100000", "01100000", "11100000",
	"00010000", "10010000", "01010000", "11010000",
	"00110000", "10110000", "01110000", "11110000",
	"00001000", "10001000", "01001000", "11001000",
	"00101000", "10101000", "01101000", "11101000",
	"00011000", "10011000", "01011000", "11011000",
	"00111000", "10111000", "01111000", "11111000",
	"00000100", "10000100", "01000100", "11000100",
	"00100100", "10100100", "01100100", "11100100",
	"00010100", "10010100", "01010100", "11010100",
	"00110100", "10110100", "01110100", "11110100",
	"00001100", "10001100", "01001100", "11001100",
	"00101100", "10101100", "01101100", "11101100",
	"00011100", "10011100", "01011100", "11011100",
	"00111100", "10111100", "01111100", "11111100",
	"00000010", "10000010", "01000010", "11000010",
	"00100010", "10100010", "01100010", "11100010",
	"00010010", "10010010", "01010010", "11010010",
	"00110010", "10110010", "01110010", "11110010",
	"00001010", "10001010", "01001010", "11001010",
	"00101010", "10101010", "01101010", "11101010",
	"00011010", "10011010", "01011010", "11011010",
	"00111010", "10111010", "01111010", "11111010",
	"00000110", "10000110", "01000110", "11000110",
	"00100110", "10100110", "01100110", "11100110",
	"00010110", "10010110", "01010110", "11010110",
	"00110110", "10110110", "01110110", "11110110",
	"00001110", "10001110", "01001110", "11001110",
	"00101110", "10101110", "01101110", "11101110",
	"00011110", "10011110", "01011110", "11011110",
	"00111110", "10111110", "01111110", "11111110",
	"00000001", "10000001", "01000001", "11000001",
	"00100001", "10100001", "01100001", "11100001",
	"00010001", "10010001", "01010001", "11010001",
	"00110001", "10110001", "01110001", "11110001",
	"00001001", "10001001", "01001001", "11001001",
	"00101001", "10101001", "01101001", "11101001",
	"00011001", "10011001", "01011001", "11011001",
	"00111001", "10111001", "01111001", "11111001",
	"00000101", "10000101", "01000101", "11000101",
	"00100101", "10100101", "01100101", "11100101",
	"00010101", "10010101", "01010101", "11010101",
	"00110101", "10110101", "01110101", "11110101",
	"00001101", "10001101", "01001101", "11001101",
	"00101101", "10101101", "01101101", "11101101",
	"00011101", "10011101", "01011101", "11011101",
	"00111101", "10111101", "01111101", "11111101",
	"00000011", "10000011", "01000011", "11000011",
	"00100011", "10100011", "01100011", "11100011",
	"00010011", "10010011", "01010011", "11010011",
	"00110011", "10110011", "01110011", "11110011",
	"00001011", "10001011", "01001011", "11001011",
	"00101011", "10101011", "01101011", "11101011",
	"00011011", "10011011", "01011011", "11011011",
	"00111011", "10111011", "01111011", "11111011",
	"00000111", "10000111", "01000111", "11000111",
	"00100111", "10100111", "01100111", "11100111",
	"00010111", "10010111", "01010111", "11010111",
	"00110111", "10110111", "01110111", "11110111",
	"00001111", "10001111", "01001111", "11001111",
	"00101111", "10101111", "01101111", "11101111",
	"00011111", "10011111", "01011111", "11011111",
	"00111111", "10111111", "01111111", "11111111"
};

void BitDump(const char* tag, UINT32 level, const BYTE* buffer, UINT32 length, UINT32 flags)
{
	DWORD i;
	int nbits;
	const char* str;
	const char** strs;
	char pbuffer[64 * 8 + 1];
	size_t pos = 0;
	strs = (flags & BITDUMP_MSB_FIRST) ? BYTE_BIT_STRINGS_MSB : BYTE_BIT_STRINGS_LSB;

	for (i = 0; i < length; i += 8)
	{
		str = strs[buffer[i / 8]];
		nbits = (length - i) > 8 ? 8 : (length - i);
		pos += trio_snprintf(&pbuffer[pos], length - pos, "%.*s ", nbits, str);

		if ((i % 64) == 0)
		{
			pos = 0;
			WLog_LVL(tag, level, "%s", pbuffer);
		}
	}

	if (i)
		WLog_LVL(tag, level, "%s ", pbuffer);
}

UINT32 ReverseBits32(UINT32 bits, UINT32 nbits)
{
	UINT32 rbits = 0;

	do
	{
		rbits = (rbits | (bits & 1)) << 1;
		bits >>= 1;
		nbits--;
	}
	while (nbits > 0);

	rbits >>= 1;
	return rbits;
}

#if 0

/**
 * These are the original functions from which the macros are derived.
 * Since it is much easier to develop and debug functions than macros,
 * we keep a copy here for later improvements and modifications.
 */

WINPR_API void BitStream_Prefetch(wBitStream* bs);
WINPR_API void BitStream_Fetch(wBitStream* bs);
WINPR_API void BitStream_Flush(wBitStream* bs);
WINPR_API void BitStream_Shift(wBitStream* bs, UINT32 nbits);
WINPR_API void BitStream_Write_Bits(wBitStream* bs, UINT32 bits, UINT32 nbits);

void BitStream_Prefetch(wBitStream* bs)
{
	(bs->prefetch) = 0;

	if ((bs->pointer - bs->buffer) < (bs->capacity + 4))
		(bs->prefetch) |= (*(bs->pointer + 4) << 24);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 5))
		(bs->prefetch) |= (*(bs->pointer + 5) << 16);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 6))
		(bs->prefetch) |= (*(bs->pointer + 6) << 8);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 7))
		(bs->prefetch) |= (*(bs->pointer + 7) << 0);
}

void BitStream_Fetch(wBitStream* bs)
{
	(bs->accumulator) = 0;

	if ((bs->pointer - bs->buffer) < (bs->capacity + 0))
		(bs->accumulator) |= (*(bs->pointer + 0) << 24);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 1))
		(bs->accumulator) |= (*(bs->pointer + 1) << 16);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 2))
		(bs->accumulator) |= (*(bs->pointer + 2) << 8);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 3))
		(bs->accumulator) |= (*(bs->pointer + 3) << 0);

	BitStream_Prefetch(bs);
}

void BitStream_Flush(wBitStream* bs)
{
	if ((bs->pointer - bs->buffer) < (bs->capacity + 0))
		*(bs->pointer + 0) = (bs->accumulator >> 24);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 1))
		*(bs->pointer + 1) = (bs->accumulator >> 16);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 2))
		*(bs->pointer + 2) = (bs->accumulator >> 8);

	if ((bs->pointer - bs->buffer) < (bs->capacity + 3))
		*(bs->pointer + 3) = (bs->accumulator >> 0);
}

void BitStream_Shift(wBitStream* bs, UINT32 nbits)
{
	bs->accumulator <<= nbits;
	bs->position += nbits;
	bs->offset += nbits;

	if (bs->offset < 32)
	{
		bs->mask = ((1 << nbits) - 1);
		bs->accumulator |= ((bs->prefetch >> (32 - nbits)) & bs->mask);
		bs->prefetch <<= nbits;
	}
	else
	{
		bs->mask = ((1 << nbits) - 1);
		bs->accumulator |= ((bs->prefetch >> (32 - nbits)) & bs->mask);
		bs->prefetch <<= nbits;
		bs->offset -= 32;
		bs->pointer += 4;
		BitStream_Prefetch(bs);

		if (bs->offset)
		{
			bs->mask = ((1 << bs->offset) - 1);
			bs->accumulator |= ((bs->prefetch >> (32 - bs->offset)) & bs->mask);
			bs->prefetch <<= bs->offset;
		}
	}
}

void BitStream_Write_Bits(wBitStream* bs, UINT32 bits, UINT32 nbits)
{
	bs->position += nbits;
	bs->offset += nbits;

	if (bs->offset < 32)
	{
		bs->accumulator |= (bits << (32 - bs->offset));
	}
	else
	{
		bs->offset -= 32;
		bs->mask = ((1 << (nbits - bs->offset)) - 1);
		bs->accumulator |= ((bits >> bs->offset) & bs->mask);
		BitStream_Flush(bs);
		bs->accumulator = 0;
		bs->pointer += 4;

		if (bs->offset)
		{
			bs->mask = ((1 << bs->offset) - 1);
			bs->accumulator |= ((bits & bs->mask) << (32 - bs->offset));
		}
	}
}

#endif

void BitStream_Attach(wBitStream* bs, const BYTE* buffer, UINT32 capacity)
{
	bs->position = 0;
	bs->buffer = buffer;
	bs->offset = 0;
	bs->accumulator = 0;
	bs->pointer = (BYTE*) bs->buffer;
	bs->capacity = capacity;
	bs->length = bs->capacity * 8;
}

wBitStream* BitStream_New()
{
	wBitStream* bs = NULL;
	bs = (wBitStream*) calloc(1, sizeof(wBitStream));

	return bs;
}

void BitStream_Free(wBitStream* bs)
{
	if (!bs)
		return;

	free(bs);
}
