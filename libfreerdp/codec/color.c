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

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/primitives.h>

#if defined(WITH_CAIRO)
#include <cairo.h>
#endif

#if defined(WITH_SWSCALE)
#include <libswscale/swscale.h>
#endif

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
	dstData = (BYTE*)winpr_aligned_malloc(width * height * 1ULL, 16);

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

BOOL freerdp_image_copy_from_monochrome(BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                        UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
                                        const BYTE* pSrcData, UINT32 backColor, UINT32 foreColor,
                                        const gdiPalette* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 monoStep;
	const UINT32 dstBytesPerPixel = FreeRDPGetBytesPerPixel(DstFormat);

	if (!pDstData || !pSrcData || !palette)
		return FALSE;

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
			BYTE* pDstPixel = &pDstLine[((nXDst + x) * FreeRDPGetBytesPerPixel(DstFormat))];
			BOOL monoPixel = (*monoBits & monoBit) ? TRUE : FALSE;

			if (!(monoBit >>= 1))
			{
				monoBits++;
				monoBit = 0x80;
			}

			if (monoPixel)
				FreeRDPWriteColor(pDstPixel, DstFormat, backColor);
			else
				FreeRDPWriteColor(pDstPixel, DstFormat, foreColor);
		}
	}

	return TRUE;
}

static INLINE UINT32 freerdp_image_inverted_pointer_color(UINT32 x, UINT32 y, UINT32 format)
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

/*
 * DIB color palettes are arrays of RGBQUAD structs with colors in BGRX format.
 * They are used only by 1, 2, 4, and 8-bit bitmaps.
 */
static void fill_gdi_palette_for_icon(const BYTE* colorTable, UINT16 cbColorTable,
                                      gdiPalette* palette)
{
	UINT16 i;

	WINPR_ASSERT(palette);

	palette->format = PIXEL_FORMAT_BGRX32;
	ZeroMemory(palette->palette, sizeof(palette->palette));

	if (!cbColorTable)
		return;

	if ((cbColorTable % 4 != 0) || (cbColorTable / 4 > 256))
	{
		WLog_WARN(TAG, "weird palette size: %u", cbColorTable);
		return;
	}

	for (i = 0; i < cbColorTable / 4; i++)
	{
		palette->palette[i] = FreeRDPReadColor(&colorTable[4 * i], palette->format);
	}
}

static INLINE UINT32 div_ceil(UINT32 a, UINT32 b)
{
	return (a + (b - 1)) / b;
}

static INLINE UINT32 round_up(UINT32 a, UINT32 b)
{
	return b * div_ceil(a, b);
}

BOOL freerdp_image_copy_from_icon_data(BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                       UINT32 nXDst, UINT32 nYDst, UINT16 nWidth, UINT16 nHeight,
                                       const BYTE* bitsColor, UINT16 cbBitsColor,
                                       const BYTE* bitsMask, UINT16 cbBitsMask,
                                       const BYTE* colorTable, UINT16 cbColorTable, UINT32 bpp)
{
	DWORD format;
	gdiPalette palette;

	if (!pDstData || !bitsColor)
		return FALSE;

	/*
	 * Color formats used by icons are DIB bitmap formats (2-bit format
	 * is not used by MS-RDPERP). Note that 16-bit is RGB555, not RGB565,
	 * and that 32-bit format uses BGRA order.
	 */
	switch (bpp)
	{
		case 1:
		case 4:
			/*
			 * These formats are not supported by freerdp_image_copy().
			 * PIXEL_FORMAT_MONO and PIXEL_FORMAT_A4 are *not* correct
			 * color formats for this. Please fix freerdp_image_copy()
			 * if you came here to fix a broken icon of some weird app
			 * that still uses 1 or 4bpp format in the 21st century.
			 */
			WLog_WARN(TAG, "1bpp and 4bpp icons are not supported");
			return FALSE;

		case 8:
			format = PIXEL_FORMAT_RGB8;
			break;

		case 16:
			format = PIXEL_FORMAT_RGB15;
			break;

		case 24:
			format = PIXEL_FORMAT_RGB24;
			break;

		case 32:
			format = PIXEL_FORMAT_BGRA32;
			break;

		default:
			WLog_WARN(TAG, "invalid icon bpp: %" PRIu32, bpp);
			return FALSE;
	}

	/* Ensure we have enough source data bytes for image copy. */
	if (cbBitsColor < nWidth * nHeight * FreeRDPGetBytesPerPixel(format))
		return FALSE;

	fill_gdi_palette_for_icon(colorTable, cbColorTable, &palette);
	if (!freerdp_image_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, bitsColor,
	                        format, 0, 0, 0, &palette, FREERDP_FLIP_VERTICAL))
		return FALSE;

	/* apply alpha mask */
	if (FreeRDPColorHasAlpha(DstFormat) && cbBitsMask)
	{
		BYTE nextBit;
		const BYTE* maskByte;
		UINT32 x, y;
		UINT32 stride;
		BYTE r, g, b;
		BYTE* dstBuf = pDstData;
		UINT32 dstBpp = FreeRDPGetBytesPerPixel(DstFormat);

		/*
		 * Each byte encodes 8 adjacent pixels (with LSB padding as needed).
		 * And due to hysterical raisins, stride of DIB bitmaps must be
		 * a multiple of 4 bytes.
		 */
		stride = round_up(div_ceil(nWidth, 8), 4);

		for (y = 0; y < nHeight; y++)
		{
			maskByte = &bitsMask[stride * (nHeight - 1 - y)];
			nextBit = 0x80;

			for (x = 0; x < nWidth; x++)
			{
				UINT32 color;
				BYTE alpha = (*maskByte & nextBit) ? 0x00 : 0xFF;

				/* read color back, add alpha and write it back */
				color = FreeRDPReadColor(dstBuf, DstFormat);
				FreeRDPSplitColor(color, DstFormat, &r, &g, &b, NULL, &palette);
				color = FreeRDPGetColor(DstFormat, r, g, b, alpha);
				FreeRDPWriteColor(dstBuf, DstFormat, color);

				nextBit >>= 1;
				dstBuf += dstBpp;
				if (!nextBit)
				{
					nextBit = 0x80;
					maskByte++;
				}
			}
		}
	}

	return TRUE;
}

static BOOL freerdp_image_copy_from_pointer_data_1bpp(BYTE* pDstData, UINT32 DstFormat,
                                                      UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                                      UINT32 nWidth, UINT32 nHeight,
                                                      const BYTE* xorMask, UINT32 xorMaskLength,
                                                      const BYTE* andMask, UINT32 andMaskLength,
                                                      UINT32 xorBpp)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 xorStep;
	UINT32 andStep;
	UINT32 xorBit;
	UINT32 andBit;
	UINT32 xorPixel;
	UINT32 andPixel;

	vFlip = (xorBpp == 1) ? FALSE : TRUE;
	andStep = (nWidth + 7) / 8;
	andStep += (andStep % 2);

	if (!xorMask || (xorMaskLength == 0))
		return FALSE;
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
		BYTE* pDstPixel =
		    &pDstData[((nYDst + y) * nDstStep) + (nXDst * FreeRDPGetBytesPerPixel(DstFormat))];
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

			FreeRDPWriteColor(pDstPixel, DstFormat, color);
			pDstPixel += FreeRDPGetBytesPerPixel(DstFormat);
		}
	}

	return TRUE;
}

static BOOL freerdp_image_copy_from_pointer_data_xbpp(BYTE* pDstData, UINT32 DstFormat,
                                                      UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                                      UINT32 nWidth, UINT32 nHeight,
                                                      const BYTE* xorMask, UINT32 xorMaskLength,
                                                      const BYTE* andMask, UINT32 andMaskLength,
                                                      UINT32 xorBpp, const gdiPalette* palette)
{
	UINT32 x, y;
	BOOL vFlip;
	UINT32 xorStep;
	UINT32 andStep;
	UINT32 andBit;
	UINT32 xorPixel;
	UINT32 andPixel;
	UINT32 dstBitsPerPixel;
	UINT32 xorBytesPerPixel;
	dstBitsPerPixel = FreeRDPGetBitsPerPixel(DstFormat);

	vFlip = (xorBpp == 1) ? FALSE : TRUE;
	andStep = (nWidth + 7) / 8;
	andStep += (andStep % 2);

	if (!xorMask || (xorMaskLength == 0))
		return FALSE;

	xorBytesPerPixel = xorBpp >> 3;
	xorStep = nWidth * xorBytesPerPixel;
	xorStep += (xorStep % 2);

	if (xorBpp == 8 && !palette)
	{
		WLog_ERR(TAG, "null palette in conversion from %" PRIu32 " bpp to %" PRIu32 " bpp", xorBpp,
		         dstBitsPerPixel);
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
		BYTE* pDstPixel =
		    &pDstData[((nYDst + y) * nDstStep) + (nXDst * FreeRDPGetBytesPerPixel(DstFormat))];
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
				xorPixel = FreeRDPReadColor(xorBits, pixelFormat);
			}
			else if (xorBpp == 16)
			{
				pixelFormat = PIXEL_FORMAT_RGB15;
				xorPixel = FreeRDPReadColor(xorBits, pixelFormat);
			}
			else if (xorBpp == 8)
			{
				pixelFormat = palette->format;
				xorPixel = palette->palette[xorBits[0]];
			}
			else
			{
				pixelFormat = PIXEL_FORMAT_BGR24;
				xorPixel = FreeRDPReadColor(xorBits, pixelFormat);
			}

			xorPixel = FreeRDPConvertColor(xorPixel, pixelFormat, PIXEL_FORMAT_ARGB32, palette);
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

			color = FreeRDPConvertColor(xorPixel, PIXEL_FORMAT_ARGB32, DstFormat, palette);
			FreeRDPWriteColor(pDstPixel, DstFormat, color);
			pDstPixel += FreeRDPGetBytesPerPixel(DstFormat);
		}
	}

	return TRUE;
}

/**
 * Drawing Monochrome Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556143/
 *
 * Drawing Color Pointers:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff556138/
 */

BOOL freerdp_image_copy_from_pointer_data(BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
                                          UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
                                          const BYTE* xorMask, UINT32 xorMaskLength,
                                          const BYTE* andMask, UINT32 andMaskLength, UINT32 xorBpp,
                                          const gdiPalette* palette)
{
	UINT32 y;
	UINT32 dstBitsPerPixel;
	UINT32 dstBytesPerPixel;
	dstBitsPerPixel = FreeRDPGetBitsPerPixel(DstFormat);
	dstBytesPerPixel = FreeRDPGetBytesPerPixel(DstFormat);

	if (nDstStep <= 0)
		nDstStep = dstBytesPerPixel * nWidth;

	for (y = nYDst; y < nHeight; y++)
	{
		BYTE* pDstLine = &pDstData[y * nDstStep + nXDst * dstBytesPerPixel];
		memset(pDstLine, 0, dstBytesPerPixel * (nWidth - nXDst) * 1ULL);
	}

	switch (xorBpp)
	{
		case 1:
			return freerdp_image_copy_from_pointer_data_1bpp(
			    pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, xorMask,
			    xorMaskLength, andMask, andMaskLength, xorBpp);

		case 8:
		case 16:
		case 24:
		case 32:
			return freerdp_image_copy_from_pointer_data_xbpp(
			    pDstData, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight, xorMask,
			    xorMaskLength, andMask, andMaskLength, xorBpp, palette);

		default:
			WLog_ERR(TAG, "failed to convert from %" PRIu32 " bpp to %" PRIu32 " bpp", xorBpp,
			         dstBitsPerPixel);
			return FALSE;
	}
}

static INLINE BOOL overlapping(const BYTE* pDstData, UINT32 nXDst, UINT32 nYDst, UINT32 nDstStep,
                               UINT32 dstBytesPerPixel, const BYTE* pSrcData, UINT32 nXSrc,
                               UINT32 nYSrc, UINT32 nSrcStep, UINT32 srcBytesPerPixel,
                               UINT32 nWidth, UINT32 nHeight)
{
	const BYTE* pDstStart = &pDstData[nXDst * dstBytesPerPixel + nYDst * nDstStep];
	const BYTE* pDstEnd = pDstStart + nHeight * nDstStep;
	const BYTE* pSrcStart = &pSrcData[nXSrc * srcBytesPerPixel + nYSrc * nSrcStep];
	const BYTE* pSrcEnd = pSrcStart + nHeight * nSrcStep;

	WINPR_UNUSED(nWidth);

	if ((pDstStart >= pSrcStart) && (pDstStart <= pSrcEnd))
		return TRUE;

	if ((pDstEnd >= pSrcStart) && (pDstEnd <= pSrcEnd))
		return TRUE;

	return FALSE;
}

BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst,
                        UINT32 nYDst, UINT32 nWidth, UINT32 nHeight, const BYTE* pSrcData,
                        DWORD SrcFormat, UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                        const gdiPalette* palette, UINT32 flags)
{
	const UINT32 dstByte = FreeRDPGetBytesPerPixel(DstFormat);
	const UINT32 srcByte = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT32 copyDstWidth = nWidth * dstByte;
	const UINT32 xSrcOffset = nXSrc * srcByte;
	const UINT32 xDstOffset = nXDst * dstByte;
	const BOOL vSrcVFlip = flags & FREERDP_FLIP_VERTICAL;
	UINT32 srcVOffset = 0;
	INT32 srcVMultiplier = 1;
	UINT32 dstVOffset = 0;
	INT32 dstVMultiplier = 1;

	if ((nHeight > INT32_MAX) || (nWidth > INT32_MAX))
		return FALSE;

	if (!pDstData || !pSrcData)
		return FALSE;

	if (nDstStep == 0)
		nDstStep = nWidth * FreeRDPGetBytesPerPixel(DstFormat);

	if (nSrcStep == 0)
		nSrcStep = nWidth * FreeRDPGetBytesPerPixel(SrcFormat);

	if (vSrcVFlip)
	{
		srcVOffset = (nHeight - 1) * nSrcStep;
		srcVMultiplier = -1;
	}

	if (((flags & FREERDP_KEEP_DST_ALPHA) != 0) && FreeRDPColorHasAlpha(DstFormat))
	{
		UINT32 x, y;

		for (y = 0; y < nHeight; y++)
		{
			const BYTE* srcLine = &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
			BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];

			UINT32 color = FreeRDPReadColor(&srcLine[nXSrc * srcByte], SrcFormat);
			UINT32 oldColor = color;
			UINT32 dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
			FreeRDPWriteColorIgnoreAlpha(&dstLine[nXDst * dstByte], DstFormat, dstColor);
			for (x = 1; x < nWidth; x++)
			{
				color = FreeRDPReadColor(&srcLine[(x + nXSrc) * srcByte], SrcFormat);
				if (color == oldColor)
				{
					FreeRDPWriteColorIgnoreAlpha(&dstLine[(x + nXDst) * dstByte], DstFormat,
					                             dstColor);
				}
				else
				{
					oldColor = color;
					dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
					FreeRDPWriteColorIgnoreAlpha(&dstLine[(x + nXDst) * dstByte], DstFormat,
					                             dstColor);
				}
			}
		}
	}
	else if (FreeRDPAreColorFormatsEqualNoAlpha(SrcFormat, DstFormat))
	{
		INT32 y;

		if (overlapping(pDstData, nXDst, nYDst, nDstStep, dstByte, pSrcData, nXSrc, nYSrc, nSrcStep,
		                srcByte, nWidth, nHeight))
		{
			/* Copy down */
			if (nYDst < nYSrc)
			{
				for (y = 0; y < (INT32)nHeight; y++)
				{
					const BYTE* srcLine =
					    &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
					memcpy(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy up */
			else if (nYDst > nYSrc)
			{
				for (y = nHeight - 1; y >= 0; y--)
				{
					const BYTE* srcLine =
					    &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
					memcpy(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy left */
			else if (nXSrc > nXDst)
			{
				for (y = 0; y < (INT32)nHeight; y++)
				{
					const BYTE* srcLine =
					    &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
					memmove(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Copy right */
			else if (nXSrc < nXDst)
			{
				for (y = (INT32)nHeight - 1; y >= 0; y--)
				{
					const BYTE* srcLine =
					    &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
					BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
					memmove(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
				}
			}
			/* Source and destination are equal... */
			else
			{
			}
		}
		else
		{
			for (y = 0; y < (INT32)nHeight; y++)
			{
				const BYTE* srcLine =
				    &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
				BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];
				memcpy(&dstLine[xDstOffset], &srcLine[xSrcOffset], copyDstWidth);
			}
		}
	}
	else
	{
		UINT32 x, y;

		for (y = 0; y < nHeight; y++)
		{
			const BYTE* srcLine = &pSrcData[(y + nYSrc) * nSrcStep * srcVMultiplier + srcVOffset];
			BYTE* dstLine = &pDstData[(y + nYDst) * nDstStep * dstVMultiplier + dstVOffset];

			UINT32 color = FreeRDPReadColor(&srcLine[nXSrc * srcByte], SrcFormat);
			UINT32 oldColor = color;
			UINT32 dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
			FreeRDPWriteColor(&dstLine[nXDst * dstByte], DstFormat, dstColor);
			for (x = 1; x < nWidth; x++)
			{
				color = FreeRDPReadColor(&srcLine[(x + nXSrc) * srcByte], SrcFormat);
				if (color == oldColor)
				{
					FreeRDPWriteColor(&dstLine[(x + nXDst) * dstByte], DstFormat, dstColor);
				}
				else
				{
					oldColor = color;
					dstColor = FreeRDPConvertColor(color, SrcFormat, DstFormat, palette);
					FreeRDPWriteColor(&dstLine[(x + nXDst) * dstByte], DstFormat, dstColor);
				}
			}
		}
	}

	return TRUE;
}

BOOL freerdp_image_fill(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst,
                        UINT32 nYDst, UINT32 nWidth, UINT32 nHeight, UINT32 color)
{
	UINT32 x, y;
	const UINT32 bpp = FreeRDPGetBytesPerPixel(DstFormat);
	BYTE* pFirstDstLine;
	BYTE* pFirstDstLineXOffset;

	if (nDstStep == 0)
		nDstStep = (nXDst + nWidth) * FreeRDPGetBytesPerPixel(DstFormat);

	pFirstDstLine = &pDstData[nYDst * nDstStep];
	pFirstDstLineXOffset = &pFirstDstLine[nXDst * bpp];

	for (x = 0; x < nWidth; x++)
	{
		BYTE* pDst = &pFirstDstLine[(x + nXDst) * bpp];
		FreeRDPWriteColor(pDst, DstFormat, color);
	}

	for (y = 1; y < nHeight; y++)
	{
		BYTE* pDstLine = &pDstData[(y + nYDst) * nDstStep + nXDst * bpp];
		memcpy(pDstLine, pFirstDstLineXOffset, nWidth * bpp * 1ULL);
	}

	return TRUE;
}

#if defined(WITH_SWSCALE)
static int av_format_for_buffer(UINT32 format)
{
	switch (format)
	{
		case PIXEL_FORMAT_ARGB32:
			return AV_PIX_FMT_BGRA;

		case PIXEL_FORMAT_XRGB32:
			return AV_PIX_FMT_BGR0;

		case PIXEL_FORMAT_BGRA32:
			return AV_PIX_FMT_RGBA;

		case PIXEL_FORMAT_BGRX32:
			return AV_PIX_FMT_RGB0;

		default:
			return AV_PIX_FMT_NONE;
	}
}
#endif

BOOL freerdp_image_scale(BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst,
                         UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight, const BYTE* pSrcData,
                         DWORD SrcFormat, UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                         UINT32 nSrcWidth, UINT32 nSrcHeight)
{
	BOOL rc = FALSE;

	if (nDstStep == 0)
		nDstStep = nDstWidth * FreeRDPGetBytesPerPixel(DstFormat);

	if (nSrcStep == 0)
		nSrcStep = nSrcWidth * FreeRDPGetBytesPerPixel(SrcFormat);

#if defined(WITH_SWSCALE) || defined(WITH_CAIRO)
	const BYTE* src = &pSrcData[nXSrc * FreeRDPGetBytesPerPixel(SrcFormat) + nYSrc * nSrcStep];
	BYTE* dst = &pDstData[nXDst * FreeRDPGetBytesPerPixel(DstFormat) + nYDst * nDstStep];
#endif

	/* direct copy is much faster than scaling, so check if we can simply copy... */
	if ((nDstWidth == nSrcWidth) && (nDstHeight == nSrcHeight))
	{
		return freerdp_image_copy(pDstData, DstFormat, nDstStep, nXDst, nYDst, nDstWidth,
		                          nDstHeight, pSrcData, SrcFormat, nSrcStep, nXSrc, nYSrc, NULL,
		                          FREERDP_FLIP_NONE);
	}
	else
#if defined(WITH_SWSCALE)
	{
		int res;
		struct SwsContext* resize;
		int srcFormat = av_format_for_buffer(SrcFormat);
		int dstFormat = av_format_for_buffer(DstFormat);
		const int srcStep[1] = { (int)nSrcStep };
		const int dstStep[1] = { (int)nDstStep };

		if ((srcFormat == AV_PIX_FMT_NONE) || (dstFormat == AV_PIX_FMT_NONE))
			return FALSE;

		resize = sws_getContext((int)nSrcWidth, (int)nSrcHeight, srcFormat, (int)nDstWidth,
		                        (int)nDstHeight, dstFormat, SWS_BILINEAR, NULL, NULL, NULL);

		if (!resize)
			goto fail;

		res = sws_scale(resize, &src, srcStep, 0, (int)nSrcHeight, &dst, dstStep);
		rc = (res == ((int)nDstHeight));
	fail:
		sws_freeContext(resize);
	}

#elif defined(WITH_CAIRO)
	{
		const double sx = (double)nDstWidth / (double)nSrcWidth;
		const double sy = (double)nDstHeight / (double)nSrcHeight;
		cairo_t* cairo_context;
		cairo_surface_t *csrc, *cdst;

		if ((nSrcWidth > INT_MAX) || (nSrcHeight > INT_MAX) || (nSrcStep > INT_MAX))
			return FALSE;

		if ((nDstWidth > INT_MAX) || (nDstHeight > INT_MAX) || (nDstStep > INT_MAX))
			return FALSE;

		csrc = cairo_image_surface_create_for_data((void*)src, CAIRO_FORMAT_ARGB32, (int)nSrcWidth,
		                                           (int)nSrcHeight, (int)nSrcStep);
		cdst = cairo_image_surface_create_for_data(dst, CAIRO_FORMAT_ARGB32, (int)nDstWidth,
		                                           (int)nDstHeight, (int)nDstStep);

		if (!csrc || !cdst)
			goto fail;

		cairo_context = cairo_create(cdst);

		if (!cairo_context)
			goto fail2;

		cairo_scale(cairo_context, sx, sy);
		cairo_set_operator(cairo_context, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(cairo_context, csrc, 0, 0);
		cairo_paint(cairo_context);
		rc = TRUE;
	fail2:
		cairo_destroy(cairo_context);
	fail:
		cairo_surface_destroy(csrc);
		cairo_surface_destroy(cdst);
	}
#else
	{
		WLog_WARN(TAG, "SmartScaling requested but compiled without libcairo support!");
	}
#endif
	return rc;
}

DWORD FreeRDPAreColorFormatsEqualNoAlpha(DWORD first, DWORD second)
{
	const DWORD mask = (DWORD) ~(8UL << 12UL);
	return (first & mask) == (second & mask);
}

const char* FreeRDPGetColorFormatName(UINT32 format)
{
	switch (format)
	{
		/* 32bpp formats */
		case PIXEL_FORMAT_ARGB32:
			return "PIXEL_FORMAT_ARGB32";

		case PIXEL_FORMAT_XRGB32:
			return "PIXEL_FORMAT_XRGB32";

		case PIXEL_FORMAT_ABGR32:
			return "PIXEL_FORMAT_ABGR32";

		case PIXEL_FORMAT_XBGR32:
			return "PIXEL_FORMAT_XBGR32";

		case PIXEL_FORMAT_BGRA32:
			return "PIXEL_FORMAT_BGRA32";

		case PIXEL_FORMAT_BGRX32:
			return "PIXEL_FORMAT_BGRX32";

		case PIXEL_FORMAT_RGBA32:
			return "PIXEL_FORMAT_RGBA32";

		case PIXEL_FORMAT_RGBX32:
			return "PIXEL_FORMAT_RGBX32";

		case PIXEL_FORMAT_BGRX32_DEPTH30:
			return "PIXEL_FORMAT_BGRX32_DEPTH30";

		case PIXEL_FORMAT_RGBX32_DEPTH30:
			return "PIXEL_FORMAT_RGBX32_DEPTH30";

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			return "PIXEL_FORMAT_RGB24";

		case PIXEL_FORMAT_BGR24:
			return "PIXEL_FORMAT_BGR24";

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			return "PIXEL_FORMAT_RGB16";

		case PIXEL_FORMAT_BGR16:
			return "PIXEL_FORMAT_BGR16";

		case PIXEL_FORMAT_ARGB15:
			return "PIXEL_FORMAT_ARGB15";

		case PIXEL_FORMAT_RGB15:
			return "PIXEL_FORMAT_RGB15";

		case PIXEL_FORMAT_ABGR15:
			return "PIXEL_FORMAT_ABGR15";

		case PIXEL_FORMAT_BGR15:
			return "PIXEL_FORMAT_BGR15";

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:
			return "PIXEL_FORMAT_RGB8";

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:
			return "PIXEL_FORMAT_A4";

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
			return "PIXEL_FORMAT_MONO";

		default:
			return "UNKNOWN";
	}
}

void FreeRDPSplitColor(UINT32 color, UINT32 format, BYTE* _r, BYTE* _g, BYTE* _b, BYTE* _a,
                       const gdiPalette* palette)
{
	UINT32 tmp;

	switch (format)
	{
		/* 32bpp formats */
		case PIXEL_FORMAT_ARGB32:
			if (_a)
				*_a = (BYTE)(color >> 24);

			if (_r)
				*_r = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_b)
				*_b = (BYTE)color;

			break;

		case PIXEL_FORMAT_XRGB32:
			if (_r)
				*_r = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_b)
				*_b = (BYTE)color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_ABGR32:
			if (_a)
				*_a = (BYTE)(color >> 24);

			if (_b)
				*_b = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_r)
				*_r = (BYTE)color;

			break;

		case PIXEL_FORMAT_XBGR32:
			if (_b)
				*_b = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_r)
				*_r = (BYTE)color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_RGBA32:
			if (_r)
				*_r = (BYTE)(color >> 24);

			if (_g)
				*_g = (BYTE)(color >> 16);

			if (_b)
				*_b = (BYTE)(color >> 8);

			if (_a)
				*_a = (BYTE)color;

			break;

		case PIXEL_FORMAT_RGBX32:
			if (_r)
				*_r = (BYTE)(color >> 24);

			if (_g)
				*_g = (BYTE)(color >> 16);

			if (_b)
				*_b = (BYTE)(color >> 8);

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGRA32:
			if (_b)
				*_b = (BYTE)(color >> 24);

			if (_g)
				*_g = (BYTE)(color >> 16);

			if (_r)
				*_r = (BYTE)(color >> 8);

			if (_a)
				*_a = (BYTE)color;

			break;

		case PIXEL_FORMAT_BGRX32:
			if (_b)
				*_b = (BYTE)(color >> 24);

			if (_g)
				*_g = (BYTE)(color >> 16);

			if (_r)
				*_r = (BYTE)(color >> 8);

			if (_a)
				*_a = 0xFF;

			break;

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			if (_r)
				*_r = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_b)
				*_b = (BYTE)color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR24:
			if (_b)
				*_b = (BYTE)(color >> 16);

			if (_g)
				*_g = (BYTE)(color >> 8);

			if (_r)
				*_r = (BYTE)color;

			if (_a)
				*_a = 0xFF;

			break;

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			if (_r)
			{
				const UINT32 c = (color >> 11) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x3F;
				const UINT32 val = (c << 2) + c / 4 / 2;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR16:
			if (_r)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x3F;
				const UINT32 val = (c << 2) + c / 4 / 2;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color >> 11) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_ARGB15:
			if (_r)
			{
				const UINT32 c = (color >> 10) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		case PIXEL_FORMAT_ABGR15:
			if (_r)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color >> 10) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			if (_r)
			{
				const UINT32 c = (color >> 10) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR15:
			if (_r)
			{
				const UINT32 c = (color)&0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_r = (BYTE)(val > 255 ? 255 : val);
			}

			if (_g)
			{
				const UINT32 c = (color >> 5) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_g = (BYTE)(val > 255 ? 255 : val);
			}

			if (_b)
			{
				const UINT32 c = (color >> 10) & 0x1F;
				const UINT32 val = (c << 3) + c / 4;
				*_b = (BYTE)(val > 255 ? 255 : val);
			}

			if (_a)
				*_a = 0xFF;

			break;

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:
			if (color <= 0xFF)
			{
				tmp = palette->palette[color];
				FreeRDPSplitColor(tmp, palette->format, _r, _g, _b, _a, NULL);
			}
			else
			{
				if (_r)
					*_r = 0x00;

				if (_g)
					*_g = 0x00;

				if (_b)
					*_b = 0x00;

				if (_a)
					*_a = 0x00;
			}

			break;

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
			if (_r)
				*_r = (color) ? 0xFF : 0x00;

			if (_g)
				*_g = (color) ? 0xFF : 0x00;

			if (_b)
				*_b = (color) ? 0xFF : 0x00;

			if (_a)
				*_a = (color) ? 0xFF : 0x00;

			break;

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:
		default:
			if (_r)
				*_r = 0x00;

			if (_g)
				*_g = 0x00;

			if (_b)
				*_b = 0x00;

			if (_a)
				*_a = 0x00;

			WLog_ERR(TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			break;
	}
}

BOOL FreeRDPWriteColorIgnoreAlpha(BYTE* dst, UINT32 format, UINT32 color)
{
	switch (format)
	{
		case PIXEL_FORMAT_XBGR32:
		case PIXEL_FORMAT_XRGB32:
		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_ARGB32:
		{
			const UINT32 tmp = ((UINT32)dst[0] << 24ULL) | (color & 0x00FFFFFFULL);
			return FreeRDPWriteColor(dst, format, tmp);
		}
		case PIXEL_FORMAT_BGRX32:
		case PIXEL_FORMAT_RGBX32:
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_RGBA32:
		{
			const UINT32 tmp = ((UINT32)dst[3]) | (color & 0xFFFFFF00ULL);
			return FreeRDPWriteColor(dst, format, tmp);
		}
		default:
			return FreeRDPWriteColor(dst, format, color);
	}
}

BOOL FreeRDPWriteColor(BYTE* dst, UINT32 format, UINT32 color)
{
	switch (FreeRDPGetBitsPerPixel(format))
	{
		case 32:
			dst[0] = (BYTE)(color >> 24);
			dst[1] = (BYTE)(color >> 16);
			dst[2] = (BYTE)(color >> 8);
			dst[3] = (BYTE)color;
			break;

		case 24:
			dst[0] = (BYTE)(color >> 16);
			dst[1] = (BYTE)(color >> 8);
			dst[2] = (BYTE)color;
			break;

		case 16:
			dst[1] = (BYTE)(color >> 8);
			dst[0] = (BYTE)color;
			break;

		case 15:
			if (!FreeRDPColorHasAlpha(format))
				color = color & 0x7FFF;

			dst[1] = (BYTE)(color >> 8);
			dst[0] = (BYTE)color;
			break;

		case 8:
			dst[0] = (BYTE)color;
			break;

		default:
			WLog_ERR(TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			return FALSE;
	}

	return TRUE;
}

UINT32 FreeRDPReadColor(const BYTE* src, UINT32 format)
{
	UINT32 color;

	switch (FreeRDPGetBitsPerPixel(format))
	{
		case 32:
			color =
			    ((UINT32)src[0] << 24) | ((UINT32)src[1] << 16) | ((UINT32)src[2] << 8) | src[3];
			break;

		case 24:
			color = ((UINT32)src[0] << 16) | ((UINT32)src[1] << 8) | src[2];
			break;

		case 16:
			color = ((UINT32)src[1] << 8) | src[0];
			break;

		case 15:
			color = ((UINT32)src[1] << 8) | src[0];

			if (!FreeRDPColorHasAlpha(format))
				color = color & 0x7FFF;

			break;

		case 8:
		case 4:
		case 1:
			color = *src;
			break;

		default:
			WLog_ERR(TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			color = 0;
			break;
	}

	return color;
}

UINT32 FreeRDPGetColor(UINT32 format, BYTE r, BYTE g, BYTE b, BYTE a)
{
	UINT32 _r = r;
	UINT32 _g = g;
	UINT32 _b = b;
	UINT32 _a = a;
	UINT32 t;

	switch (format)
	{
		/* 32bpp formats */
		case PIXEL_FORMAT_ARGB32:
			return (_a << 24) | (_r << 16) | (_g << 8) | _b;

		case PIXEL_FORMAT_XRGB32:
			return (_r << 16) | (_g << 8) | _b;

		case PIXEL_FORMAT_ABGR32:
			return (_a << 24) | (_b << 16) | (_g << 8) | _r;

		case PIXEL_FORMAT_XBGR32:
			return (_b << 16) | (_g << 8) | _r;

		case PIXEL_FORMAT_RGBA32:
			return (_r << 24) | (_g << 16) | (_b << 8) | _a;

		case PIXEL_FORMAT_RGBX32:
			return (_r << 24) | (_g << 16) | (_b << 8) | _a;

		case PIXEL_FORMAT_BGRA32:
			return (_b << 24) | (_g << 16) | (_r << 8) | _a;

		case PIXEL_FORMAT_BGRX32:
			return (_b << 24) | (_g << 16) | (_r << 8) | _a;

		case PIXEL_FORMAT_RGBX32_DEPTH30:
			// TODO: Not tested
			t = (_r << 22) | (_g << 12) | (_b << 2);
			// NOTE: Swapping byte-order because FreeRDPWriteColor written UINT32 in big-endian
			return ((t & 0xff) << 24) | (((t >> 8) & 0xff) << 16) | (((t >> 16) & 0xff) << 8) |
			       (t >> 24);

		case PIXEL_FORMAT_BGRX32_DEPTH30:
			// NOTE: Swapping b and r channel (unknown reason)
			t = (_r << 22) | (_g << 12) | (_b << 2);
			// NOTE: Swapping byte-order because FreeRDPWriteColor written UINT32 in big-endian
			return ((t & 0xff) << 24) | (((t >> 8) & 0xff) << 16) | (((t >> 16) & 0xff) << 8) |
			       (t >> 24);

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			return (_r << 16) | (_g << 8) | _b;

		case PIXEL_FORMAT_BGR24:
			return (_b << 16) | (_g << 8) | _r;

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			return (((_r >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((_b >> 3) & 0x1F);

		case PIXEL_FORMAT_BGR16:
			return (((_b >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((_r >> 3) & 0x1F);

		case PIXEL_FORMAT_ARGB15:
			return (((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_b >> 3) & 0x1F) |
			       (_a ? 0x8000 : 0x0000);

		case PIXEL_FORMAT_ABGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_r >> 3) & 0x1F) |
			       (_a ? 0x8000 : 0x0000);

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			return (((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_b >> 3) & 0x1F);

		case PIXEL_FORMAT_BGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_r >> 3) & 0x1F);

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
		default:
			WLog_ERR(TAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			return 0;
	}
}
