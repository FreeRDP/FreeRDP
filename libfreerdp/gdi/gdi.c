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

#include <freerdp/gdi/gdi.h>
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
typedef struct
{
	DWORD code;
	const char* name;
} rop_table_entry;

static const rop_table_entry rop3_code_table[] =
{
	{ GDI_BLACKNESS, "0" },
	{ GDI_DPSoon, "DPSoon" },
	{ GDI_DPSona, "DPSona" },
	{ GDI_PSon, "PSon" },
	{ GDI_SDPona, "SDPona" },
	{ GDI_DPon, "DPon" },
	{ GDI_PDSxnon, "PDSxnon" },
	{ GDI_PDSaon, "PDSaon" },
	{ GDI_SDPnaa, "SDPnaa" },
	{ GDI_PDSxon, "PDSxon" },
	{ GDI_DPna, "DPna" },
	{ GDI_PSDnaon, "PSDnaon" },
	{ GDI_SPna, "SPna" },
	{ GDI_PDSnaon, "PDSnaon" },
	{ GDI_PDSonon, "PDSonon" },
	{ GDI_Pn	, "Pn" },
	{ GDI_PDSona, "PDSona" },
	{ GDI_NOTSRCERASE, "DSon" },
	{ GDI_SDPxnon, "SDPxnon" },
	{ GDI_SDPaon, "SDPaon" },
	{ GDI_DPSxnon, "DPSxnon" },
	{ GDI_DPSaon, "DPSaon" },
	{ GDI_PSDPSanaxx, "PSDPSanaxx" },
	{ GDI_SSPxDSxaxn, "SSPxDSxaxn" },
	{ GDI_SPxPDxa, "SPxPDxa" },
	{ GDI_SDPSanaxn, "SDPSanaxn" },
	{ GDI_PDSPaox, "PDSPaox" },
	{ GDI_SDPSxaxn, "SDPSxaxn" },
	{ GDI_PSDPaox, "PSDPaox" },
	{ GDI_DSPDxaxn, "DSPDxaxn" },
	{ GDI_PDSox, "PDSox" },
	{ GDI_PDSoan, "PDSoan" },
	{ GDI_DPSnaa, "DPSnaa" },
	{ GDI_SDPxon, "SDPxon" },
	{ GDI_DSna, "DSna" },
	{ GDI_SPDnaon, "SPDnaon" },
	{ GDI_SPxDSxa, "SPxDSxa" },
	{ GDI_PDSPanaxn, "PDSPanaxn" },
	{ GDI_SDPSaox, "SDPSaox" },
	{ GDI_SDPSxnox, "SDPSxnox" },
	{ GDI_DPSxa, "DPSxa" },
	{ GDI_PSDPSaoxxn, "PSDPSaoxxn" },
	{ GDI_DPSana, "DPSana" },
	{ GDI_SSPxPDxaxn, "SSPxPDxaxn" },
	{ GDI_SPDSoax, "SPDSoax" },
	{ GDI_PSDnox, "PSDnox" },
	{ GDI_PSDPxox, "PSDPxox" },
	{ GDI_PSDnoan, "PSDnoan" },
	{ GDI_PSna, "PSna" },
	{ GDI_SDPnaon, "SDPnaon" },
	{ GDI_SDPSoox, "SDPSoox" },
	{ GDI_NOTSRCCOPY, "Sn" },
	{ GDI_SPDSaox, "SPDSaox" },
	{ GDI_SPDSxnox, "SPDSxnox" },
	{ GDI_SDPox, "SDPox" },
	{ GDI_SDPoan, "SDPoan" },
	{ GDI_PSDPoax, "PSDPoax" },
	{ GDI_SPDnox, "SPDnox" },
	{ GDI_SPDSxox, "SPDSxox" },
	{ GDI_SPDnoan, "SPDnoan" },
	{ GDI_PSx	, "PSx" },
	{ GDI_SPDSonox, "SPDSonox" },
	{ GDI_SPDSnaox, "SPDSnaox" },
	{ GDI_PSan, "PSan" },
	{ GDI_PSDnaa, "PSDnaa" },
	{ GDI_DPSxon, "DPSxon" },
	{ GDI_SDxPDxa, "SDxPDxa" },
	{ GDI_SPDSanaxn, "SPDSanaxn" },
	{ GDI_SRCERASE, "SDna" },
	{ GDI_DPSnaon, "DPSnaon" },
	{ GDI_DSPDaox, "DSPDaox" },
	{ GDI_PSDPxaxn, "PSDPxaxn" },
	{ GDI_SDPxa, "SDPxa" },
	{ GDI_PDSPDaoxxn, "PDSPDaoxxn" },
	{ GDI_DPSDoax, "DPSDoax" },
	{ GDI_PDSnox, "PDSnox" },
	{ GDI_SDPana, "SDPana" },
	{ GDI_SSPxDSxoxn, "SSPxDSxoxn" },
	{ GDI_PDSPxox, "PDSPxox" },
	{ GDI_PDSnoan, "PDSnoan" },
	{ GDI_PDna, "PDna" },
	{ GDI_DSPnaon, "DSPnaon" },
	{ GDI_DPSDaox, "DPSDaox" },
	{ GDI_SPDSxaxn, "SPDSxaxn" },
	{ GDI_DPSonon, "DPSonon" },
	{ GDI_DSTINVERT, "Dn" },
	{ GDI_DPSox, "DPSox" },
	{ GDI_DPSoan, "DPSoan" },
	{ GDI_PDSPoax, "PDSPoax" },
	{ GDI_DPSnox, "DPSnox" },
	{ GDI_PATINVERT, "DPx" },
	{ GDI_DPSDonox, "DPSDonox" },
	{ GDI_DPSDxox, "DPSDxox" },
	{ GDI_DPSnoan, "DPSnoan" },
	{ GDI_DPSDnaox, "DPSDnaox" },
	{ GDI_DPan, "DPan" },
	{ GDI_PDSxa, "PDSxa" },
	{ GDI_DSPDSaoxxn, "DSPDSaoxxn" },
	{ GDI_DSPDoax, "DSPDoax" },
	{ GDI_SDPnox, "SDPnox" },
	{ GDI_SDPSoax, "SDPSoax" },
	{ GDI_DSPnox, "DSPnox" },
	{ GDI_SRCINVERT, "DSx" },
	{ GDI_SDPSonox, "SDPSonox" },
	{ GDI_DSPDSonoxxn, "DSPDSonoxxn" },
	{ GDI_PDSxxn, "PDSxxn" },
	{ GDI_DPSax, "DPSax" },
	{ GDI_PSDPSoaxxn, "PSDPSoaxxn" },
	{ GDI_SDPax, "SDPax" },
	{ GDI_PDSPDoaxxn, "PDSPDoaxxn" },
	{ GDI_SDPSnoax, "SDPSnoax" },
	{ GDI_PDSxnan, "PDSxnan" },
	{ GDI_PDSana, "PDSana" },
	{ GDI_SSDxPDxaxn, "SSDxPDxaxn" },
	{ GDI_SDPSxox, "SDPSxox" },
	{ GDI_SDPnoan, "SDPnoan" },
	{ GDI_DSPDxox, "DSPDxox" },
	{ GDI_DSPnoan, "DSPnoan" },
	{ GDI_SDPSnaox, "SDPSnaox" },
	{ GDI_DSan, "DSan" },
	{ GDI_PDSax, "PDSax" },
	{ GDI_DSPDSoaxxn, "DSPDSoaxxn" },
	{ GDI_DPSDnoax, "DPSDnoax" },
	{ GDI_SDPxnan, "SDPxnan" },
	{ GDI_SPDSnoax, "SPDSnoax" },
	{ GDI_DPSxnan, "DPSxnan" },
	{ GDI_SPxDSxo, "SPxDSxo" },
	{ GDI_DPSaan, "DPSaan" },
	{ GDI_DPSaa, "DPSaa" },
	{ GDI_SPxDSxon, "SPxDSxon" },
	{ GDI_DPSxna, "DPSxna" },
	{ GDI_SPDSnoaxn, "SPDSnoaxn" },
	{ GDI_SDPxna, "SDPxna" },
	{ GDI_PDSPnoaxn, "PDSPnoaxn" },
	{ GDI_DSPDSoaxx, "DSPDSoaxx" },
	{ GDI_PDSaxn, "PDSaxn" },
	{ GDI_SRCAND, "DSa" },
	{ GDI_SDPSnaoxn, "SDPSnaoxn" },
	{ GDI_DSPnoa, "DSPnoa" },
	{ GDI_DSPDxoxn, "DSPDxoxn" },
	{ GDI_SDPnoa, "SDPnoa" },
	{ GDI_SDPSxoxn, "SDPSxoxn" },
	{ GDI_SSDxPDxax, "SSDxPDxax" },
	{ GDI_PDSanan, "PDSanan" },
	{ GDI_PDSxna, "PDSxna" },
	{ GDI_SDPSnoaxn, "SDPSnoaxn" },
	{ GDI_DPSDPoaxx, "DPSDPoaxx" },
	{ GDI_SPDaxn, "SPDaxn" },
	{ GDI_PSDPSoaxx, "PSDPSoaxx" },
	{ GDI_DPSaxn, "DPSaxn" },
	{ GDI_DPSxx, "DPSxx" },
	{ GDI_PSDPSonoxx, "PSDPSonoxx" },
	{ GDI_SDPSonoxn, "SDPSonoxn" },
	{ GDI_DSxn, "DSxn" },
	{ GDI_DPSnax, "DPSnax" },
	{ GDI_SDPSoaxn, "SDPSoaxn" },
	{ GDI_SPDnax, "SPDnax" },
	{ GDI_DSPDoaxn, "DSPDoaxn" },
	{ GDI_DSPDSaoxx, "DSPDSaoxx" },
	{ GDI_PDSxan, "PDSxan" },
	{ GDI_DPa	, "DPa" },
	{ GDI_PDSPnaoxn, "PDSPnaoxn" },
	{ GDI_DPSnoa, "DPSnoa" },
	{ GDI_DPSDxoxn, "DPSDxoxn" },
	{ GDI_PDSPonoxn, "PDSPonoxn" },
	{ GDI_PDxn, "PDxn" },
	{ GDI_DSPnax, "DSPnax" },
	{ GDI_PDSPoaxn, "PDSPoaxn" },
	{ GDI_DPSoa, "DPSoa" },
	{ GDI_DPSoxn, "DPSoxn" },
	{ GDI_DSTCOPY, "D" },
	{ GDI_DPSono, "DPSono" },
	{ GDI_SPDSxax, "SPDSxax" },
	{ GDI_DPSDaoxn, "DPSDaoxn" },
	{ GDI_DSPnao, "DSPnao" },
	{ GDI_DPno, "DPno" },
	{ GDI_PDSnoa, "PDSnoa" },
	{ GDI_PDSPxoxn, "PDSPxoxn" },
	{ GDI_SSPxDSxox, "SSPxDSxox" },
	{ GDI_SDPanan, "SDPanan" },
	{ GDI_PSDnax, "PSDnax" },
	{ GDI_DPSDoaxn, "DPSDoaxn" },
	{ GDI_DPSDPaoxx, "DPSDPaoxx" },
	{ GDI_SDPxan, "SDPxan" },
	{ GDI_PSDPxax, "PSDPxax" },
	{ GDI_DSPDaoxn, "DSPDaoxn" },
	{ GDI_DPSnao, "DPSnao" },
	{ GDI_MERGEPAINT, "DSno" },
	{ GDI_SPDSanax, "SPDSanax" },
	{ GDI_SDxPDxan, "SDxPDxan" },
	{ GDI_DPSxo, "DPSxo" },
	{ GDI_DPSano, "DPSano" },
	{ GDI_MERGECOPY, "PSa" },
	{ GDI_SPDSnaoxn, "SPDSnaoxn" },
	{ GDI_SPDSonoxn, "SPDSonoxn" },
	{ GDI_PSxn, "PSxn" },
	{ GDI_SPDnoa, "SPDnoa" },
	{ GDI_SPDSxoxn, "SPDSxoxn" },
	{ GDI_SDPnax, "SDPnax" },
	{ GDI_PSDPoaxn, "PSDPoaxn" },
	{ GDI_SDPoa, "SDPoa" },
	{ GDI_SPDoxn, "SPDoxn" },
	{ GDI_DPSDxax, "DPSDxax" },
	{ GDI_SPDSaoxn, "SPDSaoxn" },
	{ GDI_SRCCOPY, "S" },
	{ GDI_SDPono, "SDPono" },
	{ GDI_SDPnao, "SDPnao" },
	{ GDI_SPno, "SPno" },
	{ GDI_PSDnoa, "PSDnoa" },
	{ GDI_PSDPxoxn, "PSDPxoxn" },
	{ GDI_PDSnax, "PDSnax" },
	{ GDI_SPDSoaxn, "SPDSoaxn" },
	{ GDI_SSPxPDxax, "SSPxPDxax" },
	{ GDI_DPSanan, "DPSanan" },
	{ GDI_PSDPSaoxx, "PSDPSaoxx" },
	{ GDI_DPSxan, "DPSxan" },
	{ GDI_PDSPxax, "PDSPxax" },
	{ GDI_SDPSaoxn, "SDPSaoxn" },
	{ GDI_DPSDanax, "DPSDanax" },
	{ GDI_SPxDSxan, "SPxDSxan" },
	{ GDI_SPDnao, "SPDnao" },
	{ GDI_SDno, "SDno" },
	{ GDI_SDPxo, "SDPxo" },
	{ GDI_SDPano, "SDPano" },
	{ GDI_PDSoa, "PDSoa" },
	{ GDI_PDSoxn, "PDSoxn" },
	{ GDI_DSPDxax, "DSPDxax" },
	{ GDI_PSDPaoxn, "PSDPaoxn" },
	{ GDI_SDPSxax, "SDPSxax" },
	{ GDI_PDSPaoxn, "PDSPaoxn" },
	{ GDI_SDPSanax, "SDPSanax" },
	{ GDI_SPxPDxan, "SPxPDxan" },
	{ GDI_SSPxDSxax, "SSPxDSxax" },
	{ GDI_DSPDSanaxxn, "DSPDSanaxxn" },
	{ GDI_DPSao, "DPSao" },
	{ GDI_DPSxno, "DPSxno" },
	{ GDI_SDPao, "SDPao" },
	{ GDI_SDPxno, "SDPxno" },
	{ GDI_SRCPAINT, "DSo" },
	{ GDI_SDPnoo, "SDPnoo" },
	{ GDI_PATCOPY, "P" },
	{ GDI_PDSono, "PDSono" },
	{ GDI_PDSnao, "PDSnao" },
	{ GDI_PSno, "PSno" },
	{ GDI_PSDnao, "PSDnao" },
	{ GDI_PDno, "PDno" },
	{ GDI_PDSxo, "PDSxo" },
	{ GDI_PDSano, "PDSano" },
	{ GDI_PDSao, "PDSao" },
	{ GDI_PDSxno, "PDSxno" },
	{ GDI_DPo	, "DPo" },
	{ GDI_PATPAINT, "DPSnoo" },
	{ GDI_PSo	, "PSo" },
	{ GDI_PSDnoo, "PSDnoo" },
	{ GDI_DPSoo, "DPSoo" },
	{ GDI_WHITENESS, "1" }
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

	*color = FreeRDPConvertColor(srcColor, SrcFormat, gdi->dstFormat, &gdi->palette);
	return TRUE;
}

/* GDI Helper Functions */
DWORD gdi_rop3_code(BYTE code)
{
	return rop3_code_table[code].code;
}

const char* gdi_rop3_code_string(BYTE code)
{
	return rop3_code_table[code].name;
}

const char* gdi_rop3_string(DWORD rop)
{
	const size_t count = sizeof(rop3_code_table) / sizeof(rop3_code_table[0]);
	size_t x;

	for (x = 0; x < count; x++)
	{
		if (rop3_code_table[x].code == rop)
			return rop3_code_table[x].name;
	}

	return "UNKNOWN";
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
		    FreeRDPGetColor(gdi->dstFormat, pe->red, pe->green, pe->blue, 0xFF);
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
	           "bpp %"PRIu8" flags %"PRIx8" codecID %"PRIu16" width %"PRIu16" height %"PRIu16" length %"PRIu32"",
	           cmd->destLeft, cmd->destTop, cmd->destRight, cmd->destBottom,
			   cmd->bmp.bpp, cmd->bmp.flags, cmd->bmp.codecID, cmd->bmp.width, cmd->bmp.height, cmd->bmp.bitmapDataLength);
	region16_init(&region);
	cmdRect.left = cmd->destLeft;
	cmdRect.top = cmd->destTop;
	cmdRect.right = cmdRect.left + cmd->bmp.width;
	cmdRect.bottom = cmdRect.top + cmd->bmp.height;

	switch (cmd->bmp.codecID)
	{
		case RDP_CODEC_ID_REMOTEFX:
			if (!rfx_process_message(context->codecs->rfx, cmd->bmp.bitmapData,
									 cmd->bmp.bitmapDataLength,
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

			if (!nsc_process_message(context->codecs->nsc, cmd->bmp.bpp, cmd->bmp.width,
									 cmd->bmp.height, cmd->bmp.bitmapData,
									 cmd->bmp.bitmapDataLength, gdi->primary_buffer,
			                         format, gdi->stride, cmd->destLeft, cmd->destTop,
									 cmd->bmp.width, cmd->bmp.height, FREERDP_FLIP_VERTICAL))
			{
				WLog_ERR(TAG, "Failed to process NSCodec message");
				goto out;
			}

			region16_union_rect(&region, &region, &cmdRect);
			break;

		case RDP_CODEC_ID_NONE:
			format = gdi_get_pixel_format(cmd->bmp.bpp);

			if (!freerdp_image_copy(gdi->primary_buffer, gdi->dstFormat, gdi->stride,
									cmd->destLeft, cmd->destTop, cmd->bmp.width, cmd->bmp.height,
									cmd->bmp.bitmapData, format, 0, 0, 0,
			                        &gdi->palette, FREERDP_FLIP_VERTICAL))
			{
				WLog_ERR(TAG, "Failed to process nocodec message");
				goto out;
			}

			region16_union_rect(&region, &region, &cmdRect);
			break;

		default:
			WLog_ERR(TAG, "Unsupported codecID %"PRIu32"", cmd->bmp.codecID);
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
	           FreeRDPGetColorFormatName(gdi->dstFormat));
	WLog_Print(gdi->log, WLOG_INFO, "Remote framebuffer format %s",
	           FreeRDPGetColorFormatName(SrcFormat));

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

BOOL gdi_send_suppress_output(rdpGdi* gdi, BOOL suppress)
{
	RECTANGLE_16 rect;
	rdpSettings* settings;
	rdpUpdate* update;

	if (!gdi || !gdi->context->settings || !gdi->context->update)
		return FALSE;

	if (gdi->suppressOutput == suppress)
		return TRUE;

	gdi->suppressOutput = suppress;
	settings = gdi->context->settings;
	update = gdi->context->update;
	rect.left = 0;
	rect.top = 0;
	rect.right = settings->DesktopWidth;
	rect.bottom = settings->DesktopHeight;
	return update->SuppressOutput(gdi->context, !suppress, &rect);
}

