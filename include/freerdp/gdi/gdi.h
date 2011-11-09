/**
 * FreeRDP: A Remote Desktop Protocol Client
 * GDI Library
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __GDI_H
#define __GDI_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/cache/cache.h>
#include <freerdp/utils/debug.h>
#include <freerdp/codec/color.h>

/* For more information, see [MS-RDPEGDI] */

/* Binary Raster Operations (ROP2) */
#define GDI_R2_BLACK			0x01  /* D = 0 */
#define GDI_R2_NOTMERGEPEN		0x02  /* D = ~(D | P) */
#define GDI_R2_MASKNOTPEN		0x03  /* D = D & ~P */
#define GDI_R2_NOTCOPYPEN		0x04  /* D = ~P */
#define GDI_R2_MASKPENNOT		0x05  /* D = P & ~D */
#define GDI_R2_NOT			0x06  /* D = ~D */
#define GDI_R2_XORPEN			0x07  /* D = D ^ P */
#define GDI_R2_NOTMASKPEN		0x08  /* D = ~(D & P) */
#define GDI_R2_MASKPEN			0x09  /* D = D & P */
#define GDI_R2_NOTXORPEN		0x0A  /* D = ~(D ^ P) */
#define GDI_R2_NOP			0x0B  /* D = D */
#define GDI_R2_MERGENOTPEN		0x0C  /* D = D | ~P */
#define GDI_R2_COPYPEN			0x0D  /* D = P */
#define GDI_R2_MERGEPENNOT		0x0E  /* D = P | ~D */
#define GDI_R2_MERGEPEN			0x0F  /* D = P | D */
#define GDI_R2_WHITE			0x10  /* D = 1 */

/* Ternary Raster Operations (ROP3) */
#define GDI_SRCCOPY			0x00CC0020 /* D = S */
#define GDI_SRCPAINT			0x00EE0086 /* D = S | D	*/
#define GDI_SRCAND			0x008800C6 /* D = S & D	*/
#define GDI_SRCINVERT			0x00660046 /* D = S ^ D	*/
#define GDI_SRCERASE			0x00440328 /* D = S & ~D */
#define GDI_NOTSRCCOPY			0x00330008 /* D = ~S */
#define GDI_NOTSRCERASE			0x001100A6 /* D = ~S & ~D */
#define GDI_MERGECOPY			0x00C000CA /* D = S & P	*/
#define GDI_MERGEPAINT			0x00BB0226 /* D = ~S | D */
#define GDI_PATCOPY			0x00F00021 /* D = P */
#define GDI_PATPAINT			0x00FB0A09 /* D = D | (P | ~S) */
#define GDI_PATINVERT			0x005A0049 /* D = P ^ D	*/
#define GDI_DSTINVERT			0x00550009 /* D = ~D */
#define GDI_BLACKNESS			0x00000042 /* D = 0 */
#define GDI_WHITENESS			0x00FF0062 /* D = 1 */
#define GDI_DSPDxax			0x00E20746 /* D = (S & P) | (~S & D) */
#define GDI_SPna			0x000C0324 /* D = S & ~P */
#define GDI_DSna			0x00220326 /* D = D & ~S */
#define GDI_PDxn			0x00A50065 /* D = D ^ ~P */

/* Brush Styles */
#define GDI_BS_SOLID			0x00
#define GDI_BS_NULL			0x01
#define GDI_BS_HATCHED			0x02
#define GDI_BS_PATTERN			0x03

/* Hatch Patterns */
#define GDI_HS_HORIZONTAL		0x00
#define GDI_HS_VERTICAL			0x01
#define GDI_HS_FDIAGONAL		0x02
#define GDI_HS_BDIAGONAL		0x03
#define GDI_HS_CROSS			0x04
#define GDI_HS_DIAGCROSS		0x05

/* Pen Styles */
#define GDI_PS_SOLID			0x00
#define GDI_PS_DASH			0x01
#define GDI_PS_NULL			0x05

/* Background Modes */
#define GDI_OPAQUE			0x00000001
#define GDI_TRANSPARENT			0x00000002

/* GDI Object Types */
#define GDIOBJECT_BITMAP		0x00
#define GDIOBJECT_PEN			0x01
#define GDIOBJECT_PALETTE		0x02
#define GDIOBJECT_BRUSH			0x03
#define GDIOBJECT_RECT			0x04
#define GDIOBJECT_REGION		0x04

struct _GDIOBJECT
{
	uint8 objectType;
};
typedef struct _GDIOBJECT GDIOBJECT;
typedef GDIOBJECT* HGDIOBJECT;

/* RGB encoded as 0x00BBGGRR */
typedef unsigned int GDI_COLOR;
typedef GDI_COLOR* LPGDI_COLOR;

struct _GDI_RECT
{
	uint8 objectType;
	int left;
	int top;
	int right;
	int bottom;
};
typedef struct _GDI_RECT GDI_RECT;
typedef GDI_RECT* HGDI_RECT;

struct _GDI_RGN
{
	uint8 objectType;
	int x; /* left */
	int y; /* top */
	int w; /* width */
	int h; /* height */
	int null; /* null region */
};
typedef struct _GDI_RGN GDI_RGN;
typedef GDI_RGN* HGDI_RGN;

struct _GDI_BITMAP
{
	uint8 objectType;
	int bytesPerPixel;
	int bitsPerPixel;
	int width;
	int height;
	int scanline;
	uint8* data;
};
typedef struct _GDI_BITMAP GDI_BITMAP;
typedef GDI_BITMAP* HGDI_BITMAP;

struct _GDI_PEN
{
	uint8 objectType;
	int style;
	int width;
	int posX;
	int posY;
	GDI_COLOR color;
};
typedef struct _GDI_PEN GDI_PEN;
typedef GDI_PEN* HGDI_PEN;

struct _GDI_PALETTEENTRY
{
	uint8 red;
	uint8 green;
	uint8 blue;
};
typedef struct _GDI_PALETTEENTRY GDI_PALETTEENTRY;

struct _GDI_PALETTE
{
	uint16 count;
	GDI_PALETTEENTRY *entries;
};
typedef struct _GDI_PALETTE GDI_PALETTE;
typedef GDI_PALETTE* HGDI_PALETTE;

struct _GDI_POINT
{
	int x;
	int y;
};
typedef struct _GDI_POINT GDI_POINT;
typedef GDI_POINT* HGDI_POINT;

struct _GDI_BRUSH
{
	uint8 objectType;
	int style;
	HGDI_BITMAP pattern;
	GDI_COLOR color;
};
typedef struct _GDI_BRUSH GDI_BRUSH;
typedef GDI_BRUSH* HGDI_BRUSH;

struct _GDI_WND
{
	int count;
	int ninvalid;
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;
};
typedef struct _GDI_WND GDI_WND;
typedef GDI_WND* HGDI_WND;

struct _GDI_DC
{
	HGDIOBJECT selectedObject;
	int bytesPerPixel;
	int bitsPerPixel;
	GDI_COLOR bkColor;
	GDI_COLOR textColor;
	HGDI_BRUSH brush;
	HGDI_RGN clip;
	HGDI_PEN pen;
	HGDI_WND hwnd;
	int drawMode;
	int bkMode;
	int alpha;
	int invert;
	int rgb555;
};
typedef struct _GDI_DC GDI_DC;
typedef GDI_DC* HGDI_DC;

struct gdi_bitmap
{
	rdpBitmap _p;

	HGDI_DC hdc;
	HGDI_BITMAP bitmap;
	HGDI_BITMAP org_bitmap;
};
typedef struct gdi_bitmap gdiBitmap;

struct gdi_glyph
{
	rdpBitmap _p;

	HGDI_DC hdc;
	HGDI_BITMAP bitmap;
	HGDI_BITMAP org_bitmap;
};
typedef struct gdi_glyph gdiGlyph;

struct rdp_gdi
{
	rdpContext* context;

	int width;
	int height;
	int dstBpp;
	int srcBpp;
	int cursor_x;
	int cursor_y;
	int bytesPerPixel;

	HGDI_DC hdc;
	HCLRCONV clrconv;
	gdiBitmap* primary;
	gdiBitmap* drawing;
	uint8* primary_buffer;
	GDI_COLOR textColor;
	void* rfx_context;
	void* nsc_context;
	gdiBitmap* tile;
	gdiBitmap* image;
};

FREERDP_API uint32 gdi_rop3_code(uint8 code);
FREERDP_API uint8* gdi_get_bitmap_pointer(HGDI_DC hdcBmp, int x, int y);
FREERDP_API uint8* gdi_get_brush_pointer(HGDI_DC hdcBrush, int x, int y);
FREERDP_API int gdi_is_mono_pixel_set(uint8* data, int x, int y, int width);
FREERDP_API void gdi_resize(rdpGdi* gdi, int width, int height);

FREERDP_API int gdi_init(freerdp* instance, uint32 flags, uint8* buffer);
FREERDP_API void gdi_free(freerdp* instance);

#ifdef WITH_DEBUG_GDI
#define DEBUG_GDI(fmt, ...) DEBUG_CLASS(GDI, fmt, ## __VA_ARGS__)
#else
#define DEBUG_GDI(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __GDI_H */
