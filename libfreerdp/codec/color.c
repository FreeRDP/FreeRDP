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

BOOL freerdp_image_copy_from_monochrome(BYTE* pDstData, UINT32 DstFormat,
                                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                        UINT32 nWidth, UINT32 nHeight,
                                        const BYTE* pSrcData,
                                        UINT32 backColor, UINT32 foreColor,
                                        const gdiPalette* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 monoStep;
	const UINT32 dstBytesPerPixel = GetBytesPerPixel(DstFormat);

	if (nDstStep == 0)
		nDstStep = dstBytesPerPixel * nWidth;

	vFlip = FALSE;
	monoStep = (nWidth + 7) / 8;

	for (y = 0; y < nHeight; y++)
	{
		const BYTE* monoBits;
		BYTE* pDstLine = &pDstData[((nYDst + y) * nDstStep)];
		UINT32 monoBit = 0x80;

		if (!vFlip)
			monoBits = &pSrcData[monoStep * y];
		else
			monoBits = &pSrcData[monoStep * (nHeight - y - 1)];

		for (x = 0; x < nWidth; x++)
		{
			BYTE* pDstPixel = &pDstLine[((nXDst + x) * GetBytesPerPixel(DstFormat))];
			BOOL monoPixel = (*monoBits & monoBit) ? TRUE : FALSE;

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
	}

	return TRUE;
}

static INLINE UINT32 freerdp_image_inverted_pointer_color(UINT32 x, UINT32 y,
        UINT32 format)
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
	BYTE fill = (x + y) & 1 ? 0x00 : 0xFF;
#else
	BYTE fill = 0x00;
#endif
	return FreeRDPGetColor(format, fill, fill, fill, 0xFF);
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
    UINT32 xorBpp, const gdiPalette* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 xorStep;
	UINT32 andStep;
	UINT32 xorBit;
	UINT32 andBit;
	UINT32 xorPixel;
	UINT32 andPixel;
	UINT32 dstBitsPerPixel;
	UINT32 dstBytesPerPixel;
	dstBitsPerPixel = GetBitsPerPixel(DstFormat);
	dstBytesPerPixel = GetBytesPerPixel(DstFormat);

	if (nDstStep <= 0)
		nDstStep = dstBytesPerPixel * nWidth;

	vFlip = (xorBpp == 1) ? FALSE : TRUE;
	andStep = (nWidth + 7) / 8;
	andStep += (andStep % 2);

	if (!xorMask || (xorMaskLength == 0))
		return FALSE;

	switch (xorBpp)
	{
		case 1:
			if (!andMask || (andMaskLength == 0))
				return FALSE;

			xorStep = (nWidth + 7) / 8;
			xorStep += (xorStep % 2);

			if (xorStep * nHeight > xorMaskLength)
				return FALSE;

			if (andStep * nHeight > andMaskLength)
				return FALSE;

			for (y = 0; y < nHeight; y++)
			{
				const BYTE* andBits;
				const BYTE* xorBits;
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
					UINT32 color = 0;
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
						color = FreeRDPGetColor(DstFormat, 0, 0, 0, 0xFF); /* black */
					else if (!andPixel && xorPixel)
						color = FreeRDPGetColor(DstFormat, 0xFF, 0xFF, 0xFF, 0xFF); /* white */
					else if (andPixel && !xorPixel)
						color = FreeRDPGetColor(DstFormat, 0, 0, 0, 0); /* transparent */
					else if (andPixel && xorPixel)
						color = freerdp_image_inverted_pointer_color(x, y, DstFormat); /* inverted */

					WriteColor(pDstPixel, DstFormat, color);
					pDstPixel += GetBytesPerPixel(DstFormat);
				}
			}

			return TRUE;

		case 8:
		case 16:
		case 24:
		case 32:
			{
				UINT32 xorBytesPerPixel = xorBpp >> 3;
				xorStep = nWidth * xorBytesPerPixel;

				if (xorBpp == 8 && !palette)
				{
					WLog_ERR(TAG, "null palette in conversion from %"PRIu32" bpp to %"PRIu32" bpp",
					         xorBpp, dstBitsPerPixel);
					return FALSE;
				}

				if (xorStep * nHeight > xorMaskLength)
					return FALSE;

				if (andMask)
				{
					if (andStep * nHeight > andMaskLength)
						return FALSE;
				}

				for (y = 0; y < nHeight; y++)
				{
					const BYTE* xorBits;
					const BYTE* andBits = NULL;
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
						UINT32 pixelFormat;
						UINT32 color;

						if (xorBpp == 32)
						{
							pixelFormat = PIXEL_FORMAT_BGRA32;
							xorPixel = ReadColor(xorBits, pixelFormat);
						}
						else if (xorBpp == 16)
						{
							pixelFormat = PIXEL_FORMAT_RGB15;
							xorPixel = ReadColor(xorBits, pixelFormat);
						}
						else if (xorBpp == 8)
						{
							pixelFormat = palette->format;
							xorPixel = palette->palette[xorBits[0]];
						}
						else
						{
							pixelFormat = PIXEL_FORMAT_BGR24;
							xorPixel = ReadColor(xorBits, pixelFormat);
						}

						xorPixel = FreeRDPConvertColor(xorPixel,
						                        pixelFormat,
						                        PIXEL_FORMAT_ARGB32,
						                        palette);
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

						if (andPixel)
						{
							if (xorPixel == 0xFF000000) /* black -> transparent */
								xorPixel = 0x00000000;
							else if (xorPixel == 0xFFFFFFFF) /* white -> inverted */
								xorPixel = freerdp_image_inverted_pointer_color(x, y, PIXEL_FORMAT_ARGB32);
						}

						color = FreeRDPConvertColor(xorPixel, PIXEL_FORMAT_ARGB32,
						                     DstFormat, palette);
						WriteColor(pDstPixel, DstFormat, color);
						pDstPixel += GetBytesPerPixel(DstFormat);
					}
				}

				return TRUE;
			}

		default:
			WLog_ERR(TAG, "failed to convert from %"PRIu32" bpp to %"PRIu32" bpp",
			         xorBpp, dstBitsPerPixel);
			return FALSE;
	}
}

static INLINE BOOL overlapping(const BYTE* pDstData, UINT32 nXDst, UINT32 nYDst,
                               UINT32 nDstStep, UINT32 dstBytesPerPixel,
                               const BYTE* pSrcData, UINT32 nXSrc, UINT32 nYSrc,
                               UINT32 nSrcStep, UINT32 srcBytesPerPixel,
                               UINT32 nWidth, UINT32 nHeight)
{
	const BYTE* pDstStart = &pDstData[nXDst * dstBytesPerPixel + nYDst * nDstStep];
	const BYTE* pDstEnd = pDstStart + nHeight * nDstStep;
	const BYTE* pSrcStart = &pSrcData[nXSrc * srcBytesPerPixel + nYSrc * nSrcStep];
	const BYTE* pSrcEnd = pSrcStart + nHeight * nSrcStep;

	if ((pDstStart >= pSrcStart) && (pDstStart <= pSrcEnd))
		return TRUE;

	if ((pDstEnd >= pSrcStart) && (pDstEnd <= pSrcEnd))
		return TRUE;

	return FALSE;
}

BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat,
                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                        UINT32 nWidth, UINT32 nHeight,
                        const BYTE* pSrcData, DWORD SrcFormat,
                        UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                        const gdiPalette* palette, UINT32 flags)
{
	const UINT32 dstByte = GetBytesPerPixel(DstFormat);
	const UINT32 srcByte = GetBytesPerPixel(SrcFormat);
	const UINT32 copyDstWidth = nWidth * dstByte;
	const UINT32 xSrcOffset = nXSrc * srcByte;
	const UINT32 xDstOffset = nXDst * dstByte;
	const BOOL vSrcVFlip = flags & FREERDP_FLIP_VERTICAL;
	UINT32 srcVOffset = 0;
	INT32 srcVMultiplier = 1;
	UINT32 dstVOffset = 0;
	INT32 dstVMultiplier = 1;

	if (!pDstData || !pSrcData)
		return FALSE;

	if (nDstStep == 0)
		nDstStep = nWidth * GetBytesPerPixel(DstFormat);

	if (nSrcStep == 0)
		nSrcStep = nWidth * GetBytesPerPixel(SrcFormat);

	if (vSrcVFlip)
	{
		srcVOffset = (nHeight - 1) * nSrcStep;
		srcVMultiplier = -1;
	}

	if (AreColorFormatsEqualNoAlpha(SrcFormat, DstFormat))
	{
		INT32 y;

		if (overlapping(pDstData, nXDst, nYDst, nDstStep, dstByte,
		                pSrcData, nXSrc, nYSrc, nSrcStep, srcByte,
		                nWidth, nHeight))
		{
			/* Copy down */
			if (nYDst < nYSrc)
			{
				for (y = 0; y < nHeight; y++)
				{
					const BYTE* srcLine = &pSrcData[(y + nYSrc) *
					                                nSrcStep * srcVMultiplier +
					                                srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) *
					                          nDstStep * dstVMultiplier +
					                          dstVOffset];
					memcpy(&dstLine[xDstOffset],
					       &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy up */
			else if (nYDst > nYSrc)
			{
				for (y = nHeight - 1; y >= 0; y--)
				{
					const BYTE* srcLine = &pSrcData[(y + nYSrc) *
					                                nSrcStep * srcVMultiplier +
					                                srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) *
					                          nDstStep * dstVMultiplier +
					                          dstVOffset];
					memcpy(&dstLine[xDstOffset],
					       &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy left */
			else if (nXSrc > nXDst)
			{
				for (y = 0; y < nHeight; y++)
				{
					const BYTE* srcLine = &pSrcData[(y + nYSrc) *
					                                nSrcStep * srcVMultiplier +
					                                srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) *
					                          nDstStep * dstVMultiplier +
					                          dstVOffset];
					memmove(&dstLine[xDstOffset],
					        &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy right */
			else if (nXSrc < nXDst)
			{
				for (y = nHeight - 1; y >= 0; y--)
				{
					const BYTE* srcLine = &pSrcData[(y + nYSrc) *
					                                nSrcStep * srcVMultiplier +
					                                srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) *
					                          nDstStep * dstVMultiplier +
					                          dstVOffset];
					memmove(&dstLine[xDstOffset],
					        &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Source and destination are equal... */
			else
			{
			}
		}
		else
		{
			for (y = 0; y < nHeight; y++)
			{
				const BYTE* srcLine = &pSrcData[(y + nYSrc) *
				                                nSrcStep * srcVMultiplier +
				                                srcVOffset];
				BYTE* dstLine = &pDstData[(y + nYDst) *
				                          nDstStep * dstVMultiplier +
				                          dstVOffset];
				memcpy(&dstLine[xDstOffset],
				       &srcLine[xSrcOffset], copyDstWidth);
			}
		}
	}
	else
	{
		UINT32 x, y;

		for (y = 0; y < nHeight; y++)
		{
			const BYTE* srcLine = &pSrcData[(y + nYSrc) *
			                                nSrcStep * srcVMultiplier +
			                                srcVOffset];
			BYTE* dstLine = &pDstData[(y + nYDst) *
			                          nDstStep * dstVMultiplier + dstVOffset];

			for (x = 0; x < nWidth; x++)
			{
				UINT32 dstColor;
				UINT32 color = ReadColor(&srcLine[(x + nXSrc) * srcByte],
				                         SrcFormat);
				dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
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
	const UINT32 bpp = GetBytesPerPixel(DstFormat);
	BYTE* pFirstDstLine = &pDstData[nYDst * nDstStep];
	BYTE* pFirstDstLineXOffset = &pFirstDstLine[nXDst * bpp];

	for (x = 0; x < nWidth; x++)
	{
		BYTE* pDst = &pFirstDstLine[(x + nXDst) * bpp];
		WriteColor(pDst, DstFormat, color);
	}

	for (y = 1; y < nHeight; y++)
	{
		BYTE* pDstLine = &pDstData[(y + nYDst) * nDstStep + nXDst * bpp];
		memcpy(pDstLine, pFirstDstLineXOffset, nWidth * bpp);
	}

	return TRUE;
}
