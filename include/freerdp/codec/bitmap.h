/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Compressed Bitmap
 *
 * Copyright 2011 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef FREERDP_CODEC_BITMAP_H
#define FREERDP_CODEC_BITMAP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/color.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API BOOL bitmap_decompress(BYTE* srcData, BYTE* dstData, int width, int height, int size, int srcBpp, int dstBpp);

FREERDP_API int freerdp_bitmap_compress(char* in_data, int width, int height,
		wStream* s, int bpp, int byte_limit, int start_line, wStream* temp_s, int e);

#define PLANAR_FORMAT_HEADER_CS		(1 << 3)
#define PLANAR_FORMAT_HEADER_RLE	(1 << 4)
#define PLANAR_FORMAT_HEADER_NA		(1 << 5)
#define PLANAR_FORMAT_HEADER_CLL_MASK	0x07

typedef struct _BITMAP_PLANAR_CONTEXT BITMAP_PLANAR_CONTEXT;

FREERDP_API BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* context, BYTE* data, UINT32 format,
		int width, int height, int scanline, BYTE* dstData, int* dstSize);

FREERDP_API BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, int maxWidth, int maxHeight);
FREERDP_API void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context);

FREERDP_API int planar_decompress(BITMAP_PLANAR_CONTEXT* planar, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_BITMAP_H */
