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

typedef struct _rail_client_context RailClientContext;

typedef UINT (*pcRailOnOpen)(RailClientContext* context, BOOL* sendHandshake);

typedef UINT (*pcRailClientExecute)(RailClientContext* context, const RAIL_EXEC_ORDER* exec);
typedef UINT (*pcRailClientActivate)(RailClientContext* context,
                                     const RAIL_ACTIVATE_ORDER* activate);
typedef UINT (*pcRailClientSystemParam)(RailClientContext* context,
                                        const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT (*pcRailServerSystemParam)(RailClientContext* context,
                                        const RAIL_SYSPARAM_ORDER* sysparam);
typedef UINT (*pcRailClientSystemCommand)(RailClientContext* context,
                                          const RAIL_SYSCOMMAND_ORDER* syscommand);
typedef UINT (*pcRailClientHandshake)(RailClientContext* context,
                                      const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT (*pcRailServerHandshake)(RailClientContext* context,
                                      const RAIL_HANDSHAKE_ORDER* handshake);
typedef UINT (*pcRailServerHandshakeEx)(RailClientContext* context,
                                        const RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
typedef UINT (*pcRailClientNotifyEvent)(RailClientContext* context,
                                        const RAIL_NOTIFY_EVENT_ORDER* notifyEvent);
typedef UINT (*pcRailClientWindowMove)(RailClientContext* context,
                                       const RAIL_WINDOW_MOVE_ORDER* windowMove);
typedef UINT (*pcRailServerLocalMoveSize)(RailClientContext* context,
                                          const RAIL_LOCALMOVESIZE_ORDER* localMoveSize);
typedef UINT (*pcRailServerMinMaxInfo)(RailClientContext* context,
                                       const RAIL_MINMAXINFO_ORDER* minMaxInfo);
typedef UINT (*pcRailClientInformation)(RailClientContext* context,
                                        const RAIL_CLIENT_STATUS_ORDER* clientStatus);
typedef UINT (*pcRailClientSystemMenu)(RailClientContext* context,
                                       const RAIL_SYSMENU_ORDER* sysmenu);
typedef UINT (*pcRailServerTaskBarInfo)(RailClientContext* context,
                                        const RAIL_TASKBAR_INFO_ORDER* taskBarInfo);
typedef UINT (*pcRailClientLanguageBarInfo)(RailClientContext* context,
                                            const RAIL_LANGBAR_INFO_ORDER* langBarInfo);
typedef UINT (*pcRailServerLanguageBarInfo)(RailClientContext* context,
                                            const RAIL_LANGBAR_INFO_ORDER* langBarInfo);
typedef UINT (*pcRailClientLanguageIMEInfo)(RailClientContext* context,
                                            const RAIL_LANGUAGEIME_INFO_ORDER* langImeInfo);
typedef UINT (*pcRailServerExecuteResult)(RailClientContext* context,
                                          const RAIL_EXEC_RESULT_ORDER* execResult);
typedef UINT (*pcRailClientGetAppIdRequest)(RailClientContext* context,
                                            const RAIL_GET_APPID_REQ_ORDER* getAppIdReq);
typedef UINT (*pcRailServerGetAppIdResponse)(RailClientContext* context,
                                             const RAIL_GET_APPID_RESP_ORDER* getAppIdResp);
typedef UINT (*pcRailServerZOrderSync)(RailClientContext* context, const RAIL_ZORDER_SYNC* zorder);
typedef UINT (*pcRailServerCloak)(RailClientContext* context, const RAIL_CLOAK* cloak);
typedef UINT (*pcRailClientCloak)(RailClientContext* context, const RAIL_CLOAK* cloak);
typedef UINT (*pcRailServerPowerDisplayRequest)(RailClientContext* context,
                                                const RAIL_POWER_DISPLAY_REQUEST* power);
typedef UINT (*pcRailClientSnapArrange)(RailClientContext* context, const RAIL_SNAP_ARRANGE* snap);
typedef UINT (*pcRailServerGetAppidResponseExtended)(RailClientContext* context,
                                                     const RAIL_GET_APPID_RESP_EX* id);
typedef UINT (*pcRailClientCompartmentInfo)(RailClientContext* context,
                                            const RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo);

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
	pcRailServerHandshakeEx ServerHandshakeEx;
	pcRailClientNotifyEvent ClientNotifyEvent;
	pcRailClientWindowMove ClientWindowMove;
	pcRailServerLocalMoveSize ServerLocalMoveSize;
	pcRailServerMinMaxInfo ServerMinMaxInfo;
	pcRailClientInformation ClientInformation;
	pcRailClientSystemMenu ClientSystemMenu;
	pcRailServerTaskBarInfo ServerTaskBarInfo;
	pcRailClientLanguageBarInfo ClientLanguageBarInfo;
	pcRailServerLanguageBarInfo ServerLanguageBarInfo;
	pcRailClientLanguageIMEInfo ClientLanguageIMEInfo;
	pcRailServerExecuteResult ServerExecuteResult;
	pcRailClientGetAppIdRequest ClientGetAppIdRequest;
	pcRailServerGetAppIdResponse ServerGetAppIdResponse;
	pcRailServerZOrderSync ServerZOrderSync;
	pcRailClientCloak ClientCloak;
	pcRailServerCloak ServerCloak;
	pcRailServerPowerDisplayRequest ServerPowerDisplayRequest;
	pcRailClientSnapArrange ClientSnapArrange;
	pcRailServerGetAppidResponseExtended ServerGetAppidResponseExtended;
	pcRailClientCompartmentInfo ClientCompartmentInfo;
	pcRailOnOpen OnOpen;
};

#endif /* FREERDP_CHANNEL_RAIL_CLIENT_RAIL_H */
