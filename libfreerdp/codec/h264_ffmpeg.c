/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2015 Marc-André Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
 * Copyright 2014 erbth <t.erbesdobler@team103.com>
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

#include <winpr/wlog.h>
#include <freerdp/log.h>
#include <freerdp/codec/h264.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

#include "h264.h"

#ifdef WITH_VAAPI
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 9, 0)
#include <libavutil/hwcontext.h>
#else
#pragma warning You have asked for VA - API decoding, \
    but your version of libavutil is too old !Disabling.
#undef WITH_VAAPI
#endif
#endif

/* Fallback support for older libavcodec versions */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 59, 100)
#define AV_CODEC_ID_H264 CODEC_ID_H264
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(56, 34, 2)
#define AV_CODEC_FLAG_LOOP_FILTER CODEC_FLAG_LOOP_FILTER
#define AV_CODEC_CAP_TRUNCATED CODEC_CAP_TRUNCATED
#define AV_CODEC_FLAG_TRUNCATED CODEC_FLAG_TRUNCATED
#endif

#if LIBAVUTIL_VERSION_MAJOR < 52
#define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#endif

/* Ubuntu 14.04 ships without the functions provided by avutil,
 * so define error to string methods here. */
#if !defined(av_err2str)
static inline char* error_string(char* errbuf, size_t errbuf_size, int errnum)
{
	av_strerror(errnum, errbuf, errbuf_size);
	return errbuf;
}

#define av_err2str(errnum) error_string((char[64]){ 0 }, 64, errnum)
#endif

#ifdef WITH_VAAPI
#define VAAPI_DEVICE "/dev/dri/renderD128"
#endif

typedef struct
{
	const AVCodec* codecDecoder;
	AVCodecContext* codecDecoderContext;
	const AVCodec* codecEncoder;
	AVCodecContext* codecEncoderContext;
	AVCodecParserContext* codecParser;
	AVFrame* videoFrame;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	AVPacket bufferpacket;
#endif
	AVPacket* packet;
#ifdef WITH_VAAPI
	AVBufferRef* hwctx;
	AVFrame* hwVideoFrame;
	enum AVPixelFormat hw_pix_fmt;
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)
	AVBufferRef* hw_frames_ctx;
#endif
#endif
} H264_CONTEXT_LIBAVCODEC;

static void libavcodec_destroy_encoder(H264_CONTEXT* h264)
{
	H264_CONTEXT_LIBAVCODEC* sys;

	if (!h264 || !h264->subsystem)
		return;

	sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;

	if (sys->codecEncoderContext)
	{
		avcodec_close(sys->codecEncoderContext);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 69, 100)
		avcodec_free_context(&sys->codecEncoderContext);
#else
		av_free(sys->codecEncoderContext);
#endif
	}

	sys->codecEncoder = NULL;
	sys->codecEncoderContext = NULL;
}

static BOOL libavcodec_create_encoder(H264_CONTEXT* h264)
{
	BOOL recreate = FALSE;
	H264_CONTEXT_LIBAVCODEC* sys;

	if (!h264 || !h264->subsystem)
		return FALSE;

	if ((h264->width > INT_MAX) || (h264->height > INT_MAX))
		return FALSE;

	sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	if (!sys)
		return FALSE;
	recreate = !sys->codecEncoder || !sys->codecEncoderContext;

	if (sys->codecEncoderContext)
	{
		if ((sys->codecEncoderContext->width != (int)h264->width) ||
		    (sys->codecEncoderContext->height != (int)h264->height))
			recreate = TRUE;
	}

	if (!recreate)
		return TRUE;

	libavcodec_destroy_encoder(h264);
	sys->codecEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);

	if (!sys->codecEncoder)
		goto EXCEPTION;

	sys->codecEncoderContext = avcodec_alloc_context3(sys->codecEncoder);

	if (!sys->codecEncoderContext)
		goto EXCEPTION;

	switch (h264->RateControlMode)
	{
		case H264_RATECONTROL_VBR:
			sys->codecEncoderContext->bit_rate = h264->BitRate;
			break;

		case H264_RATECONTROL_CQP:
			/* TODO: sys->codecEncoderContext-> = h264->QP; */
			break;

		default:
			break;
	}

	sys->codecEncoderContext->width = (int)MIN(INT32_MAX, h264->width);
	sys->codecEncoderContext->height = (int)MIN(INT32_MAX, h264->height);
	sys->codecEncoderContext->delay = 0;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 13, 100)
	sys->codecEncoderContext->framerate = (AVRational){ h264->FrameRate, 1 };
#endif
	sys->codecEncoderContext->time_base = (AVRational){ 1, h264->FrameRate };
	av_opt_set(sys->codecEncoderContext, "preset", "medium", AV_OPT_SEARCH_CHILDREN);
	av_opt_set(sys->codecEncoderContext, "tune", "zerolatency", AV_OPT_SEARCH_CHILDREN);
	sys->codecEncoderContext->flags |= AV_CODEC_FLAG_LOOP_FILTER;
	sys->codecEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;

	if (avcodec_open2(sys->codecEncoderContext, sys->codecEncoder, NULL) < 0)
		goto EXCEPTION;

	return TRUE;
EXCEPTION:
	libavcodec_destroy_encoder(h264);
	return FALSE;
}

static int libavcodec_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize)
{
	union
	{
		const BYTE* cpv;
		BYTE* pv;
	} cnv;
	int rc = -1;
	int status;
	int gotFrame = 0;
	AVPacket* packet = NULL;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData || (SrcSize == 0));

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	BYTE** pYUVData = h264->pYUVData;
	UINT32* iStride = h264->iStride;

	WINPR_ASSERT(sys);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	packet = &sys->bufferpacket;
	WINPR_ASSERT(packet);
	av_init_packet(packet);
#else
	packet = av_packet_alloc();
#endif
	if (!packet)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate AVPacket");
		goto fail;
	}

	cnv.cpv = pSrcData;
	packet->data = cnv.pv;
	packet->size = (int)MIN(SrcSize, INT32_MAX);

	WINPR_ASSERT(sys->codecDecoderContext);
	/* avcodec_decode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	status = avcodec_send_packet(sys->codecDecoderContext, packet);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to decode video frame (status=%d)", status);
		goto fail;
	}

	sys->videoFrame->format = AV_PIX_FMT_YUV420P;

	do
	{
#ifdef WITH_VAAPI
		status = avcodec_receive_frame(sys->codecDecoderContext,
		                               sys->hwctx ? sys->hwVideoFrame : sys->videoFrame);
#else
		status = avcodec_receive_frame(sys->codecDecoderContext, sys->videoFrame);
#endif
	} while (status == AVERROR(EAGAIN));

	gotFrame = (status == 0);
#else
#ifdef WITH_VAAPI
	status =
	    avcodec_decode_video2(sys->codecDecoderContext,
	                          sys->hwctx ? sys->hwVideoFrame : sys->videoFrame, &gotFrame, packet);
#else
	status = avcodec_decode_video2(sys->codecDecoderContext, sys->videoFrame, &gotFrame, packet);
#endif
#endif
	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to decode video frame (status=%d)", status);
		goto fail;
	}

#ifdef WITH_VAAPI

	if (sys->hwctx)
	{
		if (sys->hwVideoFrame->format == sys->hw_pix_fmt)
		{
			sys->videoFrame->width = sys->hwVideoFrame->width;
			sys->videoFrame->height = sys->hwVideoFrame->height;
			status = av_hwframe_transfer_data(sys->videoFrame, sys->hwVideoFrame, 0);
		}
		else
		{
			status = av_frame_copy(sys->videoFrame, sys->hwVideoFrame);
		}
	}

	gotFrame = (status == 0);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to transfer video frame (status=%d) (%s)", status,
		           av_err2str(status));
		goto fail;
	}

#endif
#if 0
	WLog_Print(h264->log, WLOG_INFO,
	           "libavcodec_decompress: frame decoded (status=%d, gotFrame=%d, width=%d, height=%d, Y=[%p,%d], U=[%p,%d], V=[%p,%d])",
	           status, gotFrame, sys->videoFrame->width, sys->videoFrame->height,
	           (void*) sys->videoFrame->data[0], sys->videoFrame->linesize[0],
	           (void*) sys->videoFrame->data[1], sys->videoFrame->linesize[1],
	           (void*) sys->videoFrame->data[2], sys->videoFrame->linesize[2]);
#endif

	if (gotFrame)
	{
		WINPR_ASSERT(sys->videoFrame);

		pYUVData[0] = sys->videoFrame->data[0];
		pYUVData[1] = sys->videoFrame->data[1];
		pYUVData[2] = sys->videoFrame->data[2];
		iStride[0] = (UINT32)MAX(0, sys->videoFrame->linesize[0]);
		iStride[1] = (UINT32)MAX(0, sys->videoFrame->linesize[1]);
		iStride[2] = (UINT32)MAX(0, sys->videoFrame->linesize[2]);

		rc = 1;
	}
	else
		rc = -2;

fail:
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	av_packet_unref(packet);
#else
	av_packet_free(&packet);
#endif

	return rc;
}

static int libavcodec_compress(H264_CONTEXT* h264, const BYTE** pSrcYuv, const UINT32* pStride,
                               BYTE** ppDstData, UINT32* pDstSize)
{
	union
	{
		const BYTE* cpv;
		uint8_t* pv;
	} cnv;
	int rc = -1;
	int status;
	int gotFrame = 0;

	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (!libavcodec_create_encoder(h264))
		return -1;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
	sys->packet = &sys->bufferpacket;
	av_packet_unref(sys->packet);
	av_init_packet(sys->packet);
#else
	av_packet_free(&sys->packet);
	sys->packet = av_packet_alloc();
#endif
	if (!sys->packet)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate AVPacket");
		goto fail;
	}

	WINPR_ASSERT(sys->packet);
	sys->packet->data = NULL;
	sys->packet->size = 0;

	WINPR_ASSERT(sys->videoFrame);
	WINPR_ASSERT(sys->codecEncoderContext);
	sys->videoFrame->format = sys->codecEncoderContext->pix_fmt;
	sys->videoFrame->width = sys->codecEncoderContext->width;
	sys->videoFrame->height = sys->codecEncoderContext->height;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 48, 100)
	sys->videoFrame->colorspace = AVCOL_SPC_BT709;
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 92, 100)
	sys->videoFrame->chroma_location = AVCHROMA_LOC_LEFT;
#endif
	cnv.cpv = pSrcYuv[0];
	sys->videoFrame->data[0] = cnv.pv;

	cnv.cpv = pSrcYuv[1];
	sys->videoFrame->data[1] = cnv.pv;

	cnv.cpv = pSrcYuv[2];
	sys->videoFrame->data[2] = cnv.pv;

	sys->videoFrame->linesize[0] = (int)pStride[0];
	sys->videoFrame->linesize[1] = (int)pStride[1];
	sys->videoFrame->linesize[2] = (int)pStride[2];
	sys->videoFrame->pts++;
	/* avcodec_encode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	status = avcodec_send_frame(sys->codecEncoderContext, sys->videoFrame);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	status = avcodec_receive_packet(sys->codecEncoderContext, sys->packet);

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	gotFrame = (status == 0);
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 59, 100)

	do
	{
		status = avcodec_encode_video2(sys->codecEncoderContext, sys->packet, sys->videoFrame,
		                               &gotFrame);
	} while ((status >= 0) && (gotFrame == 0));

#else
	sys->packet->size =
	    avpicture_get_size(sys->codecDecoderContext->pix_fmt, sys->codecDecoderContext->width,
	                       sys->codecDecoderContext->height);
	sys->packet->data = av_malloc(sys->packet->size);

	if (!sys->packet->data)
		status = -1;
	else
	{
		status = avcodec_encode_video(sys->codecDecoderContext, sys->packet->data,
		                              sys->packet->size, sys->videoFrame);
	}

#endif

	if (status < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to encode video frame (%s [%d])",
		           av_err2str(status), status);
		goto fail;
	}

	WINPR_ASSERT(sys->packet);
	*ppDstData = sys->packet->data;
	*pDstSize = (UINT32)MAX(0, sys->packet->size);

	if (!gotFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Did not get frame! (%s [%d])", av_err2str(status),
		           status);
		rc = -2;
	}
	else
		rc = 1;
fail:
	return rc;
}

static void libavcodec_uninit(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;

	if (!sys)
		return;

	if (sys->packet)
	{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 133, 100)
		av_packet_unref(sys->packet);
#else
		av_packet_free(&sys->packet);
#endif
	}

	if (sys->videoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->videoFrame);
#else
		av_free(sys->videoFrame);
#endif
	}

#ifdef WITH_VAAPI

	if (sys->hwVideoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->hwVideoFrame);
#else
		av_free(sys->hwVideoFrame);
#endif
	}

	if (sys->hwctx)
		av_buffer_unref(&sys->hwctx);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)

	if (sys->hw_frames_ctx)
		av_buffer_unref(&sys->hw_frames_ctx);

#endif
#endif

	if (sys->codecParser)
		av_parser_close(sys->codecParser);

	if (sys->codecDecoderContext)
	{
		avcodec_close(sys->codecDecoderContext);
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 69, 100)
		avcodec_free_context(&sys->codecDecoderContext);
#else
		av_free(sys->codecDecoderContext);
#endif
	}

	libavcodec_destroy_encoder(h264);
	free(sys);
	h264->pSystemData = NULL;
}

#ifdef WITH_VAAPI
static enum AVPixelFormat libavcodec_get_format(struct AVCodecContext* ctx,
                                                const enum AVPixelFormat* fmts)
{
	WINPR_ASSERT(ctx);

	H264_CONTEXT* h264 = (H264_CONTEXT*)ctx->opaque;
	WINPR_ASSERT(h264);

	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	const enum AVPixelFormat* p;

	for (p = fmts; *p != AV_PIX_FMT_NONE; p++)
	{
		if (*p == sys->hw_pix_fmt)
		{
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 80, 100)
			sys->hw_frames_ctx = av_hwframe_ctx_alloc(sys->hwctx);

			if (!sys->hw_frames_ctx)
			{
				return AV_PIX_FMT_NONE;
			}

			sys->codecDecoderContext->pix_fmt = *p;
			AVHWFramesContext* frames = (AVHWFramesContext*)sys->hw_frames_ctx->data;
			frames->format = *p;
			frames->height = sys->codecDecoderContext->coded_height;
			frames->width = sys->codecDecoderContext->coded_width;
			frames->sw_format =
			    (sys->codecDecoderContext->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ? AV_PIX_FMT_P010
			                                                                  : AV_PIX_FMT_NV12);
			frames->initial_pool_size = 20;

			if (sys->codecDecoderContext->active_thread_type & FF_THREAD_FRAME)
				frames->initial_pool_size += sys->codecDecoderContext->thread_count;

			int err = av_hwframe_ctx_init(sys->hw_frames_ctx);

			if (err < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Could not init hwframes context: %s",
				           av_err2str(err));
				return AV_PIX_FMT_NONE;
			}

			sys->codecDecoderContext->hw_frames_ctx = av_buffer_ref(sys->hw_frames_ctx);
#endif
			return *p;
		}
	}

	return AV_PIX_FMT_NONE;
}
#endif

static BOOL libavcodec_init(H264_CONTEXT* h264)
{
	H264_CONTEXT_LIBAVCODEC* sys;

	WINPR_ASSERT(h264);
	sys = (H264_CONTEXT_LIBAVCODEC*)calloc(1, sizeof(H264_CONTEXT_LIBAVCODEC));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*)sys;

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100)
	avcodec_register_all();
#endif

	if (!h264->Compressor)
	{
		sys->codecDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);

		if (!sys->codecDecoder)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to find libav H.264 codec");
			goto EXCEPTION;
		}

		sys->codecDecoderContext = avcodec_alloc_context3(sys->codecDecoder);

		if (!sys->codecDecoderContext)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav codec context");
			goto EXCEPTION;
		}

		if (sys->codecDecoder->capabilities & AV_CODEC_CAP_TRUNCATED)
		{
			sys->codecDecoderContext->flags |= AV_CODEC_FLAG_TRUNCATED;
		}

#ifdef WITH_VAAPI

		if (!sys->hwctx)
		{
			int ret =
			    av_hwdevice_ctx_create(&sys->hwctx, AV_HWDEVICE_TYPE_VAAPI, VAAPI_DEVICE, NULL, 0);

			if (ret < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR,
				           "Could not initialize hardware decoder, falling back to software: %s",
				           av_err2str(ret));
				sys->hwctx = NULL;
				goto fail_hwdevice_create;
			}
		}

		sys->codecDecoderContext->get_format = libavcodec_get_format;
		sys->hw_pix_fmt = AV_PIX_FMT_VAAPI;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 80, 100)
		sys->codecDecoderContext->hw_device_ctx = av_buffer_ref(sys->hwctx);
#endif
		sys->codecDecoderContext->opaque = (void*)h264;
	fail_hwdevice_create:
#endif

		if (avcodec_open2(sys->codecDecoderContext, sys->codecDecoder, NULL) < 0)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to open libav codec");
			goto EXCEPTION;
		}

		sys->codecParser = av_parser_init(AV_CODEC_ID_H264);

		if (!sys->codecParser)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Failed to initialize libav parser");
			goto EXCEPTION;
		}
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
	sys->videoFrame = av_frame_alloc();
#ifdef WITH_VAAPI
	sys->hwVideoFrame = av_frame_alloc();
#endif
#else
	sys->videoFrame = avcodec_alloc_frame();
#endif

	if (!sys->videoFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav frame");
		goto EXCEPTION;
	}

#ifdef WITH_VAAPI

	if (!sys->hwVideoFrame)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Failed to allocate libav hw frame");
		goto EXCEPTION;
	}

#endif
	sys->videoFrame->pts = 0;
	return TRUE;
EXCEPTION:
	libavcodec_uninit(h264);
	return FALSE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec = { "libavcodec", libavcodec_init,
	                                                    libavcodec_uninit, libavcodec_decompress,
	                                                    libavcodec_compress };
