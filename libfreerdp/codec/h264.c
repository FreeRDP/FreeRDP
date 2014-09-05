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

#include <freerdp/codec/color.h>
#include <freerdp/codec/h264.h>

static INLINE BYTE clip(int x)
{
	if (x < 0) return 0;
	if (x > 255) return 255;
	return (BYTE) x;
}

static INLINE UINT32 YUV_to_RGB(BYTE Y, BYTE U, BYTE V)
{
	int C, D, E;
	BYTE R, G, B;

	C = Y;
	D = U - 128;
	E = V - 128;

	R = clip(( 256 * C           + 403 * E + 128) >> 8);
	G = clip(( 256 * C -  48 * D - 120 * E + 128) >> 8);
	B = clip(( 256 * C + 475 * D           + 128) >> 8);

	return RGB32(R, G, B);
}

static int g_H264FrameId = 0;
static BOOL g_H264DumpFrames = FALSE;

static void h264_dump_h264_data(BYTE* data, int size)
{
	FILE* fp;
	char buf[4096];

	sprintf_s(buf, sizeof(buf), "/tmp/wlog/bs_%d.h264", g_H264FrameId);
	fp = fopen(buf, "wb");
	fwrite(data, 1, size, fp);
	fflush(fp);
	fclose(fp);
}

void h264_dump_yuv_data(BYTE* yuv[], int width, int height, int stride[])
{
	FILE* fp;
	BYTE* srcp;
	char buf[4096];
	int j;

	sprintf_s(buf, sizeof(buf), "/tmp/wlog/H264_%d.ppm", g_H264FrameId);
	fp = fopen(buf, "wb");
	fwrite("P5\n", 1, 3, fp);
	sprintf_s(buf, sizeof(buf), "%d %d\n", width, height);
	fwrite(buf, 1, strlen(buf), fp);
	fwrite("255\n", 1, 4, fp);

	srcp = yuv[0];

	for (j = 0; j < height; j++)
	{
		fwrite(srcp, 1, width, fp);
		srcp += stride[0];
	}

	fflush(fp);
	fclose(fp);
}

int h264_prepare_rgb_buffer(H264_CONTEXT* h264, int width, int height)
{
	UINT32 size;

	h264->width = width;
	h264->height = height;
	h264->scanline = h264->width * 4;
	size = h264->scanline * h264->height;

	if (size > h264->size)
	{
		h264->size = size;
		h264->data = (BYTE*) realloc(h264->data, h264->size);
		memset(h264->data, 0, h264->size);
	}

	if (!h264->data)
		return -1;

	return 1;
}

int freerdp_image_copy_yuv420p_to_xrgb(BYTE* pDstData, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData[3], int nSrcStep[2], int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* pDstPixel8;
	BYTE *pY, *pU, *pV;
	int shift = 1;

	pY = pSrcData[0] + (nYSrc * nSrcStep[0]) + nXSrc;

	pDstPixel8 = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

	for (y = 0; y < nHeight; y++)
	{
		pU = pSrcData[1] + ((nYSrc + y) >> shift) * nSrcStep[1];
		pV = pSrcData[2] + ((nYSrc + y) >> shift) * nSrcStep[1];

		for (x = 0; x < nWidth; x++)
		{
			BYTE Y, U, V;

			Y = *pY;
			U = pU[(nXSrc + x) >> shift];
			V = pV[(nXSrc + x) >> shift];

			*((UINT32*) pDstPixel8) = YUV_to_RGB(Y, U, V);

			pDstPixel8 += 4;
			pY++;
		}

		pDstPixel8 += (nDstStep - (nWidth * 4));
		pY += (nSrcStep[0] - nWidth);
	}

	return 1;
}

/**
 * Dummy subsystem
 */

static int dummy_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
	BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
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
	printf("%d - %s\n", level, message);
}

static int openh264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
	BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	BYTE* pYUVData[3];
	DECODING_STATE state;
	SBufferInfo sBufferInfo;
	SSysMEMBuffer* pSystemBuffer;
	H264_CONTEXT_OPENH264* sys = (H264_CONTEXT_OPENH264*) h264->pSystemData;

	if (!sys->pDecoder)
		return -1;

	/*
	 * Decompress the image.  The RDP host only seems to send I420 format.
	 */

	pYUVData[0] = NULL;
	pYUVData[1] = NULL;
	pYUVData[2] = NULL;

	ZeroMemory(&sBufferInfo, sizeof(sBufferInfo));

	state = (*sys->pDecoder)->DecodeFrame2(
		sys->pDecoder,
		pSrcData,
		SrcSize,
		pYUVData,
		&sBufferInfo);

	/**
	 * Calling DecodeFrame2 twice apparently works around Openh264 issue #1136:
	 * https://github.com/cisco/openh264/issues/1136
	 *
	 * This is a hack, but it works and it is only necessary for the first frame.
	 */

	if (sBufferInfo.iBufferStatus != 1)
		state = (*sys->pDecoder)->DecodeFrame2(sys->pDecoder, NULL, 0, pYUVData, &sBufferInfo);

	pSystemBuffer = &sBufferInfo.UsrData.sSystemBuffer;

#if 0
	printf("h264_decompress: state=%u, pYUVData=[%p,%p,%p], bufferStatus=%d, width=%d, height=%d, format=%d, stride=[%d,%d]\n",
		state, pYUVData[0], pYUVData[1], pYUVData[2], sBufferInfo.iBufferStatus,
		pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iFormat,
		pSystemBuffer->iStride[0], pSystemBuffer->iStride[1]);
#endif

	if (state != 0)
		return -1;

	if (!pYUVData[0] || !pYUVData[1] || !pYUVData[2])
		return -1;

	if (sBufferInfo.iBufferStatus != 1)
		return -1;

	if (pSystemBuffer->iFormat != videoFormatI420)
		return -1;

	/* Convert I420 (same as IYUV) to XRGB. */

	if (g_H264DumpFrames)
	{
		h264_dump_yuv_data(pYUVData, pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iStride);
	}

	g_H264FrameId++;

	if (h264_prepare_rgb_buffer(h264, pSystemBuffer->iWidth, pSystemBuffer->iHeight) < 0)
		return -1;

	freerdp_image_copy_yuv420p_to_xrgb(h264->data, h264->scanline, 0, 0,
			h264->width, h264->height, pYUVData, pSystemBuffer->iStride, 0, 0);

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
		printf("Failed to create OpenH264 decoder\n");
		goto EXCEPTION;
	}

	ZeroMemory(&sDecParam, sizeof(sDecParam));
	sDecParam.iOutputColorFormat  = videoFormatI420;
	sDecParam.uiEcActiveFlag = 1;
	sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

	status = (*sys->pDecoder)->Initialize(sys->pDecoder, &sDecParam);

	if (status != 0)
	{
		printf("Failed to initialize OpenH264 decoder (status=%ld)\n", status);
		goto EXCEPTION;
	}

	status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_DATAFORMAT, &videoFormat);

	if (status != 0)
	{
		printf("Failed to set data format option on OpenH264 decoder (status=%ld)\n", status);
	}

	if (g_openh264_trace_enabled)
	{
		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_LEVEL, &traceLevel);

		if (status != 0)
		{
			printf("Failed to set trace level option on OpenH264 decoder (status=%ld)\n", status);
		}

		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_CALLBACK, &traceCallback);

		if (status != 0)
		{
			printf("Failed to set trace callback option on OpenH264 decoder (status=%ld)\n", status);
		}

		status = (*sys->pDecoder)->SetOption(sys->pDecoder, DECODER_OPTION_TRACE_CALLBACK_CONTEXT, &h264);

		if (status != 0)
		{
			printf("Failed to set trace callback context option on OpenH264 decoder (status=%ld)\n", status);
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

static int libavcodec_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
	BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
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
		printf("Failed to decode video frame (status=%d)\n", status);
		return -1;
	}

	printf("libavcodec_decompress: frame decoded (status=%d, gotFrame=%d, width=%d, height=%d, Y=[%p,%d], U=[%p,%d], V=[%p,%d])\n",
		status, gotFrame, sys->videoFrame->width, sys->videoFrame->height,
		sys->videoFrame->data[0], sys->videoFrame->linesize[0],
		sys->videoFrame->data[1], sys->videoFrame->linesize[1],
		sys->videoFrame->data[2], sys->videoFrame->linesize[2]);

	fflush(stdout);

	if (gotFrame)
	{
		if (g_H264DumpFrames)
		{
			h264_dump_yuv_data(sys->videoFrame->data, sys->videoFrame->width, sys->videoFrame->height, sys->videoFrame->linesize);
		}

		if (h264_prepare_rgb_buffer(h264, sys->videoFrame->width, sys->videoFrame->height) < 0)
			return -1;

		freerdp_image_copy_yuv420p_to_xrgb(h264->data, h264->scanline, 0, 0,
			h264->width, h264->height, sys->videoFrame->data, sys->videoFrame->linesize, 0, 0);
	}

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
		printf("Failed to find libav H.264 codec\n");
		goto EXCEPTION;
	}

	sys->codecContext = avcodec_alloc_context3(sys->codec);

	if (!sys->codecContext)
	{
		printf("Failed to allocate libav codec context\n");
		goto EXCEPTION;
	}

	if (sys->codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		sys->codecContext->flags |= CODEC_FLAG_TRUNCATED;
	}

	if (avcodec_open2(sys->codecContext, sys->codec, NULL) < 0)
	{
		printf("Failed to open libav codec\n");
		goto EXCEPTION;
	}

	sys->codecParser = av_parser_init(CODEC_ID_H264);

	if (!sys->codecParser)
	{
		printf("Failed to initialize libav parser\n");
		goto EXCEPTION;
	}

	sys->videoFrame = avcodec_alloc_frame();

	if (!sys->videoFrame)
	{
		printf("Failed to allocate libav frame\n");
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
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	BYTE* pDstData;
	UINT32 UncompressedSize;

	if (!h264)
		return -1;

#if 0
	printf("h264_decompress: pSrcData=%p, SrcSize=%u, pDstData=%p, nDstStep=%d, nXDst=%d, nYDst=%d, nWidth=%d, nHeight=%d)\n",
		pSrcData, SrcSize, *ppDstData, nDstStep, nXDst, nYDst, nWidth, nHeight);
#endif

	/* Allocate a destination buffer (if needed). */

	UncompressedSize = nWidth * nHeight * 4;

	if (UncompressedSize == 0)
		return -1;

	pDstData = *ppDstData;

	if (!pDstData)
	{
		pDstData = (BYTE*) malloc(UncompressedSize);

		if (!pDstData)
			return -1;

		*ppDstData = pDstData;
	}

	if (g_H264DumpFrames)
	{
		h264_dump_h264_data(pSrcData, SrcSize);
	}

	return h264->subsystem->Decompress(h264, pSrcData, SrcSize,
			pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight);
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

H264_CONTEXT* h264_context_new(BOOL Compressor)
{
	H264_CONTEXT* h264;

	h264 = (H264_CONTEXT*) calloc(1, sizeof(H264_CONTEXT));

	if (h264)
	{
		h264->Compressor = Compressor;

		h264->subsystem = &g_Subsystem_dummy;

		if (h264_prepare_rgb_buffer(h264, 256, 256) < 0)
			return NULL;

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
		free(h264->data);

		h264->subsystem->Uninit(h264);

		free(h264);
	}
}
