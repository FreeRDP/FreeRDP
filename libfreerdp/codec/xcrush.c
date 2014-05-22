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

UINT32 xcrush_update_hash(BYTE* data, UINT32 size)
{
	BYTE* end;
	UINT32 seed = 5381; /* same value as in djb2 */

	if (size > 32)
	{
		size = 32;
		seed = 5413;
	}

	end = &data[size - 4];

	while (data < end)
	{
		seed += (data[3] ^ data[0]) + (data[1] << 8);
		data += 4;
	}

	return seed;
}

int xcrush_append_chunk(XCRUSH_CONTEXT* xcrush, BYTE* data, UINT32* beg, UINT32 end)
{
	UINT16 seed;
	UINT32 size;

	if (xcrush->SignatureIndex >= xcrush->SignatureCount)
		return 0;

	size = end - *beg;

	if (size > 0xFFFF)
		return 0;

	if (size >= 15)
	{
		seed = xcrush_update_hash(&data[*beg], size);
		xcrush->Signatures[xcrush->SignatureIndex].size = size;
		xcrush->Signatures[xcrush->SignatureIndex].seed = seed;
		xcrush->SignatureIndex++;
		*beg = end;
	}

	return 1;
}

int xcrush_compute_chunks(XCRUSH_CONTEXT* xcrush, BYTE* data, UINT32 size, UINT32* pIndex)
{
	UINT32 i = 0;
	UINT32 offset = 0;
	UINT32 accumulator = 0;

	*pIndex = 0;
	xcrush->SignatureIndex = 0;

	if (size < 128)
		return 0;

	for (i = 0; i < 32; i++)
	{
		accumulator = data[i] ^ _rotl(accumulator, 1);
	}

	for (i = 0; i < size - 64; i++)
	{
		accumulator = data[i + 32] ^ data[i] ^ _rotl(accumulator, 1);

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		accumulator = data[i + 32] ^ data[i] ^ _rotl(accumulator, 1);

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		accumulator = data[i + 32] ^ data[i] ^ _rotl(accumulator, 1);

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		accumulator = data[i + 32] ^ data[i] ^ _rotl(accumulator, 1);

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
	}

	if ((size == offset) || xcrush_append_chunk(xcrush, data, &offset, size))
	{
		*pIndex = xcrush->SignatureIndex;
		return 1;
	}

	return 0;
}

UINT32 xcrush_compute_signatures(XCRUSH_CONTEXT* xcrush, BYTE* data, UINT32 size)
{
	UINT32 index = 0;

	if (xcrush_compute_chunks(xcrush, data, size, &index))
		return index;

	return 0;
}

void xcrush_clear_hash_table_range(XCRUSH_CONTEXT* xcrush, UINT32 beg, UINT32 end)
{
	UINT32 index;

	for (index = 0; index < 65536; index++)
	{
		if (xcrush->NextChunks[index] >= beg)
		{
			if (xcrush->NextChunks[index] <= end)
				xcrush->NextChunks[index] = 0;
		}
	}

	for (index = 0; index < 65534; index++)
	{
		if (xcrush->Chunks[index].next >= beg )
		{
			if (xcrush->Chunks[index].next <= end)
			{
				xcrush->Chunks[index].next = 0;
			}
		}
	}
}

XCRUSH_CHUNK* xcrush_find_next_matching_chunk(XCRUSH_CONTEXT* xcrush, XCRUSH_CHUNK* chunk)
{
	UINT32 index;
	XCRUSH_CHUNK* next = NULL;

	if (chunk->next)
	{
		index = (chunk - xcrush->Chunks) / sizeof(XCRUSH_CHUNK);

		if ((index < xcrush->ChunkHead) || (chunk->next >= xcrush->ChunkHead))
			next = &xcrush->Chunks[chunk->next];
	}

	return next;
}

XCRUSH_CHUNK* xcrush_insert_chunk(XCRUSH_CONTEXT* xcrush, XCRUSH_SIGNATURE* signature, UINT32 offset)
{
	UINT32 seed;
	UINT32 index;
	XCRUSH_CHUNK* prev = NULL;

	if (xcrush->ChunkHead >= 65530)
	{
		xcrush->ChunkHead = xcrush->ChunkTail = 1;
	}

	if (xcrush->ChunkHead >= xcrush->ChunkTail)
	{
		xcrush_clear_hash_table_range(xcrush, xcrush->ChunkTail, xcrush->ChunkTail + 10000);
		xcrush->ChunkTail += 10000;
	}

	seed = signature->seed;
	index = xcrush->ChunkHead++;

	xcrush->Chunks[index].offset = offset;

	if (xcrush->NextChunks[seed])
		prev = &xcrush->Chunks[xcrush->NextChunks[seed]];

	xcrush->Chunks[index].next = xcrush->NextChunks[seed];
	xcrush->NextChunks[seed] = index;

	return prev;
}

UINT32 xcrush_find_match_length(UINT32 MatchOffset, UINT32 ChunkOffset, BYTE* HistoryBuffer, UINT32 HistoryOffset, UINT32 SrcSize, UINT32 MaxMatchLength, XCRUSH_MATCH_INFO* MatchInfo)
{
	UINT32 MatchSymbol;
	UINT32 ChunkSymbol;
	BYTE* ChunkBuffer;
	BYTE* MatchBuffer;
	BYTE* MatchStartPtr;
	BYTE* ForwardChunkPtr;
	BYTE* ReverseChunkPtr;
	BYTE* ForwardMatchPtr;
	BYTE* ReverseMatchPtr;
	BYTE* HistoryBufferEnd;
	UINT32 ReverseMatchLength;
	UINT32 ForwardMatchLength;
	UINT32 TotalMatchLength;

	ForwardMatchLength = 0;
	ReverseMatchLength = 0;
	MatchBuffer = &HistoryBuffer[MatchOffset];
	ChunkBuffer = &HistoryBuffer[ChunkOffset];
	HistoryBufferEnd = &HistoryBuffer[HistoryOffset] + SrcSize;

	if (MatchOffset == ChunkOffset)
		return 0; /* error */
	
	if (MatchBuffer < HistoryBuffer)
		return 0; /* error */

	if (ChunkBuffer < HistoryBuffer)
		return 0; /* error */

	ForwardMatchPtr = &HistoryBuffer[MatchOffset];
	ForwardChunkPtr = &HistoryBuffer[ChunkOffset];

	if ((&MatchBuffer[MaxMatchLength + 1] < HistoryBufferEnd)
		&& (MatchBuffer[MaxMatchLength + 1] != ChunkBuffer[MaxMatchLength + 1]))
	{
		return 0;
	}

	while (1)
	{
		MatchSymbol = *ForwardMatchPtr++;
		ChunkSymbol = *ForwardChunkPtr++;

		if (MatchSymbol != ChunkSymbol)
			break;

		if (ForwardMatchPtr > HistoryBufferEnd)
			break;
			
		ForwardMatchLength++;
	}

	ReverseMatchPtr = MatchBuffer - 1;
	ReverseChunkPtr = ChunkBuffer - 1;

	while((ReverseMatchPtr > &HistoryBuffer[HistoryOffset])
		&& (ReverseChunkPtr > HistoryBuffer)
		&& (*ReverseMatchPtr == *ReverseChunkPtr))
	{
		ReverseMatchLength++;
		ReverseMatchPtr--;
		ReverseChunkPtr--;
	}

	MatchStartPtr = MatchBuffer - ReverseMatchLength;
	TotalMatchLength = ReverseMatchLength + ForwardMatchLength;

	if (TotalMatchLength < 11)
		return 0;

	if (MatchStartPtr < HistoryBuffer)
		return 0; /* error */

	MatchInfo->MatchOffset = MatchStartPtr - HistoryBuffer;
	MatchInfo->ChunkOffset = ChunkBuffer - ReverseMatchLength - HistoryBuffer;
	MatchInfo->MatchLength = TotalMatchLength;

	return TotalMatchLength;
}

UINT32 xcrush_find_all_matches(XCRUSH_CONTEXT* xcrush, UINT32 SignatureIndex, XCRUSH_SIGNATURE* Signatures, UINT32 HistoryOffset, UINT32 SrcOffset, UINT32 SrcSize)
{
	UINT32 i = 0;
	UINT32 j = 0;
	UINT32 offset = 0;
	UINT32 ChunkIndex = 0;
	UINT32 ChunkCount = 0;
	XCRUSH_CHUNK* chunk = NULL;
	UINT32 MatchLength = 0;
	UINT32 MaxMatchLength = 0;
	UINT32 PrevMatchEnd = 0;
	XCRUSH_MATCH_INFO MatchInfo = { 0 };
	XCRUSH_MATCH_INFO MaxMatchInfo = { 0 };

	for (i = 0; i < SignatureIndex; i++)
	{
		offset = SrcOffset + HistoryOffset;
		
		if (!Signatures[i].size)
			return 0; /* error */

		chunk = xcrush_insert_chunk(xcrush, &Signatures[i], offset);

		if (chunk && (SrcOffset + HistoryOffset + Signatures[i].size >= PrevMatchEnd))
		{
			ChunkCount = 0;
			MaxMatchLength = 0;
			ZeroMemory(&MaxMatchInfo, sizeof(XCRUSH_MATCH_INFO));

			while (chunk)
			{				
				if ((chunk->offset < HistoryOffset) || (chunk->offset < offset)
					|| (chunk->offset > SrcSize + HistoryOffset))
				{
					MatchLength = xcrush_find_match_length(
						offset, chunk->offset, xcrush->HistoryBuffer,
						HistoryOffset, SrcSize, MaxMatchLength, &MatchInfo);
					
					if (MatchLength > MaxMatchLength)
					{
						MaxMatchLength = MatchLength;
						MaxMatchInfo.MatchOffset = MatchInfo.MatchOffset;
						MaxMatchInfo.ChunkOffset = MatchInfo.ChunkOffset;
						MaxMatchInfo.MatchLength = MatchInfo.MatchLength;
						
						if (MatchLength > 256)
							break;
					}
				}
				
				ChunkIndex = ChunkCount++;

				if (ChunkIndex > 4)
					break;
				
				chunk = xcrush_find_next_matching_chunk(xcrush, chunk);
			}

			if (MaxMatchLength)
			{
				xcrush->OriginalMatches[j].MatchOffset = MaxMatchInfo.MatchOffset;
				xcrush->OriginalMatches[j].ChunkOffset = MaxMatchInfo.ChunkOffset;
				xcrush->OriginalMatches[j].MatchLength = MaxMatchInfo.MatchLength;
				
				if (xcrush->OriginalMatches[j].MatchOffset < HistoryOffset)
					return 0; /* error */

				PrevMatchEnd = xcrush->OriginalMatches[j].MatchLength + xcrush->OriginalMatches[j].MatchOffset;
				
				j++;
				
				if (j >= 1000)
					return 0; /* error */
			}
		}
		
		SrcOffset += Signatures[i].size;

		if (SrcOffset > SrcSize)
			return 0; /* error */
	}

	if (SrcOffset > SrcSize)
		return 0; /* error */

	return j;
}

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
	UINT32 SignatureIndex = 0;

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
		SignatureIndex = xcrush_compute_signatures(xcrush, pSrcData, *pDstSize);

		if (SignatureIndex)
		{
			xcrush_find_all_matches(xcrush, SignatureIndex, xcrush->Signatures, HistoryOffset, 0, SrcSize);
		}
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
	xcrush->SignatureIndex = 0;
	xcrush->SignatureCount = 1000;
	ZeroMemory(&(xcrush->Signatures), sizeof(XCRUSH_SIGNATURE) * xcrush->SignatureCount);

	xcrush->ChunkHead = xcrush->ChunkTail = 1;
	ZeroMemory(&(xcrush->Chunks), sizeof(xcrush->Chunks));
	ZeroMemory(&(xcrush->NextChunks), sizeof(xcrush->NextChunks));

	ZeroMemory(&(xcrush->OriginalMatches), sizeof(xcrush->OriginalMatches));
	ZeroMemory(&(xcrush->OptimizedMatches), sizeof(xcrush->OptimizedMatches));
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

