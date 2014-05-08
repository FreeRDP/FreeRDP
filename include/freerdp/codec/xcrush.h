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

#ifndef FREERDP_CODEC_XCRUSH_H
#define FREERDP_CODEC_XCRUSH_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/mppc.h>

struct _RDP61_MATCH_DETAILS
{
	UINT16 MatchLength;
	UINT16 MatchOutputOffset;
	UINT32 MatchHistoryOffset;
};
typedef struct _RDP61_MATCH_DETAILS RDP61_MATCH_DETAILS;

struct _RDP61_COMPRESSED_DATA
{
	BYTE Level1ComprFlags;
	BYTE Level2ComprFlags;
	UINT16 MatchCount;
	RDP61_MATCH_DETAILS* MatchDetails;
	BYTE* Literals;
};
typedef struct _RDP61_COMPRESSED_DATA RDP61_COMPRESSED_DATA;

struct _XCRUSH_CONTEXT
{
	BOOL Compressor;
	MPPC_CONTEXT* mppc;
	BYTE* HistoryPtr;
	UINT32 HistoryOffset;
	UINT32 HistoryBufferSize;
	BYTE HistoryBuffer[2000000];
	BYTE BlockBuffer[16384];
};
typedef struct _XCRUSH_CONTEXT XCRUSH_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int xcrush_compress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags);
FREERDP_API int xcrush_decompress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags);

FREERDP_API void xcrush_context_reset(XCRUSH_CONTEXT* xcrush);

FREERDP_API XCRUSH_CONTEXT* xcrush_context_new(BOOL Compressor);
FREERDP_API void xcrush_context_free(XCRUSH_CONTEXT* xcrush);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_XCRUSH_H */
 
