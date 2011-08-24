/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "surface.h"

static int update_recv_surfcmd_surface_bits(rdpUpdate* update, STREAM* s)
{
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

static int update_recv_surfcmd_frame_marker(rdpUpdate* update, STREAM* s)
{
	uint16 frameAction;
	uint32 frameId;

	stream_read_uint16(s, frameAction);
	stream_read_uint32(s, frameId);
	/*printf("frameAction %d frameId %d\n", frameAction, frameId);*/

	return 6;
}

boolean update_recv_surfcmds(rdpUpdate* update, uint16 size, STREAM* s)
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
				size -= update_recv_surfcmd_surface_bits(update, s);
				break;

			case CMDTYPE_FRAME_MARKER:
				size -= update_recv_surfcmd_frame_marker(update, s);
				break;

			default:
				DEBUG_WARN("unknown cmdType 0x%X", cmdType);
				return False;
		}
	}
	return True;
}

void update_write_surfcmd_surface_bits_header(STREAM* s, SURFACE_BITS_COMMAND* cmd)
{
	stream_check_size(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH);

	stream_write_uint16(s, CMDTYPE_STREAM_SURFACE_BITS);

	stream_write_uint16(s, cmd->destLeft);
	stream_write_uint16(s, cmd->destTop);
	stream_write_uint16(s, cmd->destRight);
	stream_write_uint16(s, cmd->destBottom);
	stream_write_uint8(s, cmd->bpp);
	stream_write_uint16(s, 0); /* reserved1, reserved2 */
	stream_write_uint8(s, cmd->codecID);
	stream_write_uint16(s, cmd->width);
	stream_write_uint16(s, cmd->height);
	stream_write_uint32(s, cmd->bitmapDataLength);
}

void update_write_surfcmd_frame_marker(STREAM* s, uint16 frameAction, uint32 frameId)
{
	stream_check_size(s, SURFCMD_FRAME_MARKER_LENGTH);

	stream_write_uint16(s, CMDTYPE_FRAME_MARKER);

	stream_write_uint16(s, frameAction);
	stream_write_uint32(s, frameId);
}

