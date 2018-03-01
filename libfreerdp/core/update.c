/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Update Data PDUs
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

#include "update.h"
#include "surface.h"
#include "message.h"
#include "info.h"
#include "window.h"

#include <freerdp/log.h>
#include <freerdp/peer.h>
#include <freerdp/codec/bitmap.h>


#define TAG FREERDP_TAG("core.update")

static const char* const UPDATE_TYPE_STRINGS[] =
{
	"Orders",
	"Bitmap",
	"Palette",
	"Synchronize"
};

static BOOL update_recv_orders(rdpUpdate* update, wStream* s)
{
	UINT16 numberOrders;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength(s) < 6");
		return FALSE;
	}

	Stream_Seek_UINT16(s); /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, numberOrders); /* numberOrders (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2OctetsB (2 bytes) */

	while (numberOrders > 0)
	{
		if (!update_recv_order(update, s))
		{
			WLog_ERR(TAG, "update_recv_order() failed");
			return FALSE;
		}

		numberOrders--;
	}

	return TRUE;
}

static BOOL update_read_bitmap_data(rdpUpdate* update, wStream* s,
                                    BITMAP_DATA* bitmapData)
{
	if (Stream_GetRemainingLength(s) < 18)
		return FALSE;

	Stream_Read_UINT16(s, bitmapData->destLeft);
	Stream_Read_UINT16(s, bitmapData->destTop);
	Stream_Read_UINT16(s, bitmapData->destRight);
	Stream_Read_UINT16(s, bitmapData->destBottom);
	Stream_Read_UINT16(s, bitmapData->width);
	Stream_Read_UINT16(s, bitmapData->height);
	Stream_Read_UINT16(s, bitmapData->bitsPerPixel);
	Stream_Read_UINT16(s, bitmapData->flags);
	Stream_Read_UINT16(s, bitmapData->bitmapLength);

	if (bitmapData->flags & BITMAP_COMPRESSION)
	{
		if (!(bitmapData->flags & NO_BITMAP_COMPRESSION_HDR))
		{
			Stream_Read_UINT16(s,
			                   bitmapData->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			Stream_Read_UINT16(s,
			                   bitmapData->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			Stream_Read_UINT16(s, bitmapData->cbScanWidth); /* cbScanWidth (2 bytes) */
			Stream_Read_UINT16(s,
			                   bitmapData->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			bitmapData->bitmapLength = bitmapData->cbCompMainBodySize;
		}

		bitmapData->compressed = TRUE;
	}
	else
		bitmapData->compressed = FALSE;

	if (Stream_GetRemainingLength(s) < bitmapData->bitmapLength)
		return FALSE;

	Stream_GetPointer(s, bitmapData->bitmapDataStream);
	Stream_Seek(s, bitmapData->bitmapLength);
	return TRUE;
}

static BOOL update_write_bitmap_data(rdpUpdate* update, wStream* s,
                                     BITMAP_DATA* bitmapData)
{
	if (!Stream_EnsureRemainingCapacity(s, 64 + bitmapData->bitmapLength))
		return FALSE;

	bitmapData->flags = 0;
	bitmapData->cbCompFirstRowSize = 0;

	if (bitmapData->compressed)
		bitmapData->flags |= BITMAP_COMPRESSION;

	if (update->context->settings->NoBitmapCompressionHeader)
	{
		bitmapData->flags |= NO_BITMAP_COMPRESSION_HDR;
		bitmapData->cbCompMainBodySize = bitmapData->bitmapLength;
	}

	Stream_Write_UINT16(s, bitmapData->destLeft);
	Stream_Write_UINT16(s, bitmapData->destTop);
	Stream_Write_UINT16(s, bitmapData->destRight);
	Stream_Write_UINT16(s, bitmapData->destBottom);
	Stream_Write_UINT16(s, bitmapData->width);
	Stream_Write_UINT16(s, bitmapData->height);
	Stream_Write_UINT16(s, bitmapData->bitsPerPixel);
	Stream_Write_UINT16(s, bitmapData->flags);
	Stream_Write_UINT16(s, bitmapData->bitmapLength);

	if (bitmapData->flags & BITMAP_COMPRESSION)
	{
		if (!(bitmapData->flags & NO_BITMAP_COMPRESSION_HDR))
		{
			Stream_Write_UINT16(s,
			                    bitmapData->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			Stream_Write_UINT16(s,
			                    bitmapData->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			Stream_Write_UINT16(s, bitmapData->cbScanWidth); /* cbScanWidth (2 bytes) */
			Stream_Write_UINT16(s,
			                    bitmapData->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
		}

		Stream_Write(s, bitmapData->bitmapDataStream, bitmapData->bitmapLength);
	}
	else
	{
		Stream_Write(s, bitmapData->bitmapDataStream, bitmapData->bitmapLength);
	}

	return TRUE;
}

BOOL update_read_bitmap_update(rdpUpdate* update, wStream* s,
                               BITMAP_UPDATE* bitmapUpdate)
{
	UINT32 i;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, bitmapUpdate->number); /* numberRectangles (2 bytes) */
	WLog_Print(update->log, WLOG_TRACE, "BitmapUpdate: %"PRIu32"", bitmapUpdate->number);

	if (bitmapUpdate->number > bitmapUpdate->count)
	{
		UINT16 count;
		BITMAP_DATA* newdata;
		count = bitmapUpdate->number * 2;
		newdata = (BITMAP_DATA*) realloc(bitmapUpdate->rectangles,
		                                 sizeof(BITMAP_DATA) * count);

		if (!newdata)
			return FALSE;

		bitmapUpdate->rectangles = newdata;
		ZeroMemory(&bitmapUpdate->rectangles[bitmapUpdate->count],
		           sizeof(BITMAP_DATA) * (count - bitmapUpdate->count));
		bitmapUpdate->count = count;
	}

	/* rectangles */
	for (i = 0; i < bitmapUpdate->number; i++)
	{
		if (!update_read_bitmap_data(update, s, &bitmapUpdate->rectangles[i]))
			return FALSE;
	}

	return TRUE;
}

static BOOL update_write_bitmap_update(rdpUpdate* update, wStream* s,
                                       const BITMAP_UPDATE* bitmapUpdate)
{
	int i;

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	Stream_Write_UINT16(s, UPDATE_TYPE_BITMAP); /* updateType */
	Stream_Write_UINT16(s, bitmapUpdate->number); /* numberRectangles (2 bytes) */

	/* rectangles */
	for (i = 0; i < (int) bitmapUpdate->number; i++)
	{
		if (!update_write_bitmap_data(update, s, &bitmapUpdate->rectangles[i]))
			return FALSE;
	}

	return TRUE;
}

BOOL update_read_palette(rdpUpdate* update, wStream* s,
                         PALETTE_UPDATE* palette_update)
{
	int i;
	PALETTE_ENTRY* entry;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */
	Stream_Read_UINT32(s,
	                   palette_update->number); /* numberColors (4 bytes), must be set to 256 */

	if (palette_update->number > 256)
		palette_update->number = 256;

	if (Stream_GetRemainingLength(s) < palette_update->number * 3)
		return FALSE;

	/* paletteEntries */
	for (i = 0; i < (int) palette_update->number; i++)
	{
		entry = &palette_update->entries[i];
		Stream_Read_UINT8(s, entry->red);
		Stream_Read_UINT8(s, entry->green);
		Stream_Read_UINT8(s, entry->blue);
	}

	return TRUE;
}

static void update_read_synchronize(rdpUpdate* update, wStream* s)
{
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */
	/**
	 * The Synchronize Update is an artifact from the
	 * T.128 protocol and should be ignored.
	 */
}

static BOOL update_read_play_sound(wStream* s, PLAY_SOUND_UPDATE* play_sound)
{
	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, play_sound->duration); /* duration (4 bytes) */
	Stream_Read_UINT32(s, play_sound->frequency); /* frequency (4 bytes) */
	return TRUE;
}

BOOL update_recv_play_sound(rdpUpdate* update, wStream* s)
{
	if (!update_read_play_sound(s, &update->play_sound))
		return FALSE;

	IFCALL(update->PlaySound, update->context, &update->play_sound);
	return TRUE;
}

BOOL update_read_pointer_position(wStream* s,
                                  POINTER_POSITION_UPDATE* pointer_position)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, pointer_position->xPos); /* xPos (2 bytes) */
	Stream_Read_UINT16(s, pointer_position->yPos); /* yPos (2 bytes) */
	return TRUE;
}

static BOOL update_read_pointer_system(wStream* s,
                                       POINTER_SYSTEM_UPDATE* pointer_system)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, pointer_system->type); /* systemPointerType (4 bytes) */
	return TRUE;
}

BOOL update_read_pointer_color(wStream* s, POINTER_COLOR_UPDATE* pointer_color,
                               int xorBpp)
{
	BYTE* newMask;
	UINT32 scanlineSize;

	if (Stream_GetRemainingLength(s) < 14)
		return FALSE;

	Stream_Read_UINT16(s, pointer_color->cacheIndex); /* cacheIndex (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->xPos); /* xPos (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->yPos); /* yPos (2 bytes) */
	/**
	 *  As stated in 2.2.9.1.1.4.4 Color Pointer Update:
	 *  The maximum allowed pointer width/height is 96 pixels if the client indicated support
	 *  for large pointers by setting the LARGE_POINTER_FLAG (0x00000001) in the Large
	 *  Pointer Capability Set (section 2.2.7.2.7). If the LARGE_POINTER_FLAG was not
	 *  set, the maximum allowed pointer width/height is 32 pixels.
	 *
	 *  So we check for a maximum of 96 for CVE-2014-0250.
	 */
	Stream_Read_UINT16(s, pointer_color->width); /* width (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->height); /* height (2 bytes) */

	if ((pointer_color->width > 96) || (pointer_color->height > 96))
		return FALSE;

	Stream_Read_UINT16(s,
	                   pointer_color->lengthAndMask); /* lengthAndMask (2 bytes) */
	Stream_Read_UINT16(s,
	                   pointer_color->lengthXorMask); /* lengthXorMask (2 bytes) */

	/**
	 * There does not seem to be any documentation on why
	 * xPos / yPos can be larger than width / height
	 * so it is missing in documentation or a bug in implementation
	 * 2.2.9.1.1.4.4 Color Pointer Update (TS_COLORPOINTERATTRIBUTE)
	 */
	if (pointer_color->xPos >= pointer_color->width)
		pointer_color->xPos = 0;

	if (pointer_color->yPos >= pointer_color->height)
		pointer_color->yPos = 0;

	if (pointer_color->lengthXorMask > 0)
	{
		/**
		 * Spec states that:
		 *
		 * xorMaskData (variable): A variable-length array of bytes. Contains the 24-bpp, bottom-up
		 * XOR mask scan-line data. The XOR mask is padded to a 2-byte boundary for each encoded
		 * scan-line. For example, if a 3x3 pixel cursor is being sent, then each scan-line will consume 10
		 * bytes (3 pixels per scan-line multiplied by 3 bytes per pixel, rounded up to the next even
		 * number of bytes).
		 *
		 * In fact instead of 24-bpp, the bpp parameter is given by the containing packet.
		 */
		if (Stream_GetRemainingLength(s) < pointer_color->lengthXorMask)
			return FALSE;

		scanlineSize = (7 + xorBpp * pointer_color->width) / 8;
		scanlineSize = ((scanlineSize + 1) / 2) * 2;

		if (scanlineSize * pointer_color->height != pointer_color->lengthXorMask)
		{
			WLog_ERR(TAG,
			         "invalid lengthXorMask: width=%"PRIu32" height=%"PRIu32", %"PRIu32" instead of %"PRIu32"",
			         pointer_color->width, pointer_color->height,
			         pointer_color->lengthXorMask, scanlineSize * pointer_color->height);
			return FALSE;
		}

		newMask = realloc(pointer_color->xorMaskData, pointer_color->lengthXorMask);

		if (!newMask)
			return FALSE;

		pointer_color->xorMaskData = newMask;
		Stream_Read(s, pointer_color->xorMaskData, pointer_color->lengthXorMask);
	}

	if (pointer_color->lengthAndMask > 0)
	{
		/**
		 * andMaskData (variable): A variable-length array of bytes. Contains the 1-bpp, bottom-up
		 * AND mask scan-line data. The AND mask is padded to a 2-byte boundary for each encoded
		 * scan-line. For example, if a 7x7 pixel cursor is being sent, then each scan-line will consume 2
		 * bytes (7 pixels per scan-line multiplied by 1 bpp, rounded up to the next even number of
		 * bytes).
		 */
		if (Stream_GetRemainingLength(s) < pointer_color->lengthAndMask)
			return FALSE;

		scanlineSize = ((7 + pointer_color->width) / 8);
		scanlineSize = ((1 + scanlineSize) / 2) * 2;

		if (scanlineSize * pointer_color->height != pointer_color->lengthAndMask)
		{
			WLog_ERR(TAG,  "invalid lengthAndMask: %"PRIu32" instead of %"PRIu32"",
			         pointer_color->lengthAndMask, scanlineSize * pointer_color->height);
			return FALSE;
		}

		newMask = realloc(pointer_color->andMaskData, pointer_color->lengthAndMask);

		if (!newMask)
			return FALSE;

		pointer_color->andMaskData = newMask;
		Stream_Read(s, pointer_color->andMaskData, pointer_color->lengthAndMask);
	}

	if (Stream_GetRemainingLength(s) > 0)
		Stream_Seek_UINT8(s); /* pad (1 byte) */

	return TRUE;
}

BOOL update_read_pointer_new(wStream* s, POINTER_NEW_UPDATE* pointer_new)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */

	if ((pointer_new->xorBpp < 1) || (pointer_new->xorBpp > 32))
	{
		WLog_ERR(TAG,  "invalid xorBpp %"PRIu32"", pointer_new->xorBpp);
		return FALSE;
	}

	return update_read_pointer_color(s, &pointer_new->colorPtrAttr,
	                                 pointer_new->xorBpp); /* colorPtrAttr */
}

BOOL update_read_pointer_cached(wStream* s,
                                POINTER_CACHED_UPDATE* pointer_cached)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, pointer_cached->cacheIndex); /* cacheIndex (2 bytes) */
	return TRUE;
}

BOOL update_recv_pointer(rdpUpdate* update, wStream* s)
{
	UINT16 messageType;
	rdpContext* context = update->context;
	rdpPointerUpdate* pointer = update->pointer;

	if (Stream_GetRemainingLength(s) < 2 + 2)
		return FALSE;

	Stream_Read_UINT16(s, messageType); /* messageType (2 bytes) */
	Stream_Seek_UINT16(s); /* pad2Octets (2 bytes) */

	switch (messageType)
	{
		case PTR_MSG_TYPE_POSITION:
			if (!update_read_pointer_position(s, &pointer->pointer_position))
				return FALSE;

			IFCALL(pointer->PointerPosition, context, &pointer->pointer_position);
			break;

		case PTR_MSG_TYPE_SYSTEM:
			if (!update_read_pointer_system(s, &pointer->pointer_system))
				return FALSE;

			IFCALL(pointer->PointerSystem, context, &pointer->pointer_system);
			break;

		case PTR_MSG_TYPE_COLOR:
			if (!update_read_pointer_color(s, &pointer->pointer_color, 24))
				return FALSE;

			IFCALL(pointer->PointerColor, context, &pointer->pointer_color);
			break;

		case PTR_MSG_TYPE_POINTER:
			if (!update_read_pointer_new(s, &pointer->pointer_new))
				return FALSE;

			IFCALL(pointer->PointerNew, context, &pointer->pointer_new);
			break;

		case PTR_MSG_TYPE_CACHED:
			if (!update_read_pointer_cached(s, &pointer->pointer_cached))
				return FALSE;

			IFCALL(pointer->PointerCached, context, &pointer->pointer_cached);
			break;

		default:
			break;
	}

	return TRUE;
}

BOOL update_recv(rdpUpdate* update, wStream* s)
{
	UINT16 updateType;
	rdpContext* context = update->context;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength(s) < 2");
		return FALSE;
	}

	Stream_Read_UINT16(s, updateType); /* updateType (2 bytes) */
	//WLog_DBG(TAG, "%s Update Data PDU", UPDATE_TYPE_STRINGS[updateType]);
	IFCALL(update->BeginPaint, context);

	switch (updateType)
	{
		case UPDATE_TYPE_ORDERS:
			if (!update_recv_orders(update, s))
			{
				/* XXX: Do we have to call EndPaint? */
				WLog_ERR(TAG, "UPDATE_TYPE_ORDERS - update_recv_orders() failed");
				return FALSE;
			}

			break;

		case UPDATE_TYPE_BITMAP:
			if (!update_read_bitmap_update(update, s, &update->bitmap_update))
			{
				WLog_ERR(TAG, "UPDATE_TYPE_BITMAP - update_read_bitmap_update() failed");
				return FALSE;
			}

			IFCALL(update->BitmapUpdate, context, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			if (!update_read_palette(update, s, &update->palette_update))
			{
				WLog_ERR(TAG, "UPDATE_TYPE_PALETTE - update_read_palette() failed");
				return FALSE;
			}

			IFCALL(update->Palette, context, &update->palette_update);
			break;

		case UPDATE_TYPE_SYNCHRONIZE:
			update_read_synchronize(update, s);
			IFCALL(update->Synchronize, context);
			break;

		default:
			WLog_ERR(TAG, "unknown update type %"PRIu16"", updateType);
			break;
	}

	IFCALL(update->EndPaint, context);
	return TRUE;
}

void update_reset_state(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;
	rdpAltSecUpdate* altsec = update->altsec;

	if (primary->fast_glyph.glyphData.aj)
	{
		free(primary->fast_glyph.glyphData.aj);
		primary->fast_glyph.glyphData.aj = NULL;
	}

	ZeroMemory(&primary->order_info, sizeof(ORDER_INFO));
	ZeroMemory(&primary->dstblt, sizeof(DSTBLT_ORDER));
	ZeroMemory(&primary->patblt, sizeof(PATBLT_ORDER));
	ZeroMemory(&primary->scrblt, sizeof(SCRBLT_ORDER));
	ZeroMemory(&primary->opaque_rect, sizeof(OPAQUE_RECT_ORDER));
	ZeroMemory(&primary->draw_nine_grid, sizeof(DRAW_NINE_GRID_ORDER));
	ZeroMemory(&primary->multi_dstblt, sizeof(MULTI_DSTBLT_ORDER));
	ZeroMemory(&primary->multi_patblt, sizeof(MULTI_PATBLT_ORDER));
	ZeroMemory(&primary->multi_scrblt, sizeof(MULTI_SCRBLT_ORDER));
	ZeroMemory(&primary->multi_opaque_rect, sizeof(MULTI_OPAQUE_RECT_ORDER));
	ZeroMemory(&primary->multi_draw_nine_grid, sizeof(MULTI_DRAW_NINE_GRID_ORDER));
	ZeroMemory(&primary->line_to, sizeof(LINE_TO_ORDER));
	ZeroMemory(&primary->polyline, sizeof(POLYLINE_ORDER));
	ZeroMemory(&primary->memblt, sizeof(MEMBLT_ORDER));
	ZeroMemory(&primary->mem3blt, sizeof(MEM3BLT_ORDER));
	ZeroMemory(&primary->save_bitmap, sizeof(SAVE_BITMAP_ORDER));
	ZeroMemory(&primary->glyph_index, sizeof(GLYPH_INDEX_ORDER));
	ZeroMemory(&primary->fast_index, sizeof(FAST_INDEX_ORDER));
	ZeroMemory(&primary->fast_glyph, sizeof(FAST_GLYPH_ORDER));
	ZeroMemory(&primary->polygon_sc, sizeof(POLYGON_SC_ORDER));
	ZeroMemory(&primary->polygon_cb, sizeof(POLYGON_CB_ORDER));
	ZeroMemory(&primary->ellipse_sc, sizeof(ELLIPSE_SC_ORDER));
	ZeroMemory(&primary->ellipse_cb, sizeof(ELLIPSE_CB_ORDER));
	primary->order_info.orderType = ORDER_TYPE_PATBLT;

	if (!update->initialState)
	{
		altsec->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
		IFCALL(altsec->SwitchSurface, update->context, &(altsec->switch_surface));
	}
}

BOOL update_post_connect(rdpUpdate* update)
{
	update->asynchronous = update->context->settings->AsyncUpdate;

	if (update->asynchronous)
		if (!(update->proxy = update_message_proxy_new(update)))
			return FALSE;

	update->altsec->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(update->altsec->SwitchSurface, update->context,
	       &(update->altsec->switch_surface));
	update->initialState = FALSE;
	return TRUE;
}

void update_post_disconnect(rdpUpdate* update)
{
	update->asynchronous = update->context->settings->AsyncUpdate;

	if (update->asynchronous)
		update_message_proxy_free(update->proxy);

	update->initialState = TRUE;
}

static BOOL update_begin_paint(rdpContext* context)
{
	wStream* s;
	rdpUpdate* update = context->update;

	if (update->us)
		update->EndPaint(context);

	s = fastpath_update_pdu_init_new(context->rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_SealLength(s);
	Stream_Seek(s, 2); /* numberOrders (2 bytes) */
	update->combineUpdates = TRUE;
	update->numberOrders = 0;
	update->us = s;
	return TRUE;
}

static BOOL update_end_paint(rdpContext* context)
{
	wStream* s;
	int headerLength;
	rdpUpdate* update = context->update;

	if (!update->us)
		return FALSE;

	s = update->us;
	headerLength = Stream_Length(s);
	Stream_SealLength(s);
	Stream_SetPosition(s, headerLength);
	Stream_Write_UINT16(s, update->numberOrders); /* numberOrders (2 bytes) */
	Stream_SetPosition(s, Stream_Length(s));

	if (update->numberOrders > 0)
	{
		WLog_ERR(TAG,  "sending %"PRIu16" orders", update->numberOrders);
		fastpath_send_update_pdu(context->rdp->fastpath, FASTPATH_UPDATETYPE_ORDERS, s,
		                         FALSE);
	}

	update->combineUpdates = FALSE;
	update->numberOrders = 0;
	update->us = NULL;
	Stream_Free(s, TRUE);
	return TRUE;
}

static void update_flush(rdpContext* context)
{
	rdpUpdate* update = context->update;

	if (update->numberOrders > 0)
	{
		update->EndPaint(context);
		update->BeginPaint(context);
	}
}

static void update_force_flush(rdpContext* context)
{
	rdpUpdate* update = context->update;

	if (update->numberOrders > 0)
	{
		update->EndPaint(context);
		update->BeginPaint(context);
	}
}

static BOOL update_check_flush(rdpContext* context, int size)
{
	wStream* s;
	rdpUpdate* update = context->update;
	s = update->us;

	if (!update->us)
	{
		update->BeginPaint(context);
		return FALSE;
	}

	if (Stream_GetPosition(s) + size + 64 >= 0x3FFF)
	{
		update_flush(context);
		return TRUE;
	}

	return FALSE;
}

static BOOL update_set_bounds(rdpContext* context,
                              const rdpBounds* bounds)
{
	rdpUpdate* update = context->update;
	CopyMemory(&update->previousBounds, &update->currentBounds, sizeof(rdpBounds));

	if (!bounds)
		ZeroMemory(&update->currentBounds, sizeof(rdpBounds));
	else
		CopyMemory(&update->currentBounds, bounds, sizeof(rdpBounds));

	return TRUE;
}

BOOL update_bounds_is_null(rdpBounds* bounds)
{
	if ((bounds->left == 0) && (bounds->top == 0) &&
	    (bounds->right == 0) && (bounds->bottom == 0))
		return TRUE;

	return FALSE;
}

BOOL update_bounds_equals(rdpBounds* bounds1, rdpBounds* bounds2)
{
	if ((bounds1->left == bounds2->left) && (bounds1->top == bounds2->top) &&
	    (bounds1->right == bounds2->right) && (bounds1->bottom == bounds2->bottom))
		return TRUE;

	return FALSE;
}

int update_prepare_bounds(rdpContext* context, ORDER_INFO* orderInfo)
{
	int length = 0;
	rdpUpdate* update = context->update;
	orderInfo->boundsFlags = 0;

	if (update_bounds_is_null(&update->currentBounds))
		return 0;

	orderInfo->controlFlags |= ORDER_BOUNDS;

	if (update_bounds_equals(&update->previousBounds, &update->currentBounds))
	{
		orderInfo->controlFlags |= ORDER_ZERO_BOUNDS_DELTAS;
		return 0;
	}
	else
	{
		length += 1;

		if (update->previousBounds.left != update->currentBounds.left)
		{
			orderInfo->bounds.left = update->currentBounds.left;
			orderInfo->boundsFlags |= BOUND_LEFT;
			length += 2;
		}

		if (update->previousBounds.top != update->currentBounds.top)
		{
			orderInfo->bounds.top = update->currentBounds.top;
			orderInfo->boundsFlags |= BOUND_TOP;
			length += 2;
		}

		if (update->previousBounds.right != update->currentBounds.right)
		{
			orderInfo->bounds.right = update->currentBounds.right;
			orderInfo->boundsFlags |= BOUND_RIGHT;
			length += 2;
		}

		if (update->previousBounds.bottom != update->currentBounds.bottom)
		{
			orderInfo->bounds.bottom = update->currentBounds.bottom;
			orderInfo->boundsFlags |= BOUND_BOTTOM;
			length += 2;
		}
	}

	return length;
}

static int update_prepare_order_info(rdpContext* context,
                                     ORDER_INFO* orderInfo, UINT32 orderType)
{
	int length = 1;
	orderInfo->fieldFlags = 0;
	orderInfo->orderType = orderType;
	orderInfo->controlFlags = ORDER_STANDARD;
	orderInfo->controlFlags |= ORDER_TYPE_CHANGE;
	length += 1;
	length += PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType];
	length += update_prepare_bounds(context, orderInfo);
	return length;
}

int update_write_order_info(rdpContext* context, wStream* s,
                            ORDER_INFO* orderInfo, int offset)
{
	size_t position;
	position = Stream_GetPosition(s);
	Stream_SetPosition(s, offset);
	Stream_Write_UINT8(s, orderInfo->controlFlags); /* controlFlags (1 byte) */

	if (orderInfo->controlFlags & ORDER_TYPE_CHANGE)
		Stream_Write_UINT8(s, orderInfo->orderType); /* orderType (1 byte) */

	update_write_field_flags(s, orderInfo->fieldFlags, orderInfo->controlFlags,
	                         PRIMARY_DRAWING_ORDER_FIELD_BYTES[orderInfo->orderType]);
	update_write_bounds(s, orderInfo);
	Stream_SetPosition(s, position);
	return 0;
}

static void update_write_refresh_rect(wStream* s, BYTE count,
                                      const RECTANGLE_16* areas)
{
	int i;
	Stream_Write_UINT8(s, count); /* numberOfAreas (1 byte) */
	Stream_Seek(s, 3); /* pad3Octets (3 bytes) */

	for (i = 0; i < count; i++)
	{
		Stream_Write_UINT16(s, areas[i].left); /* left (2 bytes) */
		Stream_Write_UINT16(s, areas[i].top); /* top (2 bytes) */
		Stream_Write_UINT16(s, areas[i].right); /* right (2 bytes) */
		Stream_Write_UINT16(s, areas[i].bottom); /* bottom (2 bytes) */
	}
}

static BOOL update_send_refresh_rect(rdpContext* context, BYTE count,
                                     const RECTANGLE_16* areas)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;

	if (rdp->settings->RefreshRect)
	{
		BOOL ret;
		s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		update_write_refresh_rect(s, count, areas);
		ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_REFRESH_RECT, rdp->mcs->userId);
		Stream_Release(s);
		return ret;
	}

	return TRUE;
}

static void update_write_suppress_output(wStream* s, BYTE allow,
        const RECTANGLE_16* area)
{
	Stream_Write_UINT8(s, allow); /* allowDisplayUpdates (1 byte) */
	/* Use zeros for padding (like mstsc) for compatibility with legacy servers */
	Stream_Zero(s, 3); /* pad3Octets (3 bytes) */

	if (allow > 0)
	{
		Stream_Write_UINT16(s, area->left); /* left (2 bytes) */
		Stream_Write_UINT16(s, area->top); /* top (2 bytes) */
		Stream_Write_UINT16(s, area->right); /* right (2 bytes) */
		Stream_Write_UINT16(s, area->bottom); /* bottom (2 bytes) */
	}
}

static BOOL update_send_suppress_output(rdpContext* context, BYTE allow,
                                        const RECTANGLE_16* area)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;

	if (rdp->settings->SuppressOutput)
	{
		BOOL ret;
		s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		update_write_suppress_output(s, allow, area);
		ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SUPPRESS_OUTPUT,
		                        rdp->mcs->userId);
		Stream_Release(s);
		return ret;
	}

	return TRUE;
}

static BOOL update_send_surface_command(rdpContext* context, wStream* s)
{
	wStream* update;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	update = fastpath_update_pdu_init(rdp->fastpath);

	if (!update)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(update, Stream_GetPosition(s)))
	{
		ret = FALSE;
		goto out;
	}

	Stream_Write(update, Stream_Buffer(s), Stream_GetPosition(s));
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS,
	                               update, FALSE);
out:
	Stream_Release(update);
	return ret;
}

static BOOL update_send_surface_bits(rdpContext* context,
                                     const SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	update_force_flush(context);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_surfcmd_surface_bits(s, surfaceBitsCommand))
		goto out_fail;

	if (!fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s,
	                              surfaceBitsCommand->skipCompression))
		goto out_fail;

	update_force_flush(context);
	ret = TRUE;
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_surface_frame_marker(rdpContext* context,
        const SURFACE_FRAME_MARKER* surfaceFrameMarker)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	update_force_flush(context);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_surfcmd_frame_marker(s, surfaceFrameMarker->frameAction,
	                                       surfaceFrameMarker->frameId) ||
	    !fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s,
	                              FALSE))
		goto out_fail;

	update_force_flush(context);
	ret = TRUE;
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_surface_frame_bits(rdpContext* context,
        const SURFACE_BITS_COMMAND* cmd,
        BOOL first, BOOL last, UINT32 frameId)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	update_force_flush(context);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (first)
	{
		if (!update_write_surfcmd_frame_marker(s, SURFACECMD_FRAMEACTION_BEGIN,
		                                       frameId))
			goto out_fail;
	}

	if (!update_write_surfcmd_surface_bits(s, cmd))
		goto out_fail;

	if (last)
	{
		if (!update_write_surfcmd_frame_marker(s, SURFACECMD_FRAMEACTION_END, frameId))
			goto out_fail;
	}

	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s,
	                               cmd->skipCompression);
	update_force_flush(context);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_frame_acknowledge(rdpContext* context, UINT32 frameId)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;

	if (rdp->settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
	{
		BOOL ret;
		s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		Stream_Write_UINT32(s, frameId);
		ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FRAME_ACKNOWLEDGE,
		                        rdp->mcs->userId);
		Stream_Release(s);
		return ret;
	}

	return TRUE;
}

static BOOL update_send_synchronize(rdpContext* context)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_Zero(s, 2); /* pad2Octets (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SYNCHRONIZE,
	                               s, FALSE);
	Stream_Release(s);
	return ret;
}

static BOOL update_send_desktop_resize(rdpContext* context)
{
	return rdp_server_reactivate(context->rdp);
}

static BOOL update_send_bitmap_update(rdpContext* context,
                                      const BITMAP_UPDATE* bitmapUpdate)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	rdpUpdate* update = context->update;
	BOOL ret = TRUE;
	update_force_flush(context);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_bitmap_update(update, s, bitmapUpdate) ||
	    !fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_BITMAP, s,
	                              bitmapUpdate->skipCompression))
	{
		ret = FALSE;
		goto out_fail;
	}

	update_force_flush(context);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_play_sound(rdpContext* context,
                                   const PLAY_SOUND_UPDATE* play_sound)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret;

	if (!rdp->settings->ReceivedCapabilities[CAPSET_TYPE_SOUND])
	{
		return TRUE;
	}

	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, play_sound->duration);
	Stream_Write_UINT32(s, play_sound->frequency);
	ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_PLAY_SOUND, rdp->mcs->userId);
	Stream_Release(s);
	return ret;
}
/**
 * Primary Drawing Orders
 */

static BOOL update_send_dstblt(rdpContext* context,
                               const DSTBLT_ORDER* dstblt)
{
	wStream* s;
	UINT32 offset;
	UINT32 headerLength;
	ORDER_INFO orderInfo;
	int inf;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_DSTBLT);
	inf = update_approximate_dstblt_order(&orderInfo, dstblt);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_dstblt_order(s, &orderInfo, dstblt))
		return FALSE;

	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	wStream* s;
	size_t offset;
	int headerLength;
	ORDER_INFO orderInfo;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_PATBLT);
	update_check_flush(context,
	                   headerLength + update_approximate_patblt_order(&orderInfo, patblt));
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_patblt_order(s, &orderInfo, patblt);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_scrblt(rdpContext* context, const SCRBLT_ORDER* scrblt)
{
	wStream* s;
	UINT32 offset;
	UINT32 headerLength;
	ORDER_INFO orderInfo;
	int inf;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_SCRBLT);
	inf = update_approximate_scrblt_order(&orderInfo, scrblt);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return TRUE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_scrblt_order(s, &orderInfo, scrblt);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_opaque_rect(rdpContext* context,
                                    const OPAQUE_RECT_ORDER* opaque_rect)
{
	wStream* s;
	size_t offset;
	int headerLength;
	ORDER_INFO orderInfo;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_OPAQUE_RECT);
	update_check_flush(context,
	                   headerLength + update_approximate_opaque_rect_order(&orderInfo, opaque_rect));
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_opaque_rect_order(s, &orderInfo, opaque_rect);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_line_to(rdpContext* context,
                                const LINE_TO_ORDER* line_to)
{
	wStream* s;
	int offset;
	int headerLength;
	ORDER_INFO orderInfo;
	int inf;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_LINE_TO);
	inf = update_approximate_line_to_order(&orderInfo, line_to);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_line_to_order(s, &orderInfo, line_to);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	wStream* s;
	size_t offset;
	int headerLength;
	ORDER_INFO orderInfo;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_MEMBLT);
	update_check_flush(context,
	                   headerLength + update_approximate_memblt_order(&orderInfo, memblt));
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_memblt_order(s, &orderInfo, memblt);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_glyph_index(rdpContext* context,
                                    GLYPH_INDEX_ORDER* glyph_index)
{
	wStream* s;
	size_t offset;
	int headerLength;
	int inf;
	ORDER_INFO orderInfo;
	rdpUpdate* update = context->update;
	headerLength = update_prepare_order_info(context, &orderInfo,
	               ORDER_TYPE_GLYPH_INDEX);
	inf = update_approximate_glyph_index_order(&orderInfo, glyph_index);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	offset = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);
	update_write_glyph_index_order(s, &orderInfo, glyph_index);
	update_write_order_info(context, s, &orderInfo, offset);
	update->numberOrders++;
	return TRUE;
}

/*
 * Secondary Drawing Orders
 */

static BOOL update_send_cache_bitmap(rdpContext* context,
                                     const CACHE_BITMAP_ORDER* cache_bitmap)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	int inf;
	UINT16 extraFlags;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	extraFlags = 0;
	headerLength = 6;
	orderType = cache_bitmap->compressed ?
	            ORDER_TYPE_CACHE_BITMAP_COMPRESSED : ORDER_TYPE_BITMAP_UNCOMPRESSED;
	inf = update_approximate_cache_bitmap_order(cache_bitmap,
	        cache_bitmap->compressed,
	        &extraFlags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_bitmap_order(s, cache_bitmap, cache_bitmap->compressed,
	                                     &extraFlags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_bitmap_v2(rdpContext* context,
                                        CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	UINT16 extraFlags;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	extraFlags = 0;
	headerLength = 6;
	orderType = cache_bitmap_v2->compressed ?
	            ORDER_TYPE_BITMAP_COMPRESSED_V2 : ORDER_TYPE_BITMAP_UNCOMPRESSED_V2;

	if (context->settings->NoBitmapCompressionHeader)
		cache_bitmap_v2->flags |= CBR2_NO_BITMAP_COMPRESSION_HDR;

	update_check_flush(context,
	                   headerLength + update_approximate_cache_bitmap_v2_order(cache_bitmap_v2,
	                           cache_bitmap_v2->compressed, &extraFlags));
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_bitmap_v2_order(s, cache_bitmap_v2,
	                                        cache_bitmap_v2->compressed, &extraFlags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_bitmap_v3(rdpContext* context,
                                        CACHE_BITMAP_V3_ORDER* cache_bitmap_v3)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	UINT16 extraFlags;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	extraFlags = 0;
	headerLength = 6;
	orderType = ORDER_TYPE_BITMAP_COMPRESSED_V3;
	update_check_flush(context,
	                   headerLength + update_approximate_cache_bitmap_v3_order(cache_bitmap_v3,
	                           &extraFlags));
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_bitmap_v3_order(s, cache_bitmap_v3, &extraFlags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_color_table(rdpContext* context,
        const CACHE_COLOR_TABLE_ORDER* cache_color_table)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	flags = 0;
	headerLength = 6;
	inf = update_approximate_cache_color_table_order(cache_color_table, &flags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_color_table_order(s, cache_color_table, &flags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_COLOR_TABLE); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_glyph(rdpContext* context,
                                    const CACHE_GLYPH_ORDER* cache_glyph)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	flags = 0;
	headerLength = 6;
	inf = update_approximate_cache_glyph_order(cache_glyph, &flags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_glyph_order(s, cache_glyph, &flags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_GLYPH); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_glyph_v2(rdpContext* context,
                                       const CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	flags = 0;
	headerLength = 6;
	inf =  update_approximate_cache_glyph_v2_order(cache_glyph_v2, &flags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_glyph_v2_order(s, cache_glyph_v2, &flags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_GLYPH); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_brush(rdpContext* context,
                                    const CACHE_BRUSH_ORDER* cache_brush)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdpUpdate* update = context->update;
	flags = 0;
	headerLength = 6;
	inf = update_approximate_cache_brush_order(cache_brush, &flags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_brush_order(s, cache_brush, &flags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD |
	                   ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags); /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_BRUSH); /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

/**
 * Alternate Secondary Drawing Orders
 */

static BOOL update_send_create_offscreen_bitmap_order(
    rdpContext* context,
    const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	wStream* s;
	size_t bm, em, inf;
	BYTE orderType;
	BYTE controlFlags;
	int headerLength;
	rdpUpdate* update = context->update;
	headerLength = 1;
	orderType = ORDER_TYPE_CREATE_OFFSCREEN_BITMAP;
	controlFlags = ORDER_SECONDARY | (orderType << 2);
	inf =  update_approximate_create_offscreen_bitmap_order(
	           create_offscreen_bitmap);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_create_offscreen_bitmap_order(s, create_offscreen_bitmap))
		return FALSE;

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, controlFlags); /* controlFlags (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_switch_surface_order(
    rdpContext* context,
    const SWITCH_SURFACE_ORDER* switch_surface)
{
	wStream* s;
	size_t bm, em, inf;
	BYTE orderType;
	BYTE controlFlags;
	int headerLength;
	rdpUpdate* update;

	if (!context || !switch_surface || !context->update)
		return FALSE;

	update = context->update;
	headerLength = 1;
	orderType = ORDER_TYPE_SWITCH_SURFACE;
	controlFlags = ORDER_SECONDARY | (orderType << 2);
	inf =  update_approximate_switch_surface_order(switch_surface);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_switch_surface_order(s, switch_surface))
		return FALSE;

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, controlFlags); /* controlFlags (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_pointer_system(rdpContext* context,
                                       const POINTER_SYSTEM_UPDATE* pointer_system)
{
	wStream* s;
	BYTE updateCode;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (pointer_system->type == SYSPTR_NULL)
		updateCode = FASTPATH_UPDATETYPE_PTR_NULL;
	else
		updateCode = FASTPATH_UPDATETYPE_PTR_DEFAULT;

	ret = fastpath_send_update_pdu(rdp->fastpath, updateCode, s, FALSE);
	Stream_Release(s);
	return ret;
}

static BOOL update_send_pointer_position(rdpContext* context,
        const POINTER_POSITION_UPDATE* pointerPosition)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 16))
		goto out_fail;

	Stream_Write_UINT16(s, pointerPosition->xPos); /* xPos (2 bytes) */
	Stream_Write_UINT16(s, pointerPosition->yPos); /* yPos (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_PTR_POSITION,
	                               s, FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_write_pointer_color(wStream* s,
                                       const POINTER_COLOR_UPDATE* pointer_color)
{
	if (!Stream_EnsureRemainingCapacity(s,
	                                    32 + pointer_color->lengthAndMask + pointer_color->lengthXorMask))
		return FALSE;

	Stream_Write_UINT16(s, pointer_color->cacheIndex);
	Stream_Write_UINT16(s, pointer_color->xPos);
	Stream_Write_UINT16(s, pointer_color->yPos);
	Stream_Write_UINT16(s, pointer_color->width);
	Stream_Write_UINT16(s, pointer_color->height);
	Stream_Write_UINT16(s, pointer_color->lengthAndMask);
	Stream_Write_UINT16(s, pointer_color->lengthXorMask);

	if (pointer_color->lengthXorMask > 0)
		Stream_Write(s, pointer_color->xorMaskData, pointer_color->lengthXorMask);

	if (pointer_color->lengthAndMask > 0)
		Stream_Write(s, pointer_color->andMaskData, pointer_color->lengthAndMask);

	Stream_Write_UINT8(s, 0); /* pad (1 byte) */
	return TRUE;
}

static BOOL update_send_pointer_color(rdpContext* context,
                                      const POINTER_COLOR_UPDATE* pointer_color)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_pointer_color(s, pointer_color))
		goto out_fail;

	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_COLOR, s,
	                               FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_pointer_new(rdpContext* context,
                                    const POINTER_NEW_UPDATE* pointer_new)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 16))
		goto out_fail;

	Stream_Write_UINT16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */
	update_write_pointer_color(s, &pointer_new->colorPtrAttr);
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_POINTER, s,
	                               FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_pointer_cached(rdpContext* context,
                                       const POINTER_CACHED_UPDATE* pointer_cached)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, pointer_cached->cacheIndex); /* cacheIndex (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_CACHED, s,
	                               FALSE);
	Stream_Release(s);
	return ret;
}

BOOL update_read_refresh_rect(rdpUpdate* update, wStream* s)
{
	int index;
	BYTE numberOfAreas;
	RECTANGLE_16* areas;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT8(s, numberOfAreas);
	Stream_Seek(s, 3); /* pad3Octects */

	if (Stream_GetRemainingLength(s) < ((size_t) numberOfAreas * 4 * 2))
		return FALSE;

	areas = (RECTANGLE_16*) calloc(numberOfAreas, sizeof(RECTANGLE_16));

	if (!areas)
		return FALSE;

	for (index = 0; index < numberOfAreas; index++)
	{
		Stream_Read_UINT16(s, areas[index].left);
		Stream_Read_UINT16(s, areas[index].top);
		Stream_Read_UINT16(s, areas[index].right);
		Stream_Read_UINT16(s, areas[index].bottom);
	}

	if (update->context->settings->RefreshRect)
		IFCALL(update->RefreshRect, update->context, numberOfAreas, areas);
	else
		WLog_Print(update->log, WLOG_WARN, "ignoring refresh rect request from client");

	free(areas);
	return TRUE;
}

BOOL update_read_suppress_output(rdpUpdate* update, wStream* s)
{
	BYTE allowDisplayUpdates;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT8(s, allowDisplayUpdates);
	Stream_Seek(s, 3); /* pad3Octects */

	if (allowDisplayUpdates > 0 && Stream_GetRemainingLength(s) < 8)
		return FALSE;

	if (update->context->settings->SuppressOutput)
		IFCALL(update->SuppressOutput, update->context, allowDisplayUpdates,
		       allowDisplayUpdates > 0 ? (RECTANGLE_16*) Stream_Pointer(s) : NULL);
	else
		WLog_Print(update->log, WLOG_WARN,
		           "ignoring suppress output request from client");

	return TRUE;
}

static BOOL update_send_set_keyboard_indicators(rdpContext* context,
        UINT16 led_flags)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s,
	                    0); /* unitId should be 0 according to MS-RDPBCGR 2.2.8.2.1.1 */
	Stream_Write_UINT16(s, led_flags); /* ledFlags (2 bytes) */
	ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS,
	                        rdp->mcs->userId);
	Stream_Release(s);
	return ret;
}

static BOOL update_send_set_keyboard_ime_status(rdpContext* context,
        UINT16 imeId, UINT32 imeState, UINT32 imeConvMode)
{
	wStream* s;
	rdpRdp* rdp = context->rdp;
	BOOL ret;
	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	/* unitId should be 0 according to MS-RDPBCGR 2.2.8.2.2.1 */
	Stream_Write_UINT16(s, imeId);
	Stream_Write_UINT32(s, imeState);
	Stream_Write_UINT32(s, imeConvMode);
	ret = rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS,
	                        rdp->mcs->userId);
	Stream_Release(s);
	return ret;
}

void update_register_server_callbacks(rdpUpdate* update)
{
	update->BeginPaint = update_begin_paint;
	update->EndPaint = update_end_paint;
	update->SetBounds = update_set_bounds;
	update->Synchronize = update_send_synchronize;
	update->DesktopResize = update_send_desktop_resize;
	update->BitmapUpdate = update_send_bitmap_update;
	update->SurfaceBits = update_send_surface_bits;
	update->SurfaceFrameMarker = update_send_surface_frame_marker;
	update->SurfaceCommand = update_send_surface_command;
	update->SurfaceFrameBits = update_send_surface_frame_bits;
	update->PlaySound = update_send_play_sound;
	update->SetKeyboardIndicators = update_send_set_keyboard_indicators;
	update->SetKeyboardImeStatus = update_send_set_keyboard_ime_status;
	update->SaveSessionInfo = rdp_send_save_session_info;
	update->primary->DstBlt = update_send_dstblt;
	update->primary->PatBlt = update_send_patblt;
	update->primary->ScrBlt = update_send_scrblt;
	update->primary->OpaqueRect = update_send_opaque_rect;
	update->primary->LineTo = update_send_line_to;
	update->primary->MemBlt = update_send_memblt;
	update->primary->GlyphIndex = update_send_glyph_index;
	update->secondary->CacheBitmap = update_send_cache_bitmap;
	update->secondary->CacheBitmapV2 = update_send_cache_bitmap_v2;
	update->secondary->CacheBitmapV3 = update_send_cache_bitmap_v3;
	update->secondary->CacheColorTable = update_send_cache_color_table;
	update->secondary->CacheGlyph = update_send_cache_glyph;
	update->secondary->CacheGlyphV2 = update_send_cache_glyph_v2;
	update->secondary->CacheBrush = update_send_cache_brush;
	update->altsec->CreateOffscreenBitmap =
	    update_send_create_offscreen_bitmap_order;
	update->altsec->SwitchSurface = update_send_switch_surface_order;
	update->pointer->PointerSystem = update_send_pointer_system;
	update->pointer->PointerPosition = update_send_pointer_position;
	update->pointer->PointerColor = update_send_pointer_color;
	update->pointer->PointerNew = update_send_pointer_new;
	update->pointer->PointerCached = update_send_pointer_cached;
}

void update_register_client_callbacks(rdpUpdate* update)
{
	update->RefreshRect = update_send_refresh_rect;
	update->SuppressOutput = update_send_suppress_output;
	update->SurfaceFrameAcknowledge = update_send_frame_acknowledge;
}

int update_process_messages(rdpUpdate* update)
{
	return update_message_queue_process_pending_messages(update);
}

static void update_free_queued_message(void* obj)
{
	wMessage* msg = (wMessage*)obj;
	update_message_queue_free_message(msg);
}

static void update_free_window_state(WINDOW_STATE_ORDER* window_state)
{
	if (!window_state)
		return;

	free(window_state->titleInfo.string);
	window_state->titleInfo.string = NULL;
	free(window_state->windowRects);
	window_state->windowRects = NULL;
	free(window_state->visibilityRects);
	window_state->visibilityRects = NULL;
}

rdpUpdate* update_new(rdpRdp* rdp)
{
	const wObject cb = { NULL, NULL, NULL,  update_free_queued_message, NULL };
	rdpUpdate* update;
	OFFSCREEN_DELETE_LIST* deleteList;
	update = (rdpUpdate*) calloc(1, sizeof(rdpUpdate));

	if (!update)
		return NULL;

	WLog_Init();
	update->log = WLog_Get("com.freerdp.core.update");
	update->bitmap_update.count = 64;
	update->bitmap_update.rectangles = (BITMAP_DATA*) calloc(
	                                       update->bitmap_update.count, sizeof(BITMAP_DATA));

	if (!update->bitmap_update.rectangles)
		goto error_rectangles;

	update->pointer = (rdpPointerUpdate*) calloc(1, sizeof(rdpPointerUpdate));

	if (!update->pointer)
		goto error_pointer;

	update->primary = (rdpPrimaryUpdate*) calloc(1, sizeof(rdpPrimaryUpdate));

	if (!update->primary)
		goto error_primary;

	update->secondary = (rdpSecondaryUpdate*) calloc(1, sizeof(rdpSecondaryUpdate));

	if (!update->secondary)
		goto error_secondary;

	update->altsec = (rdpAltSecUpdate*) calloc(1, sizeof(rdpAltSecUpdate));

	if (!update->altsec)
		goto error_altsec;

	update->window = (rdpWindowUpdate*) calloc(1, sizeof(rdpWindowUpdate));

	if (!update->window)
		goto error_window;

	deleteList = &(update->altsec->create_offscreen_bitmap.deleteList);
	deleteList->sIndices = 64;
	deleteList->indices = calloc(deleteList->sIndices, 2);

	if (!deleteList->indices)
		goto error_indices;

	deleteList->cIndices = 0;
	update->SuppressOutput = update_send_suppress_output;
	update->initialState = TRUE;
	update->queue = MessageQueue_New(&cb);

	if (!update->queue)
		goto error_queue;

	return update;
error_queue:
	free(deleteList->indices);
error_indices:
	free(update->window);
error_window:
	free(update->altsec);
error_altsec:
	free(update->secondary);
error_secondary:
	free(update->primary);
error_primary:
	free(update->pointer);
error_pointer:
	free(update->bitmap_update.rectangles);
error_rectangles:
	free(update);
	return NULL;
}

void update_free(rdpUpdate* update)
{
	if (update != NULL)
	{
		OFFSCREEN_DELETE_LIST* deleteList;
		deleteList = &(update->altsec->create_offscreen_bitmap.deleteList);
		free(deleteList->indices);
		free(update->bitmap_update.rectangles);
		free(update->pointer->pointer_color.andMaskData);
		free(update->pointer->pointer_color.xorMaskData);
		free(update->pointer->pointer_new.colorPtrAttr.andMaskData);
		free(update->pointer->pointer_new.colorPtrAttr.xorMaskData);
		free(update->pointer);
		free(update->primary->polyline.points);
		free(update->primary->polygon_sc.points);
		free(update->primary->fast_glyph.glyphData.aj);
		free(update->primary);
		free(update->secondary);
		free(update->altsec);
		free(update->window->monitored_desktop.windowIds);
		update_free_window_state(&update->window->window_state);
		update_free_window_icon_info(update->window->window_icon.iconInfo);
		free(update->window);
		MessageQueue_Free(update->queue);
		free(update);
	}
}
