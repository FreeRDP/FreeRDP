/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Color Conversion Routines
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/primitives.h>

#define TAG FREERDP_TAG("color")

BYTE* freerdp_glyph_convert(UINT32 width, UINT32 height, const BYTE* data)
{
	UINT32 x, y;
	const BYTE* srcp;
	BYTE* dstp;
	BYTE* dstData;
	UINT32 scanline;
	/*
	 * converts a 1-bit-per-pixel glyph to a one-byte-per-pixel glyph:
	 * this approach uses a little more memory, but provides faster
	 * means of accessing individual pixels in blitting operations
	 */
	scanline = (width + 7) / 8;
	dstData = (BYTE*) _aligned_malloc(width * height, 16);

	if (!dstData)
		return NULL;

	ZeroMemory(dstData, width * height);
	dstp = dstData;

	for (y = 0; y < height; y++)
	{
		srcp = data + (y * scanline);

		for (x = 0; x < width; x++)
		{
			if ((*srcp & (0x80 >> (x % 8))) != 0)
				*dstp = 0xFF;

			dstp++;

			if (((x + 1) % 8 == 0) && x != 0)
				srcp++;
		}
	}

	return dstData;
}

int freerdp_image_copy_from_monochrome(BYTE* pDstData, UINT32 DstFormat,
                                       UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                       UINT32 nWidth, UINT32 nHeight,
                                       const BYTE* pSrcData,
                                       UINT32 backColor, UINT32 foreColor,
                                       const UINT32* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 srcFlip;
	UINT32 dstFlip;
	UINT32 nDstPad;
	UINT32 monoStep;
	UINT32 monoBit;
	const BYTE* monoBits;
	UINT32 monoPixel;
	UINT32 dstBitsPerPixel;
	UINT32 dstBytesPerPixel;
	dstBitsPerPixel = GetBitsPerPixel(DstFormat);
	dstBytesPerPixel = GetBytesPerPixel(DstFormat);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));
	srcFlip = FREERDP_PIXEL_FLIP_NONE;
	vFlip = (srcFlip != dstFlip) ? TRUE : FALSE;
	backColor |= 0xFF000000;
	foreColor |= 0xFF000000;
	backColor = ConvertColor(backColor, PIXEL_FORMAT_ARGB32, DstFormat, palette);
	foreColor = ConvertColor(foreColor, PIXEL_FORMAT_ARGB32, DstFormat, palette);
	monoStep = (nWidth + 7) / 8;
	UINT32* pDstPixel;

	for (y = 0; y < nHeight; y++)
	{
		BYTE* pDstLine = &pDstData[((nYDst + y) * nDstStep)];
		monoBit = 0x80;

		if (!vFlip)
			monoBits = &pSrcData[monoStep * y];
		else
			monoBits = &pSrcData[monoStep * (nHeight - y - 1)];

		for (x = 0; x < nWidth; x++)
		{
			BYTE* pDstPixel = &pDstLine[((nXDst + x) * GetBytesPerPixel(DstFormat))];
			monoPixel = (*monoBits & monoBit) ? 1 : 0;

			if (!(monoBit >>= 1))
			{
				monoBits++;
				monoBit = 0x80;
			}

			if (monoPixel)
				WriteColor(pDstPixel, DstFormat, backColor);
			else
				WriteColor(pDstPixel, DstFormat, foreColor);
		}

		pDstPixel = (UINT32*) & ((BYTE*) pDstPixel)[nDstPad];
	}

	return 1;
}

static INLINE UINT32 freerdp_image_inverted_pointer_color(UINT32 x, UINT32 y)
{
#if 1
	/**
	 * Inverted pointer colors (where individual pixels can change their
	 * color to accommodate the background behind them) only seem to be
	 * supported on Windows.
	 * Using a static replacement color for these pixels (e.g. black)
	 * might result in invisible pointers depending on the background.
	 * This function returns either black or white, depending on the
	 * pixel's position.
	 */
	return (x + y) & 1 ? 0xFF000000 : 0xFFFFFFFF;
#else
	return 0xFF000000;
#endif
}

/**
 * Drawing Monochrome Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556143/
 *
 * Drawing Color Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556138/
 */

BOOL freerdp_image_copy_from_pointer_data(
    BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
    const BYTE* xorMask, UINT32 xorMaskLength,
    const BYTE* andMask, UINT32 andMaskLength,
    UINT32 xorBpp, const UINT32* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 srcFlip;
	UINT32 dstFlip;
	UINT32 nDstPad;
	UINT32 xorStep;
	UINT32 andStep;
	UINT32 xorBit;
	UINT32 andBit;
	const BYTE* xorBits;
	const BYTE* andBits;
	UINT32 xorPixel;
	UINT32 andPixel;
	UINT32 dstBitsPerPixel;
	UINT32 dstBytesPerPixel;
	dstBitsPerPixel = GetBitsPerPixel(DstFormat);
	dstBytesPerPixel = GetBytesPerPixel(DstFormat);
	dstFlip = FREERDP_PIXEL_FORMAT_FLIP(DstFormat);

	if (FREERDP_PIXEL_FORMAT_FLIP(DstFormat))
	{
		WLog_ERR(TAG, "Format %s not supported!", GetColorFormatName(DstFormat));
	}

	if (nDstStep < 0)
		nDstStep = dstBytesPerPixel * nWidth;

	nDstPad = (nDstStep - (nWidth * dstBytesPerPixel));
	srcFlip = (xorBpp == 1) ? FREERDP_PIXEL_FLIP_NONE : FREERDP_PIXEL_FLIP_VERTICAL;
	vFlip = (srcFlip != dstFlip) ? TRUE : FALSE;
	andStep = (nWidth + 7) / 8;
	andStep += (andStep % 2);

	if (!xorMask || (xorMaskLength == 0))
		return -1;

	if (xorBpp == 1)
	{
		if (!andMask || (andMaskLength == 0))
			return -1;

		xorStep = (nWidth + 7) / 8;
		xorStep += (xorStep % 2);

		if (xorStep * nHeight > xorMaskLength)
			return -1;

		if (andStep * nHeight > andMaskLength)
			return -1;

		for (y = 0; y < nHeight; y++)
		{
			BYTE* pDstPixel = &pDstData[((nYDst + y) * nDstStep) +
			                            (nXDst * GetBytesPerPixel(DstFormat))];
			xorBit = andBit = 0x80;

			if (!vFlip)
			{
				xorBits = &xorMask[xorStep * y];
				andBits = &andMask[andStep * y];
			}
			else
			{
				xorBits = &xorMask[xorStep * (nHeight - y - 1)];
				andBits = &andMask[andStep * (nHeight - y - 1)];
			}

			for (x = 0; x < nWidth; x++)
			{
				UINT32 color;
				xorPixel = (*xorBits & xorBit) ? 1 : 0;

				if (!(xorBit >>= 1))
				{
					xorBits++;
					xorBit = 0x80;
				}

				andPixel = (*andBits & andBit) ? 1 : 0;

				if (!(andBit >>= 1))
				{
					andBits++;
					andBit = 0x80;
				}

				if (!andPixel && !xorPixel)
					color = 0xFF000000; /* black */
				else if (!andPixel && xorPixel)
					color = 0xFFFFFFFF; /* white */
				else if (andPixel && !xorPixel)
					color = 0x00000000; /* transparent */
				else if (andPixel && xorPixel)
					color = freerdp_image_inverted_pointer_color(x, y); /* inverted */

				color = ConvertColor(color, PIXEL_FORMAT_ABGR32,
				                     DstFormat, NULL);
				WriteColor(pDstPixel, DstFormat, color);
				pDstPixel += GetBytesPerPixel(DstFormat);
			}
		}

		return 1;
	}
	else if (xorBpp == 24 || xorBpp == 32 || xorBpp == 16 || xorBpp == 8)
	{
		UINT32 xorBytesPerPixel = xorBpp >> 3;
		xorStep = nWidth * xorBytesPerPixel;

		if (xorBpp == 8 && !palette)
		{
			WLog_ERR(TAG, "null palette in convertion from %d bpp to %d bpp",
			         xorBpp, dstBitsPerPixel);
			return -1;
		}

		if (xorStep * nHeight > xorMaskLength)
			return -1;

		if (andMask)
		{
			if (andStep * nHeight > andMaskLength)
				return -1;
		}

		for (y = 0; y < nHeight; y++)
		{
			BYTE* pDstPixel = &pDstData[((nYDst + y) * nDstStep) +
			                            (nXDst * GetBytesPerPixel(DstFormat))];
			andBit = 0x80;

			if (!vFlip)
			{
				if (andMask)
					andBits = &andMask[andStep * y];

				xorBits = &xorMask[xorStep * y];
			}
			else
			{
				if (andMask)
					andBits = &andMask[andStep * (nHeight - y - 1)];

				xorBits = &xorMask[xorStep * (nHeight - y - 1)];
			}

			for (x = 0; x < nWidth; x++)
			{
				UINT32 color;
				BOOL ignoreAndMask = FALSE;

				if (xorBpp == 32)
				{
					xorPixel = *((UINT32*) xorBits);

					if (xorPixel & 0xFF000000)
						ignoreAndMask = TRUE;
				}
				else if (xorBpp == 16)
				{
					xorPixel = ConvertColor(*(UINT16*)xorBits,
					                        PIXEL_FORMAT_RGB16,
					                        PIXEL_FORMAT_ABGR32,
					                        NULL);
				}
				else if (xorBpp == 8)
				{
					xorPixel = 0xFF << 24 | ((UINT32*)palette)[xorBits[0]];
				}
				else
				{
					xorPixel = xorBits[0] | xorBits[1] << 8 | xorBits[2] << 16 | 0xFF << 24;
				}

				xorBits += xorBytesPerPixel;
				andPixel = 0;

				if (andMask)
				{
					andPixel = (*andBits & andBit) ? 1 : 0;

					if (!(andBit >>= 1))
					{
						andBits++;
						andBit = 0x80;
					}
				}

				/* Ignore the AND mask, if the color format already supplies alpha data. */
				if (andPixel && !ignoreAndMask)
				{
					const UINT32 xorPixelMasked = xorPixel | 0xFF000000;

					if (xorPixelMasked == 0xFF000000) /* black -> transparent */
						color = 0x00000000;
					else if (xorPixelMasked == 0xFFFFFFFF) /* white -> inverted */
						color = freerdp_image_inverted_pointer_color(x, y);
					else
						color = xorPixel;

					color = ConvertColor(color, PIXEL_FORMAT_ABGR32, DstFormat, NULL);
				}
				else
				{
					color = ConvertColor(xorPixel, PIXEL_FORMAT_ABGR32,
					                     DstFormat, NULL);
				}

				WriteColor(pDstPixel, DstFormat, color);
				pDstPixel += GetBytesPerPixel(DstFormat);
			}
		}

		return 1;
	}

	WLog_ERR(TAG, "failed to convert from %d bpp to %d bpp",
	         xorBpp, dstBitsPerPixel);
	return -1;
}


BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat,
                        INT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                        UINT32 nWidth, UINT32 nHeight,
                        const BYTE* pSrcData, DWORD SrcFormat,
                        INT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                        const UINT32* palette)
{
	const UINT32 dstByte = GetBytesPerPixel(DstFormat);
	const UINT32 srcByte = GetBytesPerPixel(SrcFormat);
	const UINT32 copyDstWidth = nWidth * dstByte;
	const UINT32 xSrcOffset = nXSrc * srcByte;
	const UINT32 xDstOffset = nXDst * dstByte;
	BOOL vSrcVFlip = FALSE;
	BOOL vDstVFlip = FALSE;
	BOOL vSrcHFlip = FALSE;
	BOOL vDstHFlip = FALSE;
	UINT32 srcVOffset = 0;
	INT32 srcVMultiplier = 1;
	UINT32 dstVOffset = 0;
	INT32 dstVMultiplier = 1;

	if (!pDstData || !pSrcData)
		return FALSE;

	switch (FREERDP_PIXEL_FORMAT_FLIP(SrcFormat))
	{
		case FREERDP_PIXEL_FLIP_HORIZONTAL:
			vSrcHFlip = TRUE;
			return FALSE;

		case FREERDP_PIXEL_FLIP_VERTICAL:
			vSrcVFlip = TRUE;
			break;

		case FREERDP_PIXEL_FLIP_NONE:
		default:
			break;
	}

	switch (FREERDP_PIXEL_FORMAT_FLIP(DstFormat))
	{
		case FREERDP_PIXEL_FLIP_HORIZONTAL:
			vDstHFlip = TRUE;
			return FALSE;

		case FREERDP_PIXEL_FLIP_VERTICAL:
			vDstVFlip = TRUE;
			break;

		case FREERDP_PIXEL_FLIP_NONE:
		default:
			break;
	}

	if (nDstStep < 0)
		nDstStep = nWidth * GetBytesPerPixel(DstFormat);

	if (nSrcStep < 0)
		nSrcStep = nWidth * GetBytesPerPixel(SrcFormat);

	if (vSrcVFlip)
	{
		srcVOffset = (nHeight - 1) * nSrcStep;
		srcVMultiplier = -1;
	}

	if (vDstVFlip)
	{
		dstVOffset = (nHeight - 1) * nDstStep;
		dstVMultiplier = -1;
	}

	if (vSrcHFlip || vDstHFlip)
	{
		WLog_ERR(TAG, "Horizontal flipping not supported! %s %s",
		         GetColorFormatName(SrcFormat), GetColorFormatName(DstFormat));
		return FALSE;
	}

	if (FREERDP_PIXEL_FORMAT_FLIP_MASKED(SrcFormat) ==
	    FREERDP_PIXEL_FORMAT_FLIP_MASKED(DstFormat))
	{
		UINT32 y;

		for (y = 0; y < nHeight; y++)
		{
			const BYTE* srcLine = &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier +
			                                srcVOffset];
			BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
			memcpy(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
		}
	}
	else
	{
		UINT32 x, y;

		for (y = 0; y < nHeight; y++)
		{
			const BYTE* srcLine = &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier +
			                                srcVOffset];
			BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];

			for (x = 0; x < nWidth; x++)
			{
				UINT32 dstColor;
				UINT32 color = ReadColor(&srcLine[(x + nXSrc) * srcByte],
				                         SrcFormat);
				dstColor = ConvertColor(color, SrcFormat, DstFormat, palette);
				WriteColor(&dstLine[(x + nXDst) * dstByte], DstFormat, dstColor);
			}
		}
	}

	return TRUE;
}

BOOL freerdp_image_fill(BYTE* pDstData, DWORD DstFormat,
                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                        UINT32 nWidth, UINT32 nHeight, UINT32 color)
{
	UINT32 x, y;

	for (y = 0; y < nHeight; y++)
	{
		BYTE* pDstLine = &pDstData[(y + nYDst) * nDstStep];

		for (x = 0; x < nWidth; x++)
		{
			BYTE* pDst = &pDstLine[(x + nXDst) * GetBytesPerPixel(DstFormat)];
			WriteColor(pDst, DstFormat, color);
		}
	}

	return TRUE;
}
