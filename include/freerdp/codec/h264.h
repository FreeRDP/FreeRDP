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

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/rdpgfx.h>

typedef struct S_H264_CONTEXT_SUBSYSTEM H264_CONTEXT_SUBSYSTEM;
typedef struct S_YUV_CONTEXT YUV_CONTEXT;

typedef enum
{
	H264_RATECONTROL_VBR = 0,
	H264_RATECONTROL_CQP
} H264_RATECONTROL_MODE;

typedef struct
{
	BOOL Compressor;

	UINT32 width;
	UINT32 height;

	H264_RATECONTROL_MODE RateControlMode;
	UINT32 BitRate;
	UINT32 FrameRate;
	UINT32 QP;
	UINT32 NumberOfThreads;

	UINT32 iStride[3];
	BYTE* pOldYUVData[3];
	BYTE* pYUVData[3];

	UINT32 iYUV444Size[3];
	UINT32 iYUV444Stride[3];
	BYTE* pOldYUV444Data[3];
	BYTE* pYUV444Data[3];

	UINT32 numSystemData;
	void* pSystemData;
	const H264_CONTEXT_SUBSYSTEM* subsystem;
	YUV_CONTEXT* yuv;

	BOOL encodingBuffer;
	BOOL firstLumaFrameDone;
	BOOL firstChromaFrameDone;

	void* lumaData;
	wLog* log;
} H264_CONTEXT;

#ifdef __cplusplus
extern "C"
{
#endif

	static INLINE void free_h264_metablock(RDPGFX_H264_METABLOCK* meta)
	{
		RDPGFX_H264_METABLOCK m = { 0 };
		if (!meta)
			return;
		free(meta->quantQualityVals);
		free(meta->regionRects);
		*meta = m;
	}

	FREERDP_API INT32 avc420_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
	                                  UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
	                                  const RECTANGLE_16* regionRect, BYTE** ppDstData,
	                                  UINT32* pDstSize, RDPGFX_H264_METABLOCK* meta);

	FREERDP_API INT32 avc420_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize,
	                                    BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                    UINT32 nDstWidth, UINT32 nDstHeight,
	                                    const RECTANGLE_16* regionRects, UINT32 numRegionRect);

	FREERDP_API INT32 avc444_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
	                                  UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
	                                  BYTE version, const RECTANGLE_16* regionRect, BYTE* op,
	                                  BYTE** pDstData, UINT32* pDstSize, BYTE** pAuxDstData,
	                                  UINT32* pAuxDstSize, RDPGFX_H264_METABLOCK* meta,
	                                  RDPGFX_H264_METABLOCK* auxMeta);

	FREERDP_API INT32 avc444_decompress(H264_CONTEXT* h264, BYTE op,
	                                    const RECTANGLE_16* regionRects, UINT32 numRegionRect,
	                                    const BYTE* pSrcData, UINT32 SrcSize,
	                                    const RECTANGLE_16* auxRegionRects, UINT32 numAuxRegionRect,
	                                    const BYTE* pAuxSrcData, UINT32 AuxSrcSize, BYTE* pDstData,
	                                    DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
	                                    UINT32 nDstHeight, UINT32 codecId);

	FREERDP_API BOOL h264_context_reset(H264_CONTEXT* h264, UINT32 width, UINT32 height);

	FREERDP_API H264_CONTEXT* h264_context_new(BOOL Compressor);
	FREERDP_API void h264_context_free(H264_CONTEXT* h264);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_H264_H */
