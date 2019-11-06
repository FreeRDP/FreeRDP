/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ZGFX (RDP8) Bulk Data Compression
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

#ifndef FREERDP_CODEC_ZGFX_H
#define FREERDP_CODEC_ZGFX_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/bulk.h>

#define ZGFX_SEGMENTED_SINGLE 0xE0
#define ZGFX_SEGMENTED_MULTIPART 0xE1

#define ZGFX_PACKET_COMPR_TYPE_RDP8 0x04

#define ZGFX_SEGMENTED_MAXSIZE 65535

typedef struct _ZGFX_CONTEXT ZGFX_CONTEXT;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API int zgfx_decompress(ZGFX_CONTEXT* zgfx, const BYTE* pSrcData, UINT32 SrcSize,
	                                BYTE** ppDstData, UINT32* pDstSize, UINT32 flags);
	FREERDP_API int zgfx_compress(ZGFX_CONTEXT* zgfx, const BYTE* pSrcData, UINT32 SrcSize,
	                              BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags);
	FREERDP_API int zgfx_compress_to_stream(ZGFX_CONTEXT* zgfx, wStream* sDst,
	                                        const BYTE* pUncompressed, UINT32 uncompressedSize,
	                                        UINT32* pFlags);

	FREERDP_API void zgfx_context_reset(ZGFX_CONTEXT* zgfx, BOOL flush);

	FREERDP_API ZGFX_CONTEXT* zgfx_context_new(BOOL Compressor);
	FREERDP_API void zgfx_context_free(ZGFX_CONTEXT* zgfx);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_ZGFX_H */
