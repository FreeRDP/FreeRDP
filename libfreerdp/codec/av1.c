/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV1 Bitmap Compression
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <freerdp/codec/av1.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/region.h>

#define TAG FREERDP_TAG("codec.av1")

#if defined(WITH_LIBAOM)
#include <aom/aom.h>

#include <aom/aom_decoder.h>
#include <aom/aom_encoder.h>

#include <aom/aom_image.h>
#include <aom/aomcx.h>
#include <aom/aomdx.h>
#endif

#if defined(WITH_DAV1D)
#include <dav1d/dav1d.h>
#endif

#if defined(WITH_LIBAOM) || defined(WITH_DAV1D)

#if defined(WITH_LIBYUV)
#include <libyuv.h>
#endif

struct S_FREERDP_AV1_CONTEXT
{
	wLog* log;
	bool encoder;
	bool initialized;
#if defined(WITH_LIBAOM)
	aom_codec_ctx_t ctx;
	aom_codec_enc_cfg_t ecfg;
	aom_codec_pts_t framecount;
	aom_enc_frame_flags_t eflags;
	aom_codec_flags_t flags;
	aom_codec_iface_t* iface;
#endif
#if defined(WITH_LIBAOM) && !defined(WITH_DAV1D)
	aom_codec_dec_cfg_t dcfg;
#endif
#if defined(WITH_DAV1D)
	Dav1dContext* dav1d;
#endif
	BYTE* yuvdata[3];
	UINT32 yuvWidth;
	UINT32 yuvStride[3];
	UINT32 yuvHeight;
};

#if defined(WITH_LIBAOM)
static void av1_aom_destroy(FREERDP_AV1_CONTEXT* av1)
{
	const aom_codec_err_t rc = aom_codec_destroy(&av1->ctx);
	if (rc != AOM_CODEC_OK)
		WLog_Print(av1->log, WLOG_WARN, "aom_codec_destroy: %s", aom_codec_err_to_string(rc));
}

WINPR_ATTR_NODISCARD
static BOOL allocate_h264_metablock(UINT32 QP, RECTANGLE_16* rectangles,
                                    RDPGFX_H264_METABLOCK* meta, size_t count)
{
	/* [MS-RDPEGFX] 2.2.4.4.2 RDPGFX_AVC420_QUANT_QUALITY */
	if (!meta || (QP > UINT8_MAX))
	{
		free(rectangles);
		return FALSE;
	}

	meta->regionRects = rectangles;
	if (count == 0)
		return TRUE;

	if (count > UINT32_MAX)
		return FALSE;

	meta->quantQualityVals = calloc(count, sizeof(RDPGFX_H264_QUANT_QUALITY));

	if (!meta->quantQualityVals || !meta->regionRects)
		return FALSE;
	meta->numRegionRects = (UINT32)count;
	for (size_t x = 0; x < count; x++)
	{
		RDPGFX_H264_QUANT_QUALITY* cur = &meta->quantQualityVals[x];
		cur->qp = (UINT8)QP;

		/* qpVal bit 6 and 7 are flags, so mask them out here.
		 * qualityVal is [0-100] so 100 - qpVal [0-64] is always in range */
		cur->qualityVal = 100 - (QP & 0x3F);
	}
	return TRUE;
}
#endif

BOOL freerdp_av1_context_set_option(FREERDP_AV1_CONTEXT* av1, FREERDP_AV1_CONTEXT_OPTION option,
                                    UINT32 value)
{
#if defined(WITH_LIBAOM)
	switch (option)
	{
		case FREERDP_AV1_CONTEXT_OPTION_PROFILE:
			if (!av1->encoder)
				return FALSE;

			av1->ecfg.g_profile = value;
			break;

		case FREERDP_AV1_CONTEXT_OPTION_RATECONTROL:
			if (!av1->encoder)
				return FALSE;
			switch (value)
			{
				case FREERDP_AV1_VBR:
				case FREERDP_AV1_CBR:
				case FREERDP_AV1_CQ:
				case FREERDP_AV1_Q:
					break;
				default:
					WLog_Print(av1->log, WLOG_WARN,
					           "Unknown FREERDP_AV1_CONTEXT_OPTION_RATECONTROL value [0x%08" PRIx32
					           "]",
					           value);
					return FALSE;
			}
			av1->ecfg.rc_end_usage = value;
			break;
		case FREERDP_AV1_CONTEXT_OPTION_BITRATE:
			if (!av1->encoder)
				return FALSE;
			av1->ecfg.rc_target_bitrate = value;
			break;
		case FREERDP_AV1_CONTEXT_OPTION_USAGETYPE:
			if (!av1->encoder)
				return FALSE;
			av1->ecfg.g_usage = value;
			break;
		default:
			WLog_Print(av1->log, WLOG_ERROR, "Unknown FREERDP_AV1_CONTEXT_OPTION[0x%08" PRIx32 "]",
			           option);
			return TRUE;
	}
	return freerdp_av1_context_reset(av1, av1->ecfg.g_w, av1->ecfg.g_h);
#else
	WLog_Print(av1->log, WLOG_WARN, "AV1 encoder requires libaom, recompile with '-DWITH_AOM=ON'");
	WINPR_UNUSED(option);
	WINPR_UNUSED(value);
	return FALSE;
#endif
}

UINT32 freerdp_av1_context_get_option(FREERDP_AV1_CONTEXT* av1, FREERDP_AV1_CONTEXT_OPTION option)
{
#if defined(WITH_LIBAOM)
	switch (option)
	{
		case FREERDP_AV1_CONTEXT_OPTION_PROFILE:
			if (!av1->encoder)
				return 0;
			return av1->ecfg.g_profile;
		case FREERDP_AV1_CONTEXT_OPTION_RATECONTROL:
			if (!av1->encoder)
				return 0;
			return av1->ecfg.rc_end_usage;
		case FREERDP_AV1_CONTEXT_OPTION_BITRATE:
			if (!av1->encoder)
				return 0;
			return av1->ecfg.rc_target_bitrate;
		case FREERDP_AV1_CONTEXT_OPTION_USAGETYPE:
			if (!av1->encoder)
				return 0;
			return av1->ecfg.g_usage;
		default:
			WLog_Print(av1->log, WLOG_ERROR, "Unknown FREERDP_AV1_CONTEXT_OPTION[0x%08" PRIx32 "]",
			           option);
			return 0;
	}
#else
	WINPR_UNUSED(option);
	return 0;
#endif
}

INT32 freerdp_av1_compress(FREERDP_AV1_CONTEXT* av1, const BYTE* pSrcData, DWORD SrcFormat,
                           UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
                           const RECTANGLE_16* regionRect, BYTE** ppDstData, UINT32* pDstSize,
                           RDPGFX_H264_METABLOCK* meta)
{
	*ppDstData = nullptr;
	*pDstSize = 0;

	if (!av1->encoder)
	{
		WLog_Print(av1->log, WLOG_ERROR, "av1->encoder: %d", av1->encoder);
		return -1;
	}

#if defined(WITH_LIBAOM)
	primitives_t* primitives = primitives_get();
	if (!primitives)
	{
		WLog_Print(av1->log, WLOG_ERROR, "primitives_get(): nullptr");
		return -1;
	}

	if (av1->yuvWidth != nSrcWidth)
	{
		WLog_Print(av1->log, WLOG_ERROR, "av1->yuvWidth[%" PRIu32 "] != nSrcWidth[%" PRIu32 "]",
		           av1->yuvWidth, nSrcWidth);
		return -1;
	}

	if (av1->yuvHeight != nSrcHeight)
	{
		WLog_Print(av1->log, WLOG_ERROR, "av1->yuvHeight[%" PRIu32 "] != nSrcHeight[%" PRIu32 "]",
		           av1->yuvHeight, nSrcHeight);
		return -1;
	}

	const prim_size_t roi = { .width = nSrcWidth, .height = nSrcHeight };

	aom_image_t buffer = WINPR_C_ARRAY_INIT;

	aom_image_t* img = &buffer;
	pstatus_t rec = -1;
#if defined(WITH_LIBYUV)
	/* ARGBToI420()/ARGBToI444() always read pSrcData as packed BGRA8888 regardless of
	 * SrcFormat, so the fast path only applies for that layout; anything else must go
	 * through the format-aware primitives conversion below. */
	if ((SrcFormat == PIXEL_FORMAT_BGRA32) || (SrcFormat == PIXEL_FORMAT_BGRX32))
	{
		switch (av1->ecfg.g_profile)
		{
			case 0:
				img->fmt = AOM_IMG_FMT_I420;
				img->x_chroma_shift = 1;
				img->y_chroma_shift = 1;
				rec = ARGBToI420(pSrcData, nSrcStep, av1->yuvdata[0], av1->yuvStride[0],
				                 av1->yuvdata[1], av1->yuvStride[1], av1->yuvdata[2],
				                 WINPR_ASSERTING_INT_CAST(int, av1->yuvStride[2]),
				                 WINPR_ASSERTING_INT_CAST(int, roi.width),
				                 WINPR_ASSERTING_INT_CAST(int, roi.height));
				break;
			case 1:
				img->fmt = AOM_IMG_FMT_I444;
				rec = ARGBToI444(pSrcData, nSrcStep, av1->yuvdata[0], av1->yuvStride[0],
				                 av1->yuvdata[1], av1->yuvStride[1], av1->yuvdata[2],
				                 WINPR_ASSERTING_INT_CAST(int, av1->yuvStride[2]),
				                 WINPR_ASSERTING_INT_CAST(int, roi.width),
				                 WINPR_ASSERTING_INT_CAST(int, roi.height));
				break;
			default:
				WLog_Print(av1->log, WLOG_ERROR, "Unsupoorted AV1 profile %" PRIu32,
				           av1->ecfg.g_profile);
				return -1;
		}
	}
	else
#endif
	{
		switch (av1->ecfg.g_profile)
		{
			case 0:
				img->fmt = AOM_IMG_FMT_I420;
				img->x_chroma_shift = 1;
				img->y_chroma_shift = 1;
				rec = primitives->RGBToYUV420_8u_P3AC4R(pSrcData, SrcFormat, nSrcStep, av1->yuvdata,
				                                        av1->yuvStride, &roi);
				break;
			case 1:
				img->fmt = AOM_IMG_FMT_I444;
				rec = primitives->RGBToI444_8u(pSrcData, SrcFormat, nSrcStep, av1->yuvdata,
				                               av1->yuvStride, &roi);
				break;
			default:
				WLog_Print(av1->log, WLOG_ERROR, "Unsupoorted AV1 profile %" PRIu32,
				           av1->ecfg.g_profile);
				return -1;
		}
	}
	if (rec != 0)
	{
		WLog_Print(av1->log, WLOG_ERROR, "RGB to YUV conversion failed: %d", rec);
		return -1;
	}
	img->bit_depth = 8;
	img->d_w = img->r_w = img->w = roi.width;
	img->d_h = img->r_h = img->h = roi.height;
	img->stride[0] = WINPR_ASSERTING_INT_CAST(int, av1->yuvStride[0]);
	img->stride[1] = WINPR_ASSERTING_INT_CAST(int, av1->yuvStride[1]);
	img->stride[2] = WINPR_ASSERTING_INT_CAST(int, av1->yuvStride[2]);
	img->planes[0] = av1->yuvdata[0];
	img->planes[1] = av1->yuvdata[1];
	img->planes[2] = av1->yuvdata[2];

	const aom_codec_err_t rc = aom_codec_encode(&av1->ctx, img, ++av1->framecount, 1, av1->eflags);
	aom_img_free(img);

	if (rc != AOM_CODEC_OK)
	{
		WLog_Print(av1->log, WLOG_WARN, "aom_codec_encode: %s", aom_codec_err_to_string(rc));
		return -1;
	}

	aom_codec_iter_t iter = nullptr;
	const aom_codec_cx_pkt_t* pkt = nullptr;
	while ((pkt = aom_codec_get_cx_data(&av1->ctx, &iter)) != nullptr)
	{
		if (pkt->kind != AOM_CODEC_CX_FRAME_PKT)
			continue;

		*ppDstData = pkt->data.frame.buf;
		*pDstSize = pkt->data.frame.sz;
	};
	if (*pDstSize == 0)
		return 0;

	RECTANGLE_16* rect = calloc(1, sizeof(RECTANGLE_16));
	if (rect)
	{
		rect->right = nSrcWidth;
		rect->bottom = nSrcHeight;
	}
	return allocate_h264_metablock(10, rect, meta, 1);
#else
	WLog_Print(av1->log, WLOG_ERROR, "AV1 encoder requires libaom, recompile with '-DWITH_AOM=ON'");
	WINPR_UNUSED(pSrcData);
	WINPR_UNUSED(SrcFormat);
	WINPR_UNUSED(nSrcStep);
	WINPR_UNUSED(nSrcWidth);
	WINPR_UNUSED(nSrcHeight);
	WINPR_UNUSED(regionRect);
	WINPR_UNUSED(meta);
	return -1;
#endif
}

static BOOL isRectValid(UINT32 width, UINT32 height, const RECTANGLE_16* rect)
{
	WINPR_ASSERT(rect);
	if (rect->left > width)
		return FALSE;
	if (rect->right > width)
		return FALSE;
	if (rect->left >= rect->right)
		return FALSE;
	if (rect->top > height)
		return FALSE;
	if (rect->bottom > height)
		return FALSE;
	if (rect->top >= rect->bottom)
		return FALSE;
	return TRUE;
}

static BOOL areRectsValid(wLog* log, UINT32 width, UINT32 height, const RECTANGLE_16* rects,
                          UINT32 count)
{
	WINPR_ASSERT(rects || (count == 0));
	for (size_t x = 0; x < count; x++)
	{
		const RECTANGLE_16* rect = &rects[x];
		if (!isRectValid(width, height, rect))
		{
			char buffer[64] = WINPR_C_ARRAY_INIT;
			WLog_Print(log, WLOG_WARN,
			           "Rectangle %" PRIuz " %s outside of bounding frame %" PRIu32 "x%" PRIu32, x,
			           rectangle_to_string(rect, buffer, sizeof(buffer)), width, height);
			return FALSE;
		}
	}
	return TRUE;
}

#if defined(WITH_DAV1D)
static void av1_dav1d_data_free(WINPR_ATTR_UNUSED const uint8_t* buf,
                                WINPR_ATTR_UNUSED void* cookie)
{
	/* pSrcData is owned by the caller of freerdp_av1_decompress(), dav1d must not free it. */
}

static INT32 av1_dav1d_convert(FREERDP_AV1_CONTEXT* av1, const Dav1dPicture* pic, BYTE* pDstData,
                               DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
                               UINT32 nDstHeight, const RECTANGLE_16* regionRects,
                               UINT32 numRegionRect)
{
	if (pic->p.bpc != 8)
	{
		WLog_Print(av1->log, WLOG_ERROR, "dav1d picture bpc %d not supported", pic->p.bpc);
		return -1;
	}

	/* The decoded frame may be smaller than the destination surface, so
	 * bound the converted region to it. Otherwise the colour conversion
	 * reads past the decoded image planes. */
	const UINT32 dw = (UINT32)pic->p.w;
	const UINT32 dh = (UINT32)pic->p.h;
	const UINT32 nWidth = (dw < nDstWidth) ? dw : nDstWidth;
	const UINT32 nHeight = (dh < nDstHeight) ? dh : nDstHeight;
	const prim_size_t roi = { .width = nWidth, .height = nHeight };

	if (!areRectsValid(av1->log, roi.width, roi.height, regionRects, numRegionRect))
		return -2;

	const UINT32 strides[] = { (UINT32)pic->stride[0], (UINT32)pic->stride[1],
		                       (UINT32)pic->stride[1] };

	pstatus_t rec = -1;
#if defined(WITH_LIBYUV)
	/* J420ToARGB()/J444ToARGB() always write pDstData as packed BGRA8888 regardless of
	 * DstFormat, so the fast path only applies for that layout; anything else must go
	 * through the format-aware primitives conversion below. */
	if ((DstFormat == PIXEL_FORMAT_BGRA32) || (DstFormat == PIXEL_FORMAT_BGRX32))
	{
		const uint8_t* y = pic->data[0];
		const uint8_t* u = pic->data[1];
		const uint8_t* v = pic->data[2];

		const int stride0 = WINPR_ASSERTING_INT_CAST(int, strides[0]);
		const int stride1 = WINPR_ASSERTING_INT_CAST(int, strides[1]);
		const int stride2 = WINPR_ASSERTING_INT_CAST(int, strides[2]);
		const int iWidth = WINPR_ASSERTING_INT_CAST(int, nWidth);
		const int iHeight = WINPR_ASSERTING_INT_CAST(int, nHeight);
		const int iDstStep = WINPR_ASSERTING_INT_CAST(int, nDstStep);

		switch (pic->p.layout)
		{
			case DAV1D_PIXEL_LAYOUT_I420:
				rec = J420ToARGB(y, stride0, u, stride1, v, stride2, pDstData, iDstStep, iWidth,
				                 iHeight);
				break;
			case DAV1D_PIXEL_LAYOUT_I444:
				rec = J444ToARGB(y, stride0, u, stride1, v, stride2, pDstData, iDstStep, iWidth,
				                 iHeight);
				break;
			default:
				WLog_Print(av1->log, WLOG_ERROR, "dav1d picture layout %d not supported",
				           pic->p.layout);
				return -1;
		}
	}
	else
#endif
	{
		primitives_t* primitives = primitives_get();
		if (!primitives)
		{
			WLog_Print(av1->log, WLOG_ERROR, "primitives_get(): nullptr");
			return -1;
		}

		const BYTE* pSrc[] = { pic->data[0], pic->data[1], pic->data[2] };
		switch (pic->p.layout)
		{
			case DAV1D_PIXEL_LAYOUT_I420:
				rec = primitives->YUV420ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep,
				                                        DstFormat, &roi);
				break;
			case DAV1D_PIXEL_LAYOUT_I444:
				rec = primitives->YUV444ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep,
				                                        DstFormat, &roi);
				break;
			default:
				WLog_Print(av1->log, WLOG_ERROR, "dav1d picture layout %d not supported",
				           pic->p.layout);
				return -1;
		}
	}
	if (rec != 0)
	{
		WLog_Print(av1->log, WLOG_ERROR, "RGB to YUV conversion failed: %d", rec);
		return -1;
	}
	return TRUE;
}

/* Return: TRUE if a picture was converted, FALSE if none was ready (EAGAIN), <0 on error. */
static INT32 av1_dav1d_drain_one(FREERDP_AV1_CONTEXT* av1, BYTE* pDstData, DWORD DstFormat,
                                 UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
                                 const RECTANGLE_16* regionRects, UINT32 numRegionRect)
{
	Dav1dPicture pic = WINPR_C_ARRAY_INIT;
	const int prc = dav1d_get_picture(av1->dav1d, &pic);
	if (prc == DAV1D_ERR(EAGAIN))
		return FALSE;
	if (prc < 0)
	{
		WLog_Print(av1->log, WLOG_WARN, "dav1d_get_picture: %d", prc);
		return -1;
	}

	const INT32 rc = av1_dav1d_convert(av1, &pic, pDstData, DstFormat, nDstStep, nDstWidth,
	                                   nDstHeight, regionRects, numRegionRect);
	dav1d_picture_unref(&pic);
	return (rc == TRUE) ? TRUE : rc;
}
#endif

INT32 freerdp_av1_decompress(FREERDP_AV1_CONTEXT* av1, const BYTE* pSrcData, UINT32 SrcSize,
                             BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
                             UINT32 nDstHeight, const RECTANGLE_16* regionRects,
                             UINT32 numRegionRect)
{
	WINPR_ASSERT(av1);
	if (av1->encoder)
	{
		WLog_Print(av1->log, WLOG_ERROR, "av1->encoder: %d", av1->encoder);
		return -1;
	}

	if (!areRectsValid(av1->log, nDstWidth, nDstHeight, regionRects, numRegionRect))
		return -2;

#if defined(WITH_DAV1D)
	Dav1dData data = WINPR_C_ARRAY_INIT;
	const int wrc = dav1d_data_wrap(&data, pSrcData, SrcSize, av1_dav1d_data_free, nullptr);
	if (wrc < 0)
	{
		WLog_Print(av1->log, WLOG_WARN, "dav1d_data_wrap: %d", wrc);
		return -1;
	}

	while (data.sz > 0)
	{
		const int src = dav1d_send_data(av1->dav1d, &data);
		if ((src < 0) && (src != DAV1D_ERR(EAGAIN)))
		{
			WLog_Print(av1->log, WLOG_WARN, "dav1d_send_data: %d", src);
			dav1d_data_unref(&data);
			return -1;
		}

		const INT32 rc = av1_dav1d_drain_one(av1, pDstData, DstFormat, nDstStep, nDstWidth,
		                                     nDstHeight, regionRects, numRegionRect);
		if ((rc != TRUE) && (rc != FALSE))
		{
			dav1d_data_unref(&data);
			return rc;
		}
	}

	/* Drain any picture the decoder buffered internally before it consumed the last input. */
	for (;;)
	{
		const INT32 rc = av1_dav1d_drain_one(av1, pDstData, DstFormat, nDstStep, nDstWidth,
		                                     nDstHeight, regionRects, numRegionRect);
		if (rc == FALSE)
			break;
		if (rc != TRUE)
			return rc;
	}

	return TRUE;
#elif defined(WITH_LIBAOM)
	const aom_codec_err_t rc = aom_codec_decode(&av1->ctx, pSrcData, SrcSize, nullptr);
	if (rc != AOM_CODEC_OK)
	{
		WLog_Print(av1->log, WLOG_WARN, "aom_codec_decode: %s", aom_codec_err_to_string(rc));
		return -1;
	}

	aom_image_t* img = nullptr;
	aom_codec_iter_t iter = nullptr;
	while ((img = aom_codec_get_frame(&av1->ctx, &iter)) != nullptr)
	{
		/* The decoded frame may be smaller than the destination surface, so
		 * bound the converted region to it. Otherwise the colour conversion
		 * reads past the decoded image planes. */
		const UINT32 nWidth = (img->d_w < nDstWidth) ? img->d_w : nDstWidth;
		const UINT32 nHeight = (img->d_h < nDstHeight) ? img->d_h : nDstHeight;
		const prim_size_t roi = { .width = nWidth, .height = nHeight };

		if (!areRectsValid(av1->log, roi.width, roi.height, regionRects, numRegionRect))
			return -2;

		pstatus_t rec = -1;
#if defined(WITH_LIBYUV)
		/* J420ToARGB()/J444ToARGB() always write pDstData as packed BGRA8888 regardless of
		 * DstFormat, so the fast path only applies for that layout; anything else must go
		 * through the format-aware primitives conversion below. */
		if ((DstFormat == PIXEL_FORMAT_BGRA32) || (DstFormat == PIXEL_FORMAT_BGRX32))
		{
			switch (img->fmt)
			{
				case AOM_IMG_FMT_I420:
					rec = J420ToARGB(img->planes[0], img->stride[0], img->planes[1], img->stride[1],
					                 img->planes[2], img->stride[2], pDstData, nDstStep, nWidth,
					                 nHeight);
					break;
				case AOM_IMG_FMT_I444:
					rec = J444ToARGB(img->planes[0], img->stride[0], img->planes[1], img->stride[1],
					                 img->planes[2], img->stride[2], pDstData, nDstStep, nWidth,
					                 nHeight);
					break;
				default:
					WLog_Print(av1->log, WLOG_ERROR, "img->fmt %d not supported", img->fmt);
					return -1;
			}
		}
		else
#endif
		{
			const BYTE* pSrc[] = { img->planes[0], img->planes[1], img->planes[2] };
			const UINT32 strides[] = { img->stride[0], img->stride[1], img->stride[2] };

			primitives_t* primitives = primitives_get();
			if (!primitives)
			{
				WLog_Print(av1->log, WLOG_ERROR, "primitives_get(): nullptr");
				return FALSE;
			}

			switch (img->fmt)
			{
				case AOM_IMG_FMT_I420:
					rec = primitives->YUV420ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep,
					                                        DstFormat, &roi);
					break;
				case AOM_IMG_FMT_I444:
					rec = primitives->YUV444ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep,
					                                        DstFormat, &roi);
					break;
				default:
					WLog_Print(av1->log, WLOG_ERROR, "img->fmt %d not supported", img->fmt);
					return -1;
			}
		}
		if (rec != 0)
		{
			WLog_Print(av1->log, WLOG_ERROR, "RGB to YUV conversion failed: %d", rec);
			return -1;
		}
	}

	return TRUE;
#endif
}

BOOL freerdp_av1_context_reset(FREERDP_AV1_CONTEXT* av1, UINT32 width, UINT32 height)
{
	if (av1->encoder)
	{
#if defined(WITH_LIBAOM)
		av1->ecfg.g_w = width;
		av1->ecfg.g_h = height;

		if (av1->initialized)
		{
			const aom_codec_err_t rc = aom_codec_destroy(&av1->ctx);
			av1->initialized = false;
			if (rc != AOM_CODEC_OK)
			{
				WLog_Print(av1->log, WLOG_WARN, "aom_codec_destroy: %s",
				           aom_codec_err_to_string(rc));
				return FALSE;
			}
		}

		const aom_codec_err_t rc =
		    aom_codec_enc_init(&av1->ctx, av1->iface, &av1->ecfg, av1->flags);
		if (rc != AOM_CODEC_OK)
		{
			WLog_Print(av1->log, WLOG_WARN, "aom_codec_enc_init: %s", aom_codec_err_to_string(rc));
			return FALSE;
		}

		av1->framecount = 0;
		av1->eflags = 0; // AOM_EFLAG_FORCE_KF;
		av1->initialized = true;
#else
		WLog_Print(av1->log, WLOG_ERROR,
		           "AV1 encoder requires libaom, recompile with '-DWITH_AOM=ON'");
		return FALSE;
#endif
	}
	else
	{
#if defined(WITH_DAV1D)
		if (av1->initialized)
		{
			dav1d_close(&av1->dav1d);
			av1->initialized = false;
		}

		Dav1dSettings settings = WINPR_C_ARRAY_INIT;
		dav1d_default_settings(&settings);
		/* Single-threaded: freerdp_av1_decompress() needs its picture back synchronously,
		 * not held back by frame-parallel decoding. */
		settings.n_threads = 1;
		settings.max_frame_delay = 1;

		const int rc = dav1d_open(&av1->dav1d, &settings);
		if (rc < 0)
		{
			WLog_Print(av1->log, WLOG_WARN, "dav1d_open: %d", rc);
			return FALSE;
		}
		av1->initialized = true;
#elif defined(WITH_LIBAOM)
		av1->dcfg.w = width;
		av1->dcfg.h = height;
		av1->dcfg.allow_lowbitdepth = 1;
		const aom_codec_err_t rc =
		    aom_codec_dec_init(&av1->ctx, av1->iface, &av1->dcfg, av1->flags);
		if (rc != AOM_CODEC_OK)
		{
			WLog_Print(av1->log, WLOG_WARN, "aom_codec_dec_init: %s", aom_codec_err_to_string(rc));
			return FALSE;
		}
		av1->initialized = true;
#else
		WLog_Print(av1->log, WLOG_WARN,
		           "This build does not support AV1 decoding. Recompile with '-DWITH_DAV1D=ON' "
		           "or '-DWITH_AOM=ON'");
		return FALSE;
#endif
	}

	av1->yuvWidth = width;
	av1->yuvStride[0] = width;

	const size_t pad = av1->yuvStride[0] % 16;
	if (pad != 0)
		av1->yuvStride[0] += 16 - pad;
	av1->yuvStride[0] *= 3;
	av1->yuvStride[1] = width;
	av1->yuvStride[2] = width;

	av1->yuvHeight = height;
	winpr_aligned_free(av1->yuvdata[0]);
	winpr_aligned_free(av1->yuvdata[1]);
	winpr_aligned_free(av1->yuvdata[2]);
	av1->yuvdata[0] = winpr_aligned_malloc(1ull * av1->yuvStride[0] * av1->yuvHeight, 64);
	av1->yuvdata[1] = winpr_aligned_malloc(1ull * av1->yuvStride[1] * av1->yuvHeight, 64);
	av1->yuvdata[2] = winpr_aligned_malloc(1ull * av1->yuvStride[2] * av1->yuvHeight, 64);
	return av1->yuvdata[0] != nullptr;
}

void freerdp_av1_context_free(FREERDP_AV1_CONTEXT* av1)
{
	if (!av1)
		return;

	if (av1->initialized)
	{
		if (av1->encoder)
		{
#if defined(WITH_LIBAOM)
			av1_aom_destroy(av1);
#endif
		}
		else
		{
#if defined(WITH_DAV1D)
			dav1d_close(&av1->dav1d);
#elif defined(WITH_LIBAOM)
			av1_aom_destroy(av1);
#endif
		}
	}
	winpr_aligned_free(av1->yuvdata[0]);
	winpr_aligned_free(av1->yuvdata[1]);
	winpr_aligned_free(av1->yuvdata[2]);
	free(av1);
}

FREERDP_AV1_CONTEXT* freerdp_av1_context_new(BOOL Compressor)
{
	FREERDP_AV1_CONTEXT* ctx = calloc(1, sizeof(FREERDP_AV1_CONTEXT));
	if (!ctx)
		return nullptr;
	ctx->encoder = Compressor;
	ctx->log = WLog_Get(TAG);
	if (!ctx->log)
		goto fail;

	if (Compressor)
	{
#if defined(WITH_LIBAOM)
		ctx->iface = aom_codec_av1_cx();
		if (!ctx->iface)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "aom_codec_av1_cx() nullptr");
			goto fail;
		}

		const aom_codec_err_t rc =
		    aom_codec_enc_config_default(ctx->iface, &ctx->ecfg, AOM_USAGE_REALTIME);
		if (rc != AOM_CODEC_OK)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "aom_codec_enc_config_default() %s",
			           aom_codec_err_to_string(rc));
			goto fail;
		}
#else
		WLog_Print(ctx->log, WLOG_ERROR,
		           "AV1 encoder requires libaom, recompile with '-DWITH_AOM=ON'");
		goto fail;
#endif
	}
#if defined(WITH_LIBAOM) && !defined(WITH_DAV1D)
	else
	{
		ctx->iface = aom_codec_av1_dx();
		if (!ctx->iface)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "aom_codec_av1_dx() nullptr");
			goto fail;
		}
	}
#endif

	return ctx;

fail:
	freerdp_av1_context_free(ctx);
	return nullptr;
}
#else

struct S_FREERDP_AV1_CONTEXT
{
	BOOL Compressor;
	wLog* log;
};

BOOL freerdp_av1_context_set_option(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                                    WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT_OPTION option,
                                    WINPR_ATTR_UNUSED UINT32 value)
{
	return FALSE;
}

UINT32 freerdp_av1_context_get_option(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                                      WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT_OPTION option)
{
	return 0;
}

INT32 freerdp_av1_compress(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                           WINPR_ATTR_UNUSED const BYTE* pSrcData,
                           WINPR_ATTR_UNUSED DWORD SrcFormat, WINPR_ATTR_UNUSED UINT32 nSrcStep,
                           WINPR_ATTR_UNUSED UINT32 nSrcWidth, WINPR_ATTR_UNUSED UINT32 nSrcHeight,
                           WINPR_ATTR_UNUSED const RECTANGLE_16* regionRect,
                           WINPR_ATTR_UNUSED BYTE** ppDstData, WINPR_ATTR_UNUSED UINT32* pDstSize,
                           WINPR_ATTR_UNUSED RDPGFX_H264_METABLOCK* meta)
{
	WINPR_ASSERT(av1);
	WLog_Print(av1->log, WLOG_ERROR,
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON' or "
	           "'-DWITH_DAV1D=ON'");
	return -1;
}

INT32 freerdp_av1_decompress(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                             WINPR_ATTR_UNUSED const BYTE* pSrcData,
                             WINPR_ATTR_UNUSED UINT32 SrcSize, WINPR_ATTR_UNUSED BYTE* pDstData,
                             WINPR_ATTR_UNUSED DWORD DstFormat, WINPR_ATTR_UNUSED UINT32 nDstStep,
                             WINPR_ATTR_UNUSED UINT32 nDstWidth,
                             WINPR_ATTR_UNUSED UINT32 nDstHeight,
                             WINPR_ATTR_UNUSED const RECTANGLE_16* regionRects,
                             WINPR_ATTR_UNUSED UINT32 numRegionRect)
{
	WINPR_ASSERT(av1);
	WLog_Print(av1->log, WLOG_ERROR,
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON' or "
	           "'-DWITH_DAV1D=ON'");
	return -1;
}

BOOL freerdp_av1_context_reset(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                               WINPR_ATTR_UNUSED UINT32 width, WINPR_ATTR_UNUSED UINT32 height)
{
	WINPR_ASSERT(av1);
	WLog_Print(av1->log, WLOG_WARN,
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON' or "
	           "'-DWITH_DAV1D=ON'");
	return FALSE;
}

void freerdp_av1_context_free(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1)
{
	free(av1);
}

FREERDP_AV1_CONTEXT* freerdp_av1_context_new(BOOL Compressor)
{
	FREERDP_AV1_CONTEXT* ctx = calloc(1, sizeof(FREERDP_AV1_CONTEXT));
	if (!ctx)
		return nullptr;
	ctx->Compressor = Compressor;
	ctx->log = WLog_Get(TAG);
	return ctx;
}
#endif
