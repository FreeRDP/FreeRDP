/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include <freerdp/client/rail.h>
#include <freerdp/server/rail.h>

#include "pf_rail.h"
#include "pf_context.h"
#include "pf_log.h"

#define TAG PROXY_TAG("rail")

BOOL pf_rail_context_init(pServerContext* ps)
{
	RailServerContext* rail;
	rail = ps->rail = rail_server_context_new(ps->vcm);

	if (!rail)
	{
		return FALSE;
	}

	rail->rdpcontext = (rdpContext*)ps;
	return TRUE;
}

static UINT pf_rail_client_on_open(RailClientContext* context, BOOL* sendHandshake)
{
	if (NULL != sendHandshake)
		*sendHandshake = FALSE;

	return CHANNEL_RC_OK;
}

/* Callbacks from client side */
static UINT pf_rail_server_handshake(RailClientContext* client,
                                     const RAIL_HANDSHAKE_ORDER* handshake)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerHandshake(server, handshake);
}

static UINT pf_rail_server_handshake_ex(RailClientContext* client,
                                        const RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerHandshakeEx(server, handshakeEx);
}

static UINT pf_rail_server_sysparam(RailClientContext* client, const RAIL_SYSPARAM_ORDER* sysparam)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerSysparam(server, sysparam);
}

static UINT pf_rail_server_local_move_size(RailClientContext* client,
                                           const RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerLocalMoveSize(server, localMoveSize);
}

static UINT pf_rail_server_min_max_info(RailClientContext* client,
                                        const RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerMinMaxInfo(server, minMaxInfo);
}

static UINT pf_rail_server_taskbar_info(RailClientContext* client,
                                        const RAIL_TASKBAR_INFO_ORDER* taskbarInfo)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerTaskbarInfo(server, taskbarInfo);
}

static UINT pf_rail_server_langbar_info(RailClientContext* client,
                                        const RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerLangbarInfo(server, langbarInfo);
}

static UINT pf_rail_server_exec_result(RailClientContext* client,
                                       const RAIL_EXEC_RESULT_ORDER* execResult)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerExecResult(server, execResult);
}

static UINT pf_rail_server_z_order_sync(RailClientContext* client,
                                        const RAIL_ZORDER_SYNC* zOrderSync)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerZOrderSync(server, zOrderSync);
}

static UINT pf_rail_server_cloak(RailClientContext* client, const RAIL_CLOAK* cloak)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerCloak(server, cloak);
}

static UINT
pf_rail_server_power_display_request(RailClientContext* client,
                                     const RAIL_POWER_DISPLAY_REQUEST* powerDisplayRequest)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerPowerDisplayRequest(server, powerDisplayRequest);
}

static UINT pf_rail_server_get_appid_resp(RailClientContext* client,
                                          const RAIL_GET_APPID_RESP_ORDER* getAppidResp)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerGetAppidResp(server, getAppidResp);
}

static UINT pf_rail_server_get_appid_resp_ex(RailClientContext* client,
                                             const RAIL_GET_APPID_RESP_EX* getAppidRespEx)
{
	proxyData* pdata = (proxyData*)client->custom;
	RailServerContext* server = (RailServerContext*)pdata->ps->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return server->ServerGetAppidRespEx(server, getAppidRespEx);
}

/* Callbacks from server side */

static UINT pf_rail_client_handshake(RailServerContext* server,
                                     const RAIL_HANDSHAKE_ORDER* handshake)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientHandshake(client, handshake);
}

static UINT pf_rail_client_client_status(RailServerContext* server,
                                         const RAIL_CLIENT_STATUS_ORDER* clientStatus)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientInformation(client, clientStatus);
}

static UINT pf_rail_client_exec(RailServerContext* server, const RAIL_EXEC_ORDER* exec)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientExecute(client, exec);
}

static UINT pf_rail_client_sysparam(RailServerContext* server, const RAIL_SYSPARAM_ORDER* sysparam)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientSystemParam(client, sysparam);
}

static UINT pf_rail_client_activate(RailServerContext* server, const RAIL_ACTIVATE_ORDER* activate)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientActivate(client, activate);
}

static UINT pf_rail_client_sysmenu(RailServerContext* server, const RAIL_SYSMENU_ORDER* sysmenu)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientSystemMenu(client, sysmenu);
}

static UINT pf_rail_client_syscommand(RailServerContext* server,
                                      const RAIL_SYSCOMMAND_ORDER* syscommand)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientSystemCommand(client, syscommand);
}

static UINT pf_rail_client_notify_event(RailServerContext* server,
                                        const RAIL_NOTIFY_EVENT_ORDER* notifyEvent)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientNotifyEvent(client, notifyEvent);
}

static UINT pf_rail_client_window_move(RailServerContext* server,
                                       const RAIL_WINDOW_MOVE_ORDER* windowMove)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientWindowMove(client, windowMove);
}

static UINT pf_rail_client_snap_arrange(RailServerContext* server,
                                        const RAIL_SNAP_ARRANGE* snapArrange)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientSnapArrange(client, snapArrange);
}

static UINT pf_rail_client_get_appid_req(RailServerContext* server,
                                         const RAIL_GET_APPID_REQ_ORDER* getAppidReq)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientGetAppIdRequest(client, getAppidReq);
}

static UINT pf_rail_client_langbar_info(RailServerContext* server,
                                        const RAIL_LANGBAR_INFO_ORDER* langbarInfo)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientLanguageBarInfo(client, langbarInfo);
}

static UINT pf_rail_client_language_ime_info(RailServerContext* server,
                                             const RAIL_LANGUAGEIME_INFO_ORDER* languageImeInfo)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientLanguageIMEInfo(client, languageImeInfo);
}

static UINT pf_rail_client_compartment_info(RailServerContext* server,
                                            const RAIL_COMPARTMENT_INFO_ORDER* compartmentInfo)
{
	WLog_DBG(TAG, __FUNCTION__);
	return 0;
}

static UINT pf_rail_client_cloak(RailServerContext* server, const RAIL_CLOAK* cloak)
{
	proxyData* pdata = (proxyData*)server->custom;
	RailClientContext* client = (RailClientContext*)pdata->pc->rail;
	WLog_DBG(TAG, __FUNCTION__);
	return client->ClientCloak(client, cloak);
}

void pf_rail_pipeline_init(RailClientContext* client, RailServerContext* server, proxyData* pdata)
{
	/* Set server and client side references to proxy data */
	client->custom = (void*)pdata;
	server->custom = (void*)pdata;
	/* Set client callbacks */
	client->OnOpen = pf_rail_client_on_open;
	client->ServerHandshake = pf_rail_server_handshake;
	client->ServerHandshakeEx = pf_rail_server_handshake_ex;
	client->ServerSystemParam = pf_rail_server_sysparam;
	client->ServerLocalMoveSize = pf_rail_server_local_move_size;
	client->ServerMinMaxInfo = pf_rail_server_min_max_info;
	client->ServerTaskBarInfo = pf_rail_server_taskbar_info;
	client->ServerLanguageBarInfo = pf_rail_server_langbar_info;
	client->ServerExecuteResult = pf_rail_server_exec_result;
	client->ServerZOrderSync = pf_rail_server_z_order_sync;
	client->ServerCloak = pf_rail_server_cloak;
	client->ServerPowerDisplayRequest = pf_rail_server_power_display_request;
	client->ServerGetAppIdResponse = pf_rail_server_get_appid_resp;
	client->ServerGetAppidResponseExtended = pf_rail_server_get_appid_resp_ex;
	/* Set server callbacks */
	server->ClientHandshake = pf_rail_client_handshake;
	server->ClientClientStatus = pf_rail_client_client_status;
	server->ClientExec = pf_rail_client_exec;
	server->ClientSysparam = pf_rail_client_sysparam;
	server->ClientActivate = pf_rail_client_activate;
	server->ClientSysmenu = pf_rail_client_sysmenu;
	server->ClientSyscommand = pf_rail_client_syscommand;
	server->ClientNotifyEvent = pf_rail_client_notify_event;
	server->ClientGetAppidReq = pf_rail_client_get_appid_req;
	server->ClientWindowMove = pf_rail_client_window_move;
	server->ClientSnapArrange = pf_rail_client_snap_arrange;
	server->ClientLangbarInfo = pf_rail_client_langbar_info;
	server->ClientLanguageImeInfo = pf_rail_client_language_ime_info;
	server->ClientCompartmentInfo = pf_rail_client_compartment_info;
	server->ClientCloak = pf_rail_client_cloak;
}
