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
#include <freerdp/freerdp.h>
#include <winpr/wlog.h>

#define FREERDP_PIXEL_FORMAT_TYPE_A		0
#define FREERDP_PIXEL_FORMAT_TYPE_ARGB		1
#define FREERDP_PIXEL_FORMAT_TYPE_ABGR		2
#define FREERDP_PIXEL_FORMAT_TYPE_RGBA		3
#define FREERDP_PIXEL_FORMAT_TYPE_BGRA		4

#define FREERDP_PIXEL_FORMAT_IS_ABGR(_format)	(FREERDP_PIXEL_FORMAT_TYPE(_format) == FREERDP_PIXEL_FORMAT_TYPE_ABGR)

#define FREERDP_PIXEL_FLIP_NONE			0
#define FREERDP_PIXEL_FLIP_VERTICAL		1
#define FREERDP_PIXEL_FLIP_HORIZONTAL		2

#define FREERDP_PIXEL_FORMAT(_flip, _bpp, _type, _a, _r, _g, _b) \
	((_flip << 30) | (_bpp << 24) | (_type << 16) | (_a << 12) | (_r << 8) | (_g << 4) | (_b))

#define FREERDP_PIXEL_FORMAT_FLIP(_format)	(((_format) >> 30) & 0x03)
#define FREERDP_PIXEL_FORMAT_TYPE(_format)	(((_format) >> 16) & 0x07)

#define FREERDP_PIXEL_FORMAT_FLIP_MASKED(_format) (_format & 0x3FFFFFFF)

/* 32bpp formats */
#define PIXEL_FORMAT_A8R8G8B8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 8, 8, 8, 8)
#define PIXEL_FORMAT_ARGB32		PIXEL_FORMAT_A8R8G8B8_F(0)
#define PIXEL_FORMAT_ARGB32_VF		PIXEL_FORMAT_A8R8G8B8_F(1)

#define PIXEL_FORMAT_X8R8G8B8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_XRGB32		PIXEL_FORMAT_X8R8G8B8_F(0)
#define PIXEL_FORMAT_XRGB32_VF		PIXEL_FORMAT_X8R8G8B8_F(1)

#define PIXEL_FORMAT_A8B8G8R8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 8, 8, 8, 8)
#define PIXEL_FORMAT_ABGR32		PIXEL_FORMAT_A8B8G8R8_F(0)
#define PIXEL_FORMAT_ABGR32_VF		PIXEL_FORMAT_A8B8G8R8_F(1)

#define PIXEL_FORMAT_X8B8G8R8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_XBGR32		PIXEL_FORMAT_X8B8G8R8_F(0)
#define PIXEL_FORMAT_XBGR32_VF		PIXEL_FORMAT_X8B8G8R8_F(1)

#define PIXEL_FORMAT_B8G8R8A8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 8, 8, 8, 8)
#define PIXEL_FORMAT_BGRA32		PIXEL_FORMAT_B8G8R8A8_F(0)
#define PIXEL_FORMAT_BGRA32_VF		PIXEL_FORMAT_B8G8R8A8_F(1)

#define PIXEL_FORMAT_B8G8R8X8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGRX32		PIXEL_FORMAT_B8G8R8X8_F(0)
#define PIXEL_FORMAT_BGRX32_VF		PIXEL_FORMAT_B8G8R8X8_F(1)

#define PIXEL_FORMAT_R8G8B8A8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 8, 8, 8, 8)
#define PIXEL_FORMAT_RGBA32		PIXEL_FORMAT_R8G8B8A8_F(0)
#define PIXEL_FORMAT_RGBA32_VF		PIXEL_FORMAT_R8G8B8A8_F(1)

#define PIXEL_FORMAT_R8G8B8X8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 0, 8, 8, 8)
#define PIXEL_FORMAT_RGBX32		PIXEL_FORMAT_R8G8B8X8_F(0)
#define PIXEL_FORMAT_RGBX32_VF		PIXEL_FORMAT_R8G8B8X8_F(1)

/* 24bpp formats */
#define PIXEL_FORMAT_R8G8B8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 24, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_RGB24		PIXEL_FORMAT_R8G8B8_F(0)
#define PIXEL_FORMAT_RGB24_VF		PIXEL_FORMAT_R8G8B8_F(1)

#define PIXEL_FORMAT_B8G8R8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 24, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGR24		PIXEL_FORMAT_B8G8R8_F(0)
#define PIXEL_FORMAT_BGR24_VF		PIXEL_FORMAT_B8G8R8_F(1)

/* 16bpp formats */
#define PIXEL_FORMAT_R5G6B5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 6, 5)
#define PIXEL_FORMAT_RGB16		PIXEL_FORMAT_R5G6B5_F(0)
#define PIXEL_FORMAT_RGB16_VF		PIXEL_FORMAT_R5G6B5_F(1)

#define PIXEL_FORMAT_B5G6R5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 6, 5)
#define PIXEL_FORMAT_BGR16		PIXEL_FORMAT_B5G6R5_F(0)
#define PIXEL_FORMAT_BGR16_VF		PIXEL_FORMAT_B5G6R5_F(1)

#define PIXEL_FORMAT_A1R5G5B5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 1, 5, 5, 5)
#define PIXEL_FORMAT_ARGB15		PIXEL_FORMAT_A1R5G5B5_F(0)
#define PIXEL_FORMAT_ARGB15_VF		PIXEL_FORMAT_A1R5G5B5_F(1)

#define PIXEL_FORMAT_X1R5G5B5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 5, 5)
#define PIXEL_FORMAT_RGB15		PIXEL_FORMAT_X1R5G5B5_F(0)
#define PIXEL_FORMAT_RGB15_VF		PIXEL_FORMAT_X1R5G5B5_F(1)

#define PIXEL_FORMAT_A1B5G5R5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 1, 5, 5, 5)
#define PIXEL_FORMAT_ABGR15		PIXEL_FORMAT_A1B5G5R5_F(0)
#define PIXEL_FORMAT_ABGR15_VF		PIXEL_FORMAT_A1B5G5R5_F(1)

#define PIXEL_FORMAT_X1B5G5R5_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 5, 5)
#define PIXEL_FORMAT_BGR15		PIXEL_FORMAT_X1B5G5R5_F(0)
#define PIXEL_FORMAT_BGR15_VF		PIXEL_FORMAT_X1B5G5R5_F(1)

/* 8bpp formats */
#define PIXEL_FORMAT_A8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 8, FREERDP_PIXEL_FORMAT_TYPE_A, 8, 0, 0, 0)
#define PIXEL_FORMAT_RGB8		PIXEL_FORMAT_A8_F(0)
#define PIXEL_FORMAT_RGB8_VF		PIXEL_FORMAT_A8_F(1)

/* 4 bpp formats */
#define PIXEL_FORMAT_A4_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 4, FREERDP_PIXEL_FORMAT_TYPE_A, 4, 0, 0, 0)
#define PIXEL_FORMAT_A4			PIXEL_FORMAT_A4_F(0)
#define PIXEL_FORMAT_A4_VF		PIXEL_FORMAT_A4_F(1)

/* 1bpp formats */
#define PIXEL_FORMAT_A1_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 1, FREERDP_PIXEL_FORMAT_TYPE_A, 1, 0, 0, 0)
#define PIXEL_FORMAT_MONO		PIXEL_FORMAT_A1_F(0)
#define PIXEL_FORMAT_MONO_VF		PIXEL_FORMAT_A1_F(1)

#ifdef __cplusplus
extern "C" {
#endif

/* Color Space Conversions: http://msdn.microsoft.com/en-us/library/ff566496/ */
static const char* GetColorFormatName(UINT32 format)
{
	switch (format)
	{
		/* 32bpp formats */
		case PIXEL_FORMAT_ARGB32:
			return "PIXEL_FORMAT_ARGB32";

		case PIXEL_FORMAT_ARGB32_VF:
			return "PIXEL_FORMAT_ARGB32_VF";

		case PIXEL_FORMAT_XRGB32:
			return "PIXEL_FORMAT_XRGB32";

		case PIXEL_FORMAT_XRGB32_VF:
			return "PIXEL_FORMAT_XRGB32_VF";

		case PIXEL_FORMAT_ABGR32:
			return "PIXEL_FORMAT_ABGR32";

		case PIXEL_FORMAT_ABGR32_VF:
			return "PIXEL_FORMAT_ABGR32_VF";

		case PIXEL_FORMAT_XBGR32:
			return "PIXEL_FORMAT_XBGR32";

		case PIXEL_FORMAT_XBGR32_VF:
			return "PIXEL_FORMAT_XBGR32_VF";

		case PIXEL_FORMAT_BGRA32:
			return "PIXEL_FORMAT_BGRA32";

		case PIXEL_FORMAT_BGRA32_VF:
			return "PIXEL_FORMAT_BGRA32_VF";

		case PIXEL_FORMAT_BGRX32:
			return "PIXEL_FORMAT_BGRX32";

		case PIXEL_FORMAT_BGRX32_VF:
			return "PIXEL_FORMAT_BGRX32_VF";

		case PIXEL_FORMAT_RGBA32:
			return "PIXEL_FORMAT_RGBA32";

		case PIXEL_FORMAT_RGBA32_VF:
			return "PIXEL_FORMAT_RGBA32_VF";

		case PIXEL_FORMAT_RGBX32:
			return "PIXEL_FORMAT_RGBX32";

		case PIXEL_FORMAT_RGBX32_VF:
			return "PIXEL_FORMAT_RGBX32_VF";

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			return "PIXEL_FORMAT_RGB24";

		case PIXEL_FORMAT_RGB24_VF:
			return "PIXEL_FORMAT_RGB24_VF";

		case PIXEL_FORMAT_BGR24:
			return "PIXEL_FORMAT_BGR24";

		case PIXEL_FORMAT_BGR24_VF:
			return "PIXEL_FORMAT_BGR24_VF";

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			return "PIXEL_FORMAT_RGB16";

		case PIXEL_FORMAT_RGB16_VF:
			return "PIXEL_FORMAT_RGB16_VF";

		case PIXEL_FORMAT_BGR16:
			return "PIXEL_FORMAT_BGR16";

		case PIXEL_FORMAT_BGR16_VF:
			return "PIXEL_FORMAT_BGR16_VF";

		case PIXEL_FORMAT_ARGB15:
			return "PIXEL_FORMAT_ARGB15";

		case PIXEL_FORMAT_ARGB15_VF:
			return "PIXEL_FORMAT_ARGB15_VF";

		case PIXEL_FORMAT_RGB15:
			return "PIXEL_FORMAT_RGB15";

		case PIXEL_FORMAT_RGB15_VF:
			return "PIXEL_FORMAT_ABGR15";

		case PIXEL_FORMAT_ABGR15:
			return "";

		case PIXEL_FORMAT_ABGR15_VF:
			return "PIXEL_FORMAT_ABGR15_VF";

		case PIXEL_FORMAT_BGR15:
			return "PIXEL_FORMAT_BGR15";

		case PIXEL_FORMAT_BGR15_VF:
			return "PIXEL_FORMAT_BGR15_VF";

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:
			return "PIXEL_FORMAT_RGB8";

		case PIXEL_FORMAT_RGB8_VF:
			return "PIXEL_FORMAT_RGB8_VF";

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:
			return "PIXEL_FORMAT_A4";

		case PIXEL_FORMAT_A4_VF:
			return "PIXEL_FORMAT_A4_VF";

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
			return "PIXEL_FORMAT_MONO";

		case PIXEL_FORMAT_MONO_VF:
			return "PIXEL_FORMAT_MONO_VF";

		default:
			return "UNKNOWN";
	}
}

static INLINE void SplitColor(UINT32 color, UINT32 format, BYTE* _r, BYTE* _g,
                              BYTE* _b, BYTE* _a, const UINT32* palette)
{
	UINT32 tmp;

	switch (FREERDP_PIXEL_FORMAT_FLIP_MASKED(format))
	{
		/* 32bpp formats */
		case PIXEL_FORMAT_ARGB32:
			if (_a)
				*_a = color >> 24;

			if (_r)
				*_r = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_b)
				*_b = color;

			break;

		case PIXEL_FORMAT_XRGB32:
			if (_r)
				*_r = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_b)
				*_b = color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_ABGR32:
			if (_a)
				*_a = color >> 24;

			if (_b)
				*_b = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_r)
				*_r = color;

			break;

		case PIXEL_FORMAT_XBGR32:
			if (_b)
				*_b = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_r)
				*_r = color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_RGBA32:
			if (_r)
				*_r = color >> 24;

			if (_g)
				*_g = color >> 16;

			if (_b)
				*_b = color >> 8;

			if (_a)
				*_a = color;

			break;

		case PIXEL_FORMAT_RGBX32:
			if (_b)
				*_b = color >> 24;

			if (_g)
				*_g = color >> 16;

			if (_r)
				*_r = color >> 8;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGRA32:
			if (_b)
				*_b = color >> 24;

			if (_g)
				*_g = color >> 16;

			if (_r)
				*_r = color >> 8;

			if (_a)
				*_a = color;

			break;

		case PIXEL_FORMAT_BGRX32:
			if (_b)
				*_b = color >> 24;

			if (_g)
				*_g = color >> 16;

			if (_r)
				*_r = color >> 8;

			if (_a)
				*_a = 0xFF;

			break;

		/* 24bpp formats */
		case PIXEL_FORMAT_RGB24:
			if (_r)
				*_r = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_b)
				*_b = color;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR24:
			if (_b)
				*_b = color >> 16;

			if (_g)
				*_g = color >> 8;

			if (_r)
				*_r = color;

			if (_a)
				*_a = 0xFF;

			break;

		/* 16bpp formats */
		case PIXEL_FORMAT_RGB16:
			if (_r)
				*_r = ((color >> 11) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x3F) << 2;

			if (_b)
				*_b = (color & 0x1F) << 3;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR16:
			if (_b)
				*_b = ((color >> 11) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x3F) << 2;

			if (_r)
				*_r = (color & 0x1F) << 3;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_ARGB15:
			if (_r)
				*_r = ((color >> 10) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x1F) << 3;

			if (_b)
				*_b = (color & 0x1F) << 3;

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		case PIXEL_FORMAT_ABGR15:
			if (_b)
				*_b = ((color >> 10) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x1F) << 3;

			if (_r)
				*_r = (color & 0x1F) << 3;

			if (_a)
				*_a = color & 0x8000 ? 0xFF : 0x00;

			break;

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			if (_r)
				*_r = ((color >> 10) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x1F) << 3;

			if (_b)
				*_b = (color & 0x1F) << 3;

			if (_a)
				*_a = 0xFF;

			break;

		case PIXEL_FORMAT_BGR15:
			if (_b)
				*_b = ((color >> 10) & 0x1F) << 3;

			if (_g)
				*_g = ((color >> 5) & 0x1F) << 3;

			if (_r)
				*_r = (color & 0x1F) << 3;

			if (_a)
				*_a = 0xFF;

			break;

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:
			tmp = palette[color];
			SplitColor(tmp, PIXEL_FORMAT_ARGB32, _r, _g, _b, _a, NULL);
			break;

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
		default:
			WLog_ERR("xxxxx", "Unsupported format %s", GetColorFormatName(format));
			break;
	}
}

static INLINE UINT32 GetColor(UINT32 format, BYTE r, BYTE g, BYTE b, BYTE a)
{
	UINT32 _r = r;
	UINT32 _g = g;
	UINT32 _b = b;
	UINT32 _a = a;

	switch (FREERDP_PIXEL_FORMAT_FLIP_MASKED(format))
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
			            _b >> 3) & 0x1F);

		case PIXEL_FORMAT_ABGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _r >> 3) & 0x1F);

		/* 15bpp formats */
		case PIXEL_FORMAT_RGB15:
			return (((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _b >> 3) & 0x1F);

		case PIXEL_FORMAT_BGR15:
			return (((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((
			            _r >> 3) & 0x1F);;

		/* 8bpp formats */
		case PIXEL_FORMAT_RGB8:

		/* 4 bpp formats */
		case PIXEL_FORMAT_A4:

		/* 1bpp formats */
		case PIXEL_FORMAT_MONO:
		default:
			WLog_ERR("xxxxx", "Unsupported format %s", GetColorFormatName(format));
			return 0;
	}
}

static INLINE UINT32 GetBitsPerPixel(UINT32 format)
{
	return (((format) >> 24) & 0x3F);
}

static INLINE BOOL ColorHasAlpha(UINT32 format)
{
	UINT32 alpha = (((format) >> 12) & 0x0F);

	if (alpha == 0)
		return FALSE;

	return TRUE;
}

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
		case 15:
			color = ((UINT32)src[1] << 8) | src[0];
			break;

		case 8:
			color = *src;
			break;

		default:
			WLog_ERR("xxxxx", "Unsupported format %s", GetColorFormatName(format));
			color = 0;
			break;
	}

	return color;
}

static INLINE BOOL WriteColor(BYTE* dst, UINT32 format, UINT32 color)
{
	switch (GetBitsPerPixel(format))
	{
		case 32:
			dst[0] = color >> 24;
			dst[1] = color >> 16;
			dst[2] = color >> 8;
			dst[3] = color;
			break;

		case 24:
			dst[0] = color >> 16;
			dst[1] = color >> 8;
			dst[2] = color;
			break;

		case 16:
		case 15:
			dst[1] = color >> 8;
			dst[0] = color;
			break;

		case 8:
			dst[0] = color;
			break;

		default:
			WLog_ERR("xxxxx", "Unsupported format %s", GetColorFormatName(format));
			return FALSE;
	}

	return TRUE;
}

static INLINE UINT32 ConvertColor(UINT32 color, UINT32 srcFormat,
                                  UINT32 dstFormat, const UINT32* palette)
{
	BYTE r = 0;
	BYTE g = 0;
	BYTE b = 0;
	BYTE a = 0;
	SplitColor(color, srcFormat, &r, &g, &b, &a, palette);
	return GetColor(dstFormat, r, g, b, a);
}

static INLINE UINT32 GetBytesPerPixel(UINT32 format)
{
	return (GetBitsPerPixel(format) + 7) / 8;
}

FREERDP_API BYTE* freerdp_glyph_convert(UINT32 width, UINT32 height,
                                        const BYTE* data);

FREERDP_API int freerdp_image_copy_from_monochrome(BYTE* pDstData,
        UINT32 DstFormat,
        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
        UINT32 nWidth, UINT32 nHeight,
        const BYTE* pSrcData,
        UINT32 backColor, UINT32 foreColor,
        const UINT32* palette);

FREERDP_API BOOL freerdp_image_copy_from_pointer_data(
    BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep,
    UINT32 nXDst, UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
    const BYTE* xorMask, UINT32 xorMaskLength,
    const BYTE* andMask, UINT32 andMaskLength,
    UINT32 xorBpp, const UINT32* palette);

FREERDP_API BOOL freerdp_image_copy(BYTE* pDstData, DWORD DstFormat,
                                    INT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                    UINT32 nWidth, UINT32 nHeight,
                                    const BYTE* pSrcData, DWORD SrcFormat,
                                    INT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
                                    const UINT32* palette);

FREERDP_API BOOL freerdp_image_fill(BYTE* pDstData, DWORD DstFormat,
                                    UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                    UINT32 nWidth, UINT32 nHeight, UINT32 color);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_COLOR_H */
