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

typedef struct S_PROGRESSIVE_CONTEXT PROGRESSIVE_CONTEXT;

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API int progressive_compress(PROGRESSIVE_CONTEXT* progressive, const BYTE* pSrcData,
	                                     UINT32 SrcSize, UINT32 SrcFormat, UINT32 Width,
	                                     UINT32 Height, UINT32 ScanLine,
	                                     const REGION16* invalidRegion, BYTE** ppDstData,
	                                     UINT32* pDstSize);

	FREERDP_API INT32 progressive_decompress(PROGRESSIVE_CONTEXT* progressive, const BYTE* pSrcData,
	                                         UINT32 SrcSize, BYTE* pDstData, UINT32 DstFormat,
	                                         UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                         REGION16* invalidRegion, UINT16 surfaceId,
	                                         UINT32 frameId);

	FREERDP_API INT32 progressive_create_surface_context(PROGRESSIVE_CONTEXT* progressive,
	                                                     UINT16 surfaceId, UINT32 width,
	                                                     UINT32 height);
	FREERDP_API int progressive_delete_surface_context(PROGRESSIVE_CONTEXT* progressive,
	                                                   UINT16 surfaceId);

	FREERDP_API BOOL progressive_context_reset(PROGRESSIVE_CONTEXT* progressive);

	FREERDP_API PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor);
	FREERDP_API void progressive_context_free(PROGRESSIVE_CONTEXT* progressive);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_PROGRESSIVE_H */
