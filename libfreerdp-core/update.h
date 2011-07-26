/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Update Data PDUs
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

#ifndef __UPDATE_H
#define __UPDATE_H

#include "rdp.h"
#include "orders.h"
#include <freerdp/freerdp.h>
#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

#define UPDATE_TYPE_ORDERS		0x0000
#define UPDATE_TYPE_BITMAP		0x0001
#define UPDATE_TYPE_PALETTE		0x0002
#define UPDATE_TYPE_SYNCHRONIZE		0x0003

typedef struct
{
	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
	uint16 width;
	uint16 height;
	uint16 bpp;
	uint16 flags;
	uint16 length;
	uint8* data;
} BITMAP_DATA;

typedef struct
{
	uint16 number;
	BITMAP_DATA* bitmaps;
} BITMAP_UPDATE;

#define BITMAP_COMPRESSION		0x0001
#define NO_BITMAP_COMPRESSION_HDR	0x0400

typedef struct
{
	uint32 number;
	uint32 entries[256];
} PALETTE_UPDATE;

typedef struct rdp_update rdpUpdate;

typedef int (*pcBitmap)(rdpUpdate* update, BITMAP_UPDATE* bitmap);
typedef int (*pcDstBlt)(rdpUpdate* update, DSTBLT_ORDER* dstblt);
typedef int (*pcPatBlt)(rdpUpdate* update, PATBLT_ORDER* patblt);
typedef int (*pcScrBlt)(rdpUpdate* update, PATBLT_ORDER* scrblt);
typedef int (*pcDrawNineGrid)(rdpUpdate* update, DRAW_NINE_GRID_ORDER* draw_nine_grid);
typedef int (*pcMultiDrawNineGrid)(rdpUpdate* update, MULTI_DRAW_NINE_GRID_ORDER* multi_draw_nine_grid);
typedef int (*pcLineTo)(rdpUpdate* update, LINE_TO_ORDER* line_to);
typedef int (*pcOpaqueRect)(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect);
typedef int (*pcSaveBitmap)(rdpUpdate* update, SAVE_BITMAP_ORDER* save_bitmap);
typedef int (*pcMemBlt)(rdpUpdate* update, MEMBLT_ORDER* memblt);
typedef int (*pcMem3Blt)(rdpUpdate* update, MEM3BLT_ORDER* memblt);
typedef int (*pcMultiDstBlt)(rdpUpdate* update, MULTI_DSTBLT_ORDER* dstblt);
typedef int (*pcMultiPatBlt)(rdpUpdate* update, MULTI_PATBLT_ORDER* patblt);
typedef int (*pcMultiScrBlt)(rdpUpdate* update, MULTI_PATBLT_ORDER* scrblt);
typedef int (*pcMultiOpaqueRect)(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect);
typedef int (*pcFastIndex)(rdpUpdate* update, FAST_INDEX_ORDER* fast_index);
typedef int (*pcPolygonSC)(rdpUpdate* update, POLYGON_SC_ORDER* polygon_sc);
typedef int (*pcPolygonCB)(rdpUpdate* update, POLYGON_CB_ORDER* polygon_cb);
typedef int (*pcPolyline)(rdpUpdate* update, POLYLINE_ORDER* polyline);
typedef int (*pcFastGlyph)(rdpUpdate* update, FAST_GLYPH_ORDER* fast_glyph);
typedef int (*pcEllipseSC)(rdpUpdate* update, ELLIPSE_SC_ORDER* ellipse_sc);
typedef int (*pcEllipseCB)(rdpUpdate* update, ELLIPSE_CB_ORDER* ellipse_cb);
typedef int (*pcGlyphIndex)(rdpUpdate* update, GLYPH_INDEX_ORDER* glyph_index);

struct rdp_update
{
	BITMAP_UPDATE bitmap_update;
	PALETTE_UPDATE palette_update;

	pcBitmap Bitmap;
	pcDstBlt DstBlt;
	pcPatBlt PatBlt;
	pcScrBlt ScrBlt;
	pcDrawNineGrid DrawNineGrid;
	pcMultiDrawNineGrid MultiDrawNineGrid;
	pcLineTo LineTo;
	pcOpaqueRect OpaqueRect;
	pcSaveBitmap SaveBitmap;
	pcMemBlt MemBlt;
	pcMem3Blt Mem3Blt;
	pcMultiDstBlt MultiDstBlt;
	pcMultiPatBlt MultiPatBlt;
	pcMultiScrBlt MultiScrBlt;
	pcMultiOpaqueRect MultiOpaqueRect;
	pcFastIndex FastIndex;
	pcPolygonSC PolygonSC;
	pcPolygonCB PolygonCB;
	pcPolyline Polyline;
	pcFastGlyph FastGlyph;
	pcEllipseSC EllipseSC;
	pcEllipseCB EllipseCB;
	pcGlyphIndex GlyphIndex;
};

rdpUpdate* update_new();
void update_free(rdpUpdate* update);

void rdp_read_bitmap_update(rdpRdp* rdp, STREAM* s, BITMAP_UPDATE* bitmap_update);
void rdp_read_palette_update(rdpRdp* rdp, STREAM* s, PALETTE_UPDATE* palette_update);
void rdp_recv_update_data_pdu(rdpRdp* rdp, STREAM* s);

#endif /* __UPDATE_H */
