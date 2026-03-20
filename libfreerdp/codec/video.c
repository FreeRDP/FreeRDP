/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Codec Wrappers
 *
 * Copyright 2026 Devolutions Inc.
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
#include <winpr/stream.h>
#include <freerdp/log.h>

#include <freerdp/codec/video.h>
#include <freerdp/codec/h264.h>

#define TAG FREERDP_TAG("codec.video")

WINPR_ATTR_NODISCARD
static const char* freerdp_video_format_string(UINT32 format);

#if defined(WITH_SWSCALE)

/* Forward declarations for static functions */
static BOOL freerdp_video_fill_plane_info(BYTE* data[4], int lineSize[4],
                                          FREERDP_VIDEO_FORMAT format, UINT32 width, UINT32 height,
                                          const BYTE* buffer);

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

	H264_CONTEXT* h264;
	BOOL h264Configured;
	UINT32 h264Framerate;
	UINT32 h264Bitrate;
	UINT32 h264UsageType;
};

/**
 * @brief Map FREERDP_VIDEO_FORMAT to AVPixelFormat
 */
static enum AVPixelFormat video_format_to_av(FREERDP_VIDEO_FORMAT format)
{
	switch (format)
	{
		/* Compressed formats - not pixel formats */
		case FREERDP_VIDEO_FORMAT_MJPEG:
		case FREERDP_VIDEO_FORMAT_H264:
			return AV_PIX_FMT_NONE;

		/* Planar YUV formats */
		case FREERDP_VIDEO_FORMAT_YUV420P:
			return AV_PIX_FMT_YUV420P;
		case FREERDP_VIDEO_FORMAT_NV12:
			return AV_PIX_FMT_NV12;

		/* Packed YUV formats */
		case FREERDP_VIDEO_FORMAT_YUYV422:
			return AV_PIX_FMT_YUYV422;

		/* RGB formats */
		case FREERDP_VIDEO_FORMAT_RGB24:
			return AV_PIX_FMT_RGB24;
		case FREERDP_VIDEO_FORMAT_RGB32:
			return AV_PIX_FMT_RGB32;

		case FREERDP_VIDEO_FORMAT_NONE:
		default:
			return AV_PIX_FMT_NONE;
	}
}

#if defined(WITH_MJPEG_DECODER)
/**
 * @brief Map AVPixelFormat to FREERDP_VIDEO_FORMAT
 */
static FREERDP_VIDEO_FORMAT av_format_to_video(enum AVPixelFormat format)
{
	switch (format)
	{
		/* Planar YUV formats */
		case AV_PIX_FMT_YUV420P:
			return FREERDP_VIDEO_FORMAT_YUV420P;

		case AV_PIX_FMT_NV12:
			return FREERDP_VIDEO_FORMAT_NV12;

		/* Packed YUV formats */
		case AV_PIX_FMT_YUYV422:
			return FREERDP_VIDEO_FORMAT_YUYV422;

		/* RGB formats */
		case AV_PIX_FMT_RGB24:
			return FREERDP_VIDEO_FORMAT_RGB24;

		case AV_PIX_FMT_RGB32:
			return FREERDP_VIDEO_FORMAT_RGB32;

		default:
			return FREERDP_VIDEO_FORMAT_NONE;
	}
}
#endif

BOOL freerdp_video_available(void)
{
	return freerdp_swscale_available() && freerdp_avutil_available();
}

FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(UINT32 width, UINT32 height)
{
	FREERDP_VIDEO_CONTEXT* context = nullptr;

	if (!freerdp_video_available())
	{
		WLog_ERR(TAG, "Video codecs not available - FFmpeg not loaded");
		return nullptr;
	}

	context = (FREERDP_VIDEO_CONTEXT*)calloc(1, sizeof(FREERDP_VIDEO_CONTEXT));
	if (!context)
		return nullptr;

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

	if (avcodec_open2(context->mjpegDecoder, codec, nullptr) < 0)
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
	return nullptr;
#endif
}

void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context)
{
	if (!context)
		return;

	if (context->sws)
	{
		freerdp_sws_freeContext(context->sws);
		context->sws = nullptr;
	}

#if defined(WITH_MJPEG_DECODER)
	if (context->mjpegFrame)
		av_frame_free(&context->mjpegFrame);

	if (context->mjpegPacket)
	{
		context->mjpegPacket->data = nullptr;
		context->mjpegPacket->size = 0;
		av_packet_free(&context->mjpegPacket);
	}

	if (context->mjpegDecoder)
		avcodec_free_context(&context->mjpegDecoder);
#endif

	if (context->h264)
	{
		h264_context_free(context->h264);
		context->h264 = nullptr;
	}

	free(context);
}

static UINT32 video_get_h264_bitrate(UINT32 height)
{
	static struct
	{
		UINT32 height;
		UINT32 bitrate;
	} bitrates[] = {
		{ 1080, 2700 }, { 720, 1250 }, { 480, 700 }, { 360, 400 },
		{ 240, 170 },   { 180, 140 },  { 0, 100 },
	};
	const size_t nBitrates = ARRAYSIZE(bitrates);

	for (size_t i = 0; i < nBitrates; i++)
	{
		if (height >= bitrates[i].height)
		{
			UINT32 bitrate = bitrates[i].bitrate;
			WLog_DBG(TAG, "Auto-calculated H.264 bitrate: %u kbps", bitrate);
			return bitrate * 1000;
		}
	}

	WINPR_ASSERT(FALSE);
	return 0;
}

BOOL freerdp_video_context_reconfigure(FREERDP_VIDEO_CONTEXT* context, UINT32 width, UINT32 height,
                                       UINT32 framerate, UINT32 bitrate, UINT32 usageType)
{
	WINPR_ASSERT(context);

	if (width == 0 || height == 0)
	{
		WLog_ERR(TAG, "Invalid dimensions: %ux%u", width, height);
		return FALSE;
	}

	if (bitrate == 0)
		bitrate = video_get_h264_bitrate(height);

	if (!context->h264)
	{
		context->h264 = h264_context_new(TRUE);
		if (!context->h264)
		{
			WLog_ERR(TAG, "h264_context_new failed");
			return FALSE;
		}
	}

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_USAGETYPE, usageType))
		goto fail;

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_FRAMERATE, framerate))
		goto fail;

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_BITRATE, bitrate))
		goto fail;

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_RATECONTROL,
	                             H264_RATECONTROL_CQP))
		goto fail;

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_QP, 26))
		goto fail;

	if (!h264_context_set_option(context->h264, H264_CONTEXT_OPTION_HW_ACCEL, FALSE))
		goto fail;

	if (!context->h264Configured || (context->width != width) || (context->height != height))
	{
		if (!h264_context_reset(context->h264, width, height))
		{
			WLog_ERR(TAG, "h264_context_reset failed");
			goto fail;
		}
	}

	context->h264Framerate = framerate;
	context->h264Bitrate = bitrate;
	context->h264UsageType = usageType;
	context->h264Configured = TRUE;
	context->width = width;
	context->height = height;

	return TRUE;

fail:
	if (context->h264)
	{
		h264_context_free(context->h264);
		context->h264 = nullptr;
	}
	return FALSE;
}

static BOOL freerdp_video_decode_mjpeg(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData,
                                       size_t srcSize, BYTE* dstData[4], int dstLineSize[4],
                                       FREERDP_VIDEO_FORMAT* dstFormat);

static BOOL freerdp_video_convert_to_yuv(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData[4],
                                         const int srcLineSize[4], FREERDP_VIDEO_FORMAT srcFormat,
                                         BYTE* dstData[3], const int dstLineSize[3],
                                         FREERDP_VIDEO_FORMAT dstFormat, UINT32 width,
                                         UINT32 height);

BOOL freerdp_video_sample_convert(FREERDP_VIDEO_CONTEXT* context, FREERDP_VIDEO_FORMAT srcFormat,
                                  const void* srcSampleData, size_t srcSampleLength,
                                  FREERDP_VIDEO_FORMAT dstFormat, wStream* output)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(srcSampleData);
	WINPR_ASSERT(output);

	if (srcFormat == dstFormat)
	{
		if (!Stream_EnsureRemainingCapacity(output, srcSampleLength))
			return FALSE;
		Stream_Write(output, srcSampleData, srcSampleLength);
		return TRUE;
	}

	if (!freerdp_video_available())
	{
		WLog_ERR(TAG, "Video codecs not available");
		return FALSE;
	}

	if (srcSampleLength == 0)
	{
		WLog_ERR(TAG, "Invalid source sample length: 0");
		return FALSE;
	}

	if (!freerdp_video_conversion_supported(srcFormat, dstFormat))
	{
		WLog_ERR(TAG, "Conversion from format %u to %u not supported", srcFormat, dstFormat);
		return FALSE;
	}

	const BOOL srcCompressed =
	    (srcFormat == FREERDP_VIDEO_FORMAT_MJPEG || srcFormat == FREERDP_VIDEO_FORMAT_H264);
	const BOOL dstCompressed =
	    (dstFormat == FREERDP_VIDEO_FORMAT_MJPEG || dstFormat == FREERDP_VIDEO_FORMAT_H264);

	BYTE* intermediate_data[4] = WINPR_C_ARRAY_INIT;
	int intermediate_linesize[4] = WINPR_C_ARRAY_INIT;
	FREERDP_VIDEO_FORMAT intermediate_format = FREERDP_VIDEO_FORMAT_NONE;

	if (srcCompressed)
	{
		if (srcFormat == FREERDP_VIDEO_FORMAT_MJPEG)
		{
			if (!freerdp_video_decode_mjpeg(context, (const BYTE*)srcSampleData, srcSampleLength,
			                                intermediate_data, intermediate_linesize,
			                                &intermediate_format))
			{
				WLog_ERR(TAG, "MJPEG decoding failed");
				return FALSE;
			}
		}
		else if (srcFormat == FREERDP_VIDEO_FORMAT_H264)
		{
			WLog_ERR(TAG, "H264 decoding not supported");
			return FALSE;
		}
	}
	else
	{
		intermediate_format = srcFormat;
	}

	if (dstFormat == FREERDP_VIDEO_FORMAT_H264)
	{
		if (!context->h264Configured)
		{
			WLog_ERR(TAG, "H264 encoder not configured");
			return FALSE;
		}

		const UINT32 hwAccel = h264_context_get_option(context->h264, H264_CONTEXT_OPTION_HW_ACCEL);
		const FREERDP_VIDEO_FORMAT yuvFormat =
		    hwAccel ? FREERDP_VIDEO_FORMAT_NV12 : FREERDP_VIDEO_FORMAT_YUV420P;

		BYTE* yuvData[3] = WINPR_C_ARRAY_INIT;
		UINT32 yuvStrides[3] = WINPR_C_ARRAY_INIT;

		if (h264_get_yuv_buffer(context->h264, 0, context->width, context->height, yuvData,
		                        yuvStrides) < 0)
		{
			WLog_ERR(TAG, "h264_get_yuv_buffer failed");
			return FALSE;
		}

		int yuvLineSizes[4] = { (int)yuvStrides[0], (int)yuvStrides[1], (int)yuvStrides[2], 0 };

		if (srcCompressed)
		{
			const BYTE* cIntermediateData[4] = { intermediate_data[0], intermediate_data[1],
				                                 intermediate_data[2], intermediate_data[3] };
			if (!freerdp_video_convert_to_yuv(context, cIntermediateData, intermediate_linesize,
			                                  intermediate_format, yuvData, yuvLineSizes, yuvFormat,
			                                  context->width, context->height))
			{
				WLog_ERR(TAG, "YUV conversion failed");
				return FALSE;
			}
		}
		else
		{
			BYTE* srcPlanes[4] = WINPR_C_ARRAY_INIT;
			int srcStrides[4] = WINPR_C_ARRAY_INIT;

			if (!freerdp_video_fill_plane_info(srcPlanes, srcStrides, srcFormat, context->width,
			                                   context->height, (const BYTE*)srcSampleData))
			{
				WLog_ERR(TAG, "Failed to fill plane info");
				return FALSE;
			}

			const BYTE* cSrcPlanes[4] = { srcPlanes[0], srcPlanes[1], srcPlanes[2], srcPlanes[3] };
			if (!freerdp_video_convert_to_yuv(context, cSrcPlanes, srcStrides, srcFormat, yuvData,
			                                  yuvLineSizes, yuvFormat, context->width,
			                                  context->height))
			{
				WLog_ERR(TAG, "YUV conversion failed");
				return FALSE;
			}
		}

		BYTE* h264Data = nullptr;
		UINT32 h264Size = 0;

		if (h264_compress(context->h264, &h264Data, &h264Size) < 0)
		{
			WLog_ERR(TAG, "H264 compression failed");
			return FALSE;
		}

		if (!Stream_EnsureRemainingCapacity(output, h264Size))
		{
			WLog_ERR(TAG, "Failed to ensure stream capacity");
			return FALSE;
		}

		Stream_Write(output, h264Data, h264Size);
		return TRUE;
	}
	else if (dstCompressed)
	{
		WLog_ERR(TAG, "Only H264 encoding is supported");
		return FALSE;
	}

	WLog_ERR(TAG, "Raw format output not yet implemented");
	return FALSE;
}

static BOOL freerdp_video_decode_mjpeg(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData,
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

static BOOL freerdp_video_convert_to_yuv(FREERDP_VIDEO_CONTEXT* context, const BYTE* srcData[4],
                                         const int srcLineSize[4], FREERDP_VIDEO_FORMAT srcFormat,
                                         BYTE* dstData[3], const int dstLineSize[3],
                                         FREERDP_VIDEO_FORMAT dstFormat, UINT32 width,
                                         UINT32 height)
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
	struct SwsContext* sws = nullptr;
	BOOL needFree = FALSE;

	if (context && context->sws)
	{
		sws = context->sws;
	}
	else
	{
		sws = freerdp_sws_getContext((int)width, (int)height, srcPixFmt, (int)width, (int)height,
		                             dstPixFmt, 0, nullptr, nullptr, nullptr);
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
	uint8_t* localDstData[4] = { dstData[0], dstData[1], dstData[2], nullptr };
	int localDstLineSize[4] = { dstLineSize[0], dstLineSize[1], dstLineSize[2], 0 };

	const int result = freerdp_sws_scale(sws, cSrcData, srcLineSize, 0, (int)height, localDstData,
	                                     localDstLineSize);

	if (needFree)
	{
		freerdp_sws_freeContext(sws);
	}

	return (result > 0);
}

static BOOL freerdp_video_fill_plane_info(BYTE* data[4], int lineSize[4],
                                          FREERDP_VIDEO_FORMAT format, UINT32 width, UINT32 height,
                                          const BYTE* buffer)
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

BOOL freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT srcFormat,
                                        FREERDP_VIDEO_FORMAT dstFormat)
{
	if (srcFormat == dstFormat)
		return TRUE;

	if (!freerdp_video_available())
		return FALSE;

	if (srcFormat == FREERDP_VIDEO_FORMAT_MJPEG)
	{
#if !defined(WITH_MJPEG_DECODER)
		return FALSE;
#endif
	}
	else if (srcFormat == FREERDP_VIDEO_FORMAT_H264)
	{
		return FALSE;
	}

	if (dstFormat == FREERDP_VIDEO_FORMAT_MJPEG)
	{
		return FALSE;
	}
	else if (dstFormat == FREERDP_VIDEO_FORMAT_H264)
	{
#if !defined(WITH_OPENH264) && !defined(WITH_VIDEO_FFMPEG) && !defined(WITH_MEDIA_FOUNDATION) && \
    !defined(WITH_MEDIACODEC)
		return FALSE;
#endif
	}

	return TRUE;
}

#else /* !WITH_SWSCALE */

/* Stubs when swscale is not available */

BOOL freerdp_video_available(void)
{
	return FALSE;
}

FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(WINPR_ATTR_UNUSED UINT32 width,
                                                 WINPR_ATTR_UNUSED UINT32 height)
{
	WINPR_UNUSED(width);
	WINPR_UNUSED(height);
	return calloc(1, sizeof(char));
}

void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context)
{
	free(context);
}

BOOL freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT srcFormat,
                                        FREERDP_VIDEO_FORMAT dstFormat)
{
	if (srcFormat == dstFormat)
		return TRUE;
	return FALSE;
}

BOOL freerdp_video_context_reconfigure(WINPR_ATTR_UNUSED FREERDP_VIDEO_CONTEXT* context,
                                       WINPR_ATTR_UNUSED UINT32 width,
                                       WINPR_ATTR_UNUSED UINT32 height,
                                       WINPR_ATTR_UNUSED UINT32 framerate,
                                       WINPR_ATTR_UNUSED UINT32 bitrate,
                                       WINPR_ATTR_UNUSED UINT32 usageType)
{
	return TRUE;
}

BOOL freerdp_video_sample_convert(WINPR_ATTR_UNUSED FREERDP_VIDEO_CONTEXT* context,
                                  FREERDP_VIDEO_FORMAT srcFormat, const void* srcSampleData,
                                  size_t srcSampleLength, FREERDP_VIDEO_FORMAT dstFormat,
                                  wStream* output)
{
	if (srcFormat == dstFormat)
	{
		if (!Stream_EnsureRemainingCapacity(output, srcSampleLength))
			return FALSE;

		Stream_Write(output, srcSampleData, srcSampleLength);
		return TRUE;
	}

	return FALSE;
}

#endif /* WITH_SWSCALE */

const char* freerdp_video_format_string(UINT32 format)
{
#define EVCASE(x) \
	case x:       \
		return #x

	switch (format)
	{
		EVCASE(FREERDP_VIDEO_FORMAT_NONE);
		EVCASE(FREERDP_VIDEO_FORMAT_MJPEG);
		EVCASE(FREERDP_VIDEO_FORMAT_H264);
		EVCASE(FREERDP_VIDEO_FORMAT_YUV420P);
		EVCASE(FREERDP_VIDEO_FORMAT_NV12);
		EVCASE(FREERDP_VIDEO_FORMAT_YUYV422);
		EVCASE(FREERDP_VIDEO_FORMAT_RGB24);
		EVCASE(FREERDP_VIDEO_FORMAT_RGB32);
		default:
			return "FREERDP_VIDEO_FORMAT_UNKNOWN";
	}
#undef EVCASE
}
