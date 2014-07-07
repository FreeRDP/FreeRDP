/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

int clear_decompress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	UINT32 i;
	UINT32 x, y;
	UINT32 count;
	UINT32 color;
	int nXDstRel;
	int nYDstRel;
	int nSrcStep;
	BYTE seqNumber;
	BYTE glyphFlags;
	BYTE* glyphData;
	UINT16 glyphIndex;
	UINT32 offset = 0;
	BYTE* pDstData = NULL;
	UINT32 residualByteCount;
	UINT32 bandsByteCount;
	UINT32 subcodecByteCount;
	UINT32 runLengthFactor;
	UINT32 pixelX, pixelY;
	UINT32 pixelIndex = 0;
	UINT32 pixelCount = 0;
	BYTE* pSrcPixel8 = NULL;
	BYTE* pDstPixel8 = NULL;
	UINT32* pSrcPixel32 = NULL;
	UINT32* pDstPixel32 = NULL;

	if (!ppDstData)
		return -1001;

	pDstData = *ppDstData;

	if (!pDstData)
		return -1002;

	if (SrcSize < 2)
		return -1003;

	glyphFlags = pSrcData[0];
	seqNumber = pSrcData[1];
	offset += 2;

	if (seqNumber != clear->seqNumber)
		return -1004;

	clear->seqNumber = (seqNumber + 1) % 256;

	if (glyphFlags & CLEARCODEC_FLAG_CACHE_RESET)
	{
		clear->VBarStorageCursor = 0;
		clear->ShortVBarStorageCursor = 0;
	}

	//printf("glyphFlags: 0x%02X seqNumber: %d\n", glyphFlags, seqNumber);

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT) && !(glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX))
		return -1005;

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		if ((nWidth * nHeight) > (1024 * 1024))
			return -1006;

		if (SrcSize < 4)
			return -1007;

		glyphIndex = *((UINT16*) &pSrcData[2]);
		offset += 2;

		if (glyphIndex >= 4000)
			return -1008;

		if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
		{
			/**
			 * Copy pixels from the Decompressor Glyph Storage position
			 * specified by the glyphIndex field to the output bitmap
			 */

			glyphData = clear->GlyphCache[glyphIndex];

			if (!glyphData)
				return -1009;

			nSrcStep = nWidth * 4;
			pSrcPixel8 = glyphData;
			pDstPixel8 = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

			for (y = 0; y < nHeight; y++)
			{
				CopyMemory(pDstPixel8, pSrcPixel8, nSrcStep);
				pSrcPixel8 += nSrcStep;
				pDstPixel8 += nDstStep;
			}

			return 1; /* Finish */
		}
	}

	/* Read composition payload header parameters */

	if ((SrcSize - offset) < 12)
		return -1010;

	residualByteCount = *((UINT32*) &pSrcData[offset]);
	bandsByteCount = *((UINT32*) &pSrcData[offset + 4]);
	subcodecByteCount = *((UINT32*) &pSrcData[offset + 8]);
	offset += 12;

	printf("residualByteCount: %d bandsByteCount: %d subcodecByteCount: %d\n",
			residualByteCount, bandsByteCount, subcodecByteCount);

	if (residualByteCount > 0)
	{
		UINT32 suboffset;
		BYTE* residualData;

		if ((SrcSize - offset) < residualByteCount)
			return -1011;

		suboffset = 0;
		residualData = &pSrcData[offset];

		pixelIndex = 0;

		while (suboffset < residualByteCount)
		{
			if ((residualByteCount - suboffset) < 4)
				return -1012;

			color = RGB32(residualData[suboffset + 2], residualData[suboffset + 1], residualData[suboffset + 0]);
			suboffset += 3;

			runLengthFactor = (UINT32) residualData[suboffset];
			suboffset++;

			if (runLengthFactor >= 0xFF)
			{
				if ((residualByteCount - suboffset) < 2)
					return -1013;

				runLengthFactor = (UINT32) *((UINT16*) &residualData[suboffset]);
				suboffset += 2;

				if (runLengthFactor >= 0xFFFF)
				{
					if ((residualByteCount - suboffset) < 4)
						return -1014;

					runLengthFactor = *((UINT32*) &residualData[suboffset]);
					suboffset += 4;
				}
			}

			pixelX = (pixelIndex % nWidth);
			pixelY = (pixelIndex - pixelX) / nWidth;
			pixelCount = runLengthFactor;

			while (pixelCount > 0)
			{
				count = nWidth - pixelX;

				if (count > pixelCount)
					count = pixelCount;

				pDstPixel32 = (UINT32*) &pDstData[((nYDst + pixelY) * nDstStep) + ((nXDst + pixelX) * 4)];
				pixelCount -= count;

				while (count--)
				{
					*pDstPixel32 = color;
					pDstPixel32++;
				}

				pixelX = 0;
				pixelY++;
			}

			pixelIndex += runLengthFactor;
		}

		if (pixelIndex != (nWidth * nHeight))
			return -1015;

		offset += residualByteCount;
	}

	if (bandsByteCount > 0)
	{
		BYTE* bandsData;
		UINT32 suboffset;

		if ((SrcSize - offset) < bandsByteCount)
			return -1016;

		suboffset = 0;
		bandsData = &pSrcData[offset];

		while (suboffset < bandsByteCount)
		{
			UINT16 xStart;
			UINT16 xEnd;
			UINT16 yStart;
			UINT16 yEnd;
			BYTE* vBar;
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
				return -1017;

			xStart = *((UINT16*) &bandsData[suboffset]);
			xEnd = *((UINT16*) &bandsData[suboffset + 2]);
			yStart = *((UINT16*) &bandsData[suboffset + 4]);
			yEnd = *((UINT16*) &bandsData[suboffset + 6]);
			suboffset += 8;

			colorBkg = RGB32(bandsData[suboffset + 2], bandsData[suboffset + 1], bandsData[suboffset + 0]);
			suboffset += 3;

			if (xEnd < xStart)
				return -1018;

			if (yEnd < yStart)
				return -1019;

			vBarCount = (xEnd - xStart) + 1;

			//printf("CLEARCODEC_BAND: xStart: %d xEnd: %d yStart: %d yEnd: %d vBarCount: %d blueBkg: 0x%02X greenBkg: 0x%02X redBkg: 0x%02X\n",
			//		xStart, xEnd, yStart, yEnd, vBarCount, b, g, r);

			for (i = 0; i < vBarCount; i++)
			{
				vBarUpdate = FALSE;
				vBar = &bandsData[suboffset];

				if ((bandsByteCount - suboffset) < 2)
					return -1020;

				vBarHeader = *((UINT16*) &vBar[0]);
				suboffset += 2;

				vBarHeight = (yEnd - yStart + 1);

				if (vBarHeight > 52)
					return -1021;

				if ((vBarHeader & 0xC000) == 0x4000) /* SHORT_VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x3FFF);

					if (vBarIndex >= 16384)
						return -1022;

					if ((bandsByteCount - suboffset) < 1)
						return -1023;

					vBarYOn = vBar[2];
					suboffset += 1;

					//printf("SHORT_VBAR_CACHE_HIT: vBarIndex: %d vBarYOn: %d Cursor: %d / %d\n",
					//		vBarIndex, vBarYOn, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					vBarShortPixelCount = (yEnd - yStart + 1 - vBarYOn); /* should be maximum value */

					if (clear->ShortVBarStorageCursor >= 16384)
						return -1024;

					vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);

					if (!vBarShortEntry)
						return -1025;

					vBarShortPixelCount = vBarShortEntry->count;

					vBarUpdate = TRUE;
				}
				else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
				{
					vBarYOn = (vBarHeader & 0xFF);
					vBarYOff = ((vBarHeader >> 8) & 0x3F);

					if (vBarYOff < vBarYOn)
						return -1026;

					pSrcPixel8 = &vBar[2];
					vBarShortPixelCount = (vBarYOff - vBarYOn);

					if (vBarShortPixelCount > 52)
						return -1027;

					//printf("SHORT_VBAR_CACHE_MISS: vBarYOn: %d vBarYOff: %d vBarPixelCount: %d Cursor: %d / %d\n",
					//		vBarYOn, vBarYOff, vBarShortPixelCount, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					if ((bandsByteCount - suboffset) < (vBarShortPixelCount * 3))
						return -1028;

					if (clear->ShortVBarStorageCursor >= 16384)
						return -1029;

					vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);

					if (!vBarShortEntry->pixels)
						vBarShortEntry->pixels = (UINT32*) malloc(52 * 4);

					if (!vBarShortEntry->pixels)
						return -1030;

					pDstPixel32 = vBarShortEntry->pixels;

					for (y = 0; y < vBarShortPixelCount; y++)
					{
						*pDstPixel32 = RGB32(pSrcPixel8[2], pSrcPixel8[1], pSrcPixel8[0]);
						pSrcPixel8 += 3;
						pDstPixel32++;
					}

					suboffset += (vBarShortPixelCount * 3);

					vBarShortEntry->count = vBarShortPixelCount;
					clear->ShortVBarStorageCursor = (clear->ShortVBarStorageCursor + 1) % 16384;

					vBarUpdate = TRUE;
				}
				else if ((vBarHeader & 0x8000) == 0x8000) /* VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x7FFF);

					//printf("VBAR_CACHE_HIT: vBarIndex: %d Cursor: %d / %d\n",
					//		vBarIndex, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					if (vBarIndex >= 32768)
						return -1031;

					vBarEntry = &(clear->VBarStorage[vBarIndex]);
				}
				else
				{
					return -1032; /* invalid vBarHeader */
				}

				if (vBarUpdate)
				{
					if (clear->VBarStorageCursor >= 32768)
						return -1033;

					vBarEntry = &(clear->VBarStorage[clear->VBarStorageCursor]);

					if (!vBarEntry->pixels)
						vBarEntry->pixels = (UINT32*) malloc(52 * 4);

					if (!vBarEntry->pixels)
						return -1034;

					vBarPixelCount = vBarHeight;
					pDstPixel32 = vBarEntry->pixels;

					/* if (y < vBarYOn), use colorBkg */

					y = 0;
					count = vBarYOn;

					if ((y + count) > vBarPixelCount)
						count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					while (count--)
					{
						*pDstPixel32 = colorBkg;
						pDstPixel32++;
					}

					/*
					 * if ((y >= vBarYOn) && (y < (vBarYOn + vBarShortPixelCount))),
					 * use vBarShortPixels at index (y - shortVBarYOn)
					 */

					y = vBarYOn;
					count = vBarShortPixelCount;

					if ((y + count) > vBarPixelCount)
						count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					pSrcPixel32 = &(vBarShortEntry->pixels[y - vBarYOn]);
					CopyMemory(pDstPixel32, pSrcPixel32, count * 4);
					pDstPixel32 += count;

					/* if (y >= (vBarYOn + vBarShortPixelCount)), use colorBkg */

					y = vBarYOn + vBarShortPixelCount;
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					while (count--)
					{
						*pDstPixel32 = colorBkg;
						pDstPixel32++;
					}

					vBarEntry->count = vBarPixelCount;
					clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % 32768;
				}

				nXDstRel = nXDst + xStart;
				nYDstRel = nYDst + yStart;

				pSrcPixel32 = vBarEntry->pixels;
				pDstPixel8 = &pDstData[(nYDstRel * nDstStep) + ((nXDstRel + i) * 4)];

				count = yEnd - yStart + 1;

				if (vBarEntry->count < count) /* vBar is smaller */
				{
					for (y = 0; y < vBarEntry->count; y++)
					{
						*((UINT32*) pDstPixel8) = *pSrcPixel32;
						pDstPixel8 += nDstStep;
						pSrcPixel32++;
					}

					count -= vBarEntry->count;

					if (vBarEntry->count)
						color = vBarEntry->pixels[vBarEntry->count - 1];
					else
						color = 0;

					for (y = 0; y < count; y++)
					{
						*((UINT32*) pDstPixel8) = color;
						pDstPixel8 += nDstStep;
					}
				}
				else /* vBar is taller or equal */
				{
					for (y = 0; y < count; y++)
					{
						*((UINT32*) pDstPixel8) = *pSrcPixel32;
						pDstPixel8 += nDstStep;
						pSrcPixel32++;
					}
				}
			}
		}

		offset += bandsByteCount;
	}

	if (subcodecByteCount > 0)
	{
		UINT16 xStart;
		UINT16 yStart;
		UINT16 width;
		UINT16 height;
		BYTE* bitmapData;
		UINT32 bitmapDataOffset;
		UINT32 bitmapDataByteCount;
		BYTE subcodecId;
		BYTE* subcodecs;
		UINT32 suboffset;

		if ((SrcSize - offset) < subcodecByteCount)
			return -1035;

		suboffset = 0;
		subcodecs = &pSrcData[offset];

		while (suboffset < subcodecByteCount)
		{
			if ((subcodecByteCount - suboffset) < 13)
				return -1036;

			xStart = *((UINT16*) &subcodecs[suboffset]);
			yStart = *((UINT16*) &subcodecs[suboffset + 2]);
			width = *((UINT16*) &subcodecs[suboffset + 4]);
			height = *((UINT16*) &subcodecs[suboffset + 6]);
			bitmapDataByteCount = *((UINT32*) &subcodecs[suboffset + 8]);
			subcodecId = subcodecs[suboffset + 12];
			suboffset += 13;

			//printf("bitmapDataByteCount: %d subcodecByteCount: %d suboffset: %d subCodecId: %d\n",
			//		bitmapDataByteCount, subcodecByteCount, suboffset, subcodecId);

			if ((subcodecByteCount - suboffset) < bitmapDataByteCount)
				return -1037;

			nXDstRel = nXDst + xStart;
			nYDstRel = nYDst + yStart;

			if ((width * height * 4) > clear->TempSize)
			{
				clear->TempSize = (width * height * 4);
				clear->TempBuffer = (BYTE*) realloc(clear->TempBuffer, clear->TempSize);

				if (!clear->TempBuffer)
					return -1038;
			}

			bitmapData = &subcodecs[suboffset];

			if (subcodecId == 0) /* Uncompressed */
			{
				if (bitmapDataByteCount != (width * height * 3))
					return -1039;

				pSrcPixel8 = bitmapData;

				for (y = 0; y < height; y++)
				{
					pDstPixel32 = (UINT32*) &pDstData[((nYDstRel + y) * nDstStep) + (nXDstRel * 4)];

					for (x = 0; x < width; x++)
					{
						*pDstPixel32 = RGB32(pSrcPixel8[2], pSrcPixel8[1], pSrcPixel8[0]);
						pSrcPixel8 += 3;
						pDstPixel32++;
					}
				}
			}
			else if (subcodecId == 1) /* NSCodec */
			{
				if (nsc_process_message(clear->nsc, 32, width, height, bitmapData, bitmapDataByteCount) < 0)
					return -1040;

				nSrcStep = width * 4;
				pSrcPixel8 = clear->nsc->BitmapData;
				pDstPixel8 = &pDstData[(nYDstRel * nDstStep) + (nXDstRel * 4)];

				for (y = 0; y < height; y++)
				{
					CopyMemory(pDstPixel8, pSrcPixel8, nSrcStep);
					pSrcPixel8 += nSrcStep;
					pDstPixel8 += nDstStep;
				}
			}
			else if (subcodecId == 2) /* CLEARCODEC_SUBCODEC_RLEX */
			{
				UINT32 numBits;
				BYTE startIndex;
				BYTE stopIndex;
				BYTE suiteIndex;
				BYTE suiteDepth;
				BYTE paletteCount;
				UINT32 palette[128];

				paletteCount = bitmapData[0];
				pSrcPixel8 = &bitmapData[1];
				bitmapDataOffset = 1 + (paletteCount * 3);

				if (paletteCount > 127)
					return -1041;

				for (i = 0; i < paletteCount; i++)
				{
					palette[i] = RGB32(pSrcPixel8[2], pSrcPixel8[1], pSrcPixel8[0]);
					pSrcPixel8 += 3;
				}

				pixelCount = 0;
				pDstPixel32 = (UINT32*) clear->TempBuffer;

				numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

				while (bitmapDataOffset < bitmapDataByteCount)
				{
					if ((bitmapDataByteCount - bitmapDataOffset) < 2)
						return -1042;

					stopIndex = bitmapData[bitmapDataOffset] & CLEAR_8BIT_MASKS[numBits];
					suiteDepth = (bitmapData[bitmapDataOffset] >> numBits) & CLEAR_8BIT_MASKS[(8 - numBits)];
					startIndex = stopIndex - suiteDepth;
					bitmapDataOffset++;

					runLengthFactor = (UINT32) bitmapData[bitmapDataOffset];
					bitmapDataOffset++;

					if (runLengthFactor >= 0xFF)
					{
						if ((bitmapDataByteCount - bitmapDataOffset) < 2)
							return -1043;

						runLengthFactor = (UINT32) *((UINT16*) &bitmapData[bitmapDataOffset]);
						bitmapDataOffset += 2;

						if (runLengthFactor >= 0xFFFF)
						{
							if ((bitmapDataByteCount - bitmapDataOffset) < 4)
								return -1044;

							runLengthFactor = *((UINT32*) &bitmapData[bitmapDataOffset]);
							bitmapDataOffset += 4;
						}
					}

					if (startIndex >= paletteCount)
						return -1045;

					if (stopIndex >= paletteCount)
						return -1046;

					suiteIndex = startIndex;
					color = palette[suiteIndex];

					for (i = 0; i < runLengthFactor; i++)
					{
						*pDstPixel32 = color;
						pDstPixel32++;
					}

					for (i = 0; i <= suiteDepth; i++)
					{
						*pDstPixel32 = palette[suiteIndex++];
						pDstPixel32++;
					}

					pixelCount += (runLengthFactor + suiteDepth + 1);
				}

				pSrcPixel8 = clear->TempBuffer;
				pDstPixel8 = &pDstData[(nYDstRel * nDstStep) + (nXDstRel * 4)];

				if (pixelCount != (width * height))
					return -1047;

				for (y = 0; (y < height) && (pixelCount > 0); y++)
				{
					count = (width > pixelCount) ? pixelCount : width;
					nSrcStep = count * 4;

					CopyMemory(pDstPixel8, pSrcPixel8, nSrcStep);
					pSrcPixel8 += nSrcStep;
					pDstPixel8 += nDstStep;
					pixelCount -= count;
				}
			}
			else
			{
				return -1048;
			}

			suboffset += bitmapDataByteCount;
		}

		offset += subcodecByteCount;
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		/**
		 * Copy decompressed bitmap to the Decompressor Glyph
		 * Storage position specified by the glyphIndex field
		 */

		if (!clear->GlyphCache[glyphIndex])
			clear->GlyphCache[glyphIndex] = (BYTE*) malloc(1024 * 1024 * 4);

		glyphData = clear->GlyphCache[glyphIndex];

		if (!glyphData)
			return -1049;

		nSrcStep = nWidth * 4;
		pDstPixel8 = glyphData;
		pSrcPixel8 = &pDstData[(nYDst * nDstStep) + (nXDst * 4)];

		for (y = 0; y < nHeight; y++)
		{
			CopyMemory(pDstPixel8, pSrcPixel8, nSrcStep);
			pDstPixel8 += nSrcStep;
			pSrcPixel8 += nDstStep;
		}
	}

	if (offset != SrcSize)
		return -1050;

	return 1;
}

int clear_compress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

void clear_context_reset(CLEAR_CONTEXT* clear)
{
	clear->seqNumber = 0;
	clear->VBarStorageCursor = 0;
	clear->ShortVBarStorageCursor = 0;
}

CLEAR_CONTEXT* clear_context_new(BOOL Compressor)
{
	CLEAR_CONTEXT* clear;

	clear = (CLEAR_CONTEXT*) calloc(1, sizeof(CLEAR_CONTEXT));

	if (clear)
	{
		clear->Compressor = Compressor;

		clear->nsc = nsc_context_new();

		if (!clear->nsc)
			return NULL;

		nsc_context_set_pixel_format(clear->nsc, RDP_PIXEL_FORMAT_R8G8B8);

		clear->TempSize = 1024 * 1024 * 4;
		clear->TempBuffer = (BYTE*) malloc(clear->TempSize);

		clear_context_reset(clear);
	}

	return clear;
}

void clear_context_free(CLEAR_CONTEXT* clear)
{
	int i;

	if (!clear)
		return;

	nsc_context_free(clear->nsc);

	free(clear->TempBuffer);

	for (i = 0; i < 4000; i++)
		free(clear->GlyphCache[i]);

	for (i = 0; i < 32768; i++)
		free(clear->VBarStorage[i].pixels);

	for (i = 0; i < 16384; i++)
		free(clear->ShortVBarStorage[i].pixels);

	free(clear);
}

