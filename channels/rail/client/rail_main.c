/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>

#include "rail_orders.h"
#include "rail_main.h"

RailClientContext* rail_get_client_interface(railPlugin* rail)
{
	RailClientContext* pInterface;
	pInterface = (RailClientContext*) rail->channelEntryPoints.pInterface;
	return pInterface;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send(railPlugin* rail, wStream* s)
{
	UINT status;

	if (!rail)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = rail->channelEntryPoints.pVirtualChannelWrite(rail->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_channel_data(railPlugin* rail, void* data, size_t length)
{
	wStream* s = NULL;

	s = Stream_New(NULL, length);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	Stream_Write(s, data, length);

	return rail_send(rail, s);
}

/**
 * Callback Interface
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_execute(RailClientContext* context, RAIL_EXEC_ORDER* exec)
{
	char* exeOrFile;
	railPlugin* rail = (railPlugin*) context->handle;

	exeOrFile = exec->RemoteApplicationProgram;

	if (!exeOrFile)
		return ERROR_INVALID_PARAMETER;

	if (strlen(exeOrFile) >= 2)
	{
		if (strncmp(exeOrFile, "||", 2) != 0)
			exec->flags |= RAIL_EXEC_FLAG_FILE;
	}

	rail_string_to_unicode_string(exec->RemoteApplicationProgram, &exec->exeOrFile); /* RemoteApplicationProgram */
	rail_string_to_unicode_string(exec->RemoteApplicationWorkingDir, &exec->workingDir); /* ShellWorkingDirectory */
	rail_string_to_unicode_string(exec->RemoteApplicationArguments, &exec->arguments); /* RemoteApplicationCmdLine */

	return rail_send_client_exec_order(rail, exec);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_activate(RailClientContext* context, RAIL_ACTIVATE_ORDER* activate)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_activate_order(rail, activate);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_client_sysparam(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	int length;
	railPlugin* rail = (railPlugin*) context->handle;
	UINT error;

	length = RAIL_SYSPARAM_ORDER_LENGTH;

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
	}

	s = rail_pdu_init(RAIL_SYSPARAM_ORDER_LENGTH + 8);
	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rail_write_client_sysparam_order(s, sysparam)))
	{
		WLog_ERR(TAG, "rail_write_client_sysparam_order failed with error %lu!", error);
		Stream_Free(s, TRUE);
		return error;
	}

	if ((error = rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSPARAM)))
	{
		WLog_ERR(TAG, "rail_send_pdu failed with error %lu!", error);
	}

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	UINT error = CHANNEL_RC_OK;

	if (sysparam->params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		sysparam->param = SPI_SET_HIGH_CONTRAST;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_TASKBAR_POS)
	{
		sysparam->param = SPI_TASKBAR_POS;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		sysparam->param = SPI_SET_MOUSE_BUTTON_SWAP;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		sysparam->param = SPI_SET_KEYBOARD_PREF;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		sysparam->param = SPI_SET_DRAG_FULL_WINDOWS;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		sysparam->param = SPI_SET_KEYBOARD_CUES;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
			return error;
		}
	}

	if (sysparam->params & SPI_MASK_SET_WORK_AREA)
	{
		sysparam->param = SPI_SET_WORK_AREA;
		if ((error = rail_send_client_sysparam(context, sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %lu!", error);
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
UINT rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_system_command(RailClientContext* context, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_syscommand_order(rail, syscommand);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_handshake_order(rail, handshake);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_handshake_ex_order(rail, handshakeEx);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_notify_event(RailClientContext* context, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_notify_event_order(rail, notifyEvent);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_window_move(RailClientContext* context, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_window_move_order(rail, windowMove);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_information(RailClientContext* context, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_status_order(rail, clientStatus);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_system_menu(RailClientContext* context, RAIL_SYSMENU_ORDER* sysmenu)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_sysmenu_order(rail, sysmenu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_langbar_info_order(rail, langBarInfo);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_client_get_appid_request(RailClientContext* context, RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	railPlugin* rail = (railPlugin*) context->handle;

	return rail_send_client_get_appid_req_order(rail, getAppIdReq);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return CHANNEL_RC_OK; /* stub - should be registered by client */
}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
	{
		g_InitHandles = ListDictionary_New(TRUE);
	}
	if (!g_InitHandles)
	{
		WLog_ERR(TAG, "ListDictionary_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!ListDictionary_Add(g_InitHandles, pInitHandle, pUserData))
	{
		WLog_ERR(TAG, "ListDictionary_Add failed!");
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

void* rail_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void rail_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
	{
		g_OpenHandles = ListDictionary_New(TRUE);
	}
	if (!g_OpenHandles)
	{
		WLog_ERR(TAG, "ListDictionary_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData))
	{
		WLog_ERR(TAG, "ListDictionary_Add failed!");
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

void* rail_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void rail_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_virtual_channel_event_data_received(railPlugin* rail,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (rail->data_in)
			Stream_Free(rail->data_in, TRUE);

		rail->data_in = Stream_New(NULL, totalLength);
		if (!rail->data_in)
		{
			WLog_ERR(TAG, "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	data_in = rail->data_in;
	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "rail_plugin_process_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		rail->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(rail->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE rail_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	railPlugin* rail;
	UINT error = CHANNEL_RC_OK;

	rail = (railPlugin*) rail_get_open_handle_data(openHandle);

	if (!rail)
	{
		WLog_ERR(TAG, "rail_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = rail_virtual_channel_event_data_received(rail, pData, dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG, "rail_virtual_channel_event_data_received failed with error %lu!", error);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error, "rail_virtual_channel_open_event reported an error");

	return;
}

static void* rail_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	railPlugin* rail = (railPlugin*) arg;
	UINT error = CHANNEL_RC_OK;

	while (1)
	{
		if (!MessageQueue_Wait(rail->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(rail->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}
		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;
			if ((error = rail_order_recv(rail, data)))
			{
				WLog_ERR(TAG, "rail_order_recv failed with error %d!", error);
				break;
			}
		}
	}

	if (error && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error, "rail_virtual_channel_client_thread reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_virtual_channel_event_connected(railPlugin* rail, LPVOID pData, UINT32 dataLength)
{
	UINT status;

	status = rail->channelEntryPoints.pVirtualChannelOpen(rail->InitHandle,
		&rail->OpenHandle, rail->channelDef.name, rail_virtual_channel_open_event);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return status;
	}

	if ((status = rail_add_open_handle_data(rail->OpenHandle, rail)))
	{
		WLog_ERR(TAG, "rail_add_open_handle_data failed with error %lu!", status);
		return status;
	}

	rail->queue = MessageQueue_New(NULL);
	if (!rail->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(rail->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rail_virtual_channel_client_thread, (void*) rail, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		MessageQueue_Free(rail->queue);
		rail->queue = NULL;
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_virtual_channel_event_disconnected(railPlugin* rail)
{
	UINT rc;
	if (MessageQueue_PostQuit(rail->queue, 0) && (WaitForSingleObject(rail->thread, INFINITE) == WAIT_FAILED))
    {
	rc = GetLastError();
	WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", rc);
	return rc;
    }

	MessageQueue_Free(rail->queue);
	CloseHandle(rail->thread);

	rail->queue = NULL;
	rail->thread = NULL;

	rc = rail->channelEntryPoints.pVirtualChannelClose(rail->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	return rc;
	}

	if (rail->data_in)
	{
		Stream_Free(rail->data_in, TRUE);
		rail->data_in = NULL;
	}

	rail_remove_open_handle_data(rail->OpenHandle);
    return CHANNEL_RC_OK;
}

static void rail_virtual_channel_event_terminated(railPlugin* rail)
{
	rail_remove_init_handle_data(rail->InitHandle);
	free(rail);
}

static VOID VCAPITYPE rail_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	railPlugin* rail;
	UINT error = CHANNEL_RC_OK;

	rail = (railPlugin*) rail_get_init_handle_data(pInitHandle);

	if (!rail)
	{
		WLog_ERR(TAG,  "rail_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = rail_virtual_channel_event_connected(rail, pData, dataLength)))
				WLog_ERR(TAG, "rail_virtual_channel_event_connected failed with error %lu!", error);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = rail_virtual_channel_event_disconnected(rail)))
		WLog_ERR(TAG, "rail_virtual_channel_event_disconnected failed with error %lu!", error);
			break;

		case CHANNEL_EVENT_TERMINATED:
			rail_virtual_channel_event_terminated(rail);
			break;
	}

	if(error && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error, "rail_virtual_channel_init_event reported an error");
}

/* rail is always built-in */
#define VirtualChannelEntry	rail_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;
	railPlugin* rail;
	RailClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;
	BOOL isFreerdp = FALSE;
	UINT error;

	rail = (railPlugin*) calloc(1, sizeof(railPlugin));
	if (!rail)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	rail->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(rail->channelDef.name, "rail");

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RailClientContext*) calloc(1, sizeof(RailClientContext));
		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(rail);
			return FALSE;
		}

		context->handle = (void*) rail;
		context->custom = NULL;

		context->ClientExecute = rail_client_execute;
		context->ClientActivate = rail_client_activate;
		context->ClientSystemParam = rail_client_system_param;
		context->ServerSystemParam = rail_server_system_param;
		context->ClientSystemCommand = rail_client_system_command;
		context->ClientHandshake = rail_client_handshake;
		context->ServerHandshake = rail_server_handshake;
		context->ClientHandshakeEx = rail_client_handshake_ex;
		context->ServerHandshakeEx = rail_server_handshake_ex;
		context->ClientNotifyEvent = rail_client_notify_event;
		context->ClientWindowMove = rail_client_window_move;
		context->ServerLocalMoveSize = rail_server_local_move_size;
		context->ServerMinMaxInfo = rail_server_min_max_info;
		context->ClientInformation = rail_client_information;
		context->ClientSystemMenu = rail_client_system_menu;
		context->ClientLanguageBarInfo = rail_client_language_bar_info;
		context->ServerLanguageBarInfo = rail_server_language_bar_info;
		context->ServerExecuteResult = rail_server_execute_result;
		context->ClientGetAppIdRequest = rail_client_get_appid_request;
		context->ServerGetAppIdResponse = rail_server_get_appid_response;
		rail->rdpcontext = pEntryPointsEx->context;

		*(pEntryPointsEx->ppInterface) = (void*) context;
		rail->context = context;
		isFreerdp = TRUE;
	}

	WLog_Init();
	rail->log = WLog_Get("com.freerdp.channels.rail.client");

	WLog_Print(rail->log, WLOG_DEBUG, "VirtualChannelEntry");

	CopyMemory(&(rail->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = rail->channelEntryPoints.pVirtualChannelInit(&rail->InitHandle,
		&rail->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, rail_virtual_channel_init_event);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		goto error_out;
	}

	rail->channelEntryPoints.pInterface = *(rail->channelEntryPoints.ppInterface);
	rail->channelEntryPoints.ppInterface = &(rail->channelEntryPoints.pInterface);

	if ((error = rail_add_init_handle_data(rail->InitHandle, (void*) rail)))
	{
		WLog_ERR(TAG, "rail_add_init_handle_data failed with error %lu!", error);
		goto error_out;
	}

	return TRUE;
error_out:
	if (isFreerdp)
		free(rail->context);
	free(rail);
	return FALSE;
}
