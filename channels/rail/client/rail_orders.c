/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL) Orders
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <freerdp/channels/log.h>

#include "rail_orders.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_unicode_string(wStream* s, const RAIL_UNICODE_STRING* unicode_string)
{
	if (!s || !unicode_string)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_EnsureRemainingCapacity(s, 2 + unicode_string->length))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, unicode_string->length); /* cbString (2 bytes) */
	Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_unicode_string_value(wStream* s, const RAIL_UNICODE_STRING* unicode_string)
{
	size_t length;

	if (!s || !unicode_string)
		return ERROR_INVALID_PARAMETER;

	length =  unicode_string->length;

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
UINT rail_send_pdu(railPlugin* rail, wStream* s, UINT16 orderType)
{
	UINT16 orderLength;

	if (!rail || !s)
		return ERROR_INVALID_PARAMETER;

	orderLength = (UINT16) Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rail_write_pdu_header(s, orderType, orderLength);
	Stream_SetPosition(s, orderLength);
	WLog_Print(rail->log, WLOG_DEBUG, "Sending %s PDU, length: %"PRIu16"",
	           RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);
	return rail_send_channel_data(rail, s);
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

	colorSchemeLength = highContrast->colorScheme.length + 2;
	Stream_Write_UINT32(s, highContrast->flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, colorSchemeLength); /* colorSchemeLength (4 bytes) */
	return rail_write_unicode_string(s, &highContrast->colorScheme); /* colorScheme */
}

static UINT rail_write_filterkeys(wStream* s, const TS_FILTERKEYS* filterKeys)
{
	if (!s || !filterKeys)
		return ERROR_INVALID_PARAMETER;

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
static UINT rail_read_server_exec_result_order(wStream* s, RAIL_EXEC_RESULT_ORDER* execResult)
{
	if (!s || !execResult)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, execResult->flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, execResult->execResult); /* execResult (2 bytes) */
	Stream_Read_UINT32(s, execResult->rawResult); /* rawResult (4 bytes) */
	Stream_Seek_UINT16(s); /* padding (2 bytes) */
	return rail_read_unicode_string(s, &execResult->exeOrFile) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR; /* exeOrFile */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_server_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam)
{
	BYTE body;

	if (!s || !sysparam)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, sysparam->param); /* systemParam (4 bytes) */
	Stream_Read_UINT8(s, body); /* body (1 byte) */

	switch (sysparam->param)
	{
		case SPI_SETSCREENSAVEACTIVE:
			sysparam->setScreenSaveActive = (body != 0) ? TRUE : FALSE;
			break;

		case SPI_SETSCREENSAVESECURE:
			sysparam->setScreenSaveSecure = (body != 0) ? TRUE : FALSE;
			break;

		default:
			break;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_server_minmaxinfo_order(wStream* s, RAIL_MINMAXINFO_ORDER* minmaxinfo)
{
	if (!s || !minmaxinfo)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 20)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, minmaxinfo->windowId); /* windowId (4 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxWidth); /* maxWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxHeight); /* maxHeight (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxPosX); /* maxPosX (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxPosY); /* maxPosY (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->minTrackWidth); /* minTrackWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->minTrackHeight); /* minTrackHeight (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxTrackWidth); /* maxTrackWidth (2 bytes) */
	Stream_Read_UINT16(s, minmaxinfo->maxTrackHeight); /* maxTrackHeight (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_server_localmovesize_order(wStream* s,
        RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	UINT16 isMoveSizeStart;

	if (!s || !localMoveSize)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, localMoveSize->windowId); /* windowId (4 bytes) */
	Stream_Read_UINT16(s, isMoveSizeStart); /* isMoveSizeStart (2 bytes) */
	localMoveSize->isMoveSizeStart = (isMoveSizeStart != 0) ? TRUE : FALSE;
	Stream_Read_UINT16(s, localMoveSize->moveSizeType); /* moveSizeType (2 bytes) */
	Stream_Read_UINT16(s, localMoveSize->posX); /* posX (2 bytes) */
	Stream_Read_UINT16(s, localMoveSize->posY); /* posY (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_server_get_appid_resp_order(wStream* s,
        RAIL_GET_APPID_RESP_ORDER* getAppidResp)
{
	if (!s || !getAppidResp)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 516)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, getAppidResp->windowId); /* windowId (4 bytes) */
	Stream_Read(s, (BYTE*) & (getAppidResp->applicationId), 512); /* applicationId (256 UNICODE chars) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	if (!s || !langbarInfo)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, langbarInfo->languageBarStatus); /* languageBarStatus (4 bytes) */
	return CHANNEL_RC_OK;
}

static UINT rail_write_client_status_order(wStream* s, const RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	if (!s || !clientStatus)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, clientStatus->flags); /* flags (4 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_client_exec_order(wStream* s, UINT16 flags,
        const RAIL_UNICODE_STRING* exeOrFile, const RAIL_UNICODE_STRING* workingDir,
        const RAIL_UNICODE_STRING* arguments)
{
	UINT error;

	if (!s || !exeOrFile || !workingDir || !arguments)
		return ERROR_INVALID_PARAMETER;

	/* [MS-RDPERP] 2.2.2.3.1 Client Execute PDU (TS_RAIL_ORDER_EXEC)
	 * Check argument limits */
	if ((exeOrFile->length > 520) || (workingDir->length > 520) ||
	    (arguments->length > 16000))
	{
		WLog_ERR(TAG,
		         "TS_RAIL_ORDER_EXEC argument limits exceeded: ExeOrFile=%"PRIu16" [max=520], WorkingDir=%"PRIu16" [max=520], Arguments=%"PRIu16" [max=16000]",
		         exeOrFile->length, workingDir->length, arguments->length);
		return ERROR_BAD_ARGUMENTS;
	}

	Stream_Write_UINT16(s, flags); /* flags (2 bytes) */
	Stream_Write_UINT16(s, exeOrFile->length); /* exeOrFileLength (2 bytes) */
	Stream_Write_UINT16(s, workingDir->length); /* workingDirLength (2 bytes) */
	Stream_Write_UINT16(s, arguments->length); /* argumentsLength (2 bytes) */

	if ((error = rail_write_unicode_string_value(s, exeOrFile)))
	{
		WLog_ERR(TAG, "rail_write_unicode_string_value failed with error %"PRIu32"", error);
		return error;
	}

	if ((error = rail_write_unicode_string_value(s, workingDir)))
	{
		WLog_ERR(TAG, "rail_write_unicode_string_value failed with error %"PRIu32"", error);
		return error;
	}

	if ((error = rail_write_unicode_string_value(s, arguments)))
	{
		WLog_ERR(TAG, "rail_write_unicode_string_value failed with error %"PRIu32"", error);
		return error;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_write_client_sysparam_order(railPlugin* rail, wStream* s,
                                      const RAIL_SYSPARAM_ORDER* sysparam)
{
	BYTE body;
	UINT error = CHANNEL_RC_OK;

	if (!s || !sysparam)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, sysparam->param); /* systemParam (4 bytes) */

	switch (sysparam->param)
	{
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
			Stream_Write_UINT16(s, sysparam->workArea.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->workArea.bottom); /* bottom (2 bytes) */
			break;

		case SPI_DISPLAY_CHANGE:
			Stream_Write_UINT16(s, sysparam->displayChange.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->displayChange.bottom); /* bottom (2 bytes) */
			break;

		case SPI_TASKBAR_POS:
			Stream_Write_UINT16(s, sysparam->taskbarPos.left); /* left (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.top); /* top (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.right); /* right (2 bytes) */
			Stream_Write_UINT16(s, sysparam->taskbarPos.bottom); /* bottom (2 bytes) */
			break;

		case SPI_SET_HIGH_CONTRAST:
			error = rail_write_high_contrast(s, &sysparam->highContrast);
			break;

		case SPI_SETCARETWIDTH:
			if ((rail->channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED) == 0)
				return ERROR_INVALID_DATA;

			if (sysparam->caretWidth < 0x0001)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->caretWidth);
			break;

		case SPI_SETSTICKYKEYS:
			if ((rail->channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED) == 0)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->stickyKeys);
			break;

		case SPI_SETTOGGLEKEYS:
			if ((rail->channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED) == 0)
				return ERROR_INVALID_DATA;

			Stream_Write_UINT32(s, sysparam->toggleKeys);
			break;

		case SPI_SETFILTERKEYS:
			if ((rail->channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_EXTENDED_SPI_SUPPORTED) == 0)
				return ERROR_INVALID_DATA;

			error = rail_write_filterkeys(s, &sysparam->filterKeys);
			break;

		default:
			return ERROR_INVALID_PARAMETER;
	}

	return error;
}

static UINT rail_write_client_activate_order(wStream* s, const RAIL_ACTIVATE_ORDER* activate)
{
	BYTE enabled;

	if (!s || !activate)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, activate->windowId); /* windowId (4 bytes) */
	enabled = activate->enabled ? 1 : 0;
	Stream_Write_UINT8(s, enabled); /* enabled (1 byte) */
	return ERROR_SUCCESS;
}

static UINT rail_write_client_sysmenu_order(wStream* s, const RAIL_SYSMENU_ORDER* sysmenu)
{
	if (!s || !sysmenu)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, sysmenu->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, sysmenu->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, sysmenu->top); /* top (2 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_client_syscommand_order(wStream* s, const RAIL_SYSCOMMAND_ORDER* syscommand)
{
	if (!s || !syscommand)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, syscommand->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, syscommand->command); /* command (2 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_client_notify_event_order(wStream* s,
        const RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	if (!s || !notifyEvent)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, notifyEvent->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT32(s, notifyEvent->notifyIconId); /* notifyIconId (4 bytes) */
	Stream_Write_UINT32(s, notifyEvent->message); /* notifyIconId (4 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_client_window_move_order(wStream* s,
        const RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	if (!s || !windowMove)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, windowMove->windowId); /* windowId (4 bytes) */
	Stream_Write_UINT16(s, windowMove->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, windowMove->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, windowMove->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, windowMove->bottom); /* bottom (2 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_client_get_appid_req_order(wStream* s,
        const RAIL_GET_APPID_REQ_ORDER* getAppidReq)
{
	if (!s || !getAppidReq)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, getAppidReq->windowId); /* windowId (4 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_langbar_info_order(wStream* s, const RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	if (!s || !langbarInfo)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, langbarInfo->languageBarStatus); /* languageBarStatus (4 bytes) */
	return ERROR_SUCCESS;
}

static UINT rail_write_languageime_info_order(wStream* s,
        const RAIL_LANGUAGEIME_INFO_ORDER* langImeInfo)
{
	if (!s || !langImeInfo)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, langImeInfo->ProfileType);
	Stream_Write_UINT32(s, langImeInfo->LanguageID);
	Stream_Write(s, &langImeInfo->LanguageProfileCLSID, sizeof(langImeInfo->LanguageProfileCLSID));
	Stream_Write(s, &langImeInfo->ProfileGUID, sizeof(langImeInfo->ProfileGUID));
	Stream_Write_UINT32(s, langImeInfo->KeyboardLayout);
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_handshake_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_HANDSHAKE_ORDER serverHandshake = { 0 };
	RAIL_HANDSHAKE_ORDER clientHandshake = { 0 };
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_handshake_order(s, &serverHandshake)))
	{
		WLog_ERR(TAG, "rail_read_handshake_order failed with error %"PRIu32"!", error);
		return error;
	}

	rail->channelBuildNumber = serverHandshake.buildNumber;
	clientHandshake.buildNumber = 0x00001DB0;
	/* 2.2.2.2.3 HandshakeEx PDU (TS_RAIL_ORDER_HANDSHAKE_EX)
	 * Client response is really a Handshake PDU */
	error = context->ClientHandshake(context, &clientHandshake);

	if (error != CHANNEL_RC_OK)
		return error;

	if (context->custom)
	{
		IFCALLRET(context->ServerHandshake, error, context, &serverHandshake);

		if (error)
			WLog_ERR(TAG, "context.ServerHandshake failed with error %"PRIu32"", error);
	}

	return error;
}

static BOOL rail_is_feature_supported(const rdpContext* context, UINT32 featureMask)
{
	UINT32 supported, masked;

	if (!context || !context->settings)
		return FALSE;

	supported = context->settings->RemoteApplicationSupportLevel &
	            context->settings->RemoteApplicationSupportMask;
	masked = (supported & featureMask);

	if (masked != featureMask)
		return FALSE;

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_handshake_ex_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_HANDSHAKE_EX_ORDER serverHandshake = { 0 };
	RAIL_HANDSHAKE_ORDER clientHandshake = { 0 };
	UINT error;

	if (!rail || !context || !s)
		return ERROR_INVALID_PARAMETER;

	if (!rail_is_feature_supported(rail->rdpcontext, RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED))
		return ERROR_BAD_CONFIGURATION;

	if ((error = rail_read_handshake_ex_order(s, &serverHandshake)))
	{
		WLog_ERR(TAG, "rail_read_handshake_ex_order failed with error %"PRIu32"!", error);
		return error;
	}

	rail->channelBuildNumber = serverHandshake.buildNumber;
	rail->channelFlags = serverHandshake.railHandshakeFlags;
	clientHandshake.buildNumber = 0x00001DB0;
	/* 2.2.2.2.3 HandshakeEx PDU (TS_RAIL_ORDER_HANDSHAKE_EX)
	 * Client response is really a Handshake PDU */
	error = context->ClientHandshake(context, &clientHandshake);

	if (error != CHANNEL_RC_OK)
		return error;

	if (context->custom)
	{
		IFCALLRET(context->ServerHandshakeEx, error, context, &serverHandshake);

		if (error)
			WLog_ERR(TAG, "context.ServerHandshakeEx failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_exec_result_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_EXEC_RESULT_ORDER execResult = { 0 };
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_exec_result_order(s, &execResult)))
	{
		WLog_ERR(TAG, "rail_read_server_exec_result_order failed with error %"PRIu32"!", error);
		goto fail;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerExecuteResult, error, context, &execResult);

		if (error)
			WLog_ERR(TAG, "context.ServerExecuteResult failed with error %"PRIu32"", error);
	}

fail:
	free(execResult.exeOrFile.string);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_server_sysparam_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_SYSPARAM_ORDER sysparam;
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_sysparam_order(s, &sysparam)))
	{
		WLog_ERR(TAG, "rail_read_server_sysparam_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerSystemParam, error, context, &sysparam);

		if (error)
			WLog_ERR(TAG, "context.ServerSystemParam failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_server_minmaxinfo_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_MINMAXINFO_ORDER minMaxInfo = { 0 };
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_minmaxinfo_order(s, &minMaxInfo)))
	{
		WLog_ERR(TAG, "rail_read_server_minmaxinfo_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerMinMaxInfo, error, context, &minMaxInfo);

		if (error)
			WLog_ERR(TAG, "context.ServerMinMaxInfo failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_server_localmovesize_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_LOCALMOVESIZE_ORDER localMoveSize = { 0 };
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_localmovesize_order(s, &localMoveSize)))
	{
		WLog_ERR(TAG, "rail_read_server_localmovesize_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerLocalMoveSize, error, context, &localMoveSize);

		if (error)
			WLog_ERR(TAG, "context.ServerLocalMoveSize failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_server_get_appid_resp_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_GET_APPID_RESP_ORDER getAppIdResp = { 0 };
	UINT error;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_get_appid_resp_order(s, &getAppIdResp)))
	{
		WLog_ERR(TAG, "rail_read_server_get_appid_resp_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerGetAppIdResponse, error, context, &getAppIdResp);

		if (error)
			WLog_ERR(TAG, "context.ServerGetAppIdResponse failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_langbar_info_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_LANGBAR_INFO_ORDER langBarInfo = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	if (!rail_is_feature_supported(rail->rdpcontext, RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED))
		return ERROR_BAD_CONFIGURATION;

	if ((error = rail_read_langbar_info_order(s, &langBarInfo)))
	{
		WLog_ERR(TAG, "rail_read_langbar_info_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerLanguageBarInfo, error, context, &langBarInfo);

		if (error)
			WLog_ERR(TAG, "context.ServerLanguageBarInfo failed with error %"PRIu32"", error);
	}

	return error;
}


static UINT rail_read_taskbar_info_order(wStream* s, RAIL_TASKBAR_INFO_ORDER* taskbarInfo)
{
	if (!s || !taskbarInfo)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, taskbarInfo->TaskbarMessage);
	Stream_Read_UINT32(s, taskbarInfo->WindowIdTab);
	Stream_Read_UINT32(s, taskbarInfo->Body);
	return CHANNEL_RC_OK;
}


static UINT rail_recv_taskbar_info_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_TASKBAR_INFO_ORDER taskBarInfo = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	/* 2.2.2.14.1 Taskbar Tab Info PDU (TS_RAIL_ORDER_TASKBARINFO)
	 * server -> client message only supported if announced. */
	if (!rail_is_feature_supported(rail->rdpcontext, RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED))
		return ERROR_BAD_CONFIGURATION;

	if ((error = rail_read_taskbar_info_order(s, &taskBarInfo)))
	{
		WLog_ERR(TAG, "rail_read_langbar_info_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerTaskBarInfo, error, context, &taskBarInfo);

		if (error)
			WLog_ERR(TAG, "context.ServerLanguageBarInfo failed with error %"PRIu32"", error);
	}

	return error;
}

static UINT rail_read_zorder_sync_order(wStream* s, RAIL_ZORDER_SYNC* zorder)
{
	if (!s || !zorder)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, zorder->windowIdMarker);
	return CHANNEL_RC_OK;
}

static UINT rail_recv_zorder_sync_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_ZORDER_SYNC zorder = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	if ((rail->clientStatus.flags & TS_RAIL_CLIENTSTATUS_ZORDER_SYNC) == 0)
		return ERROR_INVALID_DATA;

	if ((error = rail_read_zorder_sync_order(s, &zorder)))
	{
		WLog_ERR(TAG, "rail_read_zorder_sync_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerZOrderSync, error, context, &zorder);

		if (error)
			WLog_ERR(TAG, "context.ServerZOrderSync failed with error %"PRIu32"", error);
	}

	return error;
}

static UINT rail_read_order_cloak(wStream* s, RAIL_CLOAK* cloak)
{
	if (!s || !cloak)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, cloak->windowId);
	Stream_Read_UINT8(s, cloak->cloak);
	return CHANNEL_RC_OK;
}

static UINT rail_recv_order_cloak(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_CLOAK cloak = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	/* 2.2.2.12.1 Window Cloak State Change PDU (TS_RAIL_ORDER_CLOAK)
	 * server -> client message only supported if announced. */
	if ((rail->clientStatus.flags & TS_RAIL_CLIENTSTATUS_BIDIRECTIONAL_CLOAK_SUPPORTED) == 0)
		return ERROR_INVALID_DATA;

	if ((error = rail_read_order_cloak(s, &cloak)))
	{
		WLog_ERR(TAG, "rail_read_zorder_sync_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerCloak, error, context, &cloak);

		if (error)
			WLog_ERR(TAG, "context.ServerZOrderSync failed with error %"PRIu32"", error);
	}

	return error;
}

static UINT rail_read_power_display_request_order(wStream* s, RAIL_POWER_DISPLAY_REQUEST* power)
{
	if (!s || !power)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, power->active);
	return CHANNEL_RC_OK;
}

static UINT rail_recv_power_display_request_order(railPlugin* rail, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_POWER_DISPLAY_REQUEST power = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	/* 2.2.2.13.1 Power Display Request PDU(TS_RAIL_ORDER_POWER_DISPLAY_REQUEST)
	 */
	if ((rail->clientStatus.flags & TS_RAIL_CLIENTSTATUS_POWER_DISPLAY_REQUEST_SUPPORTED) == 0)
		return ERROR_INVALID_DATA;

	if ((error = rail_read_power_display_request_order(s, &power)))
	{
		WLog_ERR(TAG, "rail_read_zorder_sync_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerPowerDisplayRequest, error, context, &power);

		if (error)
			WLog_ERR(TAG, "context.ServerPowerDisplayRequest failed with error %"PRIu32"", error);
	}

	return error;
}


static UINT rail_read_get_application_id_extended_response_order(wStream* s,
        RAIL_GET_APPID_RESP_EX* id)
{
	if (!s || !id)
		return ERROR_INVALID_PARAMETER;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, id->windowID);

	if (!Stream_Read_UTF16_String(s, id->applicationID, ARRAYSIZE(id->applicationID)))
		return ERROR_INVALID_DATA;

	if (_wcsnlen(id->applicationID, ARRAYSIZE(id->applicationID)) >= ARRAYSIZE(id->applicationID))
		return ERROR_INVALID_DATA;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength failed!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, id->processId);

	if (!Stream_Read_UTF16_String(s, id->processImageName, ARRAYSIZE(id->processImageName)))
		return ERROR_INVALID_DATA;

	if (_wcsnlen(id->applicationID, ARRAYSIZE(id->processImageName)) >= ARRAYSIZE(id->processImageName))
		return ERROR_INVALID_DATA;

	return CHANNEL_RC_OK;
}

static UINT rail_recv_get_application_id_extended_response_order(railPlugin* rail,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	RAIL_GET_APPID_RESP_EX id = { 0 };
	UINT error;

	if (!context)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_get_application_id_extended_response_order(s, &id)))
	{
		WLog_ERR(TAG, "rail_read_get_application_id_extended_response_order failed with error %"PRIu32"!",
		         error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerGetAppidResponseExtended, error, context, &id);

		if (error)
			WLog_ERR(TAG, "context.ServerGetAppidResponseExtended failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_order_recv(railPlugin* rail, wStream* s)
{
	UINT16 orderType;
	UINT16 orderLength;
	UINT error;

	if (!rail || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_pdu_header(s, &orderType, &orderLength)))
	{
		WLog_ERR(TAG, "rail_read_pdu_header failed with error %"PRIu32"!", error);
		return error;
	}

	WLog_Print(rail->log, WLOG_DEBUG, "Received %s PDU, length:%"PRIu16"",
	           RAIL_ORDER_TYPE_STRINGS[((orderType & 0xF0) >> 3) + (orderType & 0x0F)], orderLength);

	switch (orderType)
	{
		case TS_RAIL_ORDER_HANDSHAKE:
			return rail_recv_handshake_order(rail, s);

		case TS_RAIL_ORDER_HANDSHAKE_EX:
			return rail_recv_handshake_ex_order(rail, s);

		case TS_RAIL_ORDER_EXEC_RESULT:
			return rail_recv_exec_result_order(rail, s);

		case TS_RAIL_ORDER_SYSPARAM:
			return rail_recv_server_sysparam_order(rail, s);

		case TS_RAIL_ORDER_MINMAXINFO:
			return rail_recv_server_minmaxinfo_order(rail, s);

		case TS_RAIL_ORDER_LOCALMOVESIZE:
			return rail_recv_server_localmovesize_order(rail, s);

		case TS_RAIL_ORDER_GET_APPID_RESP:
			return rail_recv_server_get_appid_resp_order(rail, s);

		case TS_RAIL_ORDER_LANGBARINFO:
			return rail_recv_langbar_info_order(rail, s);

		case TS_RAIL_ORDER_TASKBARINFO:
			return rail_recv_taskbar_info_order(rail, s);

		case TS_RAIL_ORDER_ZORDER_SYNC:
			return rail_recv_zorder_sync_order(rail, s);

		case TS_RAIL_ORDER_CLOAK:
			return rail_recv_order_cloak(rail, s);

		case TS_RAIL_ORDER_POWER_DISPLAY_REQUEST:
			return rail_recv_power_display_request_order(rail, s);

		case TS_RAIL_ORDER_GET_APPID_RESP_EX:
			return rail_recv_get_application_id_extended_response_order(rail, s);

		default:
			WLog_ERR(TAG,  "Unknown RAIL PDU order reveived.");
			return ERROR_INVALID_DATA;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_handshake_order(railPlugin* rail, const RAIL_HANDSHAKE_ORDER* handshake)
{
	wStream* s;
	UINT error;

	if (!rail || !handshake)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_handshake_order(s, handshake);
	error = rail_send_pdu(rail, s, TS_RAIL_ORDER_HANDSHAKE);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_handshake_ex_order(railPlugin* rail, const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	wStream* s;
	UINT error;

	if (!rail || !handshakeEx)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_HANDSHAKE_EX_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_handshake_ex_order(s, handshakeEx);
	error = rail_send_pdu(rail, s, TS_RAIL_ORDER_HANDSHAKE_EX);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_status_order(railPlugin* rail, const RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	wStream* s;
	UINT error;

	if (!rail || !clientStatus)
		return ERROR_INVALID_PARAMETER;

	rail->clientStatus = *clientStatus;
	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_status_order(s, clientStatus);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_CLIENTSTATUS);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_exec_order(railPlugin* rail, UINT16 flags,
                                 const RAIL_UNICODE_STRING* exeOrFile, const RAIL_UNICODE_STRING* workingDir,
                                 const RAIL_UNICODE_STRING* arguments)
{
	wStream* s;
	UINT error;
	size_t length;

	if (!rail || !exeOrFile || !workingDir || !arguments)
		return ERROR_INVALID_PARAMETER;

	length = RAIL_EXEC_ORDER_LENGTH +
	         exeOrFile->length +
	         workingDir->length +
	         arguments->length;
	s = rail_pdu_init(length);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rail_write_client_exec_order(s, flags, exeOrFile, workingDir, arguments)))
	{
		WLog_ERR(TAG, "rail_write_client_exec_order failed with error %"PRIu32"!", error);
		goto out;
	}

	if ((error = rail_send_pdu(rail, s, TS_RAIL_ORDER_EXEC)))
	{
		WLog_ERR(TAG, "rail_send_pdu failed with error %"PRIu32"!", error);
		goto out;
	}

out:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_client_sysparam_order(railPlugin* rail, const RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	size_t length = RAIL_SYSPARAM_ORDER_LENGTH;
	UINT error;

	if (!rail || !sysparam)
		return ERROR_INVALID_PARAMETER;

	switch (sysparam->param)
	{
		case SPI_SET_DRAG_FULL_WINDOWS:
		case SPI_SET_KEYBOARD_CUES:
		case SPI_SET_KEYBOARD_PREF:
		case SPI_SET_MOUSE_BUTTON_SWAP:
			length += 1;
			break;

		case SPI_SETCARETWIDTH:
		case SPI_SETSTICKYKEYS:
		case SPI_SETTOGGLEKEYS:
			length += 4;
			break;

		case SPI_SETFILTERKEYS:
			length += 20;
			break;

		case SPI_SET_WORK_AREA:
		case SPI_DISPLAY_CHANGE:
		case SPI_TASKBAR_POS:
			length += 8;
			break;

		case SPI_SET_HIGH_CONTRAST:
			length += sysparam->highContrast.colorSchemeLength + 10;
			break;

		default:
			length += 8;
			break;
	}

	s = rail_pdu_init(length);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rail_write_client_sysparam_order(rail, s, sysparam)))
	{
		WLog_ERR(TAG, "rail_write_client_sysparam_order failed with error %"PRIu32"!", error);
		goto out;
	}

	if ((error = rail_send_pdu(rail, s, TS_RAIL_ORDER_SYSPARAM)))
	{
		WLog_ERR(TAG, "rail_send_pdu failed with error %"PRIu32"!", error);
		goto out;
	}

out:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_client_sysparams_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam)
{
	UINT error = CHANNEL_RC_OK;

	if (!rail || !sysparam)
		return ERROR_INVALID_PARAMETER;

	if (sysparam->params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		sysparam->param = SPI_SET_HIGH_CONTRAST;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_TASKBAR_POS)
	{
		sysparam->param = SPI_TASKBAR_POS;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		sysparam->param = SPI_SET_MOUSE_BUTTON_SWAP;

		if ((error =  rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		sysparam->param = SPI_SET_KEYBOARD_PREF;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		sysparam->param = SPI_SET_DRAG_FULL_WINDOWS;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		sysparam->param = SPI_SET_KEYBOARD_CUES;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_WORK_AREA)
	{
		sysparam->param = SPI_SET_WORK_AREA;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_CARET_WIDTH)
	{
		sysparam->param = SPI_SETCARETWIDTH;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_STICKY_KEYS)
	{
		sysparam->param = SPI_SETSTICKYKEYS;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_TOGGLE_KEYS)
	{
		sysparam->param = SPI_SETTOGGLEKEYS;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_FILTER_KEYS)
	{
		sysparam->param = SPI_SETFILTERKEYS;

		if ((error = rail_send_client_sysparam_order(rail, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam_order failed with error %"PRIu32"!", error);
			return error;
		}
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_activate_order(railPlugin* rail, const RAIL_ACTIVATE_ORDER* activate)
{
	wStream* s;
	UINT error;

	if (!rail || !activate)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_ACTIVATE_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_activate_order(s, activate);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_ACTIVATE);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_sysmenu_order(railPlugin* rail, const RAIL_SYSMENU_ORDER* sysmenu)
{
	wStream* s;
	UINT error;

	if (!rail || !sysmenu)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_SYSMENU_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_sysmenu_order(s, sysmenu);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_SYSMENU);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_syscommand_order(railPlugin* rail, const RAIL_SYSCOMMAND_ORDER* syscommand)
{
	wStream* s;
	UINT error;

	if (!rail || !syscommand)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_SYSCOMMAND_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_syscommand_order(s, syscommand);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_SYSCOMMAND);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_notify_event_order(railPlugin* rail,
        const RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	wStream* s;
	UINT error;

	if (!rail || !notifyEvent)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_NOTIFY_EVENT_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_notify_event_order(s, notifyEvent);

	if (ERROR_SUCCESS == error)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_NOTIFY_EVENT);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_window_move_order(railPlugin* rail, const RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	wStream* s;
	UINT error;

	if (!rail || !windowMove)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_WINDOW_MOVE_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_window_move_order(s, windowMove);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_WINDOWMOVE);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_get_appid_req_order(railPlugin* rail,
        const RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	wStream* s;
	UINT error;

	if (!rail || !getAppIdReq)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_GET_APPID_REQ_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_get_appid_req_order(s, getAppIdReq);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_GET_APPID_REQ);

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_langbar_info_order(railPlugin* rail,
        const RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	wStream* s;
	UINT error;

	if (!rail || !langBarInfo)
		return ERROR_INVALID_PARAMETER;

	if (!rail_is_feature_supported(rail->rdpcontext, RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED))
		return ERROR_BAD_CONFIGURATION;

	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_langbar_info_order(s, langBarInfo);

	if (ERROR_SUCCESS == error)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_LANGBARINFO);

	Stream_Free(s, TRUE);
	return error;
}

UINT rail_send_client_languageime_info_order(railPlugin* rail,
        const RAIL_LANGUAGEIME_INFO_ORDER* langImeInfo)
{
	wStream* s;
	UINT error;

	if (!rail || !langImeInfo)
		return ERROR_INVALID_PARAMETER;

	if (!rail_is_feature_supported(rail->rdpcontext, RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED))
		return ERROR_BAD_CONFIGURATION;

	s = rail_pdu_init(RAIL_LANGUAGEIME_INFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_languageime_info_order(s, langImeInfo);

	if (ERROR_SUCCESS == error)
		error = rail_send_pdu(rail, s, TS_RAIL_ORDER_LANGUAGEIMEINFO);

	Stream_Free(s, TRUE);
	return error;
}

UINT rail_send_client_order_cloak_order(railPlugin* rail, const RAIL_CLOAK* cloak)
{
	wStream* s;
	UINT error;

	if (!rail || !cloak)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(5);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, cloak->windowId);
	Stream_Write_UINT8(s, cloak->cloak ? 1 : 0);
	error = rail_send_pdu(rail, s, TS_RAIL_ORDER_CLOAK);
	Stream_Free(s, TRUE);
	return error;
}

UINT rail_send_client_order_snap_arrange_order(railPlugin* rail, const RAIL_SNAP_ARRANGE* snap)
{
	wStream* s;
	UINT error;

	if (!rail)
		return ERROR_INVALID_PARAMETER;

	/* 2.2.2.7.5 Client Window Snap PDU (TS_RAIL_ORDER_SNAP_ARRANGE) */
	if ((rail->channelFlags & TS_RAIL_ORDER_HANDSHAKE_EX_FLAGS_SNAP_ARRANGE_SUPPORTED) == 0)
	{
		RAIL_WINDOW_MOVE_ORDER move = { 0 };
		move.top = snap->top;
		move.left = snap->left;
		move.right = snap->right;
		move.bottom = snap->bottom;
		move.windowId = snap->windowId;
		return rail_send_client_window_move_order(rail, &move);
	}

	s = rail_pdu_init(12);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, snap->windowId);
	Stream_Write_UINT16(s, snap->left);
	Stream_Write_UINT16(s, snap->top);
	Stream_Write_UINT16(s, snap->right);
	Stream_Write_UINT16(s, snap->bottom);
	error = rail_send_pdu(rail, s, TS_RAIL_ORDER_SNAP_ARRANGE);
	Stream_Free(s, TRUE);
	return error;
}
