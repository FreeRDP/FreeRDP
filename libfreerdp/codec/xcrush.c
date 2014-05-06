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

int xcrush_copy_bytes(BYTE* dst, BYTE* src, int num)
{
	int index;

	for (index = 0; index < num; index++)
	{
		dst[index] = src[index];
	}

	return num;
}

int xcrush_decompress_l1(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	BYTE* pSrcEnd = NULL;
	BYTE* Literals = NULL;
	UINT16 MatchCount = 0;
	UINT16 MatchIndex = 0;
	BYTE* OutputPtr = NULL;
	int OutputLength = 0;
	UINT32 OutputOffset = 0;
	BYTE* HistoryPtr = NULL;
	BYTE* HistoryBuffer = NULL;
	BYTE* HistoryBufferEnd = NULL;
	UINT32 HistoryBufferSize = 0;
	UINT16 MatchLength = 0;
	UINT16 MatchOutputOffset = 0;
	UINT32 MatchHistoryOffset = 0;
	RDP61_MATCH_DETAILS* MatchDetails = NULL;

	if (SrcSize < 1)
		return -1001;

	if (flags & L1_PACKET_AT_FRONT)
		xcrush->HistoryOffset = 0;

	pSrcEnd = &pSrcData[SrcSize];
	HistoryBuffer = xcrush->HistoryBuffer;
	HistoryBufferSize = xcrush->HistoryBufferSize;
	HistoryBufferEnd = &(HistoryBuffer[HistoryBufferSize]);
	xcrush->HistoryPtr = HistoryPtr = &(HistoryBuffer[xcrush->HistoryOffset]);

	if (flags & L1_NO_COMPRESSION)
	{
		Literals = pSrcData;
	}
	else
	{
		if (!(flags & L1_COMPRESSED))
			return -1002;

		if ((pSrcData + 2) > pSrcEnd)
			return -1003;

		MatchCount = *((UINT16*) pSrcData);

		MatchDetails = (RDP61_MATCH_DETAILS*) &pSrcData[2];
		Literals = (BYTE*) &MatchDetails[MatchCount];
		OutputOffset = 0;

		if (Literals > pSrcEnd)
			return -1004;

		for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex++)
		{
			MatchLength = MatchDetails[MatchIndex].MatchLength;
			MatchOutputOffset = MatchDetails[MatchIndex].MatchOutputOffset;
			MatchHistoryOffset = MatchDetails[MatchIndex].MatchHistoryOffset;

			if (MatchOutputOffset < OutputOffset)
				return -1005;

			if (MatchLength > HistoryBufferSize)
				return -1006;

			if (MatchHistoryOffset > HistoryBufferSize)
				return -1007;

			OutputLength = MatchOutputOffset - OutputOffset;

			if ((MatchOutputOffset - OutputOffset) > HistoryBufferSize)
				return -1008;

			if (OutputLength > 0)
			{
				if ((&HistoryPtr[OutputLength] >= HistoryBufferEnd) || (Literals >= pSrcEnd) || (&Literals[OutputLength] > pSrcEnd))
					return -1009;

				xcrush_copy_bytes(HistoryPtr, Literals, OutputLength);

				HistoryPtr += OutputLength;
				Literals += OutputLength;
				OutputOffset += OutputLength;

				if (Literals > pSrcEnd)
					return -1010;
			}

			OutputPtr = &xcrush->HistoryBuffer[MatchHistoryOffset];

			if ((&HistoryPtr[MatchLength] >= HistoryBufferEnd) || (&OutputPtr[MatchLength] >= HistoryBufferEnd))
				return -1011;

			xcrush_copy_bytes(HistoryPtr, OutputPtr, MatchLength);

			OutputOffset += MatchLength;
			HistoryPtr += MatchLength;
		}
	}

	if (Literals < pSrcEnd)
	{
		OutputLength = pSrcEnd - Literals;

		if ((&HistoryPtr[OutputLength] >= HistoryBufferEnd) || (&Literals[OutputLength] > pSrcEnd))
			return -1012;

		xcrush_copy_bytes(HistoryPtr, Literals, OutputLength);
		HistoryPtr += OutputLength;
	}

	xcrush->HistoryOffset = HistoryPtr - HistoryBuffer;
	*pDstSize = HistoryPtr - xcrush->HistoryPtr;
	*ppDstData = xcrush->HistoryPtr;

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

	if (Level2ComprFlags & PACKET_COMPRESSED)
	{
		status = mppc_decompress(xcrush->mppc, pSrcData, SrcSize, &pDstData, &DstSize, Level2ComprFlags);

		if (status < 0)
			return status;
	}
	else
	{
		pDstData = pSrcData;
		DstSize = SrcSize;
	}

	status = xcrush_decompress_l1(xcrush, pDstData, DstSize, ppDstData, pDstSize, Level1ComprFlags);

	return status;
}

int xcrush_compress_l1(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	UINT32 Flags = 0;
	UINT32 HistoryOffset = 0;
	BYTE* HistoryPtr = NULL;
	BYTE* HistoryBuffer = NULL;

	HistoryOffset = xcrush->HistoryOffset;
	HistoryBuffer = xcrush->HistoryBuffer;
	HistoryPtr = &HistoryBuffer[HistoryOffset];

	CopyMemory(HistoryPtr, pSrcData, SrcSize);
	xcrush->HistoryOffset += SrcSize;

	if (SrcSize <= 50)
	{
		Flags |= L1_NO_COMPRESSION;
	}
	else
	{

	}

	*pFlags = Flags;

	return 1;
}

int xcrush_compress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	BYTE* CompressedData = NULL;
	UINT32 CompressedDataSize = 0;
	BYTE* OriginalData = NULL;
	UINT32 OriginalDataSize = 0;
	UINT32 Level1ComprFlags = 0;
	UINT32 Level2ComprFlags = 0;
	UINT32 CompressionLevel = 3;

	if (SrcSize > 16384)
		return -1;

	if ((SrcSize + 2) > *pDstSize)
		return -1;

	OriginalData = pDstData;
	OriginalDataSize = *pDstSize;

	pDstData = xcrush->BlockBuffer;
	DstSize = sizeof(xcrush->BlockBuffer);

	status = xcrush_compress_l1(xcrush, pSrcData, SrcSize, &pDstData, &DstSize, &Level1ComprFlags);

	if (status < 0)
		return status;

	if (Level1ComprFlags & L1_COMPRESSED)
	{
		CompressedData = pDstData;
		CompressedDataSize = DstSize;

		if (CompressedDataSize > SrcSize)
			return -1;
	}
	else
	{
		CompressedData = pSrcData;
		CompressedDataSize = DstSize;

		if (CompressedDataSize != SrcSize)
			return -1;
	}

	status = 1;

	pDstData = OriginalData + 2;
	DstSize = OriginalDataSize - 2;

	if (DstSize > 50)
	{
		status = mppc_compress(xcrush->mppc, CompressedData, CompressedDataSize, &pDstData, &DstSize, &Level2ComprFlags);
	}

	if (status < 0)
		return status;

	if (!(Level2ComprFlags & PACKET_COMPRESSED) || (Level2ComprFlags & PACKET_FLUSHED))
	{
		if (CompressedDataSize > DstSize)
			return -1;

		DstSize = CompressedDataSize;
		CopyMemory(pDstData, CompressedData, CompressedDataSize);
	}

	if (Level2ComprFlags & PACKET_COMPRESSED)
	{

	}
	else
	{
		if (Level2ComprFlags & PACKET_FLUSHED)
		{

		}
		else
		{

		}
	}

	Level1ComprFlags |= L1_INNER_COMPRESSION;

	OriginalData[0] = (BYTE) Level1ComprFlags;
	OriginalData[1] = (BYTE) Level2ComprFlags;

	if (*pDstSize < (DstSize + 2))
		return -1;

	*pDstSize = DstSize + 2;
	*pFlags = PACKET_COMPRESSED | CompressionLevel;

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

