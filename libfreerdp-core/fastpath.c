/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Fast Path
 *
 * Copyright 2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/api.h>
#include <freerdp/utils/stream.h>

#include "orders.h"
#include "per.h"
#include "update.h"
#include "surface.h"

#include "fastpath.h"

/**
 * Fast-Path packet format is defined in [MS-RDPBCGR] 2.2.9.1.2, which revises
 * server output packets from the first byte with the goal of improving
 * bandwidth.
 * 
 * Slow-Path packet always starts with TPKT header, which has the first
 * byte 0x03, while Fast-Path packet starts with 2 zero bits in the first
 * two less significant bits of the first byte.
 */

#define FASTPATH_MAX_PACKET_SIZE 0x3FFF

/*
 * The fastpath header may be two or three bytes long.
 * This function assumes that at least two bytes are available in the stream
 * and doesn't touch third byte.
 */
uint16 fastpath_header_length(STREAM* s)
{
	uint8 length1;

	stream_seek_uint8(s);
	stream_read_uint8(s, length1);
	stream_rewind(s, 2);

	return ((length1 & 0x80) != 0 ? 3 : 2);
}

/**
 * Read a Fast-Path packet header.\n
 * @param s stream
 * @param encryptionFlags
 * @return length
 */
uint16 fastpath_read_header(rdpFastPath* fastpath, STREAM* s)
{
	uint8 header;
	uint16 length;

	stream_read_uint8(s, header);

	if (fastpath != NULL)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	per_read_length(s, &length);

	return length;
}

INLINE void fastpath_read_update_header(STREAM* s, uint8* updateCode, uint8* fragmentation, uint8* compression)
{
	uint8 updateHeader;

	stream_read_uint8(s, updateHeader);
	*updateCode = updateHeader & 0x0F;
	*fragmentation = (updateHeader >> 4) & 0x03;
	*compression = (updateHeader >> 6) & 0x03;
}

INLINE void fastpath_write_update_header(STREAM* s, uint8 updateCode, uint8 fragmentation, uint8 compression)
{
	uint8 updateHeader = 0;

	updateHeader |= updateCode & 0x0F;
	updateHeader |= (fragmentation & 0x03) << 4;
	updateHeader |= (compression & 0x03) << 6;
	stream_write_uint8(s, updateHeader);
}

uint16 fastpath_read_header_rdp(rdpFastPath* fastpath, STREAM* s)
{
	uint8 header;
	uint16 length;

	stream_read_uint8(s, header);

	if (fastpath != NULL)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	per_read_length(s, &length);

	return length - stream_get_length(s);
}

static void fastpath_recv_orders(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	uint16 numberOrders;

	stream_read_uint16(s, numberOrders); /* numberOrders (2 bytes) */

	while (numberOrders > 0)
	{
		update_recv_order(update, s);
		numberOrders--;
	}
}

static void fastpath_recv_update_common(rdpFastPath* fastpath, STREAM* s)
{
	uint16 updateType;
	rdpUpdate* update = fastpath->rdp->update;
	rdpContext* context = update->context;

	stream_read_uint16(s, updateType); /* updateType (2 bytes) */

	switch (updateType)
	{
		case UPDATE_TYPE_BITMAP:
			update_read_bitmap(update, s, &update->bitmap_update);
			IFCALL(update->BitmapUpdate, context, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			update_read_palette(update, s, &update->palette_update);
			IFCALL(update->Palette, context, &update->palette_update);
			break;
	}
}

static void fastpath_recv_update_synchronize(rdpFastPath* fastpath, STREAM* s)
{
	stream_seek_uint16(s); /* size (2 bytes), must be set to zero */
}

static void fastpath_recv_update(rdpFastPath* fastpath, uint8 updateCode, uint32 size, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	rdpContext* context = fastpath->rdp->update->context;
	rdpPointerUpdate* pointer = update->pointer;

	switch (updateCode)
	{
		case FASTPATH_UPDATETYPE_ORDERS:
			fastpath_recv_orders(fastpath, s);
			break;

		case FASTPATH_UPDATETYPE_BITMAP:
		case FASTPATH_UPDATETYPE_PALETTE:
			fastpath_recv_update_common(fastpath, s);
			break;

		case FASTPATH_UPDATETYPE_SYNCHRONIZE:
			fastpath_recv_update_synchronize(fastpath, s);
			IFCALL(update->Synchronize, context);
			break;

		case FASTPATH_UPDATETYPE_SURFCMDS:
			update_recv_surfcmds(update, size, s);
			break;

		case FASTPATH_UPDATETYPE_PTR_NULL:
			pointer->pointer_system.type = SYSPTR_NULL;
			IFCALL(pointer->PointerSystem, context, &pointer->pointer_system);
			break;

		case FASTPATH_UPDATETYPE_PTR_DEFAULT:
			update->pointer->pointer_system.type = SYSPTR_DEFAULT;
			IFCALL(pointer->PointerSystem, context, &pointer->pointer_system);
			break;

		case FASTPATH_UPDATETYPE_PTR_POSITION:
			update_read_pointer_position(s, &pointer->pointer_position);
			IFCALL(pointer->PointerPosition, context, &pointer->pointer_position);
			break;

		case FASTPATH_UPDATETYPE_COLOR:
			update_read_pointer_color(s, &pointer->pointer_color);
			IFCALL(pointer->PointerColor, context, &pointer->pointer_color);
			break;

		case FASTPATH_UPDATETYPE_CACHED:
			update_read_pointer_cached(s, &pointer->pointer_cached);
			IFCALL(pointer->PointerCached, context, &pointer->pointer_cached);
			break;

		case FASTPATH_UPDATETYPE_POINTER:
			update_read_pointer_new(s, &pointer->pointer_new);
			IFCALL(pointer->PointerNew, context, &pointer->pointer_new);
			break;

		default:
			DEBUG_WARN("unknown updateCode 0x%X", updateCode);
			break;
	}
}

static void fastpath_recv_update_data(rdpFastPath* fastpath, STREAM* s)
{
	uint16 size;
	int next_pos;
	uint32 totalSize;
	uint8 updateCode;
	uint8 fragmentation;
	uint8 compression;
	uint8 compressionFlags;
	STREAM* update_stream;
	STREAM* comp_stream;
	rdpRdp  *rdp;
	uint32 roff;
	uint32 rlen;

	rdp = fastpath->rdp;

	fastpath_read_update_header(s, &updateCode, &fragmentation, &compression);

	if (compression == FASTPATH_OUTPUT_COMPRESSION_USED)
		stream_read_uint8(s, compressionFlags);
	else
		compressionFlags = 0;

	stream_read_uint16(s, size);
	next_pos = stream_get_pos(s) + size;
	comp_stream = s;

	if (compressionFlags & PACKET_COMPRESSED)
	{
		if (decompress_rdp(rdp, s->p, size, compressionFlags, &roff, &rlen))
		{
			comp_stream = stream_new(0);
			comp_stream->data = rdp->mppc->history_buf + roff;
			comp_stream->p = comp_stream->data;
			comp_stream->size = rlen;
			size = comp_stream->size;
		}
		else
		{
			printf("decompress_rdp() failed\n");
			stream_seek(s, size);
		}
	}

	update_stream = NULL;
	if (fragmentation == FASTPATH_FRAGMENT_SINGLE)
	{
		totalSize = size;
		update_stream = comp_stream;
	}
	else
	{
		if (fragmentation == FASTPATH_FRAGMENT_FIRST)
			stream_set_pos(fastpath->updateData, 0);

		stream_check_size(fastpath->updateData, size);
		stream_copy(fastpath->updateData, comp_stream, size);

		if (fragmentation == FASTPATH_FRAGMENT_LAST)
		{
			update_stream = fastpath->updateData;
			totalSize = stream_get_length(update_stream);
			stream_set_pos(update_stream, 0);
		}
	}

	if (update_stream)
		fastpath_recv_update(fastpath, updateCode, totalSize, update_stream);

	stream_set_pos(s, next_pos);

	if (comp_stream != s)
		xfree(comp_stream);
}

boolean fastpath_recv_updates(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;

	IFCALL(update->BeginPaint, update->context);

	while (stream_get_left(s) >= 3)
	{
		fastpath_recv_update_data(fastpath, s);
	}

	IFCALL(update->EndPaint, update->context);

	return true;
}

static boolean fastpath_read_input_event_header(STREAM* s, uint8* eventFlags, uint8* eventCode)
{
	uint8 eventHeader;

	if (stream_get_left(s) < 1)
		return false;

	stream_read_uint8(s, eventHeader); /* eventHeader (1 byte) */

	*eventFlags = (eventHeader & 0x1F);
	*eventCode = (eventHeader >> 5);

	return true;
}

static boolean fastpath_recv_input_event_scancode(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 flags;
	uint16 code;

	if (stream_get_left(s) < 1)
		return false;

	stream_read_uint8(s, code); /* keyCode (1 byte) */

	flags = 0;
	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED))
		flags |= KBD_FLAGS_EXTENDED;

	IFCALL(fastpath->rdp->input->KeyboardEvent, fastpath->rdp->input, flags, code);

	return true;
}

static boolean fastpath_recv_input_event_mouse(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 pointerFlags;
	uint16 xPos;
	uint16 yPos;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->MouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return true;
}

static boolean fastpath_recv_input_event_mousex(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 pointerFlags;
	uint16 xPos;
	uint16 yPos;

	if (stream_get_left(s) < 6)
		return false;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->ExtendedMouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return true;
}

static boolean fastpath_recv_input_event_sync(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	IFCALL(fastpath->rdp->input->SynchronizeEvent, fastpath->rdp->input, eventFlags);

	return true;
}

static boolean fastpath_recv_input_event_unicode(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 unicodeCode;
	uint16 flags;

	if (stream_get_left(s) < 2)
		return false;

	stream_read_uint16(s, unicodeCode); /* unicodeCode (2 bytes) */

	flags = 0;
	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	IFCALL(fastpath->rdp->input->UnicodeKeyboardEvent, fastpath->rdp->input, flags, unicodeCode);

	return true;
}

static boolean fastpath_recv_input_event(rdpFastPath* fastpath, STREAM* s)
{
	uint8 eventFlags;
	uint8 eventCode;

	if (!fastpath_read_input_event_header(s, &eventFlags, &eventCode))
		return false;

	switch (eventCode)
	{
		case FASTPATH_INPUT_EVENT_SCANCODE:
			if (!fastpath_recv_input_event_scancode(fastpath, s, eventFlags))
				return false;
			break;

		case FASTPATH_INPUT_EVENT_MOUSE:
			if (!fastpath_recv_input_event_mouse(fastpath, s, eventFlags))
				return false;
			break;

		case FASTPATH_INPUT_EVENT_MOUSEX:
			if (!fastpath_recv_input_event_mousex(fastpath, s, eventFlags))
				return false;
			break;

		case FASTPATH_INPUT_EVENT_SYNC:
			if (!fastpath_recv_input_event_sync(fastpath, s, eventFlags))
				return false;
			break;

		case FASTPATH_INPUT_EVENT_UNICODE:
			if (!fastpath_recv_input_event_unicode(fastpath, s, eventFlags))
				return false;
			break;

		default:
			printf("Unknown eventCode %d\n", eventCode);
			break;
	}

	return true;
}

boolean fastpath_recv_inputs(rdpFastPath* fastpath, STREAM* s)
{
	uint8 i;

	if (fastpath->numberEvents == 0)
	{
		/**
		 * If numberEvents is not provided in fpInputHeader, it will be provided
		 * as one additional byte here.
		 */

		if (stream_get_left(s) < 1)
			return false;

		stream_read_uint8(s, fastpath->numberEvents); /* eventHeader (1 byte) */
	}

	for (i = 0; i < fastpath->numberEvents; i++)
	{
		if (!fastpath_recv_input_event(fastpath, s))
			return false;
	}

	return true;
}

static uint32 fastpath_get_sec_bytes(rdpRdp* rdp)
{
	uint32 sec_bytes;

	if (rdp->do_crypt)
	{
		sec_bytes = 8;
		if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
			sec_bytes += 4;
	}
	else
		sec_bytes = 0;
	return sec_bytes;
}

STREAM* fastpath_input_pdu_init(rdpFastPath* fastpath, uint8 eventFlags, uint8 eventCode)
{
	rdpRdp *rdp;
	STREAM* s;

	rdp = fastpath->rdp;

	s = transport_send_stream_init(rdp->transport, 256);
	stream_seek(s, 3); /* fpInputHeader, length1 and length2 */
	if (rdp->do_crypt) {
		rdp->sec_flags |= SEC_ENCRYPT;
		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}
	stream_seek(s, fastpath_get_sec_bytes(rdp));
	stream_write_uint8(s, eventFlags | (eventCode << 5)); /* eventHeader (1 byte) */
	return s;
}

boolean fastpath_send_input_pdu(rdpFastPath* fastpath, STREAM* s)
{
	rdpRdp *rdp;
	uint16 length;
	uint8 eventHeader;
	int sec_bytes;

	rdp = fastpath->rdp;

	length = stream_get_length(s);
	if (length > 127)
	{
		printf("Maximum FastPath PDU length is 127\n");
		return false;
	}

	eventHeader = FASTPATH_INPUT_ACTION_FASTPATH;
	eventHeader |= (1 << 2); /* numberEvents */
	if (rdp->sec_flags & SEC_ENCRYPT)
		eventHeader |= (FASTPATH_INPUT_ENCRYPTED << 6);
	if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
		eventHeader |= (FASTPATH_INPUT_SECURE_CHECKSUM << 6);

	stream_set_pos(s, 0);
	stream_write_uint8(s, eventHeader);
	sec_bytes = fastpath_get_sec_bytes(fastpath->rdp);
	/*
	 * We always encode length in two bytes, eventhough we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	stream_write_uint16_be(s, 0x8000 | (length + sec_bytes));

	if (sec_bytes > 0)
	{
		uint8* ptr;

		ptr = stream_get_tail(s) + sec_bytes;
		if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
			security_salted_mac_signature(rdp, ptr, length - 3, true, stream_get_tail(s));
		else
			security_mac_signature(rdp, ptr, length - 3, stream_get_tail(s));
		security_encrypt(ptr, length - 3, rdp);
	}

	rdp->sec_flags = 0;

	stream_set_pos(s, length + sec_bytes);
	if (transport_write(fastpath->rdp->transport, s) < 0)
		return false;

	return true;
}

STREAM* fastpath_update_pdu_init(rdpFastPath* fastpath)
{
	STREAM* s;
	s = transport_send_stream_init(fastpath->rdp->transport, FASTPATH_MAX_PACKET_SIZE);
	stream_seek(s, 3); /* fpOutputHeader, length1 and length2 */
	stream_seek(s, fastpath_get_sec_bytes(fastpath->rdp));
	stream_seek(s, 3); /* updateHeader, size */
	return s;
}

boolean fastpath_send_update_pdu(rdpFastPath* fastpath, uint8 updateCode, STREAM* s)
{
	rdpRdp *rdp;
	uint8* bm;
	uint8* ptr;
	int fragment;
	int sec_bytes;
	uint16 length;
	boolean result;
	uint16 pduLength;
	uint16 maxLength;
	uint32 totalLength;
	uint8 fragmentation;
	uint8 header;
	STREAM* update;

	result = true;

	rdp = fastpath->rdp;
	sec_bytes = fastpath_get_sec_bytes(rdp);
	maxLength = FASTPATH_MAX_PACKET_SIZE - 6 - sec_bytes;
	totalLength = stream_get_length(s) - 6 - sec_bytes;
	stream_set_pos(s, 0);
	update = stream_new(0);

	for (fragment = 0; totalLength > 0; fragment++)
	{
		length = MIN(maxLength, totalLength);
		totalLength -= length;
		pduLength = length + 6 + sec_bytes;

		if (totalLength == 0)
			fragmentation = (fragment == 0) ? FASTPATH_FRAGMENT_SINGLE : FASTPATH_FRAGMENT_LAST;
		else
			fragmentation = (fragment == 0) ? FASTPATH_FRAGMENT_FIRST : FASTPATH_FRAGMENT_NEXT;

		stream_get_mark(s, bm);
		header = 0;
		if (sec_bytes > 0)
			header |= (FASTPATH_OUTPUT_ENCRYPTED << 6);
		stream_write_uint8(s, header); /* fpOutputHeader (1 byte) */
		stream_write_uint8(s, 0x80 | (pduLength >> 8)); /* length1 */
		stream_write_uint8(s, pduLength & 0xFF); /* length2 */
		if (sec_bytes > 0)
			stream_seek(s, sec_bytes);
		fastpath_write_update_header(s, updateCode, fragmentation, 0);
		stream_write_uint16(s, length);

		stream_attach(update, bm, pduLength);
		stream_seek(update, pduLength);
		if (sec_bytes > 0)
		{
			ptr = bm + 3 + sec_bytes;
			if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
				security_salted_mac_signature(rdp, ptr, length + 3, true, bm + 3);
			else
				security_mac_signature(rdp, ptr, length + 3, bm + 3);
			security_encrypt(ptr, length + 3, rdp);
		}
		if (transport_write(fastpath->rdp->transport, update) < 0)
		{
			stream_detach(update);
			result = false;
			break;
		}
		stream_detach(update);

		/* Reserve 6+sec_bytes bytes for the next fragment header, if any. */
		stream_seek(s, length - 6 - sec_bytes);
	}

	stream_free(update);

	return result;
}

rdpFastPath* fastpath_new(rdpRdp* rdp)
{
	rdpFastPath* fastpath;

	fastpath = xnew(rdpFastPath);
	fastpath->rdp = rdp;
	fastpath->updateData = stream_new(4096);

	return fastpath;
}

void fastpath_free(rdpFastPath* fastpath)
{
	stream_free(fastpath->updateData);
	xfree(fastpath);
}
