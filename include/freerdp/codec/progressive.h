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

#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_PROGRESSIVE_CONTEXT PROGRESSIVE_CONTEXT;

	FREERDP_API int progressive_compress(PROGRESSIVE_CONTEXT* WINPR_RESTRICT progressive,
	                                     const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                                     UINT32 SrcFormat, UINT32 Width, UINT32 Height,
	                                     UINT32 ScanLine,
	                                     const REGION16* WINPR_RESTRICT invalidRegion,
	                                     BYTE** WINPR_RESTRICT ppDstData,
	                                     UINT32* WINPR_RESTRICT pDstSize);

	FREERDP_API INT32 progressive_decompress(PROGRESSIVE_CONTEXT* WINPR_RESTRICT progressive,
	                                         const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                                         BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
	                                         UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                         REGION16* WINPR_RESTRICT invalidRegion,
	                                         UINT16 surfaceId, UINT32 frameId);

	FREERDP_API INT32
	progressive_create_surface_context(PROGRESSIVE_CONTEXT* WINPR_RESTRICT progressive,
	                                   UINT16 surfaceId, UINT32 width, UINT32 height);
	FREERDP_API int
	progressive_delete_surface_context(PROGRESSIVE_CONTEXT* WINPR_RESTRICT progressive,
	                                   UINT16 surfaceId);

	FREERDP_API BOOL progressive_context_reset(PROGRESSIVE_CONTEXT* WINPR_RESTRICT progressive);

	FREERDP_API void progressive_context_free(PROGRESSIVE_CONTEXT* progressive);

	WINPR_ATTR_MALLOC(progressive_context_free, 1)
	FREERDP_API PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor);

	WINPR_ATTR_MALLOC(progressive_context_free, 1)
	FREERDP_API PROGRESSIVE_CONTEXT* progressive_context_new_ex(BOOL Compressor,
	                                                            UINT32 ThreadingFlags);

	/** Write a RFX message as simple progressive message to a stream.
	 *  Forward wrapper for \link rfx_write_message_progressive_simple
	 *  @param progressive The progressive codec context
	 *  @param s The stream to write to
	 *  @param msg The message to encode
	 *
	 *  @since version 3.0.0
	 *  @return \b TRUE in case of success, \b FALSE for any error
	 */
	FREERDP_API BOOL progressive_rfx_write_message_progressive_simple(
	    PROGRESSIVE_CONTEXT* progressive, wStream* s, const RFX_MESSAGE* msg);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_PROGRESSIVE_H */
