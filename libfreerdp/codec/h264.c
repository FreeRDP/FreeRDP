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

#define USE_DUMP_IMAGE	0
#define USE_GRAY_SCALE	0

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
	/*
	 * Documented colorspace conversion from YUV to RGB.
	 * See http://msdn.microsoft.com/en-us/library/ms893078.aspx
	 */

#define clip(x) ((x) & 0xFF)

	int C, D, E;

	C = Y - 16;
	D = U - 128;
	E = V - 128;

	R = clip(( 298 * C           + 409 * E + 128) >> 8);
	G = clip(( 298 * C - 100 * D - 208 * E + 128) >> 8);
	B = clip(( 298 * C + 516 * D           + 128) >> 8);
#endif

	return RGB32(R, G, B);
}

#if USE_DUMP_IMAGE
static void h264_dump_i420_image(BYTE* imageData, int imageWidth, int imageHeight, int* imageStride)
{
	static int frame_num;

	FILE* fp;
	char buffer[64];
	BYTE* yp;
	int x, y;

	sprintf(buffer, "/tmp/h264_frame_%d.ppm", frame_num++);
	fp = fopen(buffer, "wb");
	fwrite("P5\n", 1, 3, fp);
	sprintf(buffer, "%d %d\n", imageWidth, imageHeight);
	fwrite(buffer, 1, strlen(buffer), fp);
	fwrite("255\n", 1, 4, fp);

	yp = imageData;
	for (y = 0; y < imageHeight; y++)
	{
		fwrite(yp, 1, imageWidth, fp);
		yp += imageStride[0];
	}

	fclose(fp);
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

	printf("h264_decompress: pSrcData=%p, SrcSize=%u, pDstData=%p, DstFormat=%lx, nDstStep=%d, nXDst=%d, nYDst=%d, nWidth=%d, nHeight=%d)\n",
		pSrcData, SrcSize, *ppDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight);

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

	printf("h264_decompress: state=%u, pYUVData=[%p,%p,%p], bufferStatus=%d, width=%d, height=%d, format=%d, stride=[%d,%d]\n",
		state, pYUVData[0], pYUVData[1], pYUVData[2], sBufferInfo.iBufferStatus,
		pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iFormat,
		pSystemBuffer->iStride[0], pSystemBuffer->iStride[1]);

	if (state != 0)
		return -1;

	if (!pYUVData[0] || !pYUVData[1] || !pYUVData[2])
		return -1;

	if (sBufferInfo.iBufferStatus != 1)
		return -1;

	if (pSystemBuffer->iFormat != videoFormatI420)
		return -1;

	/* Convert I420 (same as IYUV) to XRGB. */

#if USE_DUMP_IMAGE
	h264_dump_i420_image(pY, pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iStride);
#endif

	pY = pYUVData[0];
	pU = pYUVData[1];
	pV = pYUVData[2];

	for (j = 0; j < nHeight; j++)
	{
		BYTE *pXRGB = pDstData + ((nYDst + j) * nDstStep) + (nXDst * 4);
		int y = nYDst + j;

		for (i = 0; i < nWidth; i++)
		{
			int x = nXDst + i;

			Y = pY[(y * pSystemBuffer->iStride[0]) + x];
			U = pU[(y/2) * pSystemBuffer->iStride[1] + (x/2)];
			V = pV[(y/2) * pSystemBuffer->iStride[1] + (x/2)];

			*(UINT32*)pXRGB = YUV_to_RGB(Y, U, V);
		
			pXRGB += 4;
		}
	}
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
<<<<<<< HEAD
	H264_CONTEXT* h264;
=======
	long status;
	H264_CONTEXT* h264;
	SDecodingParam sDecParam;
	static EVideoFormatType videoFormat = videoFormatI420;
>>>>>>> 5c5386fe042de2a638a03e856afa3ec89f9d1a12

	h264 = (H264_CONTEXT*) calloc(1, sizeof(H264_CONTEXT));

	if (h264)
	{
		h264->Compressor = Compressor;

<<<<<<< HEAD
#ifdef WITH_OPENH264
=======
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
>>>>>>> 5c5386fe042de2a638a03e856afa3ec89f9d1a12
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
