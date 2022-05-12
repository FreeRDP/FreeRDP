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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <freerdp/utils/pcap.h>
#include <freerdp/log.h>

#include "../cache/cache.h"
#include "surface.h"

#define TAG FREERDP_TAG("core.surface")

static BOOL update_recv_surfcmd_bitmap_header_ex(wStream* s, TS_COMPRESSED_BITMAP_HEADER_EX* header)
{
	if (!s || !header)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return FALSE;

	Stream_Read_UINT32(s, header->highUniqueId);
	Stream_Read_UINT32(s, header->lowUniqueId);
	Stream_Read_UINT64(s, header->tmMilliseconds);
	Stream_Read_UINT64(s, header->tmSeconds);
	return TRUE;
}

static BOOL update_recv_surfcmd_bitmap_ex(wStream* s, TS_BITMAP_DATA_EX* bmp)
{
	if (!s || !bmp)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	Stream_Read_UINT8(s, bmp->bpp);
	Stream_Read_UINT8(s, bmp->flags);
	Stream_Seek(s, 1); /* reserved */
	Stream_Read_UINT8(s, bmp->codecID);
	Stream_Read_UINT16(s, bmp->width);
	Stream_Read_UINT16(s, bmp->height);
	Stream_Read_UINT32(s, bmp->bitmapDataLength);

	if ((bmp->width == 0) || (bmp->height == 0))
	{
		WLog_ERR(TAG, "invalid size value width=%" PRIu16 ", height=%" PRIu16, bmp->width,
		         bmp->height);
		return FALSE;
	}

	if ((bmp->bpp < 1) || (bmp->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %" PRIu32 "", bmp->bpp);
		return FALSE;
	}

	if (bmp->flags & EX_COMPRESSED_BITMAP_HEADER_PRESENT)
	{
		if (!update_recv_surfcmd_bitmap_header_ex(s, &bmp->exBitmapDataHeader))
			return FALSE;
	}

	bmp->bitmapData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, bmp->bitmapDataLength))
	{
		WLog_ERR(TAG, "expected bitmapDataLength %" PRIu32 ", not enough data",
		         bmp->bitmapDataLength);
		return FALSE;
	}
	return TRUE;
}

static BOOL update_recv_surfcmd_is_rect_valid(const rdpContext* context,
                                              const SURFACE_BITS_COMMAND* cmd)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);
	WINPR_ASSERT(cmd);

	/* We need a rectangle with left/top being smaller than right/bottom.
	 * Also do not allow empty rectangles. */
	if ((cmd->destTop >= cmd->destBottom) || (cmd->destLeft >= cmd->destRight))
	{
		WLog_WARN(TAG,
		          "Empty surface bits command rectangle: %" PRIu16 "x%" PRIu16 "-%" PRIu16
		          "x%" PRIu16,
		          cmd->destLeft, cmd->destTop, cmd->destRight, cmd->destBottom);
		return FALSE;
	}

	/* The rectangle needs to fit into our session size */
	if ((cmd->destRight > context->settings->DesktopWidth) ||
	    (cmd->destBottom > context->settings->DesktopHeight))
	{
		WLog_WARN(TAG,
		          "Invalid surface bits command rectangle: %" PRIu16 "x%" PRIu16 "-%" PRIu16
		          "x%" PRIu16 " does not fit %" PRIu32 "x%" PRIu32,
		          cmd->destLeft, cmd->destTop, cmd->destRight, cmd->destBottom,
		          context->settings->DesktopWidth, context->settings->DesktopHeight);
		return FALSE;
	}

	return TRUE;
}

static BOOL update_recv_surfcmd_surface_bits(rdpUpdate* update, wStream* s, UINT16 cmdType)
{
	BOOL rc = FALSE;
	SURFACE_BITS_COMMAND cmd = { 0 };

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		goto fail;

	cmd.cmdType = cmdType;
	Stream_Read_UINT16(s, cmd.destLeft);
	Stream_Read_UINT16(s, cmd.destTop);
	Stream_Read_UINT16(s, cmd.destRight);
	Stream_Read_UINT16(s, cmd.destBottom);

	if (!update_recv_surfcmd_is_rect_valid(update->context, &cmd))
		goto fail;

	if (!update_recv_surfcmd_bitmap_ex(s, &cmd.bmp))
		goto fail;

	if (!IFCALLRESULT(TRUE, update->SurfaceBits, update->context, &cmd))
	{
		WLog_DBG(TAG, "update->SurfaceBits implementation failed");
		goto fail;
	}

	rc = TRUE;
fail:
	return rc;
}

static BOOL update_recv_surfcmd_frame_marker(rdpUpdate* update, wStream* s)
{
	SURFACE_FRAME_MARKER marker = { 0 };
	rdp_update_internal* up = update_cast(update);

	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, marker.frameAction);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		WLog_WARN(TAG,
		          "[SERVER-BUG]: got %" PRIuz ", expected %" PRIuz
		          " bytes. [MS-RDPBCGR] 2.2.9.2.3 Frame Marker Command (TS_FRAME_MARKER) is "
		          "missing frameId, ignoring",
		          Stream_GetRemainingLength(s), 4);
	else
		Stream_Read_UINT32(s, marker.frameId);
	WLog_Print(up->log, WLOG_DEBUG, "SurfaceFrameMarker: action: %s (%" PRIu32 ") id: %" PRIu32 "",
	           (!marker.frameAction) ? "Begin" : "End", marker.frameAction, marker.frameId);

	if (!update->SurfaceFrameMarker)
	{
		WLog_ERR(TAG, "Missing callback update->SurfaceFrameMarker");
		return FALSE;
	}

	if (!update->SurfaceFrameMarker(update->context, &marker))
	{
		WLog_DBG(TAG, "update->SurfaceFrameMarker implementation failed");
		return FALSE;
	}

	return TRUE;
}

int update_recv_surfcmds(rdpUpdate* update, wStream* s)
{
	UINT16 cmdType;
	rdp_update_internal* up = update_cast(update);

	WINPR_ASSERT(s);

	while (Stream_GetRemainingLength(s) >= 2)
	{
		const size_t start = Stream_GetPosition(s);
		const BYTE* mark = Stream_Pointer(s);

		Stream_Read_UINT16(s, cmdType);

		switch (cmdType)
		{
			case CMDTYPE_SET_SURFACE_BITS:
			case CMDTYPE_STREAM_SURFACE_BITS:
				if (!update_recv_surfcmd_surface_bits(update, s, cmdType))
					return -1;

				break;

			case CMDTYPE_FRAME_MARKER:
				if (!update_recv_surfcmd_frame_marker(update, s))
					return -1;

				break;

			default:
				WLog_ERR(TAG, "unknown cmdType 0x%04" PRIX16 "", cmdType);
				return -1;
		}

		if (up->dump_rfx)
		{
			const size_t size = Stream_GetPosition(s) - start;
			/* TODO: treat return values */
			pcap_add_record(up->pcap_rfx, mark, size);
			pcap_flush(up->pcap_rfx);
		}
	}

	return 0;
}

static BOOL update_write_surfcmd_bitmap_header_ex(wStream* s,
                                                  const TS_COMPRESSED_BITMAP_HEADER_EX* header)
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

	if (bmp->codecID > UINT8_MAX)
	{
		WLog_ERR(TAG, "Invalid TS_BITMAP_DATA_EX::codecID=0x%04" PRIx16 "", bmp->codecID);
		return FALSE;
	}
	Stream_Write_UINT8(s, bmp->bpp);
	Stream_Write_UINT8(s, bmp->flags);
	Stream_Write_UINT8(s, 0); /* reserved1, reserved2 */
	Stream_Write_UINT8(s, (UINT8)bmp->codecID);
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
	UINT16 cmdType;
	if (!Stream_EnsureRemainingCapacity(s, SURFCMD_SURFACE_BITS_HEADER_LENGTH))
		return FALSE;

	cmdType = cmd->cmdType;
	switch (cmdType)
	{
		case CMDTYPE_SET_SURFACE_BITS:
		case CMDTYPE_STREAM_SURFACE_BITS:
			break;
		default:
			WLog_WARN(TAG,
			          "SURFACE_BITS_COMMAND->cmdType 0x%04" PRIx16
			          " not allowed, correcting to 0x%04" PRIx16,
			          cmdType, CMDTYPE_STREAM_SURFACE_BITS);
			cmdType = CMDTYPE_STREAM_SURFACE_BITS;
			break;
	}

	Stream_Write_UINT16(s, cmdType);
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
