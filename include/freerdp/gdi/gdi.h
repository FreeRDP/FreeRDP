/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Library
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_GDI_H
#define FREERDP_GDI_H

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/cache/cache.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>

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
#define GDI_BLACKNESS		0x00000042
#define GDI_DPSoon			0x00010289
#define GDI_DPSona			0x00020C89
#define GDI_PSon			0x000300AA
#define GDI_SDPona			0x00040C88
#define GDI_DPon			0x000500A9
#define GDI_PDSxnon			0x00060865
#define GDI_PDSaon			0x000702C5
#define GDI_SDPnaa			0x00080F08
#define GDI_PDSxon			0x00090245
#define GDI_DPna			0x000A0329
#define GDI_PSDnaon			0x000B0B2A
#define GDI_SPna			0x000C0324
#define GDI_PDSnaon			0x000D0B25
#define GDI_PDSonon			0x000E08A5
#define GDI_Pn				0x000F0001
#define GDI_PDSona			0x00100C85
#define GDI_NOTSRCERASE		0x001100A6
#define GDI_SDPxnon			0x00120868
#define GDI_SDPaon			0x001302C8
#define GDI_DPSxnon			0x00140869
#define GDI_DPSaon			0x001502C9
#define GDI_PSDPSanaxx		0x00165CCA
#define GDI_SSPxDSxaxn		0x00171D54
#define GDI_SPxPDxa			0x00180D59
#define GDI_SDPSanaxn		0x00191CC8
#define GDI_PDSPaox			0x001A06C5
#define GDI_SDPSxaxn		0x001B0768
#define GDI_PSDPaox			0x001C06CA
#define GDI_DSPDxaxn		0x001D0766
#define GDI_PDSox			0x001E01A5
#define GDI_PDSoan			0x001F0385
#define GDI_DPSnaa			0x00200F09
#define GDI_SDPxon			0x00210248
#define GDI_DSna			0x00220326
#define GDI_SPDnaon			0x00230B24
#define GDI_SPxDSxa			0x00240D55
#define GDI_PDSPanaxn		0x00251CC5
#define GDI_SDPSaox			0x002606C8
#define GDI_SDPSxnox		0x00271868
#define GDI_DPSxa			0x00280369
#define GDI_PSDPSaoxxn		0x002916CA
#define GDI_DPSana			0x002A0CC9
#define GDI_SSPxPDxaxn		0x002B1D58
#define GDI_SPDSoax			0x002C0784
#define GDI_PSDnox			0x002D060A
#define GDI_PSDPxox			0x002E064A
#define GDI_PSDnoan			0x002F0E2A
#define GDI_PSna			0x0030032A
#define GDI_SDPnaon			0x00310B28
#define GDI_SDPSoox			0x00320688
#define GDI_NOTSRCCOPY		0x00330008
#define GDI_SPDSaox			0x003406C4
#define GDI_SPDSxnox		0x00351864
#define GDI_SDPox			0x003601A8
#define GDI_SDPoan			0x00370388
#define GDI_PSDPoax			0x0038078A
#define GDI_SPDnox			0x00390604
#define GDI_SPDSxox			0x003A0644
#define GDI_SPDnoan			0x003B0E24
#define GDI_PSx			0x003C004A
#define GDI_SPDSonox		0x003D18A4
#define GDI_SPDSnaox		0x003E1B24
#define GDI_PSan			0x003F00EA
#define GDI_PSDnaa			0x00400F0A
#define GDI_DPSxon			0x00410249
#define GDI_SDxPDxa			0x00420D5D
#define GDI_SPDSanaxn		0x00431CC4
#define GDI_SRCERASE		0x00440328
#define GDI_DPSnaon			0x00450B29
#define GDI_DSPDaox			0x004606C6
#define GDI_PSDPxaxn		0x0047076A
#define GDI_SDPxa			0x00480368
#define GDI_PDSPDaoxxn		0x004916C5
#define GDI_DPSDoax			0x004A0789
#define GDI_PDSnox			0x004B0605
#define GDI_SDPana			0x004C0CC8
#define GDI_SSPxDSxoxn		0x004D1954
#define GDI_PDSPxox			0x004E0645
#define GDI_PDSnoan			0x004F0E25
#define GDI_PDna			0x00500325
#define GDI_DSPnaon			0x00510B26
#define GDI_DPSDaox			0x005206C9
#define GDI_SPDSxaxn		0x00530764
#define GDI_DPSonon			0x005408A9
#define GDI_DSTINVERT		0x00550009
#define GDI_DPSox			0x005601A9
#define GDI_DPSoan			0x00570389
#define GDI_PDSPoax			0x00580785
#define GDI_DPSnox			0x00590609
#define GDI_PATINVERT		0x005A0049
#define GDI_DPSDonox		0x005B18A9
#define GDI_DPSDxox			0x005C0649
#define GDI_DPSnoan			0x005D0E29
#define GDI_DPSDnaox		0x005E1B29
#define GDI_DPan			0x005F00E9
#define GDI_PDSxa			0x00600365
#define GDI_DSPDSaoxxn		0x006116C6
#define GDI_DSPDoax			0x00620786
#define GDI_SDPnox			0x00630608
#define GDI_SDPSoax			0x00640788
#define GDI_DSPnox			0x00650606
#define GDI_SRCINVERT		0x00660046
#define GDI_SDPSonox		0x006718A8
#define GDI_DSPDSonoxxn		0x006858A6
#define GDI_PDSxxn			0x00690145
#define GDI_DPSax			0x006A01E9
#define GDI_PSDPSoaxxn		0x006B178A
#define GDI_SDPax			0x006C01E8
#define GDI_PDSPDoaxxn		0x006D1785
#define GDI_SDPSnoax		0x006E1E28
#define GDI_PDSxnan			0x006F0C65
#define GDI_PDSana			0x00700CC5
#define GDI_SSDxPDxaxn		0x00711D5C
#define GDI_SDPSxox			0x00720648
#define GDI_SDPnoan			0x00730E28
#define GDI_DSPDxox			0x00740646
#define GDI_DSPnoan			0x00750E26
#define GDI_SDPSnaox		0x00761B28
#define GDI_DSan			0x007700E6
#define GDI_PDSax			0x007801E5
#define GDI_DSPDSoaxxn		0x00791786
#define GDI_DPSDnoax		0x007A1E29
#define GDI_SDPxnan			0x007B0C68
#define GDI_SPDSnoax		0x007C1E24
#define GDI_DPSxnan			0x007D0C69
#define GDI_SPxDSxo			0x007E0955
#define GDI_DPSaan			0x007F03C9
#define GDI_DPSaa			0x008003E9
#define GDI_SPxDSxon		0x00810975
#define GDI_DPSxna			0x00820C49
#define GDI_SPDSnoaxn		0x00831E04
#define GDI_SDPxna			0x00840C48
#define GDI_PDSPnoaxn		0x00851E05
#define GDI_DSPDSoaxx		0x008617A6
#define GDI_PDSaxn			0x008701C5
#define GDI_SRCAND			0x008800C6
#define GDI_SDPSnaoxn		0x00891B08
#define GDI_DSPnoa			0x008A0E06
#define GDI_DSPDxoxn		0x008B0666
#define GDI_SDPnoa			0x008C0E08
#define GDI_SDPSxoxn		0x008D0668
#define GDI_SSDxPDxax		0x008E1D7C
#define GDI_PDSanan			0x008F0CE5
#define GDI_PDSxna			0x00900C45
#define GDI_SDPSnoaxn		0x00911E08
#define GDI_DPSDPoaxx		0x009217A9
#define GDI_SPDaxn			0x009301C4
#define GDI_PSDPSoaxx		0x009417AA
#define GDI_DPSaxn			0x009501C9
#define GDI_DPSxx			0x00960169
#define GDI_PSDPSonoxx		0x0097588A
#define GDI_SDPSonoxn		0x00981888
#define GDI_DSxn			0x00990066
#define GDI_DPSnax			0x009A0709
#define GDI_SDPSoaxn		0x009B07A8
#define GDI_SPDnax			0x009C0704
#define GDI_DSPDoaxn		0x009D07A6
#define GDI_DSPDSaoxx		0x009E16E6
#define GDI_PDSxan			0x009F0345
#define GDI_DPa			0x00A000C9
#define GDI_PDSPnaoxn		0x00A11B05
#define GDI_DPSnoa			0x00A20E09
#define GDI_DPSDxoxn		0x00A30669
#define GDI_PDSPonoxn		0x00A41885
#define GDI_PDxn			0x00A50065
#define GDI_DSPnax			0x00A60706
#define GDI_PDSPoaxn		0x00A707A5
#define GDI_DPSoa			0x00A803A9
#define GDI_DPSoxn			0x00A90189
#define GDI_DSTCOPY			0x00AA0029
#define GDI_DPSono			0x00AB0889
#define GDI_SPDSxax			0x00AC0744
#define GDI_DPSDaoxn		0x00AD06E9
#define GDI_DSPnao			0x00AE0B06
#define GDI_DPno			0x00AF0229
#define GDI_PDSnoa			0x00B00E05
#define GDI_PDSPxoxn		0x00B10665
#define GDI_SSPxDSxox		0x00B21974
#define GDI_SDPanan			0x00B30CE8
#define GDI_PSDnax			0x00B4070A
#define GDI_DPSDoaxn		0x00B507A9
#define GDI_DPSDPaoxx		0x00B616E9
#define GDI_SDPxan			0x00B70348
#define GDI_PSDPxax			0x00B8074A
#define GDI_DSPDaoxn		0x00B906E6
#define GDI_DPSnao			0x00BA0B09
#define GDI_MERGEPAINT		0x00BB0226
#define GDI_SPDSanax		0x00BC1CE4
#define GDI_SDxPDxan		0x00BD0D7D
#define GDI_DPSxo			0x00BE0269
#define GDI_DPSano			0x00BF08C9
#define GDI_MERGECOPY		0x00C000CA
#define GDI_SPDSnaoxn		0x00C11B04
#define GDI_SPDSonoxn		0x00C21884
#define GDI_PSxn			0x00C3006A
#define GDI_SPDnoa			0x00C40E04
#define GDI_SPDSxoxn		0x00C50664
#define GDI_SDPnax			0x00C60708
#define GDI_PSDPoaxn		0x00C707AA
#define GDI_SDPoa			0x00C803A8
#define GDI_SPDoxn			0x00C90184
#define GDI_DPSDxax			0x00CA0749
#define GDI_SPDSaoxn		0x00CB06E4
#define GDI_SRCCOPY			0x00CC0020
#define GDI_SDPono			0x00CD0888
#define GDI_SDPnao			0x00CE0B08
#define GDI_SPno			0x00CF0224
#define GDI_PSDnoa			0x00D00E0A
#define GDI_PSDPxoxn		0x00D1066A
#define GDI_PDSnax			0x00D20705
#define GDI_SPDSoaxn		0x00D307A4
#define GDI_SSPxPDxax		0x00D41D78
#define GDI_DPSanan			0x00D50CE9
#define GDI_PSDPSaoxx		0x00D616EA
#define GDI_DPSxan			0x00D70349
#define GDI_PDSPxax			0x00D80745
#define GDI_SDPSaoxn		0x00D906E8
#define GDI_DPSDanax		0x00DA1CE9
#define GDI_SPxDSxan		0x00DB0D75
#define GDI_SPDnao			0x00DC0B04
#define GDI_SDno			0x00DD0228
#define GDI_SDPxo			0x00DE0268
#define GDI_SDPano			0x00DF08C8
#define GDI_PDSoa			0x00E003A5
#define GDI_PDSoxn			0x00E10185
#define GDI_DSPDxax			0x00E20746
#define GDI_PSDPaoxn		0x00E306EA
#define GDI_SDPSxax			0x00E40748
#define GDI_PDSPaoxn		0x00E506E5
#define GDI_SDPSanax		0x00E61CE8
#define GDI_SPxPDxan		0x00E70D79
#define GDI_SSPxDSxax		0x00E81D74
#define GDI_DSPDSanaxxn		0x00E95CE6
#define GDI_DPSao			0x00EA02E9
#define GDI_DPSxno			0x00EB0849
#define GDI_SDPao			0x00EC02E8
#define GDI_SDPxno			0x00ED0848
#define GDI_SRCPAINT		0x00EE0086
#define GDI_SDPnoo			0x00EF0A08
#define GDI_PATCOPY			0x00F00021
#define GDI_PDSono			0x00F10885
#define GDI_PDSnao			0x00F20B05
#define GDI_PSno			0x00F3022A
#define GDI_PSDnao			0x00F40B0A
#define GDI_PDno			0x00F50225
#define GDI_PDSxo			0x00F60265
#define GDI_PDSano			0x00F708C5
#define GDI_PDSao			0x00F802E5
#define GDI_PDSxno			0x00F90845
#define GDI_DPo				0x00FA0089
#define GDI_PATPAINT		0x00FB0A09
#define GDI_PSo				0x00FC008A
#define GDI_PSDnoo			0x00FD0A0A
#define GDI_DPSoo			0x00FE02A9
#define GDI_WHITENESS		0x00FF0062
#define GDI_GLYPH_ORDER		0xFFFFFFFF


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

/* Fill Modes */
#define GDI_FILL_ALTERNATE		0x01
#define GDI_FILL_WINDING		0x02

/* GDI Object Types */
#define GDIOBJECT_BITMAP   0x00
#define GDIOBJECT_PEN      0x01
#define GDIOBJECT_PALETTE  0x02
#define GDIOBJECT_BRUSH    0x03
#define GDIOBJECT_RECT     0x04
#define GDIOBJECT_REGION   0x05

/* Region return values */
#ifndef NULLREGION
#define NULLREGION         0x01
#define SIMPLEREGION       0x02
#define COMPLEXREGION      0x03
#endif

struct _GDIOBJECT
{
	BYTE objectType;
};
typedef struct _GDIOBJECT GDIOBJECT;
typedef GDIOBJECT* HGDIOBJECT;

struct _GDI_RECT
{
	BYTE objectType;
	UINT32 left;
	UINT32 top;
	UINT32 right;
	UINT32 bottom;
};
typedef struct _GDI_RECT GDI_RECT;
typedef GDI_RECT* HGDI_RECT;

struct _GDI_RGN
{
	BYTE objectType;
	UINT32 x; /* left */
	UINT32 y; /* top */
	UINT32 w; /* width */
	UINT32 h; /* height */
	BOOL null; /* null region */
};
typedef struct _GDI_RGN GDI_RGN;
typedef GDI_RGN* HGDI_RGN;

struct _GDI_BITMAP
{
	BYTE objectType;
	UINT32 format;
	UINT32 width;
	UINT32 height;
	UINT32 scanline;
	BYTE* data;
	void (*free)(void*);
};
typedef struct _GDI_BITMAP GDI_BITMAP;
typedef GDI_BITMAP* HGDI_BITMAP;

struct _GDI_PEN
{
	BYTE objectType;
	UINT32 style;
	UINT32 width;
	UINT32 posX;
	UINT32 posY;
	UINT32 color;
	UINT32 format;
	const gdiPalette* palette;
};
typedef struct _GDI_PEN GDI_PEN;
typedef GDI_PEN* HGDI_PEN;

struct _GDI_PALETTEENTRY
{
	BYTE red;
	BYTE green;
	BYTE blue;
};
typedef struct _GDI_PALETTEENTRY GDI_PALETTEENTRY;

struct _GDI_PALETTE
{
	UINT16 count;
	GDI_PALETTEENTRY* entries;
};
typedef struct _GDI_PALETTE GDI_PALETTE;
typedef GDI_PALETTE* HGDI_PALETTE;

struct _GDI_POINT
{
	UINT32 x;
	UINT32 y;
};
typedef struct _GDI_POINT GDI_POINT;
typedef GDI_POINT* HGDI_POINT;

struct _GDI_BRUSH
{
	BYTE objectType;
	int style;
	HGDI_BITMAP pattern;
	UINT32 color;
	UINT32 nXOrg;
	UINT32 nYOrg;
};
typedef struct _GDI_BRUSH GDI_BRUSH;
typedef GDI_BRUSH* HGDI_BRUSH;

struct _GDI_WND
{
	UINT32 count;
	INT32 ninvalid;
	HGDI_RGN invalid;
	HGDI_RGN cinvalid;
};
typedef struct _GDI_WND GDI_WND;
typedef GDI_WND* HGDI_WND;

struct _GDI_DC
{
	HGDIOBJECT selectedObject;
	UINT32 format;
	UINT32 bkColor;
	UINT32 textColor;
	HGDI_BRUSH brush;
	HGDI_RGN clip;
	HGDI_PEN pen;
	HGDI_WND hwnd;
	UINT32 drawMode;
	UINT32 bkMode;
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

	UINT32 width;
	UINT32 height;
	UINT32 stride;
	UINT32 dstFormat;
	UINT32 cursor_x;
	UINT32 cursor_y;

	HGDI_DC hdc;
	gdiBitmap* primary;
	gdiBitmap* drawing;
	UINT32 bitmap_size;
	UINT32 bitmap_stride;
	BYTE* primary_buffer;
	gdiPalette palette;
	gdiBitmap* image;
	void (*free)(void*);

	BOOL inGfxFrame;
	BOOL graphicsReset;
	BOOL suppressOutput;
	UINT16 outputSurfaceId;
	RdpgfxClientContext* gfx;
	VideoClientContext* video;
	GeometryClientContext* geometry;

	wLog* log;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API DWORD gdi_rop3_code(BYTE code);
FREERDP_API const char* gdi_rop3_code_string(BYTE code);
FREERDP_API const char* gdi_rop3_string(DWORD rop);

FREERDP_API UINT32 gdi_get_pixel_format(UINT32 bitsPerPixel);
FREERDP_API BOOL gdi_decode_color(rdpGdi* gdi, const UINT32 srcColor,
                                  UINT32* color, UINT32* format);
FREERDP_API BOOL gdi_resize(rdpGdi* gdi, UINT32 width, UINT32 height);
FREERDP_API BOOL gdi_resize_ex(rdpGdi* gdi, UINT32 width, UINT32 height,
                               UINT32 stride, UINT32 format, BYTE* buffer,
                               void (*pfree)(void*));
FREERDP_API BOOL gdi_init(freerdp* instance, UINT32 format);
FREERDP_API BOOL gdi_init_ex(freerdp* instance, UINT32 format,
                             UINT32 stride, BYTE* buffer,
                             void (*pfree)(void*));
FREERDP_API void gdi_free(freerdp* instance);

FREERDP_API BOOL gdi_send_suppress_output(rdpGdi* gdi, BOOL suppress);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_GDI_H */
