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

#include <sys/time.h>

#ifdef WITH_H264_SSSE3
extern int freerdp_check_ssse3();
extern int freerdp_image_yuv420p_to_xrgb_ssse3(BYTE *pDstData,BYTE **pSrcData,int nWidth,int nHeight,int *iStride,int scanline);
#endif

#define USE_GRAY_SCALE	0
#define USE_UPCONVERT	0

static BYTE clip(int x)
{
	if (x < 0) return 0;
	if (x > 255) return 255;
	return (BYTE)x;
}

static UINT32 YUV_to_RGB(BYTE Y, BYTE U, BYTE V)
{
	BYTE R, G, B;

#if USE_GRAY_SCALE
	/*
	 * Displays the Y plane as a gray-scale image.
	 */
	R = Y;
	G = Y;
	B = Y;
#else
	int C, D, E;

#if 0
	/*
	 * Documented colorspace conversion from YUV to RGB.
	 * See http://msdn.microsoft.com/en-us/library/ms893078.aspx
	 */

	C = Y - 16;
	D = U - 128;
	E = V - 128;

	R = clip(( 298 * C           + 409 * E + 128) >> 8);
	G = clip(( 298 * C - 100 * D - 208 * E + 128) >> 8);
	B = clip(( 298 * C + 516 * D           + 128) >> 8);
#endif

#if 0
	/*
	 * These coefficients produce better results.
	 * See http://www.microchip.com/forums/m599060.aspx
	 */

	C = Y;
	D = U - 128;
	E = V - 128;

	R = clip(( 256 * C           + 359 * E + 128) >> 8);
	G = clip(( 256 * C -  88 * D - 183 * E + 128) >> 8);
	B = clip(( 256 * C + 454 * D           + 128) >> 8);
#endif

#if 1
	/*
	 * These coefficients produce excellent results.
	 */

	C = Y;
	D = U - 128;
	E = V - 128;

	R = clip(( 256 * C           + 403 * E + 128) >> 8);
	G = clip(( 256 * C -  48 * D - 120 * E + 128) >> 8);
	B = clip(( 256 * C + 475 * D           + 128) >> 8);
#endif

#endif

	return RGB32(R, G, B);
}

#if USE_UPCONVERT
static BYTE* h264_convert_420_to_444(BYTE* chroma420, int chroma420Width, int chroma420Height, int chroma420Stride)
{
	BYTE *chroma444, *src, *dst;
	int chroma444Width;
	int chroma444Height;
	int i, j;

	chroma444Width = chroma420Width * 2;
	chroma444Height = chroma420Height * 2;

	chroma444 = (BYTE*) malloc(chroma444Width * chroma444Height);

	if (!chroma444)
		return NULL;

	/* Upconvert in the horizontal direction. */

	for (j = 0; j < chroma420Height; j++)
	{
		src = chroma420 + j * chroma420Stride;
		dst = chroma444 + j * chroma444Width;
		dst[0] = src[0];
		for (i = 1; i < chroma420Width; i++)
		{
			dst[2*i-1] = (3 * src[i-1] + src[i] + 2) >> 2;
			dst[2*i] = (src[i-1] + 3 * src[i] + 2) >> 2;
		}
		dst[chroma444Width-1] = src[chroma420Width-1];
	}

	/* Upconvert in the vertical direction (in-place, bottom-up). */

	for (i = 0; i < chroma444Width; i++)   
	{
		src = chroma444 + i + (chroma420Height-2) * chroma444Width;
		dst = chroma444 + i + (2*(chroma420Height-2)+1) * chroma444Width;
		dst[2*chroma444Width] = src[chroma444Width];
		for (j = chroma420Height - 2; j >= 0; j--)
		{
			dst[chroma444Width] = (src[0] + 3 * src[chroma444Width] + 2) >> 2;
			dst[0] = (3 * src[0] + src[chroma444Width] + 2) >> 2;
			dst -= 2 * chroma444Width;
			src -= chroma444Width;
		}
	}

	return chroma444;
}
#endif

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

#ifdef WITH_LIBAVCODEC
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
		h264->data = (BYTE*) _aligned_realloc(h264->data, h264->size,16);
	}

	if (!h264->data)
		return -1;

	return 1;
}
#endif

int freerdp_image_copy_yuv420p_to_xrgb(BYTE* pDstData, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData[3], int nSrcStep[2], int nXSrc, int nYSrc)
{
	int x, y;
	BYTE* pDstPixel8;
	BYTE *pY, *pU, *pV;
	int shift = 1;

#if USE_UPCONVERT
	/* Convert 4:2:0 YUV to 4:4:4 YUV. */
	pSrcData[1] = h264_convert_420_to_444(pSrcData[1], nWidth / 2, nHeight / 2, nSrcStep[1]);
	pSrcData[2] = h264_convert_420_to_444(pSrcData[2], nWidth / 2, nHeight / 2, nSrcStep[1]);

	nSrcStep[1] = nWidth;

	shift = 0;
#endif

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

#if USE_UPCONVERT
	free(pSrcData[1]);
	free(pSrcData[2]);
#endif

	return 1;
}

BYTE* h264_strip_nal_unit_au_delimiter(BYTE* pSrcData, UINT32* pSrcSize)
{
	BYTE* data = pSrcData;
	UINT32 size = *pSrcSize;
	BYTE forbidden_zero_bit = 0;
	BYTE nal_ref_idc = 0;
	BYTE nal_unit_type = 0;

	/* ITU-T H.264 B.1.1 Byte stream NAL unit syntax */

	while (size > 0)
	{
		if (*data)
			break;

		data++;
		size--;
	}

	if (*data != 1)
		return pSrcData;

	data++;
	size--;

	forbidden_zero_bit = (data[0] >> 7);
	nal_ref_idc = (data[0] >> 5);
	nal_unit_type = (data[0] & 0x1F);

	if (forbidden_zero_bit)
		return pSrcData; /* invalid */

	if (nal_unit_type == 9)
	{
		/* NAL Unit AU Delimiter */

		printf("NAL Unit AU Delimiter: idc: %d\n", nal_ref_idc);

		data += 2;
		size -= 2;

		*pSrcSize = size;
		return data;
	}

	return pSrcData;
}



/*************************************************
 *
 * OpenH264 Implementation
 *
 ************************************************/

#ifdef WITH_OPENH264

static BOOL g_openh264_trace_enabled = FALSE;

static void openh264_trace_callback(H264_CONTEXT* h264, int level, const char* message)
{
	printf("%d - %s\n", level, message);
}

static int openh264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize)
{
	DECODING_STATE state;
	SBufferInfo sBufferInfo;
	SSysMEMBuffer* pSystemBuffer;
	
	struct timeval T1,T2;

	if (!h264->pDecoder)
		return -1;

	/*
	 * Decompress the image.  The RDP host only seems to send I420 format.
	 */

	h264->pYUVData[0] = NULL;
	h264->pYUVData[1] = NULL;
	h264->pYUVData[2] = NULL;

	ZeroMemory(&sBufferInfo, sizeof(sBufferInfo));

	gettimeofday(&T1,NULL);
	state = (*h264->pDecoder)->DecodeFrame2(
		h264->pDecoder,
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
		state = (*h264->pDecoder)->DecodeFrame2(h264->pDecoder, NULL, 0, h264->pYUVData, &sBufferInfo);
	
	gettimeofday(&T2,NULL);
	printf("OpenH264: decoding took: %u sec %u usec\n",(unsigned int)(T2.tv_sec-T1.tv_sec),(unsigned int)(T2.tv_usec-T1.tv_usec));

	pSystemBuffer = &sBufferInfo.UsrData.sSystemBuffer;

#if 0
	printf("h264_decompress: state=%u, pYUVData=[%p,%p,%p], bufferStatus=%d, width=%d, height=%d, format=%d, stride=[%d,%d]\n",
		state, h264->pYUVData[0], h264->pYUVData[1], h264->pYUVData[2], sBufferInfo.iBufferStatus,
		pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iFormat,
		pSystemBuffer->iStride[0], pSystemBuffer->iStride[1]);
#endif

	if (state != 0)
		return -1;

	if (!h264->pYUVData[0] || !h264->pYUVData[1] || !h264->pYUVData[2])
		return -1;

	if (sBufferInfo.iBufferStatus != 1)
		return -1;

	if (pSystemBuffer->iFormat != videoFormatI420)
		return -1;


	if (g_H264DumpFrames)
	{
		h264_dump_yuv_data(h264->pYUVData, pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iStride);
	}

	g_H264FrameId++;

	h264->iStride[0] = pSystemBuffer->iStride[0];
	h264->iStride[1] = pSystemBuffer->iStride[1];
	h264->width = pSystemBuffer->iWidth;
	h264->height = pSystemBuffer->iHeight;


	return 1;
}

static void openh264_free(H264_CONTEXT* h264)
{
	if (h264->pDecoder)
	{
		(*h264->pDecoder)->Uninitialize(h264->pDecoder);
		WelsDestroyDecoder(h264->pDecoder);
		h264->pDecoder = NULL;
	}
}

static BOOL openh264_init(H264_CONTEXT* h264)
{
	static EVideoFormatType videoFormat = videoFormatI420;

	static int traceLevel = WELS_LOG_DEBUG;
	static WelsTraceCallback traceCallback = (WelsTraceCallback) openh264_trace_callback;

	SDecodingParam sDecParam;
	long status;

	WelsCreateDecoder(&h264->pDecoder);

	if (!h264->pDecoder)
	{
		printf("Failed to create OpenH264 decoder\n");
		goto EXCEPTION;
	}

	ZeroMemory(&sDecParam, sizeof(sDecParam));
	sDecParam.iOutputColorFormat  = videoFormatI420;
	sDecParam.uiEcActiveFlag  = 1;
	sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

	status = (*h264->pDecoder)->Initialize(h264->pDecoder, &sDecParam);

	if (status != 0)
	{
		printf("Failed to initialize OpenH264 decoder (status=%ld)\n", status);
		goto EXCEPTION;
	}

	status = (*h264->pDecoder)->SetOption(h264->pDecoder, DECODER_OPTION_DATAFORMAT, &videoFormat);

	if (status != 0)
	{
		printf("Failed to set data format option on OpenH264 decoder (status=%ld)\n", status);
	}

	if (g_openh264_trace_enabled)
	{
		status = (*h264->pDecoder)->SetOption(h264->pDecoder, DECODER_OPTION_TRACE_LEVEL, &traceLevel);
		if (status != 0)
		{
			printf("Failed to set trace level option on OpenH264 decoder (status=%ld)\n", status);
		}

		status = (*h264->pDecoder)->SetOption(h264->pDecoder, DECODER_OPTION_TRACE_CALLBACK, &traceCallback);
		if (status != 0)
		{
			printf("Failed to set trace callback option on OpenH264 decoder (status=%ld)\n", status);
		}

		status = (*h264->pDecoder)->SetOption(h264->pDecoder, DECODER_OPTION_TRACE_CALLBACK_CONTEXT, &h264);
		if (status != 0)
		{
			printf("Failed to set trace callback context option on OpenH264 decoder (status=%ld)\n", status);
		}
	}

	return TRUE;

EXCEPTION:
	openh264_free(h264);

	return FALSE;
}

#endif



/*************************************************
 *
 * libavcodec Implementation
 *
 ************************************************/

#ifdef WITH_LIBAVCODEC

static int libavcodec_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
	BYTE* pDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	AVPacket packet;
	int gotFrame = 0;
	int status;
	
	struct timeval T1,T2;

	av_init_packet(&packet);

	packet.data = pSrcData;
	packet.size = SrcSize;

	gettimeofday(&T1,NULL);
	status = avcodec_decode_video2(h264->codecContext, h264->videoFrame, &gotFrame, &packet);
	gettimeofday(&T2,NULL);

	printf("libavcodec: decoding took: %u sec %u usec\n",(unsigned int)(T2.tv_sec-T1.tv_sec),(unsigned int)(T2.tv_usec-T1.tv_usec));

	if (status < 0)
	{
		printf("Failed to decode video frame (status=%d)\n", status);
		return -1;
	}

	printf("libavcodec_decompress: frame decoded (status=%d, gotFrame=%d, width=%d, height=%d, Y=[%p,%d], U=[%p,%d], V=[%p,%d])\n",
		status, gotFrame, h264->videoFrame->width, h264->videoFrame->height,
		h264->videoFrame->data[0], h264->videoFrame->linesize[0],
		h264->videoFrame->data[1], h264->videoFrame->linesize[1],
		h264->videoFrame->data[2], h264->videoFrame->linesize[2]);
	fflush(stdout);

	if (gotFrame)
	{
		if (g_H264DumpFrames)
		{
			h264_dump_yuv_data(h264->videoFrame->data, h264->videoFrame->width, h264->videoFrame->height, h264->videoFrame->linesize);
		}

		if (h264_prepare_rgb_buffer(h264, h264->videoFrame->width, h264->videoFrame->height) < 0)
			return -1;

		gettimeofday(&T1,NULL);
#ifdef WITH_H264_SSSE3
		freerdp_image_yuv420p_to_xrgb(h264->data,h264->videoFrame->data,h264->width,h264->height,h264->videoFrame->linesize[0],h264->videoFrame->linesize[1]);
#else
#ifdef WITH_H264_ASM
		freerdp_image_yuv_to_xrgb_asm(h264->data,h264->videoFrame->data,h264->width,h264->height,h264->videoFrame->linesize[0],h264->videoFrame->linesize[1]);
#else
		freerdp_image_copy_yuv420p_to_xrgb(h264->data, h264->scanline, 0, 0,
			h264->width, h264->height, h264->videoFrame->data, h264->videoFrame->linesize, 0, 0);
#endif
#endif
		gettimeofday(&T2,NULL);
		printf("\tconverting took %u sec %u usec\n",(unsigned int)(T2.tv_sec-T1.tv_sec),(unsigned int)(T2.tv_usec-T1.tv_usec));
	}

	return 1;
}

static void libavcodec_free(H264_CONTEXT* h264)
{
	if (h264->videoFrame)
	{
		av_free(h264->videoFrame);
	}

	if (h264->codecParser)
	{
		av_parser_close(h264->codecParser);
	}

	if (h264->codecContext)
	{
		avcodec_close(h264->codecContext);
		av_free(h264->codecContext);
	}
}

static BOOL libavcodec_init(H264_CONTEXT* h264)
{
	avcodec_register_all();

	h264->codec = avcodec_find_decoder(CODEC_ID_H264);
	if (!h264->codec)
	{
		printf("Failed to find libav H.264 codec\n");
		goto EXCEPTION;
	}

	h264->codecContext = avcodec_alloc_context3(h264->codec);
	if (!h264->codecContext)
	{
		printf("Failed to allocate libav codec context\n");
		goto EXCEPTION;
	}

	if (h264->codec->capabilities & CODEC_CAP_TRUNCATED)
	{
		h264->codecContext->flags |= CODEC_FLAG_TRUNCATED;
	}

	if (avcodec_open2(h264->codecContext, h264->codec, NULL) < 0)
	{
		printf("Failed to open libav codec\n");
		goto EXCEPTION;
	}

	h264->codecParser = av_parser_init(CODEC_ID_H264);
	if (!h264->codecParser)
	{
		printf("Failed to initialize libav parser\n");
		goto EXCEPTION;
	}

	h264->videoFrame = avcodec_alloc_frame();
	if (!h264->videoFrame)
	{
		printf("Failed to allocate libav frame\n");
		goto EXCEPTION;
	}

	return TRUE;

EXCEPTION:
	libavcodec_free(h264);

	return FALSE;
}

#endif



int h264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nDstHeight, RDPGFX_RECT16* regionRects, int numRegionRects)
{
	UINT32 UncompressedSize;
	BYTE* pDstData;
	BYTE* pDstPoint;

	BYTE** pYUVData;
	BYTE* pYUVPoint[3];

	RDPGFX_RECT16* rect;
	int* iStride;
	int ret, i, cx, cy;
	
	struct timeval T1,T2;

	if (!h264)
		return -1;

#if 0
	pSrcData = h264_strip_nal_unit_au_delimiter(pSrcData, &SrcSize);
#endif

#if 0
	printf("h264_decompress: pSrcData=%p, SrcSize=%u, pDstData=%p, nDstStep=%d, numRegionRects=%d\n",
		pSrcData, SrcSize, *ppDstData, nDstStep, numRegionRects);
#endif

	if (!(pDstData = *ppDstData))
		return -1;


	if (g_H264DumpFrames)
	{
		h264_dump_h264_data(pSrcData, SrcSize);
	}


#ifdef WITH_OPENH264
	ret = openh264_decompress(h264, pSrcData, SrcSize);
	if (ret != 1)
		return ret;
	
	pYUVData = h264->pYUVData;
	iStride = h264->iStride;
#endif

#ifdef WITH_LIBAVCODEC
	return libavcodec_decompress(
		h264, pSrcData, SrcSize,
		pDstData, DstFormat, nDstStep,
		nXDst, nYDst, nWidth, nHeight);
#endif


	/* Convert I420 (same as IYUV) to XRGB. */
	UncompressedSize = h264->width * h264->height * 4;
	if (UncompressedSize > (nDstStep * nDstHeight))
		return -1;


	gettimeofday(&T1,NULL);
	for (i = 0; i < numRegionRects; i++){
		rect = &(regionRects[i]);
		cx = rect->right - rect->left;
		cy = rect->bottom - rect->top;
		
		pDstPoint = pDstData + rect->top * nDstStep + rect->left * 4;
		pYUVPoint[0] = pYUVData[0] + rect->top * iStride[0] + rect->left;

		ret = rect->top/2 * iStride[1] + rect->left/2;
		pYUVPoint[1] = pYUVData[1] + ret;
		pYUVPoint[2] = pYUVData[2] + ret;

#if 0
		printf("regionRect: x: %d, y: %d, cx: %d, cy: %d\n",
		       rect->left, rect->top, cx, cy);
#endif

#ifdef WITH_H264_SSSE3
		freerdp_image_yuv420p_to_xrgb_ssse3(pDstPoint, pYUVPoint, cx, cy, iStride, nDstStep);
#else
		freerdp_image_copy_yuv420p_to_xrgb(pDstPoint, nDstStep, 0, 0,
			cx, cy, pYUVPoint, iStride, 0, 0);
#endif
	}
	gettimeofday(&T2,NULL);
	printf("converting took %u sec %u usec\n",(unsigned int)(T2.tv_sec-T1.tv_sec),(unsigned int)(T2.tv_usec-T1.tv_usec));

	return 1;
}

int h264_compress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

void h264_context_reset(H264_CONTEXT* h264)
{
}

H264_CONTEXT* h264_context_new(BOOL Compressor)
{
	H264_CONTEXT* h264;

	h264 = (H264_CONTEXT*) calloc(1, sizeof(H264_CONTEXT));

#ifdef WITH_H264_SSSE3
	if(freerdp_check_ssse3()){
		printf("SSSE3 seems to be not supported on this system, try without WITH_H264_SSSE3 ...");
		return NULL;
	}
#endif

	if (h264)
	{
		h264->Compressor = Compressor;

#ifdef WITH_OPENH264
		if (!openh264_init(h264))
		{
			free(h264);
			return NULL;
		}
#endif

#ifdef WITH_LIBAVCODEC
		if (!libavcodec_init(h264))
		{
			free(h264);
			return NULL;
		}
#endif
			
		h264_context_reset(h264);
	}

	return h264;
}

void h264_context_free(H264_CONTEXT* h264)
{
	if (h264)
	{
#ifdef WITH_OPENH264
		openh264_free(h264);
#endif

#ifdef WITH_LIBAVCODEC
		libavcodec_free(h264);
		_aligned_free(h264->data);
#endif

		free(h264);
	}
}
