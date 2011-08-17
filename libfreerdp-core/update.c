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
#include "bitmap.h"

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

		bitmap_data->data = (uint8*) xzalloc(dstSize);

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

		rdp_read_share_control_header(s, &length, &pduType, &source);

		if (pduType != PDU_TYPE_DATA)
			return;

		rdp_read_share_data_header(s, &length, &type, &shareId);

		if (type == DATA_PDU_TYPE_UPDATE)
			update_recv(update, s);
	}
}

void update_reset_state(rdpUpdate* update)
{
	memset(&update->order_info, 0, sizeof(ORDER_INFO));
	update->order_info.orderType = ORDER_TYPE_PATBLT;
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
	if (update != NULL)
	{
		xfree(update);
	}
}

