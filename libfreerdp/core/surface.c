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
#include <freerdp/log.h>

#include "surface.h"

#define TAG FREERDP_TAG("core.surface")

static BOOL update_recv_surfcmd_surface_bits(rdpUpdate* update, wStream* s, UINT32* length)
{
	int pos;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;

	if (Stream_GetRemainingLength(s) < 20)
		return FALSE;

	Stream_Read_UINT16(s, cmd->destLeft);
	Stream_Read_UINT16(s, cmd->destTop);
	Stream_Read_UINT16(s, cmd->destRight);
	Stream_Read_UINT16(s, cmd->destBottom);
	Stream_Read_UINT8(s, cmd->bpp);

	if ((cmd->bpp < 1) || (cmd->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %"PRIu32"", cmd->bpp);
		return FALSE;
	}

	Stream_Seek(s, 2); /* reserved1, reserved2 */
	Stream_Read_UINT8(s, cmd->codecID);
	Stream_Read_UINT16(s, cmd->width);
	Stream_Read_UINT16(s, cmd->height);
	Stream_Read_UINT32(s, cmd->bitmapDataLength);

	if (Stream_GetRemainingLength(s) < cmd->bitmapDataLength)
		return FALSE;

	pos = Stream_GetPosition(s) + cmd->bitmapDataLength;
	cmd->bitmapData = Stream_Pointer(s);
	Stream_SetPosition(s, pos);
	*length = 20 + cmd->bitmapDataLength;

	if (!update->SurfaceBits)
	{
		WLog_ERR(TAG, "Missing callback update->SurfaceBits");
		return FALSE;
	}

	return update->SurfaceBits(update->context, cmd);
}

static BOOL update_recv_surfcmd_frame_marker(rdpUpdate* update, wStream* s, UINT32* length)
{
	SURFACE_FRAME_MARKER* marker = &update->surface_frame_marker;

	if (Stream_GetRemainingLength(s) < 6)
		return FALSE;

	Stream_Read_UINT16(s, marker->frameAction);
	Stream_Read_UINT32(s, marker->frameId);
	WLog_Print(update->log, WLOG_DEBUG, "SurfaceFrameMarker: action: %s (%"PRIu32") id: %"PRIu32"",
	           (!marker->frameAction) ? "Begin" : "End",
	           marker->frameAction, marker->frameId);

	if (!update->SurfaceFrameMarker)
	{
		WLog_ERR(TAG, "Missing callback update->SurfaceFrameMarker");
		return FALSE;
	}

	*length = 6;
	return update->SurfaceFrameMarker(update->context, marker);
}

int update_recv_surfcmds(rdpUpdate* update, UINT32 size, wStream* s)
{
	BYTE* mark;
	UINT16 cmdType;
	UINT32 cmdLength = 0;

	while (size > 2)
	{
		Stream_GetPointer(s, mark);
		Stream_Read_UINT16(s, cmdType);
		size -= 2;

		switch (cmdType)
		{
			case CMDTYPE_SET_SURFACE_BITS:
			case CMDTYPE_STREAM_SURFACE_BITS:
				if (!update_recv_surfcmd_surface_bits(update, s, &cmdLength))
					return -1;

				break;

			case CMDTYPE_FRAME_MARKER:
				if (!update_recv_surfcmd_frame_marker(update, s, &cmdLength))
					return -1;

				break;

			default:
				WLog_ERR(TAG, "unknown cmdType 0x%04"PRIX16"", cmdType);
				return -1;
		}

		size -= cmdLength;

		if (update->dump_rfx)
		{
			/* TODO: treat return values */
			pcap_add_record(update->pcap_rfx, mark, cmdLength + 2);
			pcap_flush(update->pcap_rfx);
		}
	}

	return 0;
}

BOOL update_write_surfcmd_surface_bits_header(wStream* s,
        const SURFACE_BITS_COMMAND* cmd)
{
	if (!Stream_EnsureRemainingCapacity(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH))
		return FALSE;

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
	return TRUE;
}

BOOL update_write_surfcmd_frame_marker(wStream* s, UINT16 frameAction, UINT32 frameId)
{
	if (!Stream_EnsureRemainingCapacity(s, SURFCMD_FRAME_MARKER_LENGTH))
		return FALSE;

	Stream_Write_UINT16(s, CMDTYPE_FRAME_MARKER);
	Stream_Write_UINT16(s, frameAction);
	Stream_Write_UINT32(s, frameId);
	return TRUE;
}
