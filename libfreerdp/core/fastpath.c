/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/api.h>
#include <freerdp/crypto/per.h>
#include <winpr/stream.h>

#include "orders.h"
#include "update.h"
#include "surface.h"
#include "fastpath.h"
#include "rdp.h"

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

#ifdef WITH_DEBUG_RDP
static const char* const FASTPATH_UPDATETYPE_STRINGS[] =
{
	"Orders",									/* 0x0 */
	"Bitmap",									/* 0x1 */
	"Palette",								/* 0x2 */
	"Synchronize",						/* 0x3 */
	"Surface Commands",				/* 0x4 */
	"System Pointer Hidden",	/* 0x5 */
	"System Pointer Default",	/* 0x6 */
	"???",										/* 0x7 */
	"Pointer Position",				/* 0x8 */
	"Color Pointer",					/* 0x9 */
	"Cached Pointer",					/* 0xA */
	"New Pointer",						/* 0xB */
};
#endif

/*
 * The fastpath header may be two or three bytes long.
 * This function assumes that at least two bytes are available in the stream
 * and doesn't touch third byte.
 */
UINT16 fastpath_header_length(wStream* s)
{
	BYTE length1;

	stream_seek_BYTE(s);
	stream_read_BYTE(s, length1);
	stream_rewind(s, 2);

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

	stream_read_BYTE(s, header);

	if (fastpath != NULL)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	per_read_length(s, &length);

	return length;
}

static INLINE void fastpath_read_update_header(wStream* s, BYTE* updateCode, BYTE* fragmentation, BYTE* compression)
{
	BYTE updateHeader;

	stream_read_BYTE(s, updateHeader);
	*updateCode = updateHeader & 0x0F;
	*fragmentation = (updateHeader >> 4) & 0x03;
	*compression = (updateHeader >> 6) & 0x03;
}

static INLINE void fastpath_write_update_header(wStream* s, BYTE updateCode, BYTE fragmentation, BYTE compression)
{
	BYTE updateHeader = 0;

	updateHeader |= updateCode & 0x0F;
	updateHeader |= (fragmentation & 0x03) << 4;
	updateHeader |= (compression & 0x03) << 6;
	stream_write_BYTE(s, updateHeader);
}

BOOL fastpath_read_header_rdp(rdpFastPath* fastpath, wStream* s, UINT16 *length)
{
	BYTE header;

	stream_read_BYTE(s, header);

	if (fastpath != NULL)
	{
		fastpath->encryptionFlags = (header & 0xC0) >> 6;
		fastpath->numberEvents = (header & 0x3C) >> 2;
	}

	if (!per_read_length(s, length))
		return FALSE;

	*length = *length - stream_get_length(s);
	return TRUE;
}

static BOOL fastpath_recv_orders(rdpFastPath* fastpath, wStream* s)
{
	rdpUpdate* update = fastpath->rdp->update;
	UINT16 numberOrders;

	stream_read_UINT16(s, numberOrders); /* numberOrders (2 bytes) */

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
	UINT16 updateType;
	rdpUpdate* update = fastpath->rdp->update;
	rdpContext* context = update->context;

	if (stream_get_left(s) < 2)
		return FALSE;

	stream_read_UINT16(s, updateType); /* updateType (2 bytes) */

	switch (updateType)
	{
		case UPDATE_TYPE_BITMAP:
			if (!update_read_bitmap(update, s, &update->bitmap_update))
				return FALSE;
			IFCALL(update->BitmapUpdate, context, &update->bitmap_update);
			break;

		case UPDATE_TYPE_PALETTE:
			if (!update_read_palette(update, s, &update->palette_update))
				return FALSE;
			IFCALL(update->Palette, context, &update->palette_update);
			break;
	}
	return TRUE;
}

static BOOL fastpath_recv_update_synchronize(rdpFastPath* fastpath, wStream* s)
{
	/* server 2008 can send invalid synchronize packet with missing padding,
	  so don't return FALSE even if the packet is invalid */
	stream_skip(s, 2); /* size (2 bytes), MUST be set to zero */
	return TRUE;
}

static int fastpath_recv_update(rdpFastPath* fastpath, BYTE updateCode, UINT32 size, wStream* s)
{
	int status = 0;
	rdpUpdate* update = fastpath->rdp->update;
	rdpContext* context = fastpath->rdp->update->context;
	rdpPointerUpdate* pointer = update->pointer;

#ifdef WITH_DEBUG_RDP
	DEBUG_RDP("recv Fast-Path %s Update (0x%X), length:%d",
		updateCode < ARRAYSIZE(FASTPATH_UPDATETYPE_STRINGS) ? FASTPATH_UPDATETYPE_STRINGS[updateCode] : "???", updateCode, capacity);
#endif

	switch (updateCode)
	{
		case FASTPATH_UPDATETYPE_ORDERS:
			if (!fastpath_recv_orders(fastpath, s))
				return -1;
			break;

		case FASTPATH_UPDATETYPE_BITMAP:
		case FASTPATH_UPDATETYPE_PALETTE:
			if (!fastpath_recv_update_common(fastpath, s))
				return -1;
			break;

		case FASTPATH_UPDATETYPE_SYNCHRONIZE:
			if (!fastpath_recv_update_synchronize(fastpath, s))
				fprintf(stderr, "fastpath_recv_update_synchronize failure but we continue\n");
			else
				IFCALL(update->Synchronize, context);			
			break;

		case FASTPATH_UPDATETYPE_SURFCMDS:
			status = update_recv_surfcmds(update, size, s);
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
			if (!update_read_pointer_position(s, &pointer->pointer_position))
				return -1;
			IFCALL(pointer->PointerPosition, context, &pointer->pointer_position);
			break;

		case FASTPATH_UPDATETYPE_COLOR:
			if (!update_read_pointer_color(s, &pointer->pointer_color))
				return -1;
			IFCALL(pointer->PointerColor, context, &pointer->pointer_color);
			break;

		case FASTPATH_UPDATETYPE_CACHED:
			if (!update_read_pointer_cached(s, &pointer->pointer_cached))
				return -1;
			IFCALL(pointer->PointerCached, context, &pointer->pointer_cached);
			break;

		case FASTPATH_UPDATETYPE_POINTER:
			if (!update_read_pointer_new(s, &pointer->pointer_new))
				return -1;
			IFCALL(pointer->PointerNew, context, &pointer->pointer_new);
			break;

		default:
			DEBUG_WARN("unknown updateCode 0x%X", updateCode);
			break;
	}

	return status;
}

static int fastpath_recv_update_data(rdpFastPath* fastpath, wStream* s)
{
	int status;
	UINT16 size;
	int next_pos;
	UINT32 totalSize;
	BYTE updateCode;
	BYTE fragmentation;
	BYTE compression;
	BYTE compressionFlags;
	wStream* update_stream;
	wStream* comp_stream;
	rdpRdp* rdp;
	UINT32 roff;
	UINT32 rlen;

	status = 0;
	rdp = fastpath->rdp;

	fastpath_read_update_header(s, &updateCode, &fragmentation, &compression);

	if (compression == FASTPATH_OUTPUT_COMPRESSION_USED)
		stream_read_BYTE(s, compressionFlags);
	else
		compressionFlags = 0;

	stream_read_UINT16(s, size);

	if (stream_get_left(s) < size)
		return -1;

	next_pos = stream_get_pos(s) + size;
	comp_stream = s;

	if (compressionFlags & PACKET_COMPRESSED)
	{
		if (decompress_rdp(rdp->mppc_dec, s->pointer, size, compressionFlags, &roff, &rlen))
		{
			comp_stream = stream_new(0);
			comp_stream->buffer = rdp->mppc_dec->history_buf + roff;
			comp_stream->pointer = comp_stream->buffer;
			comp_stream->capacity = rlen;
			size = comp_stream->capacity;
		}
		else
		{
			fprintf(stderr, "decompress_rdp() failed\n");
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

		if (stream_get_length(fastpath->updateData) > rdp->settings->MultifragMaxRequestSize)
		{
			fprintf(stderr, "fastpath PDU is bigger than MultifragMaxRequestSize\n");
			return -1;
		}

		if (fragmentation == FASTPATH_FRAGMENT_LAST)
		{
			update_stream = fastpath->updateData;
			totalSize = stream_get_length(update_stream);
			stream_set_pos(update_stream, 0);
		}
	}

	if (update_stream)
	{
		status = fastpath_recv_update(fastpath, updateCode, totalSize, update_stream);

		if (status < 0)
			return -1;
	}

	stream_set_pos(s, next_pos);

	if (comp_stream != s)
		free(comp_stream);

	return status;
}

int fastpath_recv_updates(rdpFastPath* fastpath, wStream* s)
{
	int status = 0;
	rdpUpdate* update = fastpath->rdp->update;

	IFCALL(update->BeginPaint, update->context);

	while (stream_get_left(s) >= 3)
	{
		if (fastpath_recv_update_data(fastpath, s) < 0)
			return -1;
	}

	IFCALL(update->EndPaint, update->context);

	return status;
}

static BOOL fastpath_read_input_event_header(wStream* s, BYTE* eventFlags, BYTE* eventCode)
{
	BYTE eventHeader;

	if (stream_get_left(s) < 1)
		return FALSE;

	stream_read_BYTE(s, eventHeader); /* eventHeader (1 byte) */

	*eventFlags = (eventHeader & 0x1F);
	*eventCode = (eventHeader >> 5);

	return TRUE;
}

static BOOL fastpath_recv_input_event_scancode(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	UINT16 flags;
	UINT16 code;

	if (stream_get_left(s) < 1)
		return FALSE;

	stream_read_BYTE(s, code); /* keyCode (1 byte) */

	flags = 0;
	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_EXTENDED))
		flags |= KBD_FLAGS_EXTENDED;

	IFCALL(fastpath->rdp->input->KeyboardEvent, fastpath->rdp->input, flags, code);

	return TRUE;
}

static BOOL fastpath_recv_input_event_mouse(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	UINT16 pointerFlags;
	UINT16 xPos;
	UINT16 yPos;

	if (stream_get_left(s) < 6)
		return FALSE;

	stream_read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_UINT16(s, xPos); /* xPos (2 bytes) */
	stream_read_UINT16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->MouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return TRUE;
}

static BOOL fastpath_recv_input_event_mousex(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	UINT16 pointerFlags;
	UINT16 xPos;
	UINT16 yPos;

	if (stream_get_left(s) < 6)
		return FALSE;

	stream_read_UINT16(s, pointerFlags); /* pointerFlags (2 bytes) */
	stream_read_UINT16(s, xPos); /* xPos (2 bytes) */
	stream_read_UINT16(s, yPos); /* yPos (2 bytes) */

	IFCALL(fastpath->rdp->input->ExtendedMouseEvent, fastpath->rdp->input, pointerFlags, xPos, yPos);

	return TRUE;
}

static BOOL fastpath_recv_input_event_sync(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	IFCALL(fastpath->rdp->input->SynchronizeEvent, fastpath->rdp->input, eventFlags);

	return TRUE;
}

static BOOL fastpath_recv_input_event_unicode(rdpFastPath* fastpath, wStream* s, BYTE eventFlags)
{
	UINT16 unicodeCode;
	UINT16 flags;

	if (stream_get_left(s) < 2)
		return FALSE;

	stream_read_UINT16(s, unicodeCode); /* unicodeCode (2 bytes) */

	flags = 0;
	if ((eventFlags & FASTPATH_INPUT_KBDFLAGS_RELEASE))
		flags |= KBD_FLAGS_RELEASE;
	else
		flags |= KBD_FLAGS_DOWN;

	IFCALL(fastpath->rdp->input->UnicodeKeyboardEvent, fastpath->rdp->input, flags, unicodeCode);

	return TRUE;
}

static BOOL fastpath_recv_input_event(rdpFastPath* fastpath, wStream* s)
{
	BYTE eventFlags;
	BYTE eventCode;

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
			fprintf(stderr, "Unknown eventCode %d\n", eventCode);
			break;
	}

	return TRUE;
}

int fastpath_recv_inputs(rdpFastPath* fastpath, wStream* s)
{
	BYTE i;

	if (fastpath->numberEvents == 0)
	{
		/**
		 * If numberEvents is not provided in fpInputHeader, it will be provided
		 * as one additional byte here.
		 */

		if (stream_get_left(s) < 1)
			return -1;

		stream_read_BYTE(s, fastpath->numberEvents); /* eventHeader (1 byte) */
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

	if (rdp->do_crypt)
	{
		sec_bytes = 8;
		if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			sec_bytes += 4;
	}
	else
		sec_bytes = 0;
	return sec_bytes;
}

wStream* fastpath_input_pdu_init(rdpFastPath* fastpath, BYTE eventFlags, BYTE eventCode)
{
	rdpRdp *rdp;
	wStream* s;

	rdp = fastpath->rdp;

	s = transport_send_stream_init(rdp->transport, 256);
	stream_seek(s, 3); /* fpInputHeader, length1 and length2 */
	if (rdp->do_crypt) {
		rdp->sec_flags |= SEC_ENCRYPT;
		if (rdp->do_secure_checksum)
			rdp->sec_flags |= SEC_SECURE_CHECKSUM;
	}
	stream_seek(s, fastpath_get_sec_bytes(rdp));
	stream_write_BYTE(s, eventFlags | (eventCode << 5)); /* eventHeader (1 byte) */
	return s;
}

BOOL fastpath_send_input_pdu(rdpFastPath* fastpath, wStream* s)
{
	rdpRdp *rdp;
	UINT16 length;
	BYTE eventHeader;
	int sec_bytes;

	rdp = fastpath->rdp;

	length = stream_get_length(s);
	if (length >= (2 << 14))
	{
		fprintf(stderr, "Maximum FastPath PDU length is 32767\n");
		return FALSE;
	}

	eventHeader = FASTPATH_INPUT_ACTION_FASTPATH;
	eventHeader |= (1 << 2); /* numberEvents */
	if (rdp->sec_flags & SEC_ENCRYPT)
		eventHeader |= (FASTPATH_INPUT_ENCRYPTED << 6);
	if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
		eventHeader |= (FASTPATH_INPUT_SECURE_CHECKSUM << 6);

	stream_set_pos(s, 0);
	stream_write_BYTE(s, eventHeader);
	sec_bytes = fastpath_get_sec_bytes(fastpath->rdp);
	/*
	 * We always encode length in two bytes, eventhough we could use
	 * only one byte if length <= 0x7F. It is just easier that way,
	 * because we can leave room for fixed-length header, store all
	 * the data first and then store the header.
	 */
	stream_write_UINT16_be(s, 0x8000 | length);

	if (sec_bytes > 0)
	{
		BYTE* fpInputEvents;
		UINT16 fpInputEvents_length;

		fpInputEvents = stream_get_tail(s) + sec_bytes;
		fpInputEvents_length = length - 3 - sec_bytes;
		if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
			security_salted_mac_signature(rdp, fpInputEvents, fpInputEvents_length, TRUE, stream_get_tail(s));
		else
			security_mac_signature(rdp, fpInputEvents, fpInputEvents_length, stream_get_tail(s));
		security_encrypt(fpInputEvents, fpInputEvents_length, rdp);
	}

	rdp->sec_flags = 0;

	stream_set_pos(s, length);
	if (transport_write(fastpath->rdp->transport, s) < 0)
		return FALSE;

	return TRUE;
}

wStream* fastpath_update_pdu_init(rdpFastPath* fastpath)
{
	wStream* s;
	s = transport_send_stream_init(fastpath->rdp->transport, FASTPATH_MAX_PACKET_SIZE);
	stream_seek(s, 3); /* fpOutputHeader, length1 and length2 */
	stream_seek(s, fastpath_get_sec_bytes(fastpath->rdp));
	stream_seek(s, 3); /* updateHeader, size */
	return s;
}

BOOL fastpath_send_update_pdu(rdpFastPath* fastpath, BYTE updateCode, wStream* s)
{
	rdpRdp* rdp;
	BYTE* bm;
	BYTE* ptr_to_crypt;
	BYTE* ptr_sig;
	BYTE* holdp;
	int fragment;
	int sec_bytes;
	int try_comp;
	int comp_flags;
	int header_bytes;
	int cflags;
	int pdu_data_bytes;
	int dlen;
	int bytes_to_crypt;
	BOOL result;
	UINT16 pduLength;
	UINT16 maxLength;
	UINT32 totalLength;
	BYTE fragmentation;
	BYTE header;
	wStream* update;
	wStream* comp_update;
	wStream* ls;

	result = TRUE;
	rdp = fastpath->rdp;
	sec_bytes = fastpath_get_sec_bytes(rdp);
	maxLength = FASTPATH_MAX_PACKET_SIZE - (6 + sec_bytes);
	totalLength = stream_get_length(s) - (6 + sec_bytes);
	stream_set_pos(s, 0);
	update = stream_new(0);
	try_comp = rdp->settings->CompressionEnabled;
	comp_update = stream_new(0);

	for (fragment = 0; totalLength > 0 || fragment == 0; fragment++)
	{
		stream_get_mark(s, holdp);
		ls = s;
		dlen = MIN(maxLength, totalLength);
		cflags = 0;
		comp_flags = 0;
		header_bytes = 6 + sec_bytes;
		pdu_data_bytes = dlen;
		if (try_comp)
		{
			if (compress_rdp(rdp->mppc_enc, ls->pointer + header_bytes, dlen))
			{
				if (rdp->mppc_enc->flags & PACKET_COMPRESSED)
				{
					cflags = rdp->mppc_enc->flags;
					pdu_data_bytes = rdp->mppc_enc->bytes_in_opb;
					comp_flags = FASTPATH_OUTPUT_COMPRESSION_USED;
					header_bytes = 7 + sec_bytes;
					bm = (BYTE*) (rdp->mppc_enc->outputBuffer - header_bytes);
					stream_attach(comp_update, bm, pdu_data_bytes + header_bytes);
					ls = comp_update;
				}
			}
			else
				fprintf(stderr, "fastpath_send_update_pdu: mppc_encode failed\n");
		}

		totalLength -= dlen;
		pduLength = pdu_data_bytes + header_bytes;

		if (totalLength == 0)
			fragmentation = (fragment == 0) ? FASTPATH_FRAGMENT_SINGLE : FASTPATH_FRAGMENT_LAST;
		else
			fragmentation = (fragment == 0) ? FASTPATH_FRAGMENT_FIRST : FASTPATH_FRAGMENT_NEXT;

		stream_get_mark(ls, bm);
		header = 0;
		if (sec_bytes > 0)
			header |= (FASTPATH_OUTPUT_ENCRYPTED << 6);
		stream_write_BYTE(ls, header); /* fpOutputHeader (1 byte) */
		stream_write_BYTE(ls, 0x80 | (pduLength >> 8)); /* length1 */
		stream_write_BYTE(ls, pduLength & 0xFF); /* length2 */

		if (sec_bytes > 0)
			stream_seek(ls, sec_bytes);

		fastpath_write_update_header(ls, updateCode, fragmentation, comp_flags);

		/* extra byte if compressed */
		if (ls == comp_update)
		{
			stream_write_BYTE(ls, cflags);
			bytes_to_crypt = pdu_data_bytes + 4;
		}
		else
			bytes_to_crypt = pdu_data_bytes + 3;

		stream_write_UINT16(ls, pdu_data_bytes);

		stream_attach(update, bm, pduLength);
		stream_seek(update, pduLength);

		if (sec_bytes > 0)
		{
			/* does this work ? */
			ptr_to_crypt = bm + 3 + sec_bytes;
			ptr_sig = bm + 3;
			if (rdp->sec_flags & SEC_SECURE_CHECKSUM)
				security_salted_mac_signature(rdp, ptr_to_crypt, bytes_to_crypt, TRUE, ptr_sig);
			else
				security_mac_signature(rdp, ptr_to_crypt, bytes_to_crypt, ptr_sig);
			security_encrypt(ptr_to_crypt, bytes_to_crypt, rdp);
		}

		if (transport_write(fastpath->rdp->transport, update) < 0)
		{
			result = FALSE;
			break;
		}

		/* Reserve 6 + sec_bytes bytes for the next fragment header, if any. */
		stream_set_mark(s, holdp + dlen);
	}

	stream_detach(update);
	stream_detach(comp_update);
	stream_free(update);
	stream_free(comp_update);

	return result;
}

rdpFastPath* fastpath_new(rdpRdp* rdp)
{
	rdpFastPath* fastpath;

	fastpath = (rdpFastPath*) malloc(sizeof(rdpFastPath));
	ZeroMemory(fastpath, sizeof(rdpFastPath));

	fastpath->rdp = rdp;
	fastpath->updateData = stream_new(4096);

	return fastpath;
}

void fastpath_free(rdpFastPath* fastpath)
{
	stream_free(fastpath->updateData);
	free(fastpath);
}
