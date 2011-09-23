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
#include <freerdp/common/bitmap.h>

uint8 UPDATE_TYPE_STRINGS[][32] =
{
	"Orders",
	"Bitmap",
	"Palette",
	"Synchronize"
};

void update_free_bitmap(BITMAP_UPDATE* bitmap_update)
{
	int i;

	for (i = 0; i < bitmap_update->number; i++)
	{
		xfree(bitmap_update->bitmaps[i].data);
	}
	xfree(bitmap_update->bitmaps);
}

void update_recv_orders(rdpUpdate* update, STREAM* s)
{
	uint16 numberOrders;

	stream_seek_uint16(s); /* pad2OctetsA (2 bytes) */
	stream_read_uint16(s, numberOrders); /* numberOrders (2 bytes) */
	stream_seek_uint16(s); /* pad2OctetsB (2 bytes) */

	printf("numberOrders:%d\n", numberOrders);

	while (numberOrders > 0)
	{
		update_recv_order(update, s);
		numberOrders--;
	}
}

void update_read_bitmap_data(STREAM* s, BITMAP_DATA* bitmap_data)
{
	uint8* srcData;
	uint16 dstSize;
	boolean status;
	uint16 bytesPerPixel;

	stream_read_uint16(s, bitmap_data->left);
	stream_read_uint16(s, bitmap_data->top);
	stream_read_uint16(s, bitmap_data->right);
	stream_read_uint16(s, bitmap_data->bottom);
	stream_read_uint16(s, bitmap_data->width);
	stream_read_uint16(s, bitmap_data->height);
	stream_read_uint16(s, bitmap_data->bpp);
	stream_read_uint16(s, bitmap_data->flags);
	stream_read_uint16(s, bitmap_data->length);

	bytesPerPixel = (bitmap_data->bpp + 7) / 8;

	if (bitmap_data->flags & BITMAP_COMPRESSION)
	{
		uint16 cbCompMainBodySize;
		uint16 cbUncompressedSize;

		stream_seek_uint16(s); /* cbCompFirstRowSize (2 bytes) */
		stream_read_uint16(s, cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
		stream_seek_uint16(s); /* cbScanWidth (2 bytes) */
		stream_read_uint16(s, cbUncompressedSize); /* cbUncompressedSize (2 bytes) */

		dstSize = cbUncompressedSize;
		bitmap_data->length = cbCompMainBodySize;
		bitmap_data->data = (uint8*) xmalloc(dstSize);

		stream_get_mark(s, srcData);
		stream_seek(s, bitmap_data->length);

		status = bitmap_decompress(srcData, bitmap_data->data, bitmap_data->width, bitmap_data->height,
				bitmap_data->length, bitmap_data->bpp, bitmap_data->bpp);

		if (status != True)
			printf("bitmap decompression failed, bpp:%d\n", bitmap_data->bpp);
	}
	else
	{
		int y;
		int offset;
		int scanline;
		stream_get_mark(s, srcData);
		dstSize = bitmap_data->length;
		bitmap_data->data = (uint8*) xzalloc(dstSize);
		scanline = bitmap_data->width * (bitmap_data->bpp / 8);

		for (y = 0; y < bitmap_data->height; y++)
		{
			offset = (bitmap_data->height - y - 1) * scanline;
			stream_read(s, &bitmap_data->data[offset], scanline);
		}
	}
}

void update_read_bitmap(rdpUpdate* update, STREAM* s, BITMAP_UPDATE* bitmap_update)
{
	int i;

	update_free_bitmap(bitmap_update);
	stream_read_uint16(s, bitmap_update->number); /* numberRectangles (2 bytes) */

	bitmap_update->bitmaps = (BITMAP_DATA*) xzalloc(sizeof(BITMAP_DATA) * bitmap_update->number);

	/* rectangles */
	for (i = 0; i < bitmap_update->number; i++)
	{
		update_read_bitmap_data(s, &bitmap_update->bitmaps[i]);
	}
}

void update_read_palette(rdpUpdate* update, STREAM* s, PALETTE_UPDATE* palette_update)
{
	int i;
	uint8 byte;
	uint32 color;

	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
	stream_read_uint32(s, palette_update->number); /* numberColors (4 bytes), must be set to 256 */

	if (palette_update->number > 256)
		palette_update->number = 256;

	/* paletteEntries */
	for (i = 0; i < (int) palette_update->number; i++)
	{
		stream_read_uint8(s, byte);
		color = byte;
		stream_read_uint8(s, byte);
		color |= (byte << 8);
		stream_read_uint8(s, byte);
		color |= (byte << 16);
		palette_update->entries[i] = color;
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
	stream_read_uint32(s, pointer_color->hotSpot); /* hotSpot (4 bytes) */
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

	stream_read_uint16(s, messageType); /* messageType (2 bytes) */
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */

	switch (messageType)
	{
		case PTR_MSG_TYPE_POSITION:
			update_read_pointer_position(s, &update->pointer_position);
			IFCALL(update->PointerPosition, update, &update->pointer_position);
			break;

		case PTR_MSG_TYPE_SYSTEM:
			update_read_pointer_system(s, &update->pointer_system);
			IFCALL(update->PointerSystem, update, &update->pointer_system);
			break;

		case PTR_MSG_TYPE_COLOR:
			update_read_pointer_color(s, &update->pointer_color);
			IFCALL(update->PointerColor, update, &update->pointer_color);
			break;

		case PTR_MSG_TYPE_POINTER:
			update_read_pointer_new(s, &update->pointer_new);
			IFCALL(update->PointerNew, update, &update->pointer_new);
			break;

		case PTR_MSG_TYPE_CACHED:
			update_read_pointer_cached(s, &update->pointer_cached);
			IFCALL(update->PointerCached, update, &update->pointer_cached);
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
			IFCALL(update->Bitmap, update, &update->bitmap_update);
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
	int length;
	
	length = &update->state_end - &update->state_start;
	memset(update->state_start, 0, length);
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

static void update_send_surface_command(rdpUpdate* update, STREAM* s)
{
	rdpRdp* rdp = (rdpRdp*) update->rdp;
	fastpath_send_fragmented_update_pdu(rdp->fastpath, s);
}

static void update_send_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{
	rdpRdp* rdp = (rdpRdp*)update->rdp;
	fastpath_send_surfcmd_surface_bits(rdp->fastpath, surface_bits_command);
}

static void update_send_synchronize(rdpUpdate* update)
{
	rdpRdp* rdp = (rdpRdp*)update->rdp;
	STREAM* s;

	s = fastpath_update_pdu_init(rdp->fastpath);
	stream_write_uint8(s, FASTPATH_UPDATETYPE_SYNCHRONIZE); /* updateHeader (1 byte) */
	stream_write_uint16(s, 0); /* size (2 bytes) */
	fastpath_send_update_pdu(rdp->fastpath, s);
}

static void update_send_desktop_resize(rdpUpdate* update)
{
	rdpRdp* rdp = (rdpRdp*)update->rdp;

	rdp_server_reactivate(rdp);
}

static void update_send_pointer_system(rdpUpdate* update, POINTER_SYSTEM_UPDATE* pointer_system)
{
	rdpRdp* rdp = (rdpRdp*)update->rdp;
	STREAM* s;

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
	update->PointerSystem = update_send_pointer_system;
	update->SurfaceBits = update_send_surface_bits;
	update->SurfaceCommand = update_send_surface_command;
}

rdpUpdate* update_new(rdpRdp* rdp)
{
	rdpUpdate* update;

	update = (rdpUpdate*) xzalloc(sizeof(rdpUpdate));

	if (update != NULL)
	{
		update->rdp = (void*) rdp;
	}

	return update;
}

void update_free(rdpUpdate* update)
{
	update_free_bitmap(&update->bitmap_update);

	if (update != NULL)
	{
		xfree(update);
	}
}

