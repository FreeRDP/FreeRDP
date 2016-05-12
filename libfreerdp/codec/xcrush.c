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

#include <freerdp/log.h>
#include <freerdp/codec/xcrush.h>

#define TAG FREERDP_TAG("codec")

const char* xcrush_get_level_2_compression_flags_string(UINT32 flags)
{
	flags &= 0xE0;

	if (flags == 0)
		return "PACKET_UNCOMPRESSED";
	else if (flags == PACKET_COMPRESSED)
		return "PACKET_COMPRESSED";
	else if (flags == PACKET_AT_FRONT)
		return "PACKET_AT_FRONT";
	else if (flags == PACKET_FLUSHED)
		return "PACKET_FLUSHED";
	else if (flags == (PACKET_COMPRESSED | PACKET_AT_FRONT))
		return "PACKET_COMPRESSED | PACKET_AT_FRONT";
	else if (flags == (PACKET_COMPRESSED | PACKET_FLUSHED))
		return "PACKET_COMPRESSED | PACKET_FLUSHED";
	else if (flags == (PACKET_AT_FRONT | PACKET_FLUSHED))
		return "PACKET_AT_FRONT | PACKET_FLUSHED";
	else if (flags == (PACKET_COMPRESSED | PACKET_AT_FRONT | PACKET_FLUSHED))
		return "PACKET_COMPRESSED | PACKET_AT_FRONT | PACKET_FLUSHED";

	return "PACKET_UNKNOWN";
}

const char* xcrush_get_level_1_compression_flags_string(UINT32 flags)
{
	flags &= 0x17;

	if (flags == 0)
		return "L1_UNKNOWN";
	else if (flags == L1_PACKET_AT_FRONT)
		return "L1_PACKET_AT_FRONT";
	else if (flags == L1_NO_COMPRESSION)
		return "L1_NO_COMPRESSION";
	else if (flags == L1_COMPRESSED)
		return "L1_COMPRESSED";
	else if (flags == L1_INNER_COMPRESSION)
		return "L1_INNER_COMPRESSION";
	else if (flags == (L1_PACKET_AT_FRONT | L1_NO_COMPRESSION))
		return "L1_PACKET_AT_FRONT | L1_NO_COMPRESSION";
	else if (flags == (L1_PACKET_AT_FRONT | L1_COMPRESSED))
		return "L1_PACKET_AT_FRONT | L1_COMPRESSED";
	else if (flags == (L1_PACKET_AT_FRONT | L1_INNER_COMPRESSION))
		return "L1_PACKET_AT_FRONT | L1_INNER_COMPRESSION";
	else if (flags == (L1_NO_COMPRESSION | L1_COMPRESSED))
		return "L1_NO_COMPRESSION | L1_COMPRESSED";
	else if (flags == (L1_NO_COMPRESSION | L1_INNER_COMPRESSION))
		return "L1_NO_COMPRESSION | L1_INNER_COMPRESSION";
	else if (flags == (L1_COMPRESSED | L1_INNER_COMPRESSION))
		return "L1_COMPRESSED | L1_INNER_COMPRESSION";
	else if (flags == (L1_NO_COMPRESSION | L1_COMPRESSED | L1_INNER_COMPRESSION))
		return "L1_NO_COMPRESSION | L1_COMPRESSED | L1_INNER_COMPRESSION";
	else if (flags == (L1_PACKET_AT_FRONT | L1_COMPRESSED | L1_INNER_COMPRESSION))
		return "L1_PACKET_AT_FRONT | L1_COMPRESSED | L1_INNER_COMPRESSION";
	else if (flags == (L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_INNER_COMPRESSION))
		return "L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_INNER_COMPRESSION";
	else if (flags == (L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_COMPRESSED))
		return "L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_COMPRESSED";
	else if (flags == (L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_COMPRESSED | L1_INNER_COMPRESSION))
		return "L1_PACKET_AT_FRONT | L1_NO_COMPRESSION | L1_COMPRESSED | L1_INNER_COMPRESSION";

	return "L1_UNKNOWN";
}

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

	return (UINT16) seed;
}

int xcrush_append_chunk(XCRUSH_CONTEXT* xcrush, BYTE* data, UINT32* beg, UINT32 end)
{
	UINT16 seed;
	UINT32 size;

	if (xcrush->SignatureIndex >= xcrush->SignatureCount)
		return 0;

	size = end - *beg;

	if (size > 65535)
		return 0;

	if (size >= 15)
	{
		seed = xcrush_update_hash(&data[*beg], (UINT16) size);
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
	UINT32 rotation = 0;
	UINT32 accumulator = 0;

	*pIndex = 0;
	xcrush->SignatureIndex = 0;

	if (size < 128)
		return 0;

	for (i = 0; i < 32; i++)
	{
		rotation = _rotl(accumulator, 1);
		accumulator = data[i] ^ rotation;
	}

	for (i = 0; i < size - 64; i++)
	{
		rotation = _rotl(accumulator, 1);
		accumulator = data[i + 32] ^ data[i] ^ rotation;

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		rotation = _rotl(accumulator, 1);
		accumulator = data[i + 32] ^ data[i] ^ rotation;

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		rotation = _rotl(accumulator, 1);
		accumulator = data[i + 32] ^ data[i] ^ rotation;

		if (!(accumulator & 0x7F))
		{
			if (!xcrush_append_chunk(xcrush, data, &offset, i + 32))
				return 0;
		}
		i++;

		rotation = _rotl(accumulator, 1);
		accumulator = data[i + 32] ^ data[i] ^ rotation;

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
			{
				xcrush->NextChunks[index] = 0;
			}
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

int xcrush_find_next_matching_chunk(XCRUSH_CONTEXT* xcrush, XCRUSH_CHUNK* chunk, XCRUSH_CHUNK** pNextChunk)
{
	UINT32 index;
	XCRUSH_CHUNK* next = NULL;

	if (!chunk)
		return -4001; /* error */

	if (chunk->next)
	{
		index = (chunk - xcrush->Chunks) / sizeof(XCRUSH_CHUNK);

		if (index >= 65534)
			return -4002; /* error */

		if ((index < xcrush->ChunkHead) || (chunk->next >= xcrush->ChunkHead))
		{
			if (chunk->next >= 65534)
				return -4003; /* error */

			next = &xcrush->Chunks[chunk->next];
		}
	}

	*pNextChunk = next;

	return 1;
}

int xcrush_insert_chunk(XCRUSH_CONTEXT* xcrush, XCRUSH_SIGNATURE* signature, UINT32 offset, XCRUSH_CHUNK** pPrevChunk)
{
	UINT32 seed;
	UINT32 index;

	if (xcrush->ChunkHead >= 65530)
	{
		xcrush->ChunkHead = 1;
		xcrush->ChunkTail = 1;
	}

	if (xcrush->ChunkHead >= xcrush->ChunkTail)
	{
		xcrush_clear_hash_table_range(xcrush, xcrush->ChunkTail, xcrush->ChunkTail + 10000);
		xcrush->ChunkTail += 10000;
	}

	index = xcrush->ChunkHead++;

	if (xcrush->ChunkHead >= 65534)
		return -3001; /* error */

	xcrush->Chunks[index].offset = offset;

	seed = signature->seed;

	if (seed >= 65536)
		return -3002; /* error */

	if (xcrush->NextChunks[seed])
	{
		if (xcrush->NextChunks[seed] >= 65534)
			return -3003; /* error */

		*pPrevChunk = &xcrush->Chunks[xcrush->NextChunks[seed]];
	}

	xcrush->Chunks[index].next = xcrush->NextChunks[seed] & 0xFFFF;
	xcrush->NextChunks[seed] = index;

	return 1;
}

int xcrush_find_match_length(XCRUSH_CONTEXT* xcrush, UINT32 MatchOffset, UINT32 ChunkOffset, UINT32 HistoryOffset, UINT32 SrcSize, UINT32 MaxMatchLength, XCRUSH_MATCH_INFO* MatchInfo)
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
	BYTE* HistoryBuffer;
	UINT32 HistoryBufferSize;

	ForwardMatchLength = 0;
	ReverseMatchLength = 0;

	HistoryBuffer = xcrush->HistoryBuffer;
	HistoryBufferSize = xcrush->HistoryBufferSize;
	HistoryBufferEnd = &HistoryBuffer[HistoryOffset + SrcSize];

	if (MatchOffset > HistoryBufferSize)
		return -2001; /* error */

	MatchBuffer = &HistoryBuffer[MatchOffset];

	if (ChunkOffset > HistoryBufferSize)
		return -2002; /* error */

	ChunkBuffer = &HistoryBuffer[ChunkOffset];

	if (MatchOffset == ChunkOffset)
		return -2003; /* error */
	
	if (MatchBuffer < HistoryBuffer)
		return -2004; /* error */

	if (ChunkBuffer < HistoryBuffer)
		return -2005; /* error */

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
		return -2006; /* error */

	MatchInfo->MatchOffset = MatchStartPtr - HistoryBuffer;
	MatchInfo->ChunkOffset = ChunkBuffer - ReverseMatchLength - HistoryBuffer;
	MatchInfo->MatchLength = TotalMatchLength;

	return (int) TotalMatchLength;
}

int xcrush_find_all_matches(XCRUSH_CONTEXT* xcrush, UINT32 SignatureIndex, UINT32 HistoryOffset, UINT32 SrcOffset, UINT32 SrcSize)
{
	UINT32 i = 0;
	UINT32 j = 0;
	int status = 0;
	UINT32 offset = 0;
	UINT32 ChunkIndex = 0;
	UINT32 ChunkCount = 0;
	XCRUSH_CHUNK* chunk = NULL;
	UINT32 MatchLength = 0;
	UINT32 MaxMatchLength = 0;
	UINT32 PrevMatchEnd = 0;
	XCRUSH_MATCH_INFO MatchInfo = { 0 };
	XCRUSH_MATCH_INFO MaxMatchInfo = { 0 };
	XCRUSH_SIGNATURE* Signatures = NULL;

	Signatures = xcrush->Signatures;

	for (i = 0; i < SignatureIndex; i++)
	{
		offset = SrcOffset + HistoryOffset;
		
		if (!Signatures[i].size)
			return -1001; /* error */

		status = xcrush_insert_chunk(xcrush, &Signatures[i], offset, &chunk);

		if (status < 0)
			return status;

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
					status = xcrush_find_match_length(xcrush, offset, chunk->offset,
							HistoryOffset, SrcSize, MaxMatchLength, &MatchInfo);
					
					if (status < 0)
						return status; /* error */

					MatchLength = (UINT32) status;

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
				
				status = xcrush_find_next_matching_chunk(xcrush, chunk, &chunk);

				if (status < 0)
					return status; /* error */
			}

			if (MaxMatchLength)
			{
				xcrush->OriginalMatches[j].MatchOffset = MaxMatchInfo.MatchOffset;
				xcrush->OriginalMatches[j].ChunkOffset = MaxMatchInfo.ChunkOffset;
				xcrush->OriginalMatches[j].MatchLength = MaxMatchInfo.MatchLength;
				
				if (xcrush->OriginalMatches[j].MatchOffset < HistoryOffset)
					return -1002; /* error */

				PrevMatchEnd = xcrush->OriginalMatches[j].MatchLength + xcrush->OriginalMatches[j].MatchOffset;
				
				j++;
				
				if (j >= 1000)
					return -1003; /* error */
			}
		}
		
		SrcOffset += Signatures[i].size;

		if (SrcOffset > SrcSize)
			return -1004; /* error */
	}

	if (SrcOffset > SrcSize)
		return -1005; /* error */

	return (int) j;
}

int xcrush_optimize_matches(XCRUSH_CONTEXT* xcrush)
{
	UINT32 i, j;
	UINT32 MatchDiff;
	UINT32 PrevMatchEnd;
	UINT32 TotalMatchLength;
	UINT32 OriginalMatchCount;
	UINT32 OptimizedMatchCount;
	XCRUSH_MATCH_INFO* OriginalMatch;
	XCRUSH_MATCH_INFO* OptimizedMatch;
	XCRUSH_MATCH_INFO* OriginalMatches;
	XCRUSH_MATCH_INFO* OptimizedMatches;

	i = j = 0;
	PrevMatchEnd = 0;
	TotalMatchLength = 0;

	OriginalMatches = xcrush->OriginalMatches;
	OriginalMatchCount = xcrush->OriginalMatchCount;

	OptimizedMatches = xcrush->OptimizedMatches;
	OptimizedMatchCount = xcrush->OptimizedMatchCount;

	for (i = 0; i < OriginalMatchCount; i++)
	{
		if (OriginalMatches[i].MatchOffset <= PrevMatchEnd)
		{
			if ((OriginalMatches[i].MatchOffset < PrevMatchEnd)
				&& (OriginalMatches[i].MatchLength + OriginalMatches[i].MatchOffset > PrevMatchEnd + 6))
			{
				MatchDiff = PrevMatchEnd - OriginalMatches[i].MatchOffset;

				OriginalMatch = &OriginalMatches[i];
				OptimizedMatch = &OptimizedMatches[j];
				
				OptimizedMatch->MatchOffset = OriginalMatch->MatchOffset;
				OptimizedMatch->ChunkOffset = OriginalMatch->ChunkOffset;
				OptimizedMatch->MatchLength = OriginalMatch->MatchLength;

				if (OptimizedMatches[j].MatchLength <= MatchDiff)
					return -5001; /* error */

				if (MatchDiff >= 20000)
					return -5002; /* error */
				
				OptimizedMatches[j].MatchLength -= MatchDiff;
				OptimizedMatches[j].MatchOffset += MatchDiff;
				OptimizedMatches[j].ChunkOffset += MatchDiff;

				PrevMatchEnd = OptimizedMatches[j].MatchLength + OptimizedMatches[j].MatchOffset;
				TotalMatchLength += OptimizedMatches[j].MatchLength;
				
				j++;
			}
		}
		else
		{
			OriginalMatch = &OriginalMatches[i];
			OptimizedMatch = &OptimizedMatches[j];

			OptimizedMatch->MatchOffset = OriginalMatch->MatchOffset;
			OptimizedMatch->ChunkOffset = OriginalMatch->ChunkOffset;
			OptimizedMatch->MatchLength = OriginalMatch->MatchLength;
			
			PrevMatchEnd = OptimizedMatches[j].MatchLength + OptimizedMatches[j].MatchOffset;
			TotalMatchLength += OptimizedMatches[j].MatchLength;

			j++;
		}
	}

	OptimizedMatchCount = j;
	xcrush->OptimizedMatchCount = OptimizedMatchCount;

	return (int) TotalMatchLength;
}

int xcrush_generate_output(XCRUSH_CONTEXT* xcrush, BYTE* OutputBuffer, UINT32 OutputSize, UINT32 HistoryOffset, UINT32* pDstSize)
{
	BYTE* Literals;
	BYTE* OutputEnd;
	UINT32 MatchIndex;
	UINT32 MatchOffset;
	UINT16 MatchLength;
	UINT32 MatchCount;
	UINT32 CurrentOffset;
	UINT32 MatchOffsetDiff;
	UINT32 HistoryOffsetDiff;
	RDP61_MATCH_DETAILS* MatchDetails;

	MatchCount = xcrush->OptimizedMatchCount;

	OutputEnd = &OutputBuffer[OutputSize];

	if (&OutputBuffer[2] >= &OutputBuffer[OutputSize])
		return -6001; /* error */

	*((UINT16*) OutputBuffer) = MatchCount;

	MatchDetails = (RDP61_MATCH_DETAILS*) &OutputBuffer[2];
	Literals = (BYTE*) &MatchDetails[MatchCount];

	if (Literals > OutputEnd)
		return -6002; /* error */
	
	for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex++)
	{		
		MatchDetails[MatchIndex].MatchLength = (UINT16) (xcrush->OptimizedMatches[MatchIndex].MatchLength);
		MatchDetails[MatchIndex].MatchOutputOffset = (UINT16) (xcrush->OptimizedMatches[MatchIndex].MatchOffset - HistoryOffset);
		MatchDetails[MatchIndex].MatchHistoryOffset = xcrush->OptimizedMatches[MatchIndex].ChunkOffset;
	}

	CurrentOffset = HistoryOffset;
	
	for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex++)
	{
		MatchLength = (UINT16) (xcrush->OptimizedMatches[MatchIndex].MatchLength);
		MatchOffset = xcrush->OptimizedMatches[MatchIndex].MatchOffset;
		
		if (MatchOffset <= CurrentOffset)
		{
			if (MatchOffset != CurrentOffset)
				return -6003; /* error */

			CurrentOffset = MatchOffset + MatchLength;
		}
		else
		{
			MatchOffsetDiff = MatchOffset - CurrentOffset;

			if (Literals + MatchOffset - CurrentOffset >= OutputEnd)
				return -6004; /* error */

			CopyMemory(Literals, &xcrush->HistoryBuffer[CurrentOffset], MatchOffsetDiff);

			if (Literals >= OutputEnd)
				return -6005; /* error */

			Literals += MatchOffsetDiff;
			CurrentOffset = MatchOffset + MatchLength;
		}
	}
	
	HistoryOffsetDiff = xcrush->HistoryOffset - CurrentOffset;
	
	if (Literals + HistoryOffsetDiff >= OutputEnd)
		return -6006; /* error */

	CopyMemory(Literals, &xcrush->HistoryBuffer[CurrentOffset], HistoryOffsetDiff);
	*pDstSize = Literals + HistoryOffsetDiff - OutputBuffer;

	return 1;
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

	if (!(Level2ComprFlags & PACKET_COMPRESSED))
	{
		pDstData = pSrcData;
		DstSize = SrcSize;

		status = xcrush_decompress_l1(xcrush, pDstData, DstSize, ppDstData, pDstSize, Level1ComprFlags);

		return status;
	}

	if (Level2ComprFlags & PACKET_FLUSHED)
	{
		//xcrush_context_reset(xcrush, FALSE);
	}

	status = mppc_decompress(xcrush->mppc, pSrcData, SrcSize, &pDstData, &DstSize, Level2ComprFlags);

	if (status < 0)
		return status;

	status = xcrush_decompress_l1(xcrush, pDstData, DstSize, ppDstData, pDstSize, Level1ComprFlags);

	return status;
}

int xcrush_compress_l1(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status = 0;
	UINT32 Flags = 0;
	UINT32 HistoryOffset = 0;
	BYTE* HistoryPtr = NULL;
	BYTE* HistoryBuffer = NULL;
	UINT32 SignatureIndex = 0;

	if (xcrush->HistoryOffset + SrcSize + 8 > xcrush->HistoryBufferSize)
	{
		xcrush->HistoryOffset = 0;
		Flags |= L1_PACKET_AT_FRONT;
	}

	HistoryOffset = xcrush->HistoryOffset;
	HistoryBuffer = xcrush->HistoryBuffer;
	HistoryPtr = &HistoryBuffer[HistoryOffset];

	MoveMemory(HistoryPtr, pSrcData, SrcSize);
	xcrush->HistoryOffset += SrcSize;

	if (SrcSize > 50)
	{
		SignatureIndex = xcrush_compute_signatures(xcrush, pSrcData, SrcSize);

		if (SignatureIndex)
		{
			status = xcrush_find_all_matches(xcrush,
					SignatureIndex, HistoryOffset, 0, SrcSize);

			if (status < 0)
				return status;

			xcrush->OriginalMatchCount = (UINT32) status;

			xcrush->OptimizedMatchCount = 0;

			if (xcrush->OriginalMatchCount)
			{
				status = xcrush_optimize_matches(xcrush);

				if (status < 0)
					return status;
			}

			if (xcrush->OptimizedMatchCount)
			{
				status = xcrush_generate_output(xcrush, *ppDstData, SrcSize, HistoryOffset, pDstSize);

				if (status < 0)
					return status;

				if (status > 0)
					Flags |= L1_COMPRESSED;
			}
		}
	}

	if (!(Flags & L1_COMPRESSED))
	{
		Flags |= L1_NO_COMPRESSION;
		*pDstSize = SrcSize;
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
		return -1001;

	if ((SrcSize + 2) > *pDstSize)
		return -1002;

	OriginalData = *ppDstData;
	OriginalDataSize = SrcSize;

	pDstData = xcrush->BlockBuffer;
	CompressedDataSize = SrcSize;

	status = xcrush_compress_l1(xcrush, pSrcData, SrcSize, &pDstData, &CompressedDataSize, &Level1ComprFlags);

	if (status < 0)
		return status;

	if (Level1ComprFlags & L1_COMPRESSED)
	{
		CompressedData = pDstData;

		if (CompressedDataSize > SrcSize)
			return -1003;
	}
	else
	{
		CompressedData = pSrcData;

		if (CompressedDataSize != SrcSize)
			return -1004;
	}

	status = 0;

	pDstData = &OriginalData[2];
	DstSize = OriginalDataSize - 2;

	if (CompressedDataSize > 50)
	{
		status = mppc_compress(xcrush->mppc, CompressedData, CompressedDataSize, &pDstData, &DstSize, &Level2ComprFlags);
	}

	if (status < 0)
		return status;

	if (!status || (Level2ComprFlags & PACKET_FLUSHED))
	{
		if (CompressedDataSize > DstSize)
		{
			xcrush_context_reset(xcrush, TRUE);
			*ppDstData = pSrcData;
			*pDstSize = SrcSize;
			*pFlags = 0;
			return 1;
		}

		DstSize = CompressedDataSize;
		CopyMemory(&OriginalData[2], CompressedData, CompressedDataSize);
	}

	if (Level2ComprFlags & PACKET_COMPRESSED)
	{
		Level2ComprFlags |= xcrush->CompressionFlags;
		xcrush->CompressionFlags = 0;
	}
	else if (Level2ComprFlags & PACKET_FLUSHED)
	{
		xcrush->CompressionFlags = PACKET_FLUSHED;
	}

	Level1ComprFlags |= L1_INNER_COMPRESSION;

	OriginalData[0] = (BYTE) Level1ComprFlags;
	OriginalData[1] = (BYTE) Level2ComprFlags;

#if 0
	WLog_DBG(TAG, "XCrushCompress: Level1ComprFlags: %s Level2ComprFlags: %s",
			xcrush_get_level_1_compression_flags_string(Level1ComprFlags),
			xcrush_get_level_2_compression_flags_string(Level2ComprFlags));
#endif

	if (*pDstSize < (DstSize + 2))
		return -1006;

	*pDstSize = DstSize + 2;
	*pFlags = PACKET_COMPRESSED | CompressionLevel;

	return 1;
}

void xcrush_context_reset(XCRUSH_CONTEXT* xcrush, BOOL flush)
{
	xcrush->SignatureIndex = 0;
	xcrush->SignatureCount = 1000;
	ZeroMemory(&(xcrush->Signatures), sizeof(XCRUSH_SIGNATURE) * xcrush->SignatureCount);

	xcrush->CompressionFlags = 0;

	xcrush->ChunkHead = xcrush->ChunkTail = 1;
	ZeroMemory(&(xcrush->Chunks), sizeof(xcrush->Chunks));
	ZeroMemory(&(xcrush->NextChunks), sizeof(xcrush->NextChunks));

	ZeroMemory(&(xcrush->OriginalMatches), sizeof(xcrush->OriginalMatches));
	ZeroMemory(&(xcrush->OptimizedMatches), sizeof(xcrush->OptimizedMatches));

	if (flush)
		xcrush->HistoryOffset = xcrush->HistoryBufferSize + 1;
	else
		xcrush->HistoryOffset = 0;

	mppc_context_reset(xcrush->mppc, flush);
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

		xcrush_context_reset(xcrush, FALSE);
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

