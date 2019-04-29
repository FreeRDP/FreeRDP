/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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
#include <libfreerdp/core/listener.h>

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
#include "proxy.h"

#define TAG PROXY_TAG("server")

/**
 * Confirms client capabilities on caps advertised received.
 * The server
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT pf_server_rdpgfx_caps_advertise(RdpgfxServerContext* context,
        RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	UINT16 index;
	rdpSettings* settings = context->rdpcontext->settings;
	UINT32 flags = 0;

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_103)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 =
				                            !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_102)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 =
				                            !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_10)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxAVC444 = settings->GfxH264 = !(flags &
				                      RDPGFX_CAPS_FLAG_AVC_DISABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_81)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxAVC444v2 = settings->GfxAVC444 = FALSE;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
				settings->GfxH264 = (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_8)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	return CHANNEL_RC_UNSUPPORTED_VERSION;
}

BOOL pf_server_rdpgfx_init(clientToProxyContext* cContext)
{
	RdpgfxServerContext* gfx;
	gfx = cContext->gfx = rdpgfx_server_context_new(cContext->vcm);

	if (!gfx)
	{
		return FALSE;
	}

	gfx->rdpcontext = (rdpContext*)cContext;
	return TRUE;
}

void pf_server_handle_client_disconnection(freerdp_peer* client)
{
	proxyContext* pContext = (proxyContext*)client->context;
	clientToProxyContext* cContext = (clientToProxyContext*)client->context;
	rdpContext* sContext = pContext->peerContext;
	WLog_INFO(TAG, "Client %s disconnected; closing connection with server %s",
	          client->hostname, sContext->settings->ServerHostname);
	/* Mark connection closed for sContext */
	SetEvent(pContext->connectionClosed);
	freerdp_abort_connect(sContext->instance);
	/* Close connection to remote host */
	WLog_DBG(TAG, "Waiting for proxy's client thread to finish");
	WaitForSingleObject(cContext->thread, INFINITE);
	CloseHandle(cContext->thread);
}

static BOOL pf_server_parse_target_from_routing_token(freerdp_peer* client,
        char** target, DWORD* port)
{
#define TARGET_MAX	(100)
#define ROUTING_TOKEN_PREFIX "Cookie: msts="
	rdpNego* nego;
	char* colon;
	int len;
	int prefix_len;

	nego = client->context->rdp->nego;
	prefix_len = strlen(ROUTING_TOKEN_PREFIX);

	if (nego->RoutingToken &&
	    nego->RoutingTokenLength > prefix_len && nego->RoutingTokenLength < TARGET_MAX)
	{
		len = nego->RoutingTokenLength - prefix_len;
		*target = malloc(len + 1);
		CopyMemory(*target, nego->RoutingToken + len, nego->RoutingTokenLength - len);
		*(*target + len) = '\0';
		colon = strchr(*target, ':');

		if (colon)
		{
			/* port is specified */
			*port = strtoul(colon + 1, NULL, 10);
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
BOOL pf_server_post_connect(freerdp_peer* client)
{
	rdpContext* sContext;
	proxyConfig* config;
	clientToProxyContext* cContext;
	HANDLE connectionClosedEvent;
	proxyContext* pContext;
	pContext = (proxyContext*) client->context;
	config = pContext->server->config;
	char* host = NULL;
	DWORD port = 3389; // default port

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

	/* Start a proxy's client in it's own thread */
	sContext = proxy_to_server_context_create(client->settings, host, port);
	/* Inject proxy's client context to proxy's context */
	connectionClosedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	pContext->peerContext = sContext;
	pContext->connectionClosed = connectionClosedEvent;
	((proxyContext*)sContext)->peerContext = (rdpContext*)pContext;
	((proxyContext*)sContext)->connectionClosed = connectionClosedEvent;
	cContext = (clientToProxyContext*)client->context;
	pf_server_rdpgfx_init(cContext);

	if (!(cContext->thread = CreateThread(NULL, 0, pf_client_start, sContext, 0,
	                                      NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		return FALSE;
	}

	return TRUE;
}

BOOL pf_server_activate(freerdp_peer* client)
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
	BOOL gfxOpened = FALSE;
	DWORD eventCount;
	DWORD tmp;
	DWORD status;
	freerdp_peer* client = (freerdp_peer*) arg;
	clientToProxyContext* context;
	proxyContext* pContext;

	if (!init_client_to_proxy_context(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	context = (clientToProxyContext*) client->context;
	pContext = (proxyContext*)context;
	/* inject server struct to proxyContext */
	pContext->server = client->ContextExtra;
	client->settings->SupportGraphicsPipeline = TRUE;
	client->settings->SupportDynamicChannels = TRUE;
	/* TODO: Read path from config and default to /etc */
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

	/* TODO: Read security settings from config */
	client->settings->RdpSecurity = TRUE;
	client->settings->TlsSecurity = TRUE;
	client->settings->NlaSecurity = FALSE;
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
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(context->vcm);

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
		eventHandles[eventCount++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			/* Ignore wait fails that are caused by legitimate client disconnections */
			if (pf_common_connection_aborted_by_peer(pContext))
				break;

			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (!WTSVirtualChannelManagerCheckFileDescriptor(context->vcm))
			{
				WLog_ERR(TAG, "WTSVirtualChannelManagerCheckFileDescriptor failure");
				goto fail;
			}
		}

		switch (WTSVirtualChannelManagerGetDrdynvcState(context->vcm))
		{
			/* Dynamic channel status may have been changed after processing */
			case DRDYNVC_STATE_NONE:

				/* Initialize drdynvc channel */
				if (!WTSVirtualChannelManagerCheckFileDescriptor(context->vcm))
				{
					WLog_ERR(TAG, "Failed to initialize drdynvc channel");
					goto fail;
				}

				break;

			case DRDYNVC_STATE_READY:

				/* Initialize RDPGFX dynamic channel */
				if (!gfxOpened && client->settings->SupportGraphicsPipeline &&
				    context->gfx)
				{
					if (!context->gfx->Open(context->gfx))
					{
						WLog_WARN(TAG, "Failed to open GraphicsPipeline");
						client->settings->SupportGraphicsPipeline = FALSE;
						break;
					}

					gfxOpened = TRUE;
					context->gfx->CapsAdvertise = pf_server_rdpgfx_caps_advertise;
					WLog_DBG(TAG, "Gfx Pipeline Opened");
				}

				break;

			default:
				break;
		}
	}

fail:

	if (gfxOpened)
	{
		(void)context->gfx->Close(context->gfx);
	}

	if (client->connected && !pf_common_connection_aborted_by_peer(pContext))
	{
		pf_server_handle_client_disconnection(client);
	}

	freerdp_client_stop(pContext->peerContext);
	freerdp_client_context_free(pContext->peerContext);
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

void pf_server_mainloop(freerdp_listener* listener)
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

int pf_server_start(rdpProxyServer* server)
{
	char* localSockPath;
	char localSockName[MAX_PATH];
	BOOL success;
	WSADATA wsaData;
	proxyConfig* config;
	freerdp_listener* listener = freerdp_listener_new();

	if (!listener)
		return -1;

	config = server->config;
	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	listener->info = server;
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
