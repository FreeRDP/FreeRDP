/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __COLOR_H
#define __COLOR_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

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

typedef uint8* (*p_freerdp_image_convert)(uint8* srcData, uint8* dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv);

FREERDP_API uint8* freerdp_image_convert(uint8* srcData, uint8 *dstData, int width, int height, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint8* freerdp_glyph_convert(int width, int height, uint8* data);
FREERDP_API void   freerdp_bitmap_flip(uint8 * src, uint8 * dst, int scanLineSz, int height);
FREERDP_API uint8* freerdp_image_flip(uint8* srcData, uint8* dstData, int width, int height, int bpp);
FREERDP_API uint8* freerdp_icon_convert(uint8* srcData, uint8* dstData, uint8* mask, int width, int height, int bpp, HCLRCONV clrconv);
FREERDP_API uint8* freerdp_mono_image_convert(uint8* srcData, int width, int height, int srcBpp, int dstBpp, uint32 bgcolor, uint32 fgcolor, HCLRCONV clrconv);
FREERDP_API void freerdp_alpha_cursor_convert(uint8* alphaData, uint8* xorMask, uint8* andMask, int width, int height, int bpp, HCLRCONV clrconv);
FREERDP_API void freerdp_image_swap_color_order(uint8* data, int width, int height);

FREERDP_API uint32 freerdp_color_convert_var(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_rgb(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_bgr(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_rgb_bgr(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_bgr_rgb(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_var_rgb(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);
FREERDP_API uint32 freerdp_color_convert_var_bgr(uint32 srcColor, int srcBpp, int dstBpp, HCLRCONV clrconv);

FREERDP_API HCLRCONV freerdp_clrconv_new(uint32 flags);
FREERDP_API void freerdp_clrconv_free(HCLRCONV clrconv);

#ifdef __cplusplus
}
#endif

#endif /* __COLOR_H */
