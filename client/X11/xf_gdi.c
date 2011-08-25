/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 GDI
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "xf_gdi.h"

static const uint8 xf_rop2_table[] =
{
	0,
	GXclear,        /* 0 */
	GXnor,          /* DPon */
	GXandInverted,  /* DPna */
	GXcopyInverted, /* Pn */
	GXandReverse,   /* PDna */
	GXinvert,       /* Dn */
	GXxor,          /* DPx */
	GXnand,         /* DPan */
	GXand,          /* DPa */
	GXequiv,        /* DPxn */
	GXnoop,         /* D */
	GXorInverted,   /* DPno */
	GXcopy,         /* P */
	GXorReverse,    /* PDno */
	GXor,           /* DPo */
	GXset           /* 1 */
};

boolean xf_set_rop2(xfInfo* xfi, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		printf("Unsupported ROP2: %d\n", rop2);
		return False;
	}

	XSetFunction(xfi->display, xfi->gc, xf_rop2_table[rop2]);
	return True;
}

boolean xf_set_rop3(xfInfo* xfi, int rop3)
{
	int function = -1;

	switch (rop3)
	{
		case GDI_BLACKNESS:
			function = GXclear;
			break;

		case 0x000500A9:
			function = GXnor;
			break;

		case 0x000A0329:
			function = GXandInverted;
			break;

		case 0x000F0001:
			function = GXcopyInverted;
			break;

		case GDI_NOTSRCERASE:
			function = GXnor;
			break;

		case GDI_DSna:
			function = GXandInverted;
			break;

		case GDI_NOTSRCCOPY:
			function = GXcopyInverted;
			break;

		case GDI_SRCERASE:
			function = GXandReverse;
			break;

		case 0x00500325:
			function = GXandReverse;
			break;

		case GDI_DSTINVERT:
			function = GXinvert;
			break;

		case GDI_PATINVERT:
			function = GXxor;
			break;

		case 0x005F00E9:
			function = GXnand;
			break;

		case GDI_SRCINVERT:
			function = GXxor;
			break;

		case 0x007700E6:
			function = GXnand;
			break;

		case GDI_SRCAND:
			function = GXand;
			break;

		case 0x00990066:
			function = GXequiv;
			break;

		case 0x00A000C9:
			function = GXand;
			break;

		case GDI_PDxn:
			function = GXequiv;
			break;

		case 0x00AA0029:
			function = GXnoop;
			break;

		case 0x00AF0229:
			function = GXorInverted;
			break;

		case GDI_MERGEPAINT:
			function = GXorInverted;
			break;

		case GDI_SRCCOPY:
			function = GXcopy;
			break;

		case 0x00DD0228:
			function = GXorReverse;
			break;

		case GDI_SRCPAINT:
			function = GXor;
			break;

		case GDI_PATCOPY:
			function = GXcopy;
			break;

		case 0x00F50225:
			function = GXorReverse;
			break;

		case 0x00FA0089:
			function = GXor;
			break;

		case GDI_WHITENESS:
			function = GXset;
			break;

		default:
			break;
	}

	if (function < 0)
	{
		printf("Unsupported ROP3: 0x%08X\n", rop3);
		XSetFunction(xfi->display, xfi->gc, GXclear);
		return False;
	}

	XSetFunction(xfi->display, xfi->gc, function);

	return True;
}

void xf_bitmap_new(xfInfo* xfi, int width, int height, int bpp, uint8* data)
{

}

void xf_gdi_bitmap_update(rdpUpdate* update, BITMAP_UPDATE* bitmap)
{

}

void xf_gdi_palette_update(rdpUpdate* update, PALETTE_UPDATE* palette)
{

}

void xf_gdi_set_bounds(rdpUpdate* update, BOUNDS* bounds)
{

}

void xf_gdi_dstblt(rdpUpdate* update, DSTBLT_ORDER* dstblt)
{

}

void xf_gdi_patblt(rdpUpdate* update, PATBLT_ORDER* patblt)
{

}

void xf_gdi_scrblt(rdpUpdate* update, SCRBLT_ORDER* scrblt)
{

}

void xf_gdi_opaque_rect(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect)
{

}

void xf_gdi_multi_opaque_rect(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{

}

void xf_gdi_line_to(rdpUpdate* update, LINE_TO_ORDER* line_to)
{

}

void xf_gdi_polyline(rdpUpdate* update, POLYLINE_ORDER* polyline)
{

}

void xf_gdi_fast_index(rdpUpdate* update, FAST_INDEX_ORDER* fast_index)
{

}

void xf_gdi_create_offscreen_bitmap(rdpUpdate* update, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{

}

void xf_gdi_switch_surface(rdpUpdate* update, SWITCH_SURFACE_ORDER* switch_surface)
{

}

void xf_gdi_cache_bitmap_v2(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{

}

void xf_gdi_cache_color_table(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{

}

void xf_gdi_cache_glyph(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph)
{

}

void xf_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

void xf_gdi_cache_brush(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush)
{

}

void xf_gdi_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{

}

void xf_gdi_register_update_callbacks(rdpUpdate* update)
{
	update->Bitmap = xf_gdi_bitmap_update;
	update->Palette = xf_gdi_palette_update;
	update->SetBounds = xf_gdi_set_bounds;
	update->DstBlt = xf_gdi_dstblt;
	update->PatBlt = xf_gdi_patblt;
	update->ScrBlt = xf_gdi_scrblt;
	update->OpaqueRect = xf_gdi_opaque_rect;
	update->DrawNineGrid = NULL;
	update->MultiDstBlt = NULL;
	update->MultiPatBlt = NULL;
	update->MultiScrBlt = NULL;
	update->MultiOpaqueRect = xf_gdi_multi_opaque_rect;
	update->MultiDrawNineGrid = NULL;
	update->LineTo = xf_gdi_line_to;
	update->Polyline = NULL;
	update->MemBlt = NULL;
	update->Mem3Blt = NULL;
	update->SaveBitmap = NULL;
	update->GlyphIndex = NULL;
	update->FastIndex = xf_gdi_fast_index;
	update->FastGlyph = NULL;
	update->PolygonSC = NULL;
	update->PolygonCB = NULL;
	update->EllipseSC = NULL;
	update->EllipseCB = NULL;
	update->CreateOffscreenBitmap = xf_gdi_create_offscreen_bitmap;
	update->SwitchSurface = xf_gdi_switch_surface;

	update->CacheBitmapV2 = xf_gdi_cache_bitmap_v2;
	update->CacheColorTable = xf_gdi_cache_color_table;
	update->CacheGlyph = xf_gdi_cache_glyph;
	update->CacheGlyphV2 = xf_gdi_cache_glyph_v2;
	update->CacheBrush = xf_gdi_cache_brush;

	update->SurfaceBits = xf_gdi_surface_bits;
}

