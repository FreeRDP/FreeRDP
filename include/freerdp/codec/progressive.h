/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Progressive Codec Bitmap Compression
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

#ifndef FREERDP_CODEC_PROGRESSIVE_H
#define FREERDP_CODEC_PROGRESSIVE_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/color.h>

struct _PROGRESSIVE_CONTEXT
{
	BOOL Compressor;
};
typedef struct _PROGRESSIVE_CONTEXT PROGRESSIVE_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int progressive_compress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight);

FREERDP_API void progressive_context_reset(PROGRESSIVE_CONTEXT* progressive);

FREERDP_API PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor);
FREERDP_API void progressive_context_free(PROGRESSIVE_CONTEXT* progressive);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_PROGRESSIVE_H */
 
