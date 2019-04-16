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
	return rail_send_channel_data(rail, Stream_Buffer(s), orderLength);
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
	return rail_read_unicode_string(s,
	                                &execResult->exeOrFile) ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR; /* exeOrFile */
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
		case SPI_SET_SCREEN_SAVE_ACTIVE:
			sysparam->setScreenSaveActive = (body != 0) ? TRUE : FALSE;
			break;

		case SPI_SET_SCREEN_SAVE_SECURE:
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
	Stream_Read(s, (BYTE*) & (getAppidResp->applicationId),
	            512); /* applicationId (256 UNICODE chars) */
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
UINT rail_write_client_sysparam_order(wStream* s, const RAIL_SYSPARAM_ORDER* sysparam)
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_handshake_order(railPlugin* rail, RAIL_HANDSHAKE_ORDER* handshake, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !handshake || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_handshake_order(s, handshake)))
	{
		WLog_ERR(TAG, "rail_read_handshake_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerHandshake, error, context, handshake);

		if (error)
			WLog_ERR(TAG, "context.ServerHandshake failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_handshake_ex_order(railPlugin* rail, RAIL_HANDSHAKE_EX_ORDER* handshakeEx,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !handshakeEx || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_handshake_ex_order(s, handshakeEx)))
	{
		WLog_ERR(TAG, "rail_read_handshake_ex_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerHandshakeEx, error, context, handshakeEx);

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
static UINT rail_recv_exec_result_order(railPlugin* rail, RAIL_EXEC_RESULT_ORDER* execResult,
                                        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !execResult || !s)
		return ERROR_INVALID_PARAMETER;

	ZeroMemory(execResult, sizeof(RAIL_EXEC_RESULT_ORDER));

	if ((error = rail_read_server_exec_result_order(s, execResult)))
	{
		WLog_ERR(TAG, "rail_read_server_exec_result_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerExecuteResult, error, context, execResult);

		if (error)
			WLog_ERR(TAG, "context.ServerExecuteResult failed with error %"PRIu32"", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_server_sysparam_order(railPlugin* rail, RAIL_SYSPARAM_ORDER* sysparam,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !sysparam || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_sysparam_order(s, sysparam)))
	{
		WLog_ERR(TAG, "rail_read_server_sysparam_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerSystemParam, error, context, sysparam);

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
static UINT rail_recv_server_minmaxinfo_order(railPlugin* rail, RAIL_MINMAXINFO_ORDER* minMaxInfo,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !minMaxInfo || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_minmaxinfo_order(s, minMaxInfo)))
	{
		WLog_ERR(TAG, "rail_read_server_minmaxinfo_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerMinMaxInfo, error, context, minMaxInfo);

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
static UINT rail_recv_server_localmovesize_order(railPlugin* rail,
        RAIL_LOCALMOVESIZE_ORDER* localMoveSize,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !localMoveSize || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_localmovesize_order(s, localMoveSize)))
	{
		WLog_ERR(TAG, "rail_read_server_localmovesize_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerLocalMoveSize, error, context, localMoveSize);

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
static UINT rail_recv_server_get_appid_resp_order(railPlugin* rail,
        RAIL_GET_APPID_RESP_ORDER* getAppIdResp, wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !getAppIdResp || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_server_get_appid_resp_order(s, getAppIdResp)))
	{
		WLog_ERR(TAG, "rail_read_server_get_appid_resp_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerGetAppIdResponse, error, context, getAppIdResp);

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
static UINT rail_recv_langbar_info_order(railPlugin* rail, RAIL_LANGBAR_INFO_ORDER* langBarInfo,
        wStream* s)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT error;

	if (!context || !langBarInfo)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_langbar_info_order(s, langBarInfo)))
	{
		WLog_ERR(TAG, "rail_read_langbar_info_order failed with error %"PRIu32"!", error);
		return error;
	}

	if (context->custom)
	{
		IFCALLRET(context->ServerLanguageBarInfo, error, context, langBarInfo);

		if (error)
			WLog_ERR(TAG, "context.ServerLanguageBarInfo failed with error %"PRIu32"", error);
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
		case RDP_RAIL_ORDER_HANDSHAKE:
			{
				RAIL_HANDSHAKE_ORDER handshake;
				return rail_recv_handshake_order(rail, &handshake, s);
			}

		case RDP_RAIL_ORDER_HANDSHAKE_EX:
			{
				RAIL_HANDSHAKE_EX_ORDER handshakeEx;
				return rail_recv_handshake_ex_order(rail, &handshakeEx, s);
			}

		case RDP_RAIL_ORDER_EXEC_RESULT:
			{
				RAIL_EXEC_RESULT_ORDER execResult = { 0 };
				error = rail_recv_exec_result_order(rail, &execResult, s);
				free(execResult.exeOrFile.string);
				return error;
			}

		case RDP_RAIL_ORDER_SYSPARAM:
			{
				RAIL_SYSPARAM_ORDER sysparam;
				return rail_recv_server_sysparam_order(rail, &sysparam, s);
			}

		case RDP_RAIL_ORDER_MINMAXINFO:
			{
				RAIL_MINMAXINFO_ORDER minMaxInfo;
				return rail_recv_server_minmaxinfo_order(rail, &minMaxInfo, s);
			}

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
			{
				RAIL_LOCALMOVESIZE_ORDER localMoveSize;
				return rail_recv_server_localmovesize_order(rail, &localMoveSize, s);
			}

		case RDP_RAIL_ORDER_GET_APPID_RESP:
			{
				RAIL_GET_APPID_RESP_ORDER getAppIdResp;
				return rail_recv_server_get_appid_resp_order(rail, &getAppIdResp, s);
			}

		case RDP_RAIL_ORDER_LANGBARINFO:
			{
				RAIL_LANGBAR_INFO_ORDER langBarInfo;
				return rail_recv_langbar_info_order(rail, &langBarInfo, s);
			}

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
	error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_HANDSHAKE);
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
	error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_HANDSHAKE_EX);
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

	s = rail_pdu_init(RAIL_CLIENT_STATUS_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_client_status_order(s, clientStatus);

	if (error == ERROR_SUCCESS)
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_CLIENTSTATUS);

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

	if ((error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_EXEC)))
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

	if ((error = rail_write_client_sysparam_order(s, sysparam)))
	{
		WLog_ERR(TAG, "rail_write_client_sysparam_order failed with error %"PRIu32"!", error);
		goto out;
	}

	if ((error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSPARAM)))
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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_ACTIVATE);

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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSMENU);

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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSCOMMAND);

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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_NOTIFY_EVENT);

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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_WINDOWMOVE);

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
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_GET_APPID_REQ);

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

	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	error = rail_write_langbar_info_order(s, langBarInfo);

	if (ERROR_SUCCESS == error)
		error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_LANGBARINFO);

	Stream_Free(s, TRUE);
	return error;
}
