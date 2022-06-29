/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
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

#include <freerdp/types.h>
#include <freerdp/constants.h>

#include <freerdp/channels/log.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include "rail_main.h"

#define TAG CHANNELS_TAG("rail.server")

/**
 * Sends a single rail PDU on the channel
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send(RailServerContext* context, wStream* s, ULONG length)
{
	UINT status = CHANNEL_RC_OK;
	ULONG written;

	if (!context)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!WTSVirtualChannelWrite(context->priv->rail_channel, (PCHAR)Stream_Buffer(s), length,
	                            &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		status = ERROR_INTERNAL_ERROR;
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_server_send_pdu(RailServerContext* context, wStream* s, UINT16 orderType)
{
	char buffer[128] = { 0 };
	UINT16 orderLength;

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	orderLength = (UINT16)Stream_GetPosition(s);
	Stream_SetPosition(s, 0);
	rail_write_pdu_header(s, orderType, orderLength);
	Stream_SetPosition(s, orderLength);
	WLog_DBG(TAG, "Sending %s PDU, length: %" PRIu16 "",
	         rail_get_order_type_string_full(orderType, buffer, sizeof(buffer)), orderLength);
	return rail_send(context, s, orderLength);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_local_move_size_order(wStream* s,
                                             const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	if (!s || !localMoveSize)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, localMoveSize->windowId);                /* WindowId (4 bytes) */
	Stream_Write_UINT16(s, localMoveSize->isMoveSizeStart ? 1 : 0); /* IsMoveSizeStart (2 bytes) */
	Stream_Write_UINT16(s, localMoveSize->moveSizeType);            /* MoveSizeType (2 bytes) */
	Stream_Write_UINT16(s, localMoveSize->posX);                    /* PosX (2 bytes) */
	Stream_Write_UINT16(s, localMoveSize->posY);                    /* PosY (2 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_min_max_info_order(wStream* s, const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	if (!s || !minMaxInfo)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, minMaxInfo->windowId);      /* WindowId (4 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxWidth);       /* MaxWidth (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxHeight);      /* MaxHeight (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxPosX);        /* MaxPosX (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxPosY);        /* MaxPosY (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->minTrackWidth);  /* MinTrackWidth (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->minTrackHeight); /* MinTrackHeight (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxTrackWidth);  /* MaxTrackWidth (2 bytes) */
	Stream_Write_INT16(s, minMaxInfo->maxTrackHeight); /* MaxTrackHeight (2 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_taskbar_info_order(wStream* s, const RAIL_TASKBAR_INFO_ORDER* taskbarInfo)
{
	if (!s || !taskbarInfo)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, taskbarInfo->TaskbarMessage); /* TaskbarMessage (4 bytes) */
	Stream_Write_UINT32(s, taskbarInfo->WindowIdTab);    /* WindowIdTab (4 bytes) */
	Stream_Write_UINT32(s, taskbarInfo->Body);           /* Body (4 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_langbar_info_order(wStream* s, const RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	if (!s || !langbarInfo)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, langbarInfo->languageBarStatus); /* LanguageBarStatus (4 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_exec_result_order(wStream* s, const RAIL_EXEC_RESULT_ORDER* execResult)
{
	if (!s || !execResult)
		return ERROR_INVALID_PARAMETER;

	if (execResult->exeOrFile.length > 520 || execResult->exeOrFile.length < 1)
		return ERROR_INVALID_DATA;

	Stream_Write_UINT16(s, execResult->flags);            /* Flags (2 bytes) */
	Stream_Write_UINT16(s, execResult->execResult);       /* ExecResult (2 bytes) */
	Stream_Write_UINT32(s, execResult->rawResult);        /* RawResult (4 bytes) */
	Stream_Write_UINT16(s, 0);                            /* Padding (2 bytes) */
	Stream_Write_UINT16(s, execResult->exeOrFile.length); /* ExeOrFileLength (2 bytes) */
	Stream_Write(s, execResult->exeOrFile.string,
	             execResult->exeOrFile.length); /* ExeOrFile (variable) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_z_order_sync_order(wStream* s, const RAIL_ZORDER_SYNC* zOrderSync)
{
	if (!s || !zOrderSync)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, zOrderSync->windowIdMarker); /* WindowIdMarker (4 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_cloak_order(wStream* s, const RAIL_CLOAK* cloak)
{
	if (!s || !cloak)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, cloak->windowId);     /* WindowId (4 bytes) */
	Stream_Write_UINT8(s, cloak->cloak ? 1 : 0); /* Cloaked (1 byte) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
rail_write_power_display_request_order(wStream* s,
                                       const RAIL_POWER_DISPLAY_REQUEST* powerDisplayRequest)
{
	if (!s || !powerDisplayRequest)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, powerDisplayRequest->active ? 1 : 0); /* Active (4 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_get_app_id_resp_order(wStream* s,
                                             const RAIL_GET_APPID_RESP_ORDER* getAppidResp)
{
	if (!s || !getAppidResp)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, getAppidResp->windowId); /* WindowId (4 bytes) */
	Stream_Write_UTF16_String(
	    s, getAppidResp->applicationId,
	    ARRAYSIZE(getAppidResp->applicationId)); /* ApplicationId (512 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_write_get_appid_resp_ex_order(wStream* s,
                                               const RAIL_GET_APPID_RESP_EX* getAppidRespEx)
{
	if (!s || !getAppidRespEx)
		return ERROR_INVALID_PARAMETER;

	Stream_Write_UINT32(s, getAppidRespEx->windowID); /* WindowId (4 bytes) */
	Stream_Write_UTF16_String(
	    s, getAppidRespEx->applicationID,
	    ARRAYSIZE(getAppidRespEx->applicationID));     /* ApplicationId (520 bytes) */
	Stream_Write_UINT32(s, getAppidRespEx->processId); /* ProcessId (4 bytes) */
	Stream_Write_UTF16_String(
	    s, getAppidRespEx->processImageName,
	    ARRAYSIZE(getAppidRespEx->processImageName)); /* ProcessImageName (520 bytes) */
	return ERROR_SUCCESS;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_handshake(RailServerContext* context,
                                       const RAIL_HANDSHAKE_ORDER* handshake)
{
	wStream* s;
	UINT error;

	if (!context || !handshake)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_HANDSHAKE_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_handshake_order(s, handshake);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_HANDSHAKE);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_handshake_ex(RailServerContext* context,
                                          const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	wStream* s;
	UINT error;

	if (!context || !handshakeEx || !context->priv)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_HANDSHAKE_EX_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_server_set_handshake_ex_flags(context, handshakeEx->railHandshakeFlags);

	rail_write_handshake_ex_order(s, handshakeEx);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_HANDSHAKE_EX);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_sysparam(RailServerContext* context,
                                      const RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	UINT error;
	RailServerPrivate* priv;
	BOOL extendedSpiSupported;

	if (!context || !sysparam)
		return ERROR_INVALID_PARAMETER;

	priv = context->priv;

	if (!priv)
		return ERROR_INVALID_PARAMETER;

	extendedSpiSupported = rail_is_extended_spi_supported(context->priv->channelFlags);
	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_sysparam_order(s, sysparam, extendedSpiSupported);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_SYSPARAM);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_local_move_size(RailServerContext* context,
                                             const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	wStream* s;
	UINT error;

	if (!context || !localMoveSize)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_LOCALMOVESIZE_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_local_move_size_order(s, localMoveSize);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_LOCALMOVESIZE);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_min_max_info(RailServerContext* context,
                                          const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	wStream* s;
	UINT error;

	if (!context || !minMaxInfo)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_MINMAXINFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_min_max_info_order(s, minMaxInfo);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_MINMAXINFO);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_taskbar_info(RailServerContext* context,
                                          const RAIL_TASKBAR_INFO_ORDER* taskbarInfo)
{
	wStream* s;
	UINT error;

	if (!context || !taskbarInfo)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_TASKBAR_INFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_taskbar_info_order(s, taskbarInfo);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_TASKBARINFO);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_langbar_info(RailServerContext* context,
                                          const RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	wStream* s;
	UINT error;

	if (!context || !langbarInfo)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_LANGBAR_INFO_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_langbar_info_order(s, langbarInfo);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_LANGBARINFO);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_exec_result(RailServerContext* context,
                                         const RAIL_EXEC_RESULT_ORDER* execResult)
{
	wStream* s;
	UINT error;

	if (!context || !execResult)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_EXEC_RESULT_ORDER_LENGTH + execResult->exeOrFile.length);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_exec_result_order(s, execResult);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_EXEC_RESULT);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_z_order_sync(RailServerContext* context,
                                          const RAIL_ZORDER_SYNC* zOrderSync)
{
	wStream* s;
	UINT error;

	if (!context || !zOrderSync)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_Z_ORDER_SYNC_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_z_order_sync_order(s, zOrderSync);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_ZORDER_SYNC);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_cloak(RailServerContext* context, const RAIL_CLOAK* cloak)
{
	wStream* s;
	UINT error;

	if (!context || !cloak)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_CLOAK_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_cloak_order(s, cloak);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_CLOAK);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
rail_send_server_power_display_request(RailServerContext* context,
                                       const RAIL_POWER_DISPLAY_REQUEST* powerDisplayRequest)
{
	wStream* s;
	UINT error;

	if (!context || !powerDisplayRequest)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_POWER_DISPLAY_REQUEST_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_power_display_request_order(s, powerDisplayRequest);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_POWER_DISPLAY_REQUEST);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error coie
 */
static UINT rail_send_server_get_app_id_resp(RailServerContext* context,
                                             const RAIL_GET_APPID_RESP_ORDER* getAppidResp)
{
	wStream* s;
	UINT error;

	if (!context || !getAppidResp)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_GET_APPID_RESP_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_get_app_id_resp_order(s, getAppidResp);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_GET_APPID_RESP);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_server_get_appid_resp_ex(RailServerContext* context,
                                               const RAIL_GET_APPID_RESP_EX* getAppidRespEx)
{
	wStream* s;
	UINT error;

	if (!context || !getAppidRespEx)
		return ERROR_INVALID_PARAMETER;

	s = rail_pdu_init(RAIL_GET_APPID_RESP_EX_ORDER_LENGTH);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rail_write_get_appid_resp_ex_order(s, getAppidRespEx);
	error = rail_server_send_pdu(context, s, TS_RAIL_ORDER_GET_APPID_RESP_EX);
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_client_status_order(wStream* s, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_CLIENT_STATUS_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, clientStatus->flags); /* Flags (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_exec_order(wStream* s, RAIL_EXEC_ORDER* exec)
{
	RAIL_EXEC_ORDER order = { 0 };
	UINT16 exeLen, workLen, argLen;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_EXEC_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, exec->flags); /* Flags (2 bytes) */
	Stream_Read_UINT16(s, exeLen);      /* ExeOrFileLength (2 bytes) */
	Stream_Read_UINT16(s, workLen);     /* WorkingDirLength (2 bytes) */
	Stream_Read_UINT16(s, argLen);      /* ArgumentsLength (2 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, (size_t)exeLen + workLen + argLen))
		return ERROR_INVALID_DATA;

	{
		const int len = exeLen / sizeof(WCHAR);
		int rc;
		const WCHAR* str = (const WCHAR*)Stream_Pointer(s);
		rc = ConvertFromUnicode(CP_UTF8, 0, str, len, &exec->RemoteApplicationProgram, 0, NULL,
		                        NULL);
		if (rc != len)
			goto fail;
		Stream_Seek(s, exeLen);
	}
	{
		const int len = workLen / sizeof(WCHAR);
		int rc;

		const WCHAR* str = (const WCHAR*)Stream_Pointer(s);
		rc = ConvertFromUnicode(CP_UTF8, 0, str, len, &exec->RemoteApplicationProgram, 0, NULL,
		                        NULL);
		if (rc != len)
			goto fail;
		Stream_Seek(s, workLen);
	}
	{
		const int len = argLen / sizeof(WCHAR);
		int rc;
		const WCHAR* str = (const WCHAR*)Stream_Pointer(s);
		rc = ConvertFromUnicode(CP_UTF8, 0, str, len, &exec->RemoteApplicationProgram, 0, NULL,
		                        NULL);
		if (rc != len)
			goto fail;
		Stream_Seek(s, argLen);
	}

	return CHANNEL_RC_OK;
fail:
	free(exec->RemoteApplicationProgram);
	free(exec->RemoteApplicationArguments);
	free(exec->RemoteApplicationWorkingDir);
	*exec = order;
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_activate_order(wStream* s, RAIL_ACTIVATE_ORDER* activate)
{
	BYTE enabled;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_ACTIVATE_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, activate->windowId); /* WindowId (4 bytes) */
	Stream_Read_UINT8(s, enabled);             /* Enabled (1 byte) */
	activate->enabled = (enabled != 0) ? TRUE : FALSE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_sysmenu_order(wStream* s, RAIL_SYSMENU_ORDER* sysmenu)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_SYSMENU_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, sysmenu->windowId); /* WindowId (4 bytes) */
	Stream_Read_INT16(s, sysmenu->left);      /* Left (2 bytes) */
	Stream_Read_INT16(s, sysmenu->top);       /* Top (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_syscommand_order(wStream* s, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_SYSCOMMAND_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, syscommand->windowId); /* WindowId (4 bytes) */
	Stream_Read_UINT16(s, syscommand->command);  /* Command (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_notify_event_order(wStream* s, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_NOTIFY_EVENT_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, notifyEvent->windowId);     /* WindowId (4 bytes) */
	Stream_Read_UINT32(s, notifyEvent->notifyIconId); /* NotifyIconId (4 bytes) */
	Stream_Read_UINT32(s, notifyEvent->message);      /* Message (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_get_appid_req_order(wStream* s, RAIL_GET_APPID_REQ_ORDER* getAppidReq)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_GET_APPID_REQ_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, getAppidReq->windowId); /* WindowId (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_window_move_order(wStream* s, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_WINDOW_MOVE_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, windowMove->windowId); /* WindowId (4 bytes) */
	Stream_Read_INT16(s, windowMove->left);      /* Left (2 bytes) */
	Stream_Read_INT16(s, windowMove->top);       /* Top (2 bytes) */
	Stream_Read_INT16(s, windowMove->right);     /* Right (2 bytes) */
	Stream_Read_INT16(s, windowMove->bottom);    /* Bottom (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_snap_arange_order(wStream* s, RAIL_SNAP_ARRANGE* snapArrange)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_SNAP_ARRANGE_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, snapArrange->windowId); /* WindowId (4 bytes) */
	Stream_Read_INT16(s, snapArrange->left);      /* Left (2 bytes) */
	Stream_Read_INT16(s, snapArrange->top);       /* Top (2 bytes) */
	Stream_Read_INT16(s, snapArrange->right);     /* Right (2 bytes) */
	Stream_Read_INT16(s, snapArrange->bottom);    /* Bottom (2 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_langbar_info_order(wStream* s, RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_LANGBAR_INFO_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, langbarInfo->languageBarStatus); /* LanguageBarStatus (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_language_ime_info_order(wStream* s,
                                              RAIL_LANGUAGEIME_INFO_ORDER* languageImeInfo)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_LANGUAGEIME_INFO_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, languageImeInfo->ProfileType); /* ProfileType (4 bytes) */
	Stream_Read_UINT16(s, languageImeInfo->LanguageID);  /* LanguageID (2 bytes) */
	Stream_Read(
	    s, &languageImeInfo->LanguageProfileCLSID,
	    sizeof(languageImeInfo->LanguageProfileCLSID)); /* LanguageProfileCLSID (16 bytes) */
	Stream_Read(s, &languageImeInfo->ProfileGUID,
	            sizeof(languageImeInfo->ProfileGUID));      /* ProfileGUID (16 bytes) */
	Stream_Read_UINT32(s, languageImeInfo->KeyboardLayout); /* KeyboardLayout (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_compartment_info_order(wStream* s,
                                             RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_COMPARTMENT_INFO_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, compartmentInfo->ImeState);        /* ImeState (4 bytes) */
	Stream_Read_UINT32(s, compartmentInfo->ImeConvMode);     /* ImeConvMode (4 bytes) */
	Stream_Read_UINT32(s, compartmentInfo->ImeSentenceMode); /* ImeSentenceMode (4 bytes) */
	Stream_Read_UINT32(s, compartmentInfo->KanaMode);        /* KANAMode (4 bytes) */
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_read_cloak_order(wStream* s, RAIL_CLOAK* cloak)
{
	BYTE cloaked;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, RAIL_CLOAK_ORDER_LENGTH))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cloak->windowId); /* WindowId (4 bytes) */
	Stream_Read_UINT8(s, cloaked);          /* Cloaked (1 byte) */
	cloak->cloak = (cloaked != 0) ? TRUE : FALSE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_handshake_order(RailServerContext* context,
                                             RAIL_HANDSHAKE_ORDER* handshake, wStream* s)
{
	UINT error;

	if (!context || !handshake || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_handshake_order(s, handshake)))
	{
		WLog_ERR(TAG, "rail_read_handshake_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientHandshake, error, context, handshake);

	if (error)
		WLog_ERR(TAG, "context.ClientHandshake failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_client_status_order(RailServerContext* context,
                                                 RAIL_CLIENT_STATUS_ORDER* clientStatus, wStream* s)
{
	UINT error;

	if (!context || !clientStatus || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_client_status_order(s, clientStatus)))
	{
		WLog_ERR(TAG, "rail_read_client_status_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientClientStatus, error, context, clientStatus);

	if (error)
		WLog_ERR(TAG, "context.ClientClientStatus failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_exec_order(RailServerContext* context, wStream* s)
{
	UINT error;
	RAIL_EXEC_ORDER exec = { 0 };

	if (!context || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_exec_order(s, &exec)))
	{
		WLog_ERR(TAG, "rail_read_client_status_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientExec, error, context, &exec);

	if (error)
		WLog_ERR(TAG, "context.Exec failed with error %" PRIu32 "", error);

	free(exec.RemoteApplicationProgram);
	free(exec.RemoteApplicationArguments);
	free(exec.RemoteApplicationWorkingDir);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_sysparam_order(RailServerContext* context,
                                            RAIL_SYSPARAM_ORDER* sysparam, wStream* s)
{
	UINT error;
	BOOL extendedSpiSupported;

	if (!context || !sysparam || !s)
		return ERROR_INVALID_PARAMETER;

	extendedSpiSupported = rail_is_extended_spi_supported(context->priv->channelFlags);
	if ((error = rail_read_sysparam_order(s, sysparam, extendedSpiSupported)))
	{
		WLog_ERR(TAG, "rail_read_sysparam_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientSysparam, error, context, sysparam);

	if (error)
		WLog_ERR(TAG, "context.ClientSysparam failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_activate_order(RailServerContext* context,
                                            RAIL_ACTIVATE_ORDER* activate, wStream* s)
{
	UINT error;

	if (!context || !activate || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_activate_order(s, activate)))
	{
		WLog_ERR(TAG, "rail_read_activate_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientActivate, error, context, activate);

	if (error)
		WLog_ERR(TAG, "context.ClientActivate failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_sysmenu_order(RailServerContext* context, RAIL_SYSMENU_ORDER* sysmenu,
                                           wStream* s)
{
	UINT error;

	if (!context || !sysmenu || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_sysmenu_order(s, sysmenu)))
	{
		WLog_ERR(TAG, "rail_read_sysmenu_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientSysmenu, error, context, sysmenu);

	if (error)
		WLog_ERR(TAG, "context.ClientSysmenu failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_syscommand_order(RailServerContext* context,
                                              RAIL_SYSCOMMAND_ORDER* syscommand, wStream* s)
{
	UINT error;

	if (!context || !syscommand || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_syscommand_order(s, syscommand)))
	{
		WLog_ERR(TAG, "rail_read_syscommand_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientSyscommand, error, context, syscommand);

	if (error)
		WLog_ERR(TAG, "context.ClientSyscommand failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_notify_event_order(RailServerContext* context,
                                                RAIL_NOTIFY_EVENT_ORDER* notifyEvent, wStream* s)
{
	UINT error;

	if (!context || !notifyEvent || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_notify_event_order(s, notifyEvent)))
	{
		WLog_ERR(TAG, "rail_read_notify_event_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientNotifyEvent, error, context, notifyEvent);

	if (error)
		WLog_ERR(TAG, "context.ClientNotifyEvent failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_window_move_order(RailServerContext* context,
                                               RAIL_WINDOW_MOVE_ORDER* windowMove, wStream* s)
{
	UINT error;

	if (!context || !windowMove || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_window_move_order(s, windowMove)))
	{
		WLog_ERR(TAG, "rail_read_window_move_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientWindowMove, error, context, windowMove);

	if (error)
		WLog_ERR(TAG, "context.ClientWindowMove failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_snap_arrange_order(RailServerContext* context,
                                                RAIL_SNAP_ARRANGE* snapArrange, wStream* s)
{
	UINT error;

	if (!context || !snapArrange || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_snap_arange_order(s, snapArrange)))
	{
		WLog_ERR(TAG, "rail_read_snap_arange_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientSnapArrange, error, context, snapArrange);

	if (error)
		WLog_ERR(TAG, "context.ClientSnapArrange failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_get_appid_req_order(RailServerContext* context,
                                                 RAIL_GET_APPID_REQ_ORDER* getAppidReq, wStream* s)
{
	UINT error;

	if (!context || !getAppidReq || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_get_appid_req_order(s, getAppidReq)))
	{
		WLog_ERR(TAG, "rail_read_get_appid_req_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientGetAppidReq, error, context, getAppidReq);

	if (error)
		WLog_ERR(TAG, "context.ClientGetAppidReq failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_langbar_info_order(RailServerContext* context,
                                                RAIL_LANGBAR_INFO_ORDER* langbarInfo, wStream* s)
{
	UINT error;

	if (!context || !langbarInfo || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_langbar_info_order(s, langbarInfo)))
	{
		WLog_ERR(TAG, "rail_read_langbar_info_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientLangbarInfo, error, context, langbarInfo);

	if (error)
		WLog_ERR(TAG, "context.ClientLangbarInfo failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_language_ime_info_order(RailServerContext* context,
                                                     RAIL_LANGUAGEIME_INFO_ORDER* languageImeInfo,
                                                     wStream* s)
{
	UINT error;

	if (!context || !languageImeInfo || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_language_ime_info_order(s, languageImeInfo)))
	{
		WLog_ERR(TAG, "rail_read_language_ime_info_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientLanguageImeInfo, error, context, languageImeInfo);

	if (error)
		WLog_ERR(TAG, "context.ClientLanguageImeInfo failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_compartment_info(RailServerContext* context,
                                              RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo,
                                              wStream* s)
{
	UINT error;

	if (!context || !compartmentInfo || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_compartment_info_order(s, compartmentInfo)))
	{
		WLog_ERR(TAG, "rail_read_compartment_info_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientCompartmentInfo, error, context, compartmentInfo);

	if (error)
		WLog_ERR(TAG, "context.ClientCompartmentInfo failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_recv_client_cloak_order(RailServerContext* context, RAIL_CLOAK* cloak, wStream* s)
{
	UINT error;

	if (!context || !cloak || !s)
		return ERROR_INVALID_PARAMETER;

	if ((error = rail_read_cloak_order(s, cloak)))
	{
		WLog_ERR(TAG, "rail_read_cloak_order failed with error %" PRIu32 "!", error);
		return error;
	}

	IFCALLRET(context->ClientCloak, error, context, cloak);

	if (error)
		WLog_ERR(TAG, "context.Cloak failed with error %" PRIu32 "", error);

	return error;
}

static DWORD WINAPI rail_server_thread(LPVOID arg)
{
	RailServerContext* context = (RailServerContext*)arg;
	RailServerPrivate* priv = context->priv;
	DWORD status;
	DWORD nCount = 0;
	HANDLE events[8];
	UINT error = CHANNEL_RC_OK;
	events[nCount++] = priv->channelEvent;
	events[nCount++] = priv->stopEvent;

	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		status = WaitForSingleObject(context->priv->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(context->priv->channelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(
			    TAG,
			    "WaitForSingleObject(context->priv->channelEvent, 0) failed with error %" PRIu32
			    "!",
			    error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			if ((error = rail_server_handle_messages(context)))
			{
				WLog_ERR(TAG, "rail_server_handle_messages failed with error %" PRIu32 "", error);
				break;
			}
		}
	}

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "rail_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_server_start(RailServerContext* context)
{
	void* buffer = NULL;
	DWORD bytesReturned;
	RailServerPrivate* priv = context->priv;
	UINT error = ERROR_INTERNAL_ERROR;
	priv->rail_channel =
	    WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, RAIL_SVC_CHANNEL_NAME);

	if (!priv->rail_channel)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen failed!");
		return error;
	}

	if (!WTSVirtualChannelQuery(priv->rail_channel, WTSVirtualEventHandle, &buffer,
	                            &bytesReturned) ||
	    (bytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "error during WTSVirtualChannelQuery(WTSVirtualEventHandle) or invalid returned "
		         "size(%" PRIu32 ")",
		         bytesReturned);

		if (buffer)
			WTSFreeMemory(buffer);

		goto out_close;
	}

	CopyMemory(&priv->channelEvent, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);
	context->priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!context->priv->stopEvent)
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto out_close;
	}

	context->priv->thread = CreateThread(NULL, 0, rail_server_thread, (void*)context, 0, NULL);

	if (!context->priv->thread)
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto out_stop_event;
	}

	return CHANNEL_RC_OK;
out_stop_event:
	CloseHandle(context->priv->stopEvent);
	context->priv->stopEvent = NULL;
out_close:
	WTSVirtualChannelClose(context->priv->rail_channel);
	context->priv->rail_channel = NULL;
	return error;
}

static BOOL rail_server_stop(RailServerContext* context)
{
	RailServerPrivate* priv = (RailServerPrivate*)context->priv;

	if (priv->thread)
	{
		SetEvent(priv->stopEvent);

		if (WaitForSingleObject(priv->thread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", GetLastError());
			return FALSE;
		}

		CloseHandle(priv->thread);
		CloseHandle(priv->stopEvent);
		priv->thread = NULL;
		priv->stopEvent = NULL;
	}

	if (priv->rail_channel)
	{
		WTSVirtualChannelClose(priv->rail_channel);
		priv->rail_channel = NULL;
	}

	priv->channelEvent = NULL;
	return TRUE;
}

RailServerContext* rail_server_context_new(HANDLE vcm)
{
	RailServerContext* context;
	RailServerPrivate* priv;
	context = (RailServerContext*)calloc(1, sizeof(RailServerContext));

	if (!context)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	context->vcm = vcm;
	context->Start = rail_server_start;
	context->Stop = rail_server_stop;
	context->ServerHandshake = rail_send_server_handshake;
	context->ServerHandshakeEx = rail_send_server_handshake_ex;
	context->ServerSysparam = rail_send_server_sysparam;
	context->ServerLocalMoveSize = rail_send_server_local_move_size;
	context->ServerMinMaxInfo = rail_send_server_min_max_info;
	context->ServerTaskbarInfo = rail_send_server_taskbar_info;
	context->ServerLangbarInfo = rail_send_server_langbar_info;
	context->ServerExecResult = rail_send_server_exec_result;
	context->ServerGetAppidResp = rail_send_server_get_app_id_resp;
	context->ServerZOrderSync = rail_send_server_z_order_sync;
	context->ServerCloak = rail_send_server_cloak;
	context->ServerPowerDisplayRequest = rail_send_server_power_display_request;
	context->ServerGetAppidRespEx = rail_send_server_get_appid_resp_ex;
	context->priv = priv = (RailServerPrivate*)calloc(1, sizeof(RailServerPrivate));

	if (!priv)
	{
		WLog_ERR(TAG, "calloc failed!");
		goto out_free;
	}

	/* Create shared input stream */
	priv->input_stream = Stream_New(NULL, 4096);

	if (!priv->input_stream)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out_free_priv;
	}

	return context;
out_free_priv:
	free(context->priv);
out_free:
	free(context);
	return NULL;
}

void rail_server_context_free(RailServerContext* context)
{
	if (context->priv)
		Stream_Free(context->priv->input_stream, TRUE);

	free(context->priv);
	free(context);
}

void rail_server_set_handshake_ex_flags(RailServerContext* context, DWORD flags)
{
	RailServerPrivate* priv;

	if (!context || !context->priv)
		return;

	priv = context->priv;
	priv->channelFlags = flags;
}

UINT rail_server_handle_messages(RailServerContext* context)
{
	char buffer[128] = { 0 };
	UINT status = CHANNEL_RC_OK;
	DWORD bytesReturned;
	UINT16 orderType;
	UINT16 orderLength;
	RailServerPrivate* priv = context->priv;
	wStream* s = priv->input_stream;

	/* Read header */
	if (!Stream_EnsureRemainingCapacity(s, RAIL_PDU_HEADER_LENGTH))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed, RAIL_PDU_HEADER_LENGTH");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!WTSVirtualChannelRead(priv->rail_channel, 0, (PCHAR)Stream_Pointer(s),
	                           RAIL_PDU_HEADER_LENGTH, &bytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return ERROR_NO_DATA;

		WLog_ERR(TAG, "channel connection closed");
		return ERROR_INTERNAL_ERROR;
	}

	/* Parse header */
	if ((status = rail_read_pdu_header(s, &orderType, &orderLength)) != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "rail_read_pdu_header failed with error %" PRIu32 "!", status);
		return status;
	}

	if (!Stream_EnsureRemainingCapacity(s, orderLength - RAIL_PDU_HEADER_LENGTH))
	{
		WLog_ERR(TAG,
		         "Stream_EnsureRemainingCapacity failed, orderLength - RAIL_PDU_HEADER_LENGTH");
		return CHANNEL_RC_NO_MEMORY;
	}

	/* Read body */
	if (!WTSVirtualChannelRead(priv->rail_channel, 0, (PCHAR)Stream_Pointer(s),
	                           orderLength - RAIL_PDU_HEADER_LENGTH, &bytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return ERROR_NO_DATA;

		WLog_ERR(TAG, "channel connection closed");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "Received %s PDU, length:%" PRIu16 "",
	         rail_get_order_type_string_full(orderType, buffer, sizeof(buffer)), orderLength);

	switch (orderType)
	{
		case TS_RAIL_ORDER_HANDSHAKE:
		{
			RAIL_HANDSHAKE_ORDER handshake;
			return rail_recv_client_handshake_order(context, &handshake, s);
		}

		case TS_RAIL_ORDER_CLIENTSTATUS:
		{
			RAIL_CLIENT_STATUS_ORDER clientStatus;
			return rail_recv_client_client_status_order(context, &clientStatus, s);
		}

		case TS_RAIL_ORDER_EXEC:
			return rail_recv_client_exec_order(context, s);

		case TS_RAIL_ORDER_SYSPARAM:
		{
			RAIL_SYSPARAM_ORDER sysparam = { 0 };
			return rail_recv_client_sysparam_order(context, &sysparam, s);
		}

		case TS_RAIL_ORDER_ACTIVATE:
		{
			RAIL_ACTIVATE_ORDER activate;
			return rail_recv_client_activate_order(context, &activate, s);
		}

		case TS_RAIL_ORDER_SYSMENU:
		{
			RAIL_SYSMENU_ORDER sysmenu;
			return rail_recv_client_sysmenu_order(context, &sysmenu, s);
		}

		case TS_RAIL_ORDER_SYSCOMMAND:
		{
			RAIL_SYSCOMMAND_ORDER syscommand;
			return rail_recv_client_syscommand_order(context, &syscommand, s);
		}

		case TS_RAIL_ORDER_NOTIFY_EVENT:
		{
			RAIL_NOTIFY_EVENT_ORDER notifyEvent;
			return rail_recv_client_notify_event_order(context, &notifyEvent, s);
		}

		case TS_RAIL_ORDER_WINDOWMOVE:
		{
			RAIL_WINDOW_MOVE_ORDER windowMove;
			return rail_recv_client_window_move_order(context, &windowMove, s);
		}

		case TS_RAIL_ORDER_SNAP_ARRANGE:
		{
			RAIL_SNAP_ARRANGE snapArrange;
			return rail_recv_client_snap_arrange_order(context, &snapArrange, s);
		}

		case TS_RAIL_ORDER_GET_APPID_REQ:
		{
			RAIL_GET_APPID_REQ_ORDER getAppidReq;
			return rail_recv_client_get_appid_req_order(context, &getAppidReq, s);
		}

		case TS_RAIL_ORDER_LANGBARINFO:
		{
			RAIL_LANGBAR_INFO_ORDER langbarInfo;
			return rail_recv_client_langbar_info_order(context, &langbarInfo, s);
		}

		case TS_RAIL_ORDER_LANGUAGEIMEINFO:
		{
			RAIL_LANGUAGEIME_INFO_ORDER languageImeInfo;
			return rail_recv_client_language_ime_info_order(context, &languageImeInfo, s);
		}

		case TS_RAIL_ORDER_COMPARTMENTINFO:
		{
			RAIL_COMPARTMENT_INFO_ORDER compartmentInfo;
			return rail_recv_client_compartment_info(context, &compartmentInfo, s);
		}

		case TS_RAIL_ORDER_CLOAK:
		{
			RAIL_CLOAK cloak;
			return rail_recv_client_cloak_order(context, &cloak, s);
		}

		default:
			WLog_ERR(TAG, "Unknown RAIL PDU order received.");
			return ERROR_INVALID_DATA;
	}
}
