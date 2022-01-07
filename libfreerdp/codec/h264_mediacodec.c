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
#include <freerdp/log.h>
#include <freerdp/codec/h264.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include "h264.h"

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
};

typedef struct _H264_CONTEXT_MEDIACODEC H264_CONTEXT_MEDIACODEC;

static void set_mediacodec_format(H264_CONTEXT* h264, AMediaFormat** formatVariable, AMediaFormat* newFormat)
{
    media_status_t status = AMEDIA_OK;

    if (*formatVariable != NULL)
    {
        status = AMediaFormat_delete(*formatVariable);
        if (status != AMEDIA_OK)
        {
            WLog_Print(h264->log, WLOG_ERROR, "Error AMediaFormat_delete %d", status);
        }
    }

    *formatVariable = newFormat;
}

static void release_current_outputbuffer(H264_CONTEXT* h264)
{
    H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
    media_status_t status = AMEDIA_OK;

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

static int mediacodec_compress(H264_CONTEXT* h264, const BYTE** pSrcYuv, const UINT32* pStride,
                               BYTE** ppDstData, UINT32* pDstSize)
{
    WLog_Print(h264->log, WLOG_ERROR, "MediaCodec is not supported as an encoder");
    return -1;
}

static int mediacodec_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize)
{
    H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
    ssize_t inputBufferId = -1;
    size_t inputBufferSize, outputBufferSize;
    uint8_t* inputBuffer;
    media_status_t status;
    const char* media_format;
    BYTE** pYUVData = h264->pYUVData;
	UINT32* iStride = h264->iStride;

    release_current_outputbuffer(h264);

    if (sys->width == 0)
    {
        int32_t width = h264->width;
        int32_t height = h264->height;
        if (width % 16 != 0)
            width += 16 - width % 16;
        if (height % 16 != 0)
            height += 16 - height % 16;

        WLog_Print(h264->log, WLOG_INFO, "MediaCodec setting width and height [%d,%d]", width, height);
        AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_HEIGHT, height);

        status = AMediaCodec_setParameters(sys->decoder, sys->inputFormat);
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
        inputBufferId = AMediaCodec_dequeueInputBuffer(sys->decoder, -1);
        if (inputBufferId < 0)
        {
            WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_dequeueInputBuffer failed [%d]", inputBufferId);
            // TODO: sleep?
            continue;
        }

        inputBuffer = AMediaCodec_getInputBuffer(sys->decoder, inputBufferId, &inputBufferSize);
        if (inputBuffer == NULL)
        {
            WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputBuffer failed");
            return -1;
        }

        if (SrcSize > inputBufferSize)
        {
            WLog_Print(h264->log, WLOG_ERROR, "MediaCodec input buffer size is too small: got [%d] but wanted [%d]", inputBufferSize, SrcSize);
            return -1;
        }

        memcpy(inputBuffer, pSrcData, SrcSize);
        status = AMediaCodec_queueInputBuffer(sys->decoder, inputBufferId, 0, SrcSize, 0, 0);
        if (status != AMEDIA_OK)
        {
            WLog_Print(h264->log, WLOG_ERROR, "Error AMediaCodec_queueInputBuffer %d", status);
            return -1;
        }

        while (true)
        {
            AMediaCodecBufferInfo bufferInfo;
            ssize_t outputBufferId = AMediaCodec_dequeueOutputBuffer(sys->decoder, &bufferInfo, -1);
            if (outputBufferId >= 0)
            {
                sys->currentOutputBufferIndex = outputBufferId;

                uint8_t* outputBuffer;
                outputBuffer = AMediaCodec_getOutputBuffer(sys->decoder, outputBufferId, &outputBufferSize);
                sys->currentOutputBufferIndex = outputBufferId;

                if (outputBufferSize != (sys->width * sys->height + ((sys->width + 1) / 2) * ((sys->height + 1) / 2) * 2))
                {
                    WLog_Print(h264->log, WLOG_ERROR, "Error unexpected output buffer size %d", outputBufferSize);
                    return -1;
                }

                // TODO: work with AImageReader and get AImage object instead of COLOR_FormatYUV420Planar buffer.
                iStride[0] = sys->width;
                iStride[1] = (sys->width + 1) / 2;
                iStride[2] = (sys->width + 1) / 2;
                pYUVData[0] = outputBuffer;
                pYUVData[1] = outputBuffer + iStride[0] * sys->height;
                pYUVData[2] = outputBuffer + iStride[0] * sys->height + iStride[1] * ((sys->height + 1) / 2);
                break;
            }
            else if (outputBufferId ==  AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
            {
                AMediaFormat* outputFormat;
                outputFormat = AMediaCodec_getOutputFormat(sys->decoder);
                if (outputFormat == NULL)
                {
                    WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getOutputFormat failed");
                    return -1;
                }
                set_mediacodec_format(h264, &sys->outputFormat, outputFormat);

                media_format = AMediaFormat_toString(sys->outputFormat);
                if (media_format == NULL)
                {
                    WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
                    return -1;
                }
            }
            else if (outputBufferId ==  AMEDIACODEC_INFO_TRY_AGAIN_LATER)
            {
                WLog_Print(h264->log, WLOG_WARN, "AMediaCodec_dequeueOutputBuffer need to try again later");
                // TODO: sleep?
            }
            else if (outputBufferId == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
            {
                WLog_Print(h264->log, WLOG_WARN, "AMediaCodec_dequeueOutputBuffer returned deprecated value AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED, ignoring");
            }
            else
            {
                WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_dequeueOutputBuffer returned unknown value [%d]", outputBufferId);
                return -1;
            }
        }

        break;
    }

    return 1;
}

static void mediacodec_uninit(H264_CONTEXT* h264)
{
    H264_CONTEXT_MEDIACODEC* sys = (H264_CONTEXT_MEDIACODEC*)h264->pSystemData;
    media_status_t status = AMEDIA_OK;

    WLog_Print(h264->log, WLOG_INFO, "Uninitializing MediaCodec");

	if (!sys)
		return;


    if (sys->decoder != NULL)
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

        sys->decoder = NULL;
    }

    set_mediacodec_format(h264, &sys->inputFormat, NULL);
    set_mediacodec_format(h264, &sys->outputFormat, NULL);

    free(sys);
	h264->pSystemData = NULL;
}

static BOOL mediacodec_init(H264_CONTEXT* h264)
{
    H264_CONTEXT_MEDIACODEC* sys;
    media_status_t status;
    char* codec_name, *media_format;
    AMediaFormat* inputFormat, *outputFormat;

    if (h264->Compressor)
    {
        WLog_Print(h264->log, WLOG_ERROR, "MediaCodec is not supported as an encoder");
        goto EXCEPTION;
    }

    WLog_Print(h264->log, WLOG_INFO, "Initializing MediaCodec");

	sys = (H264_CONTEXT_MEDIACODEC*)calloc(1, sizeof(H264_CONTEXT_MEDIACODEC));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*)sys;

    sys->currentOutputBufferIndex = -1;
    sys->width = sys->height = 0; // update when we're given the height and width
    sys->decoder = AMediaCodec_createDecoderByType("video/avc");
    if (sys->decoder == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_createCodecByName failed");
        goto EXCEPTION;
    }

    status = AMediaCodec_getName(sys->decoder, &codec_name);
    if (status != AMEDIA_OK)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getName failed: %d", status);
        goto EXCEPTION;
    }

    WLog_Print(h264->log, WLOG_INFO, "MediaCodec using video/avc codec [%s]", codec_name);
    AMediaCodec_releaseName(sys->decoder, codec_name);

    sys->inputFormat = AMediaFormat_new();
    if (sys->inputFormat == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_new failed");
        goto EXCEPTION;
    }

    AMediaFormat_setString(sys->inputFormat, AMEDIAFORMAT_KEY_MIME, "video/avc");
    AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_WIDTH, sys->width);
    AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_HEIGHT, sys->height);
    AMediaFormat_setInt32(sys->inputFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FormatYUV420Planar);

    media_format = AMediaFormat_toString(sys->inputFormat);
    if (media_format == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
        goto EXCEPTION;
    }

    status = AMediaCodec_configure(sys->decoder, sys->inputFormat, NULL, NULL, 0);
    if (status != AMEDIA_OK)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_configure failed: %d", status);
        goto EXCEPTION;
    }


    inputFormat = AMediaCodec_getInputFormat(sys->decoder);
    if (inputFormat == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getInputFormat failed");
        return -1;
    }
    set_mediacodec_format(h264, &sys->inputFormat, inputFormat);

    media_format = AMediaFormat_toString(sys->inputFormat);
    if (media_format == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
        goto EXCEPTION;
    }
    WLog_Print(h264->log, WLOG_INFO, "Using MediaCodec with input MediaFormat [%s]", media_format);

    outputFormat = AMediaCodec_getOutputFormat(sys->decoder);
    if (outputFormat == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaCodec_getOutputFormat failed");
        return -1;
    }
    set_mediacodec_format(h264, &sys->outputFormat, outputFormat);

    media_format = AMediaFormat_toString(sys->outputFormat);
    if (media_format == NULL)
    {
        WLog_Print(h264->log, WLOG_ERROR, "AMediaFormat_toString failed");
        goto EXCEPTION;
    }
    WLog_Print(h264->log, WLOG_INFO, "Using MediaCodec with output MediaFormat [%s]", media_format);


    WLog_Print(h264->log, WLOG_INFO, "Starting MediaCodec");
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

H264_CONTEXT_SUBSYSTEM g_Subsystem_mediacodec = { "MediaCodec", mediacodec_init, mediacodec_uninit,
	                                            mediacodec_decompress, mediacodec_compress };
