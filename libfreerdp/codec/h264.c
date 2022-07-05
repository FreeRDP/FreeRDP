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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/library.h>
#include <winpr/bitstream.h>
#include <winpr/synch.h>

#include <freerdp/primitives.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/yuv.h>
#include <freerdp/log.h>

#include "h264.h"

#define TAG FREERDP_TAG("codec")

static BOOL avc444_ensure_buffer(H264_CONTEXT* h264, DWORD nDstHeight);

BOOL avc420_ensure_buffer(H264_CONTEXT* h264, UINT32 stride, UINT32 width, UINT32 height)
{
	size_t x;
	BOOL isNull = FALSE;
	UINT32 pheight = height;

	if (!h264)
		return FALSE;

	if (stride == 0)
		stride = width;

	if (stride % 16 != 0)
		stride += 16 - stride % 16;

	if (pheight % 16 != 0)
		pheight += 16 - pheight % 16;

	for (x = 0; x < 3; x++)
	{
		if (!h264->pYUVData[x] || !h264->pOldYUVData[x])
			isNull = TRUE;
	}

	if (isNull || (width != h264->width) || (height != h264->height) ||
	    (stride != h264->iStride[0]))
	{
		h264->iStride[0] = stride;
		h264->iStride[1] = (stride + 1) / 2;
		h264->iStride[2] = (stride + 1) / 2;
		h264->width = width;
		h264->height = height;

		for (x = 0; x < 3; x++)
		{
			BYTE* tmp1 = winpr_aligned_recalloc(h264->pYUVData[x], h264->iStride[x], pheight, 16);
			BYTE* tmp2 =
			    winpr_aligned_recalloc(h264->pOldYUVData[x], h264->iStride[x], pheight, 16);
			if (tmp1)
				h264->pYUVData[x] = tmp1;
			if (tmp2)
				h264->pOldYUVData[x] = tmp2;
			if (!tmp1 || !tmp2)
				return FALSE;
		}
	}

	return TRUE;
}

INT32 avc420_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
                        DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
                        const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	int status;
	const BYTE* pYUVData[3];

	if (!h264 || h264->Compressor)
		return -1001;

	status = h264->subsystem->Decompress(h264, pSrcData, SrcSize);

	if (status == 0)
		return 1;

	if (status < 0)
		return status;

	pYUVData[0] = h264->pYUVData[0];
	pYUVData[1] = h264->pYUVData[1];
	pYUVData[2] = h264->pYUVData[2];
	if (!yuv420_context_decode(h264->yuv, pYUVData, h264->iStride, h264->height, DstFormat,
	                           pDstData, nDstStep, regionRects, numRegionRects))
		return -1002;

	return 1;
}

static BOOL allocate_h264_metablock(UINT32 QP, RECTANGLE_16* rectangles,
                                    RDPGFX_H264_METABLOCK* meta, size_t count)
{
	size_t x;

	/* [MS-RDPEGFX] 2.2.4.4.2 RDPGFX_AVC420_QUANT_QUALITY */
	if (!meta || (QP > UINT8_MAX))
		return FALSE;

	meta->regionRects = rectangles;
	if (count == 0)
		return TRUE;

	if (count > UINT32_MAX)
		return FALSE;

	meta->quantQualityVals = calloc(count, sizeof(RDPGFX_H264_QUANT_QUALITY));

	if (!meta->quantQualityVals || !meta->regionRects)
		return FALSE;
	meta->numRegionRects = (UINT32)count;
	for (x = 0; x < count; x++)
	{
		RDPGFX_H264_QUANT_QUALITY* cur = &meta->quantQualityVals[x];
		cur->qp = (UINT8)QP;

		/* qpVal bit 6 and 7 are flags, so mask them out here.
		 * qualityVal is [0-100] so 100 - qpVal [0-64] is always in range */
		cur->qualityVal = 100 - (QP & 0x3F);
	}
	return TRUE;
}

static INLINE BOOL diff_tile(const RECTANGLE_16* regionRect, BYTE* pYUVData[3],
                             BYTE* pOldYUVData[3], UINT32 const iStride[3])
{
	size_t size, y;
	if (!regionRect || !pYUVData || !pOldYUVData || !iStride)
		return FALSE;
	size = regionRect->right - regionRect->left;
	if (regionRect->right > iStride[0])
		return FALSE;
	if (regionRect->right / 2 > iStride[1])
		return FALSE;
	if (regionRect->right / 2 > iStride[2])
		return FALSE;

	for (y = regionRect->top; y < regionRect->bottom; y++)
	{
		const BYTE* cur0 = &pYUVData[0][y * iStride[0]];
		const BYTE* cur1 = &pYUVData[1][y * iStride[1]];
		const BYTE* cur2 = &pYUVData[2][y * iStride[2]];
		const BYTE* old0 = &pOldYUVData[0][y * iStride[0]];
		const BYTE* old1 = &pOldYUVData[1][y * iStride[1]];
		const BYTE* old2 = &pOldYUVData[2][y * iStride[2]];

		if (memcmp(&cur0[regionRect->left], &old0[regionRect->left], size) != 0)
			return TRUE;
		if (memcmp(&cur1[regionRect->left / 2], &old1[regionRect->left / 2], size / 2) != 0)
			return TRUE;
		if (memcmp(&cur2[regionRect->left / 2], &old2[regionRect->left / 2], size / 2) != 0)
			return TRUE;
	}
	return FALSE;
}

static BOOL detect_changes(BOOL firstFrameDone, const UINT32 QP, const RECTANGLE_16* regionRect,
                           BYTE* pYUVData[3], BYTE* pOldYUVData[3], UINT32 const iStride[3],
                           RDPGFX_H264_METABLOCK* meta)
{
	size_t count = 0, wc, hc;
	RECTANGLE_16* rectangles;

	if (!regionRect || !pYUVData || !pOldYUVData || !iStride || !meta)
		return FALSE;

	wc = (regionRect->right - regionRect->left) / 64 + 1;
	hc = (regionRect->bottom - regionRect->top) / 64 + 1;
	rectangles = calloc(wc * hc, sizeof(RECTANGLE_16));
	if (!rectangles)
		return FALSE;
	if (!firstFrameDone)
	{
		rectangles[0] = *regionRect;
		count = 1;
	}
	else
	{
		size_t x, y;
		for (y = regionRect->top; y < regionRect->bottom; y += 64)
		{
			for (x = regionRect->left; x < regionRect->right; x += 64)
			{
				RECTANGLE_16 rect;
				rect.left = (UINT16)MIN(UINT16_MAX, regionRect->left + x);
				rect.top = (UINT16)MIN(UINT16_MAX, regionRect->top + y);
				rect.right =
				    (UINT16)MIN(UINT16_MAX, MIN(regionRect->left + x + 64, regionRect->right));
				rect.bottom =
				    (UINT16)MIN(UINT16_MAX, MIN(regionRect->top + y + 64, regionRect->bottom));
				if (diff_tile(&rect, pYUVData, pOldYUVData, iStride))
					rectangles[count++] = rect;
			}
		}
	}
	if (!allocate_h264_metablock(QP, rectangles, meta, count))
		return FALSE;
	return TRUE;
}

INT32 avc420_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat, UINT32 nSrcStep,
                      UINT32 nSrcWidth, UINT32 nSrcHeight, const RECTANGLE_16* regionRect,
                      BYTE** ppDstData, UINT32* pDstSize, RDPGFX_H264_METABLOCK* meta)
{
	size_t x;
	INT32 rc = -1;
	BYTE* pYUVData[3] = { 0 };
	const BYTE* pcYUVData[3] = { 0 };
	BYTE* pOldYUVData[3] = { 0 };

	if (!h264 || !regionRect || !meta || !h264->Compressor)
		return -1;

	if (!h264->subsystem->Compress)
		return -1;

	if (!avc420_ensure_buffer(h264, nSrcStep, nSrcWidth, nSrcHeight))
		return -1;

	if (h264->encodingBuffer)
	{
		for (x = 0; x < 3; x++)
		{
			pYUVData[x] = h264->pYUVData[x];
			pOldYUVData[x] = h264->pOldYUVData[x];
		}
	}
	else
	{
		for (x = 0; x < 3; x++)
		{
			pYUVData[x] = h264->pOldYUVData[x];
			pOldYUVData[x] = h264->pYUVData[x];
		}
	}
	h264->encodingBuffer = !h264->encodingBuffer;

	if (!yuv420_context_encode(h264->yuv, pSrcData, nSrcStep, SrcFormat, h264->iStride, pYUVData,
	                           regionRect, 1))
		goto fail;

	if (!detect_changes(h264->firstLumaFrameDone, h264->QP, regionRect, pYUVData, pOldYUVData,
	                    h264->iStride, meta))
		goto fail;

	if (meta->numRegionRects == 0)
	{
		rc = 0;
		goto fail;
	}

	for (x = 0; x < 3; x++)
		pcYUVData[x] = pYUVData[x];

	rc = h264->subsystem->Compress(h264, pcYUVData, h264->iStride, ppDstData, pDstSize);
	if (rc >= 0)
		h264->firstLumaFrameDone = TRUE;

fail:
	if (rc < 0)
		free_h264_metablock(meta);
	return rc;
}

INT32 avc444_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat, UINT32 nSrcStep,
                      UINT32 nSrcWidth, UINT32 nSrcHeight, BYTE version, const RECTANGLE_16* region,
                      BYTE* op, BYTE** ppDstData, UINT32* pDstSize, BYTE** ppAuxDstData,
                      UINT32* pAuxDstSize, RDPGFX_H264_METABLOCK* meta,
                      RDPGFX_H264_METABLOCK* auxMeta)
{
	int rc = -1;
	BYTE* coded;
	UINT32 codedSize;
	BYTE** pYUV444Data;
	BYTE** pOldYUV444Data;
	BYTE** pYUVData;
	BYTE** pOldYUVData;

	if (!h264 || !h264->Compressor)
		return -1;

	if (!h264->subsystem->Compress)
		return -1;

	if (!avc420_ensure_buffer(h264, nSrcStep, nSrcWidth, nSrcHeight))
		return -1;

	if (!avc444_ensure_buffer(h264, nSrcHeight))
		return -1;

	if (h264->encodingBuffer)
	{
		pYUV444Data = h264->pOldYUV444Data;
		pOldYUV444Data = h264->pYUV444Data;
		pYUVData = h264->pOldYUVData;
		pOldYUVData = h264->pYUVData;
	}
	else
	{
		pYUV444Data = h264->pYUV444Data;
		pOldYUV444Data = h264->pOldYUV444Data;
		pYUVData = h264->pYUVData;
		pOldYUVData = h264->pOldYUVData;
	}
	h264->encodingBuffer = !h264->encodingBuffer;

	if (!yuv444_context_encode(h264->yuv, version, pSrcData, nSrcStep, SrcFormat, h264->iStride,
	                           pYUV444Data, pYUVData, region, 1))
		goto fail;

	if (!detect_changes(h264->firstLumaFrameDone, h264->QP, region, pYUV444Data, pOldYUV444Data,
	                    h264->iStride, meta))
		goto fail;
	if (!detect_changes(h264->firstChromaFrameDone, h264->QP, region, pYUVData, pOldYUVData,
	                    h264->iStride, auxMeta))
		goto fail;

	/* [MS-RDPEGFX] 2.2.4.5 RFX_AVC444_BITMAP_STREAM
	 * LC:
	 * 0 ... Luma & Chroma
	 * 1 ... Luma
	 * 2 ... Chroma
	 */
	if ((meta->numRegionRects > 0) && (auxMeta->numRegionRects > 0))
		*op = 0;
	else if (meta->numRegionRects > 0)
		*op = 1;
	else if (auxMeta->numRegionRects > 0)
		*op = 2;
	else
	{
		WLog_INFO(TAG, "no changes detected for luma or chroma frame");
		rc = 0;
		goto fail;
	}

	if ((*op == 0) || (*op == 1))
	{
		const BYTE* pcYUV444Data[3] = { pYUV444Data[0], pYUV444Data[1], pYUV444Data[2] };

		if (h264->subsystem->Compress(h264, pcYUV444Data, h264->iStride, &coded, &codedSize) < 0)
			goto fail;
		h264->firstLumaFrameDone = TRUE;
		memcpy(h264->lumaData, coded, codedSize);
		*ppDstData = h264->lumaData;
		*pDstSize = codedSize;
	}

	if ((*op == 0) || (*op == 2))
	{
		const BYTE* pcYUVData[3] = { pYUVData[0], pYUVData[1], pYUVData[2] };

		if (h264->subsystem->Compress(h264, pcYUVData, h264->iStride, &coded, &codedSize) < 0)
			goto fail;
		h264->firstChromaFrameDone = TRUE;
		*ppAuxDstData = coded;
		*pAuxDstSize = codedSize;
	}

	rc = 1;
fail:
	if (rc < 0)
	{
		free_h264_metablock(meta);
		free_h264_metablock(auxMeta);
	}
	return rc;
}

static BOOL avc444_ensure_buffer(H264_CONTEXT* h264, DWORD nDstHeight)
{
	UINT32 x;
	const UINT32* piMainStride = h264->iStride;
	UINT32* piDstSize = h264->iYUV444Size;
	UINT32* piDstStride = h264->iYUV444Stride;
	BYTE** ppYUVDstData = h264->pYUV444Data;
	BYTE** ppOldYUVDstData = h264->pOldYUV444Data;
	const UINT32 pad = nDstHeight % 16;
	UINT32 padDstHeight = nDstHeight; /* Need alignment to 16x16 blocks */

	if (pad != 0)
		padDstHeight += 16 - pad;

	if ((piMainStride[0] != piDstStride[0]) || (piDstSize[0] != piMainStride[0] * padDstHeight))
	{
		for (x = 0; x < 3; x++)
		{
			BYTE* tmp1;
			BYTE* tmp2;
			piDstStride[x] = piMainStride[0];
			piDstSize[x] = piDstStride[x] * padDstHeight;
			tmp1 = winpr_aligned_recalloc(ppYUVDstData[x], piDstSize[x], 1, 16);
			if (tmp1)
				ppYUVDstData[x] = tmp1;
			tmp2 = winpr_aligned_recalloc(ppOldYUVDstData[x], piDstSize[x], 1, 16);
			if (tmp2)
				ppOldYUVDstData[x] = tmp2;
			if (!tmp1 || !tmp2)
				goto fail;
		}

		{
			BYTE* tmp = winpr_aligned_recalloc(h264->lumaData, piDstSize[0], 4, 16);
			if (!tmp)
				goto fail;
			h264->lumaData = tmp;
		}
	}

	for (x = 0; x < 3; x++)
	{
		if (!ppOldYUVDstData[x] || !ppYUVDstData[x] || (piDstSize[x] == 0) || (piDstStride[x] == 0))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "YUV buffer not initialized! check your decoder settings");
			goto fail;
		}
	}

	if (!h264->lumaData)
		goto fail;

	return TRUE;
fail:
	return FALSE;
}

static BOOL avc444_process_rects(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize,
                                 BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                 UINT32 nDstWidth, UINT32 nDstHeight, const RECTANGLE_16* rects,
                                 UINT32 nrRects, avc444_frame_type type)
{
	const BYTE* pYUVData[3];
	BYTE* pYUVDstData[3];
	UINT32* piDstStride = h264->iYUV444Stride;
	BYTE** ppYUVDstData = h264->pYUV444Data;
	const UINT32* piStride = h264->iStride;

	if (h264->subsystem->Decompress(h264, pSrcData, SrcSize) < 0)
		return FALSE;

	pYUVData[0] = h264->pYUVData[0];
	pYUVData[1] = h264->pYUVData[1];
	pYUVData[2] = h264->pYUVData[2];
	if (!avc444_ensure_buffer(h264, nDstHeight))
		return FALSE;

	pYUVDstData[0] = ppYUVDstData[0];
	pYUVDstData[1] = ppYUVDstData[1];
	pYUVDstData[2] = ppYUVDstData[2];
	if (!yuv444_context_decode(h264->yuv, (BYTE)type, pYUVData, piStride, h264->height, pYUVDstData,
	                           piDstStride, DstFormat, pDstData, nDstStep, rects, nrRects))
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

INT32 avc444_decompress(H264_CONTEXT* h264, BYTE op, const RECTANGLE_16* regionRects,
                        UINT32 numRegionRects, const BYTE* pSrcData, UINT32 SrcSize,
                        const RECTANGLE_16* auxRegionRects, UINT32 numAuxRegionRect,
                        const BYTE* pAuxSrcData, UINT32 AuxSrcSize, BYTE* pDstData, DWORD DstFormat,
                        UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight, UINT32 codecId)
{
	INT32 status = -1;
	avc444_frame_type chroma =
	    (codecId == RDPGFX_CODECID_AVC444) ? AVC444_CHROMAv1 : AVC444_CHROMAv2;

	if (!h264 || !regionRects || !pSrcData || !pDstData || h264->Compressor)
		return -1001;

	switch (op)
	{
		case 0: /* YUV420 in stream 1
		         * Chroma420 in stream 2 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep,
			                          nDstWidth, nDstHeight, regionRects, numRegionRects,
			                          AVC444_LUMA))
				status = -1;
			else if (!avc444_process_rects(h264, pAuxSrcData, AuxSrcSize, pDstData, DstFormat,
			                               nDstStep, nDstWidth, nDstHeight, auxRegionRects,
			                               numAuxRegionRect, chroma))
				status = -1;
			else
				status = 0;

			break;

		case 2: /* Chroma420 in stream 1 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep,
			                          nDstWidth, nDstHeight, regionRects, numRegionRects, chroma))
				status = -1;
			else
				status = 0;

			break;

		case 1: /* YUV420 in stream 1 */
			if (!avc444_process_rects(h264, pSrcData, SrcSize, pDstData, DstFormat, nDstStep,
			                          nDstWidth, nDstHeight, regionRects, numRegionRects,
			                          AVC444_LUMA))
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
	           "luma=%" PRIu64 " [avg=%lf] chroma=%" PRIu64 " [avg=%lf] combined=%" PRIu64
	           " [avg=%lf]",
	           op1, op1sum, op2, op2sum, op3, op3sum);
#endif
	return status;
}

#define MAX_SUBSYSTEMS 10
static INIT_ONCE subsystems_once = INIT_ONCE_STATIC_INIT;
static const H264_CONTEXT_SUBSYSTEM* subSystems[MAX_SUBSYSTEMS] = { 0 };

static BOOL CALLBACK h264_register_subsystems(PINIT_ONCE once, PVOID param, PVOID* context)
{
	int i = 0;

#ifdef WITH_MEDIACODEC
	{
		subSystems[i] = &g_Subsystem_mediacodec;
		i++;
	}
#endif
#if defined(_WIN32) && defined(WITH_MEDIA_FOUNDATION)
	{
		subSystems[i] = &g_Subsystem_MF;
		i++;
	}
#endif
#ifdef WITH_OPENH264
	{
		subSystems[i] = &g_Subsystem_OpenH264;
		i++;
	}
#endif
#ifdef WITH_VIDEO_FFMPEG
	{
		subSystems[i] = &g_Subsystem_libavcodec;
		i++;
	}
#endif
	return i > 0;
}

static BOOL h264_context_init(H264_CONTEXT* h264)
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
		const H264_CONTEXT_SUBSYSTEM* subsystem = subSystems[i];

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
	return yuv_context_reset(h264->yuv, width, height);
}

H264_CONTEXT* h264_context_new(BOOL Compressor)
{
	H264_CONTEXT* h264 = (H264_CONTEXT*)calloc(1, sizeof(H264_CONTEXT));
	if (!h264)
		return NULL;

	h264->Compressor = Compressor;
	if (Compressor)

	{
		/* Default compressor settings, may be changed by caller */
		h264->BitRate = 1000000;
		h264->FrameRate = 30;
	}

	if (!h264_context_init(h264))
		goto fail;

	h264->yuv = yuv_context_new(Compressor, 0);
	if (!h264->yuv)
		goto fail;

	return h264;

fail:
	h264_context_free(h264);
	return NULL;
}

void h264_context_free(H264_CONTEXT* h264)
{
	if (h264)
	{
		size_t x;
		if (h264->subsystem)
			h264->subsystem->Uninit(h264);

		for (x = 0; x < 3; x++)
		{
			if (h264->Compressor)
			{
				winpr_aligned_free(h264->pYUVData[x]);
				winpr_aligned_free(h264->pOldYUVData[x]);
			}
			winpr_aligned_free(h264->pYUV444Data[x]);
			winpr_aligned_free(h264->pOldYUV444Data[x]);
		}
		winpr_aligned_free(h264->lumaData);

		yuv_context_free(h264->yuv);
		free(h264);
	}
}
