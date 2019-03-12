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
#include "pf_client.h"
#include "pf_context.h"

#define TAG PROXY_TAG("server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT shadow_client_rdpgfx_caps_advertise(RdpgfxServerContext* context,
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
#ifndef WITH_GFX_H264
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 = !(flags &
				                        RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
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
#ifndef WITH_GFX_H264
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 = !(flags &
				                        RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
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
#ifndef WITH_GFX_H264
				settings->GfxAVC444v2 = settings->GfxAVC444 = settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444 = settings->GfxH264 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
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
#ifndef WITH_GFX_H264
				settings->GfxH264 = FALSE;
				pdu.capsSet->flags &= ~RDPGFX_CAPS_FLAG_AVC420_ENABLED;
#else
				settings->GfxH264 = (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED);
#endif
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

int pf_peer_rdpgfx_init(clientToProxyContext* cContext)
{
	RdpgfxServerContext* gfx;
	gfx = cContext->gfx = rdpgfx_server_context_new(cContext->vcm);

	if (!gfx)
	{
		return 0;
	}

	gfx->rdpcontext = (rdpContext*)cContext;

	gfx->custom = cContext;

	return 1;
}

void pf_server_handle_client_disconnection(freerdp_peer* client)
{
	proxyContext* pContext = (proxyContext*)client->context;
	clientToProxyContext* cContext = (clientToProxyContext*)client->context;
	rdpContext* sContext = pContext->peerContext;
	WLog_INFO(TAG, "Client %s disconnected; closing connection with server %s", client->hostname,
	          sContext->settings->ServerHostname);
	WLog_INFO(TAG, "connectionClosed event is not set; closing connection to remote server");
	/* Mark connection closed for sContext */
	SetEvent(pContext->connectionClosed);
	freerdp_abort_connect(sContext->instance);
	/* Close connection to remote host */
	WLog_DBG(TAG, "Waiting for proxy's client thread to finish");
	WaitForSingleObject(cContext->thread, INFINITE);
	CloseHandle(cContext->thread);
	freerdp_client_stop(sContext);
	freerdp_client_context_free(sContext);
}

static BOOL pf_server_parse_target_from_routing_token(freerdp_peer* client,
        char** target, DWORD* port)
{
#define TARGET_MAX	(100)
	rdpNego* nego = client->context->rdp->nego;

	if (nego->RoutingToken &&
	    nego->RoutingTokenLength > 0 && nego->RoutingTokenLength < TARGET_MAX)
	{
		*target = _strdup((char*)nego->RoutingToken);
		char* colon = strchr(*target, ':');

		if (colon)
		{
			// port is specified
			*port = strtoul(colon + 1, NULL, 10);
			*colon = '\0';
		}

		return TRUE;
	}

	// no routing token.
	return FALSE;
}

/* Event callbacks */

/**
 * This callback is called when the entire connection sequence is done (as described in
 * MS-RDPBCGR section 1.3)
 *
 * The server may start sending graphics output and receiving keyboard/mouse input after this
 * callback returns.
 */
BOOL pf_peer_post_connect(freerdp_peer* client)
{
	proxyContext* pContext = (proxyContext*) client->context;

	char* host = NULL;
	DWORD port = 3389; // default port

	if (!pf_server_parse_target_from_routing_token(client, &host, &port))
	{
		WLog_ERR(TAG, "pf_server_parse_target_from_routing_token failed!");
		return FALSE;
	}

	char* username = _strdup(client->settings->Username);
	char* password = _strdup(client->settings->Password);
	
	/* Start a proxy's client in it's own thread */
	rdpContext* sContext = proxy_to_server_context_create(client->context,
	                       host, port, username, password);
	/* Inject proxy's client context to proxy's context */
	HANDLE connectionClosedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	pContext->peerContext = sContext;
	pContext->connectionClosed = connectionClosedEvent;
	((proxyContext*)sContext)->peerContext = (rdpContext*)pContext;
	((proxyContext*)sContext)->connectionClosed = connectionClosedEvent;
	clientToProxyContext* cContext = (clientToProxyContext*)client->context;

	pf_peer_rdpgfx_init(cContext);

	if (!(cContext->thread = CreateThread(NULL, 0, proxy_client_start, sContext, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		return FALSE;
	}

	return TRUE;
}


BOOL pf_peer_activate(freerdp_peer* client)
{
	client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP61;
	return TRUE;
}

BOOL pf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	WLog_DBG(TAG, "Client sent a synchronize event (flags:0x%"PRIX32")", flags);
	return TRUE;
}

BOOL pf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	proxyContext* context = (proxyContext*)input->context;
	return freerdp_input_send_keyboard_event(context->peerContext->input, flags, code);
}

BOOL pf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	proxyContext* context = (proxyContext*)input->context;
	return freerdp_input_send_unicode_keyboard_event(context->peerContext->input, flags, code);
}

BOOL pf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	proxyContext* context = (proxyContext*)input->context;
	return freerdp_input_send_mouse_event(context->peerContext->input, flags, x, y);
}

BOOL pf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x,
                                  UINT16 y)
{
	proxyContext* context = (proxyContext*)input->context;
	return freerdp_input_send_extended_mouse_event(context->peerContext->input, flags, x, y);
}

static BOOL pf_peer_refresh_rect(rdpContext* context, BYTE count,
                                 const RECTANGLE_16* areas)
{
	BYTE i;
	WLog_DBG(TAG, "Client requested to refresh:");

	for (i = 0; i < count; i++)
	{
		WLog_DBG(TAG, "  (%"PRIu16", %"PRIu16") (%"PRIu16", %"PRIu16")", areas[i].left, areas[i].top,
		         areas[i].right, areas[i].bottom);
	}

	return TRUE;
}

static BOOL pf_peer_suppress_output(rdpContext* context, BYTE allow,
                                    const RECTANGLE_16* area)
{
	if (allow > 0)
	{
		WLog_DBG(TAG, "Client restore output (%"PRIu16", %"PRIu16") (%"PRIu16", %"PRIu16").", area->left,
		         area->top,
		         area->right, area->bottom);
	}
	else
	{
		WLog_DBG(TAG, "Client minimized and suppress output.");
	}

	return TRUE;
}

static BOOL init_client_context(freerdp_peer* client)
{
	client->ContextSize = sizeof(clientToProxyContext);
	client->ContextNew = (psPeerContextNew) client_to_proxy_context_new;
	client->ContextFree = (psPeerContextFree) client_to_proxy_context_free;
	return freerdp_peer_context_new(client);
}

/**
 * Handles an incoming client connection, to be run in it's own thread.
 *
 * arg is a pointer to a freerdp_peer representing the client.
 */
static DWORD WINAPI handle_client(LPVOID arg)
{
	freerdp_peer* client = (freerdp_peer*) arg;

	if (!init_client_context(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	client->settings->SupportGraphicsPipeline = TRUE;
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

	client->settings->RdpSecurity = TRUE;
	client->settings->TlsSecurity = TRUE;
	client->settings->NlaSecurity = FALSE;
	client->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_HIGH; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_LOW; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_FIPS; */
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;
	client->PostConnect = pf_peer_post_connect;
	client->Activate = pf_peer_activate;
	client->input->SynchronizeEvent = pf_peer_synchronize_event;
	client->input->KeyboardEvent = pf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = pf_peer_unicode_keyboard_event;
	client->input->MouseEvent = pf_peer_mouse_event;
	client->input->ExtendedMouseEvent = pf_peer_extended_mouse_event;
	client->update->RefreshRect = pf_peer_refresh_rect;
	client->update->SuppressOutput = pf_peer_suppress_output;
	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */
	client->Initialize(client);
	clientToProxyContext* context;
	context = (clientToProxyContext*) client->context;
	proxyContext* pContext = (proxyContext*)context;
	WLog_INFO(TAG, "Client connected: %s",
	          client->local ? "(local)" : client->hostname);
	/* Main client event handling loop */
	HANDLE eventHandles[32];
	HANDLE ChannelEvent;
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(context->vcm);
	BOOL gfxOpened = FALSE;

	while (1)
	{
		DWORD eventCount = 0;
		{
			DWORD tmp = client->GetEventHandles(client, &eventHandles[eventCount], 32 - eventCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			eventCount += tmp;
		}
		eventHandles[eventCount++] = ChannelEvent;
		eventHandles[eventCount++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		DWORD status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			/* If the wait failed because the connection closed by the proxy, that's ok */
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

				/* Call this routine to Initialize drdynvc channel */
				if (!WTSVirtualChannelManagerCheckFileDescriptor(context->vcm))
				{
					WLog_ERR(TAG, "Failed to initialize drdynvc channel");
					goto fail;
				}

				break;

			case DRDYNVC_STATE_READY:

				/* Init RDPGFX dynamic channel */
				if (client->settings->SupportGraphicsPipeline && context->gfx &&
				    !gfxOpened)
				{
					context->gfx->CapsAdvertise = shadow_client_rdpgfx_caps_advertise;

					if (!context->gfx->Open(context->gfx))
					{
						WLog_WARN(TAG, "Failed to open GraphicsPipeline");
						client->settings->SupportGraphicsPipeline = FALSE;
					}

					gfxOpened = TRUE;
					WLog_INFO(TAG, "Gfx Pipeline Opened");
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

	if (!pf_common_connection_aborted_by_peer(pContext))
	{
		pf_server_handle_client_disconnection(client);
	}
	else
	{
		WLog_INFO(TAG, "Connection already aborted; client potentially kicked by the server");
	}

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

static BOOL client_connected(freerdp_listener* listener, freerdp_peer* client)
{
	HANDLE hThread;

	if (!(hThread = CreateThread(NULL, 0, handle_client, (void*) client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

void server_mainloop(freerdp_listener* listener)
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

int proxy_server_start(char* host, long port, BOOL localOnly)
{
	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	freerdp_listener* listener = freerdp_listener_new();

	if (!listener)
	{
		return -1;
	}

	listener->PeerAccepted = client_connected;
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		freerdp_listener_free(listener);
		return -1;
	}

	/* Determine filepath for local socket */
	char* localSockPath;
	char localSockName[MAX_PATH];
	sprintf_s(localSockName, sizeof(localSockName), "proxy.%ld", port);
	localSockPath = GetKnownSubPath(KNOWN_PATH_TEMP, localSockName);

	if (!localSockPath)
	{
		freerdp_listener_free(listener);
		WSACleanup();
		return -1;
	}

	/* Listen to local connections */
	BOOL success = listener->OpenLocal(listener, localSockPath);

	/* Listen to remote connections */
	if (!localOnly)
	{
		success &= listener->Open(listener, host, port);
	}

	if (success)
	{
		server_mainloop(listener);
	}

	free(localSockPath);
	freerdp_listener_free(listener);
	WSACleanup();
	return 0;
}