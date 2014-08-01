/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Color Conversion Routines
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#define FREERDP_PIXEL_FORMAT_TYPE_A		0
#define FREERDP_PIXEL_FORMAT_TYPE_ARGB		1
#define FREERDP_PIXEL_FORMAT_TYPE_ABGR		2
#define FREERDP_PIXEL_FORMAT_TYPE_BGRA		3
#define FREERDP_PIXEL_FORMAT_TYPE_RGBA		4

#define FREERDP_PIXEL_FLIP_NONE			0
#define FREERDP_PIXEL_FLIP_VERTICAL		1
#define FREERDP_PIXEL_FLIP_HORIZONTAL		2

#define FREERDP_PIXEL_FORMAT(_flip, _bpp, _type, _a, _r, _g, _b) \
	((_flip << 30) | (_bpp << 24) | (_type << 16) | (_a << 12) | (_r << 8) | (_g << 4) | (_b))

#define FREERDP_PIXEL_FORMAT_FLIP(_format)	(((_format) >> 30) & 0x03)
#define FREERDP_PIXEL_FORMAT_BPP(_format)	(((_format) >> 24) & 0x3F)
#define FREERDP_PIXEL_FORMAT_TYPE(_format)	(((_format) >> 16) & 0xFF)
#define FREERDP_PIXEL_FORMAT_A(_format)		(((_format) >> 12) & 0x0F)
#define FREERDP_PIXEL_FORMAT_R(_format)		(((_format) >>  8) & 0x0F)
#define FREERDP_PIXEL_FORMAT_G(_format)		(((_format) >>  4) & 0x0F)
#define FREERDP_PIXEL_FORMAT_B(_format)		(((_format)      ) & 0x0F)
#define FREERDP_PIXEL_FORMAT_RGB(_format)	(((_format)      ) & 0xFFF)
#define FREERDP_PIXEL_FORMAT_VIS(_format)	(((_format)      ) & 0xFFFF)

#define FREERDP_PIXEL_FORMAT_DEPTH(_format) \
	(FREERDP_PIXEL_FORMAT_A(_format) + \
	 FREERDP_PIXEL_FORMAT_R(_format) + \
	 FREERDP_PIXEL_FORMAT_G(_format) + \
	 FREERDP_PIXEL_FORMAT_B(_format))

/* 32bpp formats */

#define PIXEL_FORMAT_A8R8G8B8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 8, 8, 8, 8)
#define PIXEL_FORMAT_A8R8G8B8		PIXEL_FORMAT_A8R8G8B8_F(0)
#define PIXEL_FORMAT_ARGB32		PIXEL_FORMAT_A8R8G8B8
#define PIXEL_FORMAT_A8R8G8B8_VF	PIXEL_FORMAT_A8R8G8B8_F(1)
#define PIXEL_FORMAT_ARGB32_VF		PIXEL_FORMAT_A8R8G8B8_VF

#define PIXEL_FORMAT_X8R8G8B8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_X8R8G8B8		PIXEL_FORMAT_X8R8G8B8_F(0)
#define PIXEL_FORMAT_XRGB32		PIXEL_FORMAT_X8R8G8B8
#define PIXEL_FORMAT_RGB32		PIXEL_FORMAT_XRGB32
#define PIXEL_FORMAT_X8R8G8B8_VF	PIXEL_FORMAT_X8R8G8B8_F(1)
#define PIXEL_FORMAT_XRGB32_VF		PIXEL_FORMAT_X8R8G8B8_VF
#define PIXEL_FORMAT_RGB32_VF		PIXEL_FORMAT_XRGB32_VF

#define PIXEL_FORMAT_A8B8G8R8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 8, 8, 8, 8)
#define PIXEL_FORMAT_A8B8G8R8		PIXEL_FORMAT_A8B8G8R8_F(0)
#define PIXEL_FORMAT_ABGR32		PIXEL_FORMAT_A8B8G8R8
#define PIXEL_FORMAT_A8B8G8R8_VF	PIXEL_FORMAT_A8B8G8R8_F(1)
#define PIXEL_FORMAT_ABGR32_VF		PIXEL_FORMAT_A8B8G8R8_VF

#define PIXEL_FORMAT_X8B8G8R8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_X8B8G8R8		PIXEL_FORMAT_X8B8G8R8_F(0)
#define PIXEL_FORMAT_XBGR32		PIXEL_FORMAT_X8B8G8R8
#define PIXEL_FORMAT_BGR32		PIXEL_FORMAT_XBGR32
#define PIXEL_FORMAT_X8B8G8R8_VF	PIXEL_FORMAT_X8B8G8R8_F(1)
#define PIXEL_FORMAT_XBGR32_VF		PIXEL_FORMAT_X8B8G8R8_VF
#define PIXEL_FORMAT_BGR32_VF		PIXEL_FORMAT_XBGR32_VF

#define PIXEL_FORMAT_B8G8R8A8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 8, 8, 8, 8)
#define PIXEL_FORMAT_B8G8R8A8		PIXEL_FORMAT_B8G8R8A8_F(0)
#define PIXEL_FORMAT_BGRA32		PIXEL_FORMAT_B8G8R8A8
#define PIXEL_FORMAT_B8G8R8A8_VF	PIXEL_FORMAT_B8G8R8A8_F(1)
#define PIXEL_FORMAT_BGRA32_VF		PIXEL_FORMAT_B8G8R8A8_VF

#define PIXEL_FORMAT_B8G8R8X8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_BGRA, 0, 8, 8, 8)
#define PIXEL_FORMAT_B8G8R8X8		PIXEL_FORMAT_B8G8R8X8_F(0)
#define PIXEL_FORMAT_BGRX32		PIXEL_FORMAT_B8G8R8X8
#define PIXEL_FORMAT_B8G8R8X8_VF	PIXEL_FORMAT_B8G8R8X8_F(1)
#define PIXEL_FORMAT_BGRX32_VF		PIXEL_FORMAT_B8G8R8X8_VF

#define PIXEL_FORMAT_R8G8B8A8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 8, 8, 8, 8)
#define PIXEL_FORMAT_R8G8B8A8		PIXEL_FORMAT_R8G8B8A8_F(0)
#define PIXEL_FORMAT_RGBA32		PIXEL_FORMAT_R8G8B8A8
#define PIXEL_FORMAT_R8G8B8A8_VF	PIXEL_FORMAT_R8G8B8A8_F(1)
#define PIXEL_FORMAT_RGBA32_VF		PIXEL_FORMAT_R8G8B8A8_VF

#define PIXEL_FORMAT_R8G8B8X8_F(_flip)	FREERDP_PIXEL_FORMAT(_flip, 32, FREERDP_PIXEL_FORMAT_TYPE_RGBA, 0, 8, 8, 8)
#define PIXEL_FORMAT_R8G8B8X8		PIXEL_FORMAT_R8G8B8X8_F(0)
#define PIXEL_FORMAT_RGBX32		PIXEL_FORMAT_R8G8B8X8
#define PIXEL_FORMAT_R8G8B8X8_VF	PIXEL_FORMAT_R8G8B8X8_F(1)
#define PIXEL_FORMAT_RGBX32_VF		PIXEL_FORMAT_R8G8B8X8_VF

/* 24bpp formats */

#define PIXEL_FORMAT_R8G8B8		FREERDP_PIXEL_FORMAT(0, 24, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 8, 8, 8)
#define PIXEL_FORMAT_RGB24		PIXEL_FORMAT_R8G8B8

#define PIXEL_FORMAT_B8G8R8		FREERDP_PIXEL_FORMAT(0, 24, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 8, 8, 8)
#define PIXEL_FORMAT_BGR24		PIXEL_FORMAT_B8G8R8

/* 16bpp formats */

#define PIXEL_FORMAT_R5G6B5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 6, 5)
#define PIXEL_FORMAT_RGB565		PIXEL_FORMAT_R5G6B5
#define PIXEL_FORMAT_RGB16		PIXEL_FORMAT_R5G6B5

#define PIXEL_FORMAT_B5G6R5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 6, 5)
#define PIXEL_FORMAT_BGR565		PIXEL_FORMAT_B5G6R5
#define PIXEL_FORMAT_BGR16		PIXEL_FORMAT_B5G6R5

#define PIXEL_FORMAT_A1R5G5B5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 1, 5, 5, 5)
#define PIXEL_FORMAT_ARGB555		PIXEL_FORMAT_A1R5G5B5
#define PIXEL_FORMAT_ARGB15		PIXEL_FORMAT_A1R5G5B5

#define PIXEL_FORMAT_X1R5G5B5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ARGB, 0, 5, 5, 5)
#define PIXEL_FORMAT_XRGB555		PIXEL_FORMAT_X1R5G5B5
#define PIXEL_FORMAT_RGB555		PIXEL_FORMAT_X1R5G5B5
#define PIXEL_FORMAT_RGB15		PIXEL_FORMAT_X1R5G5B5

#define PIXEL_FORMAT_A1B5G5R5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 1, 5, 5, 5)
#define PIXEL_FORMAT_ABGR555		PIXEL_FORMAT_A1B5G5R5
#define PIXEL_FORMAT_ABGR15		PIXEL_FORMAT_A1B5G5R5

#define PIXEL_FORMAT_X1B5G5R5		FREERDP_PIXEL_FORMAT(0, 16, FREERDP_PIXEL_FORMAT_TYPE_ABGR, 0, 5, 5, 5)
#define PIXEL_FORMAT_XBGR555		PIXEL_FORMAT_X1B5G5R5
#define PIXEL_FORMAT_BGR555		PIXEL_FORMAT_X1B5G5R5
#define PIXEL_FORMAT_BGR15		PIXEL_FORMAT_X1B5G5R5

/* 8bpp formats */

#define PIXEL_FORMAT_A8			FREERDP_PIXEL_FORMAT(0, 8, FREERDP_PIXEL_FORMAT_TYPE_A, 8, 0, 0, 0)
#define PIXEL_FORMAT_8BPP		PIXEL_FORMAT_A8
#define PIXEL_FORMAT_256		PIXEL_FORMAT_A8

/* 4 bpp formats */

#define PIXEL_FORMAT_A4			FREERDP_PIXEL_FORMAT(0, 4, FREERDP_PIXEL_FORMAT_TYPE_A, 4, 0, 0, 0)
#define PIXEL_FORMAT_4BPP		PIXEL_FORMAT_A4

/* 1bpp formats */

#define PIXEL_FORMAT_A1			FREERDP_PIXEL_FORMAT(0, 1, FREERDP_PIXEL_FORMAT_TYPE_A, 1, 0, 0, 0)
#define PIXEL_FORMAT_1BPP		PIXEL_FORMAT_A1
#define PIXEL_FORMAT_MONO		PIXEL_FORMAT_A1

#ifdef __cplusplus
extern "C" {
#endif

/* Color Space Conversions: http://msdn.microsoft.com/en-us/library/ff566496/ */

/* Color Space Conversion */

#define RGB_555_565(_r, _g, _b) \
	_r = _r; \
	_g = (_g << 1 & ~0x1) | (_g >> 4); \
	_b = _b;

#define RGB_565_555(_r, _g, _b) \
	_r = _r; \
	_g = (_g >> 1); \
	_b = _b;

#define RGB_555_888(_r, _g, _b) \
	_r = (_r << 3 & ~0x7) | (_r >> 2); \
	_g = (_g << 3 & ~0x7) | (_g >> 2); \
	_b = (_b << 3 & ~0x7) | (_b >> 2);

#define RGB_565_888(_r, _g, _b) \
	_r = (_r << 3 & ~0x7) | (_r >> 2); \
	_g = (_g << 2 & ~0x3) | (_g >> 4); \
	_b = (_b << 3 & ~0x7) | (_b >> 2);

#define RGB_888_565(_r, _g, _b) \
	_r = (_r >> 3); \
	_g = (_g >> 2); \
	_b = (_b >> 3);

#define RGB_888_555(_r, _g, _b) \
	_r = (_r >> 3); \
	_g = (_g >> 3); \
	_b = (_b >> 3);

/* RGB 15 (RGB_555) */

#define RGB555(_r, _g, _b)  \
	((_r & 0x1F) << 10) | ((_g & 0x1F) << 5) | (_b & 0x1F)

#define RGB15(_r, _g, _b)  \
	(((_r >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_b >> 3) & 0x1F)

#define GetRGB_555(_r, _g, _b, _p) \
	_r = (_p & 0x7C00) >> 10; \
	_g = (_p & 0x3E0) >> 5; \
	_b = (_p & 0x1F);

#define GetRGB15(_r, _g, _b, _p) \
	GetRGB_555(_r, _g, _b, _p); \
	RGB_555_888(_r, _g, _b);

/* BGR 15 (BGR_555) */

#define BGR555(_r, _g, _b)  \
	((_b & 0x1F) << 10) | ((_g & 0x1F) << 5) | (_r & 0x1F)

#define BGR15(_r, _g, _b)  \
	(((_b >> 3) & 0x1F) << 10) | (((_g >> 3) & 0x1F) << 5) | ((_r >> 3) & 0x1F)

#define GetBGR_555(_r, _g, _b, _p) \
	_b = (_p & 0x7C00) >> 10; \
	_g = (_p & 0x3E0) >> 5; \
	_r = (_p & 0x1F);

#define GetBGR15(_r, _g, _b, _p) \
	GetBGR_555(_r, _g, _b, _p); \
	RGB_555_888(_r, _g, _b);

/* RGB 16 (RGB_565) */

#define RGB565(_r, _g, _b)  \
	((_r & 0x1F) << 11) | ((_g & 0x3F) << 5) | (_b & 0x1F)

#define RGB16(_r, _g, _b)  \
	(((_r >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((_b >> 3) & 0x1F)

#define GetRGB_565(_r, _g, _b, _p) \
	_r = (_p & 0xF800) >> 11; \
	_g = (_p & 0x7E0) >> 5; \
	_b = (_p & 0x1F);

#define GetRGB16(_r, _g, _b, _p) \
	GetRGB_565(_r, _g, _b, _p); \
	RGB_565_888(_r, _g, _b);

/* BGR 16 (BGR_565) */

#define BGR565(_r, _g, _b)  \
	((_b & 0x1F) << 11) | ((_g & 0x3F) << 5) | (_r & 0x1F)

#define BGR16(_r, _g, _b)  \
	(((_b >> 3) & 0x1F) << 11) | (((_g >> 2) & 0x3F) << 5) | ((_r >> 3) & 0x1F)

#define GetBGR_565(_r, _g, _b, _p) \
	_b = (_p & 0xF800) >> 11; \
	_g = (_p & 0x7E0) >> 5; \
	_r = (_p & 0x1F);

#define GetBGR16(_r, _g, _b, _p) \
	GetBGR_565(_r, _g, _b, _p); \
	RGB_565_888(_r, _g, _b);

/* RGB 24 (RGB_888) */

#define RGB24(_r, _g, _b)  \
	(_r << 16) | (_g << 8) | _b

#define GetRGB24(_r, _g, _b, _p) \
	_r = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_b = (_p & 0xFF);

/* BGR 24 (BGR_888) */

#define BGR24(_r, _g, _b)  \
	(_b << 16) | (_g << 8) | _r

#define GetBGR24(_r, _g, _b, _p) \
	_b = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_r = (_p & 0xFF);

/* RGB 32 (ARGB_8888), alpha ignored */

#define RGB32(_r, _g, _b)  \
	(_r << 16) | (_g << 8) | _b

#define GetRGB32(_r, _g, _b, _p) \
	_r = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_b = (_p & 0xFF);

/* ARGB 32 (ARGB_8888) */

#define ARGB32(_a,_r, _g, _b)  \
	(_a << 24) | (_r << 16) | (_g << 8) | _b

#define GetARGB32(_a, _r, _g, _b, _p) \
	_a = (_p & 0xFF000000) >> 24; \
	_r = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_b = (_p & 0xFF);

/* BGR 32 (ABGR_8888), alpha ignored */

#define BGR32(_r, _g, _b)  \
	(_b << 16) | (_g << 8) | _r

#define GetBGR32(_r, _g, _b, _p) \
	_b = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_r = (_p & 0xFF);

/* BGR 32 (ABGR_8888) */

#define ABGR32(_a, _r, _g, _b)  \
	(_a << 24) | (_b << 16) | (_g << 8) | _r

#define GetABGR32(_a, _r, _g, _b, _p) \
	_a = (_p & 0xFF000000) >> 24; \
	_b = (_p & 0xFF0000) >> 16; \
	_g = (_p & 0xFF00) >> 8; \
	_r = (_p & 0xFF);

/* Color Conversion */

#define BGR16_RGB32(_r, _g, _b, _p) \
	GetBGR16(_r, _g, _b, _p); \
 	RGB_565_888(_r, _g, _b); \
	_p = RGB32(_r, _g, _b);

#define RGB32_RGB16(_r, _g, _b, _p) \
	GetRGB32(_r, _g, _b, _p); \
 	RGB_888_565(_r, _g, _b); \
	_p = RGB565(_r, _g, _b);

#define RGB15_RGB16(_r, _g, _b, _p) \
	GetRGB_555(_r, _g, _b, _p); \
	_g = (_g << 1 & ~0x1) | (_g >> 4); \
	_p = RGB565(_r, _g, _b);

#define RGB16_RGB15(_r, _g, _b, _p) \
	GetRGB_565(_r, _g, _b, _p); \
	_g = (_g >> 1); \
	_p = RGB555(_r, _g, _b);

#define CLRCONV_ALPHA		1
#define CLRCONV_INVERT		2
/* if defined RGB555 format is used when rendering with a 16-bit frame buffer */
#define CLRCONV_RGB555		4

/* Supported Internal Buffer Formats */
#define CLRBUF_16BPP		8
#define	CLRBUF_24BPP		16
#define	CLRBUF_32BPP		32

struct _CLRCONV
{
	int alpha;
	int invert;
	int rgb555;
	rdpPalette* palette;
};
typedef struct _CLRCONV CLRCONV;
typedef CLRCONV* HCLRCONV;

#define IBPP(_bpp) (((_bpp + 1)/ 8) % 5)

typedef BYTE* (*p_freerdp_image_convert)(BYTE* srcData, BYTE* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv);

FREERDP_API int freerdp_get_pixel(BYTE* data, int x, int y, int width, int height, int bpp);
FREERDP_API void freerdp_set_pixel(BYTE* data, int x, int y, int width, int height, int bpp, int pixel);

FREERDP_API BYTE* freerdp_image_convert(BYTE* srcData, BYTE *dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API BYTE* freerdp_glyph_convert(int width, int height, BYTE* data);
FREERDP_API void   freerdp_bitmap_flip(BYTE * src, BYTE * dst, int scanLineSz, int height);
FREERDP_API BYTE* freerdp_image_flip(BYTE* srcData, BYTE* dstData, int width, int height, int bpp);
FREERDP_API BYTE* freerdp_icon_convert(BYTE* srcData, BYTE* dstData, BYTE* mask, int width, int height, int bpp, HCLRCONV clrconv);
FREERDP_API BYTE* freerdp_mono_image_convert(BYTE* srcData, int width, int height, int srcBpp, int dstBpp, UINT32 bgcolor, UINT32 fgcolor, HCLRCONV clrconv);
FREERDP_API void freerdp_alpha_cursor_convert(BYTE* alphaData, BYTE* xorMask, BYTE* andMask, int width, int height, int bpp, HCLRCONV clrconv);
FREERDP_API void freerdp_image_swap_color_order(BYTE* data, int width, int height);

FREERDP_API UINT32 freerdp_color_convert_var(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_rgb_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_bgr_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_var_rgb(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API UINT32 freerdp_color_convert_var_bgr(UINT32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);

FREERDP_API HCLRCONV freerdp_clrconv_new(UINT32 flags);
FREERDP_API void freerdp_clrconv_free(HCLRCONV clrconv);

FREERDP_API int freerdp_image_copy(BYTE* pDstData, DWORD dwDstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, BYTE* pSrcData, DWORD dwSrcFormat, int nSrcStep, int nXSrc, int nYSrc);
FREERDP_API int freerdp_image_fill(BYTE* pDstData, DWORD dwDstFormat, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, UINT32 color);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_COLOR_H */
