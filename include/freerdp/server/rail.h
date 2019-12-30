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

#ifndef FREERDP_CHANNEL_RAIL_SERVER_RAIL_H
#define FREERDP_CHANNEL_RAIL_SERVER_RAIL_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

#include <freerdp/rail.h>
#include <freerdp/channels/rail.h>

typedef struct _rail_server_context RailServerContext;
typedef struct _rail_server_private RailServerPrivate;

typedef UINT (*psRailStart)(RailServerContext* context);
typedef BOOL (*psRailStop)(RailServerContext* context);

/* Client side callback types */
typedef UINT (*psRailClientHandshake)(RailServerContext* context,
                                      const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT (*psRailClientClientStatus)(RailServerContext* context,
                                         const RAIL_CLIENT_STATUS_ORDER* clientStatus);
typedef UINT (*psRailClientExec)(RailServerContext* context, const RAIL_EXEC_ORDER* exec);
typedef UINT (*psRailClientSysparam)(RailServerContext* context,
                                     const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT (*psRailClientActivate)(RailServerContext* context,
                                     const RAIL_ACTIVATE_ORDER* activate);
typedef UINT (*psRailClientSysmenu)(RailServerContext* context, const RAIL_SYSMENU_ORDER* sysmenu);
typedef UINT (*psRailClientSyscommand)(RailServerContext* context,
                                       const RAIL_SYSCOMMAND_ORDER* syscommand);
typedef UINT (*psRailClientNotifyEvent)(RailServerContext* context,
                                        const RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
typedef UINT (*psRailClientGetAppidReq)(RailServerContext* context,
                                        const RAIL_GET_APPID_REQ_ORDER* getAppidReq);
typedef UINT (*psRailClientWindowMove)(RailServerContext* context,
                                       const RAIL_WINDOW_MOVE_ORDER* windowMove);
typedef UINT (*psRailClientSnapArrange)(RailServerContext* context,
                                        const RAIL_SNAP_ARRANGE* snapArrange);
typedef UINT (*psRailClientLangbarInfo)(RailServerContext* context,
                                        const RAIL_LANGBAR_INFO_ORDER* langbarInfo);
typedef UINT (*psRailClientLanguageImeInfo)(RailServerContext* context,
                                            const RAIL_LANGUAGEIME_INFO_ORDER* languageImeInfo);
typedef UINT (*psRailClientCompartmentInfo)(RailServerContext* context,
                                            const RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo);
typedef UINT (*psRailClientCloak)(RailServerContext* context, const RAIL_CLOAK* cloak);

/* Server side messages sending methods */
typedef UINT (*psRailServerHandshake)(RailServerContext* context,
                                      const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT (*psRailServerHandshakeEx)(RailServerContext* context,
                                        const RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
typedef UINT (*psRailServerSysparam)(RailServerContext* context,
                                     const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT (*psRailServerLocalMoveSize)(RailServerContext* context,
                                          const RAIL_LOCALMOVESIZE_ORDER* localMoveSize);
typedef UINT (*psRailServerMinMaxInfo)(RailServerContext* context,
                                       const RAIL_MINMAXINFO_ORDER* minMaxInfo);
typedef UINT (*psRailServerTaskbarInfo)(RailServerContext* context,
                                        const RAIL_TASKBAR_INFO_ORDER* taskbarInfo);
typedef UINT (*psRailServerLangbarInfo)(RailServerContext* context,
                                        const RAIL_LANGBAR_INFO_ORDER* langbarInfo);
typedef UINT (*psRailServerExecResult)(RailServerContext* context,
                                       const RAIL_EXEC_RESULT_ORDER* execResult);
typedef UINT (*psRailServerGetAppidResp)(RailServerContext* context,
                                         const RAIL_GET_APPID_RESP_ORDER* getAppIdResp);
typedef UINT (*psRailServerZOrderSync)(RailServerContext* context,
                                       const RAIL_ZORDER_SYNC* zOrderSync);
typedef UINT (*psRailServerCloak)(RailServerContext* context, const RAIL_CLOAK* cloak);
typedef UINT (*psRailServerPowerDisplayRequest)(
    RailServerContext* context, const RAIL_POWER_DISPLAY_REQUEST* PowerDisplayRequest);
typedef UINT (*psRailServerGetAppidRespEx)(RailServerContext* context,
                                           const RAIL_GET_APPID_RESP_EX* GetAppidRespEx);

struct _rail_server_context
{
	HANDLE vcm;
	void* custom;

	psRailStart Start;
	psRailStop Stop;

	/* Callbacks from client */
	psRailClientHandshake ClientHandshake;
	psRailClientClientStatus ClientClientStatus;
	psRailClientExec ClientExec;
	psRailClientSysparam ClientSysparam;
	psRailClientActivate ClientActivate;
	psRailClientSysmenu ClientSysmenu;
	psRailClientSyscommand ClientSyscommand;
	psRailClientNotifyEvent ClientNotifyEvent;
	psRailClientGetAppidReq ClientGetAppidReq;
	psRailClientWindowMove ClientWindowMove;
	psRailClientSnapArrange ClientSnapArrange;
	psRailClientLangbarInfo ClientLangbarInfo;
	psRailClientLanguageImeInfo ClientLanguageImeInfo;
	psRailClientCompartmentInfo ClientCompartmentInfo;
	psRailClientCloak ClientCloak;

	/* Methods for sending server side messages */
	psRailServerHandshake ServerHandshake;
	psRailServerHandshakeEx ServerHandshakeEx;
	psRailServerSysparam ServerSysparam;
	psRailServerLocalMoveSize ServerLocalMoveSize;
	psRailServerMinMaxInfo ServerMinMaxInfo;
	psRailServerTaskbarInfo ServerTaskbarInfo;
	psRailServerLangbarInfo ServerLangbarInfo;
	psRailServerExecResult ServerExecResult;
	psRailServerZOrderSync ServerZOrderSync;
	psRailServerCloak ServerCloak;
	psRailServerPowerDisplayRequest ServerPowerDisplayRequest;
	psRailServerGetAppidResp ServerGetAppidResp;
	psRailServerGetAppidRespEx ServerGetAppidRespEx;

	RailServerPrivate* priv;
	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API RailServerContext* rail_server_context_new(HANDLE vcm);
	FREERDP_API void rail_server_context_free(RailServerContext* context);
	FREERDP_API UINT rail_server_handle_messages(RailServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RAIL_SERVER_RAIL_H */
