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

#include <freerdp/config.h>

#include <freerdp/codec/av1.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("codec.av1")

#if defined(WITH_LIBAOM)
#include <aom/aom.h>

#include <aom/aom_decoder.h>
#include <aom/aom_encoder.h>

#include <aom/aom_image.h>
#include <aom/aomcx.h>
#include <aom/aomdx.h>

#if defined(WITH_LIBYUV)
#include <libyuv.h>
#endif

#if defined(WITH_LIBVA_AV1_DECODE)
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixfmt.h>

#define VAAPI_DEVICE "/dev/dri/renderD128"

#if !defined(av_err2str)
static inline char* av1_error_string(char* errbuf, size_t errbuf_size, int errnum)
{
	av_strerror(errnum, errbuf, errbuf_size);
	return errbuf;
}

#define av_err2str(errnum) av1_error_string((char[64])WINPR_C_ARRAY_INIT, 64, errnum)
#endif
#endif

typedef enum
{
	AV1_BACKEND_SOFTWARE_AOM,
	AV1_BACKEND_LIBVA,
	AV1_BACKEND_FAILED_TO_SOFTWARE
} AV1_BACKEND;

struct S_FREERDP_AV1_CONTEXT
{
	aom_codec_ctx_t ctx;
	wLog* log;
	bool encoder;
	bool initialized;
	AV1_BACKEND backend;
	aom_codec_enc_cfg_t ecfg;
	aom_codec_dec_cfg_t dcfg;
	aom_codec_pts_t framecount;
	aom_enc_frame_flags_t eflags;
	aom_codec_flags_t flags;
	aom_codec_iface_t* iface;
	BYTE* yuvdata[3];
	UINT32 yuvWidth;
	UINT32 yuvStride[3];
	UINT32 yuvHeight;
#if defined(WITH_LIBVA_AV1_DECODE)
	const AVCodec* codecDecoder;
	AVCodecContext* codecDecoderContext;
	AVBufferRef* hwctx;
	AVFrame* swFrame;
	AVFrame* hwFrame;
	AVPacket* packet;
	enum AVPixelFormat hw_pix_fmt;
	bool vaapiFormatRejected;
#endif
};

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

static BOOL freerdp_av1_init_aom_decoder(FREERDP_AV1_CONTEXT* av1)
{
	WINPR_ASSERT(av1);

	if (av1->iface)
		return TRUE;

	av1->iface = aom_codec_av1_dx();
	if (!av1->iface)
	{
		WLog_Print(av1->log, WLOG_ERROR, "aom_codec_av1_dx() nullptr");
		return FALSE;
	}

	return TRUE;
}

#if defined(WITH_LIBVA_AV1_DECODE)
static void freerdp_av1_vaapi_release(FREERDP_AV1_CONTEXT* av1)
{
	WINPR_ASSERT(av1);

	if (av1->codecDecoderContext)
		avcodec_free_context(&av1->codecDecoderContext);

	if (av1->packet)
		av_packet_free(&av1->packet);

	if (av1->swFrame)
		av_frame_free(&av1->swFrame);

	if (av1->hwFrame)
		av_frame_free(&av1->hwFrame);

	if (av1->hwctx)
		av_buffer_unref(&av1->hwctx);

	av1->codecDecoder = nullptr;
	av1->hw_pix_fmt = AV_PIX_FMT_NONE;
	av1->vaapiFormatRejected = false;
}

static BOOL freerdp_av1_decoder_supports_vaapi(const AVCodec* decoder)
{
	WINPR_ASSERT(decoder);

	for (int x = 0;; x++)
	{
		const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, x);
		if (!config)
			return FALSE;

		if ((config->pix_fmt == AV_PIX_FMT_VAAPI) &&
		    ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0))
			return TRUE;
	}
}

static enum AVPixelFormat freerdp_av1_vaapi_get_format(AVCodecContext* ctx,
                                                       const enum AVPixelFormat* fmts)
{
	WINPR_ASSERT(ctx);

	FREERDP_AV1_CONTEXT* av1 = (FREERDP_AV1_CONTEXT*)ctx->opaque;
	WINPR_ASSERT(av1);

	for (const enum AVPixelFormat* cur = fmts; *cur != AV_PIX_FMT_NONE; cur++)
	{
		if (*cur == av1->hw_pix_fmt)
			return *cur;
	}

	av1->vaapiFormatRejected = true;
	return AV_PIX_FMT_NONE;
}

static void freerdp_av1_vaapi_log_fallback(FREERDP_AV1_CONTEXT* av1, const char* reason)
{
	WINPR_ASSERT(av1);
	WINPR_ASSERT(reason);

	WLog_Print(av1->log, WLOG_WARN, "AV1 VAAPI decoder not available, falling back to libaom: %s",
	           reason);
}

static BOOL freerdp_av1_try_init_vaapi_decoder(FREERDP_AV1_CONTEXT* av1)
{
	WINPR_ASSERT(av1);

	av1->codecDecoder = avcodec_find_decoder_by_name("av1");
	if (!av1->codecDecoder)
		av1->codecDecoder = avcodec_find_decoder(AV_CODEC_ID_AV1);

	if (!av1->codecDecoder)
	{
		freerdp_av1_vaapi_log_fallback(av1, "FFmpeg AV1 decoder not found");
		return FALSE;
	}

	if (!freerdp_av1_decoder_supports_vaapi(av1->codecDecoder))
	{
		freerdp_av1_vaapi_log_fallback(av1, "FFmpeg AV1 decoder has no VAAPI configuration");
		return FALSE;
	}

	const int status =
	    av_hwdevice_ctx_create(&av1->hwctx, AV_HWDEVICE_TYPE_VAAPI, VAAPI_DEVICE, nullptr, 0);
	if (status < 0)
	{
		freerdp_av1_vaapi_log_fallback(av1, av_err2str(status));
		av1->hwctx = nullptr;
		return FALSE;
	}

	av1->hw_pix_fmt = AV_PIX_FMT_VAAPI;
	av1->backend = AV1_BACKEND_LIBVA;
	return TRUE;
}

static BOOL freerdp_av1_vaapi_reset_decoder(FREERDP_AV1_CONTEXT* av1, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(av1);
	WINPR_ASSERT(av1->codecDecoder);
	WINPR_ASSERT(av1->hwctx);

	if ((width > INT_MAX) || (height > INT_MAX))
	{
		freerdp_av1_vaapi_log_fallback(av1, "AV1 frame size exceeds FFmpeg limits");
		return FALSE;
	}

	if (av1->codecDecoderContext)
		avcodec_free_context(&av1->codecDecoderContext);
	if (av1->packet)
		av_packet_free(&av1->packet);
	if (av1->swFrame)
		av_frame_free(&av1->swFrame);
	if (av1->hwFrame)
		av_frame_free(&av1->hwFrame);

	av1->codecDecoderContext = avcodec_alloc_context3(av1->codecDecoder);
	if (!av1->codecDecoderContext)
	{
		freerdp_av1_vaapi_log_fallback(av1, "failed to allocate FFmpeg decoder context");
		return FALSE;
	}

	av1->codecDecoderContext->width = (int)width;
	av1->codecDecoderContext->height = (int)height;
	av1->codecDecoderContext->get_format = freerdp_av1_vaapi_get_format;
	av1->codecDecoderContext->opaque = av1;
	av1->codecDecoderContext->hw_device_ctx = av_buffer_ref(av1->hwctx);
	if (!av1->codecDecoderContext->hw_device_ctx)
	{
		freerdp_av1_vaapi_log_fallback(av1, "failed to reference VAAPI device context");
		return FALSE;
	}

	int status = avcodec_open2(av1->codecDecoderContext, av1->codecDecoder, nullptr);
	if (status < 0)
	{
		freerdp_av1_vaapi_log_fallback(av1, av_err2str(status));
		return FALSE;
	}

	av1->packet = av_packet_alloc();
	av1->hwFrame = av_frame_alloc();
	av1->swFrame = av_frame_alloc();
	if (!av1->packet || !av1->hwFrame || !av1->swFrame)
	{
		freerdp_av1_vaapi_log_fallback(av1, "failed to allocate FFmpeg decode buffers");
		return FALSE;
	}

	av1->vaapiFormatRejected = false;
	WLog_Print(av1->log, WLOG_INFO, "Using VAAPI for accelerated AV1 decoding");
	return TRUE;
}

static BOOL freerdp_av1_copy_nv12_to_yuv420(FREERDP_AV1_CONTEXT* av1, const AVFrame* frame,
                                            UINT32 width, UINT32 height)
{
	WINPR_ASSERT(av1);
	WINPR_ASSERT(frame);

	if (!frame->data[1] || (frame->linesize[1] <= 0))
		return FALSE;

	const UINT32 chromaWidth = (width + 1U) / 2U;
	const UINT32 chromaHeight = (height + 1U) / 2U;

	for (UINT32 y = 0; y < chromaHeight; y++)
	{
		const BYTE* src = &frame->data[1][1ULL * y * (size_t)frame->linesize[1]];
		BYTE* dstU = &av1->yuvdata[1][1ULL * y * av1->yuvStride[1]];
		BYTE* dstV = &av1->yuvdata[2][1ULL * y * av1->yuvStride[2]];

		for (UINT32 x = 0; x < chromaWidth; x++)
		{
			dstU[x] = src[2ULL * x];
			dstV[x] = src[2ULL * x + 1ULL];
		}
	}

	return TRUE;
}

static BOOL freerdp_av1_vaapi_convert_frame(FREERDP_AV1_CONTEXT* av1, const AVFrame* frame,
                                            BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
                                            UINT32 nDstWidth, UINT32 nDstHeight)
{
	WINPR_ASSERT(av1);
	WINPR_ASSERT(frame);
	WINPR_ASSERT(pDstData);

	primitives_t* primitives = primitives_get();
	if (!primitives)
	{
		WLog_Print(av1->log, WLOG_WARN,
		           "AV1 VAAPI output conversion failed, falling back to libaom: primitives_get() "
		           "nullptr");
		return FALSE;
	}

	const prim_size_t roi = { .width = nDstWidth, .height = nDstHeight };
	const BYTE* pSrc[3] = { nullptr, nullptr, nullptr };
	UINT32 strides[3] = { 0 };
	pstatus_t rc = -1;

	switch (frame->format)
	{
		case AV_PIX_FMT_YUV420P:
		case AV_PIX_FMT_YUVJ420P:
			pSrc[0] = frame->data[0];
			pSrc[1] = frame->data[1];
			pSrc[2] = frame->data[2];
			strides[0] = (UINT32)MAX(0, frame->linesize[0]);
			strides[1] = (UINT32)MAX(0, frame->linesize[1]);
			strides[2] = (UINT32)MAX(0, frame->linesize[2]);
			rc = primitives->YUV420ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep, DstFormat,
			                                       &roi);
			break;

		case AV_PIX_FMT_YUV444P:
		case AV_PIX_FMT_YUVJ444P:
			pSrc[0] = frame->data[0];
			pSrc[1] = frame->data[1];
			pSrc[2] = frame->data[2];
			strides[0] = (UINT32)MAX(0, frame->linesize[0]);
			strides[1] = (UINT32)MAX(0, frame->linesize[1]);
			strides[2] = (UINT32)MAX(0, frame->linesize[2]);
			rc = primitives->YUV444ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep, DstFormat,
			                                       &roi);
			break;

		case AV_PIX_FMT_NV12:
			if (!freerdp_av1_copy_nv12_to_yuv420(av1, frame, nDstWidth, nDstHeight))
			{
				WLog_Print(av1->log, WLOG_WARN,
				           "AV1 VAAPI NV12 conversion failed, falling back to libaom");
				return FALSE;
			}

			pSrc[0] = frame->data[0];
			pSrc[1] = av1->yuvdata[1];
			pSrc[2] = av1->yuvdata[2];
			strides[0] = (UINT32)MAX(0, frame->linesize[0]);
			strides[1] = av1->yuvStride[1];
			strides[2] = av1->yuvStride[2];
			rc = primitives->YUV420ToRGB_8u_P3AC4R(pSrc, strides, pDstData, nDstStep, DstFormat,
			                                       &roi);
			break;

		default:
			WLog_Print(av1->log, WLOG_WARN,
			           "AV1 VAAPI decoded pixel format %d unsupported, falling back to libaom",
			           frame->format);
			return FALSE;
	}

	if (rc != PRIMITIVES_SUCCESS)
	{
		WLog_Print(av1->log, WLOG_WARN,
		           "AV1 VAAPI output conversion failed, falling back to libaom: status=%" PRId32,
		           rc);
		return FALSE;
	}

	return TRUE;
}

static BOOL freerdp_av1_vaapi_decompress(FREERDP_AV1_CONTEXT* av1, const BYTE* pSrcData,
                                         UINT32 SrcSize, BYTE* pDstData, DWORD DstFormat,
                                         UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight)
{
	WINPR_ASSERT(av1);
	WINPR_ASSERT(av1->packet);
	WINPR_ASSERT(av1->codecDecoderContext);

	union
	{
		const BYTE* cpv;
		uint8_t* pv;
	} cnv;

	av_packet_unref(av1->packet);
	cnv.cpv = pSrcData;
	av1->packet->data = cnv.pv;
	av1->packet->size = (int)MIN(SrcSize, INT32_MAX);

	int status = avcodec_send_packet(av1->codecDecoderContext, av1->packet);
	if (status < 0)
	{
		WLog_Print(av1->log, WLOG_WARN,
		           "AV1 VAAPI decode failed, falling back to libaom: send_packet=%s",
		           av_err2str(status));
		return FALSE;
	}

	for (;;)
	{
		av_frame_unref(av1->hwFrame);
		status = avcodec_receive_frame(av1->codecDecoderContext, av1->hwFrame);
		if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
			return TRUE;

		if (status < 0)
		{
			if (av1->vaapiFormatRejected)
			{
				WLog_Print(av1->log, WLOG_WARN,
				           "AV1 VAAPI decoder did not offer VAAPI output, falling back to libaom");
			}
			else
			{
				WLog_Print(av1->log, WLOG_WARN,
				           "AV1 VAAPI decode failed, falling back to libaom: receive_frame=%s",
				           av_err2str(status));
			}
			return FALSE;
		}

		const AVFrame* frame = av1->hwFrame;
		if (av1->hwFrame->format == av1->hw_pix_fmt)
		{
			av_frame_unref(av1->swFrame);
			status = av_hwframe_transfer_data(av1->swFrame, av1->hwFrame, 0);
			if (status < 0)
			{
				WLog_Print(av1->log, WLOG_WARN,
				           "AV1 VAAPI frame transfer failed, falling back to libaom: %s",
				           av_err2str(status));
				return FALSE;
			}

			frame = av1->swFrame;
		}

		if (!freerdp_av1_vaapi_convert_frame(av1, frame, pDstData, DstFormat, nDstStep,
		                                     nDstWidth, nDstHeight))
			return FALSE;
	}
}

static BOOL freerdp_av1_fallback_to_aom(FREERDP_AV1_CONTEXT* av1, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(av1);

	freerdp_av1_vaapi_release(av1);
	av1->backend = AV1_BACKEND_FAILED_TO_SOFTWARE;
	av1->initialized = false;

	if (!freerdp_av1_init_aom_decoder(av1))
		return FALSE;

	av1->dcfg.w = width;
	av1->dcfg.h = height;
	av1->dcfg.allow_lowbitdepth = 1;
	const aom_codec_err_t rc = aom_codec_dec_init(&av1->ctx, av1->iface, &av1->dcfg, av1->flags);
	if (rc != AOM_CODEC_OK)
	{
		WLog_Print(av1->log, WLOG_WARN, "aom_codec_dec_init: %s", aom_codec_err_to_string(rc));
		return FALSE;
	}

	av1->initialized = true;
	return TRUE;
}
#endif

BOOL freerdp_av1_context_set_option(FREERDP_AV1_CONTEXT* av1, FREERDP_AV1_CONTEXT_OPTION option,
                                    UINT32 value)
{

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
}

UINT32 freerdp_av1_context_get_option(FREERDP_AV1_CONTEXT* av1, FREERDP_AV1_CONTEXT_OPTION option)
{
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
#if defined(WITH_LIBYUV)
	int rec = -1;
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
	if (rec != 0)
	{
		WLog_Print(av1->log, WLOG_ERROR, "ARGBToI444(): %d", rec);
		return -1;
	}
#else
	pstatus_t rec = -1;
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
	if (rec != 0)
	{
		WLog_Print(av1->log, WLOG_ERROR, "primitives->RGBToI444_8u(): %d", rec);
		return -1;
	}
#endif
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
}

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

#if defined(WITH_LIBVA_AV1_DECODE)
	if (av1->backend == AV1_BACKEND_LIBVA)
	{
		if (freerdp_av1_vaapi_decompress(av1, pSrcData, SrcSize, pDstData, DstFormat, nDstStep,
		                                  nDstWidth, nDstHeight))
			return TRUE;

		if (!freerdp_av1_fallback_to_aom(av1, av1->yuvWidth, av1->yuvHeight))
			return -1;
	}
#endif

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
		const prim_size_t roi = { .width = nDstWidth, .height = nDstHeight };

#if defined(WITH_LIBYUV)
		int rec = -1;

		switch (img->fmt)
		{
			case AOM_IMG_FMT_I420:
				rec = J420ToARGB(img->planes[0], img->stride[0], img->planes[1], img->stride[1],
				                 img->planes[2], img->stride[2], pDstData, nDstStep, nDstWidth,
				                 nDstHeight);
				break;
			case AOM_IMG_FMT_I444:
				rec = J444ToARGB(img->planes[0], img->stride[0], img->planes[1], img->stride[1],
				                 img->planes[2], img->stride[2], pDstData, nDstStep, nDstWidth,
				                 nDstHeight);
				break;
			default:
				WLog_Print(av1->log, WLOG_ERROR, "img->fmt %d not supported", img->fmt);
				return -1;
		}
		if (rec != 0)
		{
			WLog_Print(av1->log, WLOG_ERROR, "I444ToABGR() %d", rec);
			return -1;
		}
#else
		const BYTE* pSrc[] = { img->planes[0], img->planes[1], img->planes[2] };
		const UINT32 strides[] = { img->stride[0], img->stride[1], img->stride[2] };

		primitives_t* primitives = primitives_get();
		if (!primitives)
		{
			WLog_Print(av1->log, WLOG_ERROR, "primitives_get(): nullptr");
			return FALSE;
		}

		pstatus_t rec = -1;
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
		if (rec != 0)
		{
			WLog_Print(av1->log, WLOG_ERROR, "I444ToABGR() %d", rec);
			return -1;
		}
#endif
	}

	return TRUE;
}

BOOL freerdp_av1_context_reset(FREERDP_AV1_CONTEXT* av1, UINT32 width, UINT32 height)
{
	if (av1->encoder)
	{
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
	}
#if defined(WITH_LIBVA_AV1_DECODE)
	else if (av1->backend == AV1_BACKEND_LIBVA)
	{
		if (freerdp_av1_vaapi_reset_decoder(av1, width, height))
			av1->initialized = true;
		else if (!freerdp_av1_fallback_to_aom(av1, width, height))
			return FALSE;
	}
#endif
	else
	{
		if (!freerdp_av1_init_aom_decoder(av1))
			return FALSE;

		if (av1->initialized)
		{
			const aom_codec_err_t destroy = aom_codec_destroy(&av1->ctx);
			av1->initialized = false;
			if (destroy != AOM_CODEC_OK)
			{
				WLog_Print(av1->log, WLOG_WARN, "aom_codec_destroy: %s",
				           aom_codec_err_to_string(destroy));
				return FALSE;
			}
		}

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
	}
	av1->initialized = true;

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

	if (av1->initialized && (av1->backend != AV1_BACKEND_LIBVA))
	{
		const aom_codec_err_t rc = aom_codec_destroy(&av1->ctx);
		if (rc != AOM_CODEC_OK)
		{
			WLog_Print(av1->log, WLOG_WARN, "aom_codec_destroy: %s", aom_codec_err_to_string(rc));
		}
	}
#if defined(WITH_LIBVA_AV1_DECODE)
	freerdp_av1_vaapi_release(av1);
#endif
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
	ctx->backend = AV1_BACKEND_SOFTWARE_AOM;
	ctx->log = WLog_Get(TAG);
	if (!ctx->log)
		goto fail;

	if (Compressor)
	{
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
	}
	else
	{
#if defined(WITH_LIBVA_AV1_DECODE)
		if (freerdp_av1_try_init_vaapi_decoder(ctx))
			return ctx;
#endif
		if (!freerdp_av1_init_aom_decoder(ctx))
			goto fail;
	}

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
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON'");
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
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON'");
	return -1;
}

BOOL freerdp_av1_context_reset(WINPR_ATTR_UNUSED FREERDP_AV1_CONTEXT* av1,
                               WINPR_ATTR_UNUSED UINT32 width, WINPR_ATTR_UNUSED UINT32 height)
{
	WINPR_ASSERT(av1);
	WLog_Print(av1->log, WLOG_WARN,
	           "This build does not support AV1 codec. Recompile with '-DWITH_AOM=ON'");
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
