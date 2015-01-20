/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#ifndef FREERDP_CODEC_H264_H
#define FREERDP_CODEC_H264_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/rdpgfx.h>

typedef struct _H264_CONTEXT H264_CONTEXT;

typedef BOOL (*pfnH264SubsystemInit)(H264_CONTEXT* h264);
typedef void (*pfnH264SubsystemUninit)(H264_CONTEXT* h264);

typedef int (*pfnH264SubsystemDecompress)(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize);

struct _H264_CONTEXT_SUBSYSTEM
{
	const char* name;
	pfnH264SubsystemInit Init;
	pfnH264SubsystemUninit Uninit;
	pfnH264SubsystemDecompress Decompress;
};
typedef struct _H264_CONTEXT_SUBSYSTEM H264_CONTEXT_SUBSYSTEM;

struct _H264_CONTEXT
{
	BOOL Compressor;

	UINT32 width;
	UINT32 height;
	
	int iStride[3];
	BYTE* pYUVData[3];

	void* pSystemData;
	H264_CONTEXT_SUBSYSTEM* subsystem;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int h264_compress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API int h264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nDstWidth, int nDstHeight,
		RDPGFX_RECT16* regionRects, int numRegionRect);

FREERDP_API int h264_context_reset(H264_CONTEXT* h264);

FREERDP_API H264_CONTEXT* h264_context_new(BOOL Compressor);
FREERDP_API void h264_context_free(H264_CONTEXT* h264);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_H264_H */

