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

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_CLEAR_CONTEXT CLEAR_CONTEXT;

	FREERDP_API int clear_compress(CLEAR_CONTEXT* WINPR_RESTRICT clear,
	                               const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                               BYTE** WINPR_RESTRICT ppDstData,
	                               UINT32* WINPR_RESTRICT pDstSize);

	FREERDP_API INT32 clear_decompress(CLEAR_CONTEXT* WINPR_RESTRICT clear,
	                                   const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                                   UINT32 nWidth, UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
	                                   UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
	                                   UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
	                                   const gdiPalette* WINPR_RESTRICT palette);

	FREERDP_API BOOL clear_context_reset(CLEAR_CONTEXT* WINPR_RESTRICT clear);

	FREERDP_API void clear_context_free(CLEAR_CONTEXT* WINPR_RESTRICT clear);

	WINPR_ATTR_MALLOC(clear_context_free, 1)
	FREERDP_API CLEAR_CONTEXT* clear_context_new(BOOL Compressor);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_CLEAR_H */
