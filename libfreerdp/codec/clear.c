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

#include <freerdp/codec/clear.h>

int clear_decompress(CLEAR_CONTEXT* clear, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	int index;
	BYTE glyphFlags;
	BYTE seqNumber;
	UINT16 glyphIndex;
	UINT32 offset = 0;
	UINT32 residualByteCount;
	UINT32 bandsByteCount;
	UINT32 subcodecByteCount;

	if (SrcSize < 2)
		return -1001;

	glyphFlags = pSrcData[0];
	seqNumber = pSrcData[1];
	offset += 2;

	printf("glyphFlags: 0x%02X seqNumber: %d\n", glyphFlags, seqNumber);

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		if (SrcSize < 4)
			return -1002;

		glyphIndex = *((UINT16*) &pSrcData[2]);
		offset += 2;

		if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
		{
			/**
			 * Copy pixels from the Decompressor Glyph Storage position
			 * specified by the glyphIndex field to the output bitmap
			 */

			return 1; /* Finish */
		}
	}

	/* Read composition payload header parameters */

	if ((SrcSize - offset) < 12)
		return -1003;

	residualByteCount = *((UINT32*) &pSrcData[offset]);
	bandsByteCount = *((UINT32*) &pSrcData[offset + 4]);
	subcodecByteCount = *((UINT32*) &pSrcData[offset + 8]);
	offset += 12;

	printf("residualByteCount: %d bandsByteCount: %d subcodecByteCount: %d\n",
			residualByteCount, bandsByteCount, subcodecByteCount);

	if (residualByteCount > 0)
	{
		BYTE blueValue;
		BYTE greenValue;
		BYTE redValue;
		UINT32 suboffset;
		BYTE* residualData;
		BYTE runLengthFactor1 = 0;
		UINT16 runLengthFactor2 = 0;
		UINT32 runLengthFactor3 = 0;
		UINT32 runLengthFactor = 0;

		if ((SrcSize - offset) < residualByteCount)
			return -1004;

		suboffset = 0;
		residualData = &pSrcData[offset];

		while (suboffset < residualByteCount)
		{
			if ((residualByteCount - suboffset) < 4)
				return -1005;

			blueValue = residualData[suboffset];
			greenValue = residualData[suboffset + 1];
			redValue = residualData[suboffset + 2];
			suboffset += 3;

			runLengthFactor1 = residualData[suboffset];
			runLengthFactor = runLengthFactor1;
			suboffset += 1;

			if (runLengthFactor1 >= 0xFF)
			{
				if ((residualByteCount - suboffset) < 2)
					return -1006;

				runLengthFactor2 = *((UINT16*) &residualData[suboffset]);
				runLengthFactor = runLengthFactor2;
				suboffset += 2;

				if (runLengthFactor2 >= 0xFFFF)
				{
					if ((residualByteCount - suboffset) < 4)
						return -1007;

					runLengthFactor3 = *((UINT32*) &residualData[suboffset]);
					runLengthFactor = runLengthFactor3;
					suboffset += 4;
				}
			}
		}

		/* Decompress residual layer and write to output bitmap */
		offset += residualByteCount;
	}

	if (bandsByteCount > 0)
	{
		BYTE* bandsData;
		UINT32 suboffset;

		if ((SrcSize - offset) < bandsByteCount)
			return -1008;

		suboffset = 0;
		bandsData = &pSrcData[offset];

		while (suboffset < bandsByteCount)
		{
			UINT16 xStart;
			UINT16 xEnd;
			UINT16 yStart;
			UINT16 yEnd;
			BYTE blueBkg;
			BYTE greenBkg;
			BYTE redBkg;
			BYTE* vBars;
			UINT16 vBarHeader;
			UINT16 vBarIndex;
			UINT16 vBarYOn;
			UINT16 vBarYOff;
			int vBarCount;
			int vBarPixelCount;

			if ((bandsByteCount - suboffset) < 11)
				return -1009;

			xStart = *((UINT16*) &bandsData[suboffset]);
			xEnd = *((UINT16*) &bandsData[suboffset + 2]);
			yStart = *((UINT16*) &bandsData[suboffset + 4]);
			yEnd = *((UINT16*) &bandsData[suboffset + 6]);
			blueBkg = bandsData[suboffset + 8];
			greenBkg = bandsData[suboffset + 9];
			redBkg = bandsData[suboffset + 10];
			suboffset += 11;

			vBarCount = (xEnd - xStart) + 1;

			printf("CLEARCODEC_BAND: xStart: %d xEnd: %d yStart: %d yEnd: %d vBarCount: %d blueBkg: 0x%02X greenBkg: 0x%02X redBkg: 0x%02X\n",
					xStart, xEnd, yStart, yEnd, vBarCount, blueBkg, greenBkg, redBkg);

			for (index = 0; index < vBarCount; index++)
			{
				vBars = &bandsData[suboffset];

				if ((bandsByteCount - suboffset) < 2)
					return -1010;

				vBarHeader = *((UINT16*) &vBars[0]);
				suboffset += 2;

				if ((vBarHeader & 0xC000) == 0x8000) /* VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x7FFF);

					printf("VBAR_CACHE_HIT: vBarIndex: %d\n",
							vBarIndex);
				}
				else if ((vBarHeader & 0xC000) == 0xC000) /* SHORT_VBAR_CACHE_HIT */
				{
					vBarIndex = (vBarHeader & 0x3FFF);

					if ((bandsByteCount - suboffset) < 1)
						return -1011;

					vBarYOn = vBars[2];
					suboffset += 1;

					printf("SHORT_VBAR_CACHE_HIT: vBarIndex: %d vBarYOn: %d\n",
							vBarIndex, vBarYOn);
				}
				else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
				{
					vBarYOn = (vBarHeader & 0xFF);
					vBarYOff = ((vBarHeader >> 8) & 0x3F);

					if (vBarYOff < vBarYOn)
						return -1012;

					/* shortVBarPixels: variable */

					vBarPixelCount = (3 * (vBarYOff - vBarYOn));

					printf("SHORT_VBAR_CACHE_MISS: vBarYOn: %d vBarYOff: %d bytes: %d\n",
							vBarYOn, vBarYOff, vBarPixelCount);

					if ((bandsByteCount - suboffset) < vBarPixelCount)
						return -1013;

					suboffset += vBarPixelCount;
				}
				else
				{
					return -1014; /* invalid vBarHeader */
				}
			}
		}

		/* Decompress bands layer and write to output bitmap */
		offset += bandsByteCount;
	}

	if (subcodecByteCount > 0)
	{
		UINT16 xStart;
		UINT16 yStart;
		UINT16 width;
		UINT16 height;
		BYTE* bitmapData;
		UINT32 bitmapDataByteCount;
		BYTE subcodecId;
		BYTE* subcodecs;
		UINT32 suboffset;

		if ((SrcSize - offset) < subcodecByteCount)
			return -1015;

		suboffset = 0;
		subcodecs = &pSrcData[offset];

		while (suboffset < subcodecByteCount)
		{
			if ((subcodecByteCount - suboffset) < 13)
				return -1016;

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
				return -1017;

			bitmapData = &subcodecs[suboffset];

			suboffset += bitmapDataByteCount;
		}

		/* Decompress subcodec layer and write to output bitmap */
		offset += subcodecByteCount;
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		/**
		 * Copy decompressed bitmap to the Decompressor Glyph
		 * Storage position specified by the glyphIndex field
		 */
	}

	if (offset != SrcSize)
	{
		printf("clear_decompress: incomplete processing of bytes: Actual: %d, Expected: %d\n", offset, SrcSize);
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

		clear_context_reset(clear);
	}

	return clear;
}

void clear_context_free(CLEAR_CONTEXT* clear)
{
	if (clear)
	{
		free(clear);
	}
}

