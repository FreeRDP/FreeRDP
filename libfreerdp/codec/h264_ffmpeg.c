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
#include <winpr/wlog.h>
#include <freerdp/log.h>
#include <freerdp/codec/h264.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/mem.h>

#define TAG FREERDP_TAG("codec")

/* Fallback support for older libavcodec versions */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 59, 100)
#define AV_CODEC_ID_H264 CODEC_ID_H264
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

#define av_err2str(errnum) \
	error_string((char[64]){0}, 64, errnum)
#endif

struct _H264_CONTEXT_LIBAVCODEC
{
	AVCodec* codecDecoder;
	AVCodecContext* codecDecoderContext;
	AVCodec* codecEncoder;
	AVCodecContext* codecEncoderContext;
	AVCodecParserContext* codecParser;
	AVFrame* videoFrame;
	AVPacket packet;
};
typedef struct _H264_CONTEXT_LIBAVCODEC H264_CONTEXT_LIBAVCODEC;

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

	sys = (H264_CONTEXT_LIBAVCODEC*)h264->pSystemData;
	recreate = !sys->codecEncoder || !sys->codecEncoderContext;

	if (sys->codecEncoderContext)
	{
		if ((sys->codecEncoderContext->width != h264->width) ||
		    (sys->codecEncoderContext->height != h264->height))
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

	sys->codecEncoderContext->width = h264->width;
	sys->codecEncoderContext->height = h264->height;
	sys->codecEncoderContext->delay = 0;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(56, 13, 100)
	sys->codecEncoderContext->framerate = (AVRational)
	{
		h264->FrameRate, 1
	};
#endif
	sys->codecEncoderContext->time_base = (AVRational)
	{
		1, h264->FrameRate
	};
	av_opt_set(sys->codecEncoderContext, "preset", "veryfast", AV_OPT_SEARCH_CHILDREN);
	av_opt_set(sys->codecEncoderContext, "tune", "zerolatency", AV_OPT_SEARCH_CHILDREN);
	sys->codecEncoderContext->flags |= CODEC_FLAG_LOOP_FILTER;
	sys->codecEncoderContext->me_cmp |= 1;
	sys->codecEncoderContext->me_subpel_quality = 3;
	sys->codecEncoderContext->me_range = 16;
	sys->codecEncoderContext->gop_size = 60;
	sys->codecEncoderContext->keyint_min = 25;
	sys->codecEncoderContext->i_quant_factor = 0.71;
	sys->codecEncoderContext->qcompress = 0.6;
	sys->codecEncoderContext->qmin = 18;
	sys->codecEncoderContext->qmax = 51;
	sys->codecEncoderContext->max_qdiff = 4;
	sys->codecEncoderContext->max_b_frames = 0;
	sys->codecEncoderContext->refs = 1;
	sys->codecEncoderContext->trellis = 0;
	sys->codecEncoderContext->thread_count = 2;
	sys->codecEncoderContext->level = 31;
	sys->codecEncoderContext->b_quant_factor = 0;
	sys->codecEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;

	if (avcodec_open2(sys->codecEncoderContext, sys->codecEncoder, NULL) < 0)
		goto EXCEPTION;

	return TRUE;
EXCEPTION:
	libavcodec_destroy_encoder(h264);
	return FALSE;
}

static int libavcodec_decompress(H264_CONTEXT* h264, const BYTE* pSrcData,
                                 UINT32 SrcSize)
{
	int status;
	int gotFrame = 0;
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*) h264->pSystemData;
	BYTE** pYUVData = h264->pYUVData;
	UINT32* iStride = h264->iStride;
	av_init_packet(&sys->packet);
	sys->packet.data = (BYTE*)pSrcData;
	sys->packet.size = SrcSize;
	/* avcodec_decode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	status = avcodec_send_packet(sys->codecDecoderContext, &sys->packet);

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to decode video frame (status=%d)", status);
		return -1;
	}

	do
	{
		status = avcodec_receive_frame(sys->codecDecoderContext, sys->videoFrame);
	}
	while (status == AVERROR(EAGAIN));

	gotFrame = (status == 0);
#else
	status = avcodec_decode_video2(sys->codecDecoderContext, sys->videoFrame, &gotFrame,
	                               &sys->packet);
#endif

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to decode video frame (status=%d)", status);
		return -1;
	}

#if 0
	WLog_INFO(TAG,
	          "libavcodec_decompress: frame decoded (status=%d, gotFrame=%d, width=%d, height=%d, Y=[%p,%d], U=[%p,%d], V=[%p,%d])",
	          status, gotFrame, sys->videoFrame->width, sys->videoFrame->height,
	          (void*) sys->videoFrame->data[0], sys->videoFrame->linesize[0],
	          (void*) sys->videoFrame->data[1], sys->videoFrame->linesize[1],
	          (void*) sys->videoFrame->data[2], sys->videoFrame->linesize[2]);
#endif

	if (gotFrame)
	{
		pYUVData[0] = sys->videoFrame->data[0];
		pYUVData[1] = sys->videoFrame->data[1];
		pYUVData[2] = sys->videoFrame->data[2];
		iStride[0] = sys->videoFrame->linesize[0];
		iStride[1] = sys->videoFrame->linesize[1];
		iStride[2] = sys->videoFrame->linesize[2];
		h264->width = sys->videoFrame->width;
		h264->height = sys->videoFrame->height;
	}
	else
		return -2;

	return 1;
}

static int libavcodec_compress(H264_CONTEXT* h264, BYTE** ppDstData, UINT32* pDstSize)
{
	int status;
	int gotFrame = 0;
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*) h264->pSystemData;

	if (!libavcodec_create_encoder(h264))
		return -1;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 39, 100)
	av_packet_unref(&sys->packet);
#else
	av_free(sys->packet.data);
#endif
	av_init_packet(&sys->packet);
	sys->packet.data = NULL;
	sys->packet.size = 0;
	sys->videoFrame->format = sys->codecEncoderContext->pix_fmt;
	sys->videoFrame->width  = sys->codecEncoderContext->width;
	sys->videoFrame->height = sys->codecEncoderContext->height;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 48, 100)
	sys->videoFrame->colorspace = AVCOL_SPC_BT709;
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 92, 100)
	sys->videoFrame->chroma_location = AVCHROMA_LOC_LEFT;
#endif
	sys->videoFrame->data[0] = h264->pYUVData[0];
	sys->videoFrame->data[1] = h264->pYUVData[1];
	sys->videoFrame->data[2] = h264->pYUVData[2];
	sys->videoFrame->linesize[0] = h264->iStride[0];
	sys->videoFrame->linesize[1] = h264->iStride[1];
	sys->videoFrame->linesize[2] = h264->iStride[2];
	sys->videoFrame->pts++;
	/* avcodec_encode_video2 is deprecated with libavcodec 57.48.101 */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 101)
	status = avcodec_send_frame(sys->codecEncoderContext, sys->videoFrame);

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to encode video frame (%s [%d])",
		         av_err2str(status), status);
		return -1;
	}

	status = avcodec_receive_packet(sys->codecEncoderContext,
	                                &sys->packet);

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to encode video frame (%s [%d])",
		         av_err2str(status), status);
		return -1;
	}

	gotFrame = (status == 0);
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54, 59, 100)

	do
	{
		status = avcodec_encode_video2(sys->codecEncoderContext,
		                               &sys->packet,
		                               sys->videoFrame, &gotFrame);
	}
	while ((status >= 0) && (gotFrame == 0));

#else
	sys->packet.size = avpicture_get_size(sys->codecDecoderContext->pix_fmt,
	                                      sys->codecDecoderContext->width,
	                                      sys->codecDecoderContext->height);
	sys->packet.data = av_malloc(sys->packet.size);

	if (!sys->packet.data)
		status = -1;
	else
	{
		status = avcodec_encode_video(sys->codecDecoderContext, sys->packet.data,
		                              sys->packet.size, sys->videoFrame);
	}

#endif

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to encode video frame (%s [%d])",
		         av_err2str(status), status);
		return -1;
	}

	*ppDstData = sys->packet.data;
	*pDstSize = sys->packet.size;

	if (!gotFrame)
	{
		WLog_ERR(TAG, "Did not get frame! (%s [%d])", av_err2str(status), status);
		return -2;
	}

	return 1;
}

static void libavcodec_uninit(H264_CONTEXT* h264)
{
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*) h264->pSystemData;

	if (!sys)
		return;

	if (sys->videoFrame)
	{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
		av_frame_free(&sys->videoFrame);
#else
		av_free(sys->videoFrame);
#endif
	}

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

static BOOL libavcodec_init(H264_CONTEXT* h264)
{
	H264_CONTEXT_LIBAVCODEC* sys;
	sys = (H264_CONTEXT_LIBAVCODEC*) calloc(1, sizeof(H264_CONTEXT_LIBAVCODEC));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*) sys;
	avcodec_register_all();

	if (!h264->Compressor)
	{
		sys->codecDecoder = avcodec_find_decoder(AV_CODEC_ID_H264);

		if (!sys->codecDecoder)
		{
			WLog_ERR(TAG, "Failed to find libav H.264 codec");
			goto EXCEPTION;
		}

		sys->codecDecoderContext = avcodec_alloc_context3(sys->codecDecoder);

		if (!sys->codecDecoderContext)
		{
			WLog_ERR(TAG, "Failed to allocate libav codec context");
			goto EXCEPTION;
		}

		if (sys->codecDecoder->capabilities & CODEC_CAP_TRUNCATED)
		{
			sys->codecDecoderContext->flags |= CODEC_FLAG_TRUNCATED;
		}

		if (avcodec_open2(sys->codecDecoderContext, sys->codecDecoder, NULL) < 0)
		{
			WLog_ERR(TAG, "Failed to open libav codec");
			goto EXCEPTION;
		}

		sys->codecParser = av_parser_init(AV_CODEC_ID_H264);

		if (!sys->codecParser)
		{
			WLog_ERR(TAG, "Failed to initialize libav parser");
			goto EXCEPTION;
		}
	}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(55, 18, 102)
	sys->videoFrame = av_frame_alloc();
#else
	sys->videoFrame = avcodec_alloc_frame();
#endif

	if (!sys->videoFrame)
	{
		WLog_ERR(TAG, "Failed to allocate libav frame");
		goto EXCEPTION;
	}

	sys->videoFrame->pts = 0;
	return TRUE;
EXCEPTION:
	libavcodec_uninit(h264);
	return FALSE;
}

H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec =
{
	"libavcodec",
	libavcodec_init,
	libavcodec_uninit,
	libavcodec_decompress,
	libavcodec_compress
};
