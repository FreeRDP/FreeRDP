/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP6 Planar Codec
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#ifndef FREERDP_CODEC_PLANAR_H
#define FREERDP_CODEC_PLANAR_H

#include <winpr/crt.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#define PLANAR_FORMAT_HEADER_CS (1 << 3)
#define PLANAR_FORMAT_HEADER_RLE (1 << 4)
#define PLANAR_FORMAT_HEADER_NA (1 << 5)
#define PLANAR_FORMAT_HEADER_CLL_MASK 0x07

#define PLANAR_CONTROL_BYTE(_nRunLength, _cRawBytes) \
	(_nRunLength & 0x0F) | ((_cRawBytes & 0x0F) << 4)

#define PLANAR_CONTROL_BYTE_RUN_LENGTH(_controlByte) (_controlByte & 0x0F)
#define PLANAR_CONTROL_BYTE_RAW_BYTES(_controlByte) ((_controlByte >> 4) & 0x0F)

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_BITMAP_PLANAR_CONTEXT BITMAP_PLANAR_CONTEXT;

	FREERDP_API BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT context,
	                                                 const BYTE* WINPR_RESTRICT data, UINT32 format,
	                                                 UINT32 width, UINT32 height, UINT32 scanline,
	                                                 BYTE* WINPR_RESTRICT dstData,
	                                                 UINT32* WINPR_RESTRICT pDstSize);

	FREERDP_API BOOL freerdp_bitmap_planar_context_reset(
	    BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT context, UINT32 width, UINT32 height);

	FREERDP_API void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context);

	WINPR_ATTR_MALLOC(freerdp_bitmap_planar_context_free, 1)
	FREERDP_API BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, UINT32 width,
	                                                                     UINT32 height);

	FREERDP_API void freerdp_planar_switch_bgr(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar,
	                                           BOOL bgr);
	FREERDP_API void freerdp_planar_topdown_image(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar,
	                                              BOOL topdown);

	FREERDP_API BOOL planar_decompress(BITMAP_PLANAR_CONTEXT* WINPR_RESTRICT planar,
	                                   const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                                   UINT32 nSrcWidth, UINT32 nSrcHeight,
	                                   BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
	                                   UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                   UINT32 nDstWidth, UINT32 nDstHeight, BOOL vFlip);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_PLANAR_H */
