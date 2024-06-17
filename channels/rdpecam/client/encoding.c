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

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-video.client")

/**
 * Function description
 *
 * @return bitrate in bps
 */
static UINT32 ecam_encoder_h264_get_max_bitrate(CameraDeviceStream* stream)
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

	UINT32 height = stream->currMediaType.Height;

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
 *
 * @return success/failure
 */
static BOOL ecam_encoder_compress_h264(CameraDeviceStream* stream, const BYTE* srcData,
                                       size_t srcSize, BYTE** ppDstData, size_t* pDstSize)
{
	UINT32 dstSize = 0;
	BYTE* srcSlice[4] = { 0 };
	BYTE* yuv420pData[3] = { 0 };
	UINT32 yuv420pStride[3] = { 0 };
	prim_size_t size = { 0 };
	size.width = stream->currMediaType.Width;
	size.height = stream->currMediaType.Height;
	CAM_MEDIA_FORMAT inputFormat = streamInputFormat(stream);
	enum AVPixelFormat pixFormat = ecamToAVPixFormat(inputFormat);

	/* get buffers for YUV420P */
	if (h264_get_yuv_buffer(stream->h264, stream->srcLineSizes[0], size.width, size.height,
	                        yuv420pData, yuv420pStride) < 0)
		return FALSE;

	/* convert from source format to YUV420P */
	if (av_image_fill_pointers(srcSlice, pixFormat, (int)size.height, (BYTE*)srcData,
	                           stream->srcLineSizes) < 0)
		return FALSE;

	const BYTE* cSrcSlice[4] = { srcSlice[0], srcSlice[1], srcSlice[2], srcSlice[3] };
	if (sws_scale(stream->sws, cSrcSlice, stream->srcLineSizes, 0, (int)size.height, yuv420pData,
	              (int*)yuv420pStride) <= 0)
		return FALSE;

	/* encode from YUV420P to H264 */
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
		sws_freeContext(stream->sws);
		stream->sws = NULL;
	}

	if (stream->h264)
	{
		h264_context_free(stream->h264);
		stream->h264 = NULL;
	}
}

/**
 * Function description
 *
 * @return success/failure
 */
static BOOL ecam_encoder_context_init_h264(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);
	if (!stream->h264)
		stream->h264 = h264_context_new(TRUE);

	if (!stream->h264)
	{
		WLog_ERR(TAG, "h264_context_new failed");
		return FALSE;
	}

	if (!h264_context_reset(stream->h264, stream->currMediaType.Width,
	                        stream->currMediaType.Height))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_USAGETYPE,
	                             H264_CAMERA_VIDEO_REAL_TIME))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_FRAMERATE,
	                             stream->currMediaType.FrameRateNumerator /
	                                 stream->currMediaType.FrameRateDenominator))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_BITRATE,
	                             ecam_encoder_h264_get_max_bitrate(stream)))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_RATECONTROL,
	                             H264_RATECONTROL_VBR))
		goto fail;

	if (!h264_context_set_option(stream->h264, H264_CONTEXT_OPTION_QP, 0))
		goto fail;

	/* initialize libswscale */
	{
		const int width = (int)stream->currMediaType.Width;
		const int height = (int)stream->currMediaType.Height;
		CAM_MEDIA_FORMAT inputFormat = streamInputFormat(stream);
		enum AVPixelFormat pixFormat = ecamToAVPixFormat(inputFormat);

		if (av_image_fill_linesizes(stream->srcLineSizes, pixFormat, width) < 0)
		{
			WLog_ERR(TAG, "av_image_fill_linesizes failed");
			goto fail;
		}

		if (!stream->sws)
		{
			stream->sws = sws_getContext(width, height, pixFormat, width, height,
			                             AV_PIX_FMT_YUV420P, 0, NULL, NULL, NULL);
		}
		if (!stream->sws)
		{
			WLog_ERR(TAG, "sws_getContext failed");
			goto fail;
		}
	}

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
