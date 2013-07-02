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

	if (Stream_GetRemainingLength(s) < 20)
		return -1;

	Stream_Read_UINT16(s, cmd->destLeft);
	Stream_Read_UINT16(s, cmd->destTop);
	Stream_Read_UINT16(s, cmd->destRight);
	Stream_Read_UINT16(s, cmd->destBottom);
	Stream_Read_UINT8(s, cmd->bpp);
	Stream_Seek(s, 2); /* reserved1, reserved2 */
	Stream_Read_UINT8(s, cmd->codecID);
	Stream_Read_UINT16(s, cmd->width);
	Stream_Read_UINT16(s, cmd->height);
	Stream_Read_UINT32(s, cmd->bitmapDataLength);

	if (Stream_GetRemainingLength(s) < cmd->bitmapDataLength)
		return -1;

	pos = Stream_GetPosition(s) + cmd->bitmapDataLength;
	cmd->bitmapData = Stream_Pointer(s);

	Stream_SetPosition(s, pos);
	*length = 20 + cmd->bitmapDataLength;

	IFCALL(update->SurfaceBits, update->context, cmd);

	return 0;
}

static int update_recv_surfcmd_frame_marker(rdpUpdate* update, wStream* s, UINT32 *length)
{
	SURFACE_FRAME_MARKER* marker = &update->surface_frame_marker;

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT16(s, marker->frameAction);
	Stream_Read_UINT32(s, marker->frameId);

	IFCALL(update->SurfaceFrameMarker, update->context, marker);

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
		Stream_GetPointer(s, mark);

		Stream_Read_UINT16(s, cmdType);
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
	Stream_EnsureRemainingCapacity(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH);

	Stream_Write_UINT16(s, CMDTYPE_STREAM_SURFACE_BITS);

	Stream_Write_UINT16(s, cmd->destLeft);
	Stream_Write_UINT16(s, cmd->destTop);
	Stream_Write_UINT16(s, cmd->destRight);
	Stream_Write_UINT16(s, cmd->destBottom);
	Stream_Write_UINT8(s, cmd->bpp);
	Stream_Write_UINT16(s, 0); /* reserved1, reserved2 */
	Stream_Write_UINT8(s, cmd->codecID);
	Stream_Write_UINT16(s, cmd->width);
	Stream_Write_UINT16(s, cmd->height);
	Stream_Write_UINT32(s, cmd->bitmapDataLength);
}

void update_write_surfcmd_frame_marker(wStream* s, UINT16 frameAction, UINT32 frameId)
{
	Stream_EnsureRemainingCapacity(s, SURFCMD_FRAME_MARKER_LENGTH);

	Stream_Write_UINT16(s, CMDTYPE_FRAME_MARKER);

	Stream_Write_UINT16(s, frameAction);
	Stream_Write_UINT32(s, frameId);
}
