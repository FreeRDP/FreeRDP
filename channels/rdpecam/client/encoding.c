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
 * @return FREERDP_VIDEO_FORMAT value
 */
static FREERDP_VIDEO_FORMAT ecamToVideoFormat(CAM_MEDIA_FORMAT ecamFormat)
{
	switch (ecamFormat)
	{
		case CAM_MEDIA_FORMAT_YUY2:
			return FREERDP_VIDEO_FORMAT_YUYV422;
		case CAM_MEDIA_FORMAT_NV12:
			return FREERDP_VIDEO_FORMAT_NV12;
		case CAM_MEDIA_FORMAT_I420:
			return FREERDP_VIDEO_FORMAT_YUV420P;
		case CAM_MEDIA_FORMAT_RGB24:
			return FREERDP_VIDEO_FORMAT_RGB24;
		case CAM_MEDIA_FORMAT_RGB32:
			return FREERDP_VIDEO_FORMAT_RGB32;
		case CAM_MEDIA_FORMAT_MJPG:
		case CAM_MEDIA_FORMAT_MJPG_H264:
			return FREERDP_VIDEO_FORMAT_MJPEG;
		default:
			WLog_ERR(TAG, "Unsupported ecamFormat %u", ecamFormat);
			return FREERDP_VIDEO_FORMAT_NONE;
	}
}

/**
 * Function description
 * initialize video context
 *
 * @return success/failure
 */
static BOOL ecam_init_video_context(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);

	if (stream->video)
		return TRUE;

	stream->video = freerdp_video_context_new(stream->currMediaType.Width,
	                                          stream->currMediaType.Height);
	if (!stream->video)
	{
		WLog_ERR(TAG, "freerdp_video_context_new failed");
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
	WINPR_ASSERT(stream);
	WINPR_ASSERT(stream->video);

	CAM_MEDIA_FORMAT inputFormat = streamInputFormat(stream);
	FREERDP_VIDEO_FORMAT videoFormat = ecamToVideoFormat(inputFormat);

	if (videoFormat == FREERDP_VIDEO_FORMAT_NONE)
	{
		WLog_ERR(TAG, "Unsupported input format %u", inputFormat);
		return FALSE;
	}

	if (!stream->h264Output)
	{
		stream->h264Output = Stream_New(NULL, 1024 * 1024);
		if (!stream->h264Output)
			return FALSE;
	}

	Stream_SetPosition(stream->h264Output, 0);

	if (!freerdp_video_sample_convert(stream->video, videoFormat, srcData, srcSize,
	                                  stream->currMediaType.Width, stream->currMediaType.Height,
	                                  FREERDP_VIDEO_FORMAT_H264, stream->h264Output))
	{
		return FALSE;
	}

	*ppDstData = Stream_Buffer(stream->h264Output);
	*pDstSize = Stream_GetPosition(stream->h264Output);

	return TRUE;
}

/**
 * Function description
 *
 */
static void ecam_encoder_context_free_h264(CameraDeviceStream* stream)
{
	WINPR_ASSERT(stream);

	if (stream->video)
	{
		freerdp_video_context_free(stream->video);
		stream->video = NULL;
	}

	if (stream->h264Output)
	{
		Stream_Free(stream->h264Output, TRUE);
		stream->h264Output = NULL;
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

	if (!ecam_init_video_context(stream))
		return FALSE;

	UINT32 framerate = stream->currMediaType.FrameRateNumerator /
	                   stream->currMediaType.FrameRateDenominator;

	if (!freerdp_video_context_configure_h264(stream->video, stream->currMediaType.Width,
	                                         stream->currMediaType.Height, framerate, 0,
	                                         H264_CAMERA_VIDEO_REAL_TIME))
	{
		WLog_ERR(TAG, "Failed to configure H.264 encoder");
		return FALSE;
	}

	return TRUE;
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
			WLog_ERR(TAG, "Unsupported output format %u", format);
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
			WLog_ERR(TAG, "Unsupported output format %u", format);
			return FALSE;
	}
}
