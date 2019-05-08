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
#include "pf_common.h"
#include "pf_log.h"
#include "pf_config.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_input.h"
#include "pf_update.h"
#include "pf_rdpgfx.h"

#define TAG PROXY_TAG("server")

static void pf_server_handle_client_disconnection(freerdp_peer* client)
{
	pServerContext* ps;
	proxyData* pdata;
	rdpContext* pc;
	ps = (pServerContext*)client->context;
	pc = (rdpContext*) ps->pdata->pc;
	pdata = ps->pdata;
	WLog_INFO(TAG, "Client %s disconnected; closing connection with server %s",
	          client->hostname, pc->settings->ServerHostname);
	/* Mark connection closed for sContext */
	SetEvent(pdata->connectionClosed);
	freerdp_abort_connect(pc->instance);
	/* Close connection to remote host */
	WLog_DBG(TAG, "Waiting for proxy's client thread to finish");
	WaitForSingleObject(ps->thread, INFINITE);
	CloseHandle(ps->thread);
}

static BOOL pf_server_parse_target_from_routing_token(freerdp_peer* client,
        char** target, DWORD* port)
{
#define TARGET_MAX	(100)
#define ROUTING_TOKEN_PREFIX "Cookie: msts="
	char* colon;
	size_t len;
	const size_t prefix_len  = strlen(ROUTING_TOKEN_PREFIX);
	DWORD routing_token_length;
	const char* routing_token = freerdp_nego_get_routing_token(client->context, &routing_token_length);

	if (routing_token &&
	    (routing_token_length > prefix_len) && (routing_token_length < TARGET_MAX))
	{
		len = routing_token_length - prefix_len;
		*target = malloc(len + 1);

		if (!(*target))
			return FALSE;

		CopyMemory(*target, routing_token + prefix_len, len);
		*(*target + len) = '\0';
		colon = strchr(*target, ':');
		WLog_INFO(TAG, "Target [parsed from routing token]: %s", *target);

		if (colon)
		{
			/* port is specified */
			unsigned long p = strtoul(colon + 1, NULL, 10);

			if (p > USHRT_MAX)
				return FALSE;

			*port = (DWORD)p;
			*colon = '\0';
		}

		return TRUE;
	}

	/* no routing token */
	return FALSE;
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
	proxyConfig* config;
	pServerContext* ps;
	pClientContext* pc;
	HANDLE connectionClosedEvent;
	proxyData* pdata;
	char* host = NULL;
	DWORD port = 3389; /* default port */
	ps = (pServerContext*)client->context;
	pdata = ps->pdata;
	config = pdata->config;

	if (config->UseLoadBalanceInfo)
	{
		if (!pf_server_parse_target_from_routing_token(client, &host, &port))
		{
			WLog_ERR(TAG, "pf_server_parse_target_from_routing_token failed!");
			return FALSE;
		}

		WLog_DBG(TAG, "Parsed target from load-balance-info: %s:%i", host, port);
	}
	else
	{
		/* use hardcoded target info from configuration */
		host = _strdup(config->TargetHost);
		port = config->TargetPort > 0 ? config->TargetPort : port;
		WLog_DBG(TAG, "Using hardcoded target host: %s:%i", host, port);
	}

	pc = (pClientContext*) p_client_context_create(client->settings, host, port);
	connectionClosedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	/* keep both sides of the connection in pdata */
	pc->pdata = ps->pdata;
	pdata->pc = (pClientContext*) pc;
	pdata->ps = ps;
	pdata->connectionClosed = connectionClosedEvent;
	pf_server_rdpgfx_init(ps);

	/* Start a proxy's client in it's own thread */
	if (!(ps->thread = CreateThread(NULL, 0, pf_client_start, (rdpContext*) pc, 0,
	                                NULL)))
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
	freerdp_peer* client = (freerdp_peer*) arg;

	if (!init_p_server_context(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	ps = (pServerContext*) client->context;
	ps->dynvcReady = CreateEvent(NULL, TRUE, FALSE, NULL);
	pdata = calloc(1, sizeof(proxyData));
	ps->pdata = pdata;
	/* keep configuration in proxyData */
	pdata->config = client->ContextExtra;
	config = pdata->config;
	client->settings->SupportGraphicsPipeline = config->GFX;
	client->settings->SupportDynamicChannels = TRUE;
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->RdpKeyFile = _strdup("server.key");

	if (!client->settings->CertificateFile || !client->settings->PrivateKeyFile
	    || !client->settings->RdpKeyFile)
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		freerdp_peer_free(client);
		return 0;
	}

	client->settings->RdpSecurity = config->RdpSecurity;
	client->settings->TlsSecurity = config->TlsSecurity;
	client->settings->NlaSecurity = config->NlaSecurity;
	client->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;
	client->PostConnect = pf_server_post_connect;
	client->Activate = pf_server_activate;
	pf_server_register_input_callbacks(client->input);
	pf_server_register_update_callbacks(client->update);
	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */
	client->Initialize(client);
	WLog_INFO(TAG, "Client connected: %s",
	          client->local ? "(local)" : client->hostname);
	/* Main client event handling loop */
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(ps->vcm);

	while (1)
	{
		eventCount = 0;
		{
			tmp = client->GetEventHandles(client, &eventHandles[eventCount],
			                              32 - eventCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			eventCount += tmp;
		}
		eventHandles[eventCount++] = ChannelEvent;
		eventHandles[eventCount++] = WTSVirtualChannelManagerGetEventHandle(ps->vcm);
		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			/* Ignore wait fails that are caused by legitimate client disconnections */
			if (pf_common_connection_aborted_by_peer(pdata))
				break;

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

	if (client->connected && !pf_common_connection_aborted_by_peer(pdata))
	{
		pf_server_handle_client_disconnection(client);
	}

	pc = (rdpContext*) pdata->pc;
	freerdp_client_stop(pc);
	free(pdata);
	freerdp_client_context_free(pc);
	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

static BOOL pf_server_client_connected(freerdp_listener* listener,
                                       freerdp_peer* client)
{
	HANDLE hThread;
	client->ContextExtra = listener->info;

	if (!(hThread = CreateThread(NULL, 0, pf_server_handle_client,
	                             (void*) client, 0, NULL)))
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
	sprintf_s(localSockName, sizeof(localSockName), "proxy.%"PRIu16"", config->Port);
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
