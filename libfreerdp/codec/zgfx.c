/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ZGFX (RDP8) Bulk Data Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/log.h>
#include <freerdp/codec/zgfx.h>

#define TAG FREERDP_TAG("codec")

/**
 * RDP8 Compressor Limits:
 *
 * Maximum number of uncompressed bytes in a single segment: 65535
 * Maximum match distance / minimum history size: 2500000 bytes.
 * Maximum number of segments: 65535
 * Maximum expansion of a segment (when compressed size exceeds uncompressed): 1000 bytes
 * Minimum match length: 3 bytes
 */

typedef struct
{
	UINT32 prefixLength;
	UINT32 prefixCode;
	UINT32 valueBits;
	UINT32 tokenType;
	UINT32 valueBase;
} ZGFX_TOKEN;

struct S_ZGFX_CONTEXT
{
	BOOL Compressor;

	const BYTE* pbInputCurrent;
	const BYTE* pbInputEnd;

	UINT32 bits;
	UINT32 cBitsRemaining;
	UINT32 BitsCurrent;
	UINT32 cBitsCurrent;

	BYTE OutputBuffer[65536];
	UINT32 OutputCount;

	BYTE HistoryBuffer[2500000];
	UINT32 HistoryIndex;
	UINT32 HistoryBufferSize;
};

static const ZGFX_TOKEN ZGFX_TOKEN_TABLE[] = {
	// len code vbits type  vbase
	{ 1, 0, 8, 0, 0 },           // 0
	{ 5, 17, 5, 1, 0 },          // 10001
	{ 5, 18, 7, 1, 32 },         // 10010
	{ 5, 19, 9, 1, 160 },        // 10011
	{ 5, 20, 10, 1, 672 },       // 10100
	{ 5, 21, 12, 1, 1696 },      // 10101
	{ 5, 24, 0, 0, 0x00 },       // 11000
	{ 5, 25, 0, 0, 0x01 },       // 11001
	{ 6, 44, 14, 1, 5792 },      // 101100
	{ 6, 45, 15, 1, 22176 },     // 101101
	{ 6, 52, 0, 0, 0x02 },       // 110100
	{ 6, 53, 0, 0, 0x03 },       // 110101
	{ 6, 54, 0, 0, 0xFF },       // 110110
	{ 7, 92, 18, 1, 54944 },     // 1011100
	{ 7, 93, 20, 1, 317088 },    // 1011101
	{ 7, 110, 0, 0, 0x04 },      // 1101110
	{ 7, 111, 0, 0, 0x05 },      // 1101111
	{ 7, 112, 0, 0, 0x06 },      // 1110000
	{ 7, 113, 0, 0, 0x07 },      // 1110001
	{ 7, 114, 0, 0, 0x08 },      // 1110010
	{ 7, 115, 0, 0, 0x09 },      // 1110011
	{ 7, 116, 0, 0, 0x0A },      // 1110100
	{ 7, 117, 0, 0, 0x0B },      // 1110101
	{ 7, 118, 0, 0, 0x3A },      // 1110110
	{ 7, 119, 0, 0, 0x3B },      // 1110111
	{ 7, 120, 0, 0, 0x3C },      // 1111000
	{ 7, 121, 0, 0, 0x3D },      // 1111001
	{ 7, 122, 0, 0, 0x3E },      // 1111010
	{ 7, 123, 0, 0, 0x3F },      // 1111011
	{ 7, 124, 0, 0, 0x40 },      // 1111100
	{ 7, 125, 0, 0, 0x80 },      // 1111101
	{ 8, 188, 20, 1, 1365664 },  // 10111100
	{ 8, 189, 21, 1, 2414240 },  // 10111101
	{ 8, 252, 0, 0, 0x0C },      // 11111100
	{ 8, 253, 0, 0, 0x38 },      // 11111101
	{ 8, 254, 0, 0, 0x39 },      // 11111110
	{ 8, 255, 0, 0, 0x66 },      // 11111111
	{ 9, 380, 22, 1, 4511392 },  // 101111100
	{ 9, 381, 23, 1, 8705696 },  // 101111101
	{ 9, 382, 24, 1, 17094304 }, // 101111110
	{ 0 }
};

static INLINE BOOL zgfx_GetBits(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, UINT32 nbits)
{
	if (!zgfx)
		return FALSE;

	while (zgfx->cBitsCurrent < nbits)
	{
		zgfx->BitsCurrent <<= 8;

		if (zgfx->pbInputCurrent < zgfx->pbInputEnd)
			zgfx->BitsCurrent += *(zgfx->pbInputCurrent)++;

		zgfx->cBitsCurrent += 8;
	}

	zgfx->cBitsRemaining -= nbits;
	zgfx->cBitsCurrent -= nbits;
	zgfx->bits = zgfx->BitsCurrent >> zgfx->cBitsCurrent;
	zgfx->BitsCurrent &= ((1 << zgfx->cBitsCurrent) - 1);
	return TRUE;
}

static INLINE void zgfx_history_buffer_ring_write(ZGFX_CONTEXT* WINPR_RESTRICT zgfx,
                                                  const BYTE* WINPR_RESTRICT src, size_t count)
{
	UINT32 front = 0;

	if (count <= 0)
		return;

	if (count > zgfx->HistoryBufferSize)
	{
		const size_t residue = count - zgfx->HistoryBufferSize;
		count = zgfx->HistoryBufferSize;
		src += residue;
		zgfx->HistoryIndex = (zgfx->HistoryIndex + residue) % zgfx->HistoryBufferSize;
	}

	if (zgfx->HistoryIndex + count <= zgfx->HistoryBufferSize)
	{
		CopyMemory(&(zgfx->HistoryBuffer[zgfx->HistoryIndex]), src, count);

		if ((zgfx->HistoryIndex += count) == zgfx->HistoryBufferSize)
			zgfx->HistoryIndex = 0;
	}
	else
	{
		front = zgfx->HistoryBufferSize - zgfx->HistoryIndex;
		CopyMemory(&(zgfx->HistoryBuffer[zgfx->HistoryIndex]), src, front);
		CopyMemory(zgfx->HistoryBuffer, &src[front], count - front);
		zgfx->HistoryIndex = count - front;
	}
}

static INLINE void zgfx_history_buffer_ring_read(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, int offset,
                                                 BYTE* WINPR_RESTRICT dst, UINT32 count)
{
	UINT32 front = 0;
	UINT32 index = 0;
	INT32 bytes = 0;
	UINT32 valid = 0;
	INT32 bytesLeft = 0;
	BYTE* dptr = dst;
	BYTE* origDst = dst;

	if ((count <= 0) || (count > INT32_MAX))
		return;

	bytesLeft = (INT32)count;
	index = (zgfx->HistoryIndex + zgfx->HistoryBufferSize - offset) % zgfx->HistoryBufferSize;
	bytes = MIN(bytesLeft, offset);

	if ((index + bytes) <= zgfx->HistoryBufferSize)
	{
		CopyMemory(dptr, &(zgfx->HistoryBuffer[index]), bytes);
	}
	else
	{
		front = zgfx->HistoryBufferSize - index;
		CopyMemory(dptr, &(zgfx->HistoryBuffer[index]), front);
		CopyMemory(&dptr[front], zgfx->HistoryBuffer, bytes - front);
	}

	if ((bytesLeft -= bytes) == 0)
		return;

	dptr += bytes;
	valid = bytes;

	do
	{
		bytes = valid;

		if (bytes > bytesLeft)
			bytes = bytesLeft;

		CopyMemory(dptr, origDst, bytes);
		dptr += bytes;
		valid <<= 1;
	} while ((bytesLeft -= bytes) > 0);
}

static INLINE BOOL zgfx_decompress_segment(ZGFX_CONTEXT* WINPR_RESTRICT zgfx,
                                           wStream* WINPR_RESTRICT stream, size_t segmentSize)
{
	BYTE c = 0;
	BYTE flags = 0;
	UINT32 extra = 0;
	int opIndex = 0;
	UINT32 haveBits = 0;
	UINT32 inPrefix = 0;
	UINT32 count = 0;
	UINT32 distance = 0;
	BYTE* pbSegment = NULL;
	size_t cbSegment = 0;

	WINPR_ASSERT(zgfx);
	WINPR_ASSERT(stream);

	if (segmentSize < 2)
		return FALSE;

	cbSegment = segmentSize - 1;

	if (!Stream_CheckAndLogRequiredLength(TAG, stream, segmentSize) || (segmentSize > UINT32_MAX))
		return FALSE;

	Stream_Read_UINT8(stream, flags); /* header (1 byte) */
	zgfx->OutputCount = 0;
	pbSegment = Stream_Pointer(stream);
	if (!Stream_SafeSeek(stream, cbSegment))
		return FALSE;

	if (!(flags & PACKET_COMPRESSED))
	{
		zgfx_history_buffer_ring_write(zgfx, pbSegment, cbSegment);

		if (cbSegment > sizeof(zgfx->OutputBuffer))
			return FALSE;

		CopyMemory(zgfx->OutputBuffer, pbSegment, cbSegment);
		zgfx->OutputCount = cbSegment;
		return TRUE;
	}

	zgfx->pbInputCurrent = pbSegment;
	zgfx->pbInputEnd = &pbSegment[cbSegment - 1];
	/* NumberOfBitsToDecode = ((NumberOfBytesToDecode - 1) * 8) - ValueOfLastByte */
	const UINT32 bits = 8u * (cbSegment - 1u);
	if (bits < *zgfx->pbInputEnd)
		return FALSE;

	zgfx->cBitsRemaining = bits - *zgfx->pbInputEnd;
	zgfx->cBitsCurrent = 0;
	zgfx->BitsCurrent = 0;

	while (zgfx->cBitsRemaining)
	{
		haveBits = 0;
		inPrefix = 0;

		for (opIndex = 0; ZGFX_TOKEN_TABLE[opIndex].prefixLength != 0; opIndex++)
		{
			while (haveBits < ZGFX_TOKEN_TABLE[opIndex].prefixLength)
			{
				zgfx_GetBits(zgfx, 1);
				inPrefix = (inPrefix << 1) + zgfx->bits;
				haveBits++;
			}

			if (inPrefix == ZGFX_TOKEN_TABLE[opIndex].prefixCode)
			{
				if (ZGFX_TOKEN_TABLE[opIndex].tokenType == 0)
				{
					/* Literal */
					zgfx_GetBits(zgfx, ZGFX_TOKEN_TABLE[opIndex].valueBits);
					c = (BYTE)(ZGFX_TOKEN_TABLE[opIndex].valueBase + zgfx->bits);
					zgfx->HistoryBuffer[zgfx->HistoryIndex] = c;

					if (++zgfx->HistoryIndex == zgfx->HistoryBufferSize)
						zgfx->HistoryIndex = 0;

					if (zgfx->OutputCount >= sizeof(zgfx->OutputBuffer))
						return FALSE;

					zgfx->OutputBuffer[zgfx->OutputCount++] = c;
				}
				else
				{
					zgfx_GetBits(zgfx, ZGFX_TOKEN_TABLE[opIndex].valueBits);
					distance = ZGFX_TOKEN_TABLE[opIndex].valueBase + zgfx->bits;

					if (distance != 0)
					{
						/* Match */
						zgfx_GetBits(zgfx, 1);

						if (zgfx->bits == 0)
						{
							count = 3;
						}
						else
						{
							count = 4;
							extra = 2;
							zgfx_GetBits(zgfx, 1);

							while (zgfx->bits == 1)
							{
								count *= 2;
								extra++;
								zgfx_GetBits(zgfx, 1);
							}

							zgfx_GetBits(zgfx, extra);
							count += zgfx->bits;
						}

						if (count > sizeof(zgfx->OutputBuffer) - zgfx->OutputCount)
							return FALSE;

						zgfx_history_buffer_ring_read(
						    zgfx, distance, &(zgfx->OutputBuffer[zgfx->OutputCount]), count);
						zgfx_history_buffer_ring_write(
						    zgfx, &(zgfx->OutputBuffer[zgfx->OutputCount]), count);
						zgfx->OutputCount += count;
					}
					else
					{
						/* Unencoded */
						zgfx_GetBits(zgfx, 15);
						count = zgfx->bits;
						zgfx->cBitsRemaining -= zgfx->cBitsCurrent;
						zgfx->cBitsCurrent = 0;
						zgfx->BitsCurrent = 0;

						if (count > sizeof(zgfx->OutputBuffer) - zgfx->OutputCount)
							return FALSE;
						else if (count > zgfx->cBitsRemaining / 8)
							return FALSE;
						else if (zgfx->pbInputCurrent + count > zgfx->pbInputEnd)
							return FALSE;

						CopyMemory(&(zgfx->OutputBuffer[zgfx->OutputCount]), zgfx->pbInputCurrent,
						           count);
						zgfx_history_buffer_ring_write(zgfx, zgfx->pbInputCurrent, count);
						zgfx->pbInputCurrent += count;
						zgfx->cBitsRemaining -= (8 * count);
						zgfx->OutputCount += count;
					}
				}

				break;
			}
		}
	}

	return TRUE;
}

/* Allocate the buffers a bit larger.
 *
 * Due to optimizations some h264 decoders will read data beyond
 * the actual available data, so ensure that it will never be a
 * out of bounds read.
 */
static INLINE BYTE* aligned_zgfx_malloc(size_t size)
{
	return malloc(size + 64);
}

static INLINE BOOL zgfx_append(ZGFX_CONTEXT* WINPR_RESTRICT zgfx,
                               BYTE** WINPR_RESTRICT ppConcatenated, size_t uncompressedSize,
                               size_t* WINPR_RESTRICT pUsed)
{
	WINPR_ASSERT(zgfx);
	WINPR_ASSERT(ppConcatenated);
	WINPR_ASSERT(pUsed);

	const size_t used = *pUsed;
	if (zgfx->OutputCount > UINT32_MAX - used)
		return FALSE;

	if (used + zgfx->OutputCount > uncompressedSize)
		return FALSE;

	BYTE* tmp = realloc(*ppConcatenated, used + zgfx->OutputCount + 64ull);
	if (!tmp)
		return FALSE;
	*ppConcatenated = tmp;
	CopyMemory(&tmp[used], zgfx->OutputBuffer, zgfx->OutputCount);
	*pUsed = used + zgfx->OutputCount;
	return TRUE;
}

int zgfx_decompress(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, const BYTE* WINPR_RESTRICT pSrcData,
                    UINT32 SrcSize, BYTE** WINPR_RESTRICT ppDstData,
                    UINT32* WINPR_RESTRICT pDstSize, UINT32 flags)
{
	int status = -1;
	BYTE descriptor = 0;
	wStream sbuffer = { 0 };
	size_t used = 0;
	BYTE* pConcatenated = NULL;
	wStream* stream = Stream_StaticConstInit(&sbuffer, pSrcData, SrcSize);

	WINPR_ASSERT(zgfx);
	WINPR_ASSERT(stream);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	*ppDstData = NULL;
	*pDstSize = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, stream, 1))
		goto fail;

	Stream_Read_UINT8(stream, descriptor); /* descriptor (1 byte) */

	if (descriptor == ZGFX_SEGMENTED_SINGLE)
	{
		if (!zgfx_decompress_segment(zgfx, stream, Stream_GetRemainingLength(stream)))
			goto fail;

		if (zgfx->OutputCount > 0)
		{
			if (!zgfx_append(zgfx, &pConcatenated, zgfx->OutputCount, &used))
				goto fail;
			if (used != zgfx->OutputCount)
				goto fail;
			*ppDstData = pConcatenated;
			*pDstSize = zgfx->OutputCount;
		}
	}
	else if (descriptor == ZGFX_SEGMENTED_MULTIPART)
	{
		UINT32 segmentSize = 0;
		UINT16 segmentNumber = 0;
		UINT16 segmentCount = 0;
		UINT32 uncompressedSize = 0;

		if (!Stream_CheckAndLogRequiredLength(TAG, stream, 6))
			goto fail;

		Stream_Read_UINT16(stream, segmentCount);     /* segmentCount (2 bytes) */
		Stream_Read_UINT32(stream, uncompressedSize); /* uncompressedSize (4 bytes) */

		for (segmentNumber = 0; segmentNumber < segmentCount; segmentNumber++)
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, stream, sizeof(UINT32)))
				goto fail;

			Stream_Read_UINT32(stream, segmentSize); /* segmentSize (4 bytes) */

			if (!zgfx_decompress_segment(zgfx, stream, segmentSize))
				goto fail;

			if (!zgfx_append(zgfx, &pConcatenated, uncompressedSize, &used))
				goto fail;
		}

		if (used != uncompressedSize)
			goto fail;

		*ppDstData = pConcatenated;
		*pDstSize = uncompressedSize;
	}
	else
	{
		goto fail;
	}

	status = 1;
fail:
	if (status < 0)
		free(pConcatenated);
	return status;
}

static BOOL zgfx_compress_segment(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, wStream* WINPR_RESTRICT s,
                                  const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
                                  UINT32* WINPR_RESTRICT pFlags)
{
	/* FIXME: Currently compression not implemented. Just copy the raw source */
	if (!Stream_EnsureRemainingCapacity(s, SrcSize + 1))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return FALSE;
	}

	(*pFlags) |= ZGFX_PACKET_COMPR_TYPE_RDP8; /* RDP 8.0 compression format */
	Stream_Write_UINT8(s, (*pFlags));         /* header (1 byte) */
	Stream_Write(s, pSrcData, SrcSize);
	return TRUE;
}

int zgfx_compress_to_stream(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, wStream* WINPR_RESTRICT sDst,
                            const BYTE* WINPR_RESTRICT pUncompressed, UINT32 uncompressedSize,
                            UINT32* WINPR_RESTRICT pFlags)
{
	int fragment = 0;
	UINT16 maxLength = 0;
	UINT32 totalLength = 0;
	size_t posSegmentCount = 0;
	const BYTE* pSrcData = NULL;
	int status = 0;
	maxLength = ZGFX_SEGMENTED_MAXSIZE;
	totalLength = uncompressedSize;
	pSrcData = pUncompressed;

	for (; (totalLength > 0) || (fragment == 0); fragment++)
	{
		UINT32 SrcSize = 0;
		size_t posDstSize = 0;
		size_t posDataStart = 0;
		UINT32 DstSize = 0;
		SrcSize = (totalLength > maxLength) ? maxLength : totalLength;
		posDstSize = 0;
		totalLength -= SrcSize;

		/* Ensure we have enough space for headers */
		if (!Stream_EnsureRemainingCapacity(sDst, 12))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return -1;
		}

		if (fragment == 0)
		{
			/* First fragment */
			/* descriptor (1 byte) */
			Stream_Write_UINT8(sDst, (totalLength == 0) ? ZGFX_SEGMENTED_SINGLE
			                                            : ZGFX_SEGMENTED_MULTIPART);

			if (totalLength > 0)
			{
				posSegmentCount = Stream_GetPosition(sDst); /* segmentCount (2 bytes) */
				Stream_Seek(sDst, 2);
				Stream_Write_UINT32(sDst, uncompressedSize); /* uncompressedSize (4 bytes) */
			}
		}

		if (fragment > 0 || totalLength > 0)
		{
			/* Multipart */
			posDstSize = Stream_GetPosition(sDst); /* size (4 bytes) */
			Stream_Seek(sDst, 4);
		}

		posDataStart = Stream_GetPosition(sDst);

		if (!zgfx_compress_segment(zgfx, sDst, pSrcData, SrcSize, pFlags))
			return -1;

		if (posDstSize)
		{
			/* Fill segment data size */
			DstSize = Stream_GetPosition(sDst) - posDataStart;
			Stream_SetPosition(sDst, posDstSize);
			Stream_Write_UINT32(sDst, DstSize);
			Stream_SetPosition(sDst, posDataStart + DstSize);
		}

		pSrcData += SrcSize;
	}

	Stream_SealLength(sDst);

	/* fill back segmentCount */
	if (posSegmentCount)
	{
		Stream_SetPosition(sDst, posSegmentCount);
		Stream_Write_UINT16(sDst, fragment);
		Stream_SetPosition(sDst, Stream_Length(sDst));
	}

	return status;
}

int zgfx_compress(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, const BYTE* WINPR_RESTRICT pSrcData,
                  UINT32 SrcSize, BYTE** WINPR_RESTRICT ppDstData, UINT32* WINPR_RESTRICT pDstSize,
                  UINT32* WINPR_RESTRICT pFlags)
{
	int status = 0;
	wStream* s = Stream_New(NULL, SrcSize);
	status = zgfx_compress_to_stream(zgfx, s, pSrcData, SrcSize, pFlags);
	(*ppDstData) = Stream_Buffer(s);
	(*pDstSize) = Stream_GetPosition(s);
	Stream_Free(s, FALSE);
	return status;
}

void zgfx_context_reset(ZGFX_CONTEXT* WINPR_RESTRICT zgfx, BOOL flush)
{
	zgfx->HistoryIndex = 0;
}

ZGFX_CONTEXT* zgfx_context_new(BOOL Compressor)
{
	ZGFX_CONTEXT* zgfx = NULL;
	zgfx = (ZGFX_CONTEXT*)calloc(1, sizeof(ZGFX_CONTEXT));

	if (zgfx)
	{
		zgfx->Compressor = Compressor;
		zgfx->HistoryBufferSize = sizeof(zgfx->HistoryBuffer);
		zgfx_context_reset(zgfx, FALSE);
	}

	return zgfx;
}

void zgfx_context_free(ZGFX_CONTEXT* zgfx)
{
	free(zgfx);
}
