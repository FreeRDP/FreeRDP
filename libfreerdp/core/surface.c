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

static BOOL update_recv_surfcmd_bitmap_header_ex(wStream* s, TS_COMPRESSED_BITMAP_HEADER_EX* header)
{
	if (!s || !header)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 24)
		return FALSE;

	Stream_Read_UINT32(s, header->highUniqueId);
	Stream_Read_UINT32(s, header->lowUniqueId);
	Stream_Read_UINT64(s, header->tmMilliseconds);
	Stream_Read_UINT64(s, header->tmSeconds);
	return TRUE;
}

static BOOL update_recv_surfcmd_bitmap_ex(wStream* s, TS_BITMAP_DATA_EX* bmp)
{
	size_t pos;
	if (!s || !bmp)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;

	Stream_Read_UINT8(s, bmp->bpp);
	Stream_Read_UINT8(s, bmp->flags);
	Stream_Seek(s, 1); /* reserved */
	Stream_Read_UINT8(s, bmp->codecID);
	Stream_Read_UINT16(s, bmp->width);
	Stream_Read_UINT16(s, bmp->height);
	Stream_Read_UINT32(s, bmp->bitmapDataLength);

	if ((bmp->bpp < 1) || (bmp->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %"PRIu32"", bmp->bpp);
		return FALSE;
	}

	memset(&bmp->exBitmapDataHeader, 0, sizeof(TS_COMPRESSED_BITMAP_HEADER_EX));
	if (bmp->flags & EX_COMPRESSED_BITMAP_HEADER_PRESENT)
	{
		if (!update_recv_surfcmd_bitmap_header_ex(s, &bmp->exBitmapDataHeader))
			return FALSE;
	}

	if (Stream_GetRemainingLength(s) < bmp->bitmapDataLength)
		return FALSE;

	pos = Stream_GetPosition(s) + bmp->bitmapDataLength;
	bmp->bitmapData = Stream_Pointer(s);
	Stream_SetPosition(s, pos);

	return TRUE;
}

static BOOL update_recv_surfcmd_surface_bits(rdpUpdate* update, wStream* s)
{
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT16(s, cmd->destLeft);
	Stream_Read_UINT16(s, cmd->destTop);
	Stream_Read_UINT16(s, cmd->destRight);
	Stream_Read_UINT16(s, cmd->destBottom);
	if (!update_recv_surfcmd_bitmap_ex(s, &cmd->bmp))
		return FALSE;

	if (!update->SurfaceBits)
	{
		WLog_ERR(TAG, "Missing callback update->SurfaceBits");
		return FALSE;
	}

	return update->SurfaceBits(update->context, cmd);
}

static BOOL update_recv_surfcmd_frame_marker(rdpUpdate* update, wStream* s)
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

	return update->SurfaceFrameMarker(update->context, marker);
}

int update_recv_surfcmds(rdpUpdate* update, wStream* s)
{
	BYTE* mark;
	UINT16 cmdType;

	while (Stream_GetRemainingLength(s) >= 2)
	{
		const size_t start = Stream_GetPosition(s);
		Stream_GetPointer(s, mark);
		Stream_Read_UINT16(s, cmdType);

		switch (cmdType)
		{
			case CMDTYPE_SET_SURFACE_BITS:
			case CMDTYPE_STREAM_SURFACE_BITS:
				if (!update_recv_surfcmd_surface_bits(update, s))
					return -1;

				break;

			case CMDTYPE_FRAME_MARKER:
				if (!update_recv_surfcmd_frame_marker(update, s))
					return -1;

				break;

			default:
				WLog_ERR(TAG, "unknown cmdType 0x%04"PRIX16"", cmdType);
				return -1;
		}

		if (update->dump_rfx)
		{
			const size_t size = Stream_GetPosition(s) - start;
			/* TODO: treat return values */
			pcap_add_record(update->pcap_rfx, mark, size);
			pcap_flush(update->pcap_rfx);
		}
	}

	return 0;
}

static BOOL update_write_surfcmd_bitmap_header_ex(wStream* s, const TS_COMPRESSED_BITMAP_HEADER_EX* header)
{
	if (!s || !header)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 24))
		return FALSE;

	Stream_Write_UINT32(s, header->highUniqueId);
	Stream_Write_UINT32(s, header->lowUniqueId);
	Stream_Write_UINT64(s, header->tmMilliseconds);
	Stream_Write_UINT64(s, header->tmSeconds);

	return TRUE;
}

static BOOL update_write_surfcmd_bitmap_ex(wStream* s, const TS_BITMAP_DATA_EX* bmp)
{
	if (!s || !bmp)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 12))
		return FALSE;

	Stream_Write_UINT8(s, bmp->bpp);
	Stream_Write_UINT8(s, bmp->flags);
	Stream_Write_UINT8(s, 0); /* reserved1, reserved2 */
	Stream_Write_UINT8(s, bmp->codecID);
	Stream_Write_UINT16(s, bmp->width);
	Stream_Write_UINT16(s, bmp->height);
	Stream_Write_UINT32(s, bmp->bitmapDataLength);

	if (bmp->flags & EX_COMPRESSED_BITMAP_HEADER_PRESENT)
	{
		if (!update_write_surfcmd_bitmap_header_ex(s, &bmp->exBitmapDataHeader))
			return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, bmp->bitmapDataLength))
		return FALSE;

	Stream_Write(s, bmp->bitmapData, bmp->bitmapDataLength);
	return TRUE;
}

BOOL update_write_surfcmd_surface_bits(wStream* s, const SURFACE_BITS_COMMAND* cmd)
{
	if (!Stream_EnsureRemainingCapacity(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH))
		return FALSE;

	Stream_Write_UINT16(s, CMDTYPE_STREAM_SURFACE_BITS);
	Stream_Write_UINT16(s, cmd->destLeft);
	Stream_Write_UINT16(s, cmd->destTop);
	Stream_Write_UINT16(s, cmd->destRight);
	Stream_Write_UINT16(s, cmd->destBottom);

	return update_write_surfcmd_bitmap_ex(s, &cmd->bmp);
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
