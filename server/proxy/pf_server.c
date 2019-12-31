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

#include <errno.h>
#include <signal.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/winsock.h>
#include <winpr/thread.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/rdpgfx.h>

#include "pf_server.h"
#include "pf_log.h"
#include "pf_config.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_input.h"
#include "pf_update.h"
#include "pf_rdpgfx.h"
#include "pf_disp.h"
#include "pf_rail.h"
#include "pf_channels.h"
#include "pf_modules.h"

#define TAG PROXY_TAG("server")

static BOOL pf_server_parse_target_from_routing_token(rdpContext* context, char** target,
                                                      DWORD* port)
{
#define TARGET_MAX (100)
#define ROUTING_TOKEN_PREFIX "Cookie: msts="
	char* colon;
	size_t len;
	DWORD routing_token_length;
	const size_t prefix_len = strnlen(ROUTING_TOKEN_PREFIX, sizeof(ROUTING_TOKEN_PREFIX));
	const char* routing_token = freerdp_nego_get_routing_token(context, &routing_token_length);
	pServerContext* ps = (pServerContext*)context;

	if (routing_token == NULL)
	{
		/* no routing token */
		return FALSE;
	}

	if ((routing_token_length <= prefix_len) || (routing_token_length >= TARGET_MAX))
	{
		LOG_ERR(TAG, ps, "invalid routing token length: %" PRIu32 "", routing_token_length);
		return FALSE;
	}

	len = routing_token_length - prefix_len;
	*target = malloc(len + 1);

	if (!(*target))
		return FALSE;

	CopyMemory(*target, routing_token + prefix_len, len);
	*(*target + len) = '\0';
	colon = strchr(*target, ':');

	if (colon)
	{
		/* port is specified */
		unsigned long p = strtoul(colon + 1, NULL, 10);

		if (p > USHRT_MAX)
		{
			free(*target);
			return FALSE;
		}

		*port = (DWORD)p;
		*colon = '\0';
	}

	return TRUE;
}

static BOOL pf_server_get_target_info(rdpContext* context, rdpSettings* settings,
                                      proxyConfig* config)
{
	pServerContext* ps = (pServerContext*)context;

	LOG_INFO(TAG, ps, "fetching target from %s",
	         config->UseLoadBalanceInfo ? "load-balance-info" : "config");

	if (config->UseLoadBalanceInfo)
		return pf_server_parse_target_from_routing_token(context, &settings->ServerHostname,
		                                                 &settings->ServerPort);

	/* use hardcoded target info from configuration */
	if (!(settings->ServerHostname = _strdup(config->TargetHost)))
	{
		LOG_ERR(TAG, ps, "strdup failed!");
		return FALSE;
	}

	settings->ServerPort = config->TargetPort > 0 ? 3389 : settings->ServerPort;
	return TRUE;
}

/* Event callbacks */
/**
 * This callback is called when the entire connection sequence is done (as
 * described in MS-RDPBCGR section 1.3)
 *
 * The server may start sending graphics output and receiving keyboard/mouse
 * input after this callback returns.
 */
static BOOL pf_server_post_connect(freerdp_peer* peer)
{
	pServerContext* ps;
	pClientContext* pc;
	rdpSettings* client_settings;
	proxyData* pdata;
	ps = (pServerContext*)peer->context;
	pdata = ps->pdata;

	if (pdata->config->SessionCapture && !peer->settings->SupportGraphicsPipeline)
	{
		LOG_ERR(TAG, ps, "Session capture feature is enabled, only accepting connections with GFX");
		return FALSE;
	}

	pc = pf_context_create_client_context(peer->settings);
	if (pc == NULL)
	{
		LOG_ERR(TAG, ps, "[%s]: pf_context_create_client_context failed!");
		return FALSE;
	}

	client_settings = pc->context.settings;

	/* keep both sides of the connection in pdata */
	proxy_data_set_client_context(pdata, pc);

	if (!pf_server_get_target_info(peer->context, client_settings, pdata->config))
	{

		LOG_INFO(TAG, ps, "pf_server_get_target_info failed!");
		return FALSE;
	}

	LOG_INFO(TAG, ps, "remote target is %s:%" PRIu16 "", client_settings->ServerHostname,
	         client_settings->ServerPort);

	if (!pf_server_channels_init(ps))
	{
		LOG_INFO(TAG, ps, "failed to initialize server's channels!");
		return FALSE;
	}

	/* Start a proxy's client in it's own thread */
	if (!(pdata->client_thread = CreateThread(NULL, 0, pf_client_start, pc, 0, NULL)))
	{
		LOG_ERR(TAG, ps, "failed to create client thread");
		return FALSE;
	}

	pf_server_register_input_callbacks(peer->input);
	pf_server_register_update_callbacks(peer->update);
	return pf_modules_run_hook(HOOK_TYPE_SERVER_POST_CONNECT, pdata);
}

static BOOL pf_server_activate(freerdp_peer* peer)
{
	peer->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP8;
	return TRUE;
}

static BOOL pf_server_adjust_monitor_layout(freerdp_peer* peer)
{
	/* proxy as is, there's no need to do anything here */
	return TRUE;
}

static BOOL pf_server_initialize_peer_connection(freerdp_peer* peer)
{
	pServerContext* ps = (pServerContext*)peer->context;
	proxyData* pdata;
	proxyConfig* config;
	rdpSettings* settings = peer->settings;

	if (!ps)
		return FALSE;

	pdata = proxy_data_new();
	if (!pdata)
		return FALSE;

	proxy_data_set_server_context(pdata, ps);
	config = pdata->config = peer->ContextExtra;

	/* currently not supporting GDI orders */
	ZeroMemory(settings->OrderSupport, 32);
	peer->update->autoCalculateBitmapData = FALSE;

	settings->SupportMonitorLayoutPdu = TRUE;
	settings->SupportGraphicsPipeline = config->GFX;
	settings->CertificateFile = _strdup("server.crt");
	settings->PrivateKeyFile = _strdup("server.key");
	settings->RdpKeyFile = _strdup("server.key");

	if (config->RemoteApp)
	{
		settings->RemoteApplicationSupportLevel =
		    RAIL_LEVEL_SUPPORTED | RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED |
		    RAIL_LEVEL_SHELL_INTEGRATION_SUPPORTED | RAIL_LEVEL_LANGUAGE_IME_SYNC_SUPPORTED |
		    RAIL_LEVEL_SERVER_TO_CLIENT_IME_SYNC_SUPPORTED |
		    RAIL_LEVEL_HIDE_MINIMIZED_APPS_SUPPORTED | RAIL_LEVEL_WINDOW_CLOAKING_SUPPORTED |
		    RAIL_LEVEL_HANDSHAKE_EX_SUPPORTED;
		settings->RemoteAppLanguageBarSupported = TRUE;
	}

	if (!settings->CertificateFile || !settings->PrivateKeyFile || !settings->RdpKeyFile)
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		return FALSE;
	}

	settings->RdpSecurity = config->ServerRdpSecurity;
	settings->TlsSecurity = config->ServerTlsSecurity;
	settings->NlaSecurity = FALSE; /* currently NLA is not supported in proxy server */
	settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	settings->ColorDepth = 32;
	settings->SuppressOutput = TRUE;
	settings->RefreshRect = TRUE;
	settings->DesktopResize = TRUE;

	peer->PostConnect = pf_server_post_connect;
	peer->Activate = pf_server_activate;
	peer->AdjustMonitorsLayout = pf_server_adjust_monitor_layout;
	peer->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */
	return TRUE;
}

/**
 * Handles an incoming client connection, to be run in it's own thread.
 *
 * arg is a pointer to a freerdp_peer representing the client.
 */
static DWORD WINAPI pf_server_handle_peer(LPVOID arg)
{
	HANDLE eventHandles[32];
	HANDLE ChannelEvent;
	DWORD eventCount;
	DWORD tmp;
	DWORD status;
	pServerContext* ps;
	rdpContext* pc;
	proxyData* pdata;
	freerdp_peer* client = (freerdp_peer*)arg;

	if (!pf_context_init_server_context(client))
		goto out_free_peer;

	if (!pf_server_initialize_peer_connection(client))
		goto out_free_peer;

	ps = (pServerContext*)client->context;
	pdata = ps->pdata;

	client->Initialize(client);
	LOG_INFO(TAG, ps, "peer connected: %s", client->hostname);
	/* Main client event handling loop */
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(ps->vcm);

	while (1)
	{
		eventCount = 0;
		{
			tmp = client->GetEventHandles(client, &eventHandles[eventCount], 32 - eventCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			eventCount += tmp;
		}
		eventHandles[eventCount++] = ChannelEvent;
		eventHandles[eventCount++] = pdata->abort_event;
		eventHandles[eventCount++] = WTSVirtualChannelManagerGetEventHandle(ps->vcm);
		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (!WTSVirtualChannelManagerCheckFileDescriptor(ps->vcm))
			{
				WLog_ERR(TAG, "WTSVirtualChannelManagerCheckFileDescriptor failure");
				goto fail;
			}
		}

		/* only disconnect after checking client's and vcm's file descriptors  */
		if (proxy_data_shall_disconnect(pdata))
		{
			WLog_INFO(TAG, "abort event is set, closing connection with peer %s", client->hostname);
			break;
		}

		switch (WTSVirtualChannelManagerGetDrdynvcState(ps->vcm))
		{
			/* Dynamic channel status may have been changed after processing */
			case DRDYNVC_STATE_NONE:

				/* Initialize drdynvc channel */
				if (!WTSVirtualChannelManagerCheckFileDescriptor(ps->vcm))
				{
					WLog_ERR(TAG, "Failed to initialize drdynvc channel");
					goto fail;
				}

				break;

			case DRDYNVC_STATE_READY:
				if (WaitForSingleObject(ps->dynvcReady, 0) == WAIT_TIMEOUT)
				{
					SetEvent(ps->dynvcReady);
				}

				break;

			default:
				break;
		}
	}

fail:

	pc = (rdpContext*)pdata->pc;
	LOG_INFO(TAG, ps, "starting shutdown of connection");
	LOG_INFO(TAG, ps, "stopping proxy's client");
	freerdp_client_stop(pc);
	LOG_INFO(TAG, ps, "freeing server's channels");
	pf_server_channels_free(ps);
	LOG_INFO(TAG, ps, "freeing proxy data");
	proxy_data_free(pdata);
	freerdp_client_context_free(pc);
	client->Close(client);
	client->Disconnect(client);
out_free_peer:
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

static BOOL pf_server_peer_accepted(freerdp_listener* listener, freerdp_peer* client)
{
	HANDLE hThread;
	client->ContextExtra = listener->info;

	if (!(hThread = CreateThread(NULL, 0, pf_server_handle_peer, (void*)client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

static void pf_server_mainloop(freerdp_listener* listener)
{
	HANDLE eventHandles[32];
	DWORD eventCount;
	DWORD status;

	while (1)
	{
		eventCount = listener->GetEventHandles(listener, eventHandles, 32);

		if (0 == eventCount)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}

		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			WLog_ERR(TAG, "select failed");
			break;
		}

		if (listener->CheckFileDescriptor(listener) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
	}

	listener->Close(listener);
}

int pf_server_start(proxyConfig* config)
{
	WSADATA wsaData;
	freerdp_listener* listener = freerdp_listener_new();

	if (!listener)
		return -1;

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	listener->info = config;
	listener->PeerAccepted = pf_server_peer_accepted;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		freerdp_listener_free(listener);
		return -1;
	}

	if (listener->Open(listener, config->Host, config->Port))
	{
		pf_server_mainloop(listener);
	}

	freerdp_listener_free(listener);
	WSACleanup();
	return 0;
}
