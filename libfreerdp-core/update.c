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

/*
static const char* const UPDATE_TYPE_STRINGS[] =
{
	"Orders",
	"Bitmap",
	"Palette",
	"Synchronize"
};
*/

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
		if (!(bitmap_data->flags & NO_BITMAP_COMPRESSION_HDR))
		{
			stream_read_uint16(s, bitmap_data->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			stream_read_uint16(s, bitmap_data->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			stream_read_uint16(s, bitmap_data->cbScanWidth); /* cbScanWidth (2 bytes) */
			stream_read_uint16(s, bitmap_data->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			bitmap_data->bitmapLength = bitmap_data->cbCompMainBodySize;
		}

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
	for (i = 0; i < (int) bitmap_update->number; i++)
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
	IFCALL(update->PlaySound, update->context, &update->play_sound);
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

	if (stream_get_left(s) > 0)
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
	rdpContext* context = update->context;

	stream_read_uint16(s, updateType); /* updateType (2 bytes) */

	//printf("%s Update Data PDU\n", UPDATE_TYPE_STRINGS[updateType]);

	IFCALL(update->BeginPaint, context);

	switch (updateType)
	{
		case UPDATE_TYPE_ORDERS:
			update_recv_orders(update, s);
			break;

		case UPDATE_TYPE_BITMAP:
			update_read_bitmap(update, s, &update->bitmap_update);
			IFCALL(update->BitmapUpdate, context, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			update_read_palette(update, s, &update->palette_update);
			IFCALL(update->Palette, context, &update->palette_update);
			break;

		case UPDATE_TYPE_SYNCHRONIZE:
			update_read_synchronize(update, s);
			IFCALL(update->Synchronize, context);
			break;
	}

	IFCALL(update->EndPaint, context);

	if (stream_get_left(s) > RDP_SHARE_DATA_HEADER_LENGTH)
	{
		uint16 pduType;
		uint16 length;
		uint16 source;

		rdp_read_share_control_header(s, &length, &pduType, &source);

		if (pduType != PDU_TYPE_DATA)
			return;

		rdp_recv_data_pdu(update->context->rdp, s);
	}
}

void update_reset_state(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;
	rdpAltSecUpdate* altsec = update->altsec;

	memset(&primary->order_info, 0, sizeof(ORDER_INFO));
	memset(&primary->dstblt, 0, sizeof(DSTBLT_ORDER));
	memset(&primary->patblt, 0, sizeof(PATBLT_ORDER));
	memset(&primary->scrblt, 0, sizeof(SCRBLT_ORDER));
	memset(&primary->opaque_rect, 0, sizeof(OPAQUE_RECT_ORDER));
	memset(&primary->draw_nine_grid, 0, sizeof(DRAW_NINE_GRID_ORDER));
	memset(&primary->multi_dstblt, 0, sizeof(MULTI_DSTBLT_ORDER));
	memset(&primary->multi_patblt, 0, sizeof(MULTI_PATBLT_ORDER));
	memset(&primary->multi_scrblt, 0, sizeof(MULTI_SCRBLT_ORDER));
	memset(&primary->multi_opaque_rect, 0, sizeof(MULTI_OPAQUE_RECT_ORDER));
	memset(&primary->multi_draw_nine_grid, 0, sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	memset(&primary->line_to, 0, sizeof(LINE_TO_ORDER));
	memset(&primary->polyline, 0, sizeof(POLYLINE_ORDER));
	memset(&primary->memblt, 0, sizeof(MEMBLT_ORDER));
	memset(&primary->mem3blt, 0, sizeof(MEM3BLT_ORDER));
	memset(&primary->save_bitmap, 0, sizeof(SAVE_BITMAP_ORDER));
	memset(&primary->glyph_index, 0, sizeof(GLYPH_INDEX_ORDER));
	memset(&primary->fast_index, 0, sizeof(FAST_INDEX_ORDER));
	memset(&primary->fast_glyph, 0, sizeof(FAST_GLYPH_ORDER));
	memset(&primary->polygon_sc, 0, sizeof(POLYGON_SC_ORDER));
	memset(&primary->polygon_cb, 0, sizeof(POLYGON_CB_ORDER));
	memset(&primary->ellipse_sc, 0, sizeof(ELLIPSE_SC_ORDER));
	memset(&primary->ellipse_cb, 0, sizeof(ELLIPSE_CB_ORDER));

	primary->order_info.orderType = ORDER_TYPE_PATBLT;
	altsec->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(altsec->SwitchSurface, update->context, &(altsec->switch_surface));
}

static void update_begin_paint(rdpContext* context)
{

}

static void update_end_paint(rdpContext* context)
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

static void update_send_refresh_rect(rdpContext* context, uint8 count, RECTANGLE_16* areas)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

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

static void update_send_suppress_output(rdpContext* context, uint8 allow, RECTANGLE_16* area)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = rdp_data_pdu_init(rdp);
	update_write_suppress_output(s, allow, area);

	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SUPPRESS_OUTPUT, rdp->mcs->user_id);
}

static void update_send_surface_command(rdpContext* context, STREAM* s)
{
	STREAM* update;
	rdpRdp* rdp = context->rdp;

	update = fastpath_update_pdu_init(rdp->fastpath);
	stream_check_size(update, stream_get_length(s));
	stream_write(update, stream_get_head(s), stream_get_length(s));
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, update);
}

static void update_send_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_check_size(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH + (int) surface_bits_command->bitmapDataLength);
	update_write_surfcmd_surface_bits_header(s, surface_bits_command);
	stream_write(s, surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s);
}

static void update_send_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	update_write_surfcmd_frame_marker(s, surface_frame_marker->frameAction, surface_frame_marker->frameId);
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s);
}

static void update_send_synchronize(rdpContext* context)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_write_zero(s, 2); /* pad2Octets (2 bytes) */
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SYNCHRONIZE, s);
}

static void update_send_desktop_resize(rdpContext* context)
{
	rdp_server_reactivate(context->rdp);
}

static void update_send_scrblt(rdpContext* context, SCRBLT_ORDER* scrblt)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);

	stream_write_uint16(s, 1); /* numberOrders (2 bytes) */
	stream_write_uint8(s, ORDER_STANDARD | ORDER_TYPE_CHANGE); /* controlFlags (1 byte) */
	stream_write_uint8(s, ORDER_TYPE_SCRBLT); /* orderType (1 byte) */
	stream_write_uint8(s, 0x7F); /* fieldFlags (variable) */

	stream_write_uint16(s, scrblt->nLeftRect);
	stream_write_uint16(s, scrblt->nTopRect);
	stream_write_uint16(s, scrblt->nWidth);
	stream_write_uint16(s, scrblt->nHeight);
	stream_write_uint8(s, scrblt->bRop);
	stream_write_uint16(s, scrblt->nXSrc);
	stream_write_uint16(s, scrblt->nYSrc);

	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_ORDERS, s);
}

static void update_send_pointer_system(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer_system)
{
	STREAM* s;
	uint8 updateCode;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	if (pointer_system->type == SYSPTR_NULL)
		updateCode = FASTPATH_UPDATETYPE_PTR_NULL;
	else
		updateCode = FASTPATH_UPDATETYPE_PTR_DEFAULT;
	fastpath_send_update_pdu(rdp->fastpath, updateCode, s);
}

static void update_write_pointer_color(STREAM* s, POINTER_COLOR_UPDATE* pointer_color)
{
	stream_check_size(s, 15 + (int) pointer_color->lengthAndMask + (int) pointer_color->lengthXorMask);
	stream_write_uint16(s, pointer_color->cacheIndex);
	stream_write_uint16(s, pointer_color->xPos);
	stream_write_uint16(s, pointer_color->yPos);
	stream_write_uint16(s, pointer_color->width);
	stream_write_uint16(s, pointer_color->height);
	stream_write_uint16(s, pointer_color->lengthAndMask);
	stream_write_uint16(s, pointer_color->lengthXorMask);
	if (pointer_color->lengthXorMask > 0)
		stream_write(s, pointer_color->xorMaskData, pointer_color->lengthXorMask);
	if (pointer_color->lengthAndMask > 0)
		stream_write(s, pointer_color->andMaskData, pointer_color->lengthAndMask);
	stream_write_uint8(s, 0); /* pad (1 byte) */
}

static void update_send_pointer_color(rdpContext* context, POINTER_COLOR_UPDATE* pointer_color)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
        update_write_pointer_color(s, pointer_color);
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_COLOR, s);
}

static void update_send_pointer_new(rdpContext* context, POINTER_NEW_UPDATE* pointer_new)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_write_uint16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */
        update_write_pointer_color(s, &pointer_new->colorPtrAttr);
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_POINTER, s);
}

static void update_send_pointer_cached(rdpContext* context, POINTER_CACHED_UPDATE* pointer_cached)
{
	STREAM* s;
	rdpRdp* rdp = context->rdp;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_write_uint16(s, pointer_cached->cacheIndex); /* cacheIndex (2 bytes) */
	fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_CACHED, s);
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
	update->SurfaceFrameMarker = update_send_surface_frame_marker;
	update->SurfaceCommand = update_send_surface_command;
	update->primary->ScrBlt = update_send_scrblt;
	update->pointer->PointerSystem = update_send_pointer_system;
	update->pointer->PointerColor = update_send_pointer_color;
	update->pointer->PointerNew = update_send_pointer_new;
	update->pointer->PointerCached = update_send_pointer_cached;
}

rdpUpdate* update_new(rdpRdp* rdp)
{
	rdpUpdate* update;

	update = (rdpUpdate*) xzalloc(sizeof(rdpUpdate));

	if (update != NULL)
	{
		OFFSCREEN_DELETE_LIST* deleteList;

		update->bitmap_update.count = 64;
		update->bitmap_update.rectangles = (BITMAP_DATA*) xzalloc(sizeof(BITMAP_DATA) * update->bitmap_update.count);

		update->pointer = xnew(rdpPointerUpdate);
		update->primary = xnew(rdpPrimaryUpdate);
		update->secondary = xnew(rdpSecondaryUpdate);
		update->altsec = xnew(rdpAltSecUpdate);
		update->window = xnew(rdpWindowUpdate);

		deleteList = &(update->altsec->create_offscreen_bitmap.deleteList);
		deleteList->sIndices = 64;
		deleteList->indices = xmalloc(deleteList->sIndices * 2);
		deleteList->cIndices = 0;

		update->SuppressOutput = update_send_suppress_output;
	}

	return update;
}

void update_free(rdpUpdate* update)
{
	if (update != NULL)
	{
		OFFSCREEN_DELETE_LIST* deleteList;
		deleteList = &(update->altsec->create_offscreen_bitmap.deleteList);
		xfree(deleteList->indices);

		xfree(update->bitmap_update.rectangles);
		xfree(update->pointer);
		xfree(update->primary);
		xfree(update->secondary);
		xfree(update->altsec);
		xfree(update->window);
		xfree(update);
	}
}

