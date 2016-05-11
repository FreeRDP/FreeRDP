/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpgfx.common")

#include "rdpgfx_common.h"

const char* RDPGFX_CMDID_STRINGS[] =
{
	"RDPGFX_CMDID_UNUSED_0000",
	"RDPGFX_CMDID_WIRETOSURFACE_1",
	"RDPGFX_CMDID_WIRETOSURFACE_2",
	"RDPGFX_CMDID_DELETEENCODINGCONTEXT",
	"RDPGFX_CMDID_SOLIDFILL",
	"RDPGFX_CMDID_SURFACETOSURFACE",
	"RDPGFX_CMDID_SURFACETOCACHE",
	"RDPGFX_CMDID_CACHETOSURFACE",
	"RDPGFX_CMDID_EVICTCACHEENTRY",
	"RDPGFX_CMDID_CREATESURFACE",
	"RDPGFX_CMDID_DELETESURFACE",
	"RDPGFX_CMDID_STARTFRAME",
	"RDPGFX_CMDID_ENDFRAME",
	"RDPGFX_CMDID_FRAMEACKNOWLEDGE",
	"RDPGFX_CMDID_RESETGRAPHICS",
	"RDPGFX_CMDID_MAPSURFACETOOUTPUT",
	"RDPGFX_CMDID_CACHEIMPORTOFFER",
	"RDPGFX_CMDID_CACHEIMPORTREPLY",
	"RDPGFX_CMDID_CAPSADVERTISE",
	"RDPGFX_CMDID_CAPSCONFIRM",
	"RDPGFX_CMDID_UNUSED_0014",
	"RDPGFX_CMDID_MAPSURFACETOWINDOW"
};

const char* rdpgfx_get_cmd_id_string(UINT16 cmdId)
{
	if (cmdId <= RDPGFX_CMDID_MAPSURFACETOWINDOW)
		return RDPGFX_CMDID_STRINGS[cmdId];
	else
		return "RDPGFX_CMDID_UNKNOWN";
}

const char* rdpgfx_get_codec_id_string(UINT16 codecId)
{
	switch (codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			return "RDPGFX_CODECID_UNCOMPRESSED";
		case RDPGFX_CODECID_CAVIDEO:
			return "RDPGFX_CODECID_CAVIDEO";
		case RDPGFX_CODECID_CLEARCODEC:
			return "RDPGFX_CODECID_CLEARCODEC";
		case RDPGFX_CODECID_PLANAR:
			return "RDPGFX_CODECID_PLANAR";
		case RDPGFX_CODECID_AVC420:
			return "RDPGFX_CODECID_AVC420";
		case RDPGFX_CODECID_AVC444:
			return "RDPGFX_CODECID_AVC444";
		case RDPGFX_CODECID_ALPHA:
			return "RDPGFX_CODECID_ALPHA";
		case RDPGFX_CODECID_CAPROGRESSIVE:
			return "RDPGFX_CODECID_CAPROGRESSIVE";
		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			return "RDPGFX_CODECID_CAPROGRESSIVE_V2";
	}

	return "RDPGFX_CODECID_UNKNOWN";
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_read_header(wStream* s, RDPGFX_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Read_UINT16(s, header->cmdId); /* cmdId (2 bytes) */
	Stream_Read_UINT16(s, header->flags); /* flags (2 bytes) */
	Stream_Read_UINT32(s, header->pduLength); /* pduLength (4 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_header(wStream* s, RDPGFX_HEADER* header)
{
	Stream_Write_UINT16(s, header->cmdId); /* cmdId (2 bytes) */
	Stream_Write_UINT16(s, header->flags); /* flags (2 bytes) */
	Stream_Write_UINT32(s, header->pduLength); /* pduLength (4 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_read_point16(wStream* s, RDPGFX_POINT16* pt16)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pt16->x); /* x (2 bytes) */
	Stream_Read_UINT16(s, pt16->y); /* y (2 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_point16(wStream* s, RDPGFX_POINT16* point16)
{
	Stream_Write_UINT16(s, point16->x); /* x (2 bytes) */
	Stream_Write_UINT16(s, point16->y); /* y (2 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_read_rect16(wStream* s, RECTANGLE_16* rect16)
{
	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, rect16->left); /* left (2 bytes) */
	Stream_Read_UINT16(s, rect16->top); /* top (2 bytes) */
	Stream_Read_UINT16(s, rect16->right); /* right (2 bytes) */
	Stream_Read_UINT16(s, rect16->bottom); /* bottom (2 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_rect16(wStream* s, RECTANGLE_16* rect16)
{
	Stream_Write_UINT16(s, rect16->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, rect16->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, rect16->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, rect16->bottom); /* bottom (2 bytes) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_read_color32(wStream* s, RDPGFX_COLOR32* color32)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, color32->B); /* B (1 byte) */
	Stream_Read_UINT8(s, color32->G); /* G (1 byte) */
	Stream_Read_UINT8(s, color32->R); /* R (1 byte) */
	Stream_Read_UINT8(s, color32->XA); /* XA (1 byte) */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_color32(wStream* s, RDPGFX_COLOR32* color32)
{
	Stream_Write_UINT8(s, color32->B); /* B (1 byte) */
	Stream_Write_UINT8(s, color32->G); /* G (1 byte) */
	Stream_Write_UINT8(s, color32->R); /* R (1 byte) */
	Stream_Write_UINT8(s, color32->XA); /* XA (1 byte) */

	return CHANNEL_RC_OK;
}
