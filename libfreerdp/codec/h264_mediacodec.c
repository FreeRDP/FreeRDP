/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2022 Ely Ronnen <elyronnen@gmail.com>
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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
#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/library.h>

#include <freerdp/log.h>
#include <freerdp/codec/h264.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "h264.h"

static const char* CODEC_NAME = "video/avc";

static const int COLOR_FormatYUV420Planar = 19;
static const int COLOR_FormatYUV420SemiPlanar = 21; /* NV12, widest encoder support */
static const int COLOR_FormatYUV420Flexible = 0x7f420888;

/* MediaFormat.KEY_PREPEND_HEADER_TO_SYNC_FRAMES has no NDK constant; emits SPS/PPS before every IDR
 */
static const char* MEDIACODEC_KEY_PREPEND_HEADER = "prepend-sps-pps-to-idr-frames";

/* AMEDIAFORMAT_KEY_LOW_LATENCY has no NDK constant below API 30; the string works at runtime */
static const char* MEDIACODEC_KEY_LOW_LATENCY = "low-latency";

/* Output crop rectangle; inclusive bounds, no NDK constants exist for these */
static const char* MEDIACODEC_KEY_CROP_LEFT = "crop-left";
static const char* MEDIACODEC_KEY_CROP_RIGHT = "crop-right";
static const char* MEDIACODEC_KEY_CROP_TOP = "crop-top";
static const char* MEDIACODEC_KEY_CROP_BOTTOM = "crop-bottom";

/* Dequeue timeouts in microseconds; output may lag a frame while the pipeline primes */
static const int64_t MEDIACODEC_INPUT_DEQUEUE_TIMEOUT = 10000;
static const int64_t MEDIACODEC_OUTPUT_DEQUEUE_TIMEOUT = 5000;

/* Bound the decode dequeue retries so a stalled codec cannot spin forever. */
static const int MEDIACODEC_DECODE_MAX_RETRIES = 100;

/* https://developer.android.com/reference/android/media/MediaCodec#qualityFloor */
static const int MEDIACODEC_MINIMUM_WIDTH = 320;
static const int MEDIACODEC_MINIMUM_HEIGHT = 240;
static const int32_t MEDIACODEC_MAXIMUM_DIMENSION = 16384;

/* Keyframe interval in seconds; short so a late-joining server recovers quickly */
static const int MEDIACODEC_IFRAME_INTERVAL = 1;
/* Fallbacks if the context did not specify a rate. */
static const int MEDIACODEC_DEFAULT_FRAMERATE = 30;
static const int MEDIACODEC_DEFAULT_BITRATE = 4000000;

/* MediaCodecInfo.EncoderCapabilities bitrate modes */
static const int MEDIACODEC_BITRATE_MODE_VBR = 1;

/* setParameters keys; no NDK constant below API 31, the strings work at runtime */
static const char* MEDIACODEC_PARAM_VIDEO_BITRATE = "video-bitrate";

typedef struct
{
	AMediaCodec* decoder;
	AMediaCodec* encoder;
	AMediaFormat* inputFormat;
	AMediaFormat* outputFormat;
	int32_t width;
	int32_t height;
	int32_t outputWidth;
	int32_t outputHeight;
	int32_t outputStride;
	int32_t outputSliceHeight;
	int32_t cropLeft;
	int32_t cropTop;
	int32_t colorFormat;
	ssize_t currentOutputBufferIndex;

	BOOL decoderStarted;

	/* decoder deinterleave buffer for NV12 / semiplanar output */
	BYTE* decodeI420Buffer;
	size_t decodeI420Capacity;

	/* encoder state */
	BOOL encoderStarted;
	int32_t encoderWidth; /* dimensions the encoder is currently configured for */
	int32_t encoderHeight;
	int32_t encoderStride; /* input buffer layout reported by the encoder */
	int32_t encoderSliceHeight;
	int32_t encoderBitrate;
	BOOL syncRequested;         /* an IDR was already asked for since the last configure */
	int64_t frameIndex;         /* monotonic source for presentation timestamps */
	BYTE* outputData;           /* staging copy of the most recent encoded frame */
	size_t outputCapacity;      /* allocated size of outputData */
	int32_t encoderFailedWidth; /* size the encoder refused */
	int32_t encoderFailedHeight;

	/* encoder cached SPS/PPS header */
	BYTE* spsPpsData;
	size_t spsPpsSize;
} H264_CONTEXT_MEDIACODEC;

#define mediacodec_warn(log, status, what) \
	mediacodec_warn_((log), (status), (what), __FILE__, __func__, __LINE__)

static void mediacodec_warn_(wLog* log, media_status_t status, const char* what, const char* file,
                             const char* fkt, size_t line)
{
	if ((status != AMEDIA_OK) && WLog_IsLevelActive(log, WLOG_WARN))
		WLog_PrintTextMessage(log, WLOG_WARN, line, file, fkt, "%s failed: %d", what, status);
}

WINPR_ATTR_NODISCARD
static AMediaFormat* mediacodec_format_new(wLog* log, int width, int height)
{
	const char* media_format = nullptr;
	AMediaFormat* format = AMediaFormat_new();
	if (format == nullptr)
	{
		WLog_Print(log, WLOG_ERROR, "AMediaFormat_new failed");
		return nullptr;
	}

	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, CODEC_NAME);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420Planar);
	/* Low-latency decode for remote desktop: drop frame reordering, ask for realtime scheduling. */
	AMediaFormat_setInt32(format, MEDIACODEC_KEY_LOW_LATENCY, 1);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_PRIORITY, 0);

	media_format = AMediaFormat_toString(format);
	if (media_format == nullptr)
	{
		WLog_Print(log, WLOG_ERROR, "AMediaFormat_toString failed");
		AMediaFormat_delete(format);
		return nullptr;
	}

	WLog_Print(log, WLOG_DEBUG, "MediaCodec configuring with desired output format [%s]",
	           media_format);

	return format;
}

static void set_mediacodec_format(H264_CONTEXT* h264, AMediaFormat** formatVariable,
                                  AMediaFormat* newFormat)
{
	media_status_t status = AMEDIA_OK;
	H264_CONTEXT_MEDIACODEC* sys = nullptr;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(formatVariable);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (*formatVariable == newFormat)
		return;

	if (*formatVariable != nullptr)
	{
		status = AMediaFormat_delete(*formatVariable);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaFormat_delete %d", status);
		}
	}

	*formatVariable = newFormat;
}

WINPR_ATTR_NODISCARD
static int update_mediacodec_inputformat(H264_CONTEXT* h264)
{
	H264_CONTEXT_MEDIACODEC* sys = nullptr;
	AMediaFormat* inputFormat = nullptr;
	const char* mediaFormatName = nullptr;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	inputFormat = AMediaCodec_getInputFormat(sys->decoder);
	if (inputFormat == nullptr)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputFormat failed");
		return -1;
	}
	set_mediacodec_format(h264, &sys->inputFormat, inputFormat);

	mediaFormatName = AMediaFormat_toString(sys->inputFormat);
	if (mediaFormatName == nullptr)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
		return -1;
	}
	WLog_Print(h264->log, WLOG_DEBUG, "Using MediaCodec with input MediaFormat [%s]",
	           mediaFormatName);

	return 1;
}

WINPR_ATTR_NODISCARD
static int update_mediacodec_outputformat(H264_CONTEXT* h264)
{
	H264_CONTEXT_MEDIACODEC* sys = nullptr;
	AMediaFormat* outputFormat = nullptr;
	const char* mediaFormatName = nullptr;
	int32_t outputWidth = 0;
	int32_t outputHeight = 0;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	outputFormat = AMediaCodec_getOutputFormat(sys->decoder);
	if (outputFormat == nullptr)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getOutputFormat failed");
		return -1;
	}
	set_mediacodec_format(h264, &sys->outputFormat, outputFormat);

	mediaFormatName = AMediaFormat_toString(sys->outputFormat);
	if (mediaFormatName == nullptr)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
		return -1;
	}
	WLog_Print(h264->log, WLOG_DEBUG, "Using MediaCodec with output MediaFormat [%s]",
	           mediaFormatName);

	if (!AMediaFormat_getInt32(sys->outputFormat, AMEDIAFORMAT_KEY_WIDTH, &outputWidth))
	{
		WLog_Print(h264->log, WLOG_ERROR, "fnAMediaFormat_getInt32 failed getting width");
		return -1;
	}

	if (!AMediaFormat_getInt32(sys->outputFormat, AMEDIAFORMAT_KEY_HEIGHT, &outputHeight))
	{
		WLog_Print(h264->log, WLOG_ERROR, "fnAMediaFormat_getInt32 failed getting height");
		return -1;
	}

	/* Honor codec stride/slice-height padding; fall back to tight packing. */
	int32_t outputStride = 0;
	int32_t outputSliceHeight = 0;
	if (!AMediaFormat_getInt32(sys->outputFormat, AMEDIAFORMAT_KEY_STRIDE, &outputStride) ||
	    outputStride < outputWidth)
		outputStride = outputWidth;
	if (!AMediaFormat_getInt32(sys->outputFormat, AMEDIAFORMAT_KEY_SLICE_HEIGHT,
	                           &outputSliceHeight) ||
	    outputSliceHeight < outputHeight)
		outputSliceHeight = outputHeight;
	if ((outputWidth <= 0) || (outputHeight <= 0) ||
	    (outputStride > MEDIACODEC_MAXIMUM_DIMENSION) ||
	    (outputSliceHeight > MEDIACODEC_MAXIMUM_DIMENSION))
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "MediaCodec output layout out of range: %dx%d stride %d slice-height %d",
		           outputWidth, outputHeight, outputStride, outputSliceHeight);
		return -1;
	}
	sys->outputStride = outputStride;
	sys->outputSliceHeight = outputSliceHeight;

	/* Width and height above describe the padded allocation; the picture inside it is the crop
	 * rectangle, with inclusive bounds. */
	sys->outputWidth = outputWidth;
	sys->outputHeight = outputHeight;
	sys->cropLeft = 0;
	sys->cropTop = 0;

	int32_t cropLeft = 0;
	int32_t cropRight = 0;
	int32_t cropTop = 0;
	int32_t cropBottom = 0;
	/* Only the far edges are always published; a missing near edge means zero. */
	if (!AMediaFormat_getInt32(sys->outputFormat, MEDIACODEC_KEY_CROP_LEFT, &cropLeft))
		cropLeft = 0;
	if (!AMediaFormat_getInt32(sys->outputFormat, MEDIACODEC_KEY_CROP_TOP, &cropTop))
		cropTop = 0;

	if (AMediaFormat_getInt32(sys->outputFormat, MEDIACODEC_KEY_CROP_RIGHT, &cropRight) &&
	    AMediaFormat_getInt32(sys->outputFormat, MEDIACODEC_KEY_CROP_BOTTOM, &cropBottom) &&
	    (cropLeft >= 0) && (cropTop >= 0) && (cropRight >= cropLeft) && (cropBottom >= cropTop) &&
	    (cropRight < outputWidth) && (cropBottom < outputHeight))
	{
		sys->outputWidth = cropRight - cropLeft + 1;
		sys->outputHeight = cropBottom - cropTop + 1;
		sys->cropLeft = cropLeft;
		sys->cropTop = cropTop;
	}

	/* The requested planar format is a hint; vendors may output NV12 or flexible YUV. */
	int32_t colorFormat = 0;
	if (!AMediaFormat_getInt32(sys->outputFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat))
		colorFormat = COLOR_FormatYUV420Planar;
	sys->colorFormat = colorFormat;

	if (colorFormat == COLOR_FormatYUV420Flexible)
		WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec output is flexible YUV, assuming NV12");

	/* Publish visible dimensions for region rectangle validation. */
	h264->YUVWidth = (UINT32)sys->outputWidth;
	h264->YUVHeight = (UINT32)sys->outputHeight;

	WLog_Print(h264->log, WLOG_DEBUG,
	           "MediaCodec output %dx%d stride %d slice-height %d crop [%d,%d] color 0x%x",
	           sys->outputWidth, sys->outputHeight, sys->outputStride, sys->outputSliceHeight,
	           sys->cropLeft, sys->cropTop, colorFormat);

	return 1;
}

static void release_current_outputbuffer(H264_CONTEXT* h264)
{
	media_status_t status = AMEDIA_OK;
	H264_CONTEXT_MEDIACODEC* sys = nullptr;

	WINPR_ASSERT(h264);
	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->currentOutputBufferIndex < 0)
	{
		return;
	}

	status = AMediaCodec_releaseOutputBuffer(sys->decoder, sys->currentOutputBufferIndex, FALSE);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_releaseOutputBuffer %d", status);
	}

	sys->currentOutputBufferIndex = -1;
}

/* Tear down the encoder (used on reconfigure and uninit). */
static void mediacodec_encoder_release(wLog* log, H264_CONTEXT_MEDIACODEC* sys)
{
	if (!sys || !sys->encoder)
		return;

	if (sys->encoderStarted)
		mediacodec_warn(log, AMediaCodec_stop(sys->encoder), "AMediaCodec_stop");
	mediacodec_warn(log, AMediaCodec_delete(sys->encoder), "AMediaCodec_delete");
	sys->encoder = nullptr;
	sys->encoderStarted = FALSE;

	/* Invalidate cached SPS/PPS on release. */
	free(sys->spsPpsData);
	sys->spsPpsData = nullptr;
	sys->spsPpsSize = 0;
}

/* Probe H.264 encoder support to allow fallback if unsupported. */
WINPR_ATTR_NODISCARD
static BOOL mediacodec_encoder_probe(wLog* log)
{
	AMediaCodec* probe = AMediaCodec_createEncoderByType(CODEC_NAME);
	if (!probe)
	{
		WLog_Print(log, WLOG_WARN, "No MediaCodec H.264 encoder on this device");
		return FALSE;
	}

	BOOL ok = FALSE;
	AMediaFormat* format = AMediaFormat_new();
	if (format)
	{
		AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, CODEC_NAME);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, MEDIACODEC_MINIMUM_WIDTH);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, MEDIACODEC_MINIMUM_HEIGHT);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420SemiPlanar);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, MEDIACODEC_DEFAULT_BITRATE);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BITRATE_MODE, MEDIACODEC_BITRATE_MODE_VBR);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, MEDIACODEC_DEFAULT_FRAMERATE);
		AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL,
		                      MEDIACODEC_IFRAME_INTERVAL);

		if (AMediaCodec_configure(probe, format, nullptr, nullptr,
		                          AMEDIACODEC_CONFIGURE_FLAG_ENCODE) == AMEDIA_OK &&
		    AMediaCodec_start(probe) == AMEDIA_OK)
		{
			ok = TRUE;
			mediacodec_warn(log, AMediaCodec_stop(probe), "AMediaCodec_stop");
		}
		mediacodec_warn(log, AMediaFormat_delete(format), "AMediaFormat_delete");
	}

	mediacodec_warn(log, AMediaCodec_delete(probe), "AMediaCodec_delete");
	if (!ok)
		WLog_Print(log, WLOG_WARN,
		           "MediaCodec H.264 encoder cannot be configured (color format unsupported?)");
	return ok;
}

WINPR_ATTR_NODISCARD
static int32_t mediacodec_rate(UINT32 value, int32_t fallback)
{
	if (value == 0)
		return fallback;
	return (value > INT32_MAX) ? INT32_MAX : WINPR_ASSERTING_INT_CAST(int32_t, value);
}

/* (Re)configure the encoder for current dimensions. */
WINPR_ATTR_NODISCARD
static BOOL mediacodec_encoder_configure(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);
	H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	mediacodec_encoder_release(h264->log, sys);

	if (h264->width < MEDIACODEC_MINIMUM_WIDTH || h264->height < MEDIACODEC_MINIMUM_HEIGHT ||
	    h264->width > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION ||
	    h264->height > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION)
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec encoder dimensions out of range [%u,%u]",
		           h264->width, h264->height);
		return FALSE;
	}

	const int32_t width = WINPR_ASSERTING_INT_CAST(int32_t, h264->width);
	const int32_t height = WINPR_ASSERTING_INT_CAST(int32_t, h264->height);
	const int32_t framerate = mediacodec_rate(h264->FrameRate, MEDIACODEC_DEFAULT_FRAMERATE);
	const int32_t bitrate = mediacodec_rate(h264->BitRate, MEDIACODEC_DEFAULT_BITRATE);

	sys->encoder = AMediaCodec_createEncoderByType(CODEC_NAME);
	if (!sys->encoder)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_createEncoderByType failed");
		return FALSE;
	}

	AMediaFormat* format = AMediaFormat_new();
	if (!format)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_new failed");
		mediacodec_encoder_release(h264->log, sys);
		return FALSE;
	}

	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, CODEC_NAME);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420SemiPlanar);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
	/* Hardware AVC encoders commonly reject CQ, so CQP requests are served as VBR. */
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BITRATE_MODE, MEDIACODEC_BITRATE_MODE_VBR);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, framerate);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, MEDIACODEC_IFRAME_INTERVAL);
	AMediaFormat_setInt32(format, MEDIACODEC_KEY_PREPEND_HEADER, 1);

	media_status_t status = AMediaCodec_configure(sys->encoder, format, nullptr, nullptr,
	                                              AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
	mediacodec_warn(h264->log, AMediaFormat_delete(format), "AMediaFormat_delete");
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_configure (encoder) failed: %d", status);
		mediacodec_encoder_release(h264->log, sys);
		return FALSE;
	}

	status = AMediaCodec_start(sys->encoder);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_start (encoder) failed: %d", status);
		mediacodec_encoder_release(h264->log, sys);
		return FALSE;
	}

	sys->encoderStarted = TRUE;
	sys->encoderWidth = width;
	sys->encoderHeight = height;
	sys->encoderBitrate = bitrate;
	sys->syncRequested = FALSE;
	sys->frameIndex = 0;

	/* Encoder stride/slice-height may be padded. Align stride to even for NV12. */
	sys->encoderStride = sys->encoderWidth + (sys->encoderWidth & 1);
	sys->encoderSliceHeight = sys->encoderHeight;
	AMediaFormat* inputFormat = AMediaCodec_getInputFormat(sys->encoder);
	if (inputFormat)
	{
		int32_t stride = 0;
		int32_t sliceHeight = 0;
		if (AMediaFormat_getInt32(inputFormat, AMEDIAFORMAT_KEY_STRIDE, &stride) &&
		    (stride >= sys->encoderWidth))
			sys->encoderStride = stride;
		if (AMediaFormat_getInt32(inputFormat, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &sliceHeight) &&
		    (sliceHeight >= sys->encoderHeight))
			sys->encoderSliceHeight = sliceHeight;
		mediacodec_warn(h264->log, AMediaFormat_delete(inputFormat), "AMediaFormat_delete");
	}

	if ((sys->encoderStride > MEDIACODEC_MAXIMUM_DIMENSION) ||
	    (sys->encoderSliceHeight > MEDIACODEC_MAXIMUM_DIMENSION))
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "MediaCodec encoder input layout out of range: stride %d slice-height %d",
		           sys->encoderStride, sys->encoderSliceHeight);
		mediacodec_encoder_release(h264->log, sys);
		return FALSE;
	}

	WLog_Print(
	    h264->log, WLOG_DEBUG,
	    "MediaCodec H.264 encoder started: %ux%u @ %d fps, %d bps, stride %d, slice-height %d",
	    h264->width, h264->height, framerate, bitrate, sys->encoderStride, sys->encoderSliceHeight);
	return TRUE;
}

/* Copy a planar I420 source frame into the encoder's NV12 (COLOR_FormatYUV420SemiPlanar) input,
 * honoring the destination stride and slice height. */
WINPR_ATTR_NODISCARD
static BOOL mediacodec_copy_i420_to_nv12(uint8_t* dst, size_t dstCapacity, const BYTE** pSrcYuv,
                                         const UINT32* pStride, int32_t width, int32_t height,
                                         int32_t dstStride, int32_t dstSliceHeight)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(pSrcYuv);
	WINPR_ASSERT(pStride);

	const int32_t cw = (width + 1) / 2;
	const int32_t ch = (height + 1) / 2;
	const size_t ySize = (size_t)dstStride * dstSliceHeight;

	if (dstCapacity < ySize + (size_t)dstStride * ((dstSliceHeight + 1) / 2))
		return FALSE;

	for (int32_t row = 0; row < height; row++)
		memcpy(dst + (size_t)row * dstStride, pSrcYuv[0] + (size_t)row * pStride[0], (size_t)width);

	/* Clamp pairs to prevent overflow on odd width. */
	const int32_t pairs = (dstStride / 2 < cw) ? dstStride / 2 : cw;
	uint8_t* uv = dst + ySize;
	for (int32_t row = 0; row < ch; row++)
	{
		const BYTE* uRow = pSrcYuv[1] + (size_t)row * pStride[1];
		const BYTE* vRow = pSrcYuv[2] + (size_t)row * pStride[2];
		uint8_t* uvRow = uv + (size_t)row * dstStride;
		for (int32_t col = 0; col < pairs; col++)
		{
			uvRow[(size_t)col * 2] = uRow[col];
			uvRow[(size_t)col * 2 + 1] = vRow[col];
		}
	}

	return TRUE;
}

/* Check if an SPS NAL appears before the first coded slice. */
WINPR_ATTR_NODISCARD
static BOOL mediacodec_has_sps(const uint8_t* data, size_t size)
{
	WINPR_ASSERT(data);

	for (size_t i = 0; i + 3 < size; i++)
	{
		if ((data[i] != 0) || (data[i + 1] != 0))
			continue;

		size_t nal = 0;
		if (data[i + 2] == 1)
			nal = i + 3;
		else if ((data[i + 2] == 0) && (data[i + 3] == 1))
			nal = i + 4;
		else
			continue;

		if (nal >= size)
			break;

		const uint8_t type = data[nal] & 0x1F;
		if (type == 7)
			return TRUE;
		if ((type == 1) || (type == 5))
			break; /* coded slice reached, no parameter set in the header */

		i = nal;
	}

	return FALSE;
}

static void mediacodec_copy_nv12_to_i420(BYTE* dstYuv[3], const UINT32 dstStride[3],
                                         const uint8_t* src, int32_t width, int32_t height,
                                         int32_t srcStride, int32_t srcSliceHeight,
                                         int32_t cropLeft, int32_t cropTop)
{
	WINPR_ASSERT(dstYuv);
	WINPR_ASSERT(dstStride);
	WINPR_ASSERT(src);

	const int32_t cw = (width + 1) / 2;
	const int32_t ch = (height + 1) / 2;
	const size_t srcLumaOffset = (size_t)cropTop * (size_t)srcStride + (size_t)cropLeft;
	const size_t srcChromaOffset = (size_t)srcStride * (size_t)srcSliceHeight +
	                               (size_t)(cropTop / 2) * (size_t)srcStride +
	                               ((size_t)cropLeft & ~1ULL);

	const uint8_t* srcY = src + srcLumaOffset;
	const uint8_t* srcUV = src + srcChromaOffset;

	/* Clamp chroma pair count to row bounds. */
	const int32_t chromaLeft = cropLeft & ~1;
	const int32_t avail = (srcStride > chromaLeft) ? (srcStride - chromaLeft) / 2 : 0;
	const int32_t pairs = (avail < cw) ? avail : cw;

	for (int32_t row = 0; row < height; row++)
	{
		memcpy(dstYuv[0] + (size_t)row * (size_t)dstStride[0],
		       srcY + (size_t)row * (size_t)srcStride, (size_t)width);
	}

	for (int32_t row = 0; row < ch; row++)
	{
		const uint8_t* uvRow = srcUV + (size_t)row * (size_t)srcStride;
		BYTE* uRow = dstYuv[1] + (size_t)row * (size_t)dstStride[1];
		BYTE* vRow = dstYuv[2] + (size_t)row * (size_t)dstStride[2];
		for (int32_t col = 0; col < pairs; col++)
		{
			uRow[col] = uvRow[(size_t)col * 2];
			vRow[col] = uvRow[(size_t)col * 2 + 1];
		}

		/* Fill unused chroma with neutral gray (0x80). */
		for (int32_t col = pairs; col < cw; col++)
		{
			uRow[col] = 0x80;
			vRow[col] = 0x80;
		}
	}
}

WINPR_ATTR_NODISCARD
static int mediacodec_compress(H264_CONTEXT* h264, const BYTE** pSrcYuv, const UINT32* pStride,
                               BYTE** ppDstData, UINT32* pDstSize)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcYuv);
	WINPR_ASSERT(pStride);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (!pSrcYuv[0] || !pSrcYuv[1] || !pSrcYuv[2])
		return -1;

	if ((h264->width > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION) ||
	    (h264->height > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION))
		return -1;

	const int32_t width = WINPR_ASSERTING_INT_CAST(int32_t, h264->width);
	const int32_t height = WINPR_ASSERTING_INT_CAST(int32_t, h264->height);

	if (!sys->encoderStarted || sys->encoderWidth != width || sys->encoderHeight != height)
	{
		if ((width == sys->encoderFailedWidth) && (height == sys->encoderFailedHeight))
			return -1;

		if (!mediacodec_encoder_configure(h264))
		{
			sys->encoderFailedWidth = width;
			sys->encoderFailedHeight = height;
			return -1;
		}
		sys->encoderFailedWidth = 0;
		sys->encoderFailedHeight = 0;
	}

	/* Adapt the encoder bitrate on the fly when the caller changes it. */
	const int32_t bitrate = mediacodec_rate(h264->BitRate, sys->encoderBitrate);
	if (bitrate != sys->encoderBitrate)
	{
		AMediaFormat* params = AMediaFormat_new();
		if (params)
		{
			AMediaFormat_setInt32(params, MEDIACODEC_PARAM_VIDEO_BITRATE, bitrate);
			if (AMediaCodec_setParameters(sys->encoder, params) == AMEDIA_OK)
				sys->encoderBitrate = bitrate;
			mediacodec_warn(h264->log, AMediaFormat_delete(params), "AMediaFormat_delete");
		}
	}

	if (!h264->firstLumaFrameDone && (sys->frameIndex > 0) && !sys->syncRequested)
	{
		AMediaFormat* syncParams = AMediaFormat_new();
		if (syncParams)
		{
			AMediaFormat_setInt32(syncParams, "request-sync", 0);
			if (AMediaCodec_setParameters(sys->encoder, syncParams) == AMEDIA_OK)
				sys->syncRequested = TRUE;
			mediacodec_warn(h264->log, AMediaFormat_delete(syncParams), "AMediaFormat_delete");
		}
	}

	const int32_t dstStride = sys->encoderStride;
	const int32_t dstSliceHeight = sys->encoderSliceHeight;
	const size_t frameSize =
	    (size_t)dstStride * dstSliceHeight + (size_t)dstStride * ((dstSliceHeight + 1) / 2);

	/* 1. Feed one raw frame to the encoder. */
	const ssize_t inIdx =
	    AMediaCodec_dequeueInputBuffer(sys->encoder, MEDIACODEC_INPUT_DEQUEUE_TIMEOUT);
	if (inIdx < 0)
	{
		/* Drain pending output buffer to unblock input queue. */
		AMediaCodecBufferInfo stale = { 0 };
		const ssize_t staleIdx = AMediaCodec_dequeueOutputBuffer(sys->encoder, &stale, 0);
		if (staleIdx >= 0)
			mediacodec_warn(h264->log,
			                AMediaCodec_releaseOutputBuffer(sys->encoder, (size_t)staleIdx, FALSE),
			                "AMediaCodec_releaseOutputBuffer");

		WLog_Print(h264->log, WLOG_WARN, "MediaCodec encoder has no input buffer, dropping frame");
		return -1;
	}

	size_t inCapacity = 0;
	uint8_t* inBuffer = AMediaCodec_getInputBuffer(sys->encoder, (size_t)inIdx, &inCapacity);
	if (!inBuffer || inCapacity < frameSize)
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec encoder input buffer too small: %zu < %zu",
		           inCapacity, frameSize);
		mediacodec_warn(h264->log,
		                AMediaCodec_queueInputBuffer(sys->encoder, (size_t)inIdx, 0, 0, 0, 0),
		                "AMediaCodec_queueInputBuffer");
		return -1;
	}

	if (!mediacodec_copy_i420_to_nv12(inBuffer, inCapacity, pSrcYuv, pStride, width, height,
	                                  dstStride, dstSliceHeight))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec encoder input buffer too small for layout");
		mediacodec_warn(h264->log,
		                AMediaCodec_queueInputBuffer(sys->encoder, (size_t)inIdx, 0, 0, 0, 0),
		                "AMediaCodec_queueInputBuffer");
		return -1;
	}

	const int32_t framerate = mediacodec_rate(h264->FrameRate, MEDIACODEC_DEFAULT_FRAMERATE);
	const uint64_t pts = (uint64_t)sys->frameIndex++ * 1000000ULL / (uint64_t)framerate;
	media_status_t status =
	    AMediaCodec_queueInputBuffer(sys->encoder, (size_t)inIdx, 0, frameSize, pts, 0);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_queueInputBuffer (encoder) failed: %d",
		           status);
		return -1;
	}

	/* 2. Collect one encoded frame; skip standalone codec-config buffer. */
	for (;;)
	{
		AMediaCodecBufferInfo info = { 0 };
		const ssize_t outIdx =
		    AMediaCodec_dequeueOutputBuffer(sys->encoder, &info, MEDIACODEC_OUTPUT_DEQUEUE_TIMEOUT);
		if (outIdx >= 0)
		{
			size_t outCapacity = 0;
			uint8_t* outBuffer =
			    AMediaCodec_getOutputBuffer(sys->encoder, (size_t)outIdx, &outCapacity);
			if (!outBuffer || (info.size < 0) || (info.offset < 0) ||
			    ((size_t)info.offset > outCapacity) ||
			    ((size_t)info.size > outCapacity - (size_t)info.offset))
			{
				WLog_Print(h264->log, WLOG_ERROR,
				           "MediaCodec encoder output out of range: offset %d size %d cap %zu",
				           info.offset, info.size, outCapacity);
				mediacodec_warn(
				    h264->log, AMediaCodec_releaseOutputBuffer(sys->encoder, (size_t)outIdx, FALSE),
				    "AMediaCodec_releaseOutputBuffer");
				return -1;
			}

			if (((info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0) && (info.size > 0))
			{
				BYTE* newCfg = (BYTE*)realloc(sys->spsPpsData, (size_t)info.size);
				if (newCfg)
				{
					sys->spsPpsData = newCfg;
					sys->spsPpsSize = (size_t)info.size;
					memcpy(sys->spsPpsData, outBuffer + info.offset, (size_t)info.size);
				}
				else
				{
					WLog_Print(h264->log, WLOG_WARN, "MediaCodec encoder failed to store SPS/PPS");
				}
			}

			if (((info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != 0) || (info.size == 0))
			{
				const media_status_t rc =
				    AMediaCodec_releaseOutputBuffer(sys->encoder, (size_t)outIdx, FALSE);
				if (rc != AMEDIA_OK)
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "AMediaCodec_releaseOutputBuffer (encoder) failed: %d", rc);
					return -1;
				}
				continue;
			}

			const BOOL isKeyFrame = (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0;
			const BOOL prependSps = isKeyFrame && sys->spsPpsData && sys->spsPpsSize > 0 &&
			                        !mediacodec_has_sps(outBuffer + info.offset, (size_t)info.size);
			const size_t totalSize = (size_t)info.size + (prependSps ? sys->spsPpsSize : 0);

			if (sys->outputCapacity < totalSize)
			{
				BYTE* resized = (BYTE*)realloc(sys->outputData, totalSize);
				if (!resized)
				{
					mediacodec_warn(
					    h264->log,
					    AMediaCodec_releaseOutputBuffer(sys->encoder, (size_t)outIdx, FALSE),
					    "AMediaCodec_releaseOutputBuffer");
					return -1;
				}
				sys->outputData = resized;
				sys->outputCapacity = totalSize;
			}

			if (prependSps)
			{
				memcpy(sys->outputData, sys->spsPpsData, sys->spsPpsSize);
				memcpy(sys->outputData + sys->spsPpsSize, outBuffer + info.offset,
				       (size_t)info.size);
			}
			else
			{
				memcpy(sys->outputData, outBuffer + info.offset, (size_t)info.size);
			}

			*ppDstData = sys->outputData;
			*pDstSize = WINPR_ASSERTING_INT_CAST(UINT32, totalSize);
			const media_status_t rc =
			    AMediaCodec_releaseOutputBuffer(sys->encoder, (size_t)outIdx, FALSE);
			if (rc != AMEDIA_OK)
				WLog_Print(h264->log, WLOG_ERROR,
				           "AMediaCodec_releaseOutputBuffer (encoder) failed: %d", rc);
			return 1;
		}
		else if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED ||
		         outIdx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
		{
			continue;
		}
		else if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
		{
			/* Output not ready yet (pipeline priming). */
			WLog_Print(h264->log, WLOG_DEBUG,
			           "MediaCodec encoder output not ready, dropping frame");
			return -1;
		}
		else
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "AMediaCodec_dequeueOutputBuffer (encoder) returned %zd", outIdx);
			return -1;
		}
	}
}

WINPR_ATTR_NODISCARD
static int mediacodec_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData);

	H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	BYTE** pYUVData = h264->pYUVData;
	WINPR_ASSERT(pYUVData);

	UINT32* iStride = h264->iStride;
	WINPR_ASSERT(iStride);

	release_current_outputbuffer(h264);

	if ((h264->width > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION) ||
	    (h264->height > (UINT32)MEDIACODEC_MAXIMUM_DIMENSION))
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "MediaCodec size [%" PRIu32 ",%" PRIu32 "] exceeds the maximum", h264->width,
		           h264->height);
		return -1;
	}

	if ((sys->width != WINPR_ASSERTING_INT_CAST(int32_t, h264->width)) ||
	    (sys->height != WINPR_ASSERTING_INT_CAST(int32_t, h264->height)))
	{
		sys->width = WINPR_ASSERTING_INT_CAST(int32_t, h264->width);
		sys->height = WINPR_ASSERTING_INT_CAST(int32_t, h264->height);

		if (sys->width < MEDIACODEC_MINIMUM_WIDTH || sys->height < MEDIACODEC_MINIMUM_HEIGHT)
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "MediaCodec got width or height smaller than minimum [%d,%d]", sys->width,
			           sys->height);
			return -1;
		}

		WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec setting new input width and height [%d,%d]",
		           sys->width, sys->height);

		AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_WIDTH, sys->width);
		AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_HEIGHT, sys->height);
		const media_status_t status = AMediaCodec_setParameters(sys->decoder, sys->inputFormat);
		if (status != AMEDIA_OK)
		{
			/* Decoder adapts via in-band SPS/PPS; ignoring failure here. */
			WLog_Print(h264->log, WLOG_DEBUG,
			           "AMediaCodec_setParameters returned %d, relying on "
			           "in-band SPS/PPS for the size change",
			           status);
		}

		/* The codec can change output width and height */
		if (update_mediacodec_outputformat(h264) < 0)
		{
			WLog_Print(h264->log, WLOG_ERROR, "MediaCodec failed updating input format");
			return -1;
		}
	}

	while (true)
	{
		UINT32 inputBufferCurrnetOffset = 0;
		int inputRetries = 0;
		while (inputBufferCurrnetOffset < SrcSize)
		{
			UINT32 numberOfBytesToCopy = SrcSize - inputBufferCurrnetOffset;
			const ssize_t inputBufferId =
			    AMediaCodec_dequeueInputBuffer(sys->decoder, MEDIACODEC_INPUT_DEQUEUE_TIMEOUT);
			if (inputBufferId < 0)
			{
				if (++inputRetries > MEDIACODEC_DECODE_MAX_RETRIES)
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "AMediaCodec_dequeueInputBuffer gave up [%zd]", inputBufferId);
					return -1;
				}
				continue;
			}
			inputRetries = 0;

			size_t inputBufferSize = 0;
			uint8_t* inputBuffer =
			    AMediaCodec_getInputBuffer(sys->decoder, inputBufferId, &inputBufferSize);
			if (inputBuffer == nullptr)
			{
				WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputBuffer failed");
				return -1;
			}

			if (numberOfBytesToCopy > inputBufferSize)
			{
				WLog_Print(h264->log, WLOG_WARN,
				           "MediaCodec inputBufferSize: got [%zu] but wanted [%u]", inputBufferSize,
				           numberOfBytesToCopy);
				numberOfBytesToCopy = WINPR_ASSERTING_INT_CAST(UINT32, inputBufferSize);
			}

			memcpy(inputBuffer, &pSrcData[inputBufferCurrnetOffset], numberOfBytesToCopy);
			inputBufferCurrnetOffset += numberOfBytesToCopy;

			const media_status_t status = AMediaCodec_queueInputBuffer(
			    sys->decoder, inputBufferId, 0, numberOfBytesToCopy, 0, 0);
			if (status != AMEDIA_OK)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_queueInputBuffer %d", status);
				return -1;
			}
		}

		int outputRetries = 0;
		while (true)
		{
			AMediaCodecBufferInfo bufferInfo = WINPR_C_ARRAY_INIT;
			ssize_t outputBufferId = AMediaCodec_dequeueOutputBuffer(
			    sys->decoder, &bufferInfo, MEDIACODEC_OUTPUT_DEQUEUE_TIMEOUT);
			if (outputBufferId >= 0)
			{
				sys->currentOutputBufferIndex = outputBufferId;

				size_t outputBufferSize = 0;
				uint8_t* outputBuffer = nullptr;
				outputBuffer =
				    AMediaCodec_getOutputBuffer(sys->decoder, outputBufferId, &outputBufferSize);
				sys->currentOutputBufferIndex = outputBufferId;

				/* End of stream and metadata buffers carry no picture. */
				if (bufferInfo.size <= 0)
				{
					release_current_outputbuffer(h264);
					if (++outputRetries > MEDIACODEC_DECODE_MAX_RETRIES)
						return 0;
					continue;
				}

				/* The picture starts at the reported offset, not at the buffer base. */
				if (!outputBuffer || (bufferInfo.offset < 0) ||
				    ((size_t)bufferInfo.offset > outputBufferSize))
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "MediaCodec output offset %d out of range %zu", bufferInfo.offset,
					           outputBufferSize);
					return -1;
				}
				outputBuffer += bufferInfo.offset;
				outputBufferSize -= (size_t)bufferInfo.offset;

				const size_t lumaSize = (size_t)sys->outputStride * (size_t)sys->outputSliceHeight;
				const int32_t chromaStride = sys->outputStride / 2;
				const int32_t chromaHeight = (sys->outputSliceHeight + 1) / 2;
				const size_t chromaSize = (size_t)chromaStride * (size_t)chromaHeight;
				const size_t minRequiredSize =
				    lumaSize + (size_t)sys->outputStride * (size_t)chromaHeight;

				if (outputBufferSize < minRequiredSize)
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "Error MediaCodec output buffer too small %zu < %zu",
					           outputBufferSize, minRequiredSize);
					return -1;
				}

				if (sys->colorFormat == COLOR_FormatYUV420Planar)
				{
					iStride[0] = (UINT32)sys->outputStride;
					iStride[1] = (UINT32)chromaStride;
					iStride[2] = (UINT32)chromaStride;
					pYUVData[0] = outputBuffer +
					              ((size_t)sys->cropTop * (size_t)sys->outputStride) +
					              (size_t)sys->cropLeft;
					pYUVData[1] = outputBuffer + lumaSize +
					              ((size_t)(sys->cropTop / 2) * (size_t)chromaStride) +
					              ((size_t)sys->cropLeft / 2);
					pYUVData[2] = outputBuffer + lumaSize + chromaSize +
					              ((size_t)(sys->cropTop / 2) * (size_t)chromaStride) +
					              ((size_t)sys->cropLeft / 2);
				}
				else
				{
					/* SemiPlanar (NV12) or flexible YUV: deinterleave into staging I420 buffer */
					const UINT32 dstYStride = (UINT32)sys->outputWidth;
					const UINT32 dstUVStride = (UINT32)((sys->outputWidth + 1) / 2);
					const size_t yBytes = (size_t)dstYStride * (size_t)sys->outputHeight;
					const size_t uvBytes =
					    (size_t)dstUVStride * (size_t)((sys->outputHeight + 1) / 2);
					const size_t neededCapacity = yBytes + uvBytes * 2;

					if (sys->decodeI420Capacity < neededCapacity)
					{
						BYTE* newBuf = (BYTE*)realloc(sys->decodeI420Buffer, neededCapacity);
						if (!newBuf)
						{
							WLog_Print(h264->log, WLOG_ERROR,
							           "Failed to allocate I420 deinterleave buffer");
							return -1;
						}
						sys->decodeI420Buffer = newBuf;
						sys->decodeI420Capacity = neededCapacity;
					}

					iStride[0] = dstYStride;
					iStride[1] = dstUVStride;
					iStride[2] = dstUVStride;
					pYUVData[0] = sys->decodeI420Buffer;
					pYUVData[1] = sys->decodeI420Buffer + yBytes;
					pYUVData[2] = sys->decodeI420Buffer + yBytes + uvBytes;

					mediacodec_copy_nv12_to_i420(
					    pYUVData, iStride, outputBuffer, sys->outputWidth, sys->outputHeight,
					    sys->outputStride, sys->outputSliceHeight, sys->cropLeft, sys->cropTop);
					release_current_outputbuffer(h264);
				}

				h264->YUVWidth = (UINT32)sys->outputWidth;
				h264->YUVHeight = (UINT32)sys->outputHeight;
				break;
			}
			else if (outputBufferId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
			{
				if (update_mediacodec_outputformat(h264) < 0)
				{
					WLog_Print(h264->log, WLOG_ERROR,
					           "MediaCodec failed updating output format in decompress");
					return -1;
				}
			}
			else if (outputBufferId == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
			{
				if (++outputRetries > MEDIACODEC_DECODE_MAX_RETRIES)
				{
					WLog_Print(h264->log, WLOG_DEBUG,
					           "AMediaCodec_dequeueOutputBuffer no buffer ready yet");
					return 0;
				}
			}
			else if (outputBufferId == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
			{
				WLog_Print(h264->log, WLOG_WARN,
				           "AMediaCodec_dequeueOutputBuffer returned deprecated value "
				           "AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED, ignoring");
			}
			else
			{
				WLog_Print(h264->log, WLOG_ERROR,
				           "AMediaCodec_dequeueOutputBuffer returned unknown value [%zd]",
				           outputBufferId);
				return -1;
			}
		}

		break;
	}

	return 1;
}

static void mediacodec_uninit(H264_CONTEXT* h264)
{
	media_status_t status = AMEDIA_OK;
	H264_CONTEXT_MEDIACODEC* sys = nullptr;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;

	WLog_Print(h264->log, WLOG_DEBUG, "Uninitializing MediaCodec");

	if (!sys)
		return;

	if (sys->decoder != nullptr)
	{
		release_current_outputbuffer(h264);

		if (sys->decoderStarted)
		{
			status = AMediaCodec_stop(sys->decoder);
			if (status != AMEDIA_OK)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_stop %d", status);
			}
			sys->decoderStarted = FALSE;
		}

		status = AMediaCodec_delete(sys->decoder);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_delete %d", status);
		}

		sys->decoder = nullptr;
	}

	mediacodec_encoder_release(h264->log, sys);
	free(sys->outputData);
	sys->outputData = nullptr;
	free(sys->decodeI420Buffer);
	sys->decodeI420Buffer = nullptr;
	free(sys->spsPpsData);
	sys->spsPpsData = nullptr;

	set_mediacodec_format(h264, &sys->inputFormat, nullptr);
	set_mediacodec_format(h264, &sys->outputFormat, nullptr);

	free(sys);
	h264->pSystemData = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL mediacodec_init(H264_CONTEXT* h264)
{
	H264_CONTEXT_MEDIACODEC* sys = nullptr;
	media_status_t status = AMEDIA_OK;

	WINPR_ASSERT(h264);

	WLog_Print(h264->log, WLOG_DEBUG, "Initializing MediaCodec");

	sys = (H264_CONTEXT_MEDIACODEC*)calloc(1, sizeof(H264_CONTEXT_MEDIACODEC));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*)sys;

	sys->currentOutputBufferIndex = -1;

	/* Updated when given height and width for the first time */
	sys->width = sys->outputWidth = MEDIACODEC_MINIMUM_WIDTH;
	sys->height = sys->outputHeight = MEDIACODEC_MINIMUM_HEIGHT;

	/* Probe encoder support to allow fallback during initialization. */
	if (h264->Compressor)
	{
		if (!mediacodec_encoder_probe(h264->log))
			goto EXCEPTION;
		WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec H.264 encoder available");
		return TRUE;
	}

	sys->decoder = AMediaCodec_createDecoderByType(CODEC_NAME);
	if (sys->decoder == nullptr)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_createCodecByName failed");
		goto EXCEPTION;
	}

	char* codec_name = nullptr;
	status = AMediaCodec_getName(sys->decoder, &codec_name);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getName failed: %d", status);
		goto EXCEPTION;
	}

	WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec using %s codec [%s]", CODEC_NAME, codec_name);
	AMediaCodec_releaseName(sys->decoder, codec_name);

	set_mediacodec_format(h264, &sys->inputFormat,
	                      mediacodec_format_new(h264->log, sys->width, sys->height));

	status = AMediaCodec_configure(sys->decoder, sys->inputFormat, nullptr, nullptr, 0);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_configure failed: %d", status);
		goto EXCEPTION;
	}

	if (update_mediacodec_inputformat(h264) < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec failed updating input format");
		goto EXCEPTION;
	}

	if (update_mediacodec_outputformat(h264) < 0)
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec failed updating output format");
		goto EXCEPTION;
	}

	WLog_Print(h264->log, WLOG_DEBUG, "Starting MediaCodec");
	status = AMediaCodec_start(sys->decoder);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_start failed %d", status);
		goto EXCEPTION;
	}

	sys->decoderStarted = TRUE;
	return TRUE;
EXCEPTION:
	mediacodec_uninit(h264);
	return FALSE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_mediacodec = { "MediaCodec", mediacodec_init,
	                                                    mediacodec_uninit, mediacodec_decompress,
	                                                    mediacodec_compress };
