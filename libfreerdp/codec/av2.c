/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV2 Bitmap Compression
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

#include <freerdp/codec/av2.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/region.h>

#define TAG FREERDP_TAG("codec.av2")

#if defined(WITH_LIBAVM)
#include <avm/avm.h>

#include <avm/avm_decoder.h>
#include <avm/avm_image.h>
#include <avm/avmdx.h>
#endif

/* The encoder: libavm's, unless it was built without one (CONFIG_AV2_ENCODER=0,
 * WITH_AVM_ENCODER=OFF), which leaves out its headers and functions; decoding is the same */
#if defined(WITH_LIBAVM) && defined(WITH_LIBAVM_ENCODER)
#define AV2_ENCODER_LIBAVM
#include <avm/avm_encoder.h>
#include <avm/avmcx.h>
#endif

/* The decoder: libavm, the reference, when there is one. dav2d 0.1.0 decodes the intra frames
 * of libavm v1.0.0 bit-exact, but not its inter frames, and rejects its 4:4:4 streams. */
#if defined(WITH_DAV2D) && !defined(WITH_LIBAVM)
#define AV2_DECODER_DAV2D
#include <dav2d/dav2d.h>
#endif

#if defined(WITH_LIBAVM) || defined(WITH_DAV2D)

#if defined(WITH_LIBYUV)
#include <libyuv.h>
#endif

/* AV2 bitstream profiles, [AV2] Annex A, Table A.1 */
#define AV2_PROFILE_MAIN_420_10_IP0 0
#define AV2_PROFILE_MAIN_444_10_IP1 4

/* The realtime usage is newer than libavm v1.0.0 */
#if defined(WITH_LIBAVM) && !defined(AVM_USAGE_REALTIME)
#define AVM_USAGE_REALTIME AVM_USAGE_GOOD_QUALITY
#endif

/* The planes of a decoded picture */
typedef struct
{
	const BYTE* data[3];
	UINT32 stride[3]; /* in bytes */
	UINT32 width;
	UINT32 height;
	UINT32 bpc;  /* significant bits per sample */
	BOOL highbd; /* samples stored in 16 bits instead of 8 */
	BOOL i444;
} av2_planes;

struct S_FREERDP_AV2_CONTEXT
{
	wLog* log;
	bool encoder;
	bool initialized;
#if defined(WITH_LIBAVM)
	avm_codec_ctx_t ctx;
#if defined(AV2_ENCODER_LIBAVM)
	avm_codec_enc_cfg_t ecfg;
	avm_codec_pts_t framecount;
	avm_enc_frame_flags_t eflags;
#endif
	avm_codec_flags_t flags;
	avm_codec_iface_t* iface;
	UINT32 profile; /* FREERDP_AV2_CONTEXT_OPTION_PROFILE: 0 4:2:0, 1 4:4:4 */
#endif
#if defined(WITH_LIBAVM)
	avm_codec_dec_cfg_t dcfg;
#endif
#if defined(AV2_DECODER_DAV2D)
	Dav2dContext* dav2d;
#endif
	BYTE* yuvdata[3];
	UINT32 yuvWidth;
	UINT32 yuvStride[3];
	UINT32 yuvHeight;
};

#if defined(WITH_LIBAVM)
static void av2_avm_destroy(FREERDP_AV2_CONTEXT* av2)
{
	const avm_codec_err_t rc = avm_codec_destroy(&av2->ctx);
	if (rc != AVM_CODEC_OK)
		WLog_Print(av2->log, WLOG_WARN, "avm_codec_destroy: %s", avm_codec_err_to_string(rc));
}
#endif

#if defined(AV2_ENCODER_LIBAVM)

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

WINPR_ATTR_NODISCARD
static BOOL av2_avm_control(FREERDP_AV2_CONTEXT* av2, int id, const char* name, int value)
{
	const avm_codec_err_t rc = avm_codec_control(&av2->ctx, id, value);
	if (rc != AVM_CODEC_OK)
	{
		WLog_Print(av2->log, WLOG_WARN, "avm_codec_control(%s, %d): %s", name, value,
		           avm_codec_err_to_string(rc));
		return FALSE;
	}
	return TRUE;
}
#endif

BOOL freerdp_av2_context_set_option(FREERDP_AV2_CONTEXT* av2, FREERDP_AV2_CONTEXT_OPTION option,
                                    UINT32 value)
{
#if defined(AV2_ENCODER_LIBAVM)
	switch (option)
	{
		case FREERDP_AV2_CONTEXT_OPTION_PROFILE:
			if (!av2->encoder)
				return FALSE;
			if (value > 1)
			{
				WLog_Print(av2->log, WLOG_WARN,
				           "Unknown FREERDP_AV2_CONTEXT_OPTION_PROFILE value %" PRIu32, value);
				return FALSE;
			}

			av2->profile = value;
			av2->ecfg.g_profile =
			    (value == 1) ? AV2_PROFILE_MAIN_444_10_IP1 : AV2_PROFILE_MAIN_420_10_IP0;
			break;

		case FREERDP_AV2_CONTEXT_OPTION_RATECONTROL:
			if (!av2->encoder)
				return FALSE;
			switch (value)
			{
				case FREERDP_AV2_VBR:
					av2->ecfg.rc_end_usage = AVM_VBR;
					break;
				case FREERDP_AV2_CBR:
					av2->ecfg.rc_end_usage = AVM_CBR;
					break;
				case FREERDP_AV2_CQ:
					av2->ecfg.rc_end_usage = AVM_CQ;
					break;
				case FREERDP_AV2_Q:
					av2->ecfg.rc_end_usage = AVM_Q;
					break;
				default:
					WLog_Print(av2->log, WLOG_WARN,
					           "Unknown FREERDP_AV2_CONTEXT_OPTION_RATECONTROL value [0x%08" PRIx32
					           "]",
					           value);
					return FALSE;
			}
			break;
		case FREERDP_AV2_CONTEXT_OPTION_BITRATE:
			if (!av2->encoder)
				return FALSE;
			av2->ecfg.rc_target_bitrate = value;
			break;
		case FREERDP_AV2_CONTEXT_OPTION_USAGETYPE:
			if (!av2->encoder)
				return FALSE;
			av2->ecfg.g_usage = value;
			break;
		default:
			WLog_Print(av2->log, WLOG_ERROR, "Unknown FREERDP_AV2_CONTEXT_OPTION[0x%08" PRIx32 "]",
			           option);
			return TRUE;
	}
	return freerdp_av2_context_reset(av2, av2->ecfg.g_w, av2->ecfg.g_h);
#else
	WLog_Print(av2->log, WLOG_WARN,
	           "AV2 encoder requires libavm with its encoder, recompile with '-DWITH_AVM=ON'");
	WINPR_UNUSED(option);
	WINPR_UNUSED(value);
	return FALSE;
#endif
}

UINT32 freerdp_av2_context_get_option(FREERDP_AV2_CONTEXT* av2, FREERDP_AV2_CONTEXT_OPTION option)
{
#if defined(AV2_ENCODER_LIBAVM)
	switch (option)
	{
		case FREERDP_AV2_CONTEXT_OPTION_PROFILE:
			if (!av2->encoder)
				return 0;
			return av2->profile;
		case FREERDP_AV2_CONTEXT_OPTION_RATECONTROL:
			if (!av2->encoder)
				return 0;
			switch (av2->ecfg.rc_end_usage)
			{
				case AVM_VBR:
					return FREERDP_AV2_VBR;
				case AVM_CBR:
					return FREERDP_AV2_CBR;
				case AVM_CQ:
					return FREERDP_AV2_CQ;
				case AVM_Q:
				default:
					return FREERDP_AV2_Q;
			}
		case FREERDP_AV2_CONTEXT_OPTION_BITRATE:
			if (!av2->encoder)
				return 0;
			return av2->ecfg.rc_target_bitrate;
		case FREERDP_AV2_CONTEXT_OPTION_USAGETYPE:
			if (!av2->encoder)
				return 0;
			return av2->ecfg.g_usage;
		default:
			WLog_Print(av2->log, WLOG_ERROR, "Unknown FREERDP_AV2_CONTEXT_OPTION[0x%08" PRIx32 "]",
			           option);
			return 0;
	}
#else
	WINPR_UNUSED(option);
	return 0;
#endif
}

INT32 freerdp_av2_compress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData, DWORD SrcFormat,
                           UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
                           const RECTANGLE_16* regionRect, BYTE** ppDstData, UINT32* pDstSize,
                           RDPGFX_H264_METABLOCK* meta)
{
	*ppDstData = nullptr;
	*pDstSize = 0;

	if (!av2->encoder)
	{
		WLog_Print(av2->log, WLOG_ERROR, "av2->encoder: %d", av2->encoder);
		return -1;
	}

#if defined(AV2_ENCODER_LIBAVM)
	primitives_t* primitives = primitives_get();
	if (!primitives)
	{
		WLog_Print(av2->log, WLOG_ERROR, "primitives_get(): nullptr");
		return -1;
	}

	if (av2->yuvWidth != nSrcWidth)
	{
		WLog_Print(av2->log, WLOG_ERROR, "av2->yuvWidth[%" PRIu32 "] != nSrcWidth[%" PRIu32 "]",
		           av2->yuvWidth, nSrcWidth);
		return -1;
	}

	if (av2->yuvHeight != nSrcHeight)
	{
		WLog_Print(av2->log, WLOG_ERROR, "av2->yuvHeight[%" PRIu32 "] != nSrcHeight[%" PRIu32 "]",
		           av2->yuvHeight, nSrcHeight);
		return -1;
	}

	const prim_size_t roi = { .width = nSrcWidth, .height = nSrcHeight };

	avm_image_t buffer = WINPR_C_ARRAY_INIT;

	avm_image_t* img = &buffer;
	pstatus_t rec = -1;
#if defined(WITH_LIBYUV)
	/* ARGBToI420()/ARGBToI444() always read pSrcData as packed BGRA8888 regardless of
	 * SrcFormat, so the fast path only applies for that layout; anything else must go
	 * through the format-aware primitives conversion below. */
	if ((SrcFormat == PIXEL_FORMAT_BGRA32) || (SrcFormat == PIXEL_FORMAT_BGRX32))
	{
		switch (av2->profile)
		{
			case 0:
				img->fmt = AVM_IMG_FMT_I420;
				img->x_chroma_shift = 1;
				img->y_chroma_shift = 1;
				rec = ARGBToI420(pSrcData, nSrcStep, av2->yuvdata[0], av2->yuvStride[0],
				                 av2->yuvdata[1], av2->yuvStride[1], av2->yuvdata[2],
				                 WINPR_ASSERTING_INT_CAST(int, av2->yuvStride[2]),
				                 WINPR_ASSERTING_INT_CAST(int, roi.width),
				                 WINPR_ASSERTING_INT_CAST(int, roi.height));
				break;
			case 1:
				img->fmt = AVM_IMG_FMT_I444;
				rec = ARGBToI444(pSrcData, nSrcStep, av2->yuvdata[0], av2->yuvStride[0],
				                 av2->yuvdata[1], av2->yuvStride[1], av2->yuvdata[2],
				                 WINPR_ASSERTING_INT_CAST(int, av2->yuvStride[2]),
				                 WINPR_ASSERTING_INT_CAST(int, roi.width),
				                 WINPR_ASSERTING_INT_CAST(int, roi.height));
				break;
			default:
				WLog_Print(av2->log, WLOG_ERROR, "Unsupported AV2 profile %" PRIu32, av2->profile);
				return -1;
		}
	}
	else
#endif
	{
		switch (av2->profile)
		{
			case 0:
				img->fmt = AVM_IMG_FMT_I420;
				img->x_chroma_shift = 1;
				img->y_chroma_shift = 1;
				rec = primitives->RGBToYUV420_8u_P3AC4R(pSrcData, SrcFormat, nSrcStep, av2->yuvdata,
				                                        av2->yuvStride, &roi);
				break;
			case 1:
				img->fmt = AVM_IMG_FMT_I444;
				rec = primitives->RGBToI444_8u(pSrcData, SrcFormat, nSrcStep, av2->yuvdata,
				                               av2->yuvStride, &roi);
				break;
			default:
				WLog_Print(av2->log, WLOG_ERROR, "Unsupported AV2 profile %" PRIu32, av2->profile);
				return -1;
		}
	}
	if (rec != 0)
	{
		WLog_Print(av2->log, WLOG_ERROR, "RGB to YUV conversion failed: %d", rec);
		return -1;
	}
	img->bit_depth = 8;
	img->d_w = img->r_w = img->w = roi.width;
	img->d_h = img->r_h = img->h = roi.height;
	img->stride[0] = WINPR_ASSERTING_INT_CAST(int, av2->yuvStride[0]);
	img->stride[1] = WINPR_ASSERTING_INT_CAST(int, av2->yuvStride[1]);
	img->stride[2] = WINPR_ASSERTING_INT_CAST(int, av2->yuvStride[2]);
	img->planes[0] = av2->yuvdata[0];
	img->planes[1] = av2->yuvdata[1];
	img->planes[2] = av2->yuvdata[2];

	const avm_codec_err_t rc = avm_codec_encode(&av2->ctx, img, ++av2->framecount, 1, av2->eflags);
	av2->eflags = 0;

	if (rc != AVM_CODEC_OK)
	{
		WLog_Print(av2->log, WLOG_WARN, "avm_codec_encode: %s", avm_codec_err_to_string(rc));
		return -1;
	}

	avm_codec_iter_t iter = nullptr;
	const avm_codec_cx_pkt_t* pkt = nullptr;
	while ((pkt = avm_codec_get_cx_data(&av2->ctx, &iter)) != nullptr)
	{
		if (pkt->kind != AVM_CODEC_CX_FRAME_PKT)
			continue;

		*ppDstData = pkt->data.frame.buf;
		*pDstSize = WINPR_ASSERTING_INT_CAST(UINT32, pkt->data.frame.sz);
	}
	if (*pDstSize == 0)
		return 0;

	RECTANGLE_16* rect = calloc(1, sizeof(RECTANGLE_16));
	if (rect)
	{
		rect->right = WINPR_ASSERTING_INT_CAST(UINT16, nSrcWidth);
		rect->bottom = WINPR_ASSERTING_INT_CAST(UINT16, nSrcHeight);
	}
	return allocate_h264_metablock(10, rect, meta, 1);
#else
	WLog_Print(av2->log, WLOG_ERROR,
	           "AV2 encoder requires libavm with its encoder, recompile with '-DWITH_AVM=ON'");
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

/* AV2's main profiles are 10 bit: a decoder may hand out 16 bit samples, even for an 8 bit
 * stream. Bring them down to 8 bit into the context's own planes, so that the colour
 * conversion below always reads 8 bit samples. */
WINPR_ATTR_NODISCARD
static BOOL av2_planes_to_8bit(FREERDP_AV2_CONTEXT* av2, av2_planes* planes)
{
	WINPR_ASSERT(av2);
	WINPR_ASSERT(planes);

	if (!planes->highbd)
		return (planes->bpc == 8);
	if ((planes->bpc < 8) || (planes->bpc > 16))
	{
		WLog_Print(av2->log, WLOG_ERROR, "AV2 picture bpc %" PRIu32 " not supported", planes->bpc);
		return FALSE;
	}

	if ((planes->width > av2->yuvWidth) || (planes->height > av2->yuvHeight))
	{
		WLog_Print(av2->log, WLOG_ERROR,
		           "AV2 picture %" PRIu32 "x%" PRIu32 " larger than the surface %" PRIu32
		           "x%" PRIu32,
		           planes->width, planes->height, av2->yuvWidth, av2->yuvHeight);
		return FALSE;
	}

	const UINT32 shift = planes->bpc - 8;
	for (size_t p = 0; p < 3; p++)
	{
		const UINT32 w = ((p == 0) || planes->i444) ? planes->width : (planes->width + 1) / 2;
		const UINT32 h = ((p == 0) || planes->i444) ? planes->height : (planes->height + 1) / 2;
		for (UINT32 y = 0; y < h; y++)
		{
			const UINT16* src = (const UINT16*)&planes->data[p][1ull * y * planes->stride[p]];
			BYTE* dst = &av2->yuvdata[p][1ull * y * av2->yuvStride[p]];
			for (UINT32 x = 0; x < w; x++)
			{
				const UINT32 v = src[x] >> shift;
				dst[x] = (BYTE)MIN(v, UINT8_MAX);
			}
		}
		planes->data[p] = av2->yuvdata[p];
		planes->stride[p] = av2->yuvStride[p];
	}
	planes->bpc = 8;
	planes->highbd = FALSE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static INT32 av2_convert(FREERDP_AV2_CONTEXT* av2, av2_planes* planes, BYTE* pDstData,
                         DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
                         const RECTANGLE_16* regionRects, UINT32 numRegionRect)
{
	if (!av2_planes_to_8bit(av2, planes))
	{
		WLog_Print(av2->log, WLOG_ERROR, "AV2 picture with %" PRIu32 " bit samples not supported",
		           planes->bpc);
		return -1;
	}

	/* The decoded frame may be smaller than the destination surface, so
	 * bound the converted region to it. Otherwise the colour conversion
	 * reads past the decoded image planes. */
	const UINT32 nWidth = (planes->width < nDstWidth) ? planes->width : nDstWidth;
	const UINT32 nHeight = (planes->height < nDstHeight) ? planes->height : nDstHeight;
	const prim_size_t roi = { .width = nWidth, .height = nHeight };

	if (!areRectsValid(av2->log, roi.width, roi.height, regionRects, numRegionRect))
		return -2;

	pstatus_t rec = -1;
#if defined(WITH_LIBYUV)
	/* J420ToARGB()/J444ToARGB() always write pDstData as packed BGRA8888 regardless of
	 * DstFormat, so the fast path only applies for that layout; anything else must go
	 * through the format-aware primitives conversion below. */
	if ((DstFormat == PIXEL_FORMAT_BGRA32) || (DstFormat == PIXEL_FORMAT_BGRX32))
	{
		const int stride0 = WINPR_ASSERTING_INT_CAST(int, planes->stride[0]);
		const int stride1 = WINPR_ASSERTING_INT_CAST(int, planes->stride[1]);
		const int stride2 = WINPR_ASSERTING_INT_CAST(int, planes->stride[2]);
		const int iWidth = WINPR_ASSERTING_INT_CAST(int, nWidth);
		const int iHeight = WINPR_ASSERTING_INT_CAST(int, nHeight);
		const int iDstStep = WINPR_ASSERTING_INT_CAST(int, nDstStep);

		if (planes->i444)
			rec = J444ToARGB(planes->data[0], stride0, planes->data[1], stride1, planes->data[2],
			                 stride2, pDstData, iDstStep, iWidth, iHeight);
		else
			rec = J420ToARGB(planes->data[0], stride0, planes->data[1], stride1, planes->data[2],
			                 stride2, pDstData, iDstStep, iWidth, iHeight);
	}
	else
#endif
	{
		primitives_t* primitives = primitives_get();
		if (!primitives)
		{
			WLog_Print(av2->log, WLOG_ERROR, "primitives_get(): nullptr");
			return -1;
		}

		if (planes->i444)
			rec = primitives->I444ToRGB_8u(planes->data, planes->stride, pDstData, nDstStep,
			                               DstFormat, &roi);
		else
			rec = primitives->YUV420ToRGB_8u_P3AC4R(planes->data, planes->stride, pDstData,
			                                        nDstStep, DstFormat, &roi);
	}
	if (rec != 0)
	{
		WLog_Print(av2->log, WLOG_ERROR, "YUV to RGB conversion failed: %d", rec);
		return -1;
	}
	return TRUE;
}

#if defined(AV2_DECODER_DAV2D)
static void av2_dav2d_data_free(WINPR_ATTR_UNUSED const uint8_t* buf,
                                WINPR_ATTR_UNUSED void* cookie)
{
	/* pSrcData is owned by the caller of freerdp_av2_decompress(), dav2d must not free it. */
}

/* Return: TRUE if a picture was converted, FALSE if none was ready (EAGAIN), <0 on error. */
static INT32 av2_dav2d_drain_one(FREERDP_AV2_CONTEXT* av2, BYTE* pDstData, DWORD DstFormat,
                                 UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
                                 const RECTANGLE_16* regionRects, UINT32 numRegionRect)
{
	Dav2dPicture pic = WINPR_C_ARRAY_INIT;
	const int prc = dav2d_get_picture(av2->dav2d, &pic);
	if (prc == DAV2D_ERR(EAGAIN))
		return FALSE;
	if (prc < 0)
	{
		WLog_Print(av2->log, WLOG_WARN, "dav2d_get_picture: %d", prc);
		return -1;
	}

	INT32 rc = -1;
	switch (pic.p.layout)
	{
		case DAV2D_PIXEL_LAYOUT_I420:
		case DAV2D_PIXEL_LAYOUT_I444:
		{
			av2_planes planes = { .data = { pic.data[0], pic.data[1], pic.data[2] },
				                  .stride = { (UINT32)pic.stride[0], (UINT32)pic.stride[1],
				                              (UINT32)pic.stride[1] },
				                  .width = (UINT32)pic.p.w,
				                  .height = (UINT32)pic.p.h,
				                  .bpc = (UINT32)pic.p.bpc,
				                  .highbd = (pic.p.bpc > 8),
				                  .i444 = (pic.p.layout == DAV2D_PIXEL_LAYOUT_I444) };
			rc = av2_convert(av2, &planes, pDstData, DstFormat, nDstStep, nDstWidth, nDstHeight,
			                 regionRects, numRegionRect);
		}
		break;
		default:
			WLog_Print(av2->log, WLOG_ERROR, "dav2d picture layout %d not supported", pic.p.layout);
			break;
	}
	dav2d_picture_unref(&pic);
	return (rc == TRUE) ? TRUE : rc;
}
#endif

INT32 freerdp_av2_decompress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData, UINT32 SrcSize,
                             BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
                             UINT32 nDstHeight, const RECTANGLE_16* regionRects,
                             UINT32 numRegionRect)
{
	WINPR_ASSERT(av2);
	if (av2->encoder)
	{
		WLog_Print(av2->log, WLOG_ERROR, "av2->encoder: %d", av2->encoder);
		return -1;
	}

	if (!areRectsValid(av2->log, nDstWidth, nDstHeight, regionRects, numRegionRect))
		return -2;

#if defined(AV2_DECODER_DAV2D)
	Dav2dData data = WINPR_C_ARRAY_INIT;
	const int wrc = dav2d_data_wrap(&data, pSrcData, SrcSize, av2_dav2d_data_free, nullptr);
	if (wrc < 0)
	{
		WLog_Print(av2->log, WLOG_WARN, "dav2d_data_wrap: %d", wrc);
		return -1;
	}

	while (data.sz > 0)
	{
		const int src = dav2d_send_data(av2->dav2d, &data);
		if ((src < 0) && (src != DAV2D_ERR(EAGAIN)))
		{
			WLog_Print(av2->log, WLOG_WARN, "dav2d_send_data: %d", src);
			dav2d_data_unref(&data);
			return -1;
		}

		const INT32 rc = av2_dav2d_drain_one(av2, pDstData, DstFormat, nDstStep, nDstWidth,
		                                     nDstHeight, regionRects, numRegionRect);
		if ((rc != TRUE) && (rc != FALSE))
		{
			dav2d_data_unref(&data);
			return rc;
		}
	}

	/* Drain any picture the decoder buffered internally before it consumed the last input. */
	for (;;)
	{
		const INT32 rc = av2_dav2d_drain_one(av2, pDstData, DstFormat, nDstStep, nDstWidth,
		                                     nDstHeight, regionRects, numRegionRect);
		if (rc == FALSE)
			break;
		if (rc != TRUE)
			return rc;
	}

	return TRUE;
#elif defined(WITH_LIBAVM)
	const avm_codec_err_t rc = avm_codec_decode(&av2->ctx, pSrcData, SrcSize, nullptr);
	if (rc != AVM_CODEC_OK)
	{
		WLog_Print(av2->log, WLOG_WARN, "avm_codec_decode: %s", avm_codec_err_to_string(rc));
		return -1;
	}

	avm_image_t* img = nullptr;
	avm_codec_iter_t iter = nullptr;
	while ((img = avm_codec_get_frame(&av2->ctx, &iter)) != nullptr)
	{
		const avm_img_fmt_t fmt = img->fmt & ~AVM_IMG_FMT_HIGHBITDEPTH;
		if ((fmt != AVM_IMG_FMT_I420) && (fmt != AVM_IMG_FMT_I444))
		{
			WLog_Print(av2->log, WLOG_ERROR, "img->fmt %d not supported", img->fmt);
			return -1;
		}

		/* bit_depth is the coded depth; a high bit depth buffer may still carry 8 bit samples */
		av2_planes planes = { .data = { img->planes[0], img->planes[1], img->planes[2] },
			                  .stride = { (UINT32)img->stride[0], (UINT32)img->stride[1],
			                              (UINT32)img->stride[2] },
			                  .width = img->d_w,
			                  .height = img->d_h,
			                  .bpc = img->bit_depth,
			                  .highbd = (img->fmt & AVM_IMG_FMT_HIGHBITDEPTH) != 0,
			                  .i444 = (fmt == AVM_IMG_FMT_I444) };

		const INT32 crc = av2_convert(av2, &planes, pDstData, DstFormat, nDstStep, nDstWidth,
		                              nDstHeight, regionRects, numRegionRect);
		if (crc != TRUE)
			return crc;
	}

	return TRUE;
#endif
}

BOOL freerdp_av2_context_reset(FREERDP_AV2_CONTEXT* av2, UINT32 width, UINT32 height)
{
	if (av2->encoder)
	{
#if defined(AV2_ENCODER_LIBAVM)
		av2->ecfg.g_w = width;
		av2->ecfg.g_h = height;

		if (av2->initialized)
		{
			const avm_codec_err_t rc = avm_codec_destroy(&av2->ctx);
			av2->initialized = false;
			if (rc != AVM_CODEC_OK)
			{
				WLog_Print(av2->log, WLOG_WARN, "avm_codec_destroy: %s",
				           avm_codec_err_to_string(rc));
				return FALSE;
			}
		}

		const avm_codec_err_t rc =
		    avm_codec_enc_init(&av2->ctx, av2->iface, &av2->ecfg, av2->flags);
		if (rc != AVM_CODEC_OK)
		{
			WLog_Print(av2->log, WLOG_WARN, "avm_codec_enc_init: %s", avm_codec_err_to_string(rc));
			return FALSE;
		}
		av2->initialized = true;

		/* A remote desktop: screen content, the fastest preset of the usage */
		const int cpuUsed = (av2->ecfg.g_usage == AVM_USAGE_REALTIME) ? 9 : 6;
		if (!av2_avm_control(av2, AVME_SET_CPUUSED, "AVME_SET_CPUUSED", cpuUsed) ||
		    !av2_avm_control(av2, AV2E_SET_TUNE_CONTENT, "AV2E_SET_TUNE_CONTENT",
		                     AVM_CONTENT_SCREEN) ||
		    !av2_avm_control(av2, AV2E_SET_ROW_MT, "AV2E_SET_ROW_MT", 1))
			return FALSE;

		av2->framecount = 0;
		av2->eflags = AVM_EFLAG_FORCE_KF;
#else
		WLog_Print(av2->log, WLOG_ERROR,
		           "AV2 encoder requires libavm with its encoder, recompile with '-DWITH_AVM=ON'");
		return FALSE;
#endif
	}
	else
	{
#if defined(AV2_DECODER_DAV2D)
		if (av2->initialized)
		{
			dav2d_close(&av2->dav2d);
			av2->initialized = false;
		}

		Dav2dSettings settings = WINPR_C_ARRAY_INIT;
		dav2d_default_settings(&settings);
		/* Single-threaded: freerdp_av2_decompress() needs its picture back synchronously,
		 * not held back by frame-parallel decoding. */
		settings.n_threads = 1;
		settings.max_frame_delay = 1;

		const int rc = dav2d_open(&av2->dav2d, &settings);
		if (rc < 0)
		{
			WLog_Print(av2->log, WLOG_WARN, "dav2d_open: %d", rc);
			return FALSE;
		}
		av2->initialized = true;
#elif defined(WITH_LIBAVM)
		if (av2->initialized)
		{
			av2_avm_destroy(av2);
			av2->initialized = false;
		}

		av2->dcfg.w = width;
		av2->dcfg.h = height;
		const avm_codec_err_t rc =
		    avm_codec_dec_init(&av2->ctx, av2->iface, &av2->dcfg, av2->flags);
		if (rc != AVM_CODEC_OK)
		{
			WLog_Print(av2->log, WLOG_WARN, "avm_codec_dec_init: %s", avm_codec_err_to_string(rc));
			return FALSE;
		}
		av2->initialized = true;
#else
		WLog_Print(av2->log, WLOG_WARN,
		           "This build does not support AV2 decoding. Recompile with '-DWITH_DAV2D=ON' "
		           "or '-DWITH_AVM=ON'");
		return FALSE;
#endif
	}

	av2->yuvWidth = width;
	av2->yuvStride[0] = width;

	const size_t pad = av2->yuvStride[0] % 16;
	if (pad != 0)
		av2->yuvStride[0] += 16 - pad;
	av2->yuvStride[1] = av2->yuvStride[0];
	av2->yuvStride[2] = av2->yuvStride[0];

	av2->yuvHeight = height;
	winpr_aligned_free(av2->yuvdata[0]);
	winpr_aligned_free(av2->yuvdata[1]);
	winpr_aligned_free(av2->yuvdata[2]);
	av2->yuvdata[0] = winpr_aligned_malloc(1ull * av2->yuvStride[0] * av2->yuvHeight, 64);
	av2->yuvdata[1] = winpr_aligned_malloc(1ull * av2->yuvStride[1] * av2->yuvHeight, 64);
	av2->yuvdata[2] = winpr_aligned_malloc(1ull * av2->yuvStride[2] * av2->yuvHeight, 64);
	return (av2->yuvdata[0] != nullptr) && (av2->yuvdata[1] != nullptr) &&
	       (av2->yuvdata[2] != nullptr);
}

void freerdp_av2_context_free(FREERDP_AV2_CONTEXT* av2)
{
	if (!av2)
		return;

	if (av2->initialized)
	{
		if (av2->encoder)
		{
#if defined(WITH_LIBAVM)
			av2_avm_destroy(av2);
#endif
		}
		else
		{
#if defined(AV2_DECODER_DAV2D)
			dav2d_close(&av2->dav2d);
#elif defined(WITH_LIBAVM)
			av2_avm_destroy(av2);
#endif
		}
	}
	winpr_aligned_free(av2->yuvdata[0]);
	winpr_aligned_free(av2->yuvdata[1]);
	winpr_aligned_free(av2->yuvdata[2]);
	free(av2);
}

FREERDP_AV2_CONTEXT* freerdp_av2_context_new(BOOL Compressor)
{
	FREERDP_AV2_CONTEXT* ctx = calloc(1, sizeof(FREERDP_AV2_CONTEXT));
	if (!ctx)
		return nullptr;
	ctx->encoder = Compressor;
	ctx->log = WLog_Get(TAG);
	if (!ctx->log)
		goto fail;

	if (Compressor)
	{
#if defined(AV2_ENCODER_LIBAVM)
		ctx->iface = avm_codec_av2_cx();
		if (!ctx->iface)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "avm_codec_av2_cx() nullptr");
			goto fail;
		}

		const avm_codec_err_t rc =
		    avm_codec_enc_config_default(ctx->iface, &ctx->ecfg, AVM_USAGE_REALTIME);
		if (rc != AVM_CODEC_OK)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "avm_codec_enc_config_default() %s",
			           avm_codec_err_to_string(rc));
			goto fail;
		}
		/* A frame out for each frame in, and no key frames but the first */
		ctx->ecfg.g_lag_in_frames = 0;
		ctx->ecfg.kf_mode = AVM_KF_DISABLED;
		ctx->ecfg.g_profile = AV2_PROFILE_MAIN_420_10_IP0;
		ctx->ecfg.g_bit_depth = AVM_BITS_8;
		ctx->ecfg.g_input_bit_depth = 8;
#else
		WLog_Print(ctx->log, WLOG_ERROR,
		           "AV2 encoder requires libavm with its encoder, recompile with '-DWITH_AVM=ON'");
		goto fail;
#endif
	}
#if defined(WITH_LIBAVM)
	else
	{
		ctx->iface = avm_codec_av2_dx();
		if (!ctx->iface)
		{
			WLog_Print(ctx->log, WLOG_ERROR, "avm_codec_av2_dx() nullptr");
			goto fail;
		}
	}
#endif

	return ctx;

fail:
	freerdp_av2_context_free(ctx);
	return nullptr;
}
#else

struct S_FREERDP_AV2_CONTEXT
{
	BOOL Compressor;
	wLog* log;
};

BOOL freerdp_av2_context_set_option(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2,
                                    WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT_OPTION option,
                                    WINPR_ATTR_UNUSED UINT32 value)
{
	return FALSE;
}

UINT32 freerdp_av2_context_get_option(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2,
                                      WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT_OPTION option)
{
	return 0;
}

INT32 freerdp_av2_compress(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2,
                           WINPR_ATTR_UNUSED const BYTE* pSrcData,
                           WINPR_ATTR_UNUSED DWORD SrcFormat, WINPR_ATTR_UNUSED UINT32 nSrcStep,
                           WINPR_ATTR_UNUSED UINT32 nSrcWidth, WINPR_ATTR_UNUSED UINT32 nSrcHeight,
                           WINPR_ATTR_UNUSED const RECTANGLE_16* regionRect,
                           WINPR_ATTR_UNUSED BYTE** ppDstData, WINPR_ATTR_UNUSED UINT32* pDstSize,
                           WINPR_ATTR_UNUSED RDPGFX_H264_METABLOCK* meta)
{
	WINPR_ASSERT(av2);
	WLog_Print(av2->log, WLOG_ERROR,
	           "This build does not support AV2 codec. Recompile with '-DWITH_AVM=ON' or "
	           "'-DWITH_DAV2D=ON'");
	return -1;
}

INT32 freerdp_av2_decompress(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2,
                             WINPR_ATTR_UNUSED const BYTE* pSrcData,
                             WINPR_ATTR_UNUSED UINT32 SrcSize, WINPR_ATTR_UNUSED BYTE* pDstData,
                             WINPR_ATTR_UNUSED DWORD DstFormat, WINPR_ATTR_UNUSED UINT32 nDstStep,
                             WINPR_ATTR_UNUSED UINT32 nDstWidth,
                             WINPR_ATTR_UNUSED UINT32 nDstHeight,
                             WINPR_ATTR_UNUSED const RECTANGLE_16* regionRects,
                             WINPR_ATTR_UNUSED UINT32 numRegionRect)
{
	WINPR_ASSERT(av2);
	WLog_Print(av2->log, WLOG_ERROR,
	           "This build does not support AV2 codec. Recompile with '-DWITH_AVM=ON' or "
	           "'-DWITH_DAV2D=ON'");
	return -1;
}

BOOL freerdp_av2_context_reset(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2,
                               WINPR_ATTR_UNUSED UINT32 width, WINPR_ATTR_UNUSED UINT32 height)
{
	WINPR_ASSERT(av2);
	WLog_Print(av2->log, WLOG_WARN,
	           "This build does not support AV2 codec. Recompile with '-DWITH_AVM=ON' or "
	           "'-DWITH_DAV2D=ON'");
	return FALSE;
}

void freerdp_av2_context_free(WINPR_ATTR_UNUSED FREERDP_AV2_CONTEXT* av2)
{
	free(av2);
}

FREERDP_AV2_CONTEXT* freerdp_av2_context_new(BOOL Compressor)
{
	FREERDP_AV2_CONTEXT* ctx = calloc(1, sizeof(FREERDP_AV2_CONTEXT));
	if (!ctx)
		return nullptr;
	ctx->Compressor = Compressor;
	ctx->log = WLog_Get(TAG);
	return ctx;
}
#endif
