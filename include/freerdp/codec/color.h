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

#ifndef FREERDP_CODEC_COLOR_H
#define FREERDP_CODEC_COLOR_H

#include <freerdp/api.h>
#include <winpr/wlog.h>
#include <freerdp/log.h>
#define CTAG FREERDP_TAG("codec.color")

#define FREERDP_PIXEL_FORMAT_TYPE_A		0
#define FREERDP_PIXEL_FORMAT_TYPE_ARGB		1
#define FREERDP_PIXEL_FORMAT_TYPE_ABGR		2
#define FREERDP_PIXEL_FORMAT_TYPE_RGBA		3
#define FREERDP_PIXEL_FORMAT_TYPE_BGRA		4

#define FREERDP_PIXEL_FORMAT_IS_ABGR(_format)	(FREERDP_PIXEL_FORMAT_TYPE(_format) == FREERDP_PIXEL_FORMAT_TYPE_ABGR)

#define FREERDP_FLIP_NONE			0
#define FREERDP_FLIP_VERTICAL		1
#define FREERDP_FLIP_HORIZONTAL		2

#define FREERDP_PIXEL_FORMAT(_bpp, _type, _a, _r, _g, _b) \
	((_bpp << 24) | (_type << 16) | (_a << 12) | (_r << 8) | (_g << 4) | (_b))

#define FREERDP_PIXEL_FORMAT_TYPE(_format)	(((_format) >> 16) & 0x07)

/*** Design considerations
 *
 * The format naming scheme is based on byte position in memory.
 * RGBA for example names a byte array with red on positon 0, green on 1 etc.
 *
 * To read and write the appropriate format from / to memory use ReadColor and
 * WriteColor.
 *
 * The single pixel manipulation functions use an intermediate integer representation
 * that must not be interpreted outside the functions as it is platform dependent.
 *
 * X for alpha channel denotes unused (but existing) alpha channel data.
 */

/* 32bpp formats */
#define PIXEL_FORMAT_ARGB32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 8, 8, 8, 8)
#define PIXEL_FORMAT_XRGB32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_ABGR32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 8, 8, 8, 8)
#define PIXEL_FORMAT_XBGR32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGRA32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 8, 8, 8, 8)
#define PIXEL_FORMAT_BGRX32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 0, 8, 8, 8)
#define PIXEL_FORMAT_RGBA32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 8, 8, 8, 8)
#define PIXEL_FORMAT_RGBX32	FREERDP_PIXEL_FORMAT(32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 0, 8, 8, 8)

/* 24bpp formats */
#define PIXEL_FORMAT_RGB24	FREERDP_PIXEL_FORMAT(24, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGR24	FREERDP_PIXEL_FORMAT(24, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)

/* 16bpp formats */
#define PIXEL_FORMAT_RGB16	FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 6, 5)
#define PIXEL_FORMAT_BGR16	FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 6, 5)
#define PIXEL_FORMAT_ARGB15	FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 1, 5, 5, 5)
#define PIXEL_FORMAT_RGB15	FREERDP_PIXEL_FORMAT(15, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 5, 5)
#define PIXEL_FORMAT_ABGR15	FREERDP_PIXEL_FORMAT(16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 1, 5, 5, 5)
#define PIXEL_FORMAT_BGR15	FREERDP_PIXEL_FORMAT(15, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 5, 5)

/* 8bpp formats */
#define PIXEL_FORMAT_RGB8	FREERDP_PIXEL_FORMAT(8, FREERDP_PIXEL_FORMAT_TYPE_A, 8, 0, 0, 0)

/* 4 bpp formats */
#define PIXEL_FORMAT_A4	FREERDP_PIXEL_FORMAT(4, FREERDP_PIXEL_FORMAT_TYPE_A, 4, 0, 0, 0)

/* 1bpp formats */
#define PIXEL_FORMAT_MONO	FREERDP_PIXEL_FORMAT(1, FREERDP_PIXEL_FORMAT_TYPE_A, 1, 0, 0, 0)

struct gdi_palette
{
	UINT32 format;
	UINT32 palette[256];
};
typedef struct gdi_palette gdiPalette;

#ifdef __cplusplus
extern "C" {
#endif

/* Compare two color formats but ignore differences in alpha channel.
 */
static INLINE DWORD AreColorFormatsEqualNoAlpha(DWORD first, DWORD second)
{
	const DWORD mask = ~(8 << 12);
	return (first & mask) == (second & mask);
}

/* Color Space Conversions: http://msdn.microsoft.com/en-us/library/ff566496/ */

/***
 *
 * Get a string representation of a color
 *
 * @param format The pixel color format
 *
 * @return A string representation of format
 */
static const char* FreeRDPGetColorFormatName(UINT32 format)
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

/***
 *
 * Converts a pixel color in internal representation to its red, green, blue
 * and alpha components.
 *
 * @param color  The color in format internal representation
 * @param format one of PIXEL_FORMAT_* color format defines
 * @param _r      red color value
 * @param _g      green color value
 * @param _b      blue color value
 * @param _a      alpha color value
 * @param palette pallete to use (only used for 8 bit color!)
 */
static INLINE void SplitColor(UINT32 color, UINT32 format, BYTE* _r, BYTE* _g,
                              BYTE* _b, BYTE* _a, const gdiPalette* palette)
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
				*_r = (BYTE)(((color >> 11) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x3F) << 2);

			if (_b)
				*_b = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR16:
			if (_b)
				*_b = (BYTE)(((color >> 11) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x3F) << 2);

			if (_r)
				*_r = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_ARGB15:
			if (_r)
				*_r = (BYTE)(((color >> 10) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x1F) << 3);

			if (_b)
				*_b = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		case PIXEL_FORMAT_ABGR15:
			if (_b)
				*_b = (BYTE)(((color >> 10) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x1F) << 3);

			if (_r)
				*_r = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			if (_r)
				*_r = (BYTE)(((color >> 10) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x1F) << 3);

			if (_b)
				*_b = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR15:
			if (_b)
				*_b = (BYTE)(((color >> 10) & 0x1F) << 3);

			if (_g)
				*_g = (BYTE)(((color >> 5) & 0x1F) << 3);

			if (_r)
				*_r = (BYTE)((color & 0x1F) << 3);

			if (_a)
				*_a = 0xFF;

			break;

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:
			if (color <= 0xFF)
			{
				tmp = palette->palette[color];
				SplitColor(tmp, palette->format, _r, _g, _b, _a, NULL);
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

			WLog_ERR(CTAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			break;
	}
}

/***
 *
 * Converts red, green, blue and alpha values to internal representation.
 *
 * @param format one of PIXEL_FORMAT_* color format defines
 * @param r      red color value
 * @param g      green color value
 * @param b      blue color value
 * @param a      alpha color value
 *
 * @return       The pixel color in the desired format. Value is in internal
 *               representation.
 */
static INLINE UINT32 FreeRDPGetColor(UINT32 format, BYTE r, BYTE g, BYTE b, BYTE a)
{
	UINT32 _r = r;
	UINT32 _g = g;
	UINT32 _b = b;
	UINT32 _a = a;

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

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			return (_r << 16) | (_g << 8) | _b;

		case PIXEL_FORMAT_BGR24:
			return (_b << 16) | (_g << 8) | _r;

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			return (((_r >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((
			            _b >> 3) & 0x1F);

		case PIXEL_FORMAT_BGR16:
			return (((_b >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((
			            _r >> 3) & 0x1F);

		case PIXEL_FORMAT_ARGB15:
			return (((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _b >> 3) & 0x1F) | (_a ? 0x8000 : 0x0000);

		case PIXEL_FORMAT_ABGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _r >> 3) & 0x1F) | (_a ? 0x8000 : 0x0000);

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			return (((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _b >> 3) & 0x1F);

		case PIXEL_FORMAT_BGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _r >> 3) & 0x1F);

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
		default:
			WLog_ERR(CTAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			return 0;
	}
}

/***
 *
 * Returns the number of bits the format format uses.
 *
 * @param format One of PIXEL_FORMAT_* defines
 *
 * @return The number of bits the format requires per pixel.
 */
static INLINE UINT32 GetBitsPerPixel(UINT32 format)
{
	return (((format) >> 24) & 0x3F);
}

/***
 * @param format one of PIXEL_FORMAT_* color format defines
 *
 * @return TRUE if the format has an alpha channel, FALSE otherwise.
 */
static INLINE BOOL ColorHasAlpha(UINT32 format)
{
	UINT32 alpha = (((format) >> 12) & 0x0F);

	if (alpha == 0)
		return FALSE;

	return TRUE;
}

/***
 *
 * Read a pixel from memory to internal representation
 *
 * @param src    The source buffer
 * @param format The PIXEL_FORMAT_* define the source buffer uses for encoding
 *
 * @return The pixel color in internal representation
 */
static INLINE UINT32 ReadColor(const BYTE* src, UINT32 format)
{
	UINT32 color;

	switch (GetBitsPerPixel(format))
	{
		case 32:
			color = ((UINT32)src[0] << 24) | ((UINT32)src[1] << 16) |
			        ((UINT32)src[2] << 8) | src[3];
			break;

		case 24:
			color = ((UINT32)src[0] << 16) | ((UINT32)src[1] << 8) | src[2];
			break;

		case 16:
			color = ((UINT32)src[1] << 8) | src[0];
			break;

		case 15:
			color = ((UINT32)src[1] << 8) | src[0];

			if (!ColorHasAlpha(format))
				color = color & 0x7FFF;

			break;

		case 8:
		case 4:
		case 1:
			color = *src;
			break;

		default:
			WLog_ERR(CTAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			color = 0;
			break;
	}

	return color;
}

/***
 *
 * Write a pixel from internal representation to memory
 *
 * @param dst    The destination buffer
 * @param format The PIXEL_FORMAT_* define for encoding
 * @param color  The pixel color in internal representation
 *
 * @return TRUE if successful, FALSE otherwise
 */
static INLINE BOOL WriteColor(BYTE* dst, UINT32 format, UINT32 color)
{
	switch (GetBitsPerPixel(format))
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
			if (!ColorHasAlpha(format))
				color = color & 0x7FFF;

			dst[1] = (BYTE)(color >> 8);
			dst[0] = (BYTE)color;
			break;

		case 8:
			dst[0] = (BYTE)color;
			break;

		default:
			WLog_ERR(CTAG, "Unsupported format %s", FreeRDPGetColorFormatName(format));
			return FALSE;
	}

	return TRUE;
}

/***
 *
 * Converts a pixel in internal representation format srcFormat to internal
 * representation format dstFormat
 *
 * @param color      The pixel color in srcFormat representation
 * @param srcFormat  The PIXEL_FORMAT_* of color
 * @param dstFormat  The PIXEL_FORMAT_* of the return.
 * @param palette    pallete to use (only used for 8 bit color!)
 *
 * @return           The converted pixel color in dstFormat representation
 */
static INLINE UINT32 FreeRDPConvertColor(UINT32 color, UINT32 srcFormat,
                                  UINT32 dstFormat, const gdiPalette* palette)
{
	BYTE r = 0;
	BYTE g = 0;
	BYTE b = 0;
	BYTE a = 0;
	SplitColor(color, srcFormat, &r, &g, &b, &a, palette);
	return FreeRDPGetColor(dstFormat, r, g, b, a);
}

/***
 *
 * Returns the number of bytes the format format uses.
 *
 * @param format One of PIXEL_FORMAT_* defines
 *
 * @return The number of bytes the format requires per pixel.
 */
static INLINE UINT32 GetBytesPerPixel(UINT32 format)
{
	return (GetBitsPerPixel(format) + 7) / 8;
}

/***
 *
 * @param nWidth    width to copy in pixels
 * @param nHeight   height to copy in pixels
 * @param data      source buffer, must be (nWidth + 7) / 8 bytes long
 *
 * @return          A buffer allocated with _aligned_malloc(width * height, 16)
 *                  if successufl, NULL otherwise.
 */
FREERDP_API BYTE* freerdp_glyph_convert(UINT32 width, UINT32 height,
                                        const BYTE* data);

/***
 *
 * @param pDstData  destination buffer
 * @param DstFormat destination buffer format
 * @param nDstStep  destination buffer stride (line in bytes) 0 for default
 * @param nXDst     destination buffer offset x
 * @param nYDst     destination buffer offset y
 * @param nWidth    width to copy in pixels
 * @param nHeight   height to copy in pixels
 * @param pSrcData  source buffer, must be (nWidth + 7) / 8 bytes long
 * @param backColor The background color in internal representation format
 * @param foreColor The foreground color in internal representation format
 * @param palette   palette to use (only used for 8 bit color!)
 *
 * @return          TRUE if success, FALSE otherwise
 */
FREERDP_API BOOL freerdp_image_copy_from_monochrome(BYTE* pDstData,
        UINT32 DstFormat,
        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
        UINT32 nWidth, UINT32 nHeight,
        const BYTE* pSrcData,
        UINT32 backColor, UINT32 foreColor,
        const gdiPalette* palette);

/***
 *
 * @param pDstData      destination buffer
 * @param DstFormat     destination buffer format
 * @param nDstStep      destination buffer stride (line in bytes) 0 for default
 * @param nXDst         destination buffer offset x
 * @param nYDst         destination buffer offset y
 * @param nWidth        width to copy in pixels
 * @param nHeight       height to copy in pixels
 * @param xorMask       XOR mask buffer
 * @param xorMaskLength XOR mask length in bytes
 * @param andMask       AND mask buffer
 * @param andMaskLength AND mask length in bytes
 * @param xorBpp        XOR bits per pixel
 * @param palette       palette to use (only used for 8 bit color!)
 *
 * @return              TRUE if success, FALSE otherwise
 */
FREERDP_API BOOL freerdp_image_copy_from_pointer_data(
    BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
    const BYTE* xorMask, UINT32 xorMaskLength,
    const BYTE* andMask, UINT32 andMaskLength,
    UINT32 xorBpp, const gdiPalette* palette);

/***
 *
 * @param pDstData  destination buffer
 * @param DstFormat destination buffer format
 * @param nDstStep  destination buffer stride (line in bytes) 0 for default
 * @param nXDst     destination buffer offset x
 * @param nYDst     destination buffer offset y
 * @param nWidth    width to copy in pixels
 * @param nHeight   height to copy in pixels
 * @param pSrcData  source buffer
 * @param SrcFormat source buffer format
 * @param nSrcStep  source buffer stride (line in bytes) 0 for default
 * @param nXSrc     source buffer x offset in pixels
 * @param nYSrc     source buffer y offset in pixels
 * @param palette   palette to use (only used for 8 bit color!)
 * @param flags     Image flipping flags FREERDP_FLIP_NONE et al
 *
 * @return          TRUE if success, FALSE otherwise
 */
FREERDP_API BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat,
                                    UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                    UINT32 nWidth, UINT32 nHeight,
                                    const BYTE* pSrcData, DWORD SrcFormat,
                                    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                    const gdiPalette* palette, UINT32 flags);

/***
 *
 * @param pDstData  destionation buffer
 * @param DstFormat destionation buffer format
 * @param nDstStep  destionation buffer stride (line in bytes) 0 for default
 * @param nXDst     destination buffer offset x
 * @param nYDst     destination buffer offset y
 * @param nWidth    width to copy in pixels
 * @param nHeight   height to copy in pixels
 * @param color     Pixel color in DstFormat (internal representation format,
 *                  use FreeRDPGetColor to create)
 *
 * @return          TRUE if success, FALSE otherwise
 */
FREERDP_API BOOL freerdp_image_fill(BYTE* pDstData, DWORD DstFormat,
                                    UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                    UINT32 nWidth, UINT32 nHeight, UINT32 color);

#if !defined(__APPLE__)
#define GetColorFormatName FreeRDPGetColorFormatName
#define GetColor FreeRDPGetColor
#define ConvertColor FreeRDPConvertColor
#endif

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_COLOR_H */
