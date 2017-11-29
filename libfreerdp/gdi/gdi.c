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
 *	 http://www.apache.org/licenses/LICENSE-2.0
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
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <freerdp/freerdp.h>

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/pen.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include "drawing.h"
#include "clipping.h"
#include "brush.h"
#include "line.h"
#include "gdi.h"
#include "../core/graphics.h"

#define TAG FREERDP_TAG("gdi")

/* Ternary Raster Operation Table */
static const DWORD rop3_code_table[] =
{
	0x00000042, /* 0 */
	0x00010289, /* DPSoon */
	0x00020C89, /* DPSona */
	0x000300AA, /* PSon */
	0x00040C88, /* SDPona */
	0x000500A9, /* DPon */
	0x00060865, /* PDSxnon */
	0x000702C5, /* PDSaon */
	0x00080F08, /* SDPnaa */
	0x00090245, /* PDSxon */
	0x000A0329, /* DPna */
	0x000B0B2A, /* PSDnaon */
	0x000C0324, /* SPna */
	0x000D0B25, /* PDSnaon */
	0x000E08A5, /* PDSonon */
	0x000F0001, /* Pn */
	0x00100C85, /* PDSona */
	0x001100A6, /* DSon */
	0x00120868, /* SDPxnon */
	0x001302C8, /* SDPaon */
	0x00140869, /* DPSxnon */
	0x001502C9, /* DPSaon */
	0x00165CCA, /* PSDPSanaxx */
	0x00171D54, /* SSPxDSxaxn */
	0x00180D59, /* SPxPDxa */
	0x00191CC8, /* SDPSanaxn */
	0x001A06C5, /* PDSPaox */
	0x001B0768, /* SDPSxaxn */
	0x001C06CA, /* PSDPaox */
	0x001D0766, /* DSPDxaxn */
	0x001E01A5, /* PDSox */
	0x001F0385, /* PDSoan */
	0x00200F09, /* DPSnaa */
	0x00210248, /* SDPxon */
	0x00220326, /* DSna */
	0x00230B24, /* SPDnaon */
	0x00240D55, /* SPxDSxa */
	0x00251CC5, /* PDSPanaxn */
	0x002606C8, /* SDPSaox */
	0x00271868, /* SDPSxnox */
	0x00280369, /* DPSxa */
	0x002916CA, /* PSDPSaoxxn */
	0x002A0CC9, /* DPSana */
	0x002B1D58, /* SSPxPDxaxn */
	0x002C0784, /* SPDSoax */
	0x002D060A, /* PSDnox */
	0x002E064A, /* PSDPxox */
	0x002F0E2A, /* PSDnoan */
	0x0030032A, /* PSna */
	0x00310B28, /* SDPnaon */
	0x00320688, /* SDPSoox */
	0x00330008, /* Sn */
	0x003406C4, /* SPDSaox */
	0x00351864, /* SPDSxnox */
	0x003601A8, /* SDPox */
	0x00370388, /* SDPoan */
	0x0038078A, /* PSDPoax */
	0x00390604, /* SPDnox */
	0x003A0644, /* SPDSxox */
	0x003B0E24, /* SPDnoan */
	0x003C004A, /* PSx */
	0x003D18A4, /* SPDSonox */
	0x003E1B24, /* SPDSnaox */
	0x003F00EA, /* PSan */
	0x00400F0A, /* PSDnaa */
	0x00410249, /* DPSxon */
	0x00420D5D, /* SDxPDxa */
	0x00431CC4, /* SPDSanaxn */
	0x00440328, /* SDna */
	0x00450B29, /* DPSnaon */
	0x004606C6, /* DSPDaox */
	0x0047076A, /* PSDPxaxn */
	0x00480368, /* SDPxa */
	0x004916C5, /* PDSPDaoxxn */
	0x004A0789, /* DPSDoax */
	0x004B0605, /* PDSnox */
	0x004C0CC8, /* SDPana */
	0x004D1954, /* SSPxDSxoxn */
	0x004E0645, /* PDSPxox */
	0x004F0E25, /* PDSnoan */
	0x00500325, /* PDna */
	0x00510B26, /* DSPnaon */
	0x005206C9, /* DPSDaox */
	0x00530764, /* SPDSxaxn */
	0x005408A9, /* DPSonon */
	0x00550009, /* Dn */
	0x005601A9, /* DPSox */
	0x00570389, /* DPSoan */
	0x00580785, /* PDSPoax */
	0x00590609, /* DPSnox */
	0x005A0049, /* DPx */
	0x005B18A9, /* DPSDonox */
	0x005C0649, /* DPSDxox */
	0x005D0E29, /* DPSnoan */
	0x005E1B29, /* DPSDnaox */
	0x005F00E9, /* DPan */
	0x00600365, /* PDSxa */
	0x006116C6, /* DSPDSaoxxn */
	0x00620786, /* DSPDoax */
	0x00630608, /* SDPnox */
	0x00640788, /* SDPSoax */
	0x00650606, /* DSPnox */
	0x00660046, /* DSx */
	0x006718A8, /* SDPSonox */
	0x006858A6, /* DSPDSonoxxn */
	0x00690145, /* PDSxxn */
	0x006A01E9, /* DPSax */
	0x006B178A, /* PSDPSoaxxn */
	0x006C01E8, /* SDPax */
	0x006D1785, /* PDSPDoaxxn */
	0x006E1E28, /* SDPSnoax */
	0x006F0C65, /* PDSxnan */
	0x00700CC5, /* PDSana */
	0x00711D5C, /* SSDxPDxaxn */
	0x00720648, /* SDPSxox */
	0x00730E28, /* SDPnoan */
	0x00740646, /* DSPDxox */
	0x00750E26, /* DSPnoan */
	0x00761B28, /* SDPSnaox */
	0x007700E6, /* DSan */
	0x007801E5, /* PDSax */
	0x00791786, /* DSPDSoaxxn */
	0x007A1E29, /* DPSDnoax */
	0x007B0C68, /* SDPxnan */
	0x007C1E24, /* SPDSnoax */
	0x007D0C69, /* DPSxnan */
	0x007E0955, /* SPxDSxo */
	0x007F03C9, /* DPSaan */
	0x008003E9, /* DPSaa */
	0x00810975, /* SPxDSxon */
	0x00820C49, /* DPSxna */
	0x00831E04, /* SPDSnoaxn */
	0x00840C48, /* SDPxna */
	0x00851E05, /* PDSPnoaxn */
	0x008617A6, /* DSPDSoaxx */
	0x008701C5, /* PDSaxn */
	0x008800C6, /* DSa */
	0x00891B08, /* SDPSnaoxn */
	0x008A0E06, /* DSPnoa */
	0x008B0666, /* DSPDxoxn */
	0x008C0E08, /* SDPnoa */
	0x008D0668, /* SDPSxoxn */
	0x008E1D7C, /* SSDxPDxax */
	0x008F0CE5, /* PDSanan */
	0x00900C45, /* PDSxna */
	0x00911E08, /* SDPSnoaxn */
	0x009217A9, /* DPSDPoaxx */
	0x009301C4, /* SPDaxn */
	0x009417AA, /* PSDPSoaxx */
	0x009501C9, /* DPSaxn */
	0x00960169, /* DPSxx */
	0x0097588A, /* PSDPSonoxx */
	0x00981888, /* SDPSonoxn */
	0x00990066, /* DSxn */
	0x009A0709, /* DPSnax */
	0x009B07A8, /* SDPSoaxn */
	0x009C0704, /* SPDnax */
	0x009D07A6, /* DSPDoaxn */
	0x009E16E6, /* DSPDSaoxx */
	0x009F0345, /* PDSxan */
	0x00A000C9, /* DPa */
	0x00A11B05, /* PDSPnaoxn */
	0x00A20E09, /* DPSnoa */
	0x00A30669, /* DPSDxoxn */
	0x00A41885, /* PDSPonoxn */
	0x00A50065, /* PDxn */
	0x00A60706, /* DSPnax */
	0x00A707A5, /* PDSPoaxn */
	0x00A803A9, /* DPSoa */
	0x00A90189, /* DPSoxn */
	0x00AA0029, /* D */
	0x00AB0889, /* DPSono */
	0x00AC0744, /* SPDSxax */
	0x00AD06E9, /* DPSDaoxn */
	0x00AE0B06, /* DSPnao */
	0x00AF0229, /* DPno */
	0x00B00E05, /* PDSnoa */
	0x00B10665, /* PDSPxoxn */
	0x00B21974, /* SSPxDSxox */
	0x00B30CE8, /* SDPanan */
	0x00B4070A, /* PSDnax */
	0x00B507A9, /* DPSDoaxn */
	0x00B616E9, /* DPSDPaoxx */
	0x00B70348, /* SDPxan */
	0x00B8074A, /* PSDPxax */
	0x00B906E6, /* DSPDaoxn */
	0x00BA0B09, /* DPSnao */
	0x00BB0226, /* DSno */
	0x00BC1CE4, /* SPDSanax */
	0x00BD0D7D, /* SDxPDxan */
	0x00BE0269, /* DPSxo */
	0x00BF08C9, /* DPSano */
	0x00C000CA, /* PSa */
	0x00C11B04, /* SPDSnaoxn */
	0x00C21884, /* SPDSonoxn */
	0x00C3006A, /* PSxn */
	0x00C40E04, /* SPDnoa */
	0x00C50664, /* SPDSxoxn */
	0x00C60708, /* SDPnax */
	0x00C707AA, /* PSDPoaxn */
	0x00C803A8, /* SDPoa */
	0x00C90184, /* SPDoxn */
	0x00CA0749, /* DPSDxax */
	0x00CB06E4, /* SPDSaoxn */
	0x00CC0020, /* S */
	0x00CD0888, /* SDPono */
	0x00CE0B08, /* SDPnao */
	0x00CF0224, /* SPno */
	0x00D00E0A, /* PSDnoa */
	0x00D1066A, /* PSDPxoxn */
	0x00D20705, /* PDSnax */
	0x00D307A4, /* SPDSoaxn */
	0x00D41D78, /* SSPxPDxax */
	0x00D50CE9, /* DPSanan */
	0x00D616EA, /* PSDPSaoxx */
	0x00D70349, /* DPSxan */
	0x00D80745, /* PDSPxax */
	0x00D906E8, /* SDPSaoxn */
	0x00DA1CE9, /* DPSDanax */
	0x00DB0D75, /* SPxDSxan */
	0x00DC0B04, /* SPDnao */
	0x00DD0228, /* SDno */
	0x00DE0268, /* SDPxo */
	0x00DF08C8, /* SDPano */
	0x00E003A5, /* PDSoa */
	0x00E10185, /* PDSoxn */
	0x00E20746, /* DSPDxax */
	0x00E306EA, /* PSDPaoxn */
	0x00E40748, /* SDPSxax */
	0x00E506E5, /* PDSPaoxn */
	0x00E61CE8, /* SDPSanax */
	0x00E70D79, /* SPxPDxan */
	0x00E81D74, /* SSPxDSxax */
	0x00E95CE6, /* DSPDSanaxxn */
	0x00EA02E9, /* DPSao */
	0x00EB0849, /* DPSxno */
	0x00EC02E8, /* SDPao */
	0x00ED0848, /* SDPxno */
	0x00EE0086, /* DSo */
	0x00EF0A08, /* SDPnoo */
	0x00F00021, /* P */
	0x00F10885, /* PDSono */
	0x00F20B05, /* PDSnao */
	0x00F3022A, /* PSno */
	0x00F40B0A, /* PSDnao */
	0x00F50225, /* PDno */
	0x00F60265, /* PDSxo */
	0x00F708C5, /* PDSano */
	0x00F802E5, /* PDSao */
	0x00F90845, /* PDSxno */
	0x00FA0089, /* DPo */
	0x00FB0A09, /* DPSnoo */
	0x00FC008A, /* PSo */
	0x00FD0A0A, /* PSDnoo */
	0x00FE02A9, /* DPSoo */
	0x00FF0062  /* 1 */
};

/* Hatch Patterns as monochrome data */
static const BYTE GDI_BS_HATCHED_PATTERNS[] =
{
	0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, /* HS_HORIZONTAL */
	0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, /* HS_VERTICAL */
	0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, /* HS_FDIAGONAL */
	0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE, /* HS_BDIAGONAL */
	0xF7, 0xF7, 0xF7, 0x00, 0xF7, 0xF7, 0xF7, 0xF7, /* HS_CROSS */
	0x7E, 0xBD, 0xDB, 0xE7, 0xE7, 0xDB, 0xBD, 0x7E /* HS_DIACROSS */
};

INLINE BOOL gdi_decode_color(rdpGdi* gdi, const UINT32 srcColor,
                             UINT32* color, UINT32* format)
{
	UINT32 SrcFormat;
	UINT32 ColorDepth;

	if (!gdi || !color || !gdi->context || !gdi->context->settings)
		return FALSE;

	ColorDepth = gdi->context->settings->ColorDepth;

	switch (ColorDepth)
	{
		case 32:
		case 24:
			SrcFormat = PIXEL_FORMAT_BGR24;
			break;

		case 16:
			SrcFormat = PIXEL_FORMAT_RGB16;
			break;

		case 15:
			SrcFormat = PIXEL_FORMAT_RGB15;
			break;

		case 8:
			SrcFormat = PIXEL_FORMAT_RGB8;
			break;

		default:
			return FALSE;
	}

	if (format)
		*format = gdi->dstFormat;

	*color = ConvertColor(srcColor, SrcFormat, gdi->dstFormat, &gdi->palette);
	return TRUE;
}

/* GDI Helper Functions */
INLINE DWORD gdi_rop3_code(BYTE code)
{
	return rop3_code_table[code];
}

UINT32 gdi_get_pixel_format(UINT32 bitsPerPixel)
{
	UINT32 format;

	switch (bitsPerPixel)
	{
		case 32:
			format = PIXEL_FORMAT_BGRA32;
			break;

		case 24:
			format = PIXEL_FORMAT_BGR24;
			break;

		case 16:
			format = PIXEL_FORMAT_RGB16;
			break;

		case 15:
			format = PIXEL_FORMAT_RGB15;
			break;

		case 8:
			format = PIXEL_FORMAT_RGB8;
			break;

		default:
			WLog_ERR(TAG, "Unsupported color depth %"PRIu32, bitsPerPixel);
			format = 0;
			break;
	}

	return format;
}

gdiBitmap* gdi_bitmap_new_ex(rdpGdi* gdi, int width, int height, int bpp,
                             BYTE* data)
{
	gdiBitmap* bitmap;
	bitmap = (gdiBitmap*) calloc(1, sizeof(gdiBitmap));

	if (!bitmap)
		goto fail_bitmap;

	if (!(bitmap->hdc = gdi_CreateCompatibleDC(gdi->hdc)))
		goto fail_hdc;

	WLog_Print(gdi->log, WLOG_DEBUG, "gdi_bitmap_new: width:%d height:%d bpp:%d",
	           width, height, bpp);

	if (!data)
		bitmap->bitmap = gdi_CreateCompatibleBitmap(gdi->hdc, width, height);
	else
		bitmap->bitmap = gdi_create_bitmap(gdi, width, height, bpp, data);

	if (!bitmap->bitmap)
		goto fail_bitmap_bitmap;

	gdi_SelectObject(bitmap->hdc, (HGDIOBJECT) bitmap->bitmap);
	bitmap->org_bitmap = NULL;
	return bitmap;
fail_bitmap_bitmap:
	gdi_DeleteDC(bitmap->hdc);
fail_hdc:
	free(bitmap);
fail_bitmap:
	return NULL;
}

void gdi_bitmap_free_ex(gdiBitmap* bitmap)
{
	if (bitmap)
	{
		gdi_SelectObject(bitmap->hdc, (HGDIOBJECT) bitmap->org_bitmap);
		gdi_DeleteObject((HGDIOBJECT) bitmap->bitmap);
		gdi_DeleteDC(bitmap->hdc);
		free(bitmap);
	}
}

BOOL gdi_bitmap_update(rdpContext* context,
                       const BITMAP_UPDATE* bitmapUpdate)
{
	UINT32 index;

	if (!context || !bitmapUpdate || !context->gdi || !context->codecs)
		return FALSE;

	for (index = 0; index < bitmapUpdate->number; index++)
	{
		const BITMAP_DATA* bitmap = &(bitmapUpdate->rectangles[index]);
		rdpBitmap* bmp = Bitmap_Alloc(context);

		if (!bmp)
			return FALSE;

		Bitmap_SetDimensions(bmp, bitmap->width, bitmap->height);
		Bitmap_SetRectangle(bmp, bitmap->destLeft, bitmap->destTop, bitmap->destRight,
		                    bitmap->destBottom);

		if (!bmp->Decompress(context, bmp, bitmap->bitmapDataStream,
		                     bitmap->width, bitmap->height, bitmap->bitsPerPixel,
		                     bitmap->bitmapLength, bitmap->compressed,
		                     RDP_CODEC_ID_NONE))
		{
			Bitmap_Free(context, bmp);
			return FALSE;
		}

		if (!bmp->New(context, bmp))
		{
			Bitmap_Free(context, bmp);
			return FALSE;
		}

		if (!bmp->Paint(context, bmp))
		{
			Bitmap_Free(context, bmp);
			return FALSE;
		}

		Bitmap_Free(context, bmp);
	}

	return TRUE;
}

static BOOL gdi_palette_update(rdpContext* context,
                               const PALETTE_UPDATE* palette)
{
	UINT32 index;
	rdpGdi* gdi;

	if (!context || !palette)
		return FALSE;

	gdi = context->gdi;
	gdi->palette.format = gdi->dstFormat;

	for (index = 0; index < palette->number; index++)
	{
		const PALETTE_ENTRY* pe = &(palette->entries[index]);
		gdi->palette.palette[index] =
		    GetColor(gdi->dstFormat, pe->red, pe->green, pe->blue, 0xFF);
	}

	return TRUE;
}

static BOOL gdi_set_bounds(rdpContext* context, const rdpBounds* bounds)
{
	rdpGdi* gdi;

	if (!context)
		return FALSE;

	gdi = context->gdi;

	if (bounds)
	{
		gdi_SetClipRgn(gdi->drawing->hdc, bounds->left, bounds->top,
		               bounds->right - bounds->left + 1, bounds->bottom - bounds->top + 1);
	}
	else
		gdi_SetNullClipRgn(gdi->drawing->hdc);

	return TRUE;
}

static BOOL gdi_dstblt(rdpContext* context, const DSTBLT_ORDER* dstblt)
{
	rdpGdi* gdi;

	if (!context || !dstblt)
		return FALSE;

	gdi = context->gdi;
	return gdi_BitBlt(gdi->drawing->hdc, dstblt->nLeftRect, dstblt->nTopRect,
	                  dstblt->nWidth, dstblt->nHeight, NULL, 0, 0,
	                  gdi_rop3_code(dstblt->bRop), &gdi->palette);
}

static BOOL gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	const rdpBrush* brush = &patblt->brush;
	UINT32 foreColor;
	UINT32 backColor;
	UINT32 originalColor;
	HGDI_BRUSH originalBrush, hbrush = NULL;
	rdpGdi* gdi = context->gdi;
	BOOL ret = FALSE;
	const DWORD rop = gdi_rop3_code(patblt->bRop);
	UINT32 nXSrc = 0;
	UINT32 nYSrc = 0;
	BYTE data[8 * 8 * 4];
	HGDI_BITMAP hBmp = NULL;

	if (!gdi_decode_color(gdi, patblt->foreColor, &foreColor, NULL))
		return FALSE;

	if (!gdi_decode_color(gdi, patblt->backColor, &backColor, NULL))
		return FALSE;

	originalColor = gdi_SetTextColor(gdi->drawing->hdc, foreColor);
	originalBrush = gdi->drawing->hdc->brush;

	switch (brush->style)
	{
		case GDI_BS_SOLID:
			hbrush = gdi_CreateSolidBrush(foreColor);
			break;

		case GDI_BS_HATCHED:
			{
				const BYTE* hatched;
				hatched = GDI_BS_HATCHED_PATTERNS + (8 * brush->hatch);

				if (!freerdp_image_copy_from_monochrome(data, gdi->drawing->hdc->format, 0, 0,
				                                        0, 8, 8,
				                                        hatched, backColor, foreColor, &gdi->palette))
					goto out_error;

				hBmp = gdi_CreateBitmapEx(8, 8, gdi->drawing->hdc->format, 0, data, NULL);

				if (!hBmp)
					goto out_error;

				hbrush = gdi_CreateHatchBrush(hBmp);
			}
			break;

		case GDI_BS_PATTERN:
			{
				UINT32 brushFormat;

				if (brush->bpp > 1)
				{
					UINT32 bpp = brush->bpp;

					if ((bpp == 16) && (context->settings->ColorDepth == 15))
						bpp = 15;

					brushFormat = gdi_get_pixel_format(bpp);

					if (!freerdp_image_copy(data, gdi->drawing->hdc->format, 0, 0, 0,
					                        8, 8, brush->data, brushFormat, 0, 0, 0,
					                        &gdi->palette, FREERDP_FLIP_NONE))
						goto out_error;
				}
				else
				{
					if (!freerdp_image_copy_from_monochrome(data, gdi->drawing->hdc->format, 0, 0,
					                                        0, 8, 8,
					                                        brush->data, backColor, foreColor, &gdi->palette))
						goto out_error;
				}

				hBmp = gdi_CreateBitmapEx(8, 8, gdi->drawing->hdc->format, 0, data, NULL);

				if (!hBmp)
					goto out_error;

				hbrush = gdi_CreatePatternBrush(hBmp);
			}
			break;

		default:
			WLog_ERR(TAG,  "unimplemented brush style:%"PRIu32"", brush->style);
			break;
	}

	if (!hbrush)
		gdi_DeleteObject((HGDIOBJECT) hBmp);
	else
	{
		hbrush->nXOrg = brush->x;
		hbrush->nYOrg = brush->y;
		gdi->drawing->hdc->brush = hbrush;
		ret = gdi_BitBlt(gdi->drawing->hdc, patblt->nLeftRect, patblt->nTopRect,
		                 patblt->nWidth, patblt->nHeight,
		                 gdi->primary->hdc, nXSrc, nYSrc, rop, &gdi->palette);
	}

out_error:
	gdi_DeleteObject((HGDIOBJECT) hbrush);
	gdi->drawing->hdc->brush = originalBrush;
	gdi_SetTextColor(gdi->drawing->hdc, originalColor);
	return ret;
}

static BOOL gdi_scrblt(rdpContext* context, const SCRBLT_ORDER* scrblt)
{
	rdpGdi* gdi;

	if (!context || !context->gdi)
		return FALSE;

	gdi = context->gdi;
	return gdi_BitBlt(gdi->drawing->hdc, scrblt->nLeftRect, scrblt->nTopRect,
	                  scrblt->nWidth, scrblt->nHeight, gdi->primary->hdc,
	                  scrblt->nXSrc, scrblt->nYSrc, gdi_rop3_code(scrblt->bRop),
	                  &gdi->palette);
}

static BOOL gdi_opaque_rect(rdpContext* context,
                            const OPAQUE_RECT_ORDER* opaque_rect)
{
	GDI_RECT rect;
	HGDI_BRUSH hBrush;
	UINT32 brush_color;
	rdpGdi* gdi = context->gdi;
	BOOL ret;
	gdi_CRgnToRect(opaque_rect->nLeftRect, opaque_rect->nTopRect,
	               opaque_rect->nWidth, opaque_rect->nHeight, &rect);

	if (!gdi_decode_color(gdi, opaque_rect->color, &brush_color, NULL))
		return FALSE;

	if (!(hBrush = gdi_CreateSolidBrush(brush_color)))
		return FALSE;

	ret = gdi_FillRect(gdi->drawing->hdc, &rect, hBrush);
	gdi_DeleteObject((HGDIOBJECT) hBrush);
	return ret;
}

static BOOL gdi_multi_opaque_rect(rdpContext* context,
                                  const MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	UINT32 i;
	GDI_RECT rect;
	HGDI_BRUSH hBrush;
	UINT32 brush_color;
	rdpGdi* gdi = context->gdi;
	BOOL ret = TRUE;

	if (!gdi_decode_color(gdi, multi_opaque_rect->color, &brush_color, NULL))
		return FALSE;

	hBrush = gdi_CreateSolidBrush(brush_color);

	if (!hBrush)
		return FALSE;

	for (i = 0; i < multi_opaque_rect->numRectangles; i++)
	{
		const DELTA_RECT* rectangle = &multi_opaque_rect->rectangles[i];
		gdi_CRgnToRect(rectangle->left, rectangle->top,
		               rectangle->width, rectangle->height, &rect);
		ret = gdi_FillRect(gdi->drawing->hdc, &rect, hBrush);

		if (!ret)
			break;
	}

	gdi_DeleteObject((HGDIOBJECT) hBrush);
	return ret;
}

static BOOL gdi_line_to(rdpContext* context, const LINE_TO_ORDER* lineTo)
{
	UINT32 color;
	HGDI_PEN hPen;
	rdpGdi* gdi = context->gdi;

	if (!gdi_decode_color(gdi, lineTo->penColor, &color, NULL))
		return FALSE;

	if (!(hPen = gdi_CreatePen(lineTo->penStyle, lineTo->penWidth, color,
	                           gdi->drawing->hdc->format, &gdi->palette)))
		return FALSE;

	gdi_SelectObject(gdi->drawing->hdc, (HGDIOBJECT) hPen);
	gdi_SetROP2(gdi->drawing->hdc, lineTo->bRop2);
	gdi_MoveToEx(gdi->drawing->hdc, lineTo->nXStart, lineTo->nYStart, NULL);
	gdi_LineTo(gdi->drawing->hdc, lineTo->nXEnd, lineTo->nYEnd);
	gdi_DeleteObject((HGDIOBJECT) hPen);
	return TRUE;
}

static BOOL gdi_polyline(rdpContext* context, const POLYLINE_ORDER* polyline)
{
	UINT32 i;
	INT32 x;
	INT32 y;
	UINT32 color;
	HGDI_PEN hPen;
	DELTA_POINT* points;
	rdpGdi* gdi = context->gdi;

	if (!gdi_decode_color(gdi, polyline->penColor, &color, NULL))
		return FALSE;

	if (!(hPen = gdi_CreatePen(GDI_PS_SOLID, 1, color, gdi->drawing->hdc->format,
	                           &gdi->palette)))
		return FALSE;

	gdi_SelectObject(gdi->drawing->hdc, (HGDIOBJECT) hPen);
	gdi_SetROP2(gdi->drawing->hdc, polyline->bRop2);
	x = polyline->xStart;
	y = polyline->yStart;
	gdi_MoveToEx(gdi->drawing->hdc, x, y, NULL);
	points = polyline->points;

	for (i = 0; i < polyline->numDeltaEntries; i++)
	{
		x += points[i].x;
		y += points[i].y;
		gdi_LineTo(gdi->drawing->hdc, x, y);
		gdi_MoveToEx(gdi->drawing->hdc, x, y, NULL);
	}

	gdi_DeleteObject((HGDIOBJECT) hPen);
	return TRUE;
}

static BOOL gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	gdiBitmap* bitmap;
	rdpGdi* gdi;

	if (!context || !memblt || !context->gdi || !memblt->bitmap)
		return FALSE;

	bitmap = (gdiBitmap*) memblt->bitmap;
	gdi = context->gdi;
	return gdi_BitBlt(gdi->drawing->hdc, memblt->nLeftRect, memblt->nTopRect,
	                  memblt->nWidth, memblt->nHeight, bitmap->hdc,
	                  memblt->nXSrc, memblt->nYSrc, gdi_rop3_code(memblt->bRop),
	                  &gdi->palette);
}

static BOOL gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	HGDI_BRUSH originalBrush;
	rdpGdi* gdi = context->gdi;
	BOOL ret = TRUE;
	const rdpBrush* brush = &mem3blt->brush;
	gdiBitmap* bitmap = (gdiBitmap*) mem3blt->bitmap;
	UINT32 foreColor;
	UINT32 backColor;
	UINT32 originalColor;

	if (!gdi_decode_color(gdi, mem3blt->foreColor, &foreColor, NULL))
		return FALSE;

	if (!gdi_decode_color(gdi, mem3blt->backColor, &backColor, NULL))
		return FALSE;

	originalColor = gdi_SetTextColor(gdi->drawing->hdc, foreColor);

	switch (brush->style)
	{
		case GDI_BS_SOLID:
			originalBrush = gdi->drawing->hdc->brush;
			gdi->drawing->hdc->brush = gdi_CreateSolidBrush(foreColor);

			if (!gdi->drawing->hdc->brush)
			{
				ret = FALSE;
				goto out_fail;
			}

			ret = gdi_BitBlt(gdi->drawing->hdc, mem3blt->nLeftRect, mem3blt->nTopRect,
			                 mem3blt->nWidth, mem3blt->nHeight, bitmap->hdc,
			                 mem3blt->nXSrc, mem3blt->nYSrc, gdi_rop3_code(mem3blt->bRop),
			                 &gdi->palette);
			gdi_DeleteObject((HGDIOBJECT) gdi->drawing->hdc->brush);
			gdi->drawing->hdc->brush = originalBrush;
			break;

		case GDI_BS_PATTERN:
			{
				HGDI_BITMAP hBmp;
				UINT32 brushFormat;
				BYTE* data = (BYTE*) _aligned_malloc(8 * 8 * GetBytesPerPixel(
				        gdi->drawing->hdc->format),
				                                     16);

				if (!data)
				{
					ret = FALSE;
					goto out_fail;
				}

				if (brush->bpp > 1)
				{
					UINT32 bpp = brush->bpp;

					if ((bpp == 16) && (context->settings->ColorDepth == 15))
						bpp = 15;

					brushFormat = gdi_get_pixel_format(bpp);

					if (!freerdp_image_copy(data, gdi->drawing->hdc->format, 0, 0, 0,
					                        8, 8, brush->data, brushFormat,
					                        0, 0, 0, &gdi->palette, FREERDP_FLIP_NONE))
					{
						ret = FALSE;
						_aligned_free(data);
						goto out_fail;
					}
				}
				else
				{
					if (!freerdp_image_copy_from_monochrome(data, gdi->drawing->hdc->format, 0, 0,
					                                        0, 8, 8,
					                                        brush->data, backColor, foreColor,
					                                        &gdi->palette))
					{
						ret = FALSE;
						_aligned_free(data);
						goto out_fail;
					}
				}

				hBmp = gdi_CreateBitmap(8, 8, gdi->drawing->hdc->format, data);

				if (!hBmp)
				{
					ret = FALSE;
					_aligned_free(data);
					goto out_fail;
				}

				originalBrush = gdi->drawing->hdc->brush;
				gdi->drawing->hdc->brush = gdi_CreatePatternBrush(hBmp);

				if (!gdi->drawing->hdc->brush)
				{
					gdi_DeleteObject((HGDIOBJECT) hBmp);
					goto out_fail;
				}

				gdi->drawing->hdc->brush->nXOrg = brush->x;
				gdi->drawing->hdc->brush->nYOrg = brush->y;
				ret = gdi_BitBlt(gdi->drawing->hdc, mem3blt->nLeftRect, mem3blt->nTopRect,
				                 mem3blt->nWidth, mem3blt->nHeight, bitmap->hdc,
				                 mem3blt->nXSrc, mem3blt->nYSrc, gdi_rop3_code(mem3blt->bRop),
				                 &gdi->palette);
				gdi_DeleteObject((HGDIOBJECT) gdi->drawing->hdc->brush);
				gdi->drawing->hdc->brush = originalBrush;
			}
			break;

		default:
			WLog_ERR(TAG,  "Mem3Blt unimplemented brush style:%"PRIu32"", brush->style);
			break;
	}

out_fail:
	gdi_SetTextColor(gdi->drawing->hdc, originalColor);
	return ret;
}

static BOOL gdi_polygon_sc(rdpContext* context,
                           const POLYGON_SC_ORDER* polygon_sc)
{
	WLog_WARN(TAG, "%s: not implemented", __FUNCTION__);
	return FALSE;
}

static BOOL gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	WLog_WARN(TAG, "%s: not implemented", __FUNCTION__);
	return FALSE;
}

static BOOL gdi_ellipse_sc(rdpContext* context,
                           const ELLIPSE_SC_ORDER* ellipse_sc)
{
	WLog_WARN(TAG, "%s: not implemented", __FUNCTION__);
	return FALSE;
}

static BOOL gdi_ellipse_cb(rdpContext* context,
                           const ELLIPSE_CB_ORDER* ellipse_cb)
{
	WLog_WARN(TAG, "%s: not implemented", __FUNCTION__);
	return FALSE;
}

static BOOL gdi_frame_marker(rdpContext* context,
                             const FRAME_MARKER_ORDER* frameMarker)
{
	return TRUE;
}

BOOL gdi_surface_frame_marker(rdpContext* context,
                              const SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	WLog_Print(context->gdi->log, WLOG_DEBUG, "frameId %"PRIu32" frameAction %"PRIu32"",
	           surfaceFrameMarker->frameId,
	           surfaceFrameMarker->frameAction);

	switch (surfaceFrameMarker->frameAction)
	{
		case SURFACECMD_FRAMEACTION_BEGIN:
			break;

		case SURFACECMD_FRAMEACTION_END:
			if (context->settings->FrameAcknowledge > 0)
			{
				IFCALL(context->update->SurfaceFrameAcknowledge, context,
				       surfaceFrameMarker->frameId);
			}

			break;
	}

	return TRUE;
}

static BOOL gdi_surface_bits(rdpContext* context,
                             const SURFACE_BITS_COMMAND* cmd)
{
	BOOL result = FALSE;
	DWORD format;
	rdpGdi* gdi;
	REGION16 region;
	RECTANGLE_16 cmdRect;
	UINT32 i, nbRects;
	const RECTANGLE_16* rects;

	if (!context || !cmd)
		return FALSE;

	gdi = context->gdi;
	WLog_Print(gdi->log, WLOG_DEBUG,
	           "destLeft %"PRIu32" destTop %"PRIu32" destRight %"PRIu32" destBottom %"PRIu32" "
	           "bpp %"PRIu32" codecID %"PRIu32" width %"PRIu32" height %"PRIu32" length %"PRIu32"",
	           cmd->destLeft, cmd->destTop, cmd->destRight, cmd->destBottom,
	           cmd->bpp, cmd->codecID, cmd->width, cmd->height, cmd->bitmapDataLength);
	region16_init(&region);
	cmdRect.left = cmd->destLeft;
	cmdRect.top = cmd->destTop;
	cmdRect.right = cmdRect.left + cmd->width;
	cmdRect.bottom = cmdRect.top + cmd->height;

	switch (cmd->codecID)
	{
		case RDP_CODEC_ID_REMOTEFX:
			if (!rfx_process_message(context->codecs->rfx, cmd->bitmapData,
			                         cmd->bitmapDataLength,
			                         cmd->destLeft, cmd->destTop,
			                         gdi->primary_buffer, gdi->dstFormat,
			                         gdi->stride, gdi->height, &region))
			{
				WLog_ERR(TAG, "Failed to process RemoteFX message");
				goto out;
			}

			break;

		case RDP_CODEC_ID_NSCODEC:
			format = gdi->dstFormat;

			if (!nsc_process_message(context->codecs->nsc, cmd->bpp, cmd->width,
			                         cmd->height, cmd->bitmapData,
			                         cmd->bitmapDataLength, gdi->primary_buffer,
			                         format, gdi->stride, cmd->destLeft, cmd->destTop,
			                         cmd->width, cmd->height, FREERDP_FLIP_VERTICAL))
			{
				WLog_ERR(TAG, "Failed to process NSCodec message");
				goto out;
			}

			region16_union_rect(&region, &region, &cmdRect);
			break;

		case RDP_CODEC_ID_NONE:
			format = gdi_get_pixel_format(cmd->bpp);

			if (!freerdp_image_copy(gdi->primary_buffer, gdi->dstFormat, gdi->stride,
			                        cmd->destLeft, cmd->destTop, cmd->width, cmd->height,
			                        cmd->bitmapData, format, 0, 0, 0,
			                        &gdi->palette, FREERDP_FLIP_VERTICAL))
			{
				WLog_ERR(TAG, "Failed to process nocodec message");
				goto out;
			}

			region16_union_rect(&region, &region, &cmdRect);
			break;

		default:
			WLog_ERR(TAG, "Unsupported codecID %"PRIu32"", cmd->codecID);
			break;
	}

	if (!(rects = region16_rects(&region, &nbRects)))
		goto out;

	for (i = 0; i < nbRects; i++)
	{
		UINT32 left = rects[i].left;
		UINT32 top = rects[i].top;
		UINT32 width = rects[i].right - rects[i].left;
		UINT32 height = rects[i].bottom - rects[i].top;

		if (!gdi_InvalidateRegion(gdi->primary->hdc, left, top, width, height))
		{
			WLog_ERR(TAG, "Failed to update invalid region");
			goto out;
		}
	}

	result = TRUE;
out:
	region16_uninit(&region);
	return result;
}

/**
 * Register GDI callbacks with libfreerdp-core.
 * @param inst current instance
 * @return
 */

static void gdi_register_update_callbacks(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;
	update->Palette = gdi_palette_update;
	update->SetBounds = gdi_set_bounds;
	primary->DstBlt = gdi_dstblt;
	primary->PatBlt = gdi_patblt;
	primary->ScrBlt = gdi_scrblt;
	primary->OpaqueRect = gdi_opaque_rect;
	primary->DrawNineGrid = NULL;
	primary->MultiDstBlt = NULL;
	primary->MultiPatBlt = NULL;
	primary->MultiScrBlt = NULL;
	primary->MultiOpaqueRect = gdi_multi_opaque_rect;
	primary->MultiDrawNineGrid = NULL;
	primary->LineTo = gdi_line_to;
	primary->Polyline = gdi_polyline;
	primary->MemBlt = gdi_memblt;
	primary->Mem3Blt = gdi_mem3blt;
	primary->SaveBitmap = NULL;
	primary->GlyphIndex = NULL;
	primary->FastIndex = NULL;
	primary->FastGlyph = NULL;
	primary->PolygonSC = gdi_polygon_sc;
	primary->PolygonCB = gdi_polygon_cb;
	primary->EllipseSC = gdi_ellipse_sc;
	primary->EllipseCB = gdi_ellipse_cb;
	update->SurfaceBits = gdi_surface_bits;
	update->SurfaceFrameMarker = gdi_surface_frame_marker;
	update->altsec->FrameMarker = gdi_frame_marker;
}

static BOOL gdi_init_primary(rdpGdi* gdi, UINT32 stride, UINT32 format,
                             BYTE* buffer, void (*pfree)(void*))
{
	gdi->primary = (gdiBitmap*) calloc(1, sizeof(gdiBitmap));

	if (format > 0)
		gdi->dstFormat = format;

	if (stride > 0)
		gdi->stride = stride;
	else
		gdi->stride = gdi->width * GetBytesPerPixel(gdi->dstFormat);

	if (!gdi->primary)
		goto fail_primary;

	if (!(gdi->primary->hdc = gdi_CreateCompatibleDC(gdi->hdc)))
		goto fail_hdc;

	if (!buffer)
	{
		gdi->primary->bitmap = gdi_CreateCompatibleBitmap(
		                           gdi->hdc, gdi->width, gdi->height);
	}
	else
	{
		gdi->primary->bitmap = gdi_CreateBitmapEx(gdi->width, gdi->height,
		                       gdi->dstFormat,
		                       gdi->stride,
		                       buffer, pfree);
	}

	if (!gdi->primary->bitmap)
		goto fail_bitmap;

	gdi->stride = gdi->primary->bitmap->scanline;

	gdi_SelectObject(gdi->primary->hdc, (HGDIOBJECT) gdi->primary->bitmap);
	gdi->primary->org_bitmap = NULL;
	gdi->primary_buffer = gdi->primary->bitmap->data;

	if (!(gdi->primary->hdc->hwnd = (HGDI_WND) calloc(1, sizeof(GDI_WND))))
		goto fail_hwnd;

	if (!(gdi->primary->hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0)))
		goto fail_hwnd;

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->count = 32;

	if (!(gdi->primary->hdc->hwnd->cinvalid = (HGDI_RGN) calloc(
	            gdi->primary->hdc->hwnd->count, sizeof(GDI_RGN))))
		goto fail_hwnd;

	gdi->primary->hdc->hwnd->ninvalid = 0;

	if (!gdi->drawing)
		gdi->drawing = gdi->primary;

	return TRUE;
fail_hwnd:
	gdi_DeleteObject((HGDIOBJECT) gdi->primary->bitmap);
fail_bitmap:
	gdi_DeleteDC(gdi->primary->hdc);
fail_hdc:
	free(gdi->primary);
	gdi->primary = NULL;
fail_primary:
	return FALSE;
}

BOOL gdi_resize(rdpGdi* gdi, UINT32 width, UINT32 height)
{
	return gdi_resize_ex(gdi, width, height, 0, 0, NULL, NULL);
}

BOOL gdi_resize_ex(rdpGdi* gdi, UINT32 width, UINT32 height,
                   UINT32 stride, UINT32 format, BYTE* buffer,
                   void (*pfree)(void*))
{
	if (!gdi || !gdi->primary)
		return FALSE;

	if (gdi->width == width && gdi->height == height &&
	    (!buffer || gdi->primary_buffer == buffer))
		return TRUE;

	if (gdi->drawing == gdi->primary)
		gdi->drawing = NULL;

	gdi->width = width;
	gdi->height = height;
	gdi_bitmap_free_ex(gdi->primary);
	gdi->primary = NULL;
	gdi->primary_buffer = NULL;
	return gdi_init_primary(gdi, stride, format, buffer, pfree);
}

/**
 * Initialize GDI
 * @param inst current instance
 * @return
 */
BOOL gdi_init(freerdp* instance, UINT32 format)
{
	return gdi_init_ex(instance, format, 0, NULL, _aligned_free);
}

BOOL gdi_init_ex(freerdp* instance, UINT32 format, UINT32 stride, BYTE* buffer,
                 void (*pfree)(void*))
{
	UINT32 SrcFormat = gdi_get_pixel_format(instance->settings->ColorDepth);
	rdpGdi* gdi = (rdpGdi*) calloc(1, sizeof(rdpGdi));
	rdpContext* context = instance->context;

	if (!gdi)
		goto fail;

	instance->context->gdi = gdi;
	gdi->log = WLog_Get(TAG);

	if (!gdi->log)
		goto fail;

	gdi->context = instance->context;
	gdi->width = instance->settings->DesktopWidth;
	gdi->height = instance->settings->DesktopHeight;
	gdi->dstFormat = format;
	/* default internal buffer format */
	WLog_Print(gdi->log, WLOG_INFO, "Local framebuffer format  %s",
	           GetColorFormatName(gdi->dstFormat));
	WLog_Print(gdi->log, WLOG_INFO, "Remote framebuffer format %s",
	           GetColorFormatName(SrcFormat));

	if (!(gdi->hdc = gdi_GetDC()))
		goto fail;

	gdi->hdc->format = gdi->dstFormat;

	if (!gdi_init_primary(gdi, stride, gdi->dstFormat, buffer, pfree))
		goto fail;

	if (!(context->cache = cache_new(instance->settings)))
		goto fail;

	if (!freerdp_client_codecs_prepare(context->codecs, FREERDP_CODEC_ALL,
	                                   gdi->width, gdi->height))
		goto fail;

	gdi_register_update_callbacks(instance->update);
	brush_cache_register_callbacks(instance->update);
	glyph_cache_register_callbacks(instance->update);
	bitmap_cache_register_callbacks(instance->update);
	offscreen_cache_register_callbacks(instance->update);
	palette_cache_register_callbacks(instance->update);

	if (!gdi_register_graphics(instance->context->graphics))
		goto fail;

	return TRUE;
fail:
	gdi_free(instance);
	WLog_ERR(TAG,  "failed to initialize gdi");
	return FALSE;
}

void gdi_free(freerdp* instance)
{
	rdpGdi* gdi;
	rdpContext* context;

	if (!instance || !instance->context)
		return;

	gdi = instance->context->gdi;

	if (gdi)
	{
		gdi_bitmap_free_ex(gdi->primary);
		gdi_DeleteDC(gdi->hdc);
		free(gdi);
	}

	context = instance->context;
	cache_free(context->cache);
	context->cache = NULL;
	instance->context->gdi = (rdpGdi*) NULL;
}

