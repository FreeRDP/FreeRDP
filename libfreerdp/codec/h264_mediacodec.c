/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2022 Ely Ronnen <elyronnen@gmail.com>
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
static const int COLOR_FormatYUV420Flexible = 0x7f420888;

/* Output crop rectangle; inclusive bounds, no NDK constants exist for these */
static const char* MEDIACODEC_KEY_CROP_LEFT = "crop-left";
static const char* MEDIACODEC_KEY_CROP_RIGHT = "crop-right";
static const char* MEDIACODEC_KEY_CROP_TOP = "crop-top";
static const char* MEDIACODEC_KEY_CROP_BOTTOM = "crop-bottom";

/* https://developer.android.com/reference/android/media/MediaCodec#qualityFloor */
static const int MEDIACODEC_MINIMUM_WIDTH = 320;
static const int MEDIACODEC_MINIMUM_HEIGHT = 240;
static const int32_t MEDIACODEC_MAXIMUM_DIMENSION = 16384;

typedef struct
{
	AMediaCodec* decoder;
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

	/* decoder deinterleave buffer for NV12 / semiplanar output */
	BYTE* decodeI420Buffer;
	size_t decodeI420Capacity;
} H264_CONTEXT_MEDIACODEC;

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

	WLog_Print(h264->log, WLOG_ERROR, "MediaCodec is not supported as an encoder");
	return -1;
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
		while (inputBufferCurrnetOffset < SrcSize)
		{
			UINT32 numberOfBytesToCopy = SrcSize - inputBufferCurrnetOffset;
			const ssize_t inputBufferId = AMediaCodec_dequeueInputBuffer(sys->decoder, -1);
			if (inputBufferId < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_dequeueInputBuffer failed [%zd]",
				           inputBufferId);
				// TODO: sleep?
				continue;
			}

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

		while (true)
		{
			AMediaCodecBufferInfo bufferInfo = WINPR_C_ARRAY_INIT;
			ssize_t outputBufferId = AMediaCodec_dequeueOutputBuffer(sys->decoder, &bufferInfo, -1);
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
				WLog_Print(h264->log, WLOG_WARN,
				           "AMediaCodec_dequeueOutputBuffer need to try again later");
				// TODO: sleep?
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
		status = AMediaCodec_stop(sys->decoder);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_stop %d", status);
		}

		status = AMediaCodec_delete(sys->decoder);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_delete %d", status);
		}

		sys->decoder = nullptr;
	}

	free(sys->decodeI420Buffer);
	sys->decodeI420Buffer = nullptr;

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

	if (h264->Compressor)
	{
		WLog_Print(h264->log, WLOG_ERROR, "MediaCodec is not supported as an encoder");
		goto EXCEPTION;
	}

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

	return TRUE;
EXCEPTION:
	mediacodec_uninit(h264);
	return FALSE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_mediacodec = { "MediaCodec", mediacodec_init,
	                                                    mediacodec_uninit, mediacodec_decompress,
	                                                    mediacodec_compress };
