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

#define CLEARCODEC_FLAG_GLYPH_INDEX	0x01
#define CLEARCODEC_FLAG_GLYPH_HIT	0x02
#define CLEARCODEC_FLAG_CACHE_RESET	0x03

struct _CLEAR_CONTEXT
{
	BOOL Compressor;
	UINT32 VBarStorageCursor;
	void* VBarStorage[32768];
	UINT32 ShortVBarStorageCursor;
	void* ShortVBarStorage[16384];
};
typedef struct _CLEAR_CONTEXT CLEAR_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int clear_compress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);
FREERDP_API int clear_decompress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API void clear_context_reset(CLEAR_CONTEXT* clear);

FREERDP_API CLEAR_CONTEXT* clear_context_new(BOOL Compressor);
FREERDP_API void clear_context_free(CLEAR_CONTEXT* clear);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_CLEAR_H */
 
