/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Surface Commands
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

#include <freerdp/utils/pcap.h>

#include "surface.h"

static int update_recv_surfcmd_surface_bits(rdpUpdate* update, wStream* s, UINT32* length)
{
	int pos;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;

	if (stream_get_left(s) < 20)
		return -1;

	stream_read_UINT16(s, cmd->destLeft);
	stream_read_UINT16(s, cmd->destTop);
	stream_read_UINT16(s, cmd->destRight);
	stream_read_UINT16(s, cmd->destBottom);
	stream_read_BYTE(s, cmd->bpp);
	stream_seek(s, 2); /* reserved1, reserved2 */
	stream_read_BYTE(s, cmd->codecID);
	stream_read_UINT16(s, cmd->width);
	stream_read_UINT16(s, cmd->height);
	stream_read_UINT32(s, cmd->bitmapDataLength);

	if (stream_get_left(s) < cmd->bitmapDataLength)
		return -1;

	pos = stream_get_pos(s) + cmd->bitmapDataLength;
	cmd->bitmapData = stream_get_tail(s);

	stream_set_pos(s, pos);
	*length = 20 + cmd->bitmapDataLength;

	IFCALL(update->SurfaceBits, update->context, cmd);

	return 0;
}

static void update_send_frame_acknowledge(rdpRdp* rdp, UINT32 frameId)
{
	wStream* s;

	s = rdp_data_pdu_init(rdp);
	stream_write_UINT32(s, frameId);
	rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FRAME_ACKNOWLEDGE, rdp->mcs->user_id);
}

static int update_recv_surfcmd_frame_marker(rdpUpdate* update, wStream* s, UINT32 *length)
{
	SURFACE_FRAME_MARKER* marker = &update->surface_frame_marker;

	if (stream_get_left(s) < 6)
		return -1;

	stream_read_UINT16(s, marker->frameAction);
	stream_read_UINT32(s, marker->frameId);

	IFCALL(update->SurfaceFrameMarker, update->context, marker);

	if (update->context->rdp->settings->ReceivedCapabilities[CAPSET_TYPE_FRAME_ACKNOWLEDGE] &&
			(update->context->rdp->settings->FrameAcknowledge > 0) &&
			(marker->frameAction == SURFACECMD_FRAMEACTION_END))
	{
		update_send_frame_acknowledge(update->context->rdp, marker->frameId);
	}

	*length = 6;

	return 0;
}

int update_recv_surfcmds(rdpUpdate* update, UINT32 size, wStream* s)
{
	BYTE* mark;
	UINT16 cmdType;
	UINT32 cmdLength;

	while (size > 2)
	{
		stream_get_mark(s, mark);

		stream_read_UINT16(s, cmdType);
		size -= 2;

		switch (cmdType)
		{
			case CMDTYPE_SET_SURFACE_BITS:
			case CMDTYPE_STREAM_SURFACE_BITS:
				if (update_recv_surfcmd_surface_bits(update, s, &cmdLength) < 0)
					return -1;
				break;

			case CMDTYPE_FRAME_MARKER:
				if (update_recv_surfcmd_frame_marker(update, s, &cmdLength) < 0)
					return -1;
				break;

			default:
				DEBUG_WARN("unknown cmdType 0x%X", cmdType);
				return -1;
		}

		size -= cmdLength;

		if (update->dump_rfx)
		{
			pcap_add_record(update->pcap_rfx, mark, cmdLength + 2);
			pcap_flush(update->pcap_rfx);
		}
	}

	return 0;
}

void update_write_surfcmd_surface_bits_header(wStream* s, SURFACE_BITS_COMMAND* cmd)
{
	stream_check_size(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH);

	stream_write_UINT16(s, CMDTYPE_STREAM_SURFACE_BITS);

	stream_write_UINT16(s, cmd->destLeft);
	stream_write_UINT16(s, cmd->destTop);
	stream_write_UINT16(s, cmd->destRight);
	stream_write_UINT16(s, cmd->destBottom);
	stream_write_BYTE(s, cmd->bpp);
	stream_write_UINT16(s, 0); /* reserved1, reserved2 */
	stream_write_BYTE(s, cmd->codecID);
	stream_write_UINT16(s, cmd->width);
	stream_write_UINT16(s, cmd->height);
	stream_write_UINT32(s, cmd->bitmapDataLength);
}

void update_write_surfcmd_frame_marker(wStream* s, UINT16 frameAction, UINT32 frameId)
{
	stream_check_size(s, SURFCMD_FRAME_MARKER_LENGTH);

	stream_write_UINT16(s, CMDTYPE_FRAME_MARKER);

	stream_write_UINT16(s, frameAction);
	stream_write_UINT32(s, frameId);
}
