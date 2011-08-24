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
#include <freerdp/utils/stream.h>

#include "orders.h"
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

#define FASTPATH_MAX_PACKET_SIZE 0x7FFF

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
	uint8 t;

	stream_read_uint8(s, header);
	if (fastpath != NULL)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	stream_read_uint8(s, length); /* length1 */
	/* If most significant bit is not set, length2 is not presented. */
	if ((length & 0x80))
	{
		length &= 0x7F;
		length <<= 8;
		stream_read_uint8(s, t);
		length += t;
	}

	return length;
}

boolean fastpath_read_security_header(rdpFastPath* fastpath, STREAM* s)
{
	/* TODO: fipsInformation */

	if ((fastpath->encryptionFlags & FASTPATH_OUTPUT_ENCRYPTED))
	{
		stream_seek(s, 8); /* dataSignature */
	}

	return True;
}

static void fastpath_recv_orders(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	uint16 numberOrders;

	stream_read_uint16(s, numberOrders); /* numberOrders (2 bytes) */

	printf("numberOrders(FastPath):%d\n", numberOrders);

	while (numberOrders > 0)
	{
		update_recv_order(update, s);
		numberOrders--;
	}
}

static void fastpath_recv_update_common(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	uint16 updateType;

	stream_read_uint16(s, updateType); /* updateType (2 bytes) */

	switch (updateType)
	{
		case UPDATE_TYPE_BITMAP:
			update_read_bitmap(update, s, &update->bitmap_update);
			IFCALL(update->Bitmap, update, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			update_read_palette(update, s, &update->palette_update);
			IFCALL(update->Palette, update, &update->palette_update);
			break;
	}
}

static void fastpath_recv_update(rdpFastPath* fastpath, uint8 updateCode, uint16 size, STREAM* s)
{
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
			IFCALL(fastpath->rdp->update->Synchronize, fastpath->rdp->update);
			break;

		case FASTPATH_UPDATETYPE_SURFCMDS:
			update_recv_surfcmds(fastpath->rdp->update, size, s);
			break;

		case FASTPATH_UPDATETYPE_PTR_NULL:
			//printf("FASTPATH_UPDATETYPE_PTR_NULL\n");
			break;

		case FASTPATH_UPDATETYPE_PTR_DEFAULT:
			//printf("FASTPATH_UPDATETYPE_PTR_DEFAULT\n");
			break;

		case FASTPATH_UPDATETYPE_PTR_POSITION:
			//printf("FASTPATH_UPDATETYPE_PTR_POSITION\n");
			break;

		case FASTPATH_UPDATETYPE_COLOR:
			//printf("FASTPATH_UPDATETYPE_COLOR\n");
			break;

		case FASTPATH_UPDATETYPE_CACHED:
			//printf("FASTPATH_UPDATETYPE_CACHED\n");
			break;

		case FASTPATH_UPDATETYPE_POINTER:
			//printf("FASTPATH_UPDATETYPE_POINTER\n");
			break;

		default:
			DEBUG_WARN("unknown updateCode 0x%X", updateCode);
			break;
	}
}

static void fastpath_recv_update_data(rdpFastPath* fastpath, STREAM* s)
{
	uint8 updateHeader;
	uint8 updateCode;
	uint8 fragmentation;
	uint8 compression;
	uint8 compressionFlags;
	uint16 size;
	STREAM* update_stream;
	int next_pos;

	stream_read_uint8(s, updateHeader);
	updateCode = updateHeader & 0x0F;
	fragmentation = (updateHeader >> 4) & 0x03;
	compression = (updateHeader >> 6) & 0x03;

	if (compression == FASTPATH_OUTPUT_COMPRESSION_USED)
		stream_read_uint8(s, compressionFlags);
	else
		compressionFlags = 0;

	stream_read_uint16(s, size);
	next_pos = stream_get_pos(s) + size;

	if (compressionFlags != 0)
	{
		printf("FastPath compression is not yet implemented!\n");
		stream_seek(s, size);
		return;
	}

	update_stream = NULL;
	if (fragmentation == FASTPATH_FRAGMENT_SINGLE)
	{
		update_stream = s;
	}
	else
	{
		if (fragmentation == FASTPATH_FRAGMENT_FIRST)
			stream_set_pos(fastpath->updateData, 0);

		stream_check_size(fastpath->updateData, size);
		stream_copy(fastpath->updateData, s, size);

		if (fragmentation == FASTPATH_FRAGMENT_LAST)
		{
			update_stream = fastpath->updateData;
			size = stream_get_length(update_stream);
			stream_set_pos(update_stream, 0);
		}
	}

	if (update_stream)
		fastpath_recv_update(fastpath, updateCode, size, update_stream);

	stream_set_pos(s, next_pos);
}

boolean fastpath_recv_updates(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;

	IFCALL(update->BeginPaint, update);

	while (stream_get_left(s) > 3)
	{
		fastpath_recv_update_data(fastpath, s);
	}

	IFCALL(update->EndPaint, update);

	return True;
}

static boolean fastpath_read_input_event_header(STREAM* s, uint8* eventFlags, uint8* eventCode)
{
	uint8 eventHeader;

	if (stream_get_left(s) < 1)
		return False;

	stream_read_uint8(s, eventHeader); /* eventHeader (1 byte) */

	*eventFlags = (eventHeader & 0x1F);
	*eventCode = (eventHeader >> 5);

	return True;
}

static boolean fastpath_recv_input_event_scancode(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 flags;
	uint16 code;

	if (stream_get_left(s) < 1)
		return False;

	stream_read_uint8(s, code); /* keyCode (1 byte) */

	flags = 0;
	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED))
		flags |= KBD_FLAGS_EXTENDED;

	IFCALL(fastpath->rdp->input->KeyboardEvent, fastpath->rdp->input, flags, code);

	return True;
}

static boolean fastpath_recv_input_event_mouse(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 pointerFlags;
	uint16 xPos;
	uint16 yPos;

	if (stream_get_left(s) < 6)
		return False;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->MouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return True;
}

static boolean fastpath_recv_input_event_mousex(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 pointerFlags;
	uint16 xPos;
	uint16 yPos;

	if (stream_get_left(s) < 6)
		return False;

	stream_read_uint16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_uint16(s, xPos); /* xPos (2 bytes) */
	stream_read_uint16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->ExtendedMouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return True;
}

static boolean fastpath_recv_input_event_sync(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	IFCALL(fastpath->rdp->input->SynchronizeEvent, fastpath->rdp->input, eventFlags);

	return True;
}

static boolean fastpath_recv_input_event_unicode(rdpFastPath* fastpath, STREAM* s, uint8 eventFlags)
{
	uint16 unicodeCode;

	if (stream_get_left(s) < 2)
		return False;

	stream_read_uint16(s, unicodeCode); /* unicodeCode (2 bytes) */

	IFCALL(fastpath->rdp->input->UnicodeKeyboardEvent, fastpath->rdp->input, unicodeCode);

	return True;
}

static boolean fastpath_recv_input_event(rdpFastPath* fastpath, STREAM* s)
{
	uint8 eventFlags;
	uint8 eventCode;

	if (!fastpath_read_input_event_header(s, &eventFlags, &eventCode))
		return False;

	switch (eventCode)
	{
		case FASTPATH_INPUT_EVENT_SCANCODE:
			if (!fastpath_recv_input_event_scancode(fastpath, s, eventFlags))
				return False;
			break;

		case FASTPATH_INPUT_EVENT_MOUSE:
			if (!fastpath_recv_input_event_mouse(fastpath, s, eventFlags))
				return False;
			break;

		case FASTPATH_INPUT_EVENT_MOUSEX:
			if (!fastpath_recv_input_event_mousex(fastpath, s, eventFlags))
				return False;
			break;

		case FASTPATH_INPUT_EVENT_SYNC:
			if (!fastpath_recv_input_event_sync(fastpath, s, eventFlags))
				return False;
			break;

		case FASTPATH_INPUT_EVENT_UNICODE:
			if (!fastpath_recv_input_event_unicode(fastpath, s, eventFlags))
				return False;
			break;

		default:
			printf("Unknown eventCode %d\n", eventCode);
			break;
	}

	return True;
}

boolean fastpath_recv_inputs(rdpFastPath* fastpath, STREAM* s)
{
	uint8 i;

	if (fastpath->numberEvents == 0)
	{
		/**
		 * If numberEvents is not provided in fpInputHeader, it will be provided
		 * as onee additional byte here.
		 */

		if (stream_get_left(s) < 1)
			return False;

		stream_read_uint8(s, fastpath->numberEvents); /* eventHeader (1 byte) */
	}

	for (i = 0; i < fastpath->numberEvents; i++)
	{
		if (!fastpath_recv_input_event(fastpath, s))
			return False;
	}

	return True;
}

STREAM* fastpath_input_pdu_init(rdpFastPath* fastpath, uint8 eventFlags, uint8 eventCode)
{
	STREAM* s;
	s = transport_send_stream_init(fastpath->rdp->transport, 127);
	stream_seek(s, 2); /* fpInputHeader and length1 */
	/* length2 is not necessary since input PDU should not exceed 127 bytes */
	stream_write_uint8(s, eventFlags | (eventCode << 5)); /* eventHeader (1 byte) */
	return s;
}

boolean fastpath_send_input_pdu(rdpFastPath* fastpath, STREAM* s)
{
	uint16 length;

	length = stream_get_length(s);
	if (length > 127)
	{
		printf("Maximum FastPath PDU length is 127\n");
		return False;
	}

	stream_set_pos(s, 0);
	stream_write_uint8(s, (1 << 2));
	stream_write_uint8(s, length);

	stream_set_pos(s, length);
	if (transport_write(fastpath->rdp->transport, s) < 0)
		return False;

	return True;
}

STREAM* fastpath_update_pdu_init(rdpFastPath* fastpath)
{
	STREAM* s;
	s = transport_send_stream_init(fastpath->rdp->transport, FASTPATH_MAX_PACKET_SIZE);
	stream_seek(s, 3); /* fpOutputHeader, length1 and length2 */
	return s;
}

boolean fastpath_send_update_pdu(rdpFastPath* fastpath, STREAM* s)
{
	uint16 length;

	length = stream_get_length(s);
	if (length > FASTPATH_MAX_PACKET_SIZE)
	{
		printf("Maximum FastPath Update PDU length is %d\n", FASTPATH_MAX_PACKET_SIZE);
		return False;
	}

	stream_set_pos(s, 0);
	stream_write_uint8(s, 0); /* fpOutputHeader (1 byte) */
	stream_write_uint8(s, 0x80 | (length >> 8)); /* length1 */
	stream_write_uint8(s, length & 0xFF); /* length2 */

	stream_set_pos(s, length);
	if (transport_write(fastpath->rdp->transport, s) < 0)
		return False;

	return True;
}

boolean fastpath_send_surfcmd_frame_marker(rdpFastPath* fastpath, uint16 frameAction, uint32 frameId)
{
	STREAM* s;
	s = transport_send_stream_init(fastpath->rdp->transport, 127);
	stream_write_uint8(s, 0); /* fpOutputHeader (1 byte) */
	stream_write_uint8(s, 5 + SURFCMD_FRAME_MARKER_LENGTH); /* length1 */
	stream_write_uint8(s, FASTPATH_UPDATETYPE_SURFCMDS); /* updateHeader (1 byte) */
	stream_write_uint16(s, SURFCMD_FRAME_MARKER_LENGTH); /* size (2 bytes) */
	update_write_surfcmd_frame_marker(s, frameAction, frameId);

	if (transport_write(fastpath->rdp->transport, s) < 0)
		return False;

	return True;
}

boolean fastpath_send_surfcmd_surface_bits(rdpFastPath* fastpath, SURFACE_BITS_COMMAND* cmd)
{
	STREAM* s;
	uint16 size;
	uint8* bitmapData;
	uint32 bitmapDataLength;
	uint16 fragment_size;
	uint8 fragmentation;
	int i;
	int bp, ep;

	bitmapData = cmd->bitmapData;
	bitmapDataLength = cmd->bitmapDataLength;
	for (i = 0; bitmapDataLength > 0; i++)
	{
		s = fastpath_update_pdu_init(fastpath);

		bp = stream_get_pos(s);
		stream_seek_uint8(s); /* updateHeader (1 byte) */
		stream_seek_uint16(s); /* size (2 bytes) */
		size = 0;

		if (i == 0)
		{
			update_write_surfcmd_surface_bits_header(s, cmd);
			size += SURFCMD_SURFACE_BITS_HEADER_LENGTH;
		}

		fragment_size = MIN(stream_get_left(s), bitmapDataLength);
		if (fragment_size == bitmapDataLength)
		{
			fragmentation = (i == 0 ? FASTPATH_FRAGMENT_SINGLE : FASTPATH_FRAGMENT_LAST);
		}
		else
		{
			fragmentation = (i == 0 ? FASTPATH_FRAGMENT_FIRST : FASTPATH_FRAGMENT_NEXT);
		}
		size += fragment_size;

		ep = stream_get_pos(s);
		stream_set_pos(s, bp);
		stream_write_uint8(s, FASTPATH_UPDATETYPE_SURFCMDS | (fragmentation << 4));
		stream_write_uint16(s, size);
		stream_set_pos(s, ep);

		stream_write(s, bitmapData, fragment_size);
		bitmapData += fragment_size;
		bitmapDataLength -= fragment_size;

		if (!fastpath_send_update_pdu(fastpath, s))
			return False;
	}

	return True;
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
