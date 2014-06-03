/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ZGFX (RDP8) Bulk Data Compression
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/codec/zgfx.h>

struct _ZGFX_TOKEN
{
	int prefixLength;
	int prefixCode;
	int valueBits;
	int tokenType;
	UINT32 valueBase;
};
typedef struct _ZGFX_TOKEN ZGFX_TOKEN;

static const ZGFX_TOKEN ZGFX_TOKEN_TABLE[] =
{
	// len code vbits type  vbase
	{  1,    0,   8,   0,           0 },    // 0
	{  5,   17,   5,   1,           0 },    // 10001
	{  5,   18,   7,   1,          32 },    // 10010
	{  5,   19,   9,   1,         160 },    // 10011
	{  5,   20,  10,   1,         672 },    // 10100
	{  5,   21,  12,   1,        1696 },    // 10101
	{  5,   24,   0,   0,  0x00       },    // 11000
	{  5,   25,   0,   0,  0x01       },    // 11001
	{  6,   44,  14,   1,        5792 },    // 101100
	{  6,   45,  15,   1,       22176 },    // 101101
	{  6,   52,   0,   0,  0x02       },    // 110100
	{  6,   53,   0,   0,  0x03       },    // 110101
	{  6,   54,   0,   0,  0xFF       },    // 110110
	{  7,   92,  18,   1,       54944 },    // 1011100
	{  7,   93,  20,   1,      317088 },    // 1011101
	{  7,  110,   0,   0,  0x04       },    // 1101110
	{  7,  111,   0,   0,  0x05       },    // 1101111
	{  7,  112,   0,   0,  0x06       },    // 1110000
	{  7,  113,   0,   0,  0x07       },    // 1110001
	{  7,  114,   0,   0,  0x08       },    // 1110010
	{  7,  115,   0,   0,  0x09       },    // 1110011
	{  7,  116,   0,   0,  0x0A       },    // 1110100
	{  7,  117,   0,   0,  0x0B       },    // 1110101
	{  7,  118,   0,   0,  0x3A       },    // 1110110
	{  7,  119,   0,   0,  0x3B       },    // 1110111
	{  7,  120,   0,   0,  0x3C       },    // 1111000
	{  7,  121,   0,   0,  0x3D       },    // 1111001
	{  7,  122,   0,   0,  0x3E       },    // 1111010
	{  7,  123,   0,   0,  0x3F       },    // 1111011
	{  7,  124,   0,   0,  0x40       },    // 1111100
	{  7,  125,   0,   0,  0x80       },    // 1111101
	{  8,  188,  20,   1,     1365664 },    // 10111100
	{  8,  189,  21,   1,     2414240 },    // 10111101
	{  8,  252,   0,   0,  0x0C       },    // 11111100
	{  8,  253,   0,   0,  0x38       },    // 11111101
	{  8,  254,   0,   0,  0x39       },    // 11111110
	{  8,  255,   0,   0,  0x66       },    // 11111111
	{  9,  380,  22,   1,     4511392 },    // 101111100
	{  9,  381,  23,   1,     8705696 },    // 101111101
	{  9,  382,  24,   1,    17094304 },    // 101111110
	{ 0 }
};

UINT32 zgfx_GetBits(ZGFX_CONTEXT* zgfx, UINT32 bitCount)
{
	UINT32 result;

	while (zgfx->cBitsCurrent < bitCount)
	{
		zgfx->BitsCurrent <<= 8;

		if (zgfx->pbInputCurrent < zgfx->pbInputEnd)
		{
			zgfx->BitsCurrent += *(zgfx->pbInputCurrent)++;
		}

		zgfx->cBitsCurrent += 8;
	}

	zgfx->cBitsRemaining -= bitCount;
	zgfx->cBitsCurrent -= bitCount;

	result = zgfx->BitsCurrent >> zgfx->cBitsCurrent;

	zgfx->BitsCurrent &= ((1 << zgfx->cBitsCurrent) - 1);

	return result;
}

int zgfx_OutputFromNotCompressed(ZGFX_CONTEXT* zgfx, BYTE* pbRaw, int cbRaw)
{
	BYTE c;
	int iRaw;

	zgfx->OutputCount = 0;

	for (iRaw = 0; iRaw < cbRaw; iRaw++)
	{
		c = pbRaw[iRaw];

		zgfx->HistoryBuffer[zgfx->HistoryIndex++] = c;

		if (zgfx->HistoryIndex == sizeof(zgfx->HistoryBuffer))
			zgfx->HistoryIndex = 0;

		zgfx->OutputBuffer[zgfx->OutputCount++] = c;
	}

	return 1;
}

int zgfx_OutputFromCompressed(ZGFX_CONTEXT* zgfx, BYTE* pbEncoded, int cbEncoded)
{
	BYTE c;
	int extra;
	int opIndex;
	int haveBits;
	int inPrefix;
	UINT32 count;
	UINT32 distance;
	UINT32 prevIndex;

	zgfx->OutputCount = 0;

	zgfx->pbInputCurrent = pbEncoded;
	zgfx->pbInputEnd = pbEncoded + cbEncoded - 1;

	zgfx->cBitsRemaining = 8 * (cbEncoded - 1) - *zgfx->pbInputEnd;
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
				inPrefix = (inPrefix << 1) + zgfx_GetBits(zgfx, 1);
				haveBits++;
			}

			if (inPrefix == ZGFX_TOKEN_TABLE[opIndex].prefixCode)
			{
				if (ZGFX_TOKEN_TABLE[opIndex].tokenType == 0)
				{
					c = (BYTE)(ZGFX_TOKEN_TABLE[opIndex].valueBase +
							zgfx_GetBits(zgfx, ZGFX_TOKEN_TABLE[opIndex].valueBits));

					goto output_literal;
				}
				else
				{
					distance = ZGFX_TOKEN_TABLE[opIndex].valueBase +
							zgfx_GetBits(zgfx, ZGFX_TOKEN_TABLE[opIndex].valueBits);

					if (distance != 0)
					{
						if (zgfx_GetBits(zgfx, 1) == 0)
						{
							count = 3;
						}
						else
						{
							count = 4;
							extra = 2;

							while (zgfx_GetBits(zgfx, 1) == 1)
							{
								count *= 2;
								extra++;
							}

							count += zgfx_GetBits(zgfx, extra);
						}

						goto output_match;
					}
					else
					{
						count = zgfx_GetBits(zgfx, 15);

						zgfx->cBitsRemaining -= zgfx->cBitsCurrent;
						zgfx->cBitsCurrent = 0;
						zgfx->BitsCurrent = 0;

						goto output_unencoded;
					}
				}
			}
		}
		break;

output_literal:

		zgfx->HistoryBuffer[zgfx->HistoryIndex] = c;

		if (++zgfx->HistoryIndex == sizeof(zgfx->HistoryBuffer))
			zgfx->HistoryIndex = 0;

		zgfx->OutputBuffer[zgfx->OutputCount++] = c;
		continue;

output_match:

		prevIndex = zgfx->HistoryIndex + sizeof(zgfx->HistoryBuffer) - distance;
		prevIndex = prevIndex % sizeof(zgfx->HistoryBuffer);

		while (count--)
		{
			c = zgfx->HistoryBuffer[prevIndex];

			if (++prevIndex == sizeof(zgfx->HistoryBuffer))
				prevIndex = 0;

			zgfx->HistoryBuffer[zgfx->HistoryIndex] = c;

			if (++zgfx->HistoryIndex == sizeof(zgfx->HistoryBuffer))
				zgfx->HistoryIndex = 0;

			zgfx->OutputBuffer[zgfx->OutputCount] = c;
			++zgfx->OutputCount;
		}
		continue;

output_unencoded:
		while (count--)
		{
			c = *zgfx->pbInputCurrent++;
			zgfx->cBitsRemaining -= 8;

			zgfx->HistoryBuffer[zgfx->HistoryIndex] = c;

			if (++zgfx->HistoryIndex == sizeof(zgfx->HistoryBuffer))
				zgfx->HistoryIndex = 0;

			zgfx->OutputBuffer[zgfx->OutputCount] = c;
			++zgfx->OutputCount;
		}
		continue;
	}

	return 1;
}

int zgfx_OutputFromSegment(ZGFX_CONTEXT* zgfx, BYTE* pbSegment, int cbSegment)
{
	int status;

	if (pbSegment[0] & PACKET_COMPRESSED)
		status = zgfx_OutputFromCompressed(zgfx, pbSegment + 1, cbSegment - 1);
	else
		status = zgfx_OutputFromNotCompressed(zgfx, pbSegment + 1, cbSegment - 1);

	return status;
}

int zgfx_Decompress_Internal(ZGFX_CONTEXT* zgfx, BYTE* pbInput, int cbInput, BYTE** ppbOutput, int* pcbOutput)
{
	int status;

	RDP_SEGMENTED_DATA* pSegmentedData = (RDP_SEGMENTED_DATA*) pbInput;

	if (pSegmentedData->descriptor == ZGFX_SEGMENTED_SINGLE)
	{
		status = zgfx_OutputFromSegment(zgfx, pbInput + 1, cbInput - 1);

		*ppbOutput = (BYTE*) malloc(zgfx->OutputCount);
		*pcbOutput = zgfx->OutputCount;
		CopyMemory(*ppbOutput, zgfx->OutputBuffer, zgfx->OutputCount);
	}
	else if (pSegmentedData->descriptor == ZGFX_SEGMENTED_MULTIPART)
	{
		UINT16 segmentNumber;
		UINT32 segmentOffset = sizeof(RDP_SEGMENTED_DATA);
		BYTE* pConcatenated = (BYTE*) malloc(pSegmentedData->uncompressedSize);

		*ppbOutput = pConcatenated;
		*pcbOutput = pSegmentedData->uncompressedSize;

		for (segmentNumber = 0; segmentNumber < pSegmentedData->segmentCount; segmentNumber++)
		{
			RDP_DATA_SEGMENT* pSegment = (RDP_DATA_SEGMENT*) (pbInput + segmentOffset);

			status = zgfx_OutputFromSegment(zgfx, pbInput + segmentOffset + sizeof(RDP_DATA_SEGMENT), pSegment->size);

			segmentOffset += sizeof(RDP_DATA_SEGMENT) + pSegment->size;
			CopyMemory(pConcatenated, zgfx->OutputBuffer, zgfx->OutputCount);
			pConcatenated += zgfx->OutputCount;
		}
	}

	return 1;
}

int zgfx_decompress(ZGFX_CONTEXT* zgfx, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	int status = 0;
	int cbOutput = 0;
	BYTE* pbOutput = NULL;

	status = zgfx_Decompress_Internal(zgfx, pSrcData, SrcSize, &pbOutput, &cbOutput);

	if (status >= 0)
	{
		*ppDstData = pbOutput;
		*pDstSize = (UINT32) cbOutput;
	}

	return status;
}

int zgfx_compress(ZGFX_CONTEXT* zgfx, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	return 1;
}

void zgfx_context_reset(ZGFX_CONTEXT* zgfx, BOOL flush)
{
	zgfx->HistoryIndex = 0;
}

ZGFX_CONTEXT* zgfx_context_new(BOOL Compressor)
{
	ZGFX_CONTEXT* zgfx;

	zgfx = (ZGFX_CONTEXT*) calloc(1, sizeof(ZGFX_CONTEXT));

	if (zgfx)
	{
		zgfx->Compressor = Compressor;

		zgfx_context_reset(zgfx, FALSE);
	}

	return zgfx;
}

void zgfx_context_free(ZGFX_CONTEXT* zgfx)
{
	if (zgfx)
	{
		free(zgfx);
	}
}

