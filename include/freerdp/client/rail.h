/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_CLIENT_RAIL_H
#define FREERDP_CHANNEL_CLIENT_RAIL_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/rail.h>
#include <freerdp/message.h>
#include <freerdp/channels/rail.h>

/**
 * Client Interface
 */

#define RAIL_SVC_CHANNEL_NAME	"rail"

typedef struct _rail_client_context RailClientContext;

typedef int (*pcRailClientExecute)(RailClientContext* context, RAIL_EXEC_ORDER* exec);
typedef int (*pcRailClientActivate)(RailClientContext* context, RAIL_ACTIVATE_ORDER* activate);
typedef int (*pcRailClientSystemParam)(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam);
typedef int (*pcRailServerSystemParam)(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam);
typedef int (*pcRailClientSystemCommand)(RailClientContext* context, RAIL_SYSCOMMAND_ORDER* syscommand);
typedef int (*pcRailClientHandshake)(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake);
typedef int (*pcRailServerHandshake)(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake);
typedef int (*pcRailClientHandshakeEx)(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
typedef int (*pcRailServerHandshakeEx)(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
typedef int (*pcRailClientNotifyEvent)(RailClientContext* context, RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
typedef int (*pcRailClientWindowMove)(RailClientContext* context, RAIL_WINDOW_MOVE_ORDER* windowMove);
typedef int (*pcRailServerLocalMoveSize)(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize);
typedef int (*pcRailServerMinMaxInfo)(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo);
typedef int (*pcRailClientInformation)(RailClientContext* context, RAIL_CLIENT_STATUS_ORDER* clientStatus);
typedef int (*pcRailClientSystemMenu)(RailClientContext* context, RAIL_SYSMENU_ORDER* sysmenu);
typedef int (*pcRailClientLanguageBarInfo)(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo);
typedef int (*pcRailServerLanguageBarInfo)(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo);
typedef int (*pcRailServerExecuteResult)(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult);
typedef int (*pcRailClientGetAppIdRequest)(RailClientContext* context, RAIL_GET_APPID_REQ_ORDER* getAppIdReq);
typedef int (*pcRailServerGetAppIdResponse)(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp);

struct _rail_client_context
{
	void* handle;
	void* custom;

	pcRailClientExecute ClientExecute;
	pcRailClientActivate ClientActivate;
	pcRailClientSystemParam ClientSystemParam;
	pcRailServerSystemParam ServerSystemParam;
	pcRailClientSystemCommand ClientSystemCommand;
	pcRailClientHandshake ClientHandshake;
	pcRailServerHandshake ServerHandshake;
	pcRailClientHandshakeEx ClientHandshakeEx;
	pcRailServerHandshakeEx ServerHandshakeEx;
	pcRailClientNotifyEvent ClientNotifyEvent;
	pcRailClientWindowMove ClientWindowMove;
	pcRailServerLocalMoveSize ServerLocalMoveSize;
	pcRailServerMinMaxInfo ServerMinMaxInfo;
	pcRailClientInformation ClientInformation;
	pcRailClientSystemMenu ClientSystemMenu;
	pcRailClientLanguageBarInfo ClientLanguageBarInfo;
	pcRailServerLanguageBarInfo ServerLanguageBarInfo;
	pcRailServerExecuteResult ServerExecuteResult;
	pcRailClientGetAppIdRequest ClientGetAppIdRequest;
	pcRailServerGetAppIdResponse ServerGetAppIdResponse;
};

#endif /* FREERDP_CHANNEL_CLIENT_RAIL_H */

