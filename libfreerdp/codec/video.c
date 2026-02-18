/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Codec Wrappers
 *
 * Copyright 2025 Devolutions Inc.
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

#include <winpr/assert.h>
#include <winpr/wlog.h>
#include <freerdp/log.h>

#include <freerdp/codec/video.h>

#define TAG FREERDP_TAG("codec.video")

#if defined(WITH_SWSCALE)

#include "image_ffmpeg.h"

/* MJPEG decoder only available when NOT using runtime loading */
#if defined(WITH_VIDEO_FFMPEG) && !defined(WITH_SWSCALE_LOADING)
#define WITH_MJPEG_DECODER
#include <libavcodec/avcodec.h>
#endif

struct s_FREERDP_VIDEO_CONTEXT
{
	UINT32 width;
	UINT32 height;
	struct SwsContext* sws;

#if defined(WITH_MJPEG_DECODER)
	AVCodecContext* mjpegDecoder;
	AVPacket* mjpegPacket;
	AVFrame* mjpegFrame;
#endif
};

/**
 * @brief Map FREERDP_VIDEO_FORMAT to AVPixelFormat
 */
static enum AVPixelFormat video_format_to_av(FREERDP_VIDEO_FORMAT format)
{
	switch (format)
	{
		case FREERDP_VIDEO_FORMAT_YUV420P:
			return AV_PIX_FMT_YUV420P;
		case FREERDP_VIDEO_FORMAT_YUYV422:
			return AV_PIX_FMT_YUYV422;
		case FREERDP_VIDEO_FORMAT_RGB24:
			return AV_PIX_FMT_RGB24;
		case FREERDP_VIDEO_FORMAT_BGR24:
			return AV_PIX_FMT_BGR24;
		case FREERDP_VIDEO_FORMAT_YUV422P:
			return AV_PIX_FMT_YUV422P;
		case FREERDP_VIDEO_FORMAT_YUV444P:
			return AV_PIX_FMT_YUV444P;
		case FREERDP_VIDEO_FORMAT_NV12:
			return AV_PIX_FMT_NV12;
		case FREERDP_VIDEO_FORMAT_NV21:
			return AV_PIX_FMT_NV21;
		case FREERDP_VIDEO_FORMAT_ARGB:
			return AV_PIX_FMT_ARGB;
		case FREERDP_VIDEO_FORMAT_RGBA:
			return AV_PIX_FMT_RGBA;
		case FREERDP_VIDEO_FORMAT_ABGR:
			return AV_PIX_FMT_ABGR;
		case FREERDP_VIDEO_FORMAT_BGRA:
			return AV_PIX_FMT_BGRA;
		case FREERDP_VIDEO_FORMAT_YUV411P:
			return AV_PIX_FMT_YUV411P;
		case FREERDP_VIDEO_FORMAT_YUVJ420P:
			return AV_PIX_FMT_YUVJ420P;
		case FREERDP_VIDEO_FORMAT_YUVJ422P:
			return AV_PIX_FMT_YUVJ422P;
		case FREERDP_VIDEO_FORMAT_YUVJ444P:
			return AV_PIX_FMT_YUVJ444P;
		case FREERDP_VIDEO_FORMAT_YUVJ440P:
			return AV_PIX_FMT_YUVJ440P;
		case FREERDP_VIDEO_FORMAT_YUV440P:
			return AV_PIX_FMT_YUV440P;
		case FREERDP_VIDEO_FORMAT_RGB32:
			return AV_PIX_FMT_RGB32;
		default:
			return AV_PIX_FMT_NONE;
	}
}

/**
 * @brief Map AVPixelFormat to FREERDP_VIDEO_FORMAT
 */
static FREERDP_VIDEO_FORMAT av_format_to_video(enum AVPixelFormat format)
{
	/* Handle deprecated JPEG formats */
	switch (format)
	{
		case AV_PIX_FMT_YUVJ411P:
			return FREERDP_VIDEO_FORMAT_YUV411P;
		case AV_PIX_FMT_YUVJ420P:
			return FREERDP_VIDEO_FORMAT_YUV420P;
		case AV_PIX_FMT_YUVJ422P:
			return FREERDP_VIDEO_FORMAT_YUV422P;
		case AV_PIX_FMT_YUVJ440P:
			return FREERDP_VIDEO_FORMAT_YUV440P;
		case AV_PIX_FMT_YUVJ444P:
			return FREERDP_VIDEO_FORMAT_YUV444P;
		default:
			break;
	}

	switch (format)
	{
		case AV_PIX_FMT_YUV420P:
			return FREERDP_VIDEO_FORMAT_YUV420P;
		case AV_PIX_FMT_YUYV422:
			return FREERDP_VIDEO_FORMAT_YUYV422;
		case AV_PIX_FMT_RGB24:
			return FREERDP_VIDEO_FORMAT_RGB24;
		case AV_PIX_FMT_BGR24:
			return FREERDP_VIDEO_FORMAT_BGR24;
		case AV_PIX_FMT_YUV422P:
			return FREERDP_VIDEO_FORMAT_YUV422P;
		case AV_PIX_FMT_YUV444P:
			return FREERDP_VIDEO_FORMAT_YUV444P;
		case AV_PIX_FMT_NV12:
			return FREERDP_VIDEO_FORMAT_NV12;
		case AV_PIX_FMT_NV21:
			return FREERDP_VIDEO_FORMAT_NV21;
		case AV_PIX_FMT_ARGB:
			return FREERDP_VIDEO_FORMAT_ARGB;
		case AV_PIX_FMT_RGBA:
			return FREERDP_VIDEO_FORMAT_RGBA;
		case AV_PIX_FMT_ABGR:
			return FREERDP_VIDEO_FORMAT_ABGR;
		case AV_PIX_FMT_BGRA:
			return FREERDP_VIDEO_FORMAT_BGRA;
		case AV_PIX_FMT_YUV411P:
			return FREERDP_VIDEO_FORMAT_YUV411P;
		case AV_PIX_FMT_YUV440P:
			return FREERDP_VIDEO_FORMAT_YUV440P;
		default:
			return FREERDP_VIDEO_FORMAT_NONE;
	}
}

BOOL freerdp_video_available(void)
{
	return freerdp_swscale_available() && freerdp_avutil_available();
}

BOOL freerdp_video_feature_available(UINT32 features)
{
	if (features & FREERDP_VIDEO_FEATURE_MJPEG_DECODE)
	{
#if !defined(WITH_MJPEG_DECODER)
		return FALSE;
#endif
	}

	if (features & FREERDP_VIDEO_FEATURE_H264_ENCODE)
	{
		/* H264 encoding available if any H264 backend is compiled */
#if !defined(WITH_OPENH264) && !defined(WITH_VIDEO_FFMPEG) && \
    !defined(WITH_MEDIA_FOUNDATION) && !defined(WITH_MEDIACODEC)
		return FALSE;
#endif
	}

	if (features & FREERDP_VIDEO_FEATURE_H264_DECODE)
	{
		/* H264 decoding available if any H264 backend is compiled */
#if !defined(WITH_OPENH264) && !defined(WITH_VIDEO_FFMPEG) && \
    !defined(WITH_MEDIA_FOUNDATION) && !defined(WITH_MEDIACODEC)
		return FALSE;
#endif
	}

	return TRUE;
}

FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(UINT32 width, UINT32 height)
{
	FREERDP_VIDEO_CONTEXT* context = NULL;

	if (!freerdp_video_available())
	{
		WLog_ERR(TAG, "Video codecs not available - FFmpeg not loaded");
		return NULL;
	}

	context = (FREERDP_VIDEO_CONTEXT*)calloc(1, sizeof(FREERDP_VIDEO_CONTEXT));
	if (!context)
		return NULL;

	context->width = width;
	context->height = height;

#if defined(WITH_MJPEG_DECODER)
	/* Initialize MJPEG decoder */
	const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	if (!codec)
	{
		WLog_ERR(TAG, "avcodec_find_decoder failed to find MJPEG codec");
		goto fail;
	}

	context->mjpegDecoder = avcodec_alloc_context3(codec);
	if (!context->mjpegDecoder)
	{
		WLog_ERR(TAG, "avcodec_alloc_context3 failed");
		goto fail;
	}

	context->mjpegDecoder->width = (int)width;
	context->mjpegDecoder->height = (int)height;
	/* Abort on minor errors to skip corrupted frames */
	context->mjpegDecoder->err_recognition |= AV_EF_EXPLODE;

	if (avcodec_open2(context->mjpegDecoder, codec, NULL) < 0)
	{
		WLog_ERR(TAG, "avcodec_open2 failed");
		goto fail;
	}

	context->mjpegPacket = av_packet_alloc();
	if (!context->mjpegPacket)
	{
		WLog_ERR(TAG, "av_packet_alloc failed");
		goto fail;
	}

	context->mjpegFrame = av_frame_alloc();
	if (!context->mjpegFrame)
	{
		WLog_ERR(TAG, "av_frame_alloc failed");
		goto fail;
	}
#endif

	return context;

#if defined(WITH_MJPEG_DECODER)
fail:
	freerdp_video_context_free(context);
	return NULL;
#endif
}

void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context)
{
	if (!context)
		return;

	if (context->sws)
	{
		freerdp_sws_freeContext(context->sws);
		context->sws = NULL;
	}

#if defined(WITH_MJPEG_DECODER)
	if (context->mjpegFrame)
		av_frame_free(&context->mjpegFrame);

	if (context->mjpegPacket)
	{
		context->mjpegPacket->data = NULL;
		context->mjpegPacket->size = 0;
		av_packet_free(&context->mjpegPacket);
	}

	if (context->mjpegDecoder)
		avcodec_free_context(&context->mjpegDecoder);
#endif

	free(context);
}

BOOL freerdp_video_context_reset(FREERDP_VIDEO_CONTEXT* context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	/* Free existing swscale context if dimensions changed */
	if (context->sws && (context->width != width || context->height != height))
	{
		freerdp_sws_freeContext(context->sws);
		context->sws = NULL;
	}

	context->width = width;
	context->height = height;

#if defined(WITH_MJPEG_DECODER)
	if (context->mjpegDecoder)
	{
		context->mjpegDecoder->width = (int)width;
		context->mjpegDecoder->height = (int)height;
	}
#endif

	return TRUE;
}

BOOL freerdp_video_decode_mjpeg(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData,
                                size_t srcSize, BYTE* dstData[4], int dstLineSize[4],
                                FREERDP_VIDEO_FORMAT* dstFormat)
{
#if defined(WITH_MJPEG_DECODER)
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcData);
	WINPR_ASSERT(dstData);
	WINPR_ASSERT(dstLineSize);
	WINPR_ASSERT(dstFormat);

	if (!context->mjpegDecoder)
	{
		WLog_ERR(TAG, "MJPEG decoder not initialized");
		return FALSE;
	}

	context->mjpegPacket->data = WINPR_CAST_CONST_PTR_AWAY(srcData, uint8_t*);
	WINPR_ASSERT(srcSize <= INT32_MAX);
	context->mjpegPacket->size = (int)srcSize;

	if (avcodec_send_packet(context->mjpegDecoder, context->mjpegPacket) < 0)
	{
		WLog_ERR(TAG, "avcodec_send_packet failed");
		return FALSE;
	}

	if (avcodec_receive_frame(context->mjpegDecoder, context->mjpegFrame) < 0)
	{
		WLog_ERR(TAG, "avcodec_receive_frame failed");
		return FALSE;
	}

	/* Copy plane pointers and line sizes */
	for (size_t i = 0; i < 4; i++)
	{
		dstData[i] = context->mjpegFrame->data[i];
		dstLineSize[i] = context->mjpegFrame->linesize[i];
	}

	/* Convert pixel format */
	*dstFormat = av_format_to_video(context->mjpegDecoder->pix_fmt);

	return TRUE;
#else
	WINPR_UNUSED(context);
	WINPR_UNUSED(srcData);
	WINPR_UNUSED(srcSize);
	WINPR_UNUSED(dstData);
	WINPR_UNUSED(dstLineSize);
	WINPR_UNUSED(dstFormat);
	WLog_ERR(TAG, "MJPEG decoder not available (requires direct FFmpeg linking)");
	return FALSE;
#endif
}

BOOL freerdp_video_convert_to_yuv(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData[4],
                                  const int srcLineSize[4], FREERDP_VIDEO_FORMAT srcFormat,
                                  BYTE* dstData[3], const int dstLineSize[3],
                                  FREERDP_VIDEO_FORMAT dstFormat, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(srcData);
	WINPR_ASSERT(srcLineSize);
	WINPR_ASSERT(dstData);
	WINPR_ASSERT(dstLineSize);

	if (!freerdp_swscale_available())
	{
		WLog_ERR(TAG, "swscale not available - install FFmpeg to enable video processing");
		return FALSE;
	}

	enum AVPixelFormat srcPixFmt = video_format_to_av(srcFormat);
	enum AVPixelFormat dstPixFmt = video_format_to_av(dstFormat);

	if (srcPixFmt == AV_PIX_FMT_NONE || dstPixFmt == AV_PIX_FMT_NONE)
	{
		WLog_ERR(TAG, "Unsupported pixel format");
		return FALSE;
	}

	/* Create or reuse swscale context */
	struct SwsContext* sws = NULL;
	BOOL needFree = FALSE;

	if (context && context->sws)
	{
		sws = context->sws;
	}
	else
	{
		sws = freerdp_sws_getContext((int)width, (int)height, srcPixFmt, (int)width,
		                             (int)height, dstPixFmt, 0, NULL, NULL, NULL);
		if (!sws)
		{
			WLog_ERR(TAG, "sws_getContext failed");
			return FALSE;
		}

		if (context)
			context->sws = sws;
		else
			needFree = TRUE;
	}

	/* Perform conversion */
	const BYTE* cSrcData[4] = { srcData[0], srcData[1], srcData[2], srcData[3] };

	/* sws_scale expects 4-element arrays, but caller may provide 3-element arrays for YUV */
	uint8_t* localDstData[4] = { dstData[0], dstData[1], dstData[2], NULL };
	int localDstLineSize[4] = { dstLineSize[0], dstLineSize[1], dstLineSize[2], 0 };

	int result = 0;

	result = freerdp_sws_scale(sws, cSrcData, srcLineSize, 0, (int)height,
	                           localDstData, localDstLineSize);

	if (needFree)
	{
		freerdp_sws_freeContext(sws);
	}

	return (result > 0);
}

BOOL freerdp_video_fill_plane_info(BYTE* data[4], int lineSize[4], FREERDP_VIDEO_FORMAT format,
                                   UINT32 width, UINT32 height, const BYTE* buffer)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(lineSize);

	enum AVPixelFormat pixFmt = video_format_to_av(format);
	if (pixFmt == AV_PIX_FMT_NONE)
	{
		WLog_ERR(TAG, "Unsupported pixel format");
		return FALSE;
	}

	if (!freerdp_avutil_available())
	{
		WLog_ERR(TAG, "avutil not available");
		return FALSE;
	}

	if (freerdp_av_image_fill_linesizes(lineSize, pixFmt, (int)width) < 0)
	{
		WLog_ERR(TAG, "av_image_fill_linesizes failed");
		return FALSE;
	}

	if (freerdp_av_image_fill_pointers(data, pixFmt, (int)height,
	                                    WINPR_CAST_CONST_PTR_AWAY(buffer, BYTE*), lineSize) < 0)
	{
		WLog_ERR(TAG, "av_image_fill_pointers failed");
		return FALSE;
	}

	return TRUE;
}

#else /* !WITH_SWSCALE */

/* Stubs when swscale is not available */

BOOL freerdp_video_available(void)
{
	return FALSE;
}

BOOL freerdp_video_feature_available(UINT32 features)
{
	WINPR_UNUSED(features);
	return FALSE;
}

FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(UINT32 width, UINT32 height)
{
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	return NULL;
}

void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context)
{
	WINPR_UNUSED(context);
}

BOOL freerdp_video_context_reset(FREERDP_VIDEO_CONTEXT* context, UINT32 width, UINT32 height)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	return FALSE;
}

BOOL freerdp_video_decode_mjpeg(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData,
                                size_t srcSize, BYTE* dstData[4], int dstLineSize[4],
                                FREERDP_VIDEO_FORMAT* dstFormat)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(srcData);
	WINPR_UNUSED(srcSize);
	WINPR_UNUSED(dstData);
	WINPR_UNUSED(dstLineSize);
	WINPR_UNUSED(dstFormat);
	return FALSE;
}

BOOL freerdp_video_convert_to_yuv(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData[4],
                                  const int srcLineSize[4], FREERDP_VIDEO_FORMAT srcFormat,
                                  BYTE* dstData[3], const int dstLineSize[3],
                                  FREERDP_VIDEO_FORMAT dstFormat, UINT32 width, UINT32 height)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(srcData);
	WINPR_UNUSED(srcLineSize);
	WINPR_UNUSED(srcFormat);
	WINPR_UNUSED(dstData);
	WINPR_UNUSED(dstLineSize);
	WINPR_UNUSED(dstFormat);
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	return FALSE;
}

BOOL freerdp_video_fill_plane_info(BYTE* data[4], int lineSize[4], FREERDP_VIDEO_FORMAT format,
                                   UINT32 width, UINT32 height, const BYTE* buffer)
{
	WINPR_UNUSED(data);
	WINPR_UNUSED(lineSize);
	WINPR_UNUSED(format);
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	WINPR_UNUSED(buffer);
	return FALSE;
}

#endif /* WITH_SWSCALE */
