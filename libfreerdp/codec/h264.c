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
static BYTE* convert_420_to_444(BYTE* chroma420, int chroma420Width, int chroma420Height, int chroma420Stride)
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

int h264_decompress(H264_CONTEXT* h264, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
#ifdef WITH_OPENH264
	DECODING_STATE state;
	SBufferInfo sBufferInfo;
	SSysMEMBuffer* pSystemBuffer;
	UINT32 UncompressedSize;
	BYTE* pDstData;
	BYTE* pYUVData[3];
	BYTE* pY;
	BYTE* pU;
	BYTE* pV;
	int Y, U, V;
	int i, j;

	if (!h264 || !h264->pDecoder)
		return -1;

#if 0
	printf("h264_decompress: pSrcData=%p, SrcSize=%u, pDstData=%p, DstFormat=%lx, nDstStep=%d, nXDst=%d, nYDst=%d, nWidth=%d, nHeight=%d)\n",
		pSrcData, SrcSize, *ppDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight);
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

	/*
	 * Decompress the image.  The RDP host only seems to send I420 format.
	 */

	pYUVData[0] = NULL;
	pYUVData[1] = NULL;
	pYUVData[2] = NULL;

	ZeroMemory(&sBufferInfo, sizeof(sBufferInfo));

	state = (*h264->pDecoder)->DecodeFrame2(
		h264->pDecoder,
		pSrcData,
		SrcSize,
		pYUVData,
		&sBufferInfo);

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

	pY = pYUVData[0];
	pU = pYUVData[1];
	pV = pYUVData[2];

#if USE_UPCONVERT
	/* Convert 4:2:0 YUV to 4:4:4 YUV. */
	pU = convert_420_to_444(pU, pSystemBuffer->iWidth / 2, pSystemBuffer->iHeight / 2, pSystemBuffer->iStride[1]);
	pV = convert_420_to_444(pV, pSystemBuffer->iWidth / 2, pSystemBuffer->iHeight / 2, pSystemBuffer->iStride[1]);
#endif

	for (j = 0; j < nHeight; j++)
	{
		BYTE *pXRGB = pDstData + ((nYDst + j) * nDstStep) + (nXDst * 4);
		int y = nYDst + j;

		for (i = 0; i < nWidth; i++)
		{
			int x = nXDst + i;

			Y = pY[(y * pSystemBuffer->iStride[0]) + x];
#if USE_UPCONVERT
			U = pU[(y * pSystemBuffer->iWidth) + x];
			V = pV[(y * pSystemBuffer->iWidth) + x];
#else
			U = pU[(y/2) * pSystemBuffer->iStride[1] + (x/2)];
			V = pV[(y/2) * pSystemBuffer->iStride[1] + (x/2)];
#endif

			*(UINT32*)pXRGB = YUV_to_RGB(Y, U, V);
		
			pXRGB += 4;
		}
	}

#if USE_UPCONVERT
	free(pU);
	free(pV);
#endif
#endif

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

	if (h264)
	{
		h264->Compressor = Compressor;

#ifdef WITH_OPENH264
		{
			static EVideoFormatType videoFormat = videoFormatI420;

			SDecodingParam sDecParam;
			long status;

			WelsCreateDecoder(&h264->pDecoder);

			if (!h264->pDecoder)
			{
				printf("Failed to create OpenH264 decoder\n");
				goto EXCEPTION;
			}

			ZeroMemory(&sDecParam, sizeof(sDecParam));
			sDecParam.iOutputColorFormat = videoFormatARGB;
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
		}
#endif
			
		h264_context_reset(h264);
	}

	return h264;

EXCEPTION:
#ifdef WITH_OPENH264
	if (h264->pDecoder)
	{
		WelsDestroyDecoder(h264->pDecoder);
	}
#endif

	free(h264);

	return NULL;
}

void h264_context_free(H264_CONTEXT* h264)
{
	if (h264)
	{
#ifdef WITH_OPENH264
		if (h264->pDecoder)
		{
			(*h264->pDecoder)->Uninitialize(h264->pDecoder);
			WelsDestroyDecoder(h264->pDecoder);
		}
#endif

		free(h264);
	}
}
