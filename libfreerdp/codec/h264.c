/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/primitives.h>
#include <freerdp/codec/h264.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("codec")

/**
 * Dummy subsystem
 */

static int dummy_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize)
{
	return -1;
}

static void dummy_uninit(H264_CONTEXT* h264)
{

}

static BOOL dummy_init(H264_CONTEXT* h264)
{
	return TRUE;
}

static H264_CONTEXT_SUBSYSTEM g_Subsystem_dummy =
{
	"dummy",
	dummy_init,
	dummy_uninit,
	dummy_decompress
};

/**
 * OpenH264 subsystem
 */

#ifdef WITH_OPENH264

#include "wels/codec_def.h"
#include "wels/codec_api.h"

struct _H264_CONTEXT_OPENH264
{
	ISVCDecoder* pDecoder;
};
typedef struct _H264_CONTEXT_OPENH264 H264_CONTEXT_OPENH264;

static BOOL g_openh264_trace_enabled = FALSE;

static void openh264_trace_callback(H264_CONTEXT* h264, int level, const char* message)
{
	WLog_INFO(TAG, "%d - %s", level, message);
}

static int openh264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize)
{
	DECODING_STATE state;
	SBufferInfo sBufferInfo;
	SSysMEMBuffer* pSystemBuffer;
	H264_CONTEXT_OPENH264* sys = (H264_CONTEXT_OPENH264*) h264->pSystemData;

	if (!sys->pDecoder)
		return -1;

	/*
	 * Decompress the image.  The RDP host only seems to send I420 format.
	 */

	h264->pYUVData[0] = NULL;
	h264->pYUVData[1] = NULL;
	h264->pYUVData[2] = NULL;

	ZeroMemory(&sBufferInfo, sizeof(sBufferInfo));

	state = (*sys->pDecoder)->DecodeFrame2(
		sys->pDecoder,
		pSrcData,
		SrcSize,
		h264->pYUVData,
		&sBufferInfo);

	/**
	 * Calling DecodeFrame2 twice apparently works around Openh264 issue #1136:
	 * https://github.com/cisco/openh264/issues/1136
	 *
	 * This is a hack, but it works and it is only necessary for the first frame.
	 */

	if (sBufferInfo.iBufferStatus != 1)
		state = (*sys->pDecoder)->DecodeFrame2(sys->pDecoder, NULL, 0, h264->pYUVData, &sBufferInfo);

	pSystemBuffer = &sBufferInfo.UsrData.sSystemBuffer;

#if 0
	WLog_INFO(TAG, "h264_decompress: state=%u, pYUVData=[%p,%p,%p], bufferStatus=%d, width=%d, height=%d, format=%d, stride=[%d,%d]",
		state, h264->pYUVData[0], h264->pYUVData[1], h264->pYUVData[2], sBufferInfo.iBufferStatus,
		pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iFormat,
		pSystemBuffer->iStride[0], pSystemBuffer->iStride[1]);
#endif

	if (state != 0)
		return -1;

	if (sBufferInfo.iBufferStatus != 1)
		return -2;

	if (pSystemBuffer->iFormat != videoFormatI420)
		return -1;

	if (!h264->pYUVData[0] || !h264->pYUVData[1] || !h264->pYUVData[2])
		return -1;

	h264->iStride[0] = pSystemBuffer->iStride[0];
	h264->iStride[1] = pSystemBuffer->iStride[1];
	h264->iStride[2] = pSystemBuffer->iStride[1];

	h264->width = pSystemBuffer->iWidth;
	h264->height = pSystemBuffer->iHeight;

	return 1;
}

static void openh264_uninit(H264_CONTEXT* h264)
{
	H264_CONTEXT_OPENH264* sys = (H264_CONTEXT_OPENH264*) h264->pSystemData;

	if (sys)
	{
		if (sys->pDecoder)
		{
			(*sys->pDecoder)->Uninitialize(sys->pDecoder);
			WelsDestroyDecoder(sys->pDecoder);
			sys->pDecoder = NULL;
		}

		free(sys);
		h264->pSystemData = NULL;
	}
}

static BOOL openh264_init(H264_CONTEXT* h264)
{
	long status;
	SDecodingParam sDecParam;
	H264_CONTEXT_OPENH264* sys;
	static int traceLevel = WELS_LOG_DEBUG;
	static EVideoFormatType videoFormat = videoFormatI420;
	static WelsTraceCallback traceCallback = (WelsTraceCallback) openh264_trace_callback;

	sys = (H264_CONTEXT_OPENH264*) calloc(1, sizeof(H264_CONTEXT_OPENH264));

	if (!sys)
	{
		goto EXCEPTION;
	}

	h264->pSystemData = (void*) sys;

	WelsCreateDecoder(&sys->pDecoder);

	if (!sys->pDecoder)
	{
		WLog_ERR(TAG, "Failed to create OpenH264 decoder");
		goto EXCEPTION;
	}

	ZeroMemory(&sDecParam, sizeof(sDecParam));
	sDecParam.eOutputColorFormat  = videoFormatI420;
	sDecParam.eEcActiveIdc = ERROR_CON_FRAME_COPY;
	sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

	status = (*sys->pDecoder)->Initialize(sys->pDecoder, &sDecParam);

	if (status != 0)
	{
		WLog_ERR(TAG, "Failed to initialize OpenH264 decoder (status=%ld)", status);
		goto EXCEPTION;
	}

	status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_DATAFORMAT, &videoFormat);

	if (status != 0)
	{
		WLog_ERR(TAG, "Failed to set data format option on OpenH264 decoder (status=%ld)", status);
	}

	if (g_openh264_trace_enabled)
	{
		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_LEVEL, &traceLevel);

		if (status != 0)
		{
			WLog_ERR(TAG, "Failed to set trace level option on OpenH264 decoder (status=%ld)", status);
		}

		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_CALLBACK, &traceCallback);

		if (status != 0)
		{
			WLog_ERR(TAG, "Failed to set trace callback option on OpenH264 decoder (status=%ld)", status);
		}

		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_CALLBACK_CONTEXT, &h264);

		if (status != 0)
		{
			WLog_ERR(TAG, "Failed to set trace callback context option on OpenH264 decoder (status=%ld)", status);
		}
	}

	return TRUE;

EXCEPTION:
	openh264_uninit(h264);

	return FALSE;
}

static H264_CONTEXT_SUBSYSTEM g_Subsystem_OpenH264 =
{
	"OpenH264",
	openh264_init,
	openh264_uninit,
	openh264_decompress
};

#endif

/**
 * libavcodec subsystem
 */

#ifdef WITH_LIBAVCODEC

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

struct _H264_CONTEXT_LIBAVCODEC
{
	AVCodec* codec;
	AVCodecContext* codecContext;
	AVCodecParserContext* codecParser;
	AVFrame* videoFrame;
};
typedef struct _H264_CONTEXT_LIBAVCODEC H264_CONTEXT_LIBAVCODEC;

static int libavcodec_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize)
{
	int status;
	int gotFrame = 0;
	AVPacket packet;
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*) h264->pSystemData;

	av_init_packet(&packet);

	packet.data = pSrcData;
	packet.size = SrcSize;

	status = avcodec_decode_video2(sys->codecContext, sys->videoFrame, &gotFrame, &packet);

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to decode video frame (status=%d)", status);
		return -1;
	}

#if 0
	WLog_INFO(TAG, "libavcodec_decompress: frame decoded (status=%d, gotFrame=%d, width=%d, height=%d, Y=[%p,%d], U=[%p,%d], V=[%p,%d])",
		status, gotFrame, sys->videoFrame->width, sys->videoFrame->height,
		sys->videoFrame->data[0], sys->videoFrame->linesize[0],
		sys->videoFrame->data[1], sys->videoFrame->linesize[1],
		sys->videoFrame->data[2], sys->videoFrame->linesize[2]);
#endif

	if (gotFrame)
	{
		h264->pYUVData[0] = sys->videoFrame->data[0];
		h264->pYUVData[1] = sys->videoFrame->data[1];
		h264->pYUVData[2] = sys->videoFrame->data[2];

		h264->iStride[0] = sys->videoFrame->linesize[0];
		h264->iStride[1] = sys->videoFrame->linesize[1];
		h264->iStride[2] = sys->videoFrame->linesize[2];

		h264->width = sys->videoFrame->width;
		h264->height = sys->videoFrame->height;
	}
	else
		return -2;

	return 1;
}

static void libavcodec_uninit(H264_CONTEXT* h264)
{
	H264_CONTEXT_LIBAVCODEC* sys = (H264_CONTEXT_LIBAVCODEC*) h264->pSystemData;

	if (!sys)
		return;

	if (sys->videoFrame)
	{
		av_free(sys->videoFrame);
	}

	if (sys->codecParser)
	{
		av_parser_close(sys->codecParser);
	}

	if (sys->codecContext)
	{
		avcodec_close(sys->codecContext);
		av_free(sys->codecContext);
	}

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

	sys->codec = avcodec_find_decoder(CODEC_ID_H264);

	if (!sys->codec)
	{
		WLog_ERR(TAG, "Failed to find libav H.264 codec");
		goto EXCEPTION;
	}

	sys->codecContext = avcodec_alloc_context3(sys->codec);

	if (!sys->codecContext)
	{
		WLog_ERR(TAG, "Failed to allocate libav codec context");
		goto EXCEPTION;
	}

	if (sys->codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		sys->codecContext->flags |= CODEC_FLAG_TRUNCATED;
	}

	if (avcodec_open2(sys->codecContext, sys->codec, NULL) < 0)
	{
		WLog_ERR(TAG, "Failed to open libav codec");
		goto EXCEPTION;
	}

	sys->codecParser = av_parser_init(CODEC_ID_H264);

	if (!sys->codecParser)
	{
		WLog_ERR(TAG, "Failed to initialize libav parser");
		goto EXCEPTION;
	}

	sys->videoFrame = avcodec_alloc_frame();

	if (!sys->videoFrame)
	{
		WLog_ERR(TAG, "Failed to allocate libav frame");
		goto EXCEPTION;
	}

	return TRUE;

EXCEPTION:
	libavcodec_uninit(h264);

	return FALSE;
}

static H264_CONTEXT_SUBSYSTEM g_Subsystem_libavcodec =
{
	"libavcodec",
	libavcodec_init,
	libavcodec_uninit,
	libavcodec_decompress
};

#endif

int h264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nDstWidth,
		int nDstHeight, RDPGFX_RECT16* regionRects, int numRegionRects)
{
	int index;
	int status;
	int* iStride;
	BYTE* pDstData;
	BYTE* pDstPoint;
	prim_size_t roi;
	BYTE** pYUVData;
	int width, height;
	BYTE* pYUVPoint[3];
	RDPGFX_RECT16* rect;
	primitives_t *prims = primitives_get();

	if (!h264)
		return -1;

#if 0
	WLog_INFO(TAG, "h264_decompress: pSrcData=%p, SrcSize=%u, pDstData=%p, nDstStep=%d, nDstHeight=%d, numRegionRects=%d",
		pSrcData, SrcSize, *ppDstData, nDstStep, nDstHeight, numRegionRects);
#endif

	if (!(pDstData = *ppDstData))
		return -1;

	if ((status = h264->subsystem->Decompress(h264, pSrcData, SrcSize)) < 0)
		return status;

	pYUVData = h264->pYUVData;
	iStride = h264->iStride;

	for (index = 0; index < numRegionRects; index++)
	{
		rect = &(regionRects[index]);

		/* Check, if the ouput rectangle is valid in decoded h264 frame. */
		if ((rect->right > h264->width) || (rect->left > h264->width))
			return -1;
		if ((rect->top > h264->height) || (rect->bottom > h264->height))
			return -1;

		/* Check, if the output rectangle is valid in destination buffer. */
		if ((rect->right > nDstWidth) || (rect->left > nDstWidth))
			return -1;
		if ((rect->bottom > nDstHeight) || (rect->top > nDstHeight))
			return -1;

		width = rect->right - rect->left;
		height = rect->bottom - rect->top;
		
		pDstPoint = pDstData + rect->top * nDstStep + rect->left * 4;
		pYUVPoint[0] = pYUVData[0] + rect->top * iStride[0] + rect->left;

		pYUVPoint[1] = pYUVData[1] + rect->top/2 * iStride[1] + rect->left/2;
		pYUVPoint[2] = pYUVData[2] + rect->top/2 * iStride[2] + rect->left/2;

#if 0
		WLog_INFO(TAG, "regionRect: x: %d y: %d width: %d height: %d",
		       rect->left, rect->top, width, height);
#endif

		roi.width = width;
		roi.height = height;

		prims->YUV420ToRGB_8u_P3AC4R((const BYTE**) pYUVPoint, iStride, pDstPoint, nDstStep, &roi);
	}

	return 1;
}

int h264_compress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

BOOL h264_context_init(H264_CONTEXT* h264)
{
#ifdef WITH_LIBAVCODEC
	if (g_Subsystem_libavcodec.Init(h264))
	{
		h264->subsystem = &g_Subsystem_libavcodec;
		return TRUE;
	}
#endif

#ifdef WITH_OPENH264
	if (g_Subsystem_OpenH264.Init(h264))
	{
		h264->subsystem = &g_Subsystem_OpenH264;
		return TRUE;
	}
#endif

	return FALSE;
}

int h264_context_reset(H264_CONTEXT* h264)
{
	return 1;
}

H264_CONTEXT* h264_context_new(BOOL Compressor)
{
	H264_CONTEXT* h264;

	h264 = (H264_CONTEXT*) calloc(1, sizeof(H264_CONTEXT));

	if (h264)
	{
		h264->Compressor = Compressor;

		h264->subsystem = &g_Subsystem_dummy;

		if (!h264_context_init(h264))
		{
			free(h264);
			return NULL;
		}
	}

	return h264;
}

void h264_context_free(H264_CONTEXT* h264)
{
	if (h264)
	{
		h264->subsystem->Uninit(h264);

		free(h264);
	}
}
