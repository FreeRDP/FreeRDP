/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Interleaved RLE Bitmap Codec
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

#ifndef FREERDP_CODEC_INTERLEAVED_H
#define FREERDP_CODEC_INTERLEAVED_H

typedef struct _BITMAP_INTERLEAVED_CONTEXT BITMAP_INTERLEAVED_CONTEXT;

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

struct _BITMAP_INTERLEAVED_CONTEXT
{
	BOOL Compressor;

	UINT32 TempSize;
	BYTE* TempBuffer;

	wStream* bts;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL interleaved_decompress(BITMAP_INTERLEAVED_CONTEXT* interleaved,
	                                        const BYTE* pSrcData, UINT32 SrcSize, UINT32 nSrcWidth,
	                                        UINT32 nSrcHeight, UINT32 bpp, BYTE* pDstData,
	                                        UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
	                                        UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
	                                        const gdiPalette* palette);

	FREERDP_API BOOL interleaved_compress(BITMAP_INTERLEAVED_CONTEXT* interleaved, BYTE* pDstData,
	                                      UINT32* pDstSize, UINT32 nWidth, UINT32 nHeight,
	                                      const BYTE* pSrcData, UINT32 SrcFormat, UINT32 nSrcStep,
	                                      UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* palette,
	                                      UINT32 bpp);

	FREERDP_API BOOL bitmap_interleaved_context_reset(BITMAP_INTERLEAVED_CONTEXT* interleaved);

	FREERDP_API BITMAP_INTERLEAVED_CONTEXT* bitmap_interleaved_context_new(BOOL Compressor);
	FREERDP_API void bitmap_interleaved_context_free(BITMAP_INTERLEAVED_CONTEXT* interleaved);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_INTERLEAVED_H */
