/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CORE_UPDATE_H
#define FREERDP_LIB_CORE_UPDATE_H

#include "rdp.h"
#include "orders.h"

#include <freerdp/types.h>
#include <freerdp/update.h>
#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

#define UPDATE_TYPE_ORDERS 0x0000
#define UPDATE_TYPE_BITMAP 0x0001
#define UPDATE_TYPE_PALETTE 0x0002
#define UPDATE_TYPE_SYNCHRONIZE 0x0003

#define BITMAP_COMPRESSION 0x0001
#define NO_BITMAP_COMPRESSION_HDR 0x0400

typedef struct
{
	rdpUpdate common;

	wLog* log;

	BOOL dump_rfx;
	BOOL play_rfx;
	rdpPcap* pcap_rfx;
	BOOL initialState;

	BOOL asynchronous;
	rdpUpdateProxy* proxy;
	wMessageQueue* queue;

	wStream* us;
	UINT16 numberOrders;
	size_t offsetOrders; /* the offset to patch numberOrders in the stream */
	BOOL combineUpdates;
	rdpBounds currentBounds;
	rdpBounds previousBounds;
	CRITICAL_SECTION mux;
} rdp_update_internal;

typedef struct
{
	rdpAltSecUpdate common;

	CREATE_OFFSCREEN_BITMAP_ORDER create_offscreen_bitmap;
	SWITCH_SURFACE_ORDER switch_surface;
	CREATE_NINE_GRID_BITMAP_ORDER create_nine_grid_bitmap;
	FRAME_MARKER_ORDER frame_marker;
	STREAM_BITMAP_FIRST_ORDER stream_bitmap_first;
	STREAM_BITMAP_NEXT_ORDER stream_bitmap_next;
	DRAW_GDIPLUS_CACHE_FIRST_ORDER draw_gdiplus_cache_first;
	DRAW_GDIPLUS_CACHE_NEXT_ORDER draw_gdiplus_cache_next;
	DRAW_GDIPLUS_CACHE_END_ORDER draw_gdiplus_cache_end;
	DRAW_GDIPLUS_FIRST_ORDER draw_gdiplus_first;
	DRAW_GDIPLUS_NEXT_ORDER draw_gdiplus_next;
	DRAW_GDIPLUS_END_ORDER draw_gdiplus_end;
} rdp_altsec_update_internal;

typedef struct
{
	rdpPrimaryUpdate common;

	ORDER_INFO order_info;
	DSTBLT_ORDER dstblt;
	PATBLT_ORDER patblt;
	SCRBLT_ORDER scrblt;
	OPAQUE_RECT_ORDER opaque_rect;
	DRAW_NINE_GRID_ORDER draw_nine_grid;
	MULTI_DSTBLT_ORDER multi_dstblt;
	MULTI_PATBLT_ORDER multi_patblt;
	MULTI_SCRBLT_ORDER multi_scrblt;
	MULTI_OPAQUE_RECT_ORDER multi_opaque_rect;
	MULTI_DRAW_NINE_GRID_ORDER multi_draw_nine_grid;
	LINE_TO_ORDER line_to;
	POLYLINE_ORDER polyline;
	MEMBLT_ORDER memblt;
	MEM3BLT_ORDER mem3blt;
	SAVE_BITMAP_ORDER save_bitmap;
	GLYPH_INDEX_ORDER glyph_index;
	FAST_INDEX_ORDER fast_index;
	FAST_GLYPH_ORDER fast_glyph;
	POLYGON_SC_ORDER polygon_sc;
	POLYGON_CB_ORDER polygon_cb;
	ELLIPSE_SC_ORDER ellipse_sc;
	ELLIPSE_CB_ORDER ellipse_cb;
} rdp_primary_update_internal;

typedef struct
{
	rdpSecondaryUpdate common;
	BOOL glyph_v2;
} rdp_secondary_update_internal;

static INLINE rdp_update_internal* update_cast(rdpUpdate* update)
{
	union
	{
		rdpUpdate* pub;
		rdp_update_internal* internal;
	} cnv;

	WINPR_ASSERT(update);
	cnv.pub = update;
	return cnv.internal;
}

static INLINE rdp_altsec_update_internal* altsec_update_cast(rdpAltSecUpdate* update)
{
	union
	{
		rdpAltSecUpdate* pub;
		rdp_altsec_update_internal* internal;
	} cnv;

	WINPR_ASSERT(update);
	cnv.pub = update;
	return cnv.internal;
}

static INLINE rdp_primary_update_internal* primary_update_cast(rdpPrimaryUpdate* update)
{
	union
	{
		rdpPrimaryUpdate* pub;
		rdp_primary_update_internal* internal;
	} cnv;

	WINPR_ASSERT(update);
	cnv.pub = update;
	return cnv.internal;
}

static INLINE rdp_secondary_update_internal* secondary_update_cast(rdpSecondaryUpdate* update)
{
	union
	{
		rdpSecondaryUpdate* pub;
		rdp_secondary_update_internal* internal;
	} cnv;

	WINPR_ASSERT(update);
	cnv.pub = update;
	return cnv.internal;
}

FREERDP_LOCAL rdpUpdate* update_new(rdpRdp* rdp);
FREERDP_LOCAL void update_free(rdpUpdate* update);
FREERDP_LOCAL void update_reset_state(rdpUpdate* update);
FREERDP_LOCAL BOOL update_post_connect(rdpUpdate* update);
FREERDP_LOCAL void update_post_disconnect(rdpUpdate* update);

FREERDP_LOCAL BOOL update_recv_play_sound(rdpUpdate* update, wStream* s);
FREERDP_LOCAL BOOL update_recv_pointer(rdpUpdate* update, wStream* s);
FREERDP_LOCAL BOOL update_recv(rdpUpdate* update, wStream* s);

FREERDP_LOCAL BITMAP_UPDATE* update_read_bitmap_update(rdpUpdate* update, wStream* s);
FREERDP_LOCAL PALETTE_UPDATE* update_read_palette(rdpUpdate* update, wStream* s);

FREERDP_LOCAL POINTER_SYSTEM_UPDATE* update_read_pointer_system(rdpUpdate* update, wStream* s);
FREERDP_LOCAL POINTER_POSITION_UPDATE* update_read_pointer_position(rdpUpdate* update, wStream* s);
FREERDP_LOCAL POINTER_COLOR_UPDATE* update_read_pointer_color(rdpUpdate* update, wStream* s,
                                                              BYTE xorBpp);
FREERDP_LOCAL POINTER_LARGE_UPDATE* update_read_pointer_large(rdpUpdate* update, wStream* s);

FREERDP_LOCAL POINTER_NEW_UPDATE* update_read_pointer_new(rdpUpdate* update, wStream* s);
FREERDP_LOCAL POINTER_CACHED_UPDATE* update_read_pointer_cached(rdpUpdate* update, wStream* s);

FREERDP_LOCAL BOOL update_read_refresh_rect(rdpUpdate* update, wStream* s);
FREERDP_LOCAL BOOL update_read_suppress_output(rdpUpdate* update, wStream* s);
FREERDP_LOCAL void update_register_server_callbacks(rdpUpdate* update);
FREERDP_LOCAL void update_register_client_callbacks(rdpUpdate* update);
FREERDP_LOCAL int update_process_messages(rdpUpdate* update);

FREERDP_LOCAL BOOL update_begin_paint(rdpUpdate* update);
FREERDP_LOCAL BOOL update_end_paint(rdpUpdate* update);

#endif /* FREERDP_LIB_CORE_UPDATE_H */
