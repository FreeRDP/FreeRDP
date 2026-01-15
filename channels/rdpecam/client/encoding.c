/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, Video Encoding
 *
 * Copyright 2024 Oleg Turovski <oleg2104@hotmail.com>
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

#include <winpr/assert.h>
#include <winpr/winpr.h>

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-video.client")

#if defined(WITH_INPUT_FORMAT_H264)
/*
 * demux a H264 frame from a MJPG container
 * args:
 *    srcData - pointer to buffer with h264 muxed in MJPG container
 *    srcSize - buff size
 *    h264_data - pointer to h264 data
 *    h264_max_size - maximum size allowed by h264_data buffer
 *
 * Credits:
 *    guvcview    http://guvcview.sourceforge.net
 *    Paulo Assis <pj.assis@gmail.com>
 *
 * see Figure 5 Payload Size in USB_Video_Payload_H 264_1 0.pdf
 * for format details
 *
 * @return: data size and copies demuxed data to h264 buffer
 */
static size_t demux_uvcH264(const BYTE* srcData, size_t srcSize, BYTE* h264_data,
                            size_t h264_max_size)
{
	WINPR_ASSERT(h264_data);
	WINPR_ASSERT(srcData);

	if (srcSize < 30)
	{
		WLog_ERR(TAG, "Expected srcSize >= 30, got %" PRIuz, srcSize);
		return 0;
	}
	const uint8_t* spl = NULL;
	uint8_t* ph264 = h264_data;

	/* search for 1st APP4 marker
	 * (30 = 2 APP4 marker + 2 length + 22 header + 4 payload size)
	 */
	for (const uint8_t* sp = srcData; sp < srcData + srcSize - 30; sp++)
	{
		if (sp[0] == 0xFF && sp[1] == 0xE4)
		{
			spl = sp + 2; /* exclude APP4 marker */
			break;
		}
	}

	if (spl == NULL)
	{
		WLog_ERR(TAG, "Expected 1st APP4 marker but none found");
		return 0;
	}

	if (spl > srcData + srcSize - 4)
	{
		WLog_ERR(TAG, "Payload + Header size bigger than srcData buffer");
		return 0;
	}

	/* 1st segment length in big endian
	 * includes payload size + header + 6 bytes (2 length + 4 payload size)
	 */
	uint16_t length = (uint16_t)(spl[0] << 8) & UINT16_MAX;
	length |= (uint16_t)spl[1];

	spl += 2; /* header */
	/* header length in little endian at offset 2 */
	uint16_t header_length = (uint16_t)spl[2];
	header_length |= (uint16_t)spl[3] << 8;

	spl += header_length;
	if (spl > srcData + srcSize)
	{
		WLog_ERR(TAG, "Header size bigger than srcData buffer");
		return 0;
	}

	/* payload size in little endian */
	uint32_t payload_size = (uint32_t)spl[0] << 0;
	payload_size |= (uint32_t)spl[1] << 8;
	payload_size |= (uint32_t)spl[2] << 16;
	payload_size |= (uint32_t)spl[3] << 24;

	if (payload_size > h264_max_size)
	{
		WLog_ERR(TAG, "Payload size bigger than h264_data buffer");
		return 0;
	}

	spl += 4;                                /* payload start */
	const uint8_t* epl = spl + payload_size; /* payload end */

	if (epl > srcData + srcSize)
	{
		WLog_ERR(TAG, "Payload size bigger than srcData buffer");
		return 0;
	}

	length -= header_length + 6;

	/* copy 1st segment to h264 buffer */
	memcpy(ph264, spl, length);
	ph264 += length;
	spl += length;

	/* copy other segments */
	while (epl > spl + 4)
	{
		if (spl[0] != 0xFF || spl[1] != 0xE4)
		{
			WLog_ERR(TAG, "Expected 2nd+ APP4 marker but none found");
			const intptr_t diff = ph264 - h264_data;
			return WINPR_ASSERTING_INT_CAST(size_t, diff);
		}

		/* 2nd+ segment length in big endian */
		length = (uint16_t)(spl[2] << 8) & UINT16_MAX;
		length |= (uint16_t)spl[3];
		if (length < 2)
		{
			WLog_ERR(TAG, "Expected 2nd+ APP4 length >= 2 but have %" PRIu16, length);
			return 0;
		}

		length -= 2;
		spl += 4; /* APP4 marker + length */

		/* copy segment to h264 buffer */
		memcpy(ph264, spl, length);
		ph264 += length;
		spl += length;
	}

	const intptr_t diff = ph264 - h264_data;
	return WINPR_ASSERTING_INT_CAST(size_t, diff);
}
#endif

/**
 * Function description
 *
 * @return bitrate in bps
 */
UINT32 h264_get_max_bitrate(UINT32 height)
{
	static struct Bitrates
	{
		UINT32 height;
		UINT32 bitrate; /* kbps */

	} bitrates[] = {
		/* source: https://livekit.io/webrtc/bitrate-guide (webcam streaming)
		 *
		 * sorted by height in descending order
		 */
		{ 1080, 2700 }, { 720, 1250 }, { 480, 700 }, { 360, 400 },
		{ 240, 170 },   { 180, 140 },  { 0, 100 },
	};
	const size_t nBitrates = ARRAYSIZE(bitrates);

	for (size_t i = 0; i < nBitrates; i++)
	{
		if (height >= bitrates[i].height)
		{
			UINT32 bitrate = bitrates[i].bitrate;
			WLog_DBG(TAG, "Setting h264 max bitrate: %u kbps", bitrate);
			return bitrate * 1000;
		}
	}

	WINPR_ASSERT(FALSE);
	return 0;
}

/**
 * Function description
 *
 * @return enum AVPixelFormat value
 */
static enum AVPixelFormat ecamToAVPixFormat(CAM_MEDIA_FORMAT ecamFormat)
{
	switch (ecamFormat)
	{
		case CAM_MEDIA_FORMAT_YUY2:
			return AV_PIX_FMT_YUYV422;
		case CAM_MEDIA_FORMAT_NV12:
			return AV_PIX_FMT_NV12;
		case CAM_MEDIA_FORMAT_I420:
			return AV_PIX_FMT_YUV420P;
		case CAM_MEDIA_FORMAT_RGB24:
			return AV_PIX_FMT_RGB24;
		case CAM_MEDIA_FORMAT_RGB32:
			return AV_PIX_FMT_RGB32;
		default:
			WLog_ERR(TAG, "Unsupported ecamFormat %d", ecamFormat);
			return AV_PIX_FMT_NONE;
	}
}

/**
 * Function description
 * initialize libswscale
 *
 * @return success/failure
 */
static BOOL ecam_init_sws_context(CameraDeviceStream* stream, enum AVPixelFormat pixFormat)
{
	WINPR_ASSERT(stream);

	if (stream->sws)
		return TRUE;

	/* replacing deprecated JPEG formats, still produced by decoder */
	switch (pixFormat)
	{
		case AV_PIX_FMT_YUVJ411P:
			pixFormat = AV_PIX_FMT_YUV411P;
			break;

		case AV_PIX_FMT_YUVJ420P:
			pixFormat = AV_PIX_FMT_YUV420P;
			break;

		case AV_PIX_FMT_YUVJ422P:
			pixFormat = AV_PIX_FMT_YUV422P;
			break;

		case AV_PIX_FMT_YUVJ440P:
			pixFormat = AV_PIX_FMT_YUV440P;
			break;

		case AV_PIX_FMT_YUVJ444P:
			pixFormat = AV_PIX_FMT_YUV444P;
			break;

		default:
			break;
	}

	const int width = (int)stream->currMediaType.Width;
	const int height = (int)stream->currMediaType.Height;

	const enum AVPixelFormat outPixFormat =
	    h264_context_get_option(stream->h264, H264_CONTEXT_OPTION_HW_ACCEL) ? AV_PIX_FMT_NV12
	                                                                        : AV_PIX_FMT_YUV420P;

#if defined(WITH_SWSCALE_LOADING)
	if (!freerdp_swscale_available())
	{
		WLog_ERR(TAG, "swscale not available - install FFmpeg to enable rdpecam");
		return FALSE;
	}
	stream->sws = freerdp_sws_getContext(width, height, pixFormat, width, height, outPixFormat, 0,
	                                     NULL, NULL, NULL);
#else
	stream->sws =
	    sws_getContext(width, height, pixFormat, width, height, outPixFormat, 0, NULL, NULL, NULL);
#endif
	if (!stream->sws)
	{
		WLog_ERR(TAG, "sws_getContext failed");
		return FALSE;
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return success/failure
 */
static BOOL ecam_encoder_compress_h264(CameraDeviceStream* stream, const BYTE* srcData,
                                       size_t srcSize, BYTE** ppDstData, size_t* pDstSize)
{
	UINT32 dstSize = 0;
	BYTE* srcSlice[4] = { 0 };
	int srcLineSizes[4] = { 0 };
	BYTE* yuvData[3] = { 0 };
	UINT32 yuvLineSizes[3] = { 0 };
	prim_size_t size = { stream->currMediaType.Width, stream->currMediaType.Height };
	CAM_MEDIA_FORMAT inputFormat = streamInputFormat(stream);
	enum AVPixelFormat pixFormat = AV_PIX_FMT_NONE;

#if defined(WITH_INPUT_FORMAT_H264)
	if (inputFormat == CAM_MEDIA_FORMAT_MJPG_H264)
	{
		const size_t rc =
		    demux_uvcH264(srcData, srcSize, stream->h264Frame, stream->h264FrameMaxSize);
		dstSize = WINPR_ASSERTING_INT_CAST(uint32_t, rc);
		*ppDstData = stream->h264Frame;
		*pDstSize = dstSize;
		return dstSize > 0;
	}
	else
#endif

#if defined(WITH_INPUT_FORMAT_MJPG)
	    if (inputFormat == CAM_MEDIA_FORMAT_MJPG)
	{
		stream->avInputPkt->data = WINPR_CAST_CONST_PTR_AWAY(srcData, uint8_t*);
		WINPR_ASSERT(srcSize <= INT32_MAX);
		stream->avInputPkt->size = (int)srcSize;

		if (avcodec_send_packet(stream->avContext, stream->avInputPkt) < 0)
		{
			WLog_ERR(TAG, "avcodec_send_packet failed");
			return FALSE;
		}

		if (avcodec_receive_frame(stream->avContext, stream->avOutFrame) < 0)
		{
			WLog_ERR(TAG, "avcodec_receive_frame failed");
			return FALSE;
		}

		for (size_t i = 0; i < 4; i++)
		{
			srcSlice[i] = stream->avOutFrame->data[i];
			srcLineSizes[i] = stream->avOutFrame->linesize[i];
		}

		/* get pixFormat produced by MJPEG decoder */
		pixFormat = stream->avContext->pix_fmt;
	}
	else
#endif
	{
		pixFormat = ecamToAVPixFormat(inputFormat);

#if defined(WITH_SWSCALE_LOADING)
		if (freerdp_av_image_fill_linesizes(srcLineSizes, pixFormat, (int)size.width) < 0)
#else
		if (av_image_fill_linesizes(srcLineSizes, pixFormat, (int)size.width) < 0)
#endif
		{
			WLog_ERR(TAG, "av_image_fill_linesizes failed");
			return FALSE;
		}

#if defined(WITH_SWSCALE_LOADING)
		if (freerdp_av_image_fill_pointers(srcSlice, pixFormat, (int)size.height,
		                                    WINPR_CAST_CONST_PTR_AWAY(srcData, BYTE*),
		                                    srcLineSizes) < 0)
#else
		if (av_image_fill_pointers(srcSlice, pixFormat, (int)size.height,
		                           WINPR_CAST_CONST_PTR_AWAY(srcData, BYTE*), srcLineSizes) < 0)
#endif
		{
			WLog_ERR(TAG, "av_image_fill_pointers failed");
			return FALSE;
		}
	}

	/* get buffers for YUV420P or NV12 */
	if (h264_get_yuv_buffer(stream->h264, 0, size.width, size.height, yuvData, yuvLineSizes) < 0)
		return FALSE;

	/* convert from source format to YUV420P or NV12 */
	if (!ecam_init_sws_context(stream, pixFormat))
		return FALSE;

	const BYTE* cSrcSlice[4] = { srcSlice[0], srcSlice[1], srcSlice[2], srcSlice[3] };
#if defined(WITH_SWSCALE_LOADING)
	if (freerdp_sws_scale(stream->sws, cSrcSlice, srcLineSizes, 0, (int)size.height, yuvData,
	                      (int*)yuvLineSizes) <= 0)
		return FALSE;
#else
	if (sws_scale(stream->sws, cSrcSlice, srcLineSizes, 0, (int)size.height, yuvData,
	              (int*)yuvLineSizes) <= 0)
		return FALSE;
#endif

	/* encode from YUV420P or NV12 to H264 */
	if (h264_compress(stream->h264, ppDstData, &dstSize) < 0)
		return FALSE;

	*pDstSize = dstSize;

	return TRUE;
}

/**
 * Function description
 *
 */
static void ecam_encoder_context_free_h264(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);

	if (stream->sws)
	{
#if defined(WITH_SWSCALE_LOADING)
		freerdp_sws_freeContext(stream->sws);
#else
		sws_freeContext(stream->sws);
#endif
		stream->sws = NULL;
	}

#if defined(WITH_INPUT_FORMAT_MJPG)
	if (stream->avOutFrame)
		av_frame_free(&stream->avOutFrame); /* sets to NULL */

	if (stream->avInputPkt)
	{
		stream->avInputPkt->data = NULL;
		stream->avInputPkt->size = 0;
		av_packet_free(&stream->avInputPkt); /* sets to NULL */
	}

	if (stream->avContext)
		avcodec_free_context(&stream->avContext); /* sets to NULL */
#endif

#if defined(WITH_INPUT_FORMAT_H264)
	if (stream->h264Frame)
	{
		free(stream->h264Frame);
		stream->h264Frame = NULL;
	}
#endif

	if (stream->h264)
	{
		h264_context_free(stream->h264);
		stream->h264 = NULL;
	}
}

#if defined(WITH_INPUT_FORMAT_MJPG)
/**
 * Function description
 *
 * @return success/failure
 */
static BOOL ecam_init_mjpeg_decoder(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);

	const AVCodec* avcodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
	if (!avcodec)
	{
		WLog_ERR(TAG, "avcodec_find_decoder failed to find MJPEG codec");
		return FALSE;
	}

	stream->avContext = avcodec_alloc_context3(avcodec);
	if (!stream->avContext)
	{
		WLog_ERR(TAG, "avcodec_alloc_context3 failed");
		return FALSE;
	}

	stream->avContext->width = WINPR_ASSERTING_INT_CAST(int, stream->currMediaType.Width);
	stream->avContext->height = WINPR_ASSERTING_INT_CAST(int, stream->currMediaType.Height);

	/* AV_EF_EXPLODE flag is to abort decoding on minor error detection,
	 * return error, so we can skip corrupted frames, if any */
	stream->avContext->err_recognition |= AV_EF_EXPLODE;

	if (avcodec_open2(stream->avContext, avcodec, NULL) < 0)
	{
		WLog_ERR(TAG, "avcodec_open2 failed");
		return FALSE;
	}

	stream->avInputPkt = av_packet_alloc();
	if (!stream->avInputPkt)
	{
		WLog_ERR(TAG, "av_packet_alloc failed");
		return FALSE;
	}

	stream->avOutFrame = av_frame_alloc();
	if (!stream->avOutFrame)
	{
		WLog_ERR(TAG, "av_frame_alloc failed");
		return FALSE;
	}

	return TRUE;
}
#endif

/**
 * Function description
 *
 * @return success/failure
 */
static BOOL ecam_encoder_context_init_h264(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);

#if defined(WITH_INPUT_FORMAT_H264)
	if (streamInputFormat(stream) == CAM_MEDIA_FORMAT_MJPG_H264)
	{
		stream->h264FrameMaxSize = 1ULL * stream->currMediaType.Width *
		                           stream->currMediaType.Height; /* 1 byte per pixel */
		stream->h264Frame = (BYTE*)calloc(stream->h264FrameMaxSize, sizeof(BYTE));
		return TRUE; /* encoder not needed */
	}
#endif

	if (!stream->h264)
		stream->h264 = h264_context_new(TRUE);

	if (!stream->h264)
	{
		WLog_ERR(TAG, "h264_context_new failed");
		return FALSE;
	}

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_USAGETYPE,
	                             H264_CAMERA_VIDEO_REAL_TIME))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_FRAMERATE,
	                             stream->currMediaType.FrameRateNumerator /
	                                 stream->currMediaType.FrameRateDenominator))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_BITRATE,
	                             h264_get_max_bitrate(stream->currMediaType.Height)))
		goto fail;

	/* Using CQP mode for rate control. It produces more comparable quality
	 * between VAAPI and software encoding than VBR mode
	 */
	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_RATECONTROL,
	                             H264_RATECONTROL_CQP))
		goto fail;

	/* Using 26 as CQP value. Lower values will produce better quality but
	 * higher bitrate; higher values - lower bitrate but degraded quality
	 */
	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_QP, 26))
		goto fail;

	/* Requesting hardware acceleration before calling h264_context_reset */
	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_HW_ACCEL, TRUE))
		goto fail;

	if (!h264_context_reset(stream->h264, stream->currMediaType.Width,
	                        stream->currMediaType.Height))
	{
		WLog_ERR(TAG, "h264_context_reset failed");
		goto fail;
	}

#if defined(WITH_INPUT_FORMAT_MJPG)
	if (streamInputFormat(stream) == CAM_MEDIA_FORMAT_MJPG && !ecam_init_mjpeg_decoder(stream))
		goto fail;
#endif

	return TRUE;

fail:
	ecam_encoder_context_free_h264(stream);
	return FALSE;
}

/**
 * Function description
 *
 * @return success/failure
 */
BOOL ecam_encoder_context_init(CameraDeviceStream* stream)
{
	CAM_MEDIA_FORMAT format = streamOutputFormat(stream);

	switch (format)
	{
		case CAM_MEDIA_FORMAT_H264:
			return ecam_encoder_context_init_h264(stream);

		default:
			WLog_ERR(TAG, "Unsupported output format %d", format);
			return FALSE;
	}
}

/**
 * Function description
 *
 * @return success/failure
 */
BOOL ecam_encoder_context_free(CameraDeviceStream* stream)
{
	CAM_MEDIA_FORMAT format = streamOutputFormat(stream);
	switch (format)
	{
		case CAM_MEDIA_FORMAT_H264:
			ecam_encoder_context_free_h264(stream);
			break;

		default:
			return FALSE;
	}
	return TRUE;
}

/**
 * Function description
 *
 * @return success/failure
 */
BOOL ecam_encoder_compress(CameraDeviceStream* stream, const BYTE* srcData, size_t srcSize,
                           BYTE** ppDstData, size_t* pDstSize)
{
	CAM_MEDIA_FORMAT format = streamOutputFormat(stream);
	switch (format)
	{
		case CAM_MEDIA_FORMAT_H264:
			return ecam_encoder_compress_h264(stream, srcData, srcSize, ppDstData, pDstSize);
		default:
			WLog_ERR(TAG, "Unsupported output format %d", format);
			return FALSE;
	}
}
