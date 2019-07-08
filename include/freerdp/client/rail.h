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

#ifndef FREERDP_CHANNEL_RAIL_CLIENT_RAIL_H
#define FREERDP_CHANNEL_RAIL_CLIENT_RAIL_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/rail.h>
#include <freerdp/message.h>
#include <freerdp/channels/rail.h>

/**
 * Client Interface
 */

#define RAIL_SVC_CHANNEL_NAME "rail"

typedef struct _rail_client_context RailClientContext;

/* Callbacks from Server */
typedef UINT(*pcRailServerHandshake)(RailClientContext* context,
                                     const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT(*pcRailServerHandshakeEx)(RailClientContext* context,
                                       const RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
typedef UINT(*pcRailServerSysparam)(RailClientContext* context,
                                    const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT(*pcRailServerLocalMoveSize)(RailClientContext* context,
        const RAIL_LOCALMOVESIZE_ORDER* localMoveSize);
typedef UINT(*pcRailServerMinMaxInfo)(RailClientContext* context,
                                      const RAIL_MINMAXINFO_ORDER* minMaxInfo);
typedef UINT(*pcRailServerTaskbarInfo)(RailClientContext* context,
                                       const RAIL_TASKBAR_INFO_ORDER* taskBarInfo);
typedef UINT(*pcRailServerLangbarInfo)(RailClientContext* context,
                                       const RAIL_LANGBAR_INFO_ORDER* langBarInfo);
typedef UINT(*pcRailServerExecResult)(RailClientContext* context,
                                      const RAIL_EXEC_RESULT_ORDER* execResult);
typedef UINT(*pcRailServerZOrderSync)(RailClientContext* context,
                                      const RAIL_ZORDER_SYNC_ORDER* zorder);
typedef UINT(*pcRailServerCloak)(RailClientContext* context,
                                 const RAIL_CLOAK_ORDER* cloak);
typedef UINT(*pcRailServerPowerDisplayRequest)(RailClientContext* context,
        const RAIL_POWER_DISPLAY_REQUEST_ORDER* power);
typedef UINT(*pcRailServerGetAppidResp)(RailClientContext* context,
                                        const RAIL_GET_APPID_RESP_ORDER* getAppidResp);
typedef UINT(*pcRailServerGetAppidRespEx)(RailClientContext* context,
        const RAIL_GET_APPID_RESP_EX_ORDER* id);

/* Methods for sending client side messages */
typedef UINT(*pcRailClientHandshake)(RailClientContext* context,
                                     const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT(*pcRailClientClientStatus)(RailClientContext* context,
                                        const RAIL_CLIENT_STATUS_ORDER* clientStatus);
typedef UINT(*pcRailClientExec)(RailClientContext* context,
                                RAIL_EXEC_ORDER* exec);
typedef UINT(*pcRailClientSysparam)(RailClientContext* context,
                                    const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT(*pcRailClientActivate)(RailClientContext* context,
                                    const RAIL_ACTIVATE_ORDER* activate);
typedef UINT(*pcRailClientSysmenu)(RailClientContext* context,
                                   const RAIL_SYSMENU_ORDER* sysmenu);
typedef UINT(*pcRailClientSyscommand)(RailClientContext* context,
                                      const RAIL_SYSCOMMAND_ORDER* syscommand);
typedef UINT(*pcRailClientNotifyEvent)(RailClientContext* context,
                                       const RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
typedef UINT(*pcRailClientGetAppidReq)(RailClientContext* context,
                                       const RAIL_GET_APPID_REQ_ORDER* getAppidReq);
typedef UINT(*pcRailClientWindowMove)(RailClientContext* context,
                                      const RAIL_WINDOW_MOVE_ORDER* windowMove);
typedef UINT(*pcRailClientSnapArrange)(RailClientContext* context,
                                       const RAIL_SNAP_ARRANGE_ORDER* snap);
typedef UINT(*pcRailClientLangbarInfo)(RailClientContext* context,
                                       const RAIL_LANGBAR_INFO_ORDER* langbarInfo);
typedef UINT(*pcRailClientLanguageImeInfo)(RailClientContext* context,
        const RAIL_LANGUAGEIME_INFO_ORDER* languageIme);
typedef UINT(*pcRailClientCompartmentInfo)(RailClientContext* context,
        const RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo);
typedef UINT(*pcRailClientCloak)(RailClientContext* context,
                                 const RAIL_CLOAK_ORDER* cloak);
struct _rail_client_context
{
	void* handle;
	void* custom;

	/* Callbacks from Server */
	pcRailServerHandshake ServerHandshake;
	pcRailServerHandshakeEx ServerHandshakeEx;
	pcRailServerSysparam ServerSysparam;
	pcRailServerLocalMoveSize ServerLocalMoveSize;
	pcRailServerMinMaxInfo ServerMinMaxInfo;
	pcRailServerTaskbarInfo ServerTaskbarInfo;
	pcRailServerLangbarInfo ServerLangbarInfo;
	pcRailServerExecResult ServerExecResult;
	pcRailServerZOrderSync ServerZOrderSync;
	pcRailServerCloak ServerCloak;
	pcRailServerPowerDisplayRequest ServerPowerDisplayRequest;
	pcRailServerGetAppidResp ServerGetAppidResp;
	pcRailServerGetAppidRespEx ServerGetAppidRespEx;

	/* Methods for sending client side messages */
	pcRailClientHandshake ClientHandshake;
	pcRailClientClientStatus ClientClientStatus;
	pcRailClientExec ClientExec;
	pcRailClientSysparam ClientSysparam;
	pcRailClientActivate ClientActivate;
	pcRailClientSysmenu ClientSysmenu;
	pcRailClientSyscommand ClientSyscommand;
	pcRailClientNotifyEvent ClientNotifyEvent;
	pcRailClientGetAppidReq ClientGetAppidReq;
	pcRailClientWindowMove ClientWindowMove;
	pcRailClientSnapArrange ClientSnapArrange;
	pcRailClientLangbarInfo ClientLangbarInfo;
	pcRailClientLanguageImeInfo ClientLanguageImeInfo;
	pcRailClientCompartmentInfo ClientCompartmentInfo;
	pcRailClientCloak ClientCloak;
};

#endif /* FREERDP_CHANNEL_RAIL_CLIENT_RAIL_H */
