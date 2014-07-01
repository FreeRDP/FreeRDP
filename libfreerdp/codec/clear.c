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
	BYTE r, g, b;
	UINT32 color;
	BYTE seqNumber;
	BYTE glyphFlags;
	BYTE* glyphData;
	UINT16 glyphIndex;
	UINT32 offset = 0;
	BYTE* pDstData = NULL;
	UINT32 residualByteCount;
	UINT32 bandsByteCount;
	UINT32 subcodecByteCount;
	BYTE runLengthFactor1;
	UINT16 runLengthFactor2;
	UINT32 runLengthFactor3;
	UINT32 runLengthFactor;
	UINT32* pSrcPixel = NULL;
	UINT32* pDstPixel = NULL;

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

	if (glyphFlags & CLEARCODEC_FLAG_CACHE_RESET)
	{
		clear->VBarStorageCursor = 0;
		clear->ShortVBarStorageCursor = 0;
	}

	printf("glyphFlags: 0x%02X seqNumber: %d\n", glyphFlags, seqNumber);

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		if ((nWidth * nHeight) > (1024 * 1024))
			return -1004;

		if (SrcSize < 4)
			return -1005;

		glyphIndex = *((UINT16*) &pSrcData[2]);
		offset += 2;

		if (glyphIndex >= 4000)
			return -1006;

		if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
		{
			/**
			 * Copy pixels from the Decompressor Glyph Storage position
			 * specified by the glyphIndex field to the output bitmap
			 */

			glyphData = clear->GlyphCache[glyphIndex];

			if (!glyphData)
				return -1007;

			for (y = 0; y < nHeight; y++)
			{
				pSrcPixel = (UINT32*) &glyphData[y * (nWidth * 4)];
				pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];
				CopyMemory(pDstPixel, pSrcPixel, nWidth * 4);
			}

			return 1; /* Finish */
		}
	}

	/* Read composition payload header parameters */

	if ((SrcSize - offset) < 12)
		return -1008;

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
		UINT32* pDstPixel;
		UINT32 pixelX, pixelY;
		UINT32 pixelIndex = 0;
		UINT32 pixelCount = 0;

		if ((SrcSize - offset) < residualByteCount)
			return -1009;

		suboffset = 0;
		residualData = &pSrcData[offset];

		while (suboffset < residualByteCount)
		{
			if ((residualByteCount - suboffset) < 4)
				return -1010;

			b = residualData[suboffset]; /* blueValue */
			g = residualData[suboffset + 1]; /* greenValue */
			r = residualData[suboffset + 2]; /* redValue */
			color = RGB32(r, g, b);
			suboffset += 3;

			runLengthFactor1 = residualData[suboffset];
			runLengthFactor = runLengthFactor1;
			suboffset += 1;

			if (runLengthFactor1 >= 0xFF)
			{
				if ((residualByteCount - suboffset) < 2)
					return -1011;

				runLengthFactor2 = *((UINT16*) &residualData[suboffset]);
				runLengthFactor = runLengthFactor2;
				suboffset += 2;

				if (runLengthFactor2 >= 0xFFFF)
				{
					if ((residualByteCount - suboffset) < 4)
						return -1012;

					runLengthFactor3 = *((UINT32*) &residualData[suboffset]);
					runLengthFactor = runLengthFactor3;
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

				pDstPixel = (UINT32*) &pDstData[((nYDst + pixelY) * nDstStep) + ((nXDst + pixelX) * 4)];
				pixelCount -= count;

				while (count--)
				{
					*pDstPixel++ = color;
				}

				pixelX = 0;
				pixelY++;
			}

			pixelIndex += runLengthFactor;
		}

		if (pixelIndex != (nWidth * nHeight))
		{
			fprintf(stderr, "ClearCodec residual data unexpected pixel count: Actual: %d, Expected: %d\n",
					pixelIndex, (nWidth * nHeight));
		}

		offset += residualByteCount;
	}

	if (bandsByteCount > 0)
	{
		BYTE* bandsData;
		UINT32 suboffset;

		if ((SrcSize - offset) < bandsByteCount)
			return -1013;

		suboffset = 0;
		bandsData = &pSrcData[offset];

		while (suboffset < bandsByteCount)
		{
			UINT16 xStart;
			UINT16 xEnd;
			UINT16 yStart;
			UINT16 yEnd;
			BYTE* vBar;
			BYTE* vBarPixels;
			BOOL vBarUpdate;
			UINT16 vBarHeader;
			UINT16 vBarIndex;
			UINT16 vBarYOn;
			UINT16 vBarYOff;
			UINT32 vBarCount;
			UINT32 vBarPixelCount;
			UINT32 vBarShortPixelCount;
			CLEAR_VBAR_ENTRY* vBarEntry;
			CLEAR_VBAR_ENTRY* vBarShortEntry;

			if ((bandsByteCount - suboffset) < 11)
				return -1014;

			xStart = *((UINT16*) &bandsData[suboffset]);
			xEnd = *((UINT16*) &bandsData[suboffset + 2]);
			yStart = *((UINT16*) &bandsData[suboffset + 4]);
			yEnd = *((UINT16*) &bandsData[suboffset + 6]);
			b = bandsData[suboffset + 8]; /* blueBkg */
			g = bandsData[suboffset + 9]; /* greenBkg */
			r = bandsData[suboffset + 10]; /* redBkg */
			color = RGB32(r, g, b);
			suboffset += 11;

			vBarCount = (xEnd - xStart) + 1;

			//printf("CLEARCODEC_BAND: xStart: %d xEnd: %d yStart: %d yEnd: %d vBarCount: %d blueBkg: 0x%02X greenBkg: 0x%02X redBkg: 0x%02X\n",
			//		xStart, xEnd, yStart, yEnd, vBarCount, b, g, r);

			for (i = 0; i < vBarCount; i++)
			{
				vBarUpdate = FALSE;
				vBar = &bandsData[suboffset];

				if ((bandsByteCount - suboffset) < 2)
					return -1015;

				vBarHeader = *((UINT16*) &vBar[0]);
				suboffset += 2;

				vBarPixelCount = (yEnd - yStart + 1);

				if ((vBarHeader & 0xC000) == 0x8000) /* VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x7FFF);

					//printf("VBAR_CACHE_HIT: vBarIndex: %d Cursor: %d / %d\n",
					//		vBarIndex, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					if (vBarIndex >= 32768)
						return -1016;

					vBarEntry = &(clear->VBarStorage[vBarIndex]);
				}
				else if ((vBarHeader & 0xC000) == 0xC000) /* SHORT_VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x3FFF);

					if (vBarIndex >= 16384)
						return -1018;

					if ((bandsByteCount - suboffset) < 1)
						return -1019;

					vBarYOn = vBar[2];
					suboffset += 1;

					//printf("SHORT_VBAR_CACHE_HIT: vBarIndex: %d vBarYOn: %d Cursor: %d / %d\n",
					//		vBarIndex, vBarYOn, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					vBarShortPixelCount = (yEnd - yStart + 1 - vBarYOn); /* maximum value */

					vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);

					if (!vBarShortEntry)
						return -1020;

					if (vBarShortEntry->count > vBarShortPixelCount)
						return -1021;

					vBarShortPixelCount = vBarShortEntry->count;

					vBarUpdate = TRUE;
				}
				else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
				{
					vBarYOn = (vBarHeader & 0xFF);
					vBarYOff = ((vBarHeader >> 8) & 0x3F);

					if (vBarYOff < vBarYOn)
						return -1022;

					vBarPixels = &vBar[2];
					vBarShortPixelCount = (vBarYOff - vBarYOn);

					//printf("SHORT_VBAR_CACHE_MISS: vBarYOn: %d vBarYOff: %d vBarPixelCount: %d Cursor: %d / %d\n",
					//		vBarYOn, vBarYOff, vBarShortPixelCount, clear->VBarStorageCursor, clear->ShortVBarStorageCursor);

					if ((bandsByteCount - suboffset) < (vBarShortPixelCount * 3))
						return -1023;

					vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);

					if (vBarShortPixelCount && (vBarShortEntry->size < vBarShortPixelCount))
					{
						if (!vBarShortEntry->pixels)
							vBarShortEntry->pixels = (UINT32*) malloc(vBarShortPixelCount * 4);
						else
							vBarShortEntry->pixels = (UINT32*) realloc(vBarShortEntry->pixels, vBarShortPixelCount * 4);

						vBarShortEntry->size = vBarShortPixelCount;
					}

					if (vBarShortPixelCount && !vBarShortEntry->pixels)
						return -1024;

					pDstPixel = vBarShortEntry->pixels;

					for (y = 0; y < vBarShortPixelCount; y++)
					{
						*pDstPixel = RGB32(vBarPixels[2], vBarPixels[1], vBarPixels[0]);
						vBarPixels += 3;
						pDstPixel++;
					}

					suboffset += (vBarShortPixelCount * 3);

					vBarShortEntry->count = vBarShortPixelCount;
					clear->ShortVBarStorageCursor++;

					vBarUpdate = TRUE;
				}
				else
				{
					return -1025; /* invalid vBarHeader */
				}

				if (vBarUpdate)
				{
					vBarEntry = &(clear->VBarStorage[clear->VBarStorageCursor]);

					if (vBarPixelCount && (vBarEntry->size < vBarPixelCount))
					{
						if (!vBarEntry->pixels)
							vBarEntry->pixels = (UINT32*) malloc(vBarPixelCount * 4);
						else
							vBarEntry->pixels = (UINT32*) realloc(vBarEntry->pixels, vBarPixelCount * 4);

						vBarEntry->size = vBarPixelCount;
					}

					if (vBarPixelCount && !vBarEntry->pixels)
						return -1026;

					pDstPixel = vBarEntry->pixels;

					/* if (y < vBarYOn), use colorBkg */

					y = 0;
					count = vBarYOn;

					if ((y + count) > vBarPixelCount)
						count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					for ( ; y < count; y++)
					{
						*pDstPixel = color;
						pDstPixel++;
					}

					/*
					 * if ((y >= vBarYOn) && (y < (vBarYOn + vBarShortPixelCount))),
					 * use vBarShortPixels at index (y - shortVBarYOn)
					 */

					y = vBarYOn;
					count = vBarShortPixelCount;

					if ((y + count) > vBarPixelCount)
						count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					pSrcPixel = &(vBarShortEntry->pixels[y - vBarYOn]);
					CopyMemory(pDstPixel, pSrcPixel, count * 4);
					pDstPixel += count;

					/* if (y >= (vBarYOn + vBarShortPixelCount)), use colorBkg */

					y = vBarYOn + vBarShortPixelCount;
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

					for ( ; y < count; y++)
					{
						*pDstPixel = color;
						pDstPixel++;
					}

					vBarEntry->count = vBarPixelCount;
					clear->VBarStorageCursor++;
				}

				count = vBarEntry->count;
				pSrcPixel = (UINT32*) vBarEntry->pixels;
				pDstPixel = (UINT32*) &pDstData[((nYDst + yStart) * nDstStep) + ((nXDst + xStart + i) * 4)];

				for (y = 0; y < count; y++)
				{
					*pDstPixel = *pSrcPixel;
					pDstPixel += (nDstStep / 4);
					pSrcPixel++;
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
		BYTE paletteCount;
		BYTE* paletteEntries;
		BYTE stopIndex;
		BYTE suiteDepth;
		UINT32 numBits;
		BYTE* pixels;

		if ((SrcSize - offset) < subcodecByteCount)
			return -1027;

		suboffset = 0;
		subcodecs = &pSrcData[offset];

		while (suboffset < subcodecByteCount)
		{
			if ((subcodecByteCount - suboffset) < 13)
				return -1028;

			xStart = *((UINT16*) &subcodecs[suboffset]);
			yStart = *((UINT16*) &subcodecs[suboffset + 2]);
			width = *((UINT16*) &subcodecs[suboffset + 4]);
			height = *((UINT16*) &subcodecs[suboffset + 6]);
			bitmapDataByteCount = *((UINT32*) &subcodecs[suboffset + 8]);
			subcodecId = subcodecs[suboffset + 12];
			suboffset += 13;

			printf("bitmapDataByteCount: %d subcodecByteCount: %d suboffset: %d subCodecId: %d\n",
					bitmapDataByteCount, subcodecByteCount, suboffset, subcodecId);

			if ((subcodecByteCount - suboffset) < bitmapDataByteCount)
				return -1029;

			bitmapData = &subcodecs[suboffset];

			if (subcodecId == 0) /* Uncompressed */
			{
				if (bitmapDataByteCount != (width * height * 3))
					return -1030;

				pixels = bitmapData;

				for (y = 0; y < height; y++)
				{
					pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];

					for (x = 0; x < width; x++)
					{
						*pDstPixel = RGB32(pixels[2], pixels[1], pixels[0]);
						pixels += 3;
						pDstPixel++;
					}
				}
			}
			else if (subcodecId == 1) /* NSCodec */
			{
				nsc_process_message(clear->nsc, 32, width, height, bitmapData, bitmapDataByteCount);

				pixels = clear->nsc->BitmapData;

				for (y = 0; y < nHeight; y++)
				{
					pSrcPixel = (UINT32*) &pixels[y * (nWidth * 4)];
					pDstPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];
					CopyMemory(pDstPixel, pSrcPixel, nWidth * 4);
				}
			}
			else if (subcodecId == 2) /* CLEARCODEC_SUBCODEC_RLEX */
			{
				paletteCount = bitmapData[0];
				paletteEntries = &bitmapData[1];
				bitmapDataOffset = 1 + (paletteCount * 3);

				for (i = 0; i < paletteCount; i++)
				{
					b = paletteEntries[(i * 3) + 0]; /* blue */
					g = paletteEntries[(i * 3) + 1]; /* green */
					r = paletteEntries[(i * 3) + 2]; /* red */
				}

				numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

				while (bitmapDataOffset < bitmapDataByteCount)
				{
					stopIndex = bitmapData[bitmapDataOffset] & CLEAR_8BIT_MASKS[numBits];
					suiteDepth = (bitmapData[bitmapDataOffset] >> numBits) & CLEAR_8BIT_MASKS[numBits];
					bitmapDataOffset++;

					runLengthFactor1 = bitmapData[bitmapDataOffset];
					runLengthFactor = runLengthFactor1;
					bitmapDataOffset += 1;

					if (runLengthFactor1 >= 0xFF)
					{
						if ((bitmapDataByteCount - bitmapDataOffset) < 2)
							return -1031;

						runLengthFactor2 = *((UINT16*) &bitmapData[bitmapDataOffset]);
						runLengthFactor = runLengthFactor2;
						bitmapDataOffset += 2;

						if (runLengthFactor2 >= 0xFFFF)
						{
							if ((bitmapDataByteCount - bitmapDataOffset) < 4)
								return -1032;

							runLengthFactor3 = *((UINT32*) &bitmapData[bitmapDataOffset]);
							runLengthFactor = runLengthFactor3;
							bitmapDataOffset += 4;
						}
					}
				}
			}
			else
			{
				fprintf(stderr, "clear_decompress: unknown subcodecId: %d\n", subcodecId);
				return -1;
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
			return -1033;

		for (y = 0; y < nHeight; y++)
		{
			pSrcPixel = (UINT32*) &pDstData[((nYDst + y) * nDstStep) + (nXDst * 4)];
			pDstPixel = (UINT32*) &glyphData[y * (nWidth * 4)];
			CopyMemory(pDstPixel, pSrcPixel, nWidth * 4);
		}
	}

	if (offset != SrcSize)
	{
		printf("clear_decompress: incomplete processing of bytes: Actual: %d, Expected: %d\n", offset, SrcSize);
		return -1034;
	}

	return 1;
}

int clear_compress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

void clear_context_reset(CLEAR_CONTEXT* clear)
{

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

	for (i = 0; i < 4000; i++)
		free(clear->GlyphCache[i]);

	for (i = 0; i < 32768; i++)
		free(clear->VBarStorage[i].pixels);

	for (i = 0; i < 16384; i++)
		free(clear->ShortVBarStorage[i].pixels);

	free(clear);
}

