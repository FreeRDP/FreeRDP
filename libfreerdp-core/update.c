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

#include "update.h"
#include "surface.h"
#include <freerdp/utils/rect.h>
#include <freerdp/codec/bitmap.h>

uint8 UPDATE_TYPE_STRINGS[][32] =
{
	"Orders",
	"Bitmap",
	"Palette",
	"Synchronize"
};

void update_recv_orders(rdpUpdate* update, STREAM* s)
{
	uint16 numberOrders;

	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_read_uint16(s, numberOrders); /* numberOrders (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsB (2 bytes) */

	while (numberOrders > 0)
	{
		update_recv_order(update, s);
		numberOrders--;
	}
}

void update_read_bitmap_data(STREAM* s, BITMAP_DATA* bitmap_data)
{
	stream_read_uint16(s, bitmap_data->destLeft);
	stream_read_uint16(s, bitmap_data->destTop);
	stream_read_uint16(s, bitmap_data->destRight);
	stream_read_uint16(s, bitmap_data->destBottom);
	stream_read_uint16(s, bitmap_data->width);
	stream_read_uint16(s, bitmap_data->height);
	stream_read_uint16(s, bitmap_data->bitsPerPixel);
	stream_read_uint16(s, bitmap_data->flags);
	stream_read_uint16(s, bitmap_data->bitmapLength);

	if (bitmap_data->flags & BITMAP_COMPRESSION)
	{
		uint16 cbCompMainBodySize;
		uint16 cbUncompressedSize;

		stream_seek_uint16(s); /* cbCompFirstRowSize (2 bytes) */
		stream_read_uint16(s, cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
		stream_seek_uint16(s); /* cbScanWidth (2 bytes) */
		stream_read_uint16(s, cbUncompressedSize); /* cbUncompressedSize (2 bytes) */

		bitmap_data->bitmapLength = cbCompMainBodySize;

		bitmap_data->compressed = true;
		stream_get_mark(s, bitmap_data->bitmapDataStream);
		stream_seek(s, bitmap_data->bitmapLength);
	}
	else
	{
		bitmap_data->compressed = false;
		stream_get_mark(s, bitmap_data->bitmapDataStream);
		stream_seek(s, bitmap_data->bitmapLength);
	}
}

void update_read_bitmap(rdpUpdate* update, STREAM* s, BITMAP_UPDATE* bitmap_update)
{
	int i;

	stream_read_uint16(s, bitmap_update->number); /* numberRectangles (2 bytes) */

	if (bitmap_update->number > bitmap_update->count)
	{
		uint16 count;

		count = bitmap_update->number * 2;

		bitmap_update->rectangles = (BITMAP_DATA*) xrealloc(bitmap_update->rectangles,
				sizeof(BITMAP_DATA) * count);

		memset(&bitmap_update->rectangles[bitmap_update->count], 0,
				sizeof(BITMAP_DATA) * (count - bitmap_update->count));

		bitmap_update->count = count;
	}

	/* rectangles */
	for (i = 0; i < bitmap_update->number; i++)
	{
		update_read_bitmap_data(s, &bitmap_update->rectangles[i]);
	}
}

void update_read_palette(rdpUpdate* update, STREAM* s, PALETTE_UPDATE* palette_update)
{
	int i;
	PALETTE_ENTRY* entry;

	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
	stream_read_uint32(s, palette_update->number); /* numberColors (4 bytes), must be set to 256 */

	if (palette_update->number > 256)
		palette_update->number = 256;

	/* paletteEntries */
	for (i = 0; i < (int) palette_update->number; i++)
	{
		entry = &palette_update->entries[i];

		stream_read_uint8(s, entry->blue);
		stream_read_uint8(s, entry->green);
		stream_read_uint8(s, entry->red);
	}
}

void update_read_synchronize(rdpUpdate* update, STREAM* s)
{
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */

	/**
	 * The Synchronize Update is an artifact from the
	 * T.128 protocol and should be ignored.
	 */
}

void update_read_play_sound(STREAM* s, PLAY_SOUND_UPDATE* play_sound)
{
	stream_read_uint32(s, play_sound->duration); /* duration (4 bytes) */
	stream_read_uint32(s, play_sound->frequency); /* frequency (4 bytes) */
}

void update_recv_play_sound(rdpUpdate* update, STREAM* s)
{
	update_read_play_sound(s, &update->play_sound);
	IFCALL(update->PlaySound, update, &update->play_sound);
}

void update_read_pointer_position(STREAM* s, POINTER_POSITION_UPDATE* pointer_position)
{
	stream_read_uint16(s, pointer_position->xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, pointer_position->yPos); /* yPos (2 bytes) */
}

void update_read_pointer_system(STREAM* s, POINTER_SYSTEM_UPDATE* pointer_system)
{
	stream_read_uint32(s, pointer_system->type); /* systemPointerType (4 bytes) */
}

void update_read_pointer_color(STREAM* s, POINTER_COLOR_UPDATE* pointer_color)
{
	stream_read_uint16(s, pointer_color->cacheIndex); /* cacheIndex (2 bytes) */
	stream_read_uint16(s, pointer_color->xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, pointer_color->yPos); /* yPos (2 bytes) */
	stream_read_uint16(s, pointer_color->width); /* width (2 bytes) */
	stream_read_uint16(s, pointer_color->height); /* height (2 bytes) */
	stream_read_uint16(s, pointer_color->lengthAndMask); /* lengthAndMask (2 bytes) */
	stream_read_uint16(s, pointer_color->lengthXorMask); /* lengthXorMask (2 bytes) */

	if (pointer_color->lengthXorMask > 0)
	{
		pointer_color->xorMaskData = (uint8*) xmalloc(pointer_color->lengthXorMask);
		stream_read(s, pointer_color->xorMaskData, pointer_color->lengthXorMask);
	}

	if (pointer_color->lengthAndMask > 0)
	{
		pointer_color->andMaskData = (uint8*) xmalloc(pointer_color->lengthAndMask);
		stream_read(s, pointer_color->andMaskData, pointer_color->lengthAndMask);
	}

	stream_seek_uint8(s); /* pad (1 byte) */
}

void update_read_pointer_new(STREAM* s, POINTER_NEW_UPDATE* pointer_new)
{
	stream_read_uint16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */
	update_read_pointer_color(s, &pointer_new->colorPtrAttr); /* colorPtrAttr */
}

void update_read_pointer_cached(STREAM* s, POINTER_CACHED_UPDATE* pointer_cached)
{
	stream_read_uint16(s, pointer_cached->cacheIndex); /* cacheIndex (2 bytes) */
}

void update_recv_pointer(rdpUpdate* update, STREAM* s)
{
	uint16 messageType;
	rdpContext* context = update->context;
	rdpPointerUpdate* pointer = update->pointer;

	stream_read_uint16(s, messageType); /* messageType (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */

	switch (messageType)
	{
		case PTR_MSG_TYPE_POSITION:
			update_read_pointer_position(s, &pointer->pointer_position);
			IFCALL(pointer->PointerPosition, context, &pointer->pointer_position);
			break;

		case PTR_MSG_TYPE_SYSTEM:
			update_read_pointer_system(s, &pointer->pointer_system);
			IFCALL(pointer->PointerSystem, context, &pointer->pointer_system);
			break;

		case PTR_MSG_TYPE_COLOR:
			update_read_pointer_color(s, &pointer->pointer_color);
			IFCALL(pointer->PointerColor, context, &pointer->pointer_color);
			break;

		case PTR_MSG_TYPE_POINTER:
			update_read_pointer_new(s, &pointer->pointer_new);
			IFCALL(pointer->PointerNew, context, &pointer->pointer_new);
			break;

		case PTR_MSG_TYPE_CACHED:
			update_read_pointer_cached(s, &pointer->pointer_cached);
			IFCALL(pointer->PointerCached, context, &pointer->pointer_cached);
			break;

		default:
			break;
	}
}

void update_recv(rdpUpdate* update, STREAM* s)
{
	uint16 updateType;

	stream_read_uint16(s, updateType); /* updateType (2 bytes) */

	//printf("%s Update Data PDU\n", UPDATE_TYPE_STRINGS[updateType]);

	IFCALL(update->BeginPaint, update);

	switch (updateType)
	{
		case UPDATE_TYPE_ORDERS:
			update_recv_orders(update, s);
			break;

		case UPDATE_TYPE_BITMAP:
			update_read_bitmap(update, s, &update->bitmap_update);
			IFCALL(update->BitmapUpdate, update, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			update_read_palette(update, s, &update->palette_update);
			IFCALL(update->Palette, update, &update->palette_update);
			break;

		case UPDATE_TYPE_SYNCHRONIZE:
			update_read_synchronize(update, s);
			IFCALL(update->Synchronize, update);
			break;
	}

	IFCALL(update->EndPaint, update);

	if (stream_get_left(s) > RDP_SHARE_DATA_HEADER_LENGTH)
	{
		uint8 type;
		uint16 pduType;
		uint16 length;
		uint16 source;
		uint32 shareId;
		uint8 compressed_type;
		uint16 compressed_len;

		rdp_read_share_control_header(s, &length, &pduType, &source);

		if (pduType != PDU_TYPE_DATA)
			return;

		rdp_read_share_data_header(s, &length, &type, &shareId, &compressed_type, &compressed_len);

		if (type == DATA_PDU_TYPE_UPDATE)
			update_recv(update, s);
	}
}

void update_reset_state(rdpUpdate* update)
{
	memset(&update->order_info, 0, sizeof(ORDER_INFO));
	memset(&update->dstblt, 0, sizeof(DSTBLT_ORDER));
	memset(&update->patblt, 0, sizeof(PATBLT_ORDER));
	memset(&update->scrblt, 0, sizeof(SCRBLT_ORDER));
	memset(&update->opaque_rect, 0, sizeof(OPAQUE_RECT_ORDER));
	memset(&update->draw_nine_grid, 0, sizeof(DRAW_NINE_GRID_ORDER));
	memset(&update->multi_dstblt, 0, sizeof(MULTI_DSTBLT_ORDER));
	memset(&update->multi_patblt, 0, sizeof(MULTI_PATBLT_ORDER));
	memset(&update->multi_scrblt, 0, sizeof(MULTI_SCRBLT_ORDER));
	memset(&update->multi_opaque_rect, 0, sizeof(MULTI_OPAQUE_RECT_ORDER));
	memset(&update->multi_draw_nine_grid, 0, sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	memset(&update->line_to, 0, sizeof(LINE_TO_ORDER));
	memset(&update->polyline, 0, sizeof(POLYLINE_ORDER));
	memset(&update->memblt, 0, sizeof(MEMBLT_ORDER));
	memset(&update->mem3blt, 0, sizeof(MEM3BLT_ORDER));
	memset(&update->save_bitmap, 0, sizeof(SAVE_BITMAP_ORDER));
	memset(&update->glyph_index, 0, sizeof(GLYPH_INDEX_ORDER));
	memset(&update->fast_index, 0, sizeof(FAST_INDEX_ORDER));
	memset(&update->fast_glyph, 0, sizeof(FAST_GLYPH_ORDER));
	memset(&update->polygon_sc, 0, sizeof(POLYGON_SC_ORDER));
	memset(&update->polygon_cb, 0, sizeof(POLYGON_CB_ORDER));
	memset(&update->ellipse_sc, 0, sizeof(ELLIPSE_SC_ORDER));
	memset(&update->ellipse_cb, 0, sizeof(ELLIPSE_CB_ORDER));

	update->order_info.orderType = ORDER_TYPE_PATBLT;
	update->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(update->SwitchSurface, update, &(update->switch_surface));
}

static void update_begin_paint(rdpUpdate* update)
{

}

static void update_end_paint(rdpUpdate* update)
{

}

static void update_write_refresh_rect(STREAM* s, uint8 count, RECTANGLE_16* areas)
{
	int i;

	stream_write_uint8(s, count); /* numberOfAreas (1 byte) */
	stream_seek(s, 3); /* pad3Octets (3 bytes) */

	for (i = 0; i < count; i++)
		freerdp_write_rectangle_16(s, &areas[i]);
}

static void update_send_refresh_rect(rdpUpdate* update, uint8 count, RECTANGLE_16* areas)
{
	STREAM* s;
	rdpRdp* rdp = update->context->rdp;

	s = rdp_data_pdu_init(rdp);
	update_write_refresh_rect(s, count, areas);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_REFRESH_RECT, rdp->mcs->user_id);
}

static void update_write_suppress_output(STREAM* s, uint8 allow, RECTANGLE_16* area)
{
	stream_write_uint8(s, allow); /* allowDisplayUpdates (1 byte) */
	stream_seek(s, 3); /* pad3Octets (3 bytes) */

	if (allow > 0)
		freerdp_write_rectangle_16(s, area);
}

static void update_send_suppress_output(rdpUpdate* update, uint8 allow, RECTANGLE_16* area)
{
	STREAM* s;
	rdpRdp* rdp = update->context->rdp;

	s = rdp_data_pdu_init(rdp);
	update_write_suppress_output(s, allow, area);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SUPPRESS_OUTPUT, rdp->mcs->user_id);
}

static void update_send_surface_command(rdpUpdate* update, STREAM* s)
{
	rdpRdp* rdp = update->context->rdp;
	fastpath_send_fragmented_update_pdu(rdp->fastpath, s);
}

static void update_send_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{
	rdpRdp* rdp = update->context->rdp;
	fastpath_send_surfcmd_surface_bits(rdp->fastpath, surface_bits_command);
}

static void update_send_synchronize(rdpUpdate* update)
{
	STREAM* s;
	rdpRdp* rdp = update->context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_write_uint8(s, FASTPATH_UPDATETYPE_SYNCHRONIZE); /* updateHeader (1 byte) */
	stream_write_uint16(s, 0); /* size (2 bytes) */
	fastpath_send_update_pdu(rdp->fastpath, s);
}

static void update_send_desktop_resize(rdpUpdate* update)
{
	rdpRdp* rdp = update->context->rdp;

	rdp_server_reactivate(rdp);
}

static void update_send_pointer_system(rdpUpdate* update, POINTER_SYSTEM_UPDATE* pointer_system)
{
	STREAM* s;
	rdpRdp* rdp = update->context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	/* updateHeader (1 byte) */
	if (pointer_system->type == SYSPTR_NULL)
		stream_write_uint8(s, FASTPATH_UPDATETYPE_PTR_NULL);
	else
		stream_write_uint8(s, FASTPATH_UPDATETYPE_PTR_DEFAULT);
	stream_write_uint16(s, 0); /* size (2 bytes) */
	fastpath_send_update_pdu(rdp->fastpath, s);
}

void update_register_server_callbacks(rdpUpdate* update)
{
	update->BeginPaint = update_begin_paint;
	update->EndPaint = update_end_paint;
	update->Synchronize = update_send_synchronize;
	update->DesktopResize = update_send_desktop_resize;
	update->RefreshRect = update_send_refresh_rect;
	update->SuppressOutput = update_send_suppress_output;
	update->SurfaceBits = update_send_surface_bits;
	update->SurfaceCommand = update_send_surface_command;
	update->pointer->PointerSystem = update_send_pointer_system;
}

rdpUpdate* update_new(rdpRdp* rdp)
{
	rdpUpdate* update;

	update = (rdpUpdate*) xzalloc(sizeof(rdpUpdate));

	if (update != NULL)
	{
		update->bitmap_update.count = 64;
		update->bitmap_update.rectangles = (BITMAP_DATA*) xzalloc(sizeof(BITMAP_DATA) * update->bitmap_update.count);

		update->pointer = xnew(rdpPointerUpdate);
	}

	return update;
}

void update_free(rdpUpdate* update)
{
	if (update != NULL)
	{
		xfree(update->bitmap_update.rectangles);
		xfree(update);
	}
}

