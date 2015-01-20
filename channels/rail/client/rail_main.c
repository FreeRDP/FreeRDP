/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

int rail_send(railPlugin* rail, wStream* s)
{
	UINT32 status = 0;

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

void rail_send_channel_data(railPlugin* rail, void* data, size_t length)
{
	wStream* s = NULL;

	s = Stream_New(NULL, length);
	Stream_Write(s, data, length);

	rail_send(rail, s);
}

/**
 * Callback Interface
 */

int rail_client_execute(RailClientContext* context, RAIL_EXEC_ORDER* exec)
{
	char* exeOrFile;
	railPlugin* rail = (railPlugin*) context->handle;

	exeOrFile = exec->RemoteApplicationProgram;

	if (!exeOrFile)
		return -1;

	if (strlen(exeOrFile) >= 2)
	{
		if (strncmp(exeOrFile, "||", 2) != 0)
			exec->flags |= RAIL_EXEC_FLAG_FILE;
	}

	rail_string_to_unicode_string(exec->RemoteApplicationProgram, &exec->exeOrFile); /* RemoteApplicationProgram */
	rail_string_to_unicode_string(exec->RemoteApplicationWorkingDir, &exec->workingDir); /* ShellWorkingDirectory */
	rail_string_to_unicode_string(exec->RemoteApplicationArguments, &exec->arguments); /* RemoteApplicationCmdLine */

	rail_send_client_exec_order(rail, exec);

	return 0;
}

int rail_client_activate(RailClientContext* context, RAIL_ACTIVATE_ORDER* activate)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_activate_order(rail, activate);

	return 0;
}

void rail_send_client_sysparam(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	int length;
	railPlugin* rail = (railPlugin*) context->handle;

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
	rail_write_client_sysparam_order(s, sysparam);
	rail_send_pdu(rail, s, RDP_RAIL_ORDER_SYSPARAM);
	Stream_Free(s, TRUE);
}

int rail_client_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	if (sysparam->params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		sysparam->param = SPI_SET_HIGH_CONTRAST;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_TASKBAR_POS)
	{
		sysparam->param = SPI_TASKBAR_POS;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		sysparam->param = SPI_SET_MOUSE_BUTTON_SWAP;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		sysparam->param = SPI_SET_KEYBOARD_PREF;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		sysparam->param = SPI_SET_DRAG_FULL_WINDOWS;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		sysparam->param = SPI_SET_KEYBOARD_CUES;
		rail_send_client_sysparam(context, sysparam);
	}

	if (sysparam->params & SPI_MASK_SET_WORK_AREA)
	{
		sysparam->param = SPI_SET_WORK_AREA;
		rail_send_client_sysparam(context, sysparam);
	}

	return 0;
}

int rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return 0; /* stub - should be registered by client */
}

int rail_client_system_command(RailClientContext* context, RAIL_SYSCOMMAND_ORDER* syscommand)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_syscommand_order(rail, syscommand);

	return 0;
}

int rail_client_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_handshake_order(rail, handshake);

	return 0;
}

int rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	return 0; /* stub - should be registered by client */
}

int rail_client_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_handshake_ex_order(rail, handshakeEx);

	return 0;
}

int rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return 0; /* stub - should be registered by client */
}

int rail_client_notify_event(RailClientContext* context, RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_notify_event_order(rail, notifyEvent);

	return 0;
}

int rail_client_window_move(RailClientContext* context, RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_window_move_order(rail, windowMove);

	return 0;
}

int rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	return 0; /* stub - should be registered by client */
}

int rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	return 0; /* stub - should be registered by client */
}

int rail_client_information(RailClientContext* context, RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_status_order(rail, clientStatus);

	return 0;
}

int rail_client_system_menu(RailClientContext* context, RAIL_SYSMENU_ORDER* sysmenu)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_sysmenu_order(rail, sysmenu);

	return 0;
}

int rail_client_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_langbar_info_order(rail, langBarInfo);

	return 0;
}

int rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return 0; /* stub - should be registered by client */
}

int rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	return 0; /* stub - should be registered by client */
}

int rail_client_get_appid_request(RailClientContext* context, RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	railPlugin* rail = (railPlugin*) context->handle;

	rail_send_client_get_appid_req_order(rail, getAppIdReq);

	return 0;
}

int rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return 0; /* stub - should be registered by client */
}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

void rail_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
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

void rail_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
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

static void rail_virtual_channel_event_data_received(railPlugin* rail,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (rail->data_in)
			Stream_Free(rail->data_in, TRUE);

		rail->data_in = Stream_New(NULL, totalLength);
	}

	data_in = rail->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "rail_plugin_process_received: read error");
		}

		rail->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(rail->queue, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE rail_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	railPlugin* rail;

	rail = (railPlugin*) rail_get_open_handle_data(openHandle);

	if (!rail)
	{
		WLog_ERR(TAG,  "rail_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			rail_virtual_channel_event_data_received(rail, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* rail_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	railPlugin* rail = (railPlugin*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(rail->queue))
			break;

		if (MessageQueue_Peek(rail->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				rail_order_recv(rail, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void rail_virtual_channel_event_connected(railPlugin* rail, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = rail->channelEntryPoints.pVirtualChannelOpen(rail->InitHandle,
		&rail->OpenHandle, rail->channelDef.name, rail_virtual_channel_open_event);

	rail_add_open_handle_data(rail->OpenHandle, rail);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	rail->queue = MessageQueue_New(NULL);

	rail->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rail_virtual_channel_client_thread, (void*) rail, 0, NULL);
}

static void rail_virtual_channel_event_disconnected(railPlugin* rail)
{
	UINT rc;
	MessageQueue_PostQuit(rail->queue, 0);
	WaitForSingleObject(rail->thread, INFINITE);

	MessageQueue_Free(rail->queue);
	CloseHandle(rail->thread);

	rail->queue = NULL;
	rail->thread = NULL;

	rc = rail->channelEntryPoints.pVirtualChannelClose(rail->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	}

	if (rail->data_in)
	{
		Stream_Free(rail->data_in, TRUE);
		rail->data_in = NULL;
	}

	rail_remove_open_handle_data(rail->OpenHandle);
}

static void rail_virtual_channel_event_terminated(railPlugin* rail)
{
	rail_remove_init_handle_data(rail->InitHandle);
	free(rail);
}

static VOID VCAPITYPE rail_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	railPlugin* rail;

	rail = (railPlugin*) rail_get_init_handle_data(pInitHandle);

	if (!rail)
	{
		WLog_ERR(TAG,  "rail_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			rail_virtual_channel_event_connected(rail, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			rail_virtual_channel_event_disconnected(rail);
			break;

		case CHANNEL_EVENT_TERMINATED:
			rail_virtual_channel_event_terminated(rail);
			break;
	}
}

/* rail is always built-in */
#define VirtualChannelEntry	rail_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;
	railPlugin* rail;
	RailClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	rail = (railPlugin*) calloc(1, sizeof(railPlugin));

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

		*(pEntryPointsEx->ppInterface) = (void*) context;
		rail->context = context;
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
		free(rail);
		return -1;
	}

	rail->channelEntryPoints.pInterface = *(rail->channelEntryPoints.ppInterface);
	rail->channelEntryPoints.ppInterface = &(rail->channelEntryPoints.pInterface);

	rail_add_init_handle_data(rail->InitHandle, (void*) rail);

	return 1;
}
