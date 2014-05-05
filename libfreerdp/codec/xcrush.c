/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * XCrush (RDP6.1) Bulk Data Compression
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

#include <freerdp/codec/xcrush.h>

int xcrush_decompress_l1(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	BYTE* pSrcEnd = NULL;
	BYTE* Literals = NULL;
	UINT16 MatchCount = 0;
	UINT16 MatchIndex = 0;
	UINT16 MatchLength = 0;
	UINT32 OutputOffset = 0;
	BYTE* HistoryPtr = NULL;
	BYTE* HistoryBuffer = NULL;
	BYTE* HistoryBufferEnd = NULL;
	RDP61_MATCH_DETAILS* MatchDetails = NULL;

	if (SrcSize < 1)
		return -1;

	if (flags & L1_PACKET_AT_FRONT)
		xcrush->HistoryOffset = 0;

	pSrcEnd = &pSrcData[SrcSize];
	HistoryBuffer = xcrush->HistoryBuffer;
	HistoryBufferEnd = &(HistoryBuffer[xcrush->HistoryBufferSize]);
	xcrush->HistoryPtr = HistoryPtr = &(HistoryBuffer[xcrush->HistoryOffset]);

	if (flags & L1_NO_COMPRESSION)
	{
		Literals = pSrcData;
	}
	else
	{
		if (!(flags & L1_COMPRESSED))
		{
			return -1;
		}

		if ((pSrcData + 2) > pSrcEnd)
		{
			return -1;
		}

		MatchCount = *((UINT16*) pSrcData);
		MatchDetails = (RDP61_MATCH_DETAILS*) &pSrcData[2];
		Literals = (BYTE*) &MatchDetails[MatchCount];
		OutputOffset = 0;

		for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex++)
		{
			MatchLength = MatchDetails->MatchLength;

			printf("Match[%d]: Length: %d OutputOffset: %d HistoryOffset: %d\n",
					(int) MatchIndex, (int) MatchDetails->MatchLength,
					(int) MatchDetails->MatchOutputOffset, (int) MatchDetails->MatchHistoryOffset);
		}
	}

	if (Literals >= pSrcEnd)
	{
		xcrush->HistoryOffset = HistoryPtr - HistoryBuffer;
		*pDstSize = HistoryPtr - xcrush->HistoryPtr;
		*ppDstData = xcrush->HistoryPtr;
		return 1;
	}

	return 1;
}

int xcrush_decompress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	BYTE Level1ComprFlags;
	BYTE Level2ComprFlags;

	if (SrcSize < 2)
		return -1;

	Level1ComprFlags = pSrcData[0];
	Level2ComprFlags = pSrcData[1];

	pSrcData += 2;
	SrcSize -= 2;

	printf("Compression Flags: L1: 0x%04X L2: 0x%04X\n", Level1ComprFlags, Level2ComprFlags);

	if (Level2ComprFlags & PACKET_COMPRESSED)
	{
		printf("Level-2 PACKET_COMPRESSED\n");

		status = mppc_decompress(xcrush->mppc, pSrcData, SrcSize, &pDstData, &DstSize, Level2ComprFlags);

		if (status < 0)
			return status;
	}
	else
	{
		printf("Level-2 PACKET_UNCOMPRESSED\n");

		pDstData = pSrcData;
		DstSize = SrcSize;
	}

	/* Level-1 Decompression */

	status = xcrush_decompress_l1(xcrush, pDstData, DstSize, ppDstData, pDstSize, Level1ComprFlags);

	return status;
}

int xcrush_compress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	return 1;
}

void xcrush_context_reset(XCRUSH_CONTEXT* xcrush)
{

}

XCRUSH_CONTEXT* xcrush_context_new(BOOL Compressor)
{
	XCRUSH_CONTEXT* xcrush;

	xcrush = (XCRUSH_CONTEXT*) calloc(1, sizeof(XCRUSH_CONTEXT));

	if (xcrush)
	{
		xcrush->Compressor = Compressor;
		xcrush->mppc = mppc_context_new(1, Compressor);

		xcrush->HistoryOffset = 0;
		xcrush->HistoryBufferSize = 2000000;

		xcrush_context_reset(xcrush);
	}

	return xcrush;
}

void xcrush_context_free(XCRUSH_CONTEXT* xcrush)
{
	if (xcrush)
	{
		mppc_context_free(xcrush->mppc);
		free(xcrush);
	}
}

