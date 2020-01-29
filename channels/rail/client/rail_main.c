/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
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

#include <freerdp/types.h>
#include <freerdp/constants.h>

#include "rail_orders.h"
#include "rail_main.h"

RailClientContext* rail_get_client_interface(railPlugin* rail)
{
	RailClientContext* pInterface;

	if (!rail)
		return NULL;

	pInterface = (RailClientContext*)rail->channelEntryPoints.pInterface;
	return pInterface;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send(railPlugin* rail, wStream* s)
{
	UINT status;

	if (!rail)
	{
		Stream_Free(s, TRUE);
		return CHANNEL_RC_BAD_INIT_HANDLE;
	}

	status = rail->channelEntryPoints.pVirtualChannelWriteEx(
	    rail->InitHandle, rail->OpenHandle, Stream_Buffer(s), (UINT32)Stream_GetPosition(s), s);

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG, "pVirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
		         WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rail_send_channel_data(railPlugin* rail, wStream* src)
{
	wStream* s;
	size_t length;

	if (!rail || !src)
		return ERROR_INVALID_PARAMETER;

	length = Stream_GetPosition(src);
	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(s, Stream_Buffer(src), length);
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
static UINT rail_client_execute(RailClientContext* context, const RAIL_EXEC_ORDER* exec)
{
	char* exeOrFile;
	UINT error;
	railPlugin* rail;
	UINT16 flags;
	RAIL_UNICODE_STRING ruExeOrFile = { 0 };
	RAIL_UNICODE_STRING ruWorkingDir = { 0 };
	RAIL_UNICODE_STRING ruArguments = { 0 };

	if (!context || !exec)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	exeOrFile = exec->RemoteApplicationProgram;
	flags = exec->flags;

	if (!exeOrFile)
		return ERROR_INVALID_PARAMETER;

	if (!utf8_string_to_rail_string(exec->RemoteApplicationProgram,
	                                &ruExeOrFile) || /* RemoteApplicationProgram */
	    !utf8_string_to_rail_string(exec->RemoteApplicationWorkingDir,
	                                &ruWorkingDir) || /* ShellWorkingDirectory */
	    !utf8_string_to_rail_string(exec->RemoteApplicationArguments,
	                                &ruArguments)) /* RemoteApplicationCmdLine */
		error = ERROR_INTERNAL_ERROR;
	else
		error = rail_send_client_exec_order(rail, flags, &ruExeOrFile, &ruWorkingDir, &ruArguments);

	free(ruExeOrFile.string);
	free(ruWorkingDir.string);
	free(ruArguments.string);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_activate(RailClientContext* context, const RAIL_ACTIVATE_ORDER* activate)
{
	railPlugin* rail;

	if (!context || !activate)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_activate_order(rail, activate);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_send_client_sysparam(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	wStream* s;
	size_t length = RAIL_SYSPARAM_ORDER_LENGTH;
	railPlugin* rail;
	UINT error;
	BOOL extendedSpiSupported;

	if (!context || !sysparam)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;

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

		case SPI_SETFILTERKEYS:
			length += 20;
			break;

		case SPI_SETSTICKYKEYS:
		case SPI_SETCARETWIDTH:
		case SPI_SETTOGGLEKEYS:
			length += 4;
			break;

		default:
			return ERROR_BAD_ARGUMENTS;
	}

	s = rail_pdu_init(length);

	if (!s)
	{
		WLog_ERR(TAG, "rail_pdu_init failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	extendedSpiSupported = rail_is_extended_spi_supported(rail->channelFlags);
	if ((error = rail_write_sysparam_order(s, sysparam, extendedSpiSupported)))
	{
		WLog_ERR(TAG, "rail_write_client_sysparam_order failed with error %" PRIu32 "!", error);
		Stream_Free(s, TRUE);
		return error;
	}

	if ((error = rail_send_pdu(rail, s, TS_RAIL_ORDER_SYSPARAM)))
	{
		WLog_ERR(TAG, "rail_send_pdu failed with error %" PRIu32 "!", error);
	}

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_system_param(RailClientContext* context,
                                     const RAIL_SYSPARAM_ORDER* sysInParam)
{
	UINT error = CHANNEL_RC_OK;
	RAIL_SYSPARAM_ORDER sysparam;

	if (!context || !sysInParam)
		return ERROR_INVALID_PARAMETER;

	sysparam = *sysInParam;

	if (sysparam.params & SPI_MASK_SET_HIGH_CONTRAST)
	{
		sysparam.param = SPI_SET_HIGH_CONTRAST;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_TASKBAR_POS)
	{
		sysparam.param = SPI_TASKBAR_POS;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_SET_MOUSE_BUTTON_SWAP)
	{
		sysparam.param = SPI_SET_MOUSE_BUTTON_SWAP;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_SET_KEYBOARD_PREF)
	{
		sysparam.param = SPI_SET_KEYBOARD_PREF;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_SET_DRAG_FULL_WINDOWS)
	{
		sysparam.param = SPI_SET_DRAG_FULL_WINDOWS;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_SET_KEYBOARD_CUES)
	{
		sysparam.param = SPI_SET_KEYBOARD_CUES;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
			return error;
		}
	}

	if (sysparam.params & SPI_MASK_SET_WORK_AREA)
	{
		sysparam.param = SPI_SET_WORK_AREA;

		if ((error = rail_send_client_sysparam(context, &sysparam)))
		{
			WLog_ERR(TAG, "rail_send_client_sysparam failed with error %" PRIu32 "!", error);
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
static UINT rail_client_system_command(RailClientContext* context,
                                       const RAIL_SYSCOMMAND_ORDER* syscommand)
{
	railPlugin* rail;

	if (!context || !syscommand)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_syscommand_order(rail, syscommand);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_handshake(RailClientContext* context, const RAIL_HANDSHAKE_ORDER* handshake)
{
	railPlugin* rail;

	if (!context || !handshake)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_handshake_order(rail, handshake);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_notify_event(RailClientContext* context,
                                     const RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	railPlugin* rail;

	if (!context || !notifyEvent)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_notify_event_order(rail, notifyEvent);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_window_move(RailClientContext* context,
                                    const RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	railPlugin* rail;

	if (!context || !windowMove)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_window_move_order(rail, windowMove);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_information(RailClientContext* context,
                                    const RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	railPlugin* rail;

	if (!context || !clientStatus)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_status_order(rail, clientStatus);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_system_menu(RailClientContext* context, const RAIL_SYSMENU_ORDER* sysmenu)
{
	railPlugin* rail;

	if (!context || !sysmenu)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_sysmenu_order(rail, sysmenu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_language_bar_info(RailClientContext* context,
                                          const RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	railPlugin* rail;

	if (!context || !langBarInfo)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_langbar_info_order(rail, langBarInfo);
}

static UINT rail_client_language_ime_info(RailClientContext* context,
                                          const RAIL_LANGUAGEIME_INFO_ORDER* langImeInfo)
{
	railPlugin* rail;

	if (!context || !langImeInfo)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_languageime_info_order(rail, langImeInfo);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_client_get_appid_request(RailClientContext* context,
                                          const RAIL_GET_APPID_REQ_ORDER* getAppIdReq)
{
	railPlugin* rail;

	if (!context || !getAppIdReq || !context->handle)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_get_appid_req_order(rail, getAppIdReq);
}

static UINT rail_client_compartment_info(RailClientContext* context,
                                         const RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo)
{
	railPlugin* rail;

	if (!context || !compartmentInfo || !context->handle)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_compartment_info_order(rail, compartmentInfo);
}

static UINT rail_client_cloak(RailClientContext* context, const RAIL_CLOAK* cloak)
{
	railPlugin* rail;

	if (!context || !cloak || !context->handle)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_cloak_order(rail, cloak);
}

static UINT rail_client_snap_arrange(RailClientContext* context, const RAIL_SNAP_ARRANGE* snap)
{
	railPlugin* rail;

	if (!context || !snap || !context->handle)
		return ERROR_INVALID_PARAMETER;

	rail = (railPlugin*)context->handle;
	return rail_send_client_snap_arrange_order(rail, snap);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_virtual_channel_event_data_received(railPlugin* rail, void* pData,
                                                     UINT32 dataLength, UINT32 totalLength,
                                                     UINT32 dataFlags)
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

	if (!Stream_EnsureRemainingCapacity(data_in, dataLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "rail_plugin_process_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		rail->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(rail->queue, NULL, 0, (void*)data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE rail_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
                                                         UINT event, LPVOID pData,
                                                         UINT32 dataLength, UINT32 totalLength,
                                                         UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	railPlugin* rail = (railPlugin*)lpUserParam;

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if (!rail || (rail->OpenHandle != openHandle))
			{
				WLog_ERR(TAG, "error no match");
				return;
			}
			if ((error = rail_virtual_channel_event_data_received(rail, pData, dataLength,
			                                                      totalLength, dataFlags)))
				WLog_ERR(TAG,
				         "rail_virtual_channel_event_data_received failed with error %" PRIu32 "!",
				         error);

			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
		case CHANNEL_EVENT_WRITE_COMPLETE:
		{
			wStream* s = (wStream*)pData;
			Stream_Free(s, TRUE);
		}
		break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && rail && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error,
		                "rail_virtual_channel_open_event reported an error");

	return;
}

static DWORD WINAPI rail_virtual_channel_client_thread(LPVOID arg)
{
	wStream* data;
	wMessage message;
	railPlugin* rail = (railPlugin*)arg;
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
			data = (wStream*)message.wParam;
			error = rail_order_recv(rail, data);
			Stream_Free(data, TRUE);

			if (error)
			{
				WLog_ERR(TAG, "rail_order_recv failed with error %" PRIu32 "!", error);
				break;
			}
		}
	}

	if (error && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error,
		                "rail_virtual_channel_client_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rail_virtual_channel_event_connected(railPlugin* rail, LPVOID pData, UINT32 dataLength)
{
	RailClientContext* context = rail_get_client_interface(rail);
	UINT status;
	status = rail->channelEntryPoints.pVirtualChannelOpenEx(rail->InitHandle, &rail->OpenHandle,
	                                                        rail->channelDef.name,
	                                                        rail_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08" PRIX32 "]",
		         WTSErrorToString(status), status);
		return status;
	}

	if (context)
	{
		IFCALLRET(context->OnOpen, status, context, &rail->sendHandshake);

		if (status != CHANNEL_RC_OK)
			WLog_ERR(TAG, "context->OnOpen failed with %s [%08" PRIX32 "]",
			         WTSErrorToString(status), status);
	}

	rail->queue = MessageQueue_New(NULL);

	if (!rail->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(rail->thread =
	          CreateThread(NULL, 0, rail_virtual_channel_client_thread, (void*)rail, 0, NULL)))
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

	if (rail->OpenHandle == 0)
		return CHANNEL_RC_OK;

	if (MessageQueue_PostQuit(rail->queue, 0) &&
	    (WaitForSingleObject(rail->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", rc);
		return rc;
	}

	MessageQueue_Free(rail->queue);
	CloseHandle(rail->thread);
	rail->queue = NULL;
	rail->thread = NULL;
	rc = rail->channelEntryPoints.pVirtualChannelCloseEx(rail->InitHandle, rail->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelCloseEx failed with %s [%08" PRIX32 "]", WTSErrorToString(rc),
		         rc);
		return rc;
	}

	rail->OpenHandle = 0;

	if (rail->data_in)
	{
		Stream_Free(rail->data_in, TRUE);
		rail->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}

static void rail_virtual_channel_event_terminated(railPlugin* rail)
{
	rail->InitHandle = 0;
	free(rail->context);
	free(rail);
}

static VOID VCAPITYPE rail_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
                                                         UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	railPlugin* rail = (railPlugin*)lpUserParam;

	if (!rail || (rail->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = rail_virtual_channel_event_connected(rail, pData, dataLength)))
				WLog_ERR(TAG, "rail_virtual_channel_event_connected failed with error %" PRIu32 "!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = rail_virtual_channel_event_disconnected(rail)))
				WLog_ERR(TAG,
				         "rail_virtual_channel_event_disconnected failed with error %" PRIu32 "!",
				         error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			rail_virtual_channel_event_terminated(rail);
			break;

		case CHANNEL_EVENT_ATTACHED:
		case CHANNEL_EVENT_DETACHED:
		default:
			break;
	}

	if (error && rail->rdpcontext)
		setChannelError(rail->rdpcontext, error,
		                "rail_virtual_channel_init_event_ex reported an error");
}

/* rail is always built-in */
#define VirtualChannelEntryEx rail_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	railPlugin* rail;
	RailClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	BOOL isFreerdp = FALSE;
	rail = (railPlugin*)calloc(1, sizeof(railPlugin));

	if (!rail)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	/* Default to automatically replying to server handshakes */
	rail->sendHandshake = TRUE;
	rail->channelDef.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	                           CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL;
	sprintf_s(rail->channelDef.name, ARRAYSIZE(rail->channelDef.name), RAIL_SVC_CHANNEL_NAME);
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RailClientContext*)calloc(1, sizeof(RailClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(rail);
			return FALSE;
		}

		context->handle = (void*)rail;
		context->custom = NULL;
		context->ClientExecute = rail_client_execute;
		context->ClientActivate = rail_client_activate;
		context->ClientSystemParam = rail_client_system_param;
		context->ClientSystemCommand = rail_client_system_command;
		context->ClientHandshake = rail_client_handshake;
		context->ClientNotifyEvent = rail_client_notify_event;
		context->ClientWindowMove = rail_client_window_move;
		context->ClientInformation = rail_client_information;
		context->ClientSystemMenu = rail_client_system_menu;
		context->ClientLanguageBarInfo = rail_client_language_bar_info;
		context->ClientLanguageIMEInfo = rail_client_language_ime_info;
		context->ClientGetAppIdRequest = rail_client_get_appid_request;
		context->ClientSnapArrange = rail_client_snap_arrange;
		context->ClientCloak = rail_client_cloak;
		context->ClientCompartmentInfo = rail_client_compartment_info;
		rail->rdpcontext = pEntryPointsEx->context;
		rail->context = context;
		isFreerdp = TRUE;
	}

	rail->log = WLog_Get("com.freerdp.channels.rail.client");
	WLog_Print(rail->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(rail->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	rail->InitHandle = pInitHandle;
	rc = rail->channelEntryPoints.pVirtualChannelInitEx(
	    rail, context, pInitHandle, &rail->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	    rail_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "failed with %s [%08" PRIX32 "]", WTSErrorToString(rc), rc);
		goto error_out;
	}

	rail->channelEntryPoints.pInterface = context;
	return TRUE;
error_out:

	if (isFreerdp)
		free(rail->context);

	free(rail);
	return FALSE;
}
