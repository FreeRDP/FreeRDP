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
		fastpath->encryptionFlags = (header & 0xC0) >> 6;

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

static int fastpath_recv_update_surfcmd_surface_bits(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	int pos;

	stream_read_uint16(s, cmd->destLeft);
	stream_read_uint16(s, cmd->destTop);
	stream_read_uint16(s, cmd->destRight);
	stream_read_uint16(s, cmd->destBottom);
	stream_read_uint8(s, cmd->bpp);
	stream_seek(s, 2); /* reserved1, reserved2 */
	stream_read_uint8(s, cmd->codecID);
	stream_read_uint16(s, cmd->width);
	stream_read_uint16(s, cmd->height);
	stream_read_uint32(s, cmd->bitmapDataLength);
	pos = stream_get_pos(s) + cmd->bitmapDataLength;
	cmd->bitmapData = stream_get_tail(s);

	IFCALL(update->SurfaceBits, update, cmd);

	stream_set_pos(s, pos);

	return 20 + cmd->bitmapDataLength;
}

static int fastpath_recv_update_surfcmd_frame_marker(rdpFastPath* fastpath, STREAM* s)
{
	uint16 frameAction;
	uint32 frameId;

	stream_read_uint16(s, frameAction);
	stream_read_uint32(s, frameId);
	/*printf("frameAction %d frameId %d\n", frameAction, frameId);*/

	return 6;
}

static void fastpath_recv_update_surfcmds(rdpFastPath* fastpath, uint16 size, STREAM* s)
{
	uint16 cmdType;

	while (size > 2)
	{
		stream_read_uint16(s, cmdType);
		size -= 2;

		switch (cmdType)
		{
			case CMDTYPE_SET_SURFACE_BITS:
			case CMDTYPE_STREAM_SURFACE_BITS:
				size -= fastpath_recv_update_surfcmd_surface_bits(fastpath, s);
				break;

			case CMDTYPE_FRAME_MARKER:
				size -= fastpath_recv_update_surfcmd_frame_marker(fastpath, s);
				break;

			default:
				DEBUG_WARN("unknown cmdType 0x%X", cmdType);
				return;
		}
	}
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
			fastpath_recv_update_surfcmds(fastpath, size, s);
			break;

		case FASTPATH_UPDATETYPE_PTR_NULL:
			printf("FASTPATH_UPDATETYPE_PTR_NULL\n");
			break;

		case FASTPATH_UPDATETYPE_PTR_DEFAULT:
			printf("FASTPATH_UPDATETYPE_PTR_DEFAULT\n");
			break;

		case FASTPATH_UPDATETYPE_PTR_POSITION:
			printf("FASTPATH_UPDATETYPE_PTR_POSITION\n");
			break;

		case FASTPATH_UPDATETYPE_COLOR:
			printf("FASTPATH_UPDATETYPE_COLOR\n");
			break;

		case FASTPATH_UPDATETYPE_CACHED:
			printf("FASTPATH_UPDATETYPE_CACHED\n");
			break;

		case FASTPATH_UPDATETYPE_POINTER:
			printf("FASTPATH_UPDATETYPE_POINTER\n");
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

void fastpath_recv_updates(rdpFastPath* fastpath, STREAM* s)
{
	rdpUpdate* update = fastpath->rdp->update;

	IFCALL(update->BeginPaint, update);

	while (stream_get_left(s) > 3)
	{
		fastpath_recv_update_data(fastpath, s);
	}

	IFCALL(update->EndPaint, update);
}

STREAM* fastpath_pdu_init(rdpFastPath* fastpath)
{
	STREAM* s;
	s = transport_send_stream_init(fastpath->rdp->transport, 127);
	stream_seek(s, 2); /* fpInputHeader and length1 */
	/* length2 is not necessary since input PDU should not exceed 127 bytes */
	return s;
}

void fastpath_send_pdu(rdpFastPath* fastpath, STREAM* s, uint8 numberEvents)
{
	int length;

	length = stream_get_length(s);
	if (length > 127)
	{
		printf("Maximum FastPath PDU length is 127\n");
		return;
	}

	stream_set_pos(s, 0);
	stream_write_uint8(s, (numberEvents << 2));
	stream_write_uint8(s, length);

	stream_set_pos(s, length);
	transport_write(fastpath->rdp->transport, s);
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
