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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rdpgfx.common")

#include "rdpgfx_common.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_read_header(wStream* s, RDPGFX_HEADER* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Read_UINT16(s, header->cmdId);     /* cmdId (2 bytes) */
	Stream_Read_UINT16(s, header->flags);     /* flags (2 bytes) */
	Stream_Read_UINT32(s, header->pduLength); /* pduLength (4 bytes) */

	if (header->pduLength < 8)
	{
		WLog_ERR(TAG, "header->pduLength %u less than 8!", header->pduLength);
		return ERROR_INVALID_DATA;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, (header->pduLength - 8)))
		return ERROR_INVALID_DATA;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_header(wStream* s, const RDPGFX_HEADER* header)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, header->cmdId);     /* cmdId (2 bytes) */
	Stream_Write_UINT16(s, header->flags);     /* flags (2 bytes) */
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
	WINPR_ASSERT(s);
	WINPR_ASSERT(pt16);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pt16->x); /* x (2 bytes) */
	Stream_Read_UINT16(s, pt16->y); /* y (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_point16(wStream* s, const RDPGFX_POINT16* point16)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(point16);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return CHANNEL_RC_NO_MEMORY;

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
	WINPR_ASSERT(s);
	WINPR_ASSERT(rect16);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, rect16->left);   /* left (2 bytes) */
	Stream_Read_UINT16(s, rect16->top);    /* top (2 bytes) */
	Stream_Read_UINT16(s, rect16->right);  /* right (2 bytes) */
	Stream_Read_UINT16(s, rect16->bottom); /* bottom (2 bytes) */
	if (rect16->left >= rect16->right)
		return ERROR_INVALID_DATA;
	if (rect16->top >= rect16->bottom)
		return ERROR_INVALID_DATA;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_rect16(wStream* s, const RECTANGLE_16* rect16)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(rect16);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT16(s, rect16->left);   /* left (2 bytes) */
	Stream_Write_UINT16(s, rect16->top);    /* top (2 bytes) */
	Stream_Write_UINT16(s, rect16->right);  /* right (2 bytes) */
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
	WINPR_ASSERT(s);
	WINPR_ASSERT(color32);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, color32->B);  /* B (1 byte) */
	Stream_Read_UINT8(s, color32->G);  /* G (1 byte) */
	Stream_Read_UINT8(s, color32->R);  /* R (1 byte) */
	Stream_Read_UINT8(s, color32->XA); /* XA (1 byte) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_write_color32(wStream* s, const RDPGFX_COLOR32* color32)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(color32);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT8(s, color32->B);  /* B (1 byte) */
	Stream_Write_UINT8(s, color32->G);  /* G (1 byte) */
	Stream_Write_UINT8(s, color32->R);  /* R (1 byte) */
	Stream_Write_UINT8(s, color32->XA); /* XA (1 byte) */
	return CHANNEL_RC_OK;
}
