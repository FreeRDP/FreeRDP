/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/library.h>
#include <winpr/bitstream.h>
#include <winpr/synch.h>

#include <freerdp/primitives.h>
#include <freerdp/codec/h264.h>
#include <freerdp/log.h>

#include "h264.h"

#define TAG FREERDP_TAG("codec")

static BOOL avc444_ensure_buffer(H264_CONTEXT* h264, DWORD nDstHeight);

BOOL avc420_ensure_buffer(H264_CONTEXT* h264, UINT32 stride, UINT32 width, UINT32 height)
{
	if (!h264)
		return FALSE;

	if (stride == 0)
		stride = width;

	if (stride % 16 != 0)
		stride += 16 - stride % 16;

	if (height % 16 != 0)
		height += 16 - height % 16;

	if (!h264->pYUVData[0] || !h264->pYUVData[1] || !h264->pYUVData[2] ||
	    (width != h264->width) || (height != h264->height) || (stride != h264->iStride[0]))
	{
		h264->iStride[0] = stride;
		h264->iStride[1] = (stride + 1) / 2;
		h264->iStride[2] = (stride + 1) / 2;
		h264->width = width;
		h264->height = height;
		_aligned_free(h264->pYUVData[0]);
		_aligned_free(h264->pYUVData[1]);
		_aligned_free(h264->pYUVData[2]);
		h264->pYUVData[0] = _aligned_malloc(h264->iStride[0] * height, 16);
		h264->pYUVData[1] = _aligned_malloc(h264->iStride[1] * height, 16);
		h264->pYUVData[2] = _aligned_malloc(h264->iStride[2] * height, 16);

		if (!h264->pYUVData[0] || !h264->pYUVData[1] || !h264->pYUVData[2])
			return FALSE;
	}

	return TRUE;
}


static BOOL check_rect(const H264_CONTEXT* h264, const RECTANGLE_16* rect,
                       UINT32 nDstWidth, UINT32 nDstHeight)
{
	/* Check, if the output rectangle is valid in decoded h264 frame. */
	if ((rect->right > h264->width) || (rect->left > h264->width))
		return FALSE;

	if ((rect->top > h264->height) || (rect->bottom > h264->height))
		return FALSE;

	/* Check, if the output rectangle is valid in destination buffer. */
	if ((rect->right > nDstWidth) || (rect->left > nDstWidth))
		return FALSE;

	if ((rect->bottom > nDstHeight) || (rect->top > nDstHeight))
		return FALSE;

	return TRUE;
}

static BOOL avc_yuv_to_rgb(H264_CONTEXT* h264, const RECTANGLE_16* regionRects,
                           UINT32 numRegionRects, UINT32 nDstWidth,
                           UINT32 nDstHeight, UINT32 nDstStep, BYTE* pDstData,
                           DWORD DstFormat, BOOL use444)
{
	UINT32 x;
	BYTE* pDstPoint;
	prim_size_t roi;
	INT32 width, height;
	const BYTE* pYUVPoint[3];
	primitives_t* prims = primitives_get();

	for (x = 0; x < numRegionRects; x++)
	{
		const RECTANGLE_16* rect = &(regionRects[x]);
		const UINT32* iStride;
		BYTE** ppYUVData;

		if (use444)
		{
			iStride = h264->iYUV444Stride;
			ppYUVData = h264->pYUV444Data;
		}
		else
		{
			iStride = h264->iStride;
			ppYUVData = h264->pYUVData;
		}

		if (!check_rect(h264, rect, nDstWidth, nDstHeight))
			return FALSE;

		width = rect->right - rect->left;
		height = rect->bottom - rect->top;
		pDstPoint = pDstData + rect->top * nDstStep + rect->left * 4;
		pYUVPoint[0] = ppYUVData[0] + rect->top * iStride[0] + rect->left;
		pYUVPoint[1] = ppYUVData[1];
		pYUVPoint[2] = ppYUVData[2];

		if (use444)
		{
			pYUVPoint[1] += rect->top * iStride[1] + rect->left;
			pYUVPoint[2] += rect->top * iStride[2] + rect->left;
		}
		else
		{
			pYUVPoint[1] += rect->top / 2 * iStride[1] + rect->left / 2;
			pYUVPoint[2] += rect->top / 2 * iStride[2] + rect->left / 2;
		}

		roi.width = width;
		roi.height = height;

		if (use444)
		{
			if (prims->YUV444ToRGB_8u_P3AC4R(
			        pYUVPoint, iStride, pDstPoint, nDstStep,
			        DstFormat, &roi) != PRIMITIVES_SUCCESS)
			{
				return FALSE;
			}
		}
		else
		{
			if (prims->YUV420ToRGB_8u_P3AC4R(
			        pYUVPoint, iStride, pDstPoint, nDstStep,
			        DstFormat, &roi) != PRIMITIVES_SUCCESS)
				return FALSE;
		}
	}

	return TRUE;
}

INT32 avc420_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize,
                        BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
                        UINT32 nDstWidth, UINT32 nDstHeight,
                        RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	int status;

	if (!h264)
		return -1001;

	status = h264->subsystem->Decompress(h264, pSrcData, SrcSize);
	if (status == 0)
		return 1;

	if (status < 0)
		return status;

	if (!avc_yuv_to_rgb(h264, regionRects, numRegionRects, nDstWidth,
	                    nDstHeight, nDstStep, pDstData, DstFormat, FALSE))
		return -1002;

	return 1;
}

INT32 avc420_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
                      UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
                      BYTE** ppDstData, UINT32* pDstSize)
{
	prim_size_t roi;
	primitives_t* prims = primitives_get();

	if (!h264)
		return -1;

	if (!h264->subsystem->Compress)
		return -1;

	if (!avc420_ensure_buffer(h264, nSrcStep, nSrcWidth, nSrcHeight))
		return -1;

	roi.width = nSrcWidth;
	roi.height = nSrcHeight;

	if (prims->RGBToYUV420_8u_P3AC4R(pSrcData, SrcFormat, nSrcStep, h264->pYUVData, h264->iStride,
	                                 &roi) != PRIMITIVES_SUCCESS)
		return -1;

	return h264->subsystem->Compress(h264, h264->pYUVData, h264->iStride, ppDstData, pDstSize);
}

INT32 avc444_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
                      UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
                      BYTE version, BYTE* op, BYTE** ppDstData, UINT32* pDstSize,
                      BYTE** ppAuxDstData, UINT32* pAuxDstSize)
{
	prim_size_t roi;
	primitives_t* prims = primitives_get();
	BYTE* coded;
	UINT32 codedSize;

	if (!h264)
		return -1;

	if (!h264->subsystem->Compress)
		return -1;

	if (!avc420_ensure_buffer(h264, nSrcStep, nSrcWidth, nSrcHeight))
		return -1;

	if (!avc444_ensure_buffer(h264, nSrcHeight))
		return -1;

	roi.width = nSrcWidth;
	roi.height = nSrcHeight;

	switch (version)
	{
		case 1:
			if (prims->RGBToAVC444YUV(pSrcData, SrcFormat, nSrcStep, h264->pYUV444Data, h264->iStride,
			                          h264->pYUVData, h264->iStride, &roi) != PRIMITIVES_SUCCESS)
				return -1;

			break;

		case 2:
			if (prims->RGBToAVC444YUVv2(pSrcData, SrcFormat, nSrcStep, h264->pYUV444Data, h264->iStride,
			                            h264->pYUVData, h264->iStride, &roi) != PRIMITIVES_SUCCESS)
				return -1;

			break;

		default:
			return -1;
	}

	if (h264->subsystem->Compress(h264, h264->pYUV444Data, h264->iStride, &coded, &codedSize) < 0)
		return -1;

	memcpy(h264->lumaData, coded, codedSize);
	*ppDstData = h264->lumaData;
	*pDstSize = codedSize;

	if (h264->subsystem->Compress(h264, h264->pYUVData, h264->iStride, &coded, &codedSize) < 0)
		return -1;

	*ppAuxDstData = coded;
	*pAuxDstSize = codedSize;
	*op = 0;
	return 0;
}

static BOOL avc444_ensure_buffer(H264_CONTEXT* h264,
                                 DWORD nDstHeight)
{
	UINT32 x;
	const UINT32* piMainStride = h264->iStride;
	UINT32* piDstSize = h264->iYUV444Size;
	UINT32* piDstStride = h264->iYUV444Stride;
	BYTE** ppYUVDstData = h264->pYUV444Data;
	UINT32 padDstHeight = nDstHeight + 16; /* Need alignment to 16x16 blocks */

	if ((piMainStride[0] != piDstStride[0]) ||
	    (piDstSize[0] != piMainStride[0] * padDstHeight))
	{
		for (x = 0; x < 3; x++)
		{
			piDstStride[x] = piMainStride[0];
			piDstSize[x] = piDstStride[x] * padDstHeight;
			_aligned_free(ppYUVDstData[x]);
			ppYUVDstData[x] = _aligned_malloc(piDstSize[x], 16);

			if (!ppYUVDstData[x])
				goto fail;

			memset(ppYUVDstData[x], 0, piDstSize[x]);
		}

		_aligned_free(h264->lumaData);
		h264->lumaData = _aligned_malloc(piDstSize[0] * 4, 16);
	}

	for (x = 0; x < 3; x++)
	{
		if (!ppYUVDstData[x] || (piDstSize[x] == 0) || (piDstStride[x] == 0))
		{
			WLog_Print(h264->log, WLOG_ERROR, "YUV buffer not initialized! check your decoder settings");
			goto fail;
		}
	}

	if (!h264->lumaData)
		goto fail;

	return TRUE;
fail:
	_aligned_free(ppYUVDstData[0]);
	_aligned_free(ppYUVDstData[1]);
	_aligned_free(ppYUVDstData[2]);
	_aligned_free(h264->lumaData);
	ppYUVDstData[0] = NULL;
	ppYUVDstData[1] = NULL;
	ppYUVDstData[2] = NULL;
	h264->lumaData = NULL;
	return FALSE;
}

static BOOL avc444_process_rects(H264_CONTEXT* h264, const BYTE* pSrcData,
                                 UINT32 SrcSize, BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                 UINT32 nDstWidth, UINT32 nDstHeight,
                                 const RECTANGLE_16* rects, UINT32 nrRects,
                                 avc444_frame_type type)
{
	const primitives_t* prims = primitives_get();
	UINT32 x;
	UINT32* piDstStride = h264->iYUV444Stride;
	BYTE** ppYUVDstData = h264->pYUV444Data;
	const UINT32* piStride = h264->iStride;
	const BYTE** ppYUVData = (const BYTE**)h264->pYUVData;

	if (h264->subsystem->Decompress(h264, pSrcData, SrcSize) < 0)
		return FALSE;

	if (!avc444_ensure_buffer(h264, nDstHeight))
		return FALSE;

	for (x = 0; x < nrRects; x++)
	{
		const RECTANGLE_16* rect = &rects[x];
		const UINT32 alignedWidth = h264->width + ((h264->width % 16 != 0) ? 16 - h264->width % 16 : 0);
		const UINT32 alignedHeight = h264->height + ((h264->height % 16 != 0) ? 16 - h264->height % 16 : 0);

		if (!check_rect(h264, rect, nDstWidth, nDstHeight))
			continue;

		if (prims->YUV420CombineToYUV444(type, ppYUVData, piStride,
		                                 alignedWidth, alignedHeight,
		                                 ppYUVDstData, piDstStride,
		                                 rect) != PRIMITIVES_SUCCESS)
			return FALSE;
	}

	if (!avc_yuv_to_rgb(h264, rects, nrRects, nDstWidth,
	                    nDstHeight, nDstStep, pDstData, DstFormat, TRUE))
		return FALSE;

	return TRUE;
}

#if defined(AVC444_FRAME_STAT)
static UINT64 op1 = 0;
static double op1sum = 0;
static UINT64 op2 = 0;
static double op2sum = 0;
static UINT64 op3 = 0;
static double op3sum = 0;
static double avg(UINT64* count, double old, double size)
{
	double tmp = size + *count * old;
	(*count)++;
	tmp = tmp / *count;
	return tmp;
}
#endif

INT32 avc444_decompress(H264_CONTEXT* h264, BYTE op,
                        RECTANGLE_16* regionRects, UINT32 numRegionRects,
                        const BYTE* pSrcData, UINT32 SrcSize,
                        RECTANGLE_16* auxRegionRects, UINT32 numAuxRegionRect,
                        const BYTE* pAuxSrcData, UINT32 AuxSrcSize,
                        BYTE* pDstData, DWORD DstFormat,
                        UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
                        UINT32 codecId)
{
	INT32 status = -1;
	avc444_frame_type chroma = (codecId == RDPGFX_CODECID_AVC444) ? AVC444_CHROMAv1 : AVC444_CHROMAv2;

	if (!h264 || !regionRects ||
	    !pSrcData || !pDstData)
		return -1001;

	switch (op)
	{
		case 0: /* YUV420 in stream 1
		 * Chroma420 in stream 2 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep, nDstWidth,
			                          nDstHeight,
			                          regionRects, numRegionRects, AVC444_LUMA))
				status = -1;
			else if (!avc444_process_rects(h264, pAuxSrcData, AuxSrcSize, pDstData, DstFormat, nDstStep,
			                               nDstWidth, nDstHeight,
			                               auxRegionRects, numAuxRegionRect, chroma))
				status = -1;
			else
				status = 0;

			break;

		case 2: /* Chroma420 in stream 1 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep, nDstWidth,
			                          nDstHeight,
			                          regionRects, numRegionRects, chroma))
				status = -1;
			else
				status = 0;

			break;

		case 1: /* YUV420 in stream 1 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep, nDstWidth,
			                          nDstHeight,
			                          regionRects, numRegionRects, AVC444_LUMA))
				status = -1;
			else
				status = 0;

			break;

		default: /* WTF? */
			break;
	}

#if defined(AVC444_FRAME_STAT)

	switch (op)
	{
		case 0:
			op1sum = avg(&op1, op1sum, SrcSize + AuxSrcSize);
			break;

		case 1:
			op2sum = avg(&op2, op2sum, SrcSize);
			break;

		case 2:
			op3sum = avg(&op3, op3sum, SrcSize);
			break;

		default:
			break;
	}

	WLog_Print(h264->log, WLOG_INFO,
	           "luma=%"PRIu64" [avg=%lf] chroma=%"PRIu64" [avg=%lf] combined=%"PRIu64" [avg=%lf]",
	           op1, op1sum, op2, op2sum, op3, op3sum);
#endif
	return status;
}

#define MAX_SUBSYSTEMS 10
static INIT_ONCE subsystems_once = INIT_ONCE_STATIC_INIT;
static H264_CONTEXT_SUBSYSTEM* subSystems[MAX_SUBSYSTEMS];

#if defined(_WIN32) && defined(WITH_MEDIA_FOUNDATION)
extern H264_CONTEXT_SUBSYSTEM g_Subsystem_MF;
#endif

static BOOL CALLBACK h264_register_subsystems(PINIT_ONCE once, PVOID param, PVOID* context)
{
	int i = 0;
	ZeroMemory(subSystems, sizeof(subSystems));
#if defined(_WIN32) && defined(WITH_MEDIA_FOUNDATION)
	{
		subSystems[i] = &g_Subsystem_MF;
		i++;
	}
#endif
#ifdef WITH_OPENH264
	{
		extern H264_CONTEXT_SUBSYSTEM g_Subsystem_OpenH264;
		subSystems[i] = &g_Subsystem_OpenH264;
		i++;
	}
#endif
#ifdef WITH_FFMPEG
	{
		extern H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec;
		subSystems[i] = &g_Subsystem_libavcodec;
		i++;
	}
#endif
#ifdef WITH_X264
	{
		extern H264_CONTEXT_SUBSYSTEM g_Subsystem_x264;
		subSystems[i] = &g_Subsystem_x264;
		i++;
	}
#endif
	return i > 0;
}


BOOL h264_context_init(H264_CONTEXT* h264)
{
	int i;

	if (!h264)
		return FALSE;

	h264->log = WLog_Get(TAG);

	if (!h264->log)
		return FALSE;

	h264->subsystem = NULL;
	InitOnceExecuteOnce(&subsystems_once, h264_register_subsystems, NULL, NULL);

	for (i = 0; i < MAX_SUBSYSTEMS; i++)
	{
		H264_CONTEXT_SUBSYSTEM* subsystem = subSystems[i];

		if (!subsystem || !subsystem->Init)
			break;

		if (subsystem->Init(h264))
		{
			h264->subsystem = subsystem;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL h264_context_reset(H264_CONTEXT* h264, UINT32 width, UINT32 height)
{
	if (!h264)
		return FALSE;

	h264->width = width;
	h264->height = height;
	return TRUE;
}

H264_CONTEXT* h264_context_new(BOOL Compressor)
{
	H264_CONTEXT* h264;
	h264 = (H264_CONTEXT*) calloc(1, sizeof(H264_CONTEXT));

	if (h264)
	{
		h264->Compressor = Compressor;

		if (Compressor)
		{
			/* Default compressor settings, may be changed by caller */
			h264->BitRate = 1000000;
			h264->FrameRate = 30;
		}

		if (!h264_context_init(h264))
		{
			free(h264);
			return NULL;
		}
	}

	return h264;
}

void h264_context_free(H264_CONTEXT* h264)
{
	if (h264)
	{
		h264->subsystem->Uninit(h264);
		_aligned_free(h264->pYUV444Data[0]);
		_aligned_free(h264->pYUV444Data[1]);
		_aligned_free(h264->pYUV444Data[2]);
		_aligned_free(h264->lumaData);
		free(h264);
	}
}
