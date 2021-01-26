/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Fast Path
 *
 * Copyright 2011 Vic Lee
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <freerdp/crypto/per.h>

#include "orders.h"
#include "update.h"
#include "surface.h"
#include "fastpath.h"
#include "rdp.h"

#include "../cache/pointer.h"
#include "../cache/palette.h"
#include "../cache/bitmap.h"

#define TAG FREERDP_TAG("core.fastpath")

/**
 * Fast-Path packet format is defined in [MS-RDPBCGR] 2.2.9.1.2, which revises
 * server output packets from the first byte with the goal of improving
 * bandwidth.
 *
 * Slow-Path packet always starts with TPKT header, which has the first
 * byte 0x03, while Fast-Path packet starts with 2 zero bits in the first
 * two less significant bits of the first byte.
 */

static const char* const FASTPATH_UPDATETYPE_STRINGS[] = {
	"Orders",                 /* 0x0 */
	"Bitmap",                 /* 0x1 */
	"Palette",                /* 0x2 */
	"Synchronize",            /* 0x3 */
	"Surface Commands",       /* 0x4 */
	"System Pointer Hidden",  /* 0x5 */
	"System Pointer Default", /* 0x6 */
	"???",                    /* 0x7 */
	"Pointer Position",       /* 0x8 */
	"Color Pointer",          /* 0x9 */
	"Cached Pointer",         /* 0xA */
	"New Pointer",            /* 0xB */
};

static const char* fastpath_update_to_string(UINT8 update)
{
	if (update >= ARRAYSIZE(FASTPATH_UPDATETYPE_STRINGS))
		return "UNKNOWN";

	return FASTPATH_UPDATETYPE_STRINGS[update];
}

/*
 * The fastpath header may be two or three bytes long.
 * This function assumes that at least two bytes are available in the stream
 * and doesn't touch third byte.
 */
UINT16 fastpath_header_length(wStream* s)
{
	BYTE length1;

	if (!s || (Stream_GetRemainingLength(s) < 2))
		return 0;

	Stream_Seek_UINT8(s);
	Stream_Read_UINT8(s, length1);
	Stream_Rewind(s, 2);
	return ((length1 & 0x80) != 0 ? 3 : 2);
}

/**
 * Read a Fast-Path packet header.\n
 * @param s stream
 * @param encryptionFlags
 * @return length
 */
UINT16 fastpath_read_header(rdpFastPath* fastpath, wStream* s)
{
	BYTE header;
	UINT16 length;

	if (!s || (Stream_GetRemainingLength(s) < 1))
		return 0;

	Stream_Read_UINT8(s, header);

	if (fastpath)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	if (!per_read_length(s, &length))
		return 0;

	return length;
}

static BOOL fastpath_read_update_header(wStream* s, BYTE* updateCode, BYTE* fragmentation,
                                        BYTE* compression)
{
	BYTE updateHeader;

	if (!s || !updateCode || !fragmentation || !compression)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, updateHeader);
	*updateCode = updateHeader & 0x0F;
	*fragmentation = (updateHeader >> 4) & 0x03;
	*compression = (updateHeader >> 6) & 0x03;
	return TRUE;
}

static BOOL fastpath_write_update_header(wStream* s, FASTPATH_UPDATE_HEADER* fpUpdateHeader)
{
	if (!s || !fpUpdateHeader)
		return FALSE;

	fpUpdateHeader->updateHeader = 0;
	fpUpdateHeader->updateHeader |= fpUpdateHeader->updateCode & 0x0F;
	fpUpdateHeader->updateHeader |= (fpUpdateHeader->fragmentation & 0x03) << 4;
	fpUpdateHeader->updateHeader |= (fpUpdateHeader->compression & 0x03) << 6;
	Stream_Write_UINT8(s, fpUpdateHeader->updateHeader);

	if (fpUpdateHeader->compression)
	{
		if (Stream_GetRemainingCapacity(s) < 1)
			return FALSE;

		Stream_Write_UINT8(s, fpUpdateHeader->compressionFlags);
	}

	if (Stream_GetRemainingCapacity(s) < 2)
		return FALSE;

	Stream_Write_UINT16(s, fpUpdateHeader->size);
	return TRUE;
}

static UINT32 fastpath_get_update_header_size(FASTPATH_UPDATE_HEADER* fpUpdateHeader)
{
	if (!fpUpdateHeader)
		return 0;

	return (fpUpdateHeader->compression) ? 4 : 3;
}

static BOOL fastpath_write_update_pdu_header(wStream* s,
                                             FASTPATH_UPDATE_PDU_HEADER* fpUpdatePduHeader,
                                             rdpRdp* rdp)
{
	if (!s || !fpUpdatePduHeader || !rdp)
		return FALSE;

	if (Stream_GetRemainingCapacity(s) < 3)
		return FALSE;

	fpUpdatePduHeader->fpOutputHeader = 0;
	fpUpdatePduHeader->fpOutputHeader |= (fpUpdatePduHeader->action & 0x03);
	fpUpdatePduHeader->fpOutputHeader |= (fpUpdatePduHeader->secFlags & 0x03) << 6;
	Stream_Write_UINT8(s, fpUpdatePduHeader->fpOutputHeader);       /* fpOutputHeader (1 byte) */
	Stream_Write_UINT8(s, 0x80 | (fpUpdatePduHeader->length >> 8)); /* length1 */
	Stream_Write_UINT8(s, fpUpdatePduHeader->length & 0xFF);        /* length2 */

	if (fpUpdatePduHeader->secFlags)
	{
		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
		{
			if (Stream_GetRemainingCapacity(s) < 4)
				return FALSE;

			Stream_Write(s, fpUpdatePduHeader->fipsInformation, 4);
		}

		if (Stream_GetRemainingCapacity(s) < 8)
			return FALSE;

		Stream_Write(s, fpUpdatePduHeader->dataSignature, 8);
	}

	return FALSE;
}

static UINT32 fastpath_get_update_pdu_header_size(FASTPATH_UPDATE_PDU_HEADER* fpUpdatePduHeader,
                                                  rdpRdp* rdp)
{
	UINT32 size = 3; /* fpUpdatePduHeader + length1 + length2 */

	if (!fpUpdatePduHeader || !rdp)
		return 0;

	if (fpUpdatePduHeader->secFlags)
	{
		size += 8; /* dataSignature */

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			size += 4; /* fipsInformation */
	}

	return size;
}

BOOL fastpath_read_header_rdp(rdpFastPath* fastpath, wStream* s, UINT16* length)
{
	BYTE header;

	if (!s || !length)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, header);

	if (fastpath)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	if (!per_read_length(s, length))
		return FALSE;

	*length = *length - Stream_GetPosition(s);
	return TRUE;
}

static BOOL fastpath_recv_orders(rdpFastPath* fastpath, wStream* s)
{
	rdpUpdate* update;
	UINT16 numberOrders;

	if (!fastpath || !fastpath->rdp || !s)
	{
		WLog_ERR(TAG, "Invalid arguments");
		return FALSE;
	}

	update = fastpath->rdp->update;

	if (!update)
	{
		WLog_ERR(TAG, "Invalid configuration");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_ERR(TAG, "Stream short");
		return FALSE;
	}

	Stream_Read_UINT16(s, numberOrders); /* numberOrders (2 bytes) */

	while (numberOrders > 0)
	{
		if (!update_recv_order(update, s))
			return FALSE;

		numberOrders--;
	}

	return TRUE;
}

static BOOL fastpath_recv_update_common(rdpFastPath* fastpath, wStream* s)
{
	BOOL rc = FALSE;
	UINT16 updateType;
	rdpUpdate* update;
	rdpContext* context;

	if (!fastpath || !s || !fastpath->rdp)
		return FALSE;

	update = fastpath->rdp->update;

	if (!update || !update->context)
		return FALSE;

	context = update->context;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, updateType); /* updateType (2 bytes) */
	switch (updateType)
	{
		case UPDATE_TYPE_BITMAP:
		{
			BITMAP_UPDATE* bitmap_update = update_read_bitmap_update(update, s);

			if (!bitmap_update)
				return FALSE;

			rc = IFCALLRESULT(FALSE, update->BitmapUpdate, context, bitmap_update);
			free_bitmap_update(context, bitmap_update);
		}
		break;

		case UPDATE_TYPE_PALETTE:
		{
			PALETTE_UPDATE* palette_update = update_read_palette(update, s);

			if (!palette_update)
				return FALSE;

			rc = IFCALLRESULT(FALSE, update->Palette, context, palette_update);
			free_palette_update(context, palette_update);
		}
		break;

		default:
			break;
	}

	return rc;
}

static BOOL fastpath_recv_update_synchronize(rdpFastPath* fastpath, wStream* s)
{
	/* server 2008 can send invalid synchronize packet with missing padding,
	  so don't return FALSE even if the packet is invalid */
	Stream_SafeSeek(s, 2); /* size (2 bytes), MUST be set to zero */
	return TRUE;
}

static int fastpath_recv_update(rdpFastPath* fastpath, BYTE updateCode, wStream* s)
{
	BOOL rc = FALSE;
	int status = 0;
	rdpUpdate* update;
	rdpContext* context;
	rdpPointerUpdate* pointer;

	if (!fastpath || !fastpath->rdp || !s)
		return -1;

	update = fastpath->rdp->update;

	if (!update || !update->pointer || !update->context)
		return -1;

	context = update->context;
	pointer = update->pointer;
#ifdef WITH_DEBUG_RDP
	DEBUG_RDP("recv Fast-Path %s Update (0x%02" PRIX8 "), length:%" PRIuz "",
	          fastpath_update_to_string(updateCode), updateCode, Stream_GetRemainingLength(s));
#endif

	switch (updateCode)
	{
		case FASTPATH_UPDATETYPE_ORDERS:
			rc = fastpath_recv_orders(fastpath, s);
			break;

		case FASTPATH_UPDATETYPE_BITMAP:
		case FASTPATH_UPDATETYPE_PALETTE:
			rc = fastpath_recv_update_common(fastpath, s);
			break;

		case FASTPATH_UPDATETYPE_SYNCHRONIZE:
			if (!fastpath_recv_update_synchronize(fastpath, s))
				WLog_ERR(TAG, "fastpath_recv_update_synchronize failure but we continue");
			else
				rc = IFCALLRESULT(TRUE, update->Synchronize, context);

			break;

		case FASTPATH_UPDATETYPE_SURFCMDS:
			status = update_recv_surfcmds(update, s);
			rc = (status < 0) ? FALSE : TRUE;
			break;

		case FASTPATH_UPDATETYPE_PTR_NULL:
		{
			POINTER_SYSTEM_UPDATE pointer_system;
			pointer_system.type = SYSPTR_NULL;
			rc = IFCALLRESULT(FALSE, pointer->PointerSystem, context, &pointer_system);
		}
		break;

		case FASTPATH_UPDATETYPE_PTR_DEFAULT:
		{
			POINTER_SYSTEM_UPDATE pointer_system;
			pointer_system.type = SYSPTR_DEFAULT;
			rc = IFCALLRESULT(FALSE, pointer->PointerSystem, context, &pointer_system);
		}
		break;

		case FASTPATH_UPDATETYPE_PTR_POSITION:
		{
			POINTER_POSITION_UPDATE* pointer_position = update_read_pointer_position(update, s);

			if (pointer_position)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerPosition, context, pointer_position);
				free_pointer_position_update(context, pointer_position);
			}
		}
		break;

		case FASTPATH_UPDATETYPE_COLOR:
		{
			POINTER_COLOR_UPDATE* pointer_color = update_read_pointer_color(update, s, 24);

			if (pointer_color)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerColor, context, pointer_color);
				free_pointer_color_update(context, pointer_color);
			}
		}
		break;

		case FASTPATH_UPDATETYPE_CACHED:
		{
			POINTER_CACHED_UPDATE* pointer_cached = update_read_pointer_cached(update, s);

			if (pointer_cached)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerCached, context, pointer_cached);
				free_pointer_cached_update(context, pointer_cached);
			}
		}
		break;

		case FASTPATH_UPDATETYPE_POINTER:
		{
			POINTER_NEW_UPDATE* pointer_new = update_read_pointer_new(update, s);

			if (pointer_new)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerNew, context, pointer_new);
				free_pointer_new_update(context, pointer_new);
			}
		}
		break;

		case FASTPATH_UPDATETYPE_LARGE_POINTER:
		{
			POINTER_LARGE_UPDATE* pointer_large = update_read_pointer_large(update, s);

			if (pointer_large)
			{
				rc = IFCALLRESULT(FALSE, pointer->PointerLarge, context, pointer_large);
				free_pointer_large_update(context, pointer_large);
			}
		}
		break;
		default:
			break;
	}

	if (!rc)
	{
		WLog_ERR(TAG, "Fastpath update %s [%" PRIx8 "] failed, status %d",
		         fastpath_update_to_string(updateCode), updateCode, status);
		return -1;
	}

	return status;
}

static int fastpath_recv_update_data(rdpFastPath* fastpath, wStream* s)
{
	int status;
	UINT16 size;
	rdpRdp* rdp;
	int bulkStatus;
	BYTE updateCode;
	BYTE fragmentation;
	BYTE compression;
	BYTE compressionFlags;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	rdpTransport* transport;

	if (!fastpath || !s)
		return -1;

	status = 0;
	rdp = fastpath->rdp;

	if (!rdp)
		return -1;

	transport = fastpath->rdp->transport;

	if (!transport)
		return -1;

	if (!fastpath_read_update_header(s, &updateCode, &fragmentation, &compression))
		return -1;

	if (compression == FASTPATH_OUTPUT_COMPRESSION_USED)
	{
		if (Stream_GetRemainingLength(s) < 1)
			return -1;

		Stream_Read_UINT8(s, compressionFlags);
	}
	else
		compressionFlags = 0;

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, size);

	if (Stream_GetRemainingLength(s) < size)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength() < size");
		return -1;
	}

	bulkStatus =
	    bulk_decompress(rdp->bulk, Stream_Pointer(s), size, &pDstData, &DstSize, compressionFlags);
	Stream_Seek(s, size);

	if (bulkStatus < 0)
	{
		WLog_ERR(TAG, "bulk_decompress() failed");
		return -1;
	}

	if (!Stream_EnsureRemainingCapacity(fastpath->updateData, DstSize))
		return -1;

	Stream_Write(fastpath->updateData, pDstData, DstSize);

	if (fragmentation == FASTPATH_FRAGMENT_SINGLE)
	{
		if (fastpath->fragmentation != -1)
		{
			WLog_ERR(TAG, "Unexpected FASTPATH_FRAGMENT_SINGLE");
			goto out_fail;
		}

		Stream_SealLength(fastpath->updateData);
		Stream_SetPosition(fastpath->updateData, 0);
		status = fastpath_recv_update(fastpath, updateCode, fastpath->updateData);
		Stream_SetPosition(fastpath->updateData, 0);

		if (status < 0)
		{
			WLog_ERR(TAG, "fastpath_recv_update() - %i", status);
			goto out_fail;
		}
	}
	else
	{
		const size_t totalSize = Stream_GetPosition(fastpath->updateData);

		if (totalSize > transport->settings->MultifragMaxRequestSize)
		{
			WLog_ERR(TAG, "Total size (%" PRIuz ") exceeds MultifragMaxRequestSize (%" PRIu32 ")",
			         totalSize, transport->settings->MultifragMaxRequestSize);
			goto out_fail;
		}

		if (fragmentation == FASTPATH_FRAGMENT_FIRST)
		{
			if (fastpath->fragmentation != -1)
			{
				WLog_ERR(TAG, "fastpath_recv_update_data: Unexpected FASTPATH_FRAGMENT_FIRST");
				goto out_fail;
			}

			fastpath->fragmentation = FASTPATH_FRAGMENT_FIRST;
		}
		else if (fragmentation == FASTPATH_FRAGMENT_NEXT)
		{
			if ((fastpath->fragmentation != FASTPATH_FRAGMENT_FIRST) &&
			    (fastpath->fragmentation != FASTPATH_FRAGMENT_NEXT))
			{
				WLog_ERR(TAG, "fastpath_recv_update_data: Unexpected FASTPATH_FRAGMENT_NEXT");
				goto out_fail;
			}

			fastpath->fragmentation = FASTPATH_FRAGMENT_NEXT;
		}
		else if (fragmentation == FASTPATH_FRAGMENT_LAST)
		{
			if ((fastpath->fragmentation != FASTPATH_FRAGMENT_FIRST) &&
			    (fastpath->fragmentation != FASTPATH_FRAGMENT_NEXT))
			{
				WLog_ERR(TAG, "fastpath_recv_update_data: Unexpected FASTPATH_FRAGMENT_LAST");
				goto out_fail;
			}

			fastpath->fragmentation = -1;
			Stream_SealLength(fastpath->updateData);
			Stream_SetPosition(fastpath->updateData, 0);
			status = fastpath_recv_update(fastpath, updateCode, fastpath->updateData);
			Stream_SetPosition(fastpath->updateData, 0);

			if (status < 0)
			{
				WLog_ERR(TAG, "fastpath_recv_update_data: fastpath_recv_update() - %i", status);
				goto out_fail;
			}
		}
	}

	return status;
out_fail:
	return -1;
}

int fastpath_recv_updates(rdpFastPath* fastpath, wStream* s)
{
	int rc = -2;
	rdpUpdate* update;

	if (!fastpath || !fastpath->rdp || !fastpath->rdp->update || !s)
		return -1;

	update = fastpath->rdp->update;

	if (!update_begin_paint(update))
		goto fail;

	while (Stream_GetRemainingLength(s) >= 3)
	{
		if (fastpath_recv_update_data(fastpath, s) < 0)
		{
			WLog_ERR(TAG, "fastpath_recv_update_data() fail");
			rc = -3;
			goto fail;
		}
	}

	rc = 0;
fail:

	if (!update_end_paint(update))
		return -4;

	return rc;
}

static BOOL fastpath_read_input_event_header(wStream* s, BYTE* eventFlags, BYTE* eventCode)
{
	BYTE eventHeader;

	if (!s || !eventFlags || !eventCode)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, eventHeader); /* eventHeader (1 byte) */
	*eventFlags = (eventHeader & 0x1F);
	*eventCode = (eventHeader >> 5);
	return TRUE;
}

static BOOL fastpath_recv_input_event_scancode(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	rdpInput* input;
	UINT16 flags;
	UINT16 code;

	if (!fastpath || !fastpath->rdp || !fastpath->rdp->input || !s)
		return FALSE;

	input = fastpath->rdp->input;

	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;

	Stream_Read_UINT8(s, code); /* keyCode (1 byte) */
	flags = 0;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED))
		flags |= KBD_FLAGS_EXTENDED;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_PREFIX_E1))
		flags |= KBD_FLAGS_EXTENDED1;

	return IFCALLRESULT(TRUE, input->KeyboardEvent, input, flags, code);
}

static BOOL fastpath_recv_input_event_mouse(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	rdpInput* input;
	UINT16 pointerFlags;
	UINT16 xPos;
	UINT16 yPos;

	if (!fastpath || !fastpath->rdp || !fastpath->rdp->input || !s)
		return FALSE;

	input = fastpath->rdp->input;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos);         /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos);         /* yPos (2 bytes) */
	return IFCALLRESULT(TRUE, input->MouseEvent, input, pointerFlags, xPos, yPos);
}

static BOOL fastpath_recv_input_event_mousex(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	rdpInput* input;
	UINT16 pointerFlags;
	UINT16 xPos;
	UINT16 yPos;

	if (!fastpath || !fastpath->rdp || !fastpath->rdp->input || !s)
		return FALSE;

	input = fastpath->rdp->input;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	Stream_Read_UINT16(s, xPos);         /* xPos (2 bytes) */
	Stream_Read_UINT16(s, yPos);         /* yPos (2 bytes) */
	return IFCALLRESULT(TRUE, input->ExtendedMouseEvent, input, pointerFlags, xPos, yPos);
}

static BOOL fastpath_recv_input_event_sync(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	rdpInput* input;

	if (!fastpath || !fastpath->rdp || !fastpath->rdp->input || !s)
		return FALSE;

	input = fastpath->rdp->input;
	return IFCALLRESULT(TRUE, input->SynchronizeEvent, input, eventFlags);
}

static BOOL fastpath_recv_input_event_unicode(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	UINT16 unicodeCode;
	UINT16 flags;

	if (!fastpath || !s)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, unicodeCode); /* unicodeCode (2 bytes) */
	flags = 0;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	return IFCALLRESULT(FALSE, fastpath->rdp->input->UnicodeKeyboardEvent, fastpath->rdp->input,
	                    flags, unicodeCode);
}

static BOOL fastpath_recv_input_event(rdpFastPath* fastpath, wStream* s)
{
	BYTE eventFlags;
	BYTE eventCode;

	if (!fastpath || !s)
		return FALSE;

	if (!fastpath_read_input_event_header(s, &eventFlags, &eventCode))
		return FALSE;

	switch (eventCode)
	{
		case FASTPATH_INPUT_EVENT_SCANCODE:
			if (!fastpath_recv_input_event_scancode(fastpath, s, eventFlags))
				return FALSE;

			break;

		case FASTPATH_INPUT_EVENT_MOUSE:
			if (!fastpath_recv_input_event_mouse(fastpath, s, eventFlags))
				return FALSE;

			break;

		case FASTPATH_INPUT_EVENT_MOUSEX:
			if (!fastpath_recv_input_event_mousex(fastpath, s, eventFlags))
				return FALSE;

			break;

		case FASTPATH_INPUT_EVENT_SYNC:
			if (!fastpath_recv_input_event_sync(fastpath, s, eventFlags))
				return FALSE;

			break;

		case FASTPATH_INPUT_EVENT_UNICODE:
			if (!fastpath_recv_input_event_unicode(fastpath, s, eventFlags))
				return FALSE;

			break;

		default:
			WLog_ERR(TAG, "Unknown eventCode %" PRIu8 "", eventCode);
			break;
	}

	return TRUE;
}

int fastpath_recv_inputs(rdpFastPath* fastpath, wStream* s)
{
	BYTE i;

	if (!fastpath || !s)
		return -1;

	if (fastpath->numberEvents == 0)
	{
		/**
		 * If numberEvents is not provided in fpInputHeader, it will be provided
		 * as one additional byte here.
		 */
		if (Stream_GetRemainingLength(s) < 1)
			return -1;

		Stream_Read_UINT8(s, fastpath->numberEvents); /* eventHeader (1 byte) */
	}

	for (i = 0; i < fastpath->numberEvents; i++)
	{
		if (!fastpath_recv_input_event(fastpath, s))
			return -1;
	}

	return 0;
}

static UINT32 fastpath_get_sec_bytes(rdpRdp* rdp)
{
	UINT32 sec_bytes;
	sec_bytes = 0;

	if (!rdp)
		return 0;

	if (rdp->do_crypt)
	{
		sec_bytes = 8;

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			sec_bytes += 4;
	}

	return sec_bytes;
}

wStream* fastpath_input_pdu_init_header(rdpFastPath* fastpath)
{
	rdpRdp* rdp;
	wStream* s;

	if (!fastpath || !fastpath->rdp)
		return NULL;

	rdp = fastpath->rdp;
	s = transport_send_stream_init(rdp->transport, 256);

	if (!s)
		return NULL;

	Stream_Seek(s, 3); /* fpInputHeader, length1 and length2 */

	if (rdp->do_crypt)
	{
		rdp->sec_flags |= SEC_ENCRYPT;

		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}

	Stream_Seek(s, fastpath_get_sec_bytes(rdp));
	return s;
}

wStream* fastpath_input_pdu_init(rdpFastPath* fastpath, BYTE eventFlags, BYTE eventCode)
{
	wStream* s;
	s = fastpath_input_pdu_init_header(fastpath);

	if (!s)
		return NULL;

	Stream_Write_UINT8(s, eventFlags | (eventCode << 5)); /* eventHeader (1 byte) */
	return s;
}

BOOL fastpath_send_multiple_input_pdu(rdpFastPath* fastpath, wStream* s, size_t iNumEvents)
{
	int state;
	BOOL rc = FALSE;
	rdpRdp* rdp;
	UINT16 length;
	BYTE eventHeader;

	if (!s)
		return FALSE;

	if (!fastpath)
		goto fail;

	rdp = fastpath->rdp;
	state = rdp_client_get_state(rdp);
	if (state != CONNECTION_STATE_ACTIVE)
	{
		WLog_WARN(TAG, "[%s] called before activation [%s]", __FUNCTION__,
		          rdp_client_connection_state_string(state));
		goto fail;
	}

	/*
	 *  A maximum of 15 events are allowed per request
	 *  if the optional numEvents field isn't used
	 *  see MS-RDPBCGR 2.2.8.1.2 for details
	 */
	if (iNumEvents > 15)
		goto fail;

	length = Stream_GetPosition(s);

	if (length >= (2 << 14))
	{
		WLog_ERR(TAG, "Maximum FastPath PDU length is 32767");
		goto fail;
	}

	eventHeader = FASTPATH_INPUT_ACTION_FASTPATH;
	eventHeader |= (iNumEvents << 2); /* numberEvents */

	if (rdp->sec_flags & SEC_ENCRYPT)
		eventHeader |= (FASTPATH_INPUT_ENCRYPTED << 6);

	if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
		eventHeader |= (FASTPATH_INPUT_SECURE_CHECKSUM << 6);

	Stream_SetPosition(s, 0);
	Stream_Write_UINT8(s, eventHeader);
	/* Write length later, RDP encryption might add a padding */
	Stream_Seek(s, 2);

	if (rdp->sec_flags & SEC_ENCRYPT)
	{
		int sec_bytes = fastpath_get_sec_bytes(fastpath->rdp);
		BYTE* fpInputEvents = Stream_Pointer(s) + sec_bytes;
		UINT16 fpInputEvents_length = length - 3 - sec_bytes;

		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
		{
			BYTE pad;

			if ((pad = 8 - (fpInputEvents_length % 8)) == 8)
				pad = 0;

			Stream_Write_UINT16(s, 0x10); /* length */
			Stream_Write_UINT8(s, 0x1);   /* TSFIPS_VERSION 1*/
			Stream_Write_UINT8(s, pad);   /* padding */

			if (!security_hmac_signature(fpInputEvents, fpInputEvents_length, Stream_Pointer(s),
			                             rdp))
				goto fail;

			if (pad)
				memset(fpInputEvents + fpInputEvents_length, 0, pad);

			if (!security_fips_encrypt(fpInputEvents, fpInputEvents_length + pad, rdp))
				goto fail;

			length += pad;
		}
		else
		{
			BOOL status;

			if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
				status = security_salted_mac_signature(rdp, fpInputEvents, fpInputEvents_length,
				                                       TRUE, Stream_Pointer(s));
			else
				status = security_mac_signature(rdp, fpInputEvents, fpInputEvents_length,
				                                Stream_Pointer(s));

			if (!status || !security_encrypt(fpInputEvents, fpInputEvents_length, rdp))
				goto fail;
		}
	}

	rdp->sec_flags = 0;
	/*
	 * We always encode length in two bytes, even though we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	Stream_SetPosition(s, 1);
	Stream_Write_UINT16_BE(s, 0x8000 | length);
	Stream_SetPosition(s, length);
	Stream_SealLength(s);

	if (transport_write(fastpath->rdp->transport, s) < 0)
		goto fail;

	rc = TRUE;
fail:
	Stream_Release(s);
	return rc;
}

BOOL fastpath_send_input_pdu(rdpFastPath* fastpath, wStream* s)
{
	return fastpath_send_multiple_input_pdu(fastpath, s, 1);
}

wStream* fastpath_update_pdu_init(rdpFastPath* fastpath)
{
	return transport_send_stream_init(fastpath->rdp->transport, FASTPATH_MAX_PACKET_SIZE);
}

wStream* fastpath_update_pdu_init_new(rdpFastPath* fastpath)
{
	wStream* s;
	s = Stream_New(NULL, FASTPATH_MAX_PACKET_SIZE);
	return s;
}

BOOL fastpath_send_update_pdu(rdpFastPath* fastpath, BYTE updateCode, wStream* s,
                              BOOL skipCompression)
{
	int fragment;
	UINT16 maxLength;
	UINT32 totalLength;
	BOOL status = TRUE;
	wStream* fs = NULL;
	rdpSettings* settings;
	rdpRdp* rdp;
	UINT32 fpHeaderSize = 6;
	UINT32 fpUpdatePduHeaderSize;
	UINT32 fpUpdateHeaderSize;
	UINT32 CompressionMaxSize;
	FASTPATH_UPDATE_PDU_HEADER fpUpdatePduHeader = { 0 };
	FASTPATH_UPDATE_HEADER fpUpdateHeader = { 0 };

	if (!fastpath || !fastpath->rdp || !fastpath->fs || !s)
		return FALSE;

	rdp = fastpath->rdp;
	fs = fastpath->fs;
	settings = rdp->settings;

	if (!settings)
		return FALSE;

	maxLength = FASTPATH_MAX_PACKET_SIZE - 20;

	if (settings->CompressionEnabled && !skipCompression)
	{
		CompressionMaxSize = bulk_compression_max_size(rdp->bulk);
		maxLength = (maxLength < CompressionMaxSize) ? maxLength : CompressionMaxSize;
		maxLength -= 20;
	}

	totalLength = Stream_GetPosition(s);
	Stream_SetPosition(s, 0);

	/* check if fast path output is possible */
	if (!settings->FastPathOutput)
	{
		WLog_ERR(TAG, "client does not support fast path output");
		return FALSE;
	}

	/* check if the client's fast path pdu buffer is large enough */
	if (totalLength > settings->MultifragMaxRequestSize)
	{
		WLog_ERR(TAG,
		         "fast path update size (%" PRIu32
		         ") exceeds the client's maximum request size (%" PRIu32 ")",
		         totalLength, settings->MultifragMaxRequestSize);
		return FALSE;
	}

	if (rdp->do_crypt)
	{
		rdp->sec_flags |= SEC_ENCRYPT;

		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}

	for (fragment = 0; (totalLength > 0) || (fragment == 0); fragment++)
	{
		BYTE* pSrcData;
		UINT32 SrcSize;
		UINT32 DstSize = 0;
		BYTE* pDstData = NULL;
		UINT32 compressionFlags = 0;
		BYTE pad = 0;
		BYTE* pSignature = NULL;
		fpUpdatePduHeader.action = 0;
		fpUpdatePduHeader.secFlags = 0;
		fpUpdateHeader.compression = 0;
		fpUpdateHeader.compressionFlags = 0;
		fpUpdateHeader.updateCode = updateCode;
		fpUpdateHeader.size = (totalLength > maxLength) ? maxLength : totalLength;
		pSrcData = pDstData = Stream_Pointer(s);
		SrcSize = DstSize = fpUpdateHeader.size;

		if (rdp->sec_flags & SEC_ENCRYPT)
			fpUpdatePduHeader.secFlags |= FASTPATH_OUTPUT_ENCRYPTED;

		if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
			fpUpdatePduHeader.secFlags |= FASTPATH_OUTPUT_SECURE_CHECKSUM;

		if (settings->CompressionEnabled && !skipCompression)
		{
			if (bulk_compress(rdp->bulk, pSrcData, SrcSize, &pDstData, &DstSize,
			                  &compressionFlags) >= 0)
			{
				if (compressionFlags)
				{
					fpUpdateHeader.compressionFlags = compressionFlags;
					fpUpdateHeader.compression = FASTPATH_OUTPUT_COMPRESSION_USED;
				}
			}
		}

		if (!fpUpdateHeader.compression)
		{
			pDstData = Stream_Pointer(s);
			DstSize = fpUpdateHeader.size;
		}

		fpUpdateHeader.size = DstSize;
		totalLength -= SrcSize;

		if (totalLength == 0)
			fpUpdateHeader.fragmentation =
			    (fragment == 0) ? FASTPATH_FRAGMENT_SINGLE : FASTPATH_FRAGMENT_LAST;
		else
			fpUpdateHeader.fragmentation =
			    (fragment == 0) ? FASTPATH_FRAGMENT_FIRST : FASTPATH_FRAGMENT_NEXT;

		fpUpdateHeaderSize = fastpath_get_update_header_size(&fpUpdateHeader);
		fpUpdatePduHeaderSize = fastpath_get_update_pdu_header_size(&fpUpdatePduHeader, rdp);
		fpHeaderSize = fpUpdateHeaderSize + fpUpdatePduHeaderSize;

		if (rdp->sec_flags & SEC_ENCRYPT)
		{
			pSignature = Stream_Buffer(fs) + 3;

			if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			{
				pSignature += 4;

				if ((pad = 8 - ((DstSize + fpUpdateHeaderSize) % 8)) == 8)
					pad = 0;

				fpUpdatePduHeader.fipsInformation[0] = 0x10;
				fpUpdatePduHeader.fipsInformation[1] = 0x00;
				fpUpdatePduHeader.fipsInformation[2] = 0x01;
				fpUpdatePduHeader.fipsInformation[3] = pad;
			}
		}

		fpUpdatePduHeader.length = fpUpdateHeader.size + fpHeaderSize + pad;
		Stream_SetPosition(fs, 0);
		fastpath_write_update_pdu_header(fs, &fpUpdatePduHeader, rdp);
		fastpath_write_update_header(fs, &fpUpdateHeader);
		Stream_Write(fs, pDstData, DstSize);

		if (pad)
			Stream_Zero(fs, pad);

		if (rdp->sec_flags & SEC_ENCRYPT)
		{
			UINT32 dataSize = fpUpdateHeaderSize + DstSize + pad;
			BYTE* data = Stream_Pointer(fs) - dataSize;

			if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			{
				if (!security_hmac_signature(data, dataSize - pad, pSignature, rdp))
					return FALSE;

				security_fips_encrypt(data, dataSize, rdp);
			}
			else
			{
				if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
					status = security_salted_mac_signature(rdp, data, dataSize, TRUE, pSignature);
				else
					status = security_mac_signature(rdp, data, dataSize, pSignature);

				if (!status || !security_encrypt(data, dataSize, rdp))
					return FALSE;
			}
		}

		Stream_SealLength(fs);

		if (transport_write(rdp->transport, fs) < 0)
		{
			status = FALSE;
			break;
		}

		Stream_Seek(s, SrcSize);
	}

	rdp->sec_flags = 0;
	return status;
}

rdpFastPath* fastpath_new(rdpRdp* rdp)
{
	rdpFastPath* fastpath;
	fastpath = (rdpFastPath*)calloc(1, sizeof(rdpFastPath));

	if (!fastpath)
		return NULL;

	fastpath->rdp = rdp;
	fastpath->fragmentation = -1;
	fastpath->fs = Stream_New(NULL, FASTPATH_MAX_PACKET_SIZE);
	fastpath->updateData = Stream_New(NULL, FASTPATH_MAX_PACKET_SIZE);

	if (!fastpath->fs || !fastpath->updateData)
		goto out_free;

	return fastpath;
out_free:
	fastpath_free(fastpath);
	return NULL;
}

void fastpath_free(rdpFastPath* fastpath)
{
	if (fastpath)
	{
		Stream_Free(fastpath->updateData, TRUE);
		Stream_Free(fastpath->fs, TRUE);
		free(fastpath);
	}
}
