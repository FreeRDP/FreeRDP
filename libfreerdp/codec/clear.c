/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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
#include <freerdp/codec/clear.h>

static UINT32 CLEAR_LOG2_FLOOR[256] =
{
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static BYTE CLEAR_8BIT_MASKS[9] =
{
	0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF
};

static void convert_color(BYTE* dst, UINT32 nDstStep, UINT32 DstFormat,
                          UINT32 nXDst, UINT32 nYDst,
                          const BYTE* src, UINT32 nSrcStep, UINT32 SrcFormat,
                          UINT32 nWidth, UINT32 nHeight)
{
	UINT32 x, y;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* pSrcLine = &src[y * nSrcStep];
		BYTE* pDstLine = &dst[(nYDst + y) * nDstStep];

		for (x = 0; x < nWidth; x++)
		{
			const BYTE* pSrcPixel =
			    &pSrcLine[x * GetBytesPerPixel(SrcFormat)];
			BYTE* pDstPixel =
			    &pDstLine[(nXDst + x) * GetBytesPerPixel(DstFormat)];
			UINT32 color = ReadColor(pSrcPixel, SrcFormat);
			color = ConvertColor(color, SrcFormat,
			                     DstFormat, NULL);
			WriteColor(pDstPixel, DstFormat, color);
		}
	}
}

static BOOL clear_decompress_nscodec(NSC_CONTEXT* nsc, UINT32 width,
                                     UINT32 height,
                                     const BYTE* bitmapData, UINT32 bitmapDataByteCount,
                                     BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                     UINT32 nXDstRel, UINT32 nYDstRel)
{
	UINT32 nSrcStep;

	if (nsc_process_message(nsc, 32, width, height, bitmapData,
	                        bitmapDataByteCount) < 0)
		return FALSE;

	nSrcStep = width * GetBytesPerPixel(nsc->format);
	convert_color(pDstData, nDstStep, DstFormat,
	              nXDstRel, nYDstRel,
	              nsc->BitmapData, nSrcStep, nsc->format,
	              width, height);
	return TRUE;
}

static BOOL clear_decompress_subcode_rlex(const BYTE* bitmapData,
        UINT32 bitmapDataByteCount,
        UINT32 width, UINT32 height,
        BYTE* tmpBuffer, UINT32 nTmpBufferSize,
        BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
        UINT32 nXDstRel, UINT32 nYDstRel)
{
	UINT32 i;
	UINT32 SrcFormat = PIXEL_FORMAT_BGR24;
	UINT32 pixelCount;
	UINT32 nSrcStep;
	UINT32 bitmapDataOffset;
	UINT32 pixelIndex;
	UINT32 numBits;
	BYTE startIndex;
	BYTE stopIndex;
	BYTE suiteIndex;
	BYTE suiteDepth;
	BYTE paletteCount;
	const BYTE* pSrcPixel8;
	UINT32 palette[128];
	paletteCount = bitmapData[0];
	pSrcPixel8 = &bitmapData[1];
	bitmapDataOffset = 1 + (paletteCount * 3);

	if (paletteCount > 127)
		return -1047;

	for (i = 0; i < paletteCount; i++)
	{
		UINT32 color = GetColor(SrcFormat,
		                        pSrcPixel8[2],
		                        pSrcPixel8[1],
		                        pSrcPixel8[0],
		                        0xFF);
		palette[i] = color;
		pSrcPixel8 += GetBytesPerPixel(SrcFormat);
	}

	pixelIndex = 0;
	pixelCount = width * height;
	numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

	while (bitmapDataOffset < bitmapDataByteCount)
	{
		UINT32 color;
		UINT32 runLengthFactor;

		if ((bitmapDataByteCount - bitmapDataOffset) < 2)
			return FALSE;

		stopIndex = bitmapData[bitmapDataOffset] & CLEAR_8BIT_MASKS[numBits];
		suiteDepth = (bitmapData[bitmapDataOffset] >> numBits) & CLEAR_8BIT_MASKS[(8 -
		             numBits)];
		startIndex = stopIndex - suiteDepth;
		bitmapDataOffset++;
		runLengthFactor = (UINT32) bitmapData[bitmapDataOffset];
		bitmapDataOffset++;

		if (runLengthFactor >= 0xFF)
		{
			if ((bitmapDataByteCount - bitmapDataOffset) < 2)
				return FALSE;

			runLengthFactor = (UINT32) * ((UINT16*) &bitmapData[bitmapDataOffset]);
			bitmapDataOffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if ((bitmapDataByteCount - bitmapDataOffset) < 4)
					return FALSE;

				runLengthFactor = *((UINT32*) &bitmapData[bitmapDataOffset]);
				bitmapDataOffset += 4;
			}
		}

		if (startIndex >= paletteCount)
			return FALSE;

		if (stopIndex >= paletteCount)
			return FALSE;

		suiteIndex = startIndex;
		color = palette[suiteIndex];

		if ((pixelIndex + runLengthFactor) > pixelCount)
			return FALSE;

		for (i = 0; i < runLengthFactor; i++)
		{
			WriteColor(tmpBuffer, SrcFormat, color);
			tmpBuffer += GetBytesPerPixel(SrcFormat);
		}

		pixelIndex += runLengthFactor;

		if ((pixelIndex + (suiteDepth + 1)) > pixelCount)
			return FALSE;

		for (i = 0; i <= suiteDepth; i++)
		{
			UINT32 color = palette[suiteIndex++];
			WriteColor(tmpBuffer, SrcFormat, color);
			tmpBuffer += GetBytesPerPixel(SrcFormat);
		}

		pixelIndex += (suiteDepth + 1);
	}

	nSrcStep = width * GetBytesPerPixel(SrcFormat);

	if (pixelIndex != pixelCount)
		return -1055;

	convert_color(pDstData, nDstStep, DstFormat,
	              nXDstRel, nYDstRel,
	              tmpBuffer, nSrcStep, SrcFormat,
	              width, height);
	return TRUE;
}

static BOOL clear_decompress_residual_data(CLEAR_CONTEXT* clear,
        const BYTE* residualData,
        UINT32 residualByteCount, UINT32 SrcSize,
        UINT32 nWidth, UINT32 nHeight,
        BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
        UINT32 nXDst, UINT32 nYDst)
{
	UINT32 i;
	UINT32 nSrcStep;
	UINT32 suboffset;
	BYTE* dstBuffer;
	UINT32 pixelIndex;
	UINT32 pixelCount;

	if (SrcSize < residualByteCount)
		return -1013;

	suboffset = 0;

	if ((nWidth * nHeight * GetBytesPerPixel(clear->format)) > clear->TempSize)
	{
		clear->TempSize = (nWidth * nHeight * GetBytesPerPixel(clear->format));
		clear->TempBuffer = (BYTE*) realloc(clear->TempBuffer, clear->TempSize);

		if (!clear->TempBuffer)
			return -1014;
	}

	pixelIndex = 0;
	pixelCount = nWidth * nHeight;
	dstBuffer = clear->TempBuffer;

	while (suboffset < residualByteCount)
	{
		UINT32 runLengthFactor;
		UINT32 color;

		if ((residualByteCount - suboffset) < 4)
			return -1015;

		color = GetColor(clear->format, residualData[suboffset + 2],
		                 residualData[suboffset + 1],
		                 residualData[suboffset + 0], 0xFF);
		suboffset += 3;
		runLengthFactor = residualData[suboffset];
		suboffset++;

		if (runLengthFactor >= 0xFF)
		{
			if ((residualByteCount - suboffset) < 2)
				return -1016;

			runLengthFactor = (UINT32) * ((UINT16*) &residualData[suboffset]);
			suboffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if ((residualByteCount - suboffset) < 4)
					return -1017;

				runLengthFactor = *((UINT32*) &residualData[suboffset]);
				suboffset += 4;
			}
		}

		if ((pixelIndex + runLengthFactor) > pixelCount)
			return -1018;

		for (i = 0; i < runLengthFactor; i++)
		{
			WriteColor(dstBuffer, clear->format, color);
			dstBuffer += GetBytesPerPixel(clear->format);
		}

		pixelIndex += runLengthFactor;
	}

	nSrcStep = nWidth * GetBytesPerPixel(clear->format);

	if (pixelIndex != pixelCount)
		return -1019;

	convert_color(pDstData, nDstStep, DstFormat,
	              nXDst, nYDst,
	              clear->TempBuffer, nSrcStep, clear->format,
	              nWidth, nHeight);
	return TRUE;
}

static BOOL clear_decompress_bands_data(CLEAR_CONTEXT* clear,
                                        const BYTE* bandsData,
                                        UINT32 bandsByteCount, UINT32 SrcSize,
                                        UINT32 nWidth, UINT32 nHeight,
                                        BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                        UINT32 nXDst, UINT32 nYDst)
{
	UINT32 i, y;
	UINT32 count;
	UINT32 suboffset;
	UINT32 nXDstRel;
	UINT32 nYDstRel;

	if (SrcSize < bandsByteCount)
		return -1020;

	suboffset = 0;

	while (suboffset < bandsByteCount)
	{
		UINT16 xStart;
		UINT16 xEnd;
		UINT16 yStart;
		UINT16 yEnd;
		const BYTE* vBar;
		BOOL vBarUpdate;
		UINT32 colorBkg;
		UINT16 vBarHeader;
		UINT16 vBarIndex;
		UINT16 vBarYOn;
		UINT16 vBarYOff;
		UINT32 vBarCount;
		UINT32 vBarHeight;
		UINT32 vBarPixelCount;
		UINT32 vBarShortPixelCount;
		CLEAR_VBAR_ENTRY* vBarEntry;
		CLEAR_VBAR_ENTRY* vBarShortEntry;

		if ((bandsByteCount - suboffset) < 11)
			return -1021;

		xStart = *((UINT16*) &bandsData[suboffset]);
		xEnd = *((UINT16*) &bandsData[suboffset + 2]);
		yStart = *((UINT16*) &bandsData[suboffset + 4]);
		yEnd = *((UINT16*) &bandsData[suboffset + 6]);
		suboffset += 8;
		colorBkg = GetColor(clear->format, bandsData[suboffset + 2],
		                    bandsData[suboffset + 1],
		                    bandsData[suboffset + 0],
		                    0xFF);
		suboffset += 3;

		if (xEnd < xStart)
			return -1022;

		if (yEnd < yStart)
			return -1023;

		vBarCount = (xEnd - xStart) + 1;

		for (i = 0; i < vBarCount; i++)
		{
			const BYTE* pSrcPixel;
			vBarUpdate = FALSE;
			vBar = &bandsData[suboffset];

			if ((bandsByteCount - suboffset) < 2)
				return -1024;

			vBarHeader = *((UINT16*) &vBar[0]);
			suboffset += 2;
			vBarHeight = (yEnd - yStart + 1);

			if (vBarHeight > 52)
				return -1025;

			if ((vBarHeader & 0xC000) == 0x4000) /* SHORT_VBAR_CACHE_HIT */
			{
				vBarIndex = (vBarHeader & 0x3FFF);

				if (vBarIndex >= 16384)
					return -1026;

				if ((bandsByteCount - suboffset) < 1)
					return -1027;

				vBarYOn = vBar[2];
				suboffset += 1;
				vBarShortEntry = &(clear->ShortVBarStorage[vBarIndex]);

				if (!vBarShortEntry)
					return -1028;

				vBarShortPixelCount = vBarShortEntry->count;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
			{
				const BYTE* pSrcPixel8;
				vBarYOn = (vBarHeader & 0xFF);
				vBarYOff = ((vBarHeader >> 8) & 0x3F);

				if (vBarYOff < vBarYOn)
					return -1029;

				pSrcPixel8 = &vBar[2];
				vBarShortPixelCount = (vBarYOff - vBarYOn);

				if (vBarShortPixelCount > 52)
					return -1030;

				if ((bandsByteCount - suboffset) < (vBarShortPixelCount * 3))
					return -1031;

				if (clear->ShortVBarStorageCursor >= 16384)
					return -1032;

				vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);
				vBarShortEntry->count = vBarShortPixelCount;

				if (vBarShortEntry->count > vBarShortEntry->size)
				{
					BYTE* tmp;
					vBarShortEntry->size = vBarShortEntry->count;
					tmp = (BYTE*) realloc(
					          vBarShortEntry->pixels,
					          vBarShortEntry->count * GetBytesPerPixel(clear->format));

					if (!tmp)
						return -1;

					vBarShortEntry->pixels = tmp;
				}

				if (!vBarShortEntry->pixels && vBarShortEntry->size)
					return -1033;

				for (y = 0; y < vBarShortPixelCount; y++)
				{
					BYTE* dstBuffer =
					    &vBarShortEntry->pixels[y * GetBytesPerPixel(clear->format)];
					UINT32 color = GetColor(clear->format,
					                        pSrcPixel8[2],
					                        pSrcPixel8[1],
					                        pSrcPixel8[0],
					                        0xFF);
					WriteColor(dstBuffer, clear->format, color);
					pSrcPixel8 += 3;
				}

				suboffset += (vBarShortPixelCount * 3);
				vBarShortEntry->count = vBarShortPixelCount;
				clear->ShortVBarStorageCursor = (clear->ShortVBarStorageCursor + 1) % 16384;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0x8000) == 0x8000) /* VBAR_CACHE_HIT */
			{
				vBarIndex = (vBarHeader & 0x7FFF);

				if (vBarIndex >= 32768)
					return -1034;

				vBarEntry = &(clear->VBarStorage[vBarIndex]);
			}
			else
			{
				return -1035; /* invalid vBarHeader */
			}

			if (vBarUpdate)
			{
				UINT32 x;
				BYTE* pSrcPixel;
				BYTE* dstBuffer;

				if (clear->VBarStorageCursor >= 32768)
					return -1036;

				vBarEntry = &(clear->VBarStorage[clear->VBarStorageCursor]);
				vBarPixelCount = vBarHeight;
				vBarEntry->count = vBarPixelCount;

				if (vBarEntry->count > vBarEntry->size)
				{
					BYTE* tmp;
					vBarEntry->size = vBarEntry->count;
					tmp = (BYTE*) realloc(vBarEntry->pixels,
					                      vBarEntry->count * GetBytesPerPixel(clear->format));

					if (!tmp)
						return -1;

					vBarEntry->pixels = tmp;
				}

				if (!vBarEntry->pixels && vBarEntry->size)
					return -1037;

				dstBuffer = vBarEntry->pixels;
				/* if (y < vBarYOn), use colorBkg */
				y = 0;
				count = vBarYOn;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				while (count--)
				{
					WriteColor(dstBuffer, clear->format, colorBkg);
					dstBuffer += GetBytesPerPixel(clear->format);
				}

				/*
				 * if ((y >= vBarYOn) && (y < (vBarYOn + vBarShortPixelCount))),
				 * use vBarShortPixels at index (y - shortVBarYOn)
				 */
				y = vBarYOn;
				count = vBarShortPixelCount;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				pSrcPixel = &vBarShortEntry->pixels[(y - vBarYOn) * GetBytesPerPixel(
				                                        clear->format)];

				for (x = 0; x < count; x++)
				{
					UINT32 color;
					color =	ReadColor(&vBarShortEntry->pixels[x * GetBytesPerPixel(clear->format)],
					                  clear->format);
					WriteColor(dstBuffer, clear->format, color);
					dstBuffer += GetBytesPerPixel(clear->format);
				}

				/* if (y >= (vBarYOn + vBarShortPixelCount)), use colorBkg */
				y = vBarYOn + vBarShortPixelCount;
				count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				while (count--)
				{
					WriteColor(dstBuffer, clear->format, colorBkg);
					dstBuffer += GetBytesPerPixel(clear->format);
				}

				vBarEntry->count = vBarPixelCount;
				clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % 32768;
			}

			nXDstRel = nXDst + xStart;
			nYDstRel = nYDst + yStart;
			pSrcPixel = vBarEntry->pixels;
			count = yEnd - yStart + 1;

			if (vBarEntry->count != count)
				return -1038;

			for (y = 0; y < count; y++)
			{
				BYTE* pDstPixel8 = &pDstData[((nYDstRel + y) * nDstStep) +
				                             ((nXDstRel + i) * GetBytesPerPixel(DstFormat))];
				UINT32 color = ReadColor(pSrcPixel, clear->format);
				color = ConvertColor(color, clear->format,
				                     DstFormat, NULL);
				WriteColor(pDstPixel8, DstFormat, color);
				pSrcPixel += GetBytesPerPixel(clear->format);
			}
		}
	}

	return TRUE;
}

INT32 clear_decompress(CLEAR_CONTEXT* clear, const BYTE* pSrcData,
                       UINT32 SrcSize,
                       BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                       UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight)
{
	BYTE seqNumber;
	BYTE glyphFlags;
	BYTE* glyphData;
	UINT32 offset = 0;
	UINT16 glyphIndex = 0;
	UINT32 residualByteCount;
	UINT32 bandsByteCount;
	UINT32 subcodecByteCount;
	CLEAR_GLYPH_ENTRY* glyphEntry;

	if (!pDstData)
		return -1002;

	if (SrcSize < 2)
		return -1003;

	if ((nWidth > 0xFFFF) || (nHeight > 0xFFFF))
		return -1004;

	glyphFlags = pSrcData[0];
	seqNumber = pSrcData[1];
	offset += 2;

	if (!clear->seqNumber && seqNumber)
		clear->seqNumber = seqNumber;

	if (seqNumber != clear->seqNumber)
		return -1005;

	clear->seqNumber = (seqNumber + 1) % 256;

	if (glyphFlags & CLEARCODEC_FLAG_CACHE_RESET)
	{
		clear->VBarStorageCursor = 0;
		clear->ShortVBarStorageCursor = 0;
	}

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT) &&
	    !(glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX))
		return -1006;

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		if ((nWidth * nHeight) > (1024 * 1024))
			return -1007;

		if (SrcSize < 4)
			return -1008;

		glyphIndex = *((UINT16*) &pSrcData[2]);
		offset += 2;

		if (glyphIndex >= 4000)
			return -1009;

		if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
		{
			UINT32 nSrcStep;
			glyphEntry = &(clear->GlyphCache[glyphIndex]);
			glyphData = (BYTE*) glyphEntry->pixels;

			if (!glyphData)
				return -1010;

			if ((nWidth * nHeight) > (int) glyphEntry->count)
				return -1011;

			nSrcStep = nWidth * GetBytesPerPixel(clear->format);
			convert_color(pDstData, nDstStep, DstFormat,
			              nXDst, nYDst,
			              glyphData, nSrcStep, clear->format,
			              nWidth, nHeight);
			return 1; /* Finish */
		}
	}

	/* Read composition payload header parameters */
	if ((SrcSize - offset) < 12)
		return -1012;

	residualByteCount = *((UINT32*) &pSrcData[offset]);
	bandsByteCount = *((UINT32*) &pSrcData[offset + 4]);
	subcodecByteCount = *((UINT32*) &pSrcData[offset + 8]);
	offset += 12;

	if (residualByteCount > 0)
	{
		if (!clear_decompress_residual_data(clear,
		                                    &pSrcData[offset], residualByteCount,
		                                    SrcSize - offset, nWidth, nHeight,
		                                    pDstData, DstFormat, nDstStep, nXDst, nYDst))
			return -1111;

		offset += residualByteCount;
	}

	if (bandsByteCount > 0)
	{
		if (!clear_decompress_bands_data(clear,
		                                 &pSrcData[offset], bandsByteCount,
		                                 SrcSize - offset, nWidth, nHeight,
		                                 pDstData, DstFormat, nDstStep, nXDst, nYDst))
			return FALSE;

		offset += bandsByteCount;
	}

	if (subcodecByteCount > 0)
	{
		UINT16 xStart;
		UINT16 yStart;
		UINT16 width;
		UINT16 height;
		const BYTE* bitmapData;
		UINT32 bitmapDataByteCount;
		BYTE subcodecId;
		const BYTE* subcodecs;
		UINT32 suboffset;

		if ((SrcSize - offset) < subcodecByteCount)
			return -1039;

		suboffset = 0;
		subcodecs = &pSrcData[offset];

		while (suboffset < subcodecByteCount)
		{
			UINT32 nXDstRel;
			UINT32 nYDstRel;

			if ((subcodecByteCount - suboffset) < 13)
				return -1040;

			xStart = *((UINT16*) &subcodecs[suboffset]);
			yStart = *((UINT16*) &subcodecs[suboffset + 2]);
			width = *((UINT16*) &subcodecs[suboffset + 4]);
			height = *((UINT16*) &subcodecs[suboffset + 6]);
			bitmapDataByteCount = *((UINT32*) &subcodecs[suboffset + 8]);
			subcodecId = subcodecs[suboffset + 12];
			suboffset += 13;

			if ((subcodecByteCount - suboffset) < bitmapDataByteCount)
				return -1041;

			nXDstRel = nXDst + xStart;
			nYDstRel = nYDst + yStart;

			if (width > nWidth)
				return -1042;

			if (height > nHeight)
				return -1043;

			if (((UINT32)(width * height * GetBytesPerPixel(clear->format))) >
			    clear->TempSize)
			{
				clear->TempSize = (width * height * GetBytesPerPixel(clear->format));
				clear->TempBuffer = (BYTE*) realloc(clear->TempBuffer, clear->TempSize);

				if (!clear->TempBuffer)
					return -1044;
			}

			bitmapData = &subcodecs[suboffset];

			if (subcodecId == 0) /* Uncompressed */
			{
				UINT32 nSrcStep = width * height * GetBytesPerPixel(PIXEL_FORMAT_BGR24);

				if (bitmapDataByteCount != nSrcStep)
					return -1045;

				convert_color(pDstData, nDstStep, DstFormat,
				              nXDstRel, nYDstRel,
				              bitmapData, nSrcStep,
				              PIXEL_FORMAT_BGR24,
				              width, height);
			}
			else if (subcodecId == 1) /* NSCodec */
			{
				if (!clear_decompress_nscodec(clear->nsc, width, height,
				                              bitmapData, bitmapDataByteCount,
				                              pDstData, DstFormat, nDstStep,
				                              nXDstRel, nYDstRel))
					return -1046;
			}
			else if (subcodecId == 2) /* CLEARCODEC_SUBCODEC_RLEX */
			{
				if (!clear_decompress_subcode_rlex(bitmapData,
				                                   bitmapDataByteCount,
				                                   width, height,
				                                   clear->TempBuffer, clear->TempSize,
				                                   pDstData, DstFormat, nDstStep,
				                                   nXDstRel, nYDstRel))
					return -1047;
			}
			else
			{
				return -1056;
			}

			suboffset += bitmapDataByteCount;
		}

		offset += subcodecByteCount;
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		UINT32 nSrcStep;
		glyphEntry = &(clear->GlyphCache[glyphIndex]);
		glyphEntry->count = nWidth * nHeight;

		if (glyphEntry->count > glyphEntry->size)
		{
			UINT32* tmp;
			glyphEntry->size = glyphEntry->count;
			tmp = (UINT32*) realloc(glyphEntry->pixels, glyphEntry->size * 4);

			if (!tmp)
				return -1;

			glyphEntry->pixels = tmp;
		}

		if (!glyphEntry->pixels)
			return -1057;

		glyphData = (BYTE*) glyphEntry->pixels;
		nSrcStep = nWidth * GetBytesPerPixel(clear->format);
		convert_color(pDstData, nDstStep, DstFormat,
		              nXDst, nYDst,
		              glyphData, nSrcStep, clear->format,
		              nWidth, nHeight);
	}

	if (offset != SrcSize)
		return -1058;

	return 1;
}

int clear_compress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize,
                   BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

BOOL clear_context_reset(CLEAR_CONTEXT* clear)
{
	if (!clear)
		return FALSE;

	clear->seqNumber = 0;
	clear->VBarStorageCursor = 0;
	clear->ShortVBarStorageCursor = 0;
	return TRUE;
}

CLEAR_CONTEXT* clear_context_new(BOOL Compressor)
{
	CLEAR_CONTEXT* clear;
	clear = (CLEAR_CONTEXT*) calloc(1, sizeof(CLEAR_CONTEXT));

	if (!clear)
		return NULL;

	clear->Compressor = Compressor;
	clear->nsc = nsc_context_new();
	clear->format = PIXEL_FORMAT_BGRX32;

	if (!clear->nsc)
		goto error_nsc;

	nsc_context_set_pixel_format(clear->nsc, PIXEL_FORMAT_RGB24);
	clear->TempSize = 512 * 512 * 4;
	clear->TempBuffer = (BYTE*) malloc(clear->TempSize);

	if (!clear->TempBuffer)
		goto error_temp_buffer;

	clear_context_reset(clear);
	return clear;
error_temp_buffer:
	nsc_context_free(clear->nsc);
error_nsc:
	free(clear);
	return NULL;
}

void clear_context_free(CLEAR_CONTEXT* clear)
{
	int i;

	if (!clear)
		return;

	nsc_context_free(clear->nsc);
	free(clear->TempBuffer);

	for (i = 0; i < 4000; i++)
		free(clear->GlyphCache[i].pixels);

	for (i = 0; i < 32768; i++)
		free(clear->VBarStorage[i].pixels);

	for (i = 0; i < 16384; i++)
		free(clear->ShortVBarStorage[i].pixels);

	free(clear);
}

