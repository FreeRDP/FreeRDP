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

#include <freerdp/config.h>

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

#include "../cache/pointer.h"
#include "../cache/palette.h"
#include "../cache/bitmap.h"

#define TAG FREERDP_TAG("core.update")

static const char* const UPDATE_TYPE_STRINGS[] = { "Orders", "Bitmap", "Palette", "Synchronize" };

static const char* update_type_to_string(UINT16 updateType)
{
	if (updateType >= ARRAYSIZE(UPDATE_TYPE_STRINGS))
		return "UNKNOWN";

	return UPDATE_TYPE_STRINGS[updateType];
}

static BOOL update_recv_orders(rdpUpdate* update, wStream* s)
{
	UINT16 numberOrders;

	WINPR_ASSERT(update);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return FALSE;

	Stream_Seek_UINT16(s);               /* pad2OctetsA (2 bytes) */
	Stream_Read_UINT16(s, numberOrders); /* numberOrders (2 bytes) */
	Stream_Seek_UINT16(s);               /* pad2OctetsB (2 bytes) */

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

static BOOL update_read_bitmap_data(rdpUpdate* update, wStream* s, BITMAP_DATA* bitmapData)
{
	WINPR_UNUSED(update);
	WINPR_ASSERT(bitmapData);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 18))
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

	if ((bitmapData->width == 0) || (bitmapData->height == 0))
	{
		WLog_ERR(TAG, "Invalid BITMAP_DATA: width=%" PRIu16 ", height=%" PRIu16, bitmapData->width,
		         bitmapData->height);
		return FALSE;
	}

	if (bitmapData->flags & BITMAP_COMPRESSION)
	{
		if (!(bitmapData->flags & NO_BITMAP_COMPRESSION_HDR))
		{
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
				return FALSE;

			Stream_Read_UINT16(s,
			                   bitmapData->cbCompFirstRowSize); /* cbCompFirstRowSize (2 bytes) */
			Stream_Read_UINT16(s,
			                   bitmapData->cbCompMainBodySize); /* cbCompMainBodySize (2 bytes) */
			Stream_Read_UINT16(s, bitmapData->cbScanWidth);     /* cbScanWidth (2 bytes) */
			Stream_Read_UINT16(s,
			                   bitmapData->cbUncompressedSize); /* cbUncompressedSize (2 bytes) */
			bitmapData->bitmapLength = bitmapData->cbCompMainBodySize;
		}

		bitmapData->compressed = TRUE;
	}
	else
		bitmapData->compressed = FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, bitmapData->bitmapLength))
		return FALSE;

	if (bitmapData->bitmapLength > 0)
	{
		bitmapData->bitmapDataStream = malloc(bitmapData->bitmapLength);

		if (!bitmapData->bitmapDataStream)
			return FALSE;

		memcpy(bitmapData->bitmapDataStream, Stream_Pointer(s), bitmapData->bitmapLength);
		Stream_Seek(s, bitmapData->bitmapLength);
	}

	return TRUE;
}

static BOOL update_write_bitmap_data(rdpUpdate* update_pub, wStream* s, BITMAP_DATA* bitmapData)
{
	rdp_update_internal* update = update_cast(update_pub);

	WINPR_ASSERT(bitmapData);

	if (!Stream_EnsureRemainingCapacity(s, 64 + bitmapData->bitmapLength))
		return FALSE;

	if (update->common.autoCalculateBitmapData)
	{
		bitmapData->flags = 0;
		bitmapData->cbCompFirstRowSize = 0;

		if (bitmapData->compressed)
			bitmapData->flags |= BITMAP_COMPRESSION;

		if (update->common.context->settings->NoBitmapCompressionHeader)
		{
			bitmapData->flags |= NO_BITMAP_COMPRESSION_HDR;
			bitmapData->cbCompMainBodySize = bitmapData->bitmapLength;
		}
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
			Stream_Write_UINT16(s, bitmapData->cbScanWidth);     /* cbScanWidth (2 bytes) */
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

BITMAP_UPDATE* update_read_bitmap_update(rdpUpdate* update, wStream* s)
{
	UINT32 i;
	BITMAP_UPDATE* bitmapUpdate = calloc(1, sizeof(BITMAP_UPDATE));
	rdp_update_internal* up = update_cast(update);

	if (!bitmapUpdate)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		goto fail;

	Stream_Read_UINT16(s, bitmapUpdate->number); /* numberRectangles (2 bytes) */
	WLog_Print(up->log, WLOG_TRACE, "BitmapUpdate: %" PRIu32 "", bitmapUpdate->number);

	bitmapUpdate->rectangles = (BITMAP_DATA*)calloc(bitmapUpdate->number, sizeof(BITMAP_DATA));

	if (!bitmapUpdate->rectangles)
		goto fail;

	/* rectangles */
	for (i = 0; i < bitmapUpdate->number; i++)
	{
		if (!update_read_bitmap_data(update, s, &bitmapUpdate->rectangles[i]))
			goto fail;
	}

	return bitmapUpdate;
fail:
	free_bitmap_update(update->context, bitmapUpdate);
	return NULL;
}

static BOOL update_write_bitmap_update(rdpUpdate* update, wStream* s,
                                       const BITMAP_UPDATE* bitmapUpdate)
{
	WINPR_ASSERT(update);
	WINPR_ASSERT(bitmapUpdate);

	if (!Stream_EnsureRemainingCapacity(s, 32))
		return FALSE;

	Stream_Write_UINT16(s, UPDATE_TYPE_BITMAP);   /* updateType */
	Stream_Write_UINT16(s, bitmapUpdate->number); /* numberRectangles (2 bytes) */

	/* rectangles */
	for (UINT32 i = 0; i < bitmapUpdate->number; i++)
	{
		if (!update_write_bitmap_data(update, s, &bitmapUpdate->rectangles[i]))
			return FALSE;
	}

	return TRUE;
}

PALETTE_UPDATE* update_read_palette(rdpUpdate* update, wStream* s)
{
	int i;
	PALETTE_ENTRY* entry;
	PALETTE_UPDATE* palette_update = calloc(1, sizeof(PALETTE_UPDATE));

	if (!palette_update)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		goto fail;

	Stream_Seek_UINT16(s);                         /* pad2Octets (2 bytes) */
	Stream_Read_UINT32(s, palette_update->number); /* numberColors (4 bytes), must be set to 256 */

	if (palette_update->number > 256)
		palette_update->number = 256;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 3ull * palette_update->number))
		goto fail;

	/* paletteEntries */
	for (i = 0; i < (int)palette_update->number; i++)
	{
		entry = &palette_update->entries[i];
		Stream_Read_UINT8(s, entry->red);
		Stream_Read_UINT8(s, entry->green);
		Stream_Read_UINT8(s, entry->blue);
	}

	return palette_update;
fail:
	free_palette_update(update->context, palette_update);
	return NULL;
}

static BOOL update_read_synchronize(rdpUpdate* update, wStream* s)
{
	WINPR_UNUSED(update);
	return Stream_SafeSeek(s, 2); /* pad2Octets (2 bytes) */
	                              /**
	                               * The Synchronize Update is an artifact from the
	                               * T.128 protocol and should be ignored.
	                               */
}

static BOOL update_read_play_sound(wStream* s, PLAY_SOUND_UPDATE* play_sound)
{
	WINPR_ASSERT(play_sound);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, play_sound->duration);  /* duration (4 bytes) */
	Stream_Read_UINT32(s, play_sound->frequency); /* frequency (4 bytes) */
	return TRUE;
}

BOOL update_recv_play_sound(rdpUpdate* update, wStream* s)
{
	PLAY_SOUND_UPDATE play_sound = { 0 };

	WINPR_ASSERT(update);

	if (!update_read_play_sound(s, &play_sound))
		return FALSE;

	return IFCALLRESULT(FALSE, update->PlaySound, update->context, &play_sound);
}

POINTER_POSITION_UPDATE* update_read_pointer_position(rdpUpdate* update, wStream* s)
{
	POINTER_POSITION_UPDATE* pointer_position = calloc(1, sizeof(POINTER_POSITION_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer_position)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		goto fail;

	Stream_Read_UINT16(s, pointer_position->xPos); /* xPos (2 bytes) */
	Stream_Read_UINT16(s, pointer_position->yPos); /* yPos (2 bytes) */
	return pointer_position;
fail:
	free_pointer_position_update(update->context, pointer_position);
	return NULL;
}

POINTER_SYSTEM_UPDATE* update_read_pointer_system(rdpUpdate* update, wStream* s)
{
	POINTER_SYSTEM_UPDATE* pointer_system = calloc(1, sizeof(POINTER_SYSTEM_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer_system)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		goto fail;

	Stream_Read_UINT32(s, pointer_system->type); /* systemPointerType (4 bytes) */
	return pointer_system;
fail:
	free_pointer_system_update(update->context, pointer_system);
	return NULL;
}

static BOOL _update_read_pointer_color(wStream* s, POINTER_COLOR_UPDATE* pointer_color, BYTE xorBpp,
                                       UINT32 flags)
{
	BYTE* newMask;
	UINT32 scanlineSize;
	UINT32 max = 32;

	WINPR_ASSERT(pointer_color);

	if (flags & LARGE_POINTER_FLAG_96x96)
		max = 96;

	if (!pointer_color)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 14))
		goto fail;

	Stream_Read_UINT16(s, pointer_color->cacheIndex); /* cacheIndex (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->xPos);       /* xPos (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->yPos);       /* yPos (2 bytes) */
	/**
	 *  As stated in 2.2.9.1.1.4.4 Color Pointer Update:
	 *  The maximum allowed pointer width/height is 96 pixels if the client indicated support
	 *  for large pointers by setting the LARGE_POINTER_FLAG (0x00000001) in the Large
	 *  Pointer Capability Set (section 2.2.7.2.7). If the LARGE_POINTER_FLAG was not
	 *  set, the maximum allowed pointer width/height is 32 pixels.
	 *
	 *  So we check for a maximum for CVE-2014-0250.
	 */
	Stream_Read_UINT16(s, pointer_color->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->height); /* height (2 bytes) */

	if ((pointer_color->width > max) || (pointer_color->height > max))
		goto fail;

	Stream_Read_UINT16(s, pointer_color->lengthAndMask); /* lengthAndMask (2 bytes) */
	Stream_Read_UINT16(s, pointer_color->lengthXorMask); /* lengthXorMask (2 bytes) */

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
		 * scan-line. For example, if a 3x3 pixel cursor is being sent, then each scan-line will
		 * consume 10 bytes (3 pixels per scan-line multiplied by 3 bytes per pixel, rounded up to
		 * the next even number of bytes).
		 *
		 * In fact instead of 24-bpp, the bpp parameter is given by the containing packet.
		 */
		if (!Stream_CheckAndLogRequiredLength(TAG, s, pointer_color->lengthXorMask))
			goto fail;

		scanlineSize = (7 + xorBpp * pointer_color->width) / 8;
		scanlineSize = ((scanlineSize + 1) / 2) * 2;

		if (scanlineSize * pointer_color->height != pointer_color->lengthXorMask)
		{
			WLog_ERR(TAG,
			         "invalid lengthXorMask: width=%" PRIu32 " height=%" PRIu32 ", %" PRIu32
			         " instead of %" PRIu32 "",
			         pointer_color->width, pointer_color->height, pointer_color->lengthXorMask,
			         scanlineSize * pointer_color->height);
			goto fail;
		}

		newMask = realloc(pointer_color->xorMaskData, pointer_color->lengthXorMask);

		if (!newMask)
			goto fail;

		pointer_color->xorMaskData = newMask;
		Stream_Read(s, pointer_color->xorMaskData, pointer_color->lengthXorMask);
	}

	if (pointer_color->lengthAndMask > 0)
	{
		/**
		 * andMaskData (variable): A variable-length array of bytes. Contains the 1-bpp, bottom-up
		 * AND mask scan-line data. The AND mask is padded to a 2-byte boundary for each encoded
		 * scan-line. For example, if a 7x7 pixel cursor is being sent, then each scan-line will
		 * consume 2 bytes (7 pixels per scan-line multiplied by 1 bpp, rounded up to the next even
		 * number of bytes).
		 */
		if (!Stream_CheckAndLogRequiredLength(TAG, s, pointer_color->lengthAndMask))
			goto fail;

		scanlineSize = ((7 + pointer_color->width) / 8);
		scanlineSize = ((1 + scanlineSize) / 2) * 2;

		if (scanlineSize * pointer_color->height != pointer_color->lengthAndMask)
		{
			WLog_ERR(TAG, "invalid lengthAndMask: %" PRIu32 " instead of %" PRIu32 "",
			         pointer_color->lengthAndMask, scanlineSize * pointer_color->height);
			goto fail;
		}

		newMask = realloc(pointer_color->andMaskData, pointer_color->lengthAndMask);

		if (!newMask)
			goto fail;

		pointer_color->andMaskData = newMask;
		Stream_Read(s, pointer_color->andMaskData, pointer_color->lengthAndMask);
	}

	if (Stream_GetRemainingLength(s) > 0)
		Stream_Seek_UINT8(s); /* pad (1 byte) */

	return TRUE;
fail:
	return FALSE;
}

POINTER_COLOR_UPDATE* update_read_pointer_color(rdpUpdate* update, wStream* s, BYTE xorBpp)
{
	POINTER_COLOR_UPDATE* pointer_color = calloc(1, sizeof(POINTER_COLOR_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer_color)
		goto fail;

	if (!_update_read_pointer_color(s, pointer_color, xorBpp,
	                                update->context->settings->LargePointerFlag))
		goto fail;

	return pointer_color;
fail:
	free_pointer_color_update(update->context, pointer_color);
	return NULL;
}

static BOOL _update_read_pointer_large(wStream* s, POINTER_LARGE_UPDATE* pointer)
{
	BYTE* newMask;
	UINT32 scanlineSize;

	if (!pointer)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		goto fail;

	Stream_Read_UINT16(s, pointer->xorBpp);
	Stream_Read_UINT16(s, pointer->cacheIndex); /* cacheIndex (2 bytes) */
	Stream_Read_UINT16(s, pointer->hotSpotX);   /* xPos (2 bytes) */
	Stream_Read_UINT16(s, pointer->hotSpotY);   /* yPos (2 bytes) */

	Stream_Read_UINT16(s, pointer->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, pointer->height); /* height (2 bytes) */

	if ((pointer->width > 384) || (pointer->height > 384))
		goto fail;

	Stream_Read_UINT32(s, pointer->lengthAndMask); /* lengthAndMask (4 bytes) */
	Stream_Read_UINT32(s, pointer->lengthXorMask); /* lengthXorMask (4 bytes) */

	if (pointer->hotSpotX >= pointer->width)
		pointer->hotSpotX = 0;

	if (pointer->hotSpotY >= pointer->height)
		pointer->hotSpotY = 0;

	if (pointer->lengthXorMask > 0)
	{
		/**
		 * Spec states that:
		 *
		 * xorMaskData (variable): A variable-length array of bytes. Contains the 24-bpp, bottom-up
		 * XOR mask scan-line data. The XOR mask is padded to a 2-byte boundary for each encoded
		 * scan-line. For example, if a 3x3 pixel cursor is being sent, then each scan-line will
		 * consume 10 bytes (3 pixels per scan-line multiplied by 3 bytes per pixel, rounded up to
		 * the next even number of bytes).
		 *
		 * In fact instead of 24-bpp, the bpp parameter is given by the containing packet.
		 */
		if (!Stream_CheckAndLogRequiredLength(TAG, s, pointer->lengthXorMask))
			goto fail;

		scanlineSize = (7 + pointer->xorBpp * pointer->width) / 8;
		scanlineSize = ((scanlineSize + 1) / 2) * 2;

		if (scanlineSize * pointer->height != pointer->lengthXorMask)
		{
			WLog_ERR(TAG,
			         "invalid lengthXorMask: width=%" PRIu32 " height=%" PRIu32 ", %" PRIu32
			         " instead of %" PRIu32 "",
			         pointer->width, pointer->height, pointer->lengthXorMask,
			         scanlineSize * pointer->height);
			goto fail;
		}

		newMask = realloc(pointer->xorMaskData, pointer->lengthXorMask);

		if (!newMask)
			goto fail;

		pointer->xorMaskData = newMask;
		Stream_Read(s, pointer->xorMaskData, pointer->lengthXorMask);
	}

	if (pointer->lengthAndMask > 0)
	{
		/**
		 * andMaskData (variable): A variable-length array of bytes. Contains the 1-bpp, bottom-up
		 * AND mask scan-line data. The AND mask is padded to a 2-byte boundary for each encoded
		 * scan-line. For example, if a 7x7 pixel cursor is being sent, then each scan-line will
		 * consume 2 bytes (7 pixels per scan-line multiplied by 1 bpp, rounded up to the next even
		 * number of bytes).
		 */
		if (!Stream_CheckAndLogRequiredLength(TAG, s, pointer->lengthAndMask))
			goto fail;

		scanlineSize = ((7 + pointer->width) / 8);
		scanlineSize = ((1 + scanlineSize) / 2) * 2;

		if (scanlineSize * pointer->height != pointer->lengthAndMask)
		{
			WLog_ERR(TAG, "invalid lengthAndMask: %" PRIu32 " instead of %" PRIu32 "",
			         pointer->lengthAndMask, scanlineSize * pointer->height);
			goto fail;
		}

		newMask = realloc(pointer->andMaskData, pointer->lengthAndMask);

		if (!newMask)
			goto fail;

		pointer->andMaskData = newMask;
		Stream_Read(s, pointer->andMaskData, pointer->lengthAndMask);
	}

	if (Stream_GetRemainingLength(s) > 0)
		Stream_Seek_UINT8(s); /* pad (1 byte) */

	return TRUE;
fail:
	return FALSE;
}

POINTER_LARGE_UPDATE* update_read_pointer_large(rdpUpdate* update, wStream* s)
{
	POINTER_LARGE_UPDATE* pointer = calloc(1, sizeof(POINTER_LARGE_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer)
		goto fail;

	if (!_update_read_pointer_large(s, pointer))
		goto fail;

	return pointer;
fail:
	free_pointer_large_update(update->context, pointer);
	return NULL;
}

POINTER_NEW_UPDATE* update_read_pointer_new(rdpUpdate* update, wStream* s)
{
	POINTER_NEW_UPDATE* pointer_new = calloc(1, sizeof(POINTER_NEW_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer_new)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		goto fail;

	Stream_Read_UINT16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */

	if ((pointer_new->xorBpp < 1) || (pointer_new->xorBpp > 32))
	{
		WLog_ERR(TAG, "invalid xorBpp %" PRIu32 "", pointer_new->xorBpp);
		goto fail;
	}

	if (!_update_read_pointer_color(s, &pointer_new->colorPtrAttr, pointer_new->xorBpp,
	                                update->context->settings->LargePointerFlag)) /* colorPtrAttr */
		goto fail;

	return pointer_new;
fail:
	free_pointer_new_update(update->context, pointer_new);
	return NULL;
}

POINTER_CACHED_UPDATE* update_read_pointer_cached(rdpUpdate* update, wStream* s)
{
	POINTER_CACHED_UPDATE* pointer = calloc(1, sizeof(POINTER_CACHED_UPDATE));

	WINPR_ASSERT(update);

	if (!pointer)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		goto fail;

	Stream_Read_UINT16(s, pointer->cacheIndex); /* cacheIndex (2 bytes) */
	return pointer;
fail:
	free_pointer_cached_update(update->context, pointer);
	return NULL;
}

BOOL update_recv_pointer(rdpUpdate* update, wStream* s)
{
	BOOL rc = FALSE;
	UINT16 messageType;

	WINPR_ASSERT(update);

	rdpContext* context = update->context;
	rdpPointerUpdate* pointer = update->pointer;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2 + 2))
		return FALSE;

	Stream_Read_UINT16(s, messageType); /* messageType (2 bytes) */
	Stream_Seek_UINT16(s);              /* pad2Octets (2 bytes) */

	switch (messageType)
	{
		case PTR_MSG_TYPE_POSITION:
		{
			POINTER_POSITION_UPDATE* pointer_position = update_read_pointer_position(update, s);

			if (pointer_position)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerPosition, context, pointer_position);
				free_pointer_position_update(context, pointer_position);
			}
		}
		break;

		case PTR_MSG_TYPE_SYSTEM:
		{
			POINTER_SYSTEM_UPDATE* pointer_system = update_read_pointer_system(update, s);

			if (pointer_system)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerSystem, context, pointer_system);
				free_pointer_system_update(context, pointer_system);
			}
		}
		break;

		case PTR_MSG_TYPE_COLOR:
		{
			POINTER_COLOR_UPDATE* pointer_color = update_read_pointer_color(update, s, 24);

			if (pointer_color)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerColor, context, pointer_color);
				free_pointer_color_update(context, pointer_color);
			}
		}
		break;

		case PTR_MSG_TYPE_POINTER_LARGE:
		{
			POINTER_LARGE_UPDATE* pointer_large = update_read_pointer_large(update, s);

			if (pointer_large)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerLarge, context, pointer_large);
				free_pointer_large_update(context, pointer_large);
			}
		}
		break;

		case PTR_MSG_TYPE_POINTER:
		{
			POINTER_NEW_UPDATE* pointer_new = update_read_pointer_new(update, s);

			if (pointer_new)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerNew, context, pointer_new);
				free_pointer_new_update(context, pointer_new);
			}
		}
		break;

		case PTR_MSG_TYPE_CACHED:
		{
			POINTER_CACHED_UPDATE* pointer_cached = update_read_pointer_cached(update, s);

			if (pointer_cached)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerCached, context, pointer_cached);
				free_pointer_cached_update(context, pointer_cached);
			}
		}
		break;

		default:
			break;
	}

	return rc;
}

BOOL update_recv(rdpUpdate* update, wStream* s)
{
	BOOL rc = FALSE;
	UINT16 updateType;
	rdp_update_internal* up = update_cast(update);
	rdpContext* context = update->context;

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, updateType); /* updateType (2 bytes) */
	WLog_Print(up->log, WLOG_TRACE, "%s Update Data PDU", update_type_to_string(updateType));

	if (!update_begin_paint(update))
		goto fail;

	switch (updateType)
	{
		case UPDATE_TYPE_ORDERS:
			rc = update_recv_orders(update, s);
			break;

		case UPDATE_TYPE_BITMAP:
		{
			BITMAP_UPDATE* bitmap_update = update_read_bitmap_update(update, s);

			if (!bitmap_update)
			{
				WLog_ERR(TAG, "UPDATE_TYPE_BITMAP - update_read_bitmap_update() failed");
				goto fail;
			}

			rc = IFCALLRESULT(FALSE, update->BitmapUpdate, context, bitmap_update);
			free_bitmap_update(context, bitmap_update);
		}
		break;

		case UPDATE_TYPE_PALETTE:
		{
			PALETTE_UPDATE* palette_update = update_read_palette(update, s);

			if (!palette_update)
			{
				WLog_ERR(TAG, "UPDATE_TYPE_PALETTE - update_read_palette() failed");
				goto fail;
			}

			rc = IFCALLRESULT(FALSE, update->Palette, context, palette_update);
			free_palette_update(context, palette_update);
		}
		break;

		case UPDATE_TYPE_SYNCHRONIZE:
			if (!update_read_synchronize(update, s))
				goto fail;
			rc = IFCALLRESULT(TRUE, update->Synchronize, context);
			break;

		default:
			break;
	}

fail:

	if (!update_end_paint(update))
		rc = FALSE;

	if (!rc)
	{
		WLog_ERR(TAG, "UPDATE_TYPE %s [%" PRIu16 "] failed", update_type_to_string(updateType),
		         updateType);
		return FALSE;
	}

	return TRUE;
}

void update_reset_state(rdpUpdate* update)
{
	rdp_update_internal* up = update_cast(update);
	rdp_primary_update_internal* primary = primary_update_cast(update->primary);

	WINPR_ASSERT(primary);

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

	if (!up->initialState)
	{
		rdp_altsec_update_internal* altsec = altsec_update_cast(update->altsec);
		WINPR_ASSERT(altsec);

		altsec->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
		IFCALL(altsec->common.SwitchSurface, update->context, &(altsec->switch_surface));
	}
}

BOOL update_post_connect(rdpUpdate* update)
{
	rdp_update_internal* up = update_cast(update);
	rdp_altsec_update_internal* altsec = altsec_update_cast(update->altsec);

	WINPR_ASSERT(update->context);
	WINPR_ASSERT(update->context->settings);
	up->asynchronous = update->context->settings->AsyncUpdate;

	if (up->asynchronous)
		if (!(up->proxy = update_message_proxy_new(update)))
			return FALSE;

	altsec->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(update->altsec->SwitchSurface, update->context, &(altsec->switch_surface));
	up->initialState = FALSE;
	return TRUE;
}

void update_post_disconnect(rdpUpdate* update)
{
	rdp_update_internal* up = update_cast(update);

	WINPR_ASSERT(update->context);
	WINPR_ASSERT(update->context->settings);

	up->asynchronous = update->context->settings->AsyncUpdate;

	if (up->asynchronous)
		update_message_proxy_free(up->proxy);

	up->initialState = TRUE;
}

static BOOL _update_begin_paint(rdpContext* context)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdp_update_internal* update = update_cast(context->update);

	if (update->us)
	{
		if (!update_end_paint(&update->common))
			return FALSE;
	}

	WINPR_ASSERT(context->rdp);
	s = fastpath_update_pdu_init_new(context->rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_SealLength(s);
	Stream_GetLength(s, update->offsetOrders);
	Stream_Seek(s, 2); /* numberOrders (2 bytes) */
	update->combineUpdates = TRUE;
	update->numberOrders = 0;
	update->us = s;
	return TRUE;
}

static BOOL _update_end_paint(rdpContext* context)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdp_update_internal* update = update_cast(context->update);

	if (!update->us)
		return FALSE;

	s = update->us;
	Stream_SealLength(s);
	Stream_SetPosition(s, update->offsetOrders);
	Stream_Write_UINT16(s, update->numberOrders); /* numberOrders (2 bytes) */
	Stream_SetPosition(s, Stream_Length(s));

	if (update->numberOrders > 0)
	{
		WLog_DBG(TAG, "sending %" PRIu16 " orders", update->numberOrders);
		fastpath_send_update_pdu(context->rdp->fastpath, FASTPATH_UPDATETYPE_ORDERS, s, FALSE);
	}

	update->combineUpdates = FALSE;
	update->numberOrders = 0;
	update->offsetOrders = 0;
	update->us = NULL;
	Stream_Free(s, TRUE);
	return TRUE;
}

static void update_flush(rdpContext* context)
{
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	update = update_cast(context->update);

	if (update->numberOrders > 0)
	{
		update_end_paint(&update->common);
		update_begin_paint(&update->common);
	}
}

static void update_force_flush(rdpContext* context)
{
	update_flush(context);
}

static BOOL update_check_flush(rdpContext* context, size_t size)
{
	wStream* s;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	update = update_cast(context->update);

	s = update->us;

	if (!update->us)
	{
		update_begin_paint(&update->common);
		return FALSE;
	}

	if (Stream_GetPosition(s) + size + 64 >= 0x3FFF)
	{
		update_flush(context);
		return TRUE;
	}

	return FALSE;
}

static BOOL update_set_bounds(rdpContext* context, const rdpBounds* bounds)
{
	rdp_update_internal* update;

	WINPR_ASSERT(context);

	update = update_cast(context->update);

	CopyMemory(&update->previousBounds, &update->currentBounds, sizeof(rdpBounds));

	if (!bounds)
		ZeroMemory(&update->currentBounds, sizeof(rdpBounds));
	else
		CopyMemory(&update->currentBounds, bounds, sizeof(rdpBounds));

	return TRUE;
}

static BOOL update_bounds_is_null(rdpBounds* bounds)
{
	WINPR_ASSERT(bounds);
	if ((bounds->left == 0) && (bounds->top == 0) && (bounds->right == 0) && (bounds->bottom == 0))
		return TRUE;

	return FALSE;
}

static BOOL update_bounds_equals(rdpBounds* bounds1, rdpBounds* bounds2)
{
	WINPR_ASSERT(bounds1);
	WINPR_ASSERT(bounds2);

	if ((bounds1->left == bounds2->left) && (bounds1->top == bounds2->top) &&
	    (bounds1->right == bounds2->right) && (bounds1->bottom == bounds2->bottom))
		return TRUE;

	return FALSE;
}

static int update_prepare_bounds(rdpContext* context, ORDER_INFO* orderInfo)
{
	int length = 0;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);

	update = update_cast(context->update);

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

static int update_prepare_order_info(rdpContext* context, ORDER_INFO* orderInfo, UINT32 orderType)
{
	int length = 1;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);

	orderInfo->fieldFlags = 0;
	orderInfo->orderType = orderType;
	orderInfo->controlFlags = ORDER_STANDARD;
	orderInfo->controlFlags |= ORDER_TYPE_CHANGE;
	length += 1;
	length += get_primary_drawing_order_field_bytes(orderInfo->orderType, NULL);
	length += update_prepare_bounds(context, orderInfo);
	return length;
}

static int update_write_order_info(rdpContext* context, wStream* s, ORDER_INFO* orderInfo,
                                   size_t offset)
{
	size_t position;

	WINPR_UNUSED(context);
	WINPR_ASSERT(orderInfo);

	position = Stream_GetPosition(s);
	Stream_SetPosition(s, offset);
	Stream_Write_UINT8(s, orderInfo->controlFlags); /* controlFlags (1 byte) */

	if (orderInfo->controlFlags & ORDER_TYPE_CHANGE)
		Stream_Write_UINT8(s, orderInfo->orderType); /* orderType (1 byte) */

	if (!update_write_field_flags(
	        s, orderInfo->fieldFlags, orderInfo->controlFlags,
	        get_primary_drawing_order_field_bytes(orderInfo->orderType, NULL)))
		return -1;
	if (!update_write_bounds(s, orderInfo))
		return -1;
	Stream_SetPosition(s, position);
	return 0;
}

static void update_write_refresh_rect(wStream* s, BYTE count, const RECTANGLE_16* areas)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(areas || (count == 0));

	Stream_Write_UINT8(s, count); /* numberOfAreas (1 byte) */
	Stream_Seek(s, 3);            /* pad3Octets (3 bytes) */

	for (BYTE i = 0; i < count; i++)
	{
		Stream_Write_UINT16(s, areas[i].left);   /* left (2 bytes) */
		Stream_Write_UINT16(s, areas[i].top);    /* top (2 bytes) */
		Stream_Write_UINT16(s, areas[i].right);  /* right (2 bytes) */
		Stream_Write_UINT16(s, areas[i].bottom); /* bottom (2 bytes) */
	}
}

static BOOL update_send_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;

	WINPR_ASSERT(rdp->settings);
	if (rdp->settings->RefreshRect)
	{
		wStream* s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		update_write_refresh_rect(s, count, areas);
		return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_REFRESH_RECT, rdp->mcs->userId);
	}

	return TRUE;
}

static void update_write_suppress_output(wStream* s, BYTE allow, const RECTANGLE_16* area)
{
	WINPR_ASSERT(s);

	Stream_Write_UINT8(s, allow); /* allowDisplayUpdates (1 byte) */
	/* Use zeros for padding (like mstsc) for compatibility with legacy servers */
	Stream_Zero(s, 3); /* pad3Octets (3 bytes) */

	if (allow > 0)
	{
		WINPR_ASSERT(area);
		Stream_Write_UINT16(s, area->left);   /* left (2 bytes) */
		Stream_Write_UINT16(s, area->top);    /* top (2 bytes) */
		Stream_Write_UINT16(s, area->right);  /* right (2 bytes) */
		Stream_Write_UINT16(s, area->bottom); /* bottom (2 bytes) */
	}
}

static BOOL update_send_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	if (rdp->settings->SuppressOutput)
	{
		wStream* s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		update_write_suppress_output(s, allow, area);
		WINPR_ASSERT(rdp->mcs);
		return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SUPPRESS_OUTPUT, rdp->mcs->userId);
	}

	return TRUE;
}

static BOOL update_send_surface_command(rdpContext* context, wStream* s)
{
	wStream* update;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret;

	WINPR_ASSERT(rdp);
	update = fastpath_update_pdu_init(rdp->fastpath);

	if (!update)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(update, Stream_GetPosition(s)))
	{
		ret = FALSE;
		goto out;
	}

	Stream_Write(update, Stream_Buffer(s), Stream_GetPosition(s));
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, update, FALSE);
out:
	Stream_Release(update);
	return ret;
}

static BOOL update_send_surface_bits(rdpContext* context,
                                     const SURFACE_BITS_COMMAND* surfaceBitsCommand)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	WINPR_ASSERT(surfaceBitsCommand);
	WINPR_ASSERT(rdp);

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
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;
	update_force_flush(context);

	WINPR_ASSERT(rdp);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_surfcmd_frame_marker(s, surfaceFrameMarker->frameAction,
	                                       surfaceFrameMarker->frameId) ||
	    !fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SURFCMDS, s, FALSE))
		goto out_fail;

	update_force_flush(context);
	ret = TRUE;
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_surface_frame_bits(rdpContext* context, const SURFACE_BITS_COMMAND* cmd,
                                           BOOL first, BOOL last, UINT32 frameId)
{
	wStream* s;

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	update_force_flush(context);

	WINPR_ASSERT(rdp);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (first)
	{
		if (!update_write_surfcmd_frame_marker(s, SURFACECMD_FRAMEACTION_BEGIN, frameId))
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
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	if (rdp->settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE])
	{
		wStream* s = rdp_data_pdu_init(rdp);

		if (!s)
			return FALSE;

		Stream_Write_UINT32(s, frameId);
		return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FRAME_ACKNOWLEDGE, rdp->mcs->userId);
	}

	return TRUE;
}

static BOOL update_send_synchronize(rdpContext* context)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret;

	WINPR_ASSERT(rdp);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_Zero(s, 2); /* pad2Octets (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_SYNCHRONIZE, s, FALSE);
	Stream_Release(s);
	return ret;
}

static BOOL update_send_desktop_resize(rdpContext* context)
{
	WINPR_ASSERT(context);
	return rdp_server_reactivate(context->rdp);
}

static BOOL update_send_bitmap_update(rdpContext* context, const BITMAP_UPDATE* bitmapUpdate)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	rdpUpdate* update = context->update;
	BOOL ret = TRUE;

	update_force_flush(context);

	WINPR_ASSERT(rdp);
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

static BOOL update_send_play_sound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(play_sound);
	if (!rdp->settings->ReceivedCapabilities[CAPSET_TYPE_SOUND])
	{
		return TRUE;
	}

	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Write_UINT32(s, play_sound->duration);
	Stream_Write_UINT32(s, play_sound->frequency);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_PLAY_SOUND, rdp->mcs->userId);
}

/**
 * Primary Drawing Orders
 */

static BOOL update_send_dstblt(rdpContext* context, const DSTBLT_ORDER* dstblt)
{
	wStream* s;
	size_t offset;
	UINT32 headerLength;
	ORDER_INFO orderInfo;
	int inf;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(dstblt);

	update = update_cast(context->update);

	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_DSTBLT);
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
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(patblt);
	update = update_cast(context->update);

	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_PATBLT);
	update_check_flush(context, headerLength + update_approximate_patblt_order(&orderInfo, patblt));
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
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(scrblt);
	update = update_cast(context->update);

	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_SCRBLT);
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

static BOOL update_send_opaque_rect(rdpContext* context, const OPAQUE_RECT_ORDER* opaque_rect)
{
	wStream* s;
	size_t offset;
	int headerLength;
	ORDER_INFO orderInfo;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(opaque_rect);
	update = update_cast(context->update);

	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_OPAQUE_RECT);
	update_check_flush(context, headerLength +
	                                update_approximate_opaque_rect_order(&orderInfo, opaque_rect));
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

static BOOL update_send_line_to(rdpContext* context, const LINE_TO_ORDER* line_to)
{
	wStream* s;
	int offset;
	int headerLength;
	ORDER_INFO orderInfo;
	int inf;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(line_to);
	update = update_cast(context->update);
	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_LINE_TO);
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
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(memblt);
	update = update_cast(context->update);
	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_MEMBLT);
	update_check_flush(context, headerLength + update_approximate_memblt_order(&orderInfo, memblt));
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

static BOOL update_send_glyph_index(rdpContext* context, GLYPH_INDEX_ORDER* glyph_index)
{
	wStream* s;
	size_t offset;
	int headerLength;
	int inf;
	ORDER_INFO orderInfo;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(glyph_index);
	update = update_cast(context->update);

	headerLength = update_prepare_order_info(context, &orderInfo, ORDER_TYPE_GLYPH_INDEX);
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

static BOOL update_send_cache_bitmap(rdpContext* context, const CACHE_BITMAP_ORDER* cache_bitmap)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	int inf;
	UINT16 extraFlags;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_bitmap);
	update = update_cast(context->update);

	extraFlags = 0;
	headerLength = 6;
	orderType = cache_bitmap->compressed ? ORDER_TYPE_CACHE_BITMAP_COMPRESSED
	                                     : ORDER_TYPE_BITMAP_UNCOMPRESSED;
	inf =
	    update_approximate_cache_bitmap_order(cache_bitmap, cache_bitmap->compressed, &extraFlags);
	update_check_flush(context, headerLength + inf);
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_bitmap_order(s, cache_bitmap, cache_bitmap->compressed, &extraFlags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags);                      /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType);                        /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_bitmap_v2(rdpContext* context, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	UINT16 extraFlags;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_bitmap_v2);
	update = update_cast(context->update);

	extraFlags = 0;
	headerLength = 6;
	orderType = cache_bitmap_v2->compressed ? ORDER_TYPE_BITMAP_COMPRESSED_V2
	                                        : ORDER_TYPE_BITMAP_UNCOMPRESSED_V2;

	if (context->settings->NoBitmapCompressionHeader)
		cache_bitmap_v2->flags |= CBR2_NO_BITMAP_COMPRESSION_HDR;

	update_check_flush(context, headerLength +
	                                update_approximate_cache_bitmap_v2_order(
	                                    cache_bitmap_v2, cache_bitmap_v2->compressed, &extraFlags));
	s = update->us;

	if (!s)
		return FALSE;

	bm = Stream_GetPosition(s);

	if (!Stream_EnsureRemainingCapacity(s, headerLength))
		return FALSE;

	Stream_Seek(s, headerLength);

	if (!update_write_cache_bitmap_v2_order(s, cache_bitmap_v2, cache_bitmap_v2->compressed,
	                                        &extraFlags))
		return FALSE;

	em = Stream_GetPosition(s);
	orderLength = (em - bm) - 13;
	Stream_SetPosition(s, bm);
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags);                      /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType);                        /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_bitmap_v3(rdpContext* context, CACHE_BITMAP_V3_ORDER* cache_bitmap_v3)
{
	wStream* s;
	size_t bm, em;
	BYTE orderType;
	int headerLength;
	UINT16 extraFlags;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_bitmap_v3);
	update = update_cast(context->update);

	extraFlags = 0;
	headerLength = 6;
	orderType = ORDER_TYPE_BITMAP_COMPRESSED_V3;
	update_check_flush(context, headerLength + update_approximate_cache_bitmap_v3_order(
	                                               cache_bitmap_v3, &extraFlags));
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
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, extraFlags);                      /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, orderType);                        /* orderType (1 byte) */
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
	size_t headerLength;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_color_table);
	update = update_cast(context->update);

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
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags);                           /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_COLOR_TABLE);     /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_glyph(rdpContext* context, const CACHE_GLYPH_ORDER* cache_glyph)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_glyph);
	update = update_cast(context->update);

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
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags);                           /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_GLYPH);           /* orderType (1 byte) */
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
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_glyph_v2);
	update = update_cast(context->update);

	flags = 0;
	headerLength = 6;
	inf = update_approximate_cache_glyph_v2_order(cache_glyph_v2, &flags);
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
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags);                           /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_GLYPH);           /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_cache_brush(rdpContext* context, const CACHE_BRUSH_ORDER* cache_brush)
{
	wStream* s;
	UINT16 flags;
	size_t bm, em, inf;
	int headerLength;
	INT16 orderLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cache_brush);
	update = update_cast(context->update);

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
	Stream_Write_UINT8(s, ORDER_STANDARD | ORDER_SECONDARY); /* controlFlags (1 byte) */
	Stream_Write_UINT16(s, orderLength);                     /* orderLength (2 bytes) */
	Stream_Write_UINT16(s, flags);                           /* extraFlags (2 bytes) */
	Stream_Write_UINT8(s, ORDER_TYPE_CACHE_BRUSH);           /* orderType (1 byte) */
	Stream_SetPosition(s, em);
	update->numberOrders++;
	return TRUE;
}

/**
 * Alternate Secondary Drawing Orders
 */

static BOOL update_send_create_offscreen_bitmap_order(
    rdpContext* context, const CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	wStream* s;
	size_t bm, em, inf;
	BYTE orderType;
	BYTE controlFlags;
	size_t headerLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(create_offscreen_bitmap);
	update = update_cast(context->update);

	headerLength = 1;
	orderType = ORDER_TYPE_CREATE_OFFSCREEN_BITMAP;
	controlFlags = ORDER_SECONDARY | (orderType << 2);
	inf = update_approximate_create_offscreen_bitmap_order(create_offscreen_bitmap);
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

static BOOL update_send_switch_surface_order(rdpContext* context,
                                             const SWITCH_SURFACE_ORDER* switch_surface)
{
	wStream* s;
	size_t bm, em, inf;
	BYTE orderType;
	BYTE controlFlags;
	size_t headerLength;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(switch_surface);
	update = update_cast(context->update);

	headerLength = 1;
	orderType = ORDER_TYPE_SWITCH_SURFACE;
	controlFlags = ORDER_SECONDARY | (orderType << 2);
	inf = update_approximate_switch_surface_order(switch_surface);
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

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret;

	WINPR_ASSERT(rdp);
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
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 16))
		goto out_fail;

	Stream_Write_UINT16(s, pointerPosition->xPos); /* xPos (2 bytes) */
	Stream_Write_UINT16(s, pointerPosition->yPos); /* yPos (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_PTR_POSITION, s, FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_write_pointer_color(wStream* s, const POINTER_COLOR_UPDATE* pointer_color)
{
	WINPR_ASSERT(pointer_color);
	if (!Stream_EnsureRemainingCapacity(s, 32 + pointer_color->lengthAndMask +
	                                           pointer_color->lengthXorMask))
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

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(pointer_color);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_pointer_color(s, pointer_color))
		goto out_fail;

	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_COLOR, s, FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_write_pointer_large(wStream* s, const POINTER_LARGE_UPDATE* pointer)
{
	WINPR_ASSERT(pointer);

	if (!Stream_EnsureRemainingCapacity(s, 32 + pointer->lengthAndMask + pointer->lengthXorMask))
		return FALSE;

	Stream_Write_UINT16(s, pointer->xorBpp);
	Stream_Write_UINT16(s, pointer->cacheIndex);
	Stream_Write_UINT16(s, pointer->hotSpotX);
	Stream_Write_UINT16(s, pointer->hotSpotY);
	Stream_Write_UINT16(s, pointer->width);
	Stream_Write_UINT16(s, pointer->height);
	Stream_Write_UINT32(s, pointer->lengthAndMask);
	Stream_Write_UINT32(s, pointer->lengthXorMask);
	Stream_Write(s, pointer->xorMaskData, pointer->lengthXorMask);
	Stream_Write(s, pointer->andMaskData, pointer->lengthAndMask);
	Stream_Write_UINT8(s, 0); /* pad (1 byte) */
	return TRUE;
}

static BOOL update_send_pointer_large(rdpContext* context, const POINTER_LARGE_UPDATE* pointer)
{
	wStream* s;
	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(pointer);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!update_write_pointer_large(s, pointer))
		goto out_fail;

	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_LARGE_POINTER, s, FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_pointer_new(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new)
{
	wStream* s;

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(pointer_new);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 16))
		goto out_fail;

	Stream_Write_UINT16(s, pointer_new->xorBpp); /* xorBpp (2 bytes) */
	update_write_pointer_color(s, &pointer_new->colorPtrAttr);
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_POINTER, s, FALSE);
out_fail:
	Stream_Release(s);
	return ret;
}

static BOOL update_send_pointer_cached(rdpContext* context,
                                       const POINTER_CACHED_UPDATE* pointer_cached)
{
	wStream* s;

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	BOOL ret;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(pointer_cached);
	s = fastpath_update_pdu_init(rdp->fastpath);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, pointer_cached->cacheIndex); /* cacheIndex (2 bytes) */
	ret = fastpath_send_update_pdu(rdp->fastpath, FASTPATH_UPDATETYPE_CACHED, s, FALSE);
	Stream_Release(s);
	return ret;
}

BOOL update_read_refresh_rect(rdpUpdate* update, wStream* s)
{
	BYTE numberOfAreas;
	RECTANGLE_16 areas[256] = { 0 };
	rdp_update_internal* up = update_cast(update);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, numberOfAreas);
	Stream_Seek(s, 3); /* pad3Octects */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8ull * numberOfAreas))
		return FALSE;

	for (BYTE index = 0; index < numberOfAreas; index++)
	{
		RECTANGLE_16* area = &areas[index];

		Stream_Read_UINT16(s, area->left);
		Stream_Read_UINT16(s, area->top);
		Stream_Read_UINT16(s, area->right);
		Stream_Read_UINT16(s, area->bottom);
	}

	WINPR_ASSERT(update->context);
	WINPR_ASSERT(update->context->settings);
	if (update->context->settings->RefreshRect)
		IFCALL(update->RefreshRect, update->context, numberOfAreas, areas);
	else
		WLog_Print(up->log, WLOG_WARN, "ignoring refresh rect request from client");

	return TRUE;
}

BOOL update_read_suppress_output(rdpUpdate* update, wStream* s)
{
	rdp_update_internal* up = update_cast(update);
	RECTANGLE_16* prect = NULL;
	RECTANGLE_16 rect = { 0 };
	BYTE allowDisplayUpdates;

	WINPR_ASSERT(up);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, allowDisplayUpdates);
	Stream_Seek(s, 3); /* pad3Octects */

	if (allowDisplayUpdates > 0)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(RECTANGLE_16)))
			return FALSE;

		Stream_Read_UINT16(s, rect.left);
		Stream_Read_UINT16(s, rect.top);
		Stream_Read_UINT16(s, rect.right);
		Stream_Read_UINT16(s, rect.bottom);

		prect = &rect;
	}

	WINPR_ASSERT(update->context);
	WINPR_ASSERT(update->context->settings);
	if (update->context->settings->SuppressOutput)
		IFCALL(update->SuppressOutput, update->context, allowDisplayUpdates, prect);
	else
		WLog_Print(up->log, WLOG_WARN, "ignoring suppress output request from client");

	return TRUE;
}

static BOOL update_send_set_keyboard_indicators(rdpContext* context, UINT16 led_flags)
{
	wStream* s;

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Write_UINT16(s, 0);         /* unitId should be 0 according to MS-RDPBCGR 2.2.8.2.1.1 */
	Stream_Write_UINT16(s, led_flags); /* ledFlags (2 bytes) */

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SET_KEYBOARD_INDICATORS, rdp->mcs->userId);
}

static BOOL update_send_set_keyboard_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                                UINT32 imeConvMode)
{
	wStream* s;

	WINPR_ASSERT(context);
	rdpRdp* rdp = context->rdp;
	s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	/* unitId should be 0 according to MS-RDPBCGR 2.2.8.2.2.1 */
	Stream_Write_UINT16(s, imeId);
	Stream_Write_UINT32(s, imeState);
	Stream_Write_UINT32(s, imeConvMode);

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SET_KEYBOARD_IME_STATUS, rdp->mcs->userId);
}

static UINT16 update_calculate_new_or_existing_window(const WINDOW_ORDER_INFO* orderInfo,
                                                      const WINDOW_STATE_ORDER* stateOrder)
{
	UINT16 orderSize = 11;

	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(stateOrder);

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER) != 0)
		orderSize += 4;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW) != 0)
		orderSize += 1;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE) != 0)
		orderSize += 2 + stateOrder->titleInfo.length;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT) != 0)
		orderSize += 1;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT) != 0)
		orderSize += 4;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS) != 0)
		orderSize += 2 + stateOrder->numWindowRects * sizeof(RECTANGLE_16);

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET) != 0)
		orderSize += 8;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY) != 0)
		orderSize += 2 + stateOrder->numVisibilityRects * sizeof(RECTANGLE_16);

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OVERLAY_DESCRIPTION) != 0)
		orderSize += 2 + stateOrder->OverlayDescription.length;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TASKBAR_BUTTON) != 0)
		orderSize += 1;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ENFORCE_SERVER_ZORDER) != 0)
		orderSize += 1;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_STATE) != 0)
		orderSize += 1;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_EDGE) != 0)
		orderSize += 1;

	return orderSize;
}

static BOOL update_send_new_or_existing_window(rdpContext* context,
                                               const WINDOW_ORDER_INFO* orderInfo,
                                               const WINDOW_STATE_ORDER* stateOrder)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = update_calculate_new_or_existing_window(orderInfo, stateOrder);
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(stateOrder);

	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, orderSize))
		return FALSE;

	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);   /* WindowID (4 bytes) */

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER) != 0)
		Stream_Write_UINT32(s, stateOrder->ownerWindowId);

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->style);
		Stream_Write_UINT32(s, stateOrder->extendedStyle);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->showState);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE) != 0)
	{
		Stream_Write_UINT16(s, stateOrder->titleInfo.length);
		Stream_Write(s, stateOrder->titleInfo.string, stateOrder->titleInfo.length);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET) != 0)
	{
		Stream_Write_INT32(s, stateOrder->clientOffsetX);
		Stream_Write_INT32(s, stateOrder->clientOffsetY);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->clientAreaWidth);
		Stream_Write_UINT32(s, stateOrder->clientAreaHeight);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_X) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->resizeMarginLeft);
		Stream_Write_UINT32(s, stateOrder->resizeMarginRight);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RESIZE_MARGIN_Y) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->resizeMarginTop);
		Stream_Write_UINT32(s, stateOrder->resizeMarginBottom);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->RPContent);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->rootParentHandle);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) != 0)
	{
		Stream_Write_INT32(s, stateOrder->windowOffsetX);
		Stream_Write_INT32(s, stateOrder->windowOffsetY);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA) != 0)
	{
		Stream_Write_INT32(s, stateOrder->windowClientDeltaX);
		Stream_Write_INT32(s, stateOrder->windowClientDeltaY);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->windowWidth);
		Stream_Write_UINT32(s, stateOrder->windowHeight);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS) != 0)
	{
		Stream_Write_UINT16(s, stateOrder->numWindowRects);
		Stream_Write(s, stateOrder->windowRects, stateOrder->numWindowRects * sizeof(RECTANGLE_16));
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET) != 0)
	{
		Stream_Write_UINT32(s, stateOrder->visibleOffsetX);
		Stream_Write_UINT32(s, stateOrder->visibleOffsetY);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY) != 0)
	{
		Stream_Write_UINT16(s, stateOrder->numVisibilityRects);
		Stream_Write(s, stateOrder->visibilityRects,
		             stateOrder->numVisibilityRects * sizeof(RECTANGLE_16));
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OVERLAY_DESCRIPTION) != 0)
	{
		Stream_Write_UINT16(s, stateOrder->OverlayDescription.length);
		Stream_Write(s, stateOrder->OverlayDescription.string,
		             stateOrder->OverlayDescription.length);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TASKBAR_BUTTON) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->TaskbarButton);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ENFORCE_SERVER_ZORDER) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->EnforceServerZOrder);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_STATE) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->AppBarState);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_APPBAR_EDGE) != 0)
	{
		Stream_Write_UINT8(s, stateOrder->AppBarEdge);
	}

	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_window_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                      const WINDOW_STATE_ORDER* stateOrder)
{
	return update_send_new_or_existing_window(context, orderInfo, stateOrder);
}

static BOOL update_send_window_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                      const WINDOW_STATE_ORDER* stateOrder)
{
	return update_send_new_or_existing_window(context, orderInfo, stateOrder);
}

static UINT16 update_calculate_window_icon_order(const WINDOW_ORDER_INFO* orderInfo,
                                                 const WINDOW_ICON_ORDER* iconOrder)
{
	UINT16 orderSize = 23;

	WINPR_ASSERT(iconOrder);
	ICON_INFO* iconInfo = iconOrder->iconInfo;
	WINPR_ASSERT(iconInfo);

	orderSize += iconInfo->cbBitsColor + iconInfo->cbBitsMask;

	if (iconInfo->bpp <= 8)
		orderSize += 2 + iconInfo->cbColorTable;

	return orderSize;
}

static BOOL update_send_window_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                    const WINDOW_ICON_ORDER* iconOrder)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);

	WINPR_ASSERT(iconOrder);
	ICON_INFO* iconInfo = iconOrder->iconInfo;
	UINT16 orderSize = update_calculate_window_icon_order(orderInfo, iconOrder);
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(iconInfo);

	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s || !iconInfo)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, orderSize))
		return FALSE;

	/* Write Hdr */
	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);   /* WindowID (4 bytes) */
	/* Write body */
	Stream_Write_UINT16(s, iconInfo->cacheEntry); /* CacheEntry (2 bytes) */
	Stream_Write_UINT8(s, iconInfo->cacheId);     /* CacheId (1 byte) */
	Stream_Write_UINT8(s, iconInfo->bpp);         /* Bpp (1 byte) */
	Stream_Write_UINT16(s, iconInfo->width);      /* Width (2 bytes) */
	Stream_Write_UINT16(s, iconInfo->height);     /* Height (2 bytes) */

	if (iconInfo->bpp <= 8)
	{
		Stream_Write_UINT16(s, iconInfo->cbColorTable); /* CbColorTable (2 bytes) */
	}

	Stream_Write_UINT16(s, iconInfo->cbBitsMask);              /* CbBitsMask (2 bytes) */
	Stream_Write_UINT16(s, iconInfo->cbBitsColor);             /* CbBitsColor (2 bytes) */
	Stream_Write(s, iconInfo->bitsMask, iconInfo->cbBitsMask); /* BitsMask (variable) */

	if (iconInfo->bpp <= 8)
	{
		Stream_Write(s, iconInfo->colorTable, iconInfo->cbColorTable); /* ColorTable (variable) */
	}

	Stream_Write(s, iconInfo->bitsColor, iconInfo->cbBitsColor); /* BitsColor (variable) */

	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_window_cached_icon(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                           const WINDOW_CACHED_ICON_ORDER* cachedIconOrder)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = 14;

	WINPR_ASSERT(cachedIconOrder);
	const CACHED_ICON_INFO* cachedIcon = &cachedIconOrder->cachedIcon;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(cachedIcon);

	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, orderSize))
		return FALSE;

	/* Write Hdr */
	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);   /* WindowID (4 bytes) */
	/* Write body */
	Stream_Write_UINT16(s, cachedIcon->cacheEntry); /* CacheEntry (2 bytes) */
	Stream_Write_UINT8(s, cachedIcon->cacheId);     /* CacheId (1 byte) */
	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_window_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = 11;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, orderSize))
		return FALSE;

	/* Write Hdr */
	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);   /* WindowID (4 bytes) */
	update->numberOrders++;
	return TRUE;
}

static UINT16 update_calculate_new_or_existing_notification_icons_order(
    const WINDOW_ORDER_INFO* orderInfo, const NOTIFY_ICON_STATE_ORDER* iconStateOrder)
{
	UINT16 orderSize = 15;

	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(iconStateOrder);

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION) != 0)
		orderSize += 4;

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP) != 0)
	{
		orderSize += 2 + iconStateOrder->toolTip.length;
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP) != 0)
	{
		NOTIFY_ICON_INFOTIP infoTip = iconStateOrder->infoTip;
		orderSize += 12 + infoTip.text.length + infoTip.title.length;
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE) != 0)
	{
		orderSize += 4;
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_ICON) != 0)
	{
		ICON_INFO iconInfo = iconStateOrder->icon;
		orderSize += 12;

		if (iconInfo.bpp <= 8)
			orderSize += 2 + iconInfo.cbColorTable;

		orderSize += iconInfo.cbBitsMask + iconInfo.cbBitsColor;
	}
	else if ((orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON) != 0)
	{
		orderSize += 3;
	}

	return orderSize;
}

static BOOL
update_send_new_or_existing_notification_icons(rdpContext* context,
                                               const WINDOW_ORDER_INFO* orderInfo,
                                               const NOTIFY_ICON_STATE_ORDER* iconStateOrder)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	BOOL versionFieldPresent = FALSE;
	const UINT16 orderSize =
	    update_calculate_new_or_existing_notification_icons_order(orderInfo, iconStateOrder);
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(iconStateOrder);
	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, orderSize))
		return FALSE;

	/* Write Hdr */
	Stream_Write_UINT8(s, controlFlags);             /* Header (1 byte) */
	Stream_Write_INT16(s, orderSize);                /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags);   /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);     /* WindowID (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->notifyIconId); /* NotifyIconId (4 bytes) */

	/* Write body */
	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION) != 0)
	{
		versionFieldPresent = TRUE;
		Stream_Write_UINT32(s, iconStateOrder->version);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP) != 0)
	{
		Stream_Write_UINT16(s, iconStateOrder->toolTip.length);
		Stream_Write(s, iconStateOrder->toolTip.string, iconStateOrder->toolTip.length);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP) != 0)
	{
		NOTIFY_ICON_INFOTIP infoTip = iconStateOrder->infoTip;

		/* info tip should not be sent when version is 0 */
		if (versionFieldPresent && iconStateOrder->version == 0)
			return FALSE;

		Stream_Write_UINT32(s, infoTip.timeout);     /* Timeout (4 bytes) */
		Stream_Write_UINT32(s, infoTip.flags);       /* InfoFlags (4 bytes) */
		Stream_Write_UINT16(s, infoTip.text.length); /* InfoTipText (variable) */
		Stream_Write(s, infoTip.text.string, infoTip.text.length);
		Stream_Write_UINT16(s, infoTip.title.length); /* Title (variable) */
		Stream_Write(s, infoTip.title.string, infoTip.title.length);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE) != 0)
	{
		/* notify state should not be sent when version is 0 */
		if (versionFieldPresent && iconStateOrder->version == 0)
			return FALSE;

		Stream_Write_UINT32(s, iconStateOrder->state);
	}

	if ((orderInfo->fieldFlags & WINDOW_ORDER_ICON) != 0)
	{
		ICON_INFO iconInfo = iconStateOrder->icon;
		Stream_Write_UINT16(s, iconInfo.cacheEntry); /* CacheEntry (2 bytes) */
		Stream_Write_UINT8(s, iconInfo.cacheId);     /* CacheId (1 byte) */
		Stream_Write_UINT8(s, iconInfo.bpp);         /* Bpp (1 byte) */
		Stream_Write_UINT16(s, iconInfo.width);      /* Width (2 bytes) */
		Stream_Write_UINT16(s, iconInfo.height);     /* Height (2 bytes) */

		if (iconInfo.bpp <= 8)
		{
			Stream_Write_UINT16(s, iconInfo.cbColorTable); /* CbColorTable (2 bytes) */
		}

		Stream_Write_UINT16(s, iconInfo.cbBitsMask);             /* CbBitsMask (2 bytes) */
		Stream_Write_UINT16(s, iconInfo.cbBitsColor);            /* CbBitsColor (2 bytes) */
		Stream_Write(s, iconInfo.bitsMask, iconInfo.cbBitsMask); /* BitsMask (variable) */

		if (iconInfo.bpp <= 8)
		{
			Stream_Write(s, iconInfo.colorTable, iconInfo.cbColorTable); /* ColorTable (variable) */
		}

		Stream_Write(s, iconInfo.bitsColor, iconInfo.cbBitsColor); /* BitsColor (variable) */
	}
	else if ((orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON) != 0)
	{
		CACHED_ICON_INFO cachedIcon = iconStateOrder->cachedIcon;
		Stream_Write_UINT16(s, cachedIcon.cacheEntry); /* CacheEntry (2 bytes) */
		Stream_Write_UINT8(s, cachedIcon.cacheId);     /* CacheId (1 byte) */
	}

	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_notify_icon_create(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                           const NOTIFY_ICON_STATE_ORDER* iconStateOrder)
{
	return update_send_new_or_existing_notification_icons(context, orderInfo, iconStateOrder);
}

static BOOL update_send_notify_icon_update(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                           const NOTIFY_ICON_STATE_ORDER* iconStateOrder)
{
	return update_send_new_or_existing_notification_icons(context, orderInfo, iconStateOrder);
}

static BOOL update_send_notify_icon_delete(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = 15;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s)
		return FALSE;

	/* Write Hdr */
	Stream_Write_UINT8(s, controlFlags);             /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);               /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags);   /* FieldsPresentFlags (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->windowId);     /* WindowID (4 bytes) */
	Stream_Write_UINT32(s, orderInfo->notifyIconId); /* NotifyIconId (4 bytes) */
	update->numberOrders++;
	return TRUE;
}

static UINT16 update_calculate_monitored_desktop(const WINDOW_ORDER_INFO* orderInfo,
                                                 const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	UINT16 orderSize = 7;

	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(monitoredDesktop);

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
	{
		orderSize += 4;
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		orderSize += 1 + (4 * monitoredDesktop->numWindowIds);
	}

	return orderSize;
}

static BOOL update_send_monitored_desktop(rdpContext* context, const WINDOW_ORDER_INFO* orderInfo,
                                          const MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	UINT32 i;
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = update_calculate_monitored_desktop(orderInfo, monitoredDesktop);
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	WINPR_ASSERT(monitoredDesktop);

	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s)
		return FALSE;

	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ACTIVE_WND)
	{
		Stream_Write_UINT32(s, monitoredDesktop->activeWindowId); /* activeWindowId (4 bytes) */
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_DESKTOP_ZORDER)
	{
		Stream_Write_UINT8(s, monitoredDesktop->numWindowIds); /* numWindowIds (1 byte) */

		/* windowIds */
		for (i = 0; i < monitoredDesktop->numWindowIds; i++)
		{
			Stream_Write_UINT32(s, monitoredDesktop->windowIds[i]);
		}
	}

	update->numberOrders++;
	return TRUE;
}

static BOOL update_send_non_monitored_desktop(rdpContext* context,
                                              const WINDOW_ORDER_INFO* orderInfo)
{
	wStream* s;
	BYTE controlFlags = ORDER_SECONDARY | (ORDER_TYPE_WINDOW << 2);
	UINT16 orderSize = 7;
	rdp_update_internal* update;

	WINPR_ASSERT(context);
	WINPR_ASSERT(orderInfo);
	update = update_cast(context->update);

	update_check_flush(context, orderSize);

	s = update->us;

	if (!s)
		return FALSE;

	Stream_Write_UINT8(s, controlFlags);           /* Header (1 byte) */
	Stream_Write_UINT16(s, orderSize);             /* OrderSize (2 bytes) */
	Stream_Write_UINT32(s, orderInfo->fieldFlags); /* FieldsPresentFlags (4 bytes) */
	update->numberOrders++;
	return TRUE;
}

void update_register_server_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);

	update->BeginPaint = _update_begin_paint;
	update->EndPaint = _update_end_paint;
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
	update->ServerStatusInfo = rdp_send_server_status_info;
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
	update->altsec->CreateOffscreenBitmap = update_send_create_offscreen_bitmap_order;
	update->altsec->SwitchSurface = update_send_switch_surface_order;
	update->pointer->PointerSystem = update_send_pointer_system;
	update->pointer->PointerPosition = update_send_pointer_position;
	update->pointer->PointerColor = update_send_pointer_color;
	update->pointer->PointerLarge = update_send_pointer_large;
	update->pointer->PointerNew = update_send_pointer_new;
	update->pointer->PointerCached = update_send_pointer_cached;
	update->window->WindowCreate = update_send_window_create;
	update->window->WindowUpdate = update_send_window_update;
	update->window->WindowIcon = update_send_window_icon;
	update->window->WindowCachedIcon = update_send_window_cached_icon;
	update->window->WindowDelete = update_send_window_delete;
	update->window->NotifyIconCreate = update_send_notify_icon_create;
	update->window->NotifyIconUpdate = update_send_notify_icon_update;
	update->window->NotifyIconDelete = update_send_notify_icon_delete;
	update->window->MonitoredDesktop = update_send_monitored_desktop;
	update->window->NonMonitoredDesktop = update_send_non_monitored_desktop;
}

void update_register_client_callbacks(rdpUpdate* update)
{
	WINPR_ASSERT(update);

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

void update_free_window_state(WINDOW_STATE_ORDER* window_state)
{
	if (!window_state)
		return;

	free(window_state->OverlayDescription.string);
	free(window_state->titleInfo.string);
	free(window_state->windowRects);
	free(window_state->visibilityRects);
	memset(window_state, 0, sizeof(WINDOW_STATE_ORDER));
}

rdpUpdate* update_new(rdpRdp* rdp)
{
	const wObject cb = { NULL, NULL, NULL, update_free_queued_message, NULL };
	rdp_update_internal* update;
	rdp_altsec_update_internal* altsec;
	rdp_primary_update_internal* primary;
	rdp_secondary_update_internal* secondary;
	OFFSCREEN_DELETE_LIST* deleteList;
	WINPR_UNUSED(rdp);
	update = (rdp_update_internal*)calloc(1, sizeof(rdp_update_internal));

	if (!update)
		return NULL;

	update->common.context = rdp->context;
	update->log = WLog_Get("com.freerdp.core.update");
	InitializeCriticalSection(&(update->mux));
	update->common.pointer = (rdpPointerUpdate*)calloc(1, sizeof(rdpPointerUpdate));

	if (!update->common.pointer)
		goto fail;

	primary = (rdp_primary_update_internal*)calloc(1, sizeof(rdp_primary_update_internal));

	if (!primary)
		goto fail;
	update->common.primary = &primary->common;

	secondary = (rdp_secondary_update_internal*)calloc(1, sizeof(rdp_secondary_update_internal));

	if (!secondary)
		goto fail;
	update->common.secondary = &secondary->common;

	altsec = (rdp_altsec_update_internal*)calloc(1, sizeof(rdp_altsec_update_internal));

	if (!altsec)
		goto fail;

	update->common.altsec = &altsec->common;
	update->common.window = (rdpWindowUpdate*)calloc(1, sizeof(rdpWindowUpdate));

	if (!update->common.window)
		goto fail;

	deleteList = &(altsec->create_offscreen_bitmap.deleteList);
	deleteList->sIndices = 64;
	deleteList->indices = calloc(deleteList->sIndices, 2);

	if (!deleteList->indices)
		goto fail;

	deleteList->cIndices = 0;
	update->common.SuppressOutput = update_send_suppress_output;
	update->initialState = TRUE;
	update->common.autoCalculateBitmapData = TRUE;
	update->queue = MessageQueue_New(&cb);

	if (!update->queue)
		goto fail;

	return &update->common;
fail:
	update_free(&update->common);
	return NULL;
}

void update_free(rdpUpdate* update)
{
	if (update != NULL)
	{
		rdp_update_internal* up = update_cast(update);
		rdp_altsec_update_internal* altsec = altsec_update_cast(update->altsec);
		OFFSCREEN_DELETE_LIST* deleteList = &(altsec->create_offscreen_bitmap.deleteList);

		if (deleteList)
			free(deleteList->indices);

		free(update->pointer);

		if (update->primary)
		{
			rdp_primary_update_internal* primary = primary_update_cast(update->primary);
			free(primary->polyline.points);
			free(primary->polygon_sc.points);
			free(primary->fast_glyph.glyphData.aj);
			free(primary);
		}

		free(update->secondary);
		free(altsec);

		if (update->window)
		{
			free(update->window);
		}

		MessageQueue_Free(up->queue);
		DeleteCriticalSection(&up->mux);
		free(update);
	}
}

void rdp_update_lock(rdpUpdate* update)
{
	rdp_update_internal* up = update_cast(update);
	EnterCriticalSection(&up->mux);
}

void rdp_update_unlock(rdpUpdate* update)
{
	rdp_update_internal* up = update_cast(update);
	LeaveCriticalSection(&up->mux);
}

BOOL update_begin_paint(rdpUpdate* update)
{
	WINPR_ASSERT(update);
	rdp_update_lock(update);

	if (!update->BeginPaint)
		return TRUE;

	return update->BeginPaint(update->context);
}

BOOL update_end_paint(rdpUpdate* update)
{
	BOOL rc = TRUE;

	if (!update)
		return FALSE;

	if (update->EndPaint)
		rc = update->EndPaint(update->context);

	rdp_update_unlock(update);
	return rc;
}
