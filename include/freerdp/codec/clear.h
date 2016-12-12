/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
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

#ifndef FREERDP_CODEC_CLEAR_H
#define FREERDP_CODEC_CLEAR_H

typedef struct _CLEAR_CONTEXT CLEAR_CONTEXT;

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#define CLEARCODEC_FLAG_GLYPH_INDEX	0x01
#define CLEARCODEC_FLAG_GLYPH_HIT	0x02
#define CLEARCODEC_FLAG_CACHE_RESET	0x04

#define CLEARCODEC_VBAR_SIZE 32768
#define CLEARCODEC_VBAR_SHORT_SIZE 16384

struct _CLEAR_GLYPH_ENTRY
{
	UINT32 size;
	UINT32 count;
	UINT32* pixels;
};
typedef struct _CLEAR_GLYPH_ENTRY CLEAR_GLYPH_ENTRY;

struct _CLEAR_VBAR_ENTRY
{
	UINT32 size;
	UINT32 count;
	BYTE* pixels;
};
typedef struct _CLEAR_VBAR_ENTRY CLEAR_VBAR_ENTRY;

struct _CLEAR_CONTEXT
{
	BOOL Compressor;
	NSC_CONTEXT* nsc;
	UINT32 seqNumber;
	BYTE* TempBuffer;
	UINT32 TempSize;
	UINT32 nTempStep;
	UINT32 TempFormat;
	UINT32 format;
	CLEAR_GLYPH_ENTRY GlyphCache[4000];
	UINT32 VBarStorageCursor;
	CLEAR_VBAR_ENTRY VBarStorage[CLEARCODEC_VBAR_SIZE];
	UINT32 ShortVBarStorageCursor;
	CLEAR_VBAR_ENTRY ShortVBarStorage[CLEARCODEC_VBAR_SHORT_SIZE];
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int clear_compress(CLEAR_CONTEXT* clear, BYTE* pSrcData,
                               UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API INT32 clear_decompress(CLEAR_CONTEXT* clear, const BYTE* pSrcData,
                                   UINT32 SrcSize, UINT32 nWidth, UINT32 nHeight,
                                   BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                   UINT32 nXDst, UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
                                   const gdiPalette* palette);

FREERDP_API BOOL clear_context_reset(CLEAR_CONTEXT* clear);

FREERDP_API CLEAR_CONTEXT* clear_context_new(BOOL Compressor);
FREERDP_API void clear_context_free(CLEAR_CONTEXT* clear);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_CLEAR_H */

