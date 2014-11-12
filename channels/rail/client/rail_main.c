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

RailClientContext* rail_get_client_interface(void* railObject)
{
	RailClientContext* pInterface;
	rdpSvcPlugin* plugin = (rdpSvcPlugin*) railObject;
	pInterface = (RailClientContext*) plugin->channel_entry_points.pInterface;
	return pInterface;
}

void rail_send_channel_data(void* railObject, void* data, size_t length)
{
	wStream* s = NULL;
	railPlugin* plugin = (railPlugin*) railObject;

	s = Stream_New(NULL, length);
	Stream_Write(s, data, length);

	svc_plugin_send((rdpSvcPlugin*) plugin, s);
}

static void rail_process_connect(rdpSvcPlugin* plugin)
{
	railPlugin* rail = (railPlugin*) plugin;

	WLog_Print(rail->log, WLOG_DEBUG, "Connect");
}

static void rail_process_terminate(rdpSvcPlugin* plugin)
{
	railPlugin* rail = (railPlugin*) plugin;

	WLog_Print(rail->log, WLOG_DEBUG, "Terminate");
	svc_plugin_terminate(plugin);
}

static void rail_process_receive(rdpSvcPlugin* plugin, wStream* s)
{
	railPlugin* rail = (railPlugin*) plugin;
	rail_order_recv(rail, s);
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

/* rail is always built-in */
#define VirtualChannelEntry	rail_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	railPlugin* rail;
	RailClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	rail = (railPlugin*) calloc(1, sizeof(railPlugin));

	rail->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(rail->plugin.channel_def.name, "rail");

	rail->plugin.connect_callback = rail_process_connect;
	rail->plugin.receive_callback = rail_process_receive;
	rail->plugin.terminate_callback = rail_process_terminate;

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
	}

	WLog_Init();
	rail->log = WLog_Get("com.freerdp.channels.rail.client");

	WLog_Print(rail->log, WLOG_DEBUG, "VirtualChannelEntry");

	svc_plugin_init((rdpSvcPlugin*) rail, pEntryPoints);

	return 1;
}
