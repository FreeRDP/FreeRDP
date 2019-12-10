/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL common functions
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
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
#include "rail_common.h"

#include <winpr/crt.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("rail.common")

const char* const RAIL_ORDER_TYPE_STRINGS[] = { "",
	                                            "Execute",
	                                            "Activate",
	                                            "System Parameters Update",
	                                            "System Command",
	                                            "Handshake",
	                                            "Notify Event",
	                                            "",
	                                            "Window Move",
	                                            "Local Move/Size",
	                                            "Min Max Info",
	                                            "Client Status",
	                                            "System Menu",
	                                            "Language Bar Info",
	                                            "Get Application ID Request",
	                                            "Get Application ID Response",
	                                            "Execute Result",
	                                            "",
	                                            "",
	                                            "",
	                                            "",
	                                            "",
	                                            "" };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_read_pdu_header(wStream* s, UINT16* orderType, UINT16* orderLength)
{
	if (!s || !orderType || !orderLength)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, *orderType);   /* orderType (2 bytes) */
	Stream_Read_UINT16(s, *orderLength); /* orderLength (2 bytes) */
	return CHANNEL_RC_OK;
}

void rail_write_pdu_header(wStream* s, UINT16 orderType, UINT16 orderLength)
{
	Stream_Write_UINT16(s, orderType);   /* orderType (2 bytes) */
	Stream_Write_UINT16(s, orderLength); /* orderLength (2 bytes) */
}

wStream* rail_pdu_init(size_t length)
{
	wStream* s;
	s = Stream_New(NULL, length + RAIL_PDU_HEADER_LENGTH);

	if (!s)
		return NULL;

	Stream_Seek(s, RAIL_PDU_HEADER_LENGTH);
	return s;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_read_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake)
{
	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
	return CHANNEL_RC_OK;
}

void rail_write_handshake_order(wStream* s, const RAIL_HANDSHAKE_ORDER* handshake)
{
	Stream_Write_UINT32(s, handshake->buildNumber); /* buildNumber (4 bytes) */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_read_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, handshakeEx->buildNumber);        /* buildNumber (4 bytes) */
	Stream_Read_UINT32(s, handshakeEx->railHandshakeFlags); /* railHandshakeFlags (4 bytes) */
	return CHANNEL_RC_OK;
}

void rail_write_handshake_ex_order(wStream* s, const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	Stream_Write_UINT32(s, handshakeEx->buildNumber);        /* buildNumber (4 bytes) */
	Stream_Write_UINT32(s, handshakeEx->railHandshakeFlags); /* railHandshakeFlags (4 bytes) */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_write_unicode_string(wStream* s, const RAIL_UNICODE_STRING* unicode_string)
{
	if (!s || !unicode_string)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_EnsureRemainingCapacity(s, 2 + unicode_string->length))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, unicode_string->length);                  /* cbString (2 bytes) */
	Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_write_unicode_string_value(wStream* s, const RAIL_UNICODE_STRING* unicode_string)
{
	size_t length;

	if (!s || !unicode_string)
		return ERROR_INVALID_PARAMETER;

	length = unicode_string->length;

	if (length > 0)
	{
		if (!Stream_EnsureRemainingCapacity(s, length))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		Stream_Write(s, unicode_string->string, length); /* string */
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_high_contrast(wStream* s, RAIL_HIGH_CONTRAST* highContrast)
{
	if (!s || !highContrast)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 8)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, highContrast->flags);             /* flags (4 bytes) */
	Stream_Read_UINT32(s, highContrast->colorSchemeLength); /* colorSchemeLength (4 bytes) */

	if (!rail_read_unicode_string(s, &highContrast->colorScheme)) /* colorScheme */
		return ERROR_INTERNAL_ERROR;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_high_contrast(wStream* s, const RAIL_HIGH_CONTRAST* highContrast)
{
	UINT32 colorSchemeLength;

	if (!s || !highContrast)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return CHANNEL_RC_NO_MEMORY;

	colorSchemeLength = highContrast->colorScheme.length + 2;
	Stream_Write_UINT32(s, highContrast->flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, colorSchemeLength);   /* colorSchemeLength (4 bytes) */
	return rail_write_unicode_string(s, &highContrast->colorScheme); /* colorScheme */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_filterkeys(wStream* s, TS_FILTERKEYS* filterKeys)
{
	if (!s || !filterKeys)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 20)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, filterKeys->Flags);
	Stream_Read_UINT32(s, filterKeys->WaitTime);
	Stream_Read_UINT32(s, filterKeys->DelayTime);
	Stream_Read_UINT32(s, filterKeys->RepeatTime);
	Stream_Read_UINT32(s, filterKeys->BounceTime);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_filterkeys(wStream* s, const TS_FILTERKEYS* filterKeys)
{
	if (!s || !filterKeys)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_EnsureRemainingCapacity(s, 20))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, filterKeys->Flags);
	Stream_Write_UINT32(s, filterKeys->WaitTime);
	Stream_Write_UINT32(s, filterKeys->DelayTime);
	Stream_Write_UINT32(s, filterKeys->RepeatTime);
	Stream_Write_UINT32(s, filterKeys->BounceTime);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_read_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam, BOOL extendedSpiSupported)
{
	BYTE body;
	UINT error = CHANNEL_RC_OK;

	if (!s || !sysparam)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, sysparam->param); /* systemParam (4 bytes) */

	sysparam->params = 0; /* bitflags of received params */

	switch (sysparam->param)
	{
		/* Client sysparams */
		case SPI_SET_DRAG_FULL_WINDOWS:
			sysparam->params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->dragFullWindows = body != 0;
			break;

		case SPI_SET_KEYBOARD_CUES:
			sysparam->params |= SPI_MASK_SET_KEYBOARD_CUES;
			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->keyboardCues = body != 0;
			break;

		case SPI_SET_KEYBOARD_PREF:
			sysparam->params |= SPI_MASK_SET_KEYBOARD_PREF;
			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->keyboardPref = body != 0;
			break;

		case SPI_SET_MOUSE_BUTTON_SWAP:
			sysparam->params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->mouseButtonSwap = body != 0;
			break;

		case SPI_SET_WORK_AREA:
			sysparam->params |= SPI_MASK_SET_WORK_AREA;

			if (Stream_GetRemainingLength(s) < 8)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT16(s, sysparam->workArea.left);   /* left (2 bytes) */
			Stream_Read_UINT16(s, sysparam->workArea.top);    /* top (2 bytes) */
			Stream_Read_UINT16(s, sysparam->workArea.right);  /* right (2 bytes) */
			Stream_Read_UINT16(s, sysparam->workArea.bottom); /* bottom (2 bytes) */
			break;

		case SPI_DISPLAY_CHANGE:
			sysparam->params |= SPI_MASK_DISPLAY_CHANGE;

			if (Stream_GetRemainingLength(s) < 8)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT16(s, sysparam->displayChange.left);   /* left (2 bytes) */
			Stream_Read_UINT16(s, sysparam->displayChange.top);    /* top (2 bytes) */
			Stream_Read_UINT16(s, sysparam->displayChange.right);  /* right (2 bytes) */
			Stream_Read_UINT16(s, sysparam->displayChange.bottom); /* bottom (2 bytes) */
			break;

		case SPI_TASKBAR_POS:
			sysparam->params |= SPI_MASK_TASKBAR_POS;

			if (Stream_GetRemainingLength(s) < 8)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT16(s, sysparam->taskbarPos.left);   /* left (2 bytes) */
			Stream_Read_UINT16(s, sysparam->taskbarPos.top);    /* top (2 bytes) */
			Stream_Read_UINT16(s, sysparam->taskbarPos.right);  /* right (2 bytes) */
			Stream_Read_UINT16(s, sysparam->taskbarPos.bottom); /* bottom (2 bytes) */
			break;

		case SPI_SET_HIGH_CONTRAST:
			sysparam->params |= SPI_MASK_SET_HIGH_CONTRAST;
			if (Stream_GetRemainingLength(s) < 8)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			error = rail_read_high_contrast(s, &sysparam->highContrast);
			break;

		case SPI_SETCARETWIDTH:
			sysparam->params |= SPI_MASK_SET_CARET_WIDTH;

			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			if (Stream_GetRemainingLength(s) < 4)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT32(s, sysparam->caretWidth);

			if (sysparam->caretWidth < 0x0001)
				return ERROR_INVALID_DATA;

			break;

		case SPI_SETSTICKYKEYS:
			sysparam->params |= SPI_MASK_SET_STICKY_KEYS;

			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			if (Stream_GetRemainingLength(s) < 4)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT32(s, sysparam->stickyKeys);
			break;

		case SPI_SETTOGGLEKEYS:
			sysparam->params |= SPI_MASK_SET_TOGGLE_KEYS;

			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			if (Stream_GetRemainingLength(s) < 4)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			Stream_Read_UINT32(s, sysparam->toggleKeys);
			break;

		case SPI_SETFILTERKEYS:
			sysparam->params |= SPI_MASK_SET_FILTER_KEYS;

			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			if (Stream_GetRemainingLength(s) < 20)
			{
				WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
				return ERROR_INVALID_DATA;
			}

			error = rail_read_filterkeys(s, &sysparam->filterKeys);
			break;

		/* Server sysparams */
		case SPI_SETSCREENSAVEACTIVE:
			sysparam->params |= SPI_MASK_SET_SCREEN_SAVE_ACTIVE;

			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->setScreenSaveActive = body != 0;
			break;

		case SPI_SETSCREENSAVESECURE:
			sysparam->params |= SPI_MASK_SET_SET_SCREEN_SAVE_SECURE;

			Stream_Read_UINT8(s, body); /* body (1 byte) */
			sysparam->setScreenSaveSecure = body != 0;
			break;

		default:
			break;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 err2or code
 */
UINT rail_write_sysparam_order(wStream* s, const RAIL_SYSPARAM_ORDER* sysparam,
                               BOOL extendedSpiSupported)
{
	BYTE body;
	UINT error = CHANNEL_RC_OK;

	if (!s || !sysparam)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_EnsureRemainingCapacity(s, 12))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, sysparam->param); /* systemParam (4 bytes) */

	switch (sysparam->param)
	{
		/* Client sysparams */
		case SPI_SET_DRAG_FULL_WINDOWS:
			body = sysparam->dragFullWindows ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_KEYBOARD_CUES:
			body = sysparam->keyboardCues ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_KEYBOARD_PREF:
			body = sysparam->keyboardPref ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_MOUSE_BUTTON_SWAP:
			body = sysparam->mouseButtonSwap ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SET_WORK_AREA:
			Stream_Write_UINT16(s, sysparam->workArea.left);   /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.top);    /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.right);  /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.bottom); /* bottom (2 bytes) */
			break;

		case SPI_DISPLAY_CHANGE:
			Stream_Write_UINT16(s, sysparam->displayChange.left);   /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.top);    /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.right);  /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.bottom); /* bottom (2 bytes) */
			break;

		case SPI_TASKBAR_POS:
			Stream_Write_UINT16(s, sysparam->taskbarPos.left);   /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.top);    /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.right);  /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.bottom); /* bottom (2 bytes) */
			break;

		case SPI_SET_HIGH_CONTRAST:
			error = rail_write_high_contrast(s, &sysparam->highContrast);
			break;

		case SPI_SETCARETWIDTH:
			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			if (sysparam->caretWidth < 0x0001)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->caretWidth);
			break;

		case SPI_SETSTICKYKEYS:
			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->stickyKeys);
			break;

		case SPI_SETTOGGLEKEYS:
			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->toggleKeys);
			break;

		case SPI_SETFILTERKEYS:
			if (!extendedSpiSupported)
				return ERROR_INVALID_DATA;

			error = rail_write_filterkeys(s, &sysparam->filterKeys);
			break;

		/* Server sysparams */
		case SPI_SETSCREENSAVEACTIVE:
			body = sysparam->setScreenSaveActive ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		case SPI_SETSCREENSAVESECURE:
			body = sysparam->setScreenSaveSecure ? 1 : 0;
			Stream_Write_UINT8(s, body);
			break;

		default:
			return ERROR_INVALID_PARAMETER;
	}

	return error;
}

BOOL rail_is_extended_spi_supported(UINT32 channelFlags)
{
	return channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED;
}
