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
#include "pf_channels.h"
#include "pf_rdpgfx.h"
#include "pf_disp.h"
#include "pf_channels.h"

#define TAG PROXY_TAG("server")

static BOOL pf_server_parse_target_from_routing_token(rdpContext* context,
        char** target, DWORD* port)
{
#define TARGET_MAX	(100)
#define ROUTING_TOKEN_PREFIX "Cookie: msts="
	char* colon;
	size_t len;
	const size_t prefix_len  = strlen(ROUTING_TOKEN_PREFIX);
	DWORD routing_token_length;
	const char* routing_token = freerdp_nego_get_routing_token(context, &routing_token_length);

	if (routing_token == NULL)
	{
		/* no routing token */
		return FALSE;
	}

	if ((routing_token_length <= prefix_len) || (routing_token_length >= TARGET_MAX))
	{
		WLog_ERR(TAG, "pf_server_parse_target_from_routing_token(): invalid routing token length: %"PRIu32"",
		         routing_token_length);
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
	WLog_INFO(TAG, "pf_server_get_target_info(): fetching target from %s",
		config->UseLoadBalanceInfo ? "load-balance-info" : "config");

	if (config->UseLoadBalanceInfo)
		return pf_server_parse_target_from_routing_token(context, &settings->ServerHostname,
		                                                 &settings->ServerPort);

	/* use hardcoded target info from configuration */
	if (!(settings->ServerHostname = _strdup(config->TargetHost)))
	{
		WLog_DBG(TAG, "pf_server_get_target_info(): strdup failed!");
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
static BOOL pf_server_post_connect(freerdp_peer* client)
{
	pServerContext* ps;
	rdpContext* pc;
	proxyData* pdata;
	ps = (pServerContext*)client->context;
	pdata = ps->pdata;

	pc = p_client_context_create(client->settings);
	if (pc == NULL)
	{
		WLog_ERR(TAG, "pf_server_post_connect(): p_client_context_create failed!");
		return FALSE;
	}

	/* keep both sides of the connection in pdata */
	((pClientContext*)pc)->pdata = ps->pdata;
	pdata->pc = (pClientContext*)pc;

	if (!pf_server_get_target_info(client->context, pc->settings, pdata->config))
	{
		WLog_ERR(TAG, "pf_server_post_connect(): pf_server_get_target_info failed!");
		return FALSE;
	}

	WLog_INFO(TAG, "pf_server_post_connect(): target == %s:%"PRIu16"", pc->settings->ServerHostname,
	      pc->settings->ServerPort);

	if (!pf_server_channels_init(ps))
	{
		WLog_ERR(TAG, "pf_server_post_connect(): failed to initialize server's channels!");
		return FALSE;
	}

	/* Start a proxy's client in it's own thread */
	if (!(pdata->client_thread = CreateThread(NULL, 0, pf_client_start, pc, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		return FALSE;
	}

	return TRUE;
}

static BOOL pf_server_activate(freerdp_peer* client)
{
	client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP8;
	return TRUE;
}

static BOOL pf_server_adjust_monitor_layout(freerdp_peer* peer)
{
	/* proxy as is, there's no need to do anything here */
	return TRUE;
}

/**
 * Handles an incoming client connection, to be run in it's own thread.
 *
 * arg is a pointer to a freerdp_peer representing the client.
 */
static DWORD WINAPI pf_server_handle_client(LPVOID arg)
{
	HANDLE eventHandles[32];
	HANDLE ChannelEvent;
	DWORD eventCount;
	DWORD tmp;
	DWORD status;
	pServerContext* ps;
	rdpContext* pc;
	proxyData* pdata;
	proxyConfig* config;
	freerdp_peer* client = (freerdp_peer*)arg;

	if (!init_p_server_context(client))
		goto out_free_peer;

	ps = (pServerContext*)client->context;
	if (!(ps->dynvcReady = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "pf_server_post_connect(): CreateEvent failed!");
		goto out_free_peer;
	}

	if (!(pdata = ps->pdata = proxy_data_new()))
	{
		WLog_ERR(TAG, "pf_server_post_connect(): proxy_data_new failed!");
		goto out_free_peer;
	}

	/* currently not supporting GDI orders */
	ZeroMemory(client->settings->OrderSupport, 32);
	client->update->autoCalculateBitmapData = FALSE;
	pdata->ps = ps;
	/* keep configuration in proxyData */
	pdata->config = client->ContextExtra;
	config = pdata->config;
	client->settings->UseMultimon = TRUE;
	client->settings->AudioPlayback = FALSE;
	client->settings->DeviceRedirection = TRUE;
	client->settings->SupportGraphicsPipeline = config->GFX;
	client->settings->SupportDynamicChannels = TRUE;
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->RdpKeyFile = _strdup("server.key");

	if (!client->settings->CertificateFile || !client->settings->PrivateKeyFile ||
	    !client->settings->RdpKeyFile)
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		goto out_free_peer;
	}

	client->settings->SupportDisplayControl = config->DisplayControl;
	client->settings->DynamicResolutionUpdate = config->DisplayControl;
	client->settings->SupportMonitorLayoutPdu = TRUE;
	client->settings->RdpSecurity = config->RdpSecurity;
	client->settings->TlsSecurity = config->TlsSecurity;
	client->settings->NlaSecurity = config->NlaSecurity;
	client->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;
	client->settings->DesktopResize = TRUE;
	client->PostConnect = pf_server_post_connect;
	client->Activate = pf_server_activate;
	client->AdjustMonitorsLayout = pf_server_adjust_monitor_layout;
	pf_server_register_input_callbacks(client->input);
	pf_server_register_update_callbacks(client->update);
	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */
	client->Initialize(client);
	WLog_INFO(TAG, "Client connected: %s", client->local ? "(local)" : client->hostname);
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
			WLog_INFO(TAG, "abort_event is set, closing connection with client %s", client->hostname);
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

	pc = (rdpContext*) pdata->pc;
	WLog_INFO(TAG, "pf_server_handle_client(): starting shutdown of connection (client %s)", client->hostname);
	WLog_INFO(TAG, "pf_server_handle_client(): stopping proxy's client");
	freerdp_client_stop(pc);
	WLog_INFO(TAG, "pf_server_handle_client(): freeing server's channels");
	pf_server_channels_free(ps);
	WLog_INFO(TAG, "pf_server_handle_client(): freeing proxy data");
	proxy_data_free(pdata);
	freerdp_client_context_free(pc);
	client->Close(client);
	client->Disconnect(client);
out_free_peer:
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

static BOOL pf_server_client_connected(freerdp_listener* listener, freerdp_peer* client)
{
	HANDLE hThread;
	client->ContextExtra = listener->info;

	if (!(hThread = CreateThread(NULL, 0, pf_server_handle_client, (void*)client, 0, NULL)))
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
	char* localSockPath;
	char localSockName[MAX_PATH];
	BOOL success;
	WSADATA wsaData;
	freerdp_listener* listener = freerdp_listener_new();

	if (!listener)
		return -1;

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	listener->info = config;
	listener->PeerAccepted = pf_server_client_connected;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		freerdp_listener_free(listener);
		return -1;
	}

	/* Determine filepath for local socket */
	sprintf_s(localSockName, sizeof(localSockName), "proxy.%" PRIu16 "", config->Port);
	localSockPath = GetKnownSubPath(KNOWN_PATH_TEMP, localSockName);

	if (!localSockPath)
	{
		freerdp_listener_free(listener);
		WSACleanup();
		return -1;
	}

	/* Listen to local connections */
	success = listener->OpenLocal(listener, localSockPath);

	/* Listen to remote connections */
	if (!config->LocalOnly)
	{
		success &= listener->Open(listener, config->Host, config->Port);
	}

	if (success)
	{
		pf_server_mainloop(listener);
	}

	free(localSockPath);
	freerdp_listener_free(listener);
	WSACleanup();
	return 0;
}
