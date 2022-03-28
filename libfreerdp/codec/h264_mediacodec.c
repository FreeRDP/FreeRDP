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

#include <winpr/wlog.h>
#include <winpr/assert.h>
#include <winpr/library.h>

#include <freerdp/log.h>
#include <freerdp/codec/h264.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "h264.h"

#define RESOLVE_MEDIANDK_FUNC(sys, name)                                          \
	({                                                                            \
		BOOL rc = TRUE;                                                           \
		sys->fn##name = GetProcAddress(sys->mediandkLibrary, #name);              \
		if (sys->fn##name == NULL)                                                \
		{                                                                         \
			WLog_Print(h264->log, WLOG_ERROR,                                     \
			           "Error resolving function " #name " from libmediandk.so"); \
			rc = FALSE;                                                           \
		}                                                                         \
		rc;                                                                       \
	})

#define RESOLVE_MEDIANDK_VARIABLE(sys, member, exported)                             \
	({                                                                               \
		BOOL rc = FALSE;                                                             \
		const char** temp = GetProcAddress(sys->mediandkLibrary, exported);          \
		if (temp == NULL)                                                            \
		{                                                                            \
			WLog_Print(h264->log, WLOG_ERROR,                                        \
			           "Error resolving variable " exported " from libmediandk.so"); \
		}                                                                            \
		else                                                                         \
		{                                                                            \
			sys->member = *temp;                                                     \
			rc = TRUE;                                                               \
		}                                                                            \
		rc;                                                                          \
	})

typedef AMediaFormat* (*AMediaFormat_new_t)(void);
typedef media_status_t (*AMediaFormat_delete_t)(AMediaFormat*);
typedef const char* (*AMediaFormat_toString_t)(AMediaFormat*);
typedef void (*AMediaFormat_setInt32_t)(AMediaFormat*, const char*, int32_t);
typedef void (*AMediaFormat_setString_t)(AMediaFormat*, const char*, const char*);
typedef AMediaCodec* (*AMediaCodec_createDecoderByType_t)(const char*);
typedef media_status_t (*AMediaCodec_delete_t)(AMediaCodec*);
typedef media_status_t (*AMediaCodec_configure_t)(AMediaCodec*, const AMediaFormat*, ANativeWindow*,
                                                  AMediaCrypto*, uint32_t);
typedef media_status_t (*AMediaCodec_start_t)(AMediaCodec*);
typedef media_status_t (*AMediaCodec_stop_t)(AMediaCodec*);
typedef uint8_t* (*AMediaCodec_getInputBuffer_t)(AMediaCodec*, size_t, size_t*);
typedef uint8_t* (*AMediaCodec_getOutputBuffer_t)(AMediaCodec*, size_t, size_t*);
typedef ssize_t (*AMediaCodec_dequeueInputBuffer_t)(AMediaCodec*, int64_t);
typedef media_status_t (*AMediaCodec_queueInputBuffer_t)(AMediaCodec*, size_t, ssize_t, size_t,
                                                         uint64_t, uint32_t);
typedef ssize_t (*AMediaCodec_dequeueOutputBuffer_t)(AMediaCodec*, AMediaCodecBufferInfo*, int64_t);
typedef AMediaFormat* (*AMediaCodec_getOutputFormat_t)(AMediaCodec*);
typedef media_status_t (*AMediaCodec_releaseOutputBuffer_t)(AMediaCodec*, size_t, bool);
typedef media_status_t (*AMediaCodec_setParameters_t)(AMediaCodec*, const AMediaFormat*); // 26
typedef media_status_t (*AMediaCodec_getName_t)(AMediaCodec*, char**);                    // 28
typedef void (*AMediaCodec_releaseName_t)(AMediaCodec*, char*);                           // 28
typedef AMediaFormat* (*AMediaCodec_getInputFormat_t)(AMediaCodec*);                      // 28

const int COLOR_FormatYUV420Planar = 19;
const int COLOR_FormatYUV420Flexible = 0x7f420888;

struct _H264_CONTEXT_MEDIACODEC
{
	AMediaCodec* decoder;
	AMediaFormat* inputFormat;
	AMediaFormat* outputFormat;
	int32_t width;
	int32_t height;
	ssize_t currentOutputBufferIndex;

	// libmediandk.so imports
	HMODULE mediandkLibrary;

	AMediaFormat_new_t fnAMediaFormat_new;
	AMediaFormat_delete_t fnAMediaFormat_delete;
	AMediaFormat_toString_t fnAMediaFormat_toString;
	AMediaFormat_setInt32_t fnAMediaFormat_setInt32;
	AMediaFormat_setString_t fnAMediaFormat_setString;
	AMediaCodec_createDecoderByType_t fnAMediaCodec_createDecoderByType;
	AMediaCodec_delete_t fnAMediaCodec_delete;
	AMediaCodec_configure_t fnAMediaCodec_configure;
	AMediaCodec_start_t fnAMediaCodec_start;
	AMediaCodec_stop_t fnAMediaCodec_stop;
	AMediaCodec_getInputBuffer_t fnAMediaCodec_getInputBuffer;
	AMediaCodec_getOutputBuffer_t fnAMediaCodec_getOutputBuffer;
	AMediaCodec_dequeueInputBuffer_t fnAMediaCodec_dequeueInputBuffer;
	AMediaCodec_queueInputBuffer_t fnAMediaCodec_queueInputBuffer;
	AMediaCodec_dequeueOutputBuffer_t fnAMediaCodec_dequeueOutputBuffer;
	AMediaCodec_getOutputFormat_t fnAMediaCodec_getOutputFormat;
	AMediaCodec_releaseOutputBuffer_t fnAMediaCodec_releaseOutputBuffer;
	AMediaCodec_setParameters_t fnAMediaCodec_setParameters;
	AMediaCodec_getName_t fnAMediaCodec_getName;
	AMediaCodec_releaseName_t fnAMediaCodec_releaseName;
	AMediaCodec_getInputFormat_t fnAMediaCodec_getInputFormat;

	const char* gAMediaFormatKeyMime;
	const char* gAMediaFormatKeyWidth;
	const char* gAMediaFormatKeyHeight;
	const char* gAMediaFormatKeyColorFormat;
};

typedef struct _H264_CONTEXT_MEDIACODEC H264_CONTEXT_MEDIACODEC;

static int load_libmediandk(H264_CONTEXT* h264)
{
	BOOL rc;
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec Loading libmediandk.so");

	sys->mediandkLibrary = LoadLibraryA("libmediandk.so");
	if (sys->mediandkLibrary == NULL)
	{
		WLog_Print(h264->log, WLOG_WARN, "Error loading libmediandk.so");
		return -1;
	}

	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaFormat_new);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaFormat_delete);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaFormat_toString);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaFormat_setInt32);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaFormat_setString);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_createDecoderByType);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_delete);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_configure);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_start);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_stop);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_getInputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_getOutputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_dequeueInputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_queueInputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_dequeueOutputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_getOutputFormat);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_releaseOutputBuffer);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_setParameters);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_getName);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_releaseName);
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_FUNC(sys, AMediaCodec_getInputFormat);
	if (!rc)
		return -1;

	rc = RESOLVE_MEDIANDK_VARIABLE(sys, gAMediaFormatKeyMime, "AMEDIAFORMAT_KEY_MIME");
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_VARIABLE(sys, gAMediaFormatKeyWidth, "AMEDIAFORMAT_KEY_WIDTH");
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_VARIABLE(sys, gAMediaFormatKeyHeight, "AMEDIAFORMAT_KEY_HEIGHT");
	if (!rc)
		return -1;
	rc = RESOLVE_MEDIANDK_VARIABLE(sys, gAMediaFormatKeyColorFormat,
	                               "AMEDIAFORMAT_KEY_COLOR_FORMAT");
	if (!rc)
		return -1;

	return 0;
}

static void unload_libmediandk(H264_CONTEXT* h264)
{
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (NULL == sys->mediandkLibrary)
	{
		return;
	}

	FreeLibrary(sys->mediandkLibrary);
}

static void set_mediacodec_format(H264_CONTEXT* h264, AMediaFormat** formatVariable,
                                  AMediaFormat* newFormat)
{
	media_status_t status = AMEDIA_OK;
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(formatVariable);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (*formatVariable != NULL)
	{
		status = sys->fnAMediaFormat_delete(*formatVariable);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaFormat_delete %d", status);
		}
	}

	*formatVariable = newFormat;
}

static void release_current_outputbuffer(H264_CONTEXT* h264)
{
	media_status_t status = AMEDIA_OK;
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);
	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->currentOutputBufferIndex < 0)
	{
		return;
	}

	status =
	    sys->fnAMediaCodec_releaseOutputBuffer(sys->decoder, sys->currentOutputBufferIndex, FALSE);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_releaseOutputBuffer %d", status);
	}

	sys->currentOutputBufferIndex = -1;
}

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

static int mediacodec_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize)
{
	ssize_t inputBufferId = -1;
	size_t inputBufferSize, outputBufferSize;
	uint8_t* inputBuffer;
	media_status_t status;
	const char* media_format;
	BYTE** pYUVData;
	UINT32* iStride;
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
	WINPR_ASSERT(sys);

	pYUVData = h264->pYUVData;
	WINPR_ASSERT(pYUVData);

	iStride = h264->iStride;
	WINPR_ASSERT(iStride);

	release_current_outputbuffer(h264);

	if (sys->width == 0)
	{
		int32_t width = h264->width;
		int32_t height = h264->height;
		if (width % 16 != 0)
			width += 16 - width % 16;
		if (height % 16 != 0)
			height += 16 - height % 16;

		WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec setting width and height [%d,%d]", width,
		           height);
		sys->fnAMediaFormat_setInt32(sys->inputFormat, sys->gAMediaFormatKeyWidth, width);
		sys->fnAMediaFormat_setInt32(sys->inputFormat, sys->gAMediaFormatKeyHeight, height);

		status = sys->fnAMediaCodec_setParameters(sys->decoder, sys->inputFormat);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_setParameters failed: %d", status);
			return -1;
		}

		sys->width = width;
		sys->height = height;
	}

	while (true)
	{
		UINT32 inputBufferCurrnetOffset = 0;
		while (inputBufferCurrnetOffset < SrcSize)
		{
			UINT32 numberOfBytesToCopy = SrcSize - inputBufferCurrnetOffset;
			inputBufferId = sys->fnAMediaCodec_dequeueInputBuffer(sys->decoder, -1);
			if (inputBufferId < 0)
			{
				WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_dequeueInputBuffer failed [%d]",
				           inputBufferId);
				// TODO: sleep?
				continue;
			}

			inputBuffer =
			    sys->fnAMediaCodec_getInputBuffer(sys->decoder, inputBufferId, &inputBufferSize);
			if (inputBuffer == NULL)
			{
				WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputBuffer failed");
				return -1;
			}

			if (numberOfBytesToCopy > inputBufferSize)
			{
				WLog_Print(h264->log, WLOG_WARN,
				           "MediaCodec inputBufferSize: got [%d] but wanted [%d]", inputBufferSize,
				           numberOfBytesToCopy);
				numberOfBytesToCopy = inputBufferSize;
			}

			memcpy(inputBuffer, pSrcData + inputBufferCurrnetOffset, numberOfBytesToCopy);
			inputBufferCurrnetOffset += numberOfBytesToCopy;

			status = sys->fnAMediaCodec_queueInputBuffer(sys->decoder, inputBufferId, 0,
			                                             numberOfBytesToCopy, 0, 0);
			if (status != AMEDIA_OK)
			{
				WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_queueInputBuffer %d", status);
				return -1;
			}
		}

		while (true)
		{
			AMediaCodecBufferInfo bufferInfo;
			ssize_t outputBufferId =
			    sys->fnAMediaCodec_dequeueOutputBuffer(sys->decoder, &bufferInfo, -1);
			if (outputBufferId >= 0)
			{
				sys->currentOutputBufferIndex = outputBufferId;

				uint8_t* outputBuffer;
				outputBuffer = sys->fnAMediaCodec_getOutputBuffer(sys->decoder, outputBufferId,
				                                                  &outputBufferSize);
				sys->currentOutputBufferIndex = outputBufferId;

				if (outputBufferSize != (sys->width * sys->height +
				                         ((sys->width + 1) / 2) * ((sys->height + 1) / 2) * 2))
				{
					WLog_Print(h264->log, WLOG_ERROR, "Error unexpected output buffer size %d",
					           outputBufferSize);
					return -1;
				}

				// TODO: work with AImageReader and get AImage object instead of
				// COLOR_FormatYUV420Planar buffer.
				iStride[0] = sys->width;
				iStride[1] = (sys->width + 1) / 2;
				iStride[2] = (sys->width + 1) / 2;
				pYUVData[0] = outputBuffer;
				pYUVData[1] = outputBuffer + iStride[0] * sys->height;
				pYUVData[2] =
				    outputBuffer + iStride[0] * sys->height + iStride[1] * ((sys->height + 1) / 2);
				break;
			}
			else if (outputBufferId == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
			{
				AMediaFormat* outputFormat;
				outputFormat = sys->fnAMediaCodec_getOutputFormat(sys->decoder);
				if (outputFormat == NULL)
				{
					WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getOutputFormat failed");
					return -1;
				}
				set_mediacodec_format(h264, &sys->outputFormat, outputFormat);

				media_format = sys->fnAMediaFormat_toString(sys->outputFormat);
				if (media_format == NULL)
				{
					WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
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
				           "AMediaCodec_dequeueOutputBuffer returned unknown value [%d]",
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
	H264_CONTEXT_MEDIACODEC* sys;

	WINPR_ASSERT(h264);

	sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;

	WLog_Print(h264->log, WLOG_DEBUG, "Uninitializing MediaCodec");

	if (!sys)
		return;

	if (sys->decoder != NULL)
	{
		release_current_outputbuffer(h264);
		status = sys->fnAMediaCodec_stop(sys->decoder);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_stop %d", status);
		}

		status = sys->fnAMediaCodec_delete(sys->decoder);
		if (status != AMEDIA_OK)
		{
			WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_delete %d", status);
		}

		sys->decoder = NULL;
	}

	set_mediacodec_format(h264, &sys->inputFormat, NULL);
	set_mediacodec_format(h264, &sys->outputFormat, NULL);

	unload_libmediandk(h264);

	free(sys);
	h264->pSystemData = NULL;
}

static BOOL mediacodec_init(H264_CONTEXT* h264)
{
	H264_CONTEXT_MEDIACODEC* sys;
	media_status_t status;
	const char* media_format;
	char* codec_name;
	AMediaFormat *inputFormat, *outputFormat;

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

	if (load_libmediandk(h264) < 0)
	{
		goto EXCEPTION;
	}

	sys->currentOutputBufferIndex = -1;
	sys->width = sys->height = 0; // update when we're given the height and width
	sys->decoder = sys->fnAMediaCodec_createDecoderByType("video/avc");
	if (sys->decoder == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_createCodecByName failed");
		goto EXCEPTION;
	}

	status = sys->fnAMediaCodec_getName(sys->decoder, &codec_name);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getName failed: %d", status);
		goto EXCEPTION;
	}

	WLog_Print(h264->log, WLOG_DEBUG, "MediaCodec using video/avc codec [%s]", codec_name);
	sys->fnAMediaCodec_releaseName(sys->decoder, codec_name);

	sys->inputFormat = sys->fnAMediaFormat_new();
	if (sys->inputFormat == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_new failed");
		goto EXCEPTION;
	}

	sys->fnAMediaFormat_setString(sys->inputFormat, sys->gAMediaFormatKeyMime, "video/avc");
	sys->fnAMediaFormat_setInt32(sys->inputFormat, sys->gAMediaFormatKeyWidth, sys->width);
	sys->fnAMediaFormat_setInt32(sys->inputFormat, sys->gAMediaFormatKeyHeight, sys->height);
	sys->fnAMediaFormat_setInt32(sys->inputFormat, sys->gAMediaFormatKeyColorFormat,
	                             COLOR_FormatYUV420Planar);

	media_format = sys->fnAMediaFormat_toString(sys->inputFormat);
	if (media_format == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
		goto EXCEPTION;
	}

	status = sys->fnAMediaCodec_configure(sys->decoder, sys->inputFormat, NULL, NULL, 0);
	if (status != AMEDIA_OK)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_configure failed: %d", status);
		goto EXCEPTION;
	}

	inputFormat = sys->fnAMediaCodec_getInputFormat(sys->decoder);
	if (inputFormat == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputFormat failed");
		goto EXCEPTION;
	}
	set_mediacodec_format(h264, &sys->inputFormat, inputFormat);

	media_format = sys->fnAMediaFormat_toString(sys->inputFormat);
	if (media_format == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
		goto EXCEPTION;
	}
	WLog_Print(h264->log, WLOG_DEBUG, "Using MediaCodec with input MediaFormat [%s]", media_format);

	outputFormat = sys->fnAMediaCodec_getOutputFormat(sys->decoder);
	if (outputFormat == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getOutputFormat failed");
		goto EXCEPTION;
	}
	set_mediacodec_format(h264, &sys->outputFormat, outputFormat);

	media_format = sys->fnAMediaFormat_toString(sys->outputFormat);
	if (media_format == NULL)
	{
		WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
		goto EXCEPTION;
	}
	WLog_Print(h264->log, WLOG_DEBUG, "Using MediaCodec with output MediaFormat [%s]",
	           media_format);

	WLog_Print(h264->log, WLOG_DEBUG, "Starting MediaCodec");
	status = sys->fnAMediaCodec_start(sys->decoder);
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

H264_CONTEXT_SUBSYSTEM g_Subsystem_mediacodec = { "MediaCodec", mediacodec_init, mediacodec_uninit,
	                                              mediacodec_decompress, mediacodec_compress };
