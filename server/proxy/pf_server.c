/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/winsock.h>
#include <winpr/thread.h>
#include <errno.h>

#include <freerdp/freerdp.h>
#include <freerdp/streamdump.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/build-config.h>

#include <freerdp/channels/rdpdr.h>

#include <freerdp/server/proxy/proxy_server.h>
#include <freerdp/server/proxy/proxy_log.h>

#include "pf_server.h"
#include "pf_channel.h"
#include <freerdp/server/proxy/proxy_config.h>
#include "pf_client.h"
#include <freerdp/server/proxy/proxy_context.h>
#include "pf_update.h"
#include "proxy_modules.h"
#include "pf_utils.h"
#include "channels/pf_channel_drdynvc.h"
#include "channels/pf_channel_rdpdr.h"

#define TAG PROXY_TAG("server")

typedef struct
{
	HANDLE thread;
	freerdp_peer* client;
} peer_thread_args;

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

	if (!routing_token)
		return FALSE;

	if ((routing_token_length <= prefix_len) || (routing_token_length >= TARGET_MAX))
	{
		PROXY_LOG_ERR(TAG, ps, "invalid routing token length: %" PRIu32 "", routing_token_length);
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
                                      const proxyConfig* config)
{
	pServerContext* ps = (pServerContext*)context;
	proxyFetchTargetEventInfo ev = { 0 };

	WINPR_ASSERT(settings);
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);

	ev.fetch_method = config->FixedTarget ? PROXY_FETCH_TARGET_METHOD_CONFIG
	                                      : PROXY_FETCH_TARGET_METHOD_LOAD_BALANCE_INFO;

	if (!pf_modules_run_filter(ps->pdata->module, FILTER_TYPE_SERVER_FETCH_TARGET_ADDR, ps->pdata,
	                           &ev))
		return FALSE;

	switch (ev.fetch_method)
	{
		case PROXY_FETCH_TARGET_METHOD_DEFAULT:
		case PROXY_FETCH_TARGET_METHOD_LOAD_BALANCE_INFO:
			return pf_server_parse_target_from_routing_token(context, &settings->ServerHostname,
			                                                 &settings->ServerPort);

		case PROXY_FETCH_TARGET_METHOD_CONFIG:
		{
			WINPR_ASSERT(config);

			if (config->TargetPort > 0)
				freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, config->TargetPort);
			else
				freerdp_settings_set_uint32(settings, FreeRDP_ServerPort, 3389);

			if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, config->TargetHost))
			{
				PROXY_LOG_ERR(TAG, ps, "strdup failed!");
				return FALSE;
			}

			if (config->TargetUser)
				freerdp_settings_set_string(settings, FreeRDP_Username, config->TargetUser);

			if (config->TargetDomain)
				freerdp_settings_set_string(settings, FreeRDP_Domain, config->TargetDomain);

			if (config->TargetPassword)
				freerdp_settings_set_string(settings, FreeRDP_Password, config->TargetPassword);

			return TRUE;
		}
		case PROXY_FETCH_TARGET_USE_CUSTOM_ADDR:
		{
			if (!ev.target_address)
			{
				WLog_ERR(TAG, "router: using CUSTOM_ADDR fetch method, but target_address == NULL");
				return FALSE;
			}

			if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname, ev.target_address))
			{
				PROXY_LOG_ERR(TAG, ps, "strdup failed!");
				return FALSE;
			}

			free(ev.target_address);
			settings->ServerPort = ev.target_port;
			return TRUE;
		}
		default:
			WLog_WARN(TAG, "unknown target fetch method: %d", ev.fetch_method);
			return FALSE;
	}

	return TRUE;
}

static BOOL pf_server_setup_channels(freerdp_peer* peer)
{
	char** accepted_channels = NULL;
	size_t accepted_channels_count;
	size_t i;
	pServerContext* ps = (pServerContext*)peer->context;
	wHashTable* byId = ps->channelsById;

	accepted_channels = WTSGetAcceptedChannelNames(peer, &accepted_channels_count);
	if (!accepted_channels)
		return TRUE;

	for (i = 0; i < accepted_channels_count; i++)
	{
		pServerStaticChannelContext* channelContext;
		const char* cname = accepted_channels[i];
		UINT16 channelId = WTSChannelGetId(peer, cname);

		PROXY_LOG_INFO(TAG, ps, "Accepted channel: %s (%d)", cname, channelId);
		channelContext = StaticChannelContext_new(ps, cname, channelId);
		if (!channelContext)
		{
			PROXY_LOG_ERR(TAG, ps, "error seting up channelContext for '%s'", cname);
			return FALSE;
		}

		if (strcmp(cname, DRDYNVC_SVC_CHANNEL_NAME) == 0)
		{
			if (!pf_channel_setup_drdynvc(ps->pdata, channelContext))
			{
				PROXY_LOG_ERR(TAG, ps, "error while setting up dynamic channel");
				StaticChannelContext_free(channelContext);
				return FALSE;
			}
		}
		else if (strcmp(cname, RDPDR_SVC_CHANNEL_NAME) == 0 &&
		         (channelContext->channelMode == PF_UTILS_CHANNEL_INTERCEPT))
		{
			if (!pf_channel_setup_rdpdr(ps, channelContext))
			{
				PROXY_LOG_ERR(TAG, ps, "error while setting up redirection channel");
				StaticChannelContext_free(channelContext);
				return FALSE;
			}
		}
		else
		{
			if (!pf_channel_setup_generic(channelContext))
			{
				PROXY_LOG_ERR(TAG, ps, "error while setting up generic channel");
				StaticChannelContext_free(channelContext);
				return FALSE;
			}
		}

		if (!HashTable_Insert(byId, &channelContext->channel_id, channelContext))
		{
			StaticChannelContext_free(channelContext);
			PROXY_LOG_ERR(TAG, ps, "error inserting channelContext in byId table for '%s'", cname);
			return FALSE;
		}
	}

	free(accepted_channels);
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
	rdpSettings* frontSettings;

	WINPR_ASSERT(peer);

	ps = (pServerContext*)peer->context;
	WINPR_ASSERT(ps);

	frontSettings = peer->context->settings;
	WINPR_ASSERT(frontSettings);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);

	PROXY_LOG_INFO(TAG, ps, "Accepted client: %s", frontSettings->ClientHostname);
	if (!pf_server_setup_channels(peer))
	{
		PROXY_LOG_ERR(TAG, ps, "error setting up channels");
		return FALSE;
	}

	pc = pf_context_create_client_context(frontSettings);
	if (pc == NULL)
	{
		PROXY_LOG_ERR(TAG, ps, "failed to create client context!");
		return FALSE;
	}

	client_settings = pc->context.settings;

	/* keep both sides of the connection in pdata */
	proxy_data_set_client_context(pdata, pc);

	if (!pf_server_get_target_info(peer->context, client_settings, pdata->config))
	{
		PROXY_LOG_INFO(TAG, ps, "pf_server_get_target_info failed!");
		return FALSE;
	}

	PROXY_LOG_INFO(TAG, ps, "remote target is %s:%" PRIu16 "", client_settings->ServerHostname,
	               client_settings->ServerPort);

	if (!pf_modules_run_hook(pdata->module, HOOK_TYPE_SERVER_POST_CONNECT, pdata, peer))
		return FALSE;

	/* Start a proxy's client in it's own thread */
	if (!(pdata->client_thread = CreateThread(NULL, 0, pf_client_start, pc, 0, NULL)))
	{
		PROXY_LOG_ERR(TAG, ps, "failed to create client thread");
		return FALSE;
	}

	return TRUE;
}

static BOOL pf_server_activate(freerdp_peer* peer)
{
	pServerContext* ps;
	proxyData* pdata;
	rdpSettings* settings;

	WINPR_ASSERT(peer);

	ps = (pServerContext*)peer->context;
	WINPR_ASSERT(ps);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP8;
	if (!pf_modules_run_hook(pdata->module, HOOK_TYPE_SERVER_ACTIVATE, pdata, peer))
		return FALSE;

	return TRUE;
}

static BOOL pf_server_logon(freerdp_peer* peer, const SEC_WINNT_AUTH_IDENTITY* identity,
                            BOOL automatic)
{
	pServerContext* ps;
	proxyData* pdata;
	proxyServerPeerLogon info = { 0 };

	WINPR_ASSERT(peer);

	ps = (pServerContext*)peer->context;
	WINPR_ASSERT(ps);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(identity);

	info.identity = identity;
	info.automatic = automatic;
	if (!pf_modules_run_filter(pdata->module, FILTER_TYPE_SERVER_PEER_LOGON, pdata, &info))
		return FALSE;
	return TRUE;
}

static BOOL pf_server_adjust_monitor_layout(freerdp_peer* peer)
{
	WINPR_ASSERT(peer);
	/* proxy as is, there's no need to do anything here */
	return TRUE;
}

static BOOL pf_server_receive_channel_data_hook(freerdp_peer* peer, UINT16 channelId,
                                                const BYTE* data, size_t size, UINT32 flags,
                                                size_t totalSize)
{
	pServerContext* ps;
	pClientContext* pc;
	proxyData* pdata;
	const proxyConfig* config;
	const pServerStaticChannelContext* channel;
	UINT64 channelId64 = channelId;

	WINPR_ASSERT(peer);

	ps = (pServerContext*)peer->context;
	WINPR_ASSERT(ps);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);

	pc = pdata->pc;
	config = pdata->config;
	WINPR_ASSERT(config);
	/*
	 * client side is not initialized yet, call original callback.
	 * this is probably a drdynvc message between peer and proxy server,
	 * which doesn't need to be proxied.
	 */
	if (!pc)
		goto original_cb;

	channel = HashTable_GetItemValue(ps->channelsById, &channelId64);
	if (!channel)
	{
		PROXY_LOG_ERR(TAG, ps, "channel id=%d not registered here, dropping", channelId64);
		return TRUE;
	}

	WINPR_ASSERT(channel->onFrontData);
	switch (channel->onFrontData(pdata, channel, data, size, flags, totalSize))
	{
		case PF_CHANNEL_RESULT_PASS:
		{
			proxyChannelDataEventInfo ev = { 0 };

			ev.channel_id = channelId;
			ev.channel_name = channel->channel_name;
			ev.data = data;
			ev.data_len = size;
			ev.flags = flags;
			ev.total_size = totalSize;
			return IFCALLRESULT(TRUE, pc->sendChannelData, pc, &ev);
		}
		case PF_CHANNEL_RESULT_DROP:
			return TRUE;
		case PF_CHANNEL_RESULT_ERROR:
			return FALSE;
	}

original_cb:
	WINPR_ASSERT(pdata->server_receive_channel_data_original);
	return pdata->server_receive_channel_data_original(peer, channelId, data, size, flags,
	                                                   totalSize);
}

static BOOL pf_server_initialize_peer_connection(freerdp_peer* peer)
{
	pServerContext* ps;
	rdpSettings* settings;
	proxyData* pdata;
	const proxyConfig* config;
	proxyServer* server;

	WINPR_ASSERT(peer);

	ps = (pServerContext*)peer->context;
	if (!ps)
		return FALSE;

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	pdata = proxy_data_new();
	if (!pdata)
		return FALSE;
	server = (proxyServer*)peer->ContextExtra;
	WINPR_ASSERT(server);

	proxy_data_set_server_context(pdata, ps);

	pdata->module = server->module;
	config = pdata->config = server->config;

	/* currently not supporting GDI orders */
	ZeroMemory(settings->OrderSupport, 32);

	WINPR_ASSERT(peer->context->update);
	peer->context->update->autoCalculateBitmapData = FALSE;

	settings->SupportMonitorLayoutPdu = TRUE;
	settings->SupportGraphicsPipeline = config->GFX;

	if (pf_utils_is_passthrough(config))
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DeactivateClientDecoding, TRUE))
			return FALSE;
	}

	if (!freerdp_settings_set_string(settings, FreeRDP_CertificateFile, config->CertificateFile) ||
	    !freerdp_settings_set_string(settings, FreeRDP_CertificateContent,
	                                 config->CertificateContent) ||
	    !freerdp_settings_set_string(settings, FreeRDP_PrivateKeyFile, config->PrivateKeyFile) ||
	    !freerdp_settings_set_string(settings, FreeRDP_PrivateKeyContent,
	                                 config->PrivateKeyContent) ||
	    !freerdp_settings_set_string(settings, FreeRDP_RdpKeyFile, config->RdpKeyFile) ||
	    !freerdp_settings_set_string(settings, FreeRDP_RdpKeyContent, config->RdpKeyContent))
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		return FALSE;
	}

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

	settings->RdpSecurity = config->ServerRdpSecurity;
	settings->TlsSecurity = config->ServerTlsSecurity;
	settings->NlaSecurity = config->ServerNlaSecurity;
	settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
		return FALSE;
	settings->SuppressOutput = TRUE;
	settings->RefreshRect = TRUE;
	settings->DesktopResize = TRUE;

	peer->PostConnect = pf_server_post_connect;
	peer->Activate = pf_server_activate;
	peer->Logon = pf_server_logon;
	peer->AdjustMonitorsLayout = pf_server_adjust_monitor_layout;
	settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */

	/* virtual channels receive data hook */
	pdata->server_receive_channel_data_original = peer->ReceiveChannelData;
	peer->ReceiveChannelData = pf_server_receive_channel_data_hook;

	if (!stream_dump_register_handlers(peer->context, CONNECTION_STATE_NEGO))
		return FALSE;
	return TRUE;
}

/**
 * Handles an incoming client connection, to be run in it's own thread.
 *
 * arg is a pointer to a freerdp_peer representing the client.
 */
static DWORD WINAPI pf_server_handle_peer(LPVOID arg)
{
	HANDLE eventHandles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	DWORD tmp;
	DWORD status;
	pServerContext* ps = NULL;
	proxyData* pdata = NULL;
	freerdp_peer* client;
	proxyServer* server;
	size_t count;
	peer_thread_args* args = arg;

	WINPR_ASSERT(args);

	client = args->client;
	WINPR_ASSERT(client);

	server = (proxyServer*)client->ContextExtra;
	WINPR_ASSERT(server);

	count = ArrayList_Count(server->peer_list);

	if (!pf_context_init_server_context(client))
		goto out_free_peer;

	if (!pf_server_initialize_peer_connection(client))
		goto out_free_peer;

	ps = (pServerContext*)client->context;
	WINPR_ASSERT(ps);
	PROXY_LOG_DBG(TAG, ps, "Added peer, %" PRIuz " connected", count);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);

	pf_modules_run_hook(pdata->module, HOOK_TYPE_SERVER_SESSION_INITIALIZE, pdata, client);

	WINPR_ASSERT(client->Initialize);
	client->Initialize(client);

	PROXY_LOG_INFO(TAG, ps, "new connection: proxy address: %s, client address: %s",
	               pdata->config->Host, client->hostname);

	pf_modules_run_hook(pdata->module, HOOK_TYPE_SERVER_SESSION_STARTED, pdata, client);

	while (1)
	{
		HANDLE ChannelEvent = INVALID_HANDLE_VALUE;
		DWORD eventCount = 0;
		{
			WINPR_ASSERT(client->GetEventHandles);
			tmp = client->GetEventHandles(client, &eventHandles[eventCount],
			                              ARRAYSIZE(eventHandles) - eventCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			eventCount += tmp;
		}
		/* Main client event handling loop */
		ChannelEvent = WTSVirtualChannelManagerGetEventHandle(ps->vcm);

		WINPR_ASSERT(ChannelEvent && (ChannelEvent != INVALID_HANDLE_VALUE));
		WINPR_ASSERT(pdata->abort_event && (pdata->abort_event != INVALID_HANDLE_VALUE));
		eventHandles[eventCount++] = ChannelEvent;
		eventHandles[eventCount++] = pdata->abort_event;
		eventHandles[eventCount++] = server->stopEvent;

		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE,
		                                1000); /* Do periodic polling to avoid client hang */

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (status: %d)", status);
			break;
		}

		WINPR_ASSERT(client->CheckFileDescriptor);
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

		if (WaitForSingleObject(server->stopEvent, 0) == WAIT_OBJECT_0)
		{
			WLog_INFO(TAG, "Server shutting down, terminating peer");
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

	PROXY_LOG_INFO(TAG, ps, "starting shutdown of connection");
	PROXY_LOG_INFO(TAG, ps, "stopping proxy's client");

	/* Abort the client. */
	proxy_data_abort_connect(pdata);

	pf_modules_run_hook(pdata->module, HOOK_TYPE_SERVER_SESSION_END, pdata, client);

	PROXY_LOG_INFO(TAG, ps, "freeing server's channels");

	WINPR_ASSERT(client->Close);
	client->Close(client);

	WINPR_ASSERT(client->Disconnect);
	client->Disconnect(client);

out_free_peer:
	PROXY_LOG_INFO(TAG, ps, "freeing proxy data");

	if (pdata && pdata->client_thread)
	{
		proxy_data_abort_connect(pdata);
		WaitForSingleObject(pdata->client_thread, INFINITE);
	}

	{
		ArrayList_Lock(server->peer_list);
		ArrayList_Remove(server->peer_list, args->thread);
		count = ArrayList_Count(server->peer_list);
		ArrayList_Unlock(server->peer_list);
	}
	PROXY_LOG_DBG(TAG, ps, "Removed peer, %" PRIuz " connected", count);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	proxy_data_free(pdata);

#if defined(WITH_DEBUG_EVENTS)
	DumpEventHandles();
#endif
	free(args);
	ExitThread(0);
	return 0;
}

static BOOL pf_server_start_peer(freerdp_peer* client)
{
	HANDLE hThread;
	proxyServer* server;
	peer_thread_args* args = calloc(1, sizeof(peer_thread_args));
	if (!args)
		return FALSE;

	WINPR_ASSERT(client);
	args->client = client;

	server = (proxyServer*)client->ContextExtra;
	WINPR_ASSERT(server);

	hThread = CreateThread(NULL, 0, pf_server_handle_peer, args, CREATE_SUSPENDED, NULL);
	if (!hThread)
		return FALSE;

	args->thread = hThread;
	if (!ArrayList_Append(server->peer_list, hThread))
	{
		CloseHandle(hThread);
		return FALSE;
	}

	return ResumeThread(hThread) != (DWORD)-1;
}

static BOOL pf_server_peer_accepted(freerdp_listener* listener, freerdp_peer* client)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(client);

	client->ContextExtra = listener->info;

	return pf_server_start_peer(client);
}

BOOL pf_server_start(proxyServer* server)
{
	WSADATA wsaData;

	WINPR_ASSERT(server);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		goto error;

	WINPR_ASSERT(server->config);
	WINPR_ASSERT(server->listener);
	WINPR_ASSERT(server->listener->Open);
	if (!server->listener->Open(server->listener, server->config->Host, server->config->Port))
	{
		switch (errno)
		{
			case EADDRINUSE:
				WLog_ERR(TAG, "failed to start listener: address already in use!");
				break;
			case EACCES:
				WLog_ERR(TAG, "failed to start listener: insufficent permissions!");
				break;
			default:
				WLog_ERR(TAG, "failed to start listener: errno=%d", errno);
				break;
		}

		goto error;
	}

	return TRUE;

error:
	WSACleanup();
	return FALSE;
}

BOOL pf_server_start_from_socket(proxyServer* server, int socket)
{
	WSADATA wsaData;

	WINPR_ASSERT(server);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		goto error;

	WINPR_ASSERT(server->listener);
	WINPR_ASSERT(server->listener->OpenFromSocket);
	if (!server->listener->OpenFromSocket(server->listener, socket))
	{
		switch (errno)
		{
			case EADDRINUSE:
				WLog_ERR(TAG, "failed to start listener: address already in use!");
				break;
			case EACCES:
				WLog_ERR(TAG, "failed to start listener: insufficent permissions!");
				break;
			default:
				WLog_ERR(TAG, "failed to start listener: errno=%d", errno);
				break;
		}

		goto error;
	}

	return TRUE;

error:
	WSACleanup();
	return FALSE;
}

BOOL pf_server_start_with_peer_socket(proxyServer* server, int peer_fd)
{
	struct sockaddr_storage peer_addr;
	socklen_t len = sizeof(peer_addr);
	freerdp_peer* client = NULL;

	WINPR_ASSERT(server);

	if (WaitForSingleObject(server->stopEvent, 0) == WAIT_OBJECT_0)
		goto fail;

	client = freerdp_peer_new(peer_fd);
	if (!client)
		goto fail;

	if (getpeername(peer_fd, (struct sockaddr*)&peer_addr, &len) != 0)
		goto fail;

	if (!freerdp_peer_set_local_and_hostname(client, &peer_addr))
		goto fail;

	client->ContextExtra = server;

	if (!pf_server_start_peer(client))
		goto fail;

	return TRUE;

fail:
	WLog_ERR(TAG, "PeerAccepted callback failed");
	freerdp_peer_free(client);
	return FALSE;
}

static BOOL are_all_required_modules_loaded(proxyModule* module, const proxyConfig* config)
{
	size_t i;

	for (i = 0; i < pf_config_required_plugins_count(config); i++)
	{
		const char* plugin_name = pf_config_required_plugin(config, i);

		if (!pf_modules_is_plugin_loaded(module, plugin_name))
		{
			WLog_ERR(TAG, "Required plugin '%s' is not loaded. stopping.", plugin_name);
			return FALSE;
		}
	}

	return TRUE;
}

static void peer_free(void* obj)
{
	HANDLE hdl = (HANDLE)obj;
	CloseHandle(hdl);
}

proxyServer* pf_server_new(const proxyConfig* config)
{
	wObject* obj;
	proxyServer* server;

	WINPR_ASSERT(config);

	server = calloc(1, sizeof(proxyServer));
	if (!server)
		return NULL;

	if (!pf_config_clone(&server->config, config))
		goto out;

	server->module = pf_modules_new(FREERDP_PROXY_PLUGINDIR, pf_config_modules(server->config),
	                                pf_config_modules_count(server->config));
	if (!server->module)
	{
		WLog_ERR(TAG, "failed to initialize proxy modules!");
		goto out;
	}

	pf_modules_list_loaded_plugins(server->module);
	if (!are_all_required_modules_loaded(server->module, server->config))
		goto out;

	server->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!server->stopEvent)
		goto out;

	server->listener = freerdp_listener_new();
	if (!server->listener)
		goto out;

	server->peer_list = ArrayList_New(FALSE);
	if (!server->peer_list)
		goto out;

	obj = ArrayList_Object(server->peer_list);
	WINPR_ASSERT(obj);

	obj->fnObjectFree = peer_free;

	server->listener->info = server;
	server->listener->PeerAccepted = pf_server_peer_accepted;

	if (!pf_modules_add(server->module, pf_config_plugin, (void*)server->config))
		goto out;

	return server;

out:
	pf_server_free(server);
	return NULL;
}

BOOL pf_server_run(proxyServer* server)
{
	BOOL rc = TRUE;
	HANDLE eventHandles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	DWORD eventCount;
	DWORD status;
	freerdp_listener* listener;

	WINPR_ASSERT(server);

	listener = server->listener;
	WINPR_ASSERT(listener);

	while (1)
	{
		WINPR_ASSERT(listener->GetEventHandles);
		eventCount = listener->GetEventHandles(listener, eventHandles, ARRAYSIZE(eventHandles));

		if ((0 == eventCount) || (eventCount >= ARRAYSIZE(eventHandles)))
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}

		WINPR_ASSERT(server->stopEvent);
		eventHandles[eventCount++] = server->stopEvent;
		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, 1000);

		if (WAIT_FAILED == status)
			break;

		if (WaitForSingleObject(server->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		if (WAIT_FAILED == status)
		{
			WLog_ERR(TAG, "select failed");
			rc = FALSE;
			break;
		}

		WINPR_ASSERT(listener->CheckFileDescriptor);
		if (listener->CheckFileDescriptor(listener) != TRUE)
		{
			WLog_ERR(TAG, "Failed to accept new peer");
			// TODO: Set out of resource error
			continue;
		}
	}

	WINPR_ASSERT(listener->Close);
	listener->Close(listener);
	return rc;
}

void pf_server_stop(proxyServer* server)
{

	if (!server)
		return;

	/* signal main thread to stop and wait for the thread to exit */
	SetEvent(server->stopEvent);
}

void pf_server_free(proxyServer* server)
{
	if (!server)
		return;

	pf_server_stop(server);

	while (ArrayList_Count(server->peer_list) > 0)
	{
		/* pf_server_stop triggers the threads to shut down.
		 * loop here until all of them stopped.
		 *
		 * This must be done before ArrayList_Free otherwise the thread removal
		 * in pf_server_handle_peer will deadlock due to both threads trying to
		 * lock the list.
		 */
		Sleep(100);
	}
	ArrayList_Free(server->peer_list);
	freerdp_listener_free(server->listener);

	if (server->stopEvent)
		CloseHandle(server->stopEvent);

	pf_server_config_free(server->config);
	pf_modules_free(server->module);
	free(server);

#if defined(WITH_DEBUG_EVENTS)
	DumpEventHandles();
#endif
}

BOOL pf_server_add_module(proxyServer* server, proxyModuleEntryPoint ep, void* userdata)
{
	WINPR_ASSERT(server);
	WINPR_ASSERT(ep);

	return pf_modules_add(server->module, ep, userdata);
}
