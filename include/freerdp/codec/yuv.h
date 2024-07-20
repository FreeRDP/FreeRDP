/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * YUV decoder
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CODEC_YUV_H
#define FREERDP_CODEC_YUV_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_YUV_CONTEXT YUV_CONTEXT;

	FREERDP_API BOOL yuv420_context_decode(
	    YUV_CONTEXT* WINPR_RESTRICT context, const BYTE* WINPR_RESTRICT pYUVData[3],
	    const UINT32 iStride[3], UINT32 yuvHeight, DWORD DstFormat, BYTE* WINPR_RESTRICT dest,
	    UINT32 nDstStep, const RECTANGLE_16* WINPR_RESTRICT regionRects, UINT32 numRegionRects);
	FREERDP_API BOOL yuv420_context_encode(YUV_CONTEXT* WINPR_RESTRICT context,
	                                       const BYTE* WINPR_RESTRICT rgbData, UINT32 srcStep,
	                                       UINT32 srcFormat, const UINT32 iStride[3],
	                                       BYTE* WINPR_RESTRICT yuvData[3],
	                                       const RECTANGLE_16* WINPR_RESTRICT regionRects,
	                                       UINT32 numRegionRects);

	FREERDP_API BOOL yuv444_context_decode(
	    YUV_CONTEXT* WINPR_RESTRICT context, BYTE type, const BYTE* WINPR_RESTRICT pYUVData[3],
	    const UINT32 iStride[3], UINT32 srcYuvHeight, BYTE* WINPR_RESTRICT pYUVDstData[3],
	    const UINT32 iDstStride[3], DWORD DstFormat, BYTE* WINPR_RESTRICT dest, UINT32 nDstStep,
	    const RECTANGLE_16* WINPR_RESTRICT regionRects, UINT32 numRegionRects);
	FREERDP_API BOOL yuv444_context_encode(YUV_CONTEXT* WINPR_RESTRICT context, BYTE version,
	                                       const BYTE* WINPR_RESTRICT pSrcData, UINT32 nSrcStep,
	                                       UINT32 SrcFormat, const UINT32 iStride[3],
	                                       BYTE* WINPR_RESTRICT pYUVLumaData[3],
	                                       BYTE* WINPR_RESTRICT pYUVChromaData[3],
	                                       const RECTANGLE_16* WINPR_RESTRICT regionRects,
	                                       UINT32 numRegionRects);

	FREERDP_API BOOL yuv_context_reset(YUV_CONTEXT* WINPR_RESTRICT context, UINT32 width,
	                                   UINT32 height);

	FREERDP_API void yuv_context_free(YUV_CONTEXT* context);

	WINPR_ATTR_MALLOC(yuv_context_free, 1)
	FREERDP_API YUV_CONTEXT* yuv_context_new(BOOL encoder, UINT32 ThreadingFlags);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_YUV_H */
