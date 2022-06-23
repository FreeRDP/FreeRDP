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

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>

#include <freerdp/server/proxy/proxy_log.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/encomsp.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/channels/channels.h>

#include "pf_client.h"
#include "pf_channel.h"
#include <freerdp/server/proxy/proxy_context.h>
#include "pf_update.h"
#include "pf_input.h"
#include <freerdp/server/proxy/proxy_config.h>
#include "proxy_modules.h"
#include "pf_utils.h"
#include "channels/pf_channel_rdpdr.h"
#include "channels/pf_channel_smartcard.h"

#define TAG PROXY_TAG("client")

static void channel_data_free(void* obj);
static BOOL proxy_server_reactivate(rdpContext* ps, const rdpContext* pc)
{
	WINPR_ASSERT(ps);
	WINPR_ASSERT(pc);

	if (!pf_context_copy_settings(ps->settings, pc->settings))
		return FALSE;

	/*
	 * DesktopResize causes internal function rdp_server_reactivate to be called,
	 * which causes the reactivation.
	 */
	WINPR_ASSERT(ps->update);
	if (!ps->update->DesktopResize(ps))
		return FALSE;

	return TRUE;
}

static void pf_client_on_error_info(void* ctx, const ErrorInfoEventArgs* e)
{
	pClientContext* pc = (pClientContext*)ctx;
	pServerContext* ps;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	WINPR_ASSERT(e);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);

	if (e->code == ERRINFO_NONE)
		return;

	PROXY_LOG_WARN(TAG, pc, "received ErrorInfo PDU. code=0x%08" PRIu32 ", message: %s", e->code,
	               freerdp_get_error_info_string(e->code));

	/* forward error back to client */
	freerdp_set_error_info(ps->context.rdp, e->code);
	freerdp_send_error_info(ps->context.rdp);
}

static void pf_client_on_activated(void* ctx, const ActivatedEventArgs* e)
{
	pClientContext* pc = (pClientContext*)ctx;
	pServerContext* ps;
	freerdp_peer* peer;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	WINPR_ASSERT(e);

	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);
	peer = ps->context.peer;
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);

	PROXY_LOG_INFO(TAG, pc, "client activated, registering server input callbacks");

	/* Register server input/update callbacks only after proxy client is fully activated */
	pf_server_register_input_callbacks(peer->context->input);
	pf_server_register_update_callbacks(peer->context->update);
}

static BOOL pf_client_load_rdpsnd(pClientContext* pc)
{
	rdpContext* context = (rdpContext*)pc;
	pServerContext* ps;
	const proxyConfig* config;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);
	config = pc->pdata->config;
	WINPR_ASSERT(config);

	/*
	 * if AudioOutput is enabled in proxy and client connected with rdpsnd, use proxy as rdpsnd
	 * backend. Otherwise, use sys:fake.
	 */
	if (!freerdp_static_channel_collection_find(context->settings, RDPSND_CHANNEL_NAME))
	{
		const char* params[2] = { RDPSND_CHANNEL_NAME, "sys:fake" };

		if (!freerdp_client_add_static_channel(context->settings, ARRAYSIZE(params), params))
			return FALSE;
	}

	return TRUE;
}

static BOOL pf_client_use_peer_load_balance_info(pClientContext* pc)
{
	pServerContext* ps;
	rdpSettings* settings;
	DWORD lb_info_len;
	const char* lb_info;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);
	settings = pc->context.settings;
	WINPR_ASSERT(settings);

	lb_info = freerdp_nego_get_routing_token(&ps->context, &lb_info_len);
	if (!lb_info)
		return TRUE;

	free(settings->LoadBalanceInfo);

	settings->LoadBalanceInfoLength = lb_info_len;
	settings->LoadBalanceInfo = malloc(settings->LoadBalanceInfoLength);

	if (!settings->LoadBalanceInfo)
		return FALSE;

	CopyMemory(settings->LoadBalanceInfo, lb_info, settings->LoadBalanceInfoLength);
	return TRUE;
}

static BOOL str_is_empty(const char* str)
{
	if (!str)
		return TRUE;
	if (strlen(str) == 0)
		return TRUE;
	return FALSE;
}

static BOOL pf_client_use_proxy_smartcard_auth(const rdpSettings* settings)
{
	BOOL enable = freerdp_settings_get_bool(settings, FreeRDP_SmartcardLogon);
	const char* key = freerdp_settings_get_string(settings, FreeRDP_SmartcardPrivateKey);
	const char* cert = freerdp_settings_get_string(settings, FreeRDP_SmartcardCertificate);

	if (!enable)
		return FALSE;

	if (str_is_empty(key))
		return FALSE;

	if (str_is_empty(cert))
		return FALSE;

	return TRUE;
}

static BOOL freerdp_client_load_static_channel_addin(rdpChannels* channels, rdpSettings* settings,
                                                     const char* name, void* data)
{
	PVIRTUALCHANNELENTRY entry = NULL;
	PVIRTUALCHANNELENTRYEX entryEx = NULL;
	entryEx = (PVIRTUALCHANNELENTRYEX)(void*)freerdp_load_channel_addin_entry(
	    name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX);

	if (!entryEx)
		entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (entryEx)
	{
		if (freerdp_channels_client_load_ex(channels, settings, entryEx, data) == 0)
		{
			WLog_INFO(TAG, "loading channelEx %s", name);
			return TRUE;
		}
	}
	else if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			WLog_INFO(TAG, "loading channel %s", name);
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL pf_client_pre_connect(freerdp* instance)
{
	pClientContext* pc;
	pServerContext* ps;
	const proxyConfig* config;
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);
	config = ps->pdata->config;
	WINPR_ASSERT(config);
	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	/*
	 * as the client's settings are copied from the server's, GlyphSupportLevel might not be
	 * GLYPH_SUPPORT_NONE. the proxy currently do not support GDI & GLYPH_SUPPORT_CACHE, so
	 * GlyphCacheSupport must be explicitly set to GLYPH_SUPPORT_NONE.
	 *
	 * Also, OrderSupport need to be zeroed, because it is currently not supported.
	 */
	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;
	ZeroMemory(settings->OrderSupport, 32);

	if (WTSVirtualChannelManagerIsChannelJoined(ps->vcm, DRDYNVC_SVC_CHANNEL_NAME))
		settings->SupportDynamicChannels = TRUE;

	/* Multimon */
	settings->UseMultimon = TRUE;

	/* Sound */
	if (!freerdp_settings_set_bool(settings, FreeRDP_AudioCapture, config->AudioInput) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AudioPlayback, config->AudioOutput) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_DeviceRedirection,
	                               config->DeviceRedirection) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_SupportDisplayControl,
	                               config->DisplayControl) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_MultiTouchInput, config->Multitouch))
		return FALSE;

	if (config->RemoteApp)
	{
		if (WTSVirtualChannelManagerIsChannelJoined(ps->vcm, RAIL_SVC_CHANNEL_NAME))
			settings->RemoteApplicationMode = TRUE;
	}

	if (config->DeviceRedirection)
	{
		if (WTSVirtualChannelManagerIsChannelJoined(ps->vcm, RDPDR_SVC_CHANNEL_NAME))
			settings->DeviceRedirection = TRUE;
	}

	/* Display control */
	settings->SupportDisplayControl = config->DisplayControl;
	settings->DynamicResolutionUpdate = config->DisplayControl;

	if (WTSVirtualChannelManagerIsChannelJoined(ps->vcm, ENCOMSP_SVC_CHANNEL_NAME))
		settings->EncomspVirtualChannel = TRUE;

	if (config->Clipboard)
	{
		if (WTSVirtualChannelManagerIsChannelJoined(ps->vcm, CLIPRDR_SVC_CHANNEL_NAME))
			settings->RedirectClipboard = config->Clipboard;
	}

	settings->AutoReconnectionEnabled = TRUE;

	PubSub_SubscribeErrorInfo(instance->context->pubSub, pf_client_on_error_info);
	PubSub_SubscribeActivated(instance->context->pubSub, pf_client_on_activated);
	if (!pf_client_use_peer_load_balance_info(pc))
		return FALSE;

	return pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_PRE_CONNECT, pc->pdata, pc);
}

static BOOL pf_client_load_channels(freerdp* instance)
{
	pClientContext* pc;
	pServerContext* ps;
	const proxyConfig* config;
	rdpSettings* settings;

	WINPR_ASSERT(instance);
	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);
	WINPR_ASSERT(ps->pdata);
	config = ps->pdata->config;
	WINPR_ASSERT(config);
	settings = instance->context->settings;
	WINPR_ASSERT(settings);
	/**
	 * Load all required plugins / channels / libraries specified by current
	 * settings.
	 */
	PROXY_LOG_INFO(TAG, pc, "Loading addins");

	if (!pf_client_load_rdpsnd(pc))
	{
		PROXY_LOG_ERR(TAG, pc, "Failed to load rdpsnd client");
		return FALSE;
	}

	if (!pf_utils_is_passthrough(config))
	{
		if (!freerdp_client_load_addins(instance->context->channels, settings))
		{
			PROXY_LOG_ERR(TAG, pc, "Failed to load addins");
			return FALSE;
		}
	}
	else
	{
		if (!pf_channel_rdpdr_client_new(pc))
			return FALSE;
#if defined(WITH_PROXY_EMULATE_SMARTCARD)
		if (!pf_channel_smartcard_client_new(pc))
			return FALSE;
#endif
		/* Copy the current channel settings from the peer connection to the client. */
		if (!freerdp_channels_from_mcs(settings, &ps->context))
			return FALSE;

		/* Filter out channels we do not want */
		{
			CHANNEL_DEF* channels = (CHANNEL_DEF*)freerdp_settings_get_pointer_array_writable(
			    settings, FreeRDP_ChannelDefArray, 0);
			size_t x, size = freerdp_settings_get_uint32(settings, FreeRDP_ChannelCount);

			WINPR_ASSERT(channels || (size == 0));

			for (x = 0; x < size;)
			{
				CHANNEL_DEF* cur = &channels[x];
				proxyChannelDataEventInfo dev = { 0 };

				dev.channel_name = cur->name;
				dev.flags = cur->options;

				/* Filter out channels blocked by config */
				if (!pf_modules_run_filter(pc->pdata->module,
				                           FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_CREATE, pc->pdata,
				                           &dev))
				{
					const size_t s = size - MIN(size, x + 1);
					memmove(cur, &cur[1], sizeof(CHANNEL_DEF) * s);
					size--;
				}
				else
					x++;
			}

			if (!freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, x))
				return FALSE;
		}
	}
	return pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_LOAD_CHANNELS, pc->pdata, pc);
}

static BOOL pf_client_receive_channel_data_hook(freerdp* instance, UINT16 channelId,
                                                const BYTE* xdata, size_t xsize, UINT32 flags,
                                                size_t totalSize)
{
	pClientContext* pc;
	pServerContext* ps;
	proxyData* pdata;
	pServerStaticChannelContext* channel;
	UINT16 server_channel_id;
	UINT64 channelId64 = channelId;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(xdata || (xsize == 0));

	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);

	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);

	pdata = ps->pdata;
	WINPR_ASSERT(pdata);

	channel = HashTable_GetItemValue(ps->channelsById, &channelId64);
	if (!channel)
		return TRUE;

	WINPR_ASSERT(channel->onBackData);
	switch (channel->onBackData(pdata, channel, xdata, xsize, flags, totalSize))
	{
		case PF_CHANNEL_RESULT_PASS:
			break;
		case PF_CHANNEL_RESULT_DROP:
			return TRUE;
		case PF_CHANNEL_RESULT_ERROR:
			return FALSE;
	}

	server_channel_id = WTSChannelGetId(ps->context.peer, channel->channel_name);

	/* Ignore messages for channels that can not be mapped.
	 * The client might not have enabled support for this specific channel,
	 * so just drop the message. */
	if (server_channel_id == 0)
		return TRUE;

	return ps->context.peer->SendChannelPacket(ps->context.peer, server_channel_id, totalSize,
	                                           flags, xdata, xsize);
}

static BOOL pf_client_on_server_heartbeat(freerdp* instance, BYTE period, BYTE count1, BYTE count2)
{
	pClientContext* pc;
	pServerContext* ps;

	WINPR_ASSERT(instance);
	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = pc->pdata->ps;
	WINPR_ASSERT(ps);

	return freerdp_heartbeat_send_heartbeat_pdu(ps->context.peer, period, count1, count2);
}

static BOOL pf_client_send_channel_data(pClientContext* pc, const proxyChannelDataEventInfo* ev)
{
	WINPR_ASSERT(pc);
	WINPR_ASSERT(ev);

	return Queue_Enqueue(pc->cached_server_channel_data, ev);
}

static BOOL sendQueuedChannelData(pClientContext* pc)
{
	BOOL rc = TRUE;

	WINPR_ASSERT(pc);

	if (pc->connected)
	{
		proxyChannelDataEventInfo* ev;

		Queue_Lock(pc->cached_server_channel_data);
		while (rc && (ev = Queue_Dequeue(pc->cached_server_channel_data)))
		{
			UINT16 channelId;
			WINPR_ASSERT(pc->context.instance);

			channelId = freerdp_channels_get_id_by_name(pc->context.instance, ev->channel_name);
			/* Ignore unmappable channels */
			if ((channelId == 0) || (channelId == UINT16_MAX))
				rc = TRUE;
			else
			{
				WINPR_ASSERT(pc->context.instance->SendChannelPacket);
				rc = pc->context.instance->SendChannelPacket(pc->context.instance, channelId,
				                                             ev->total_size, ev->flags, ev->data,
				                                             ev->data_len);
			}
			channel_data_free(ev);
		}

		Queue_Unlock(pc->cached_server_channel_data);
	}

	return rc;
}

/**
 * Called after a RDP connection was successfully established.
 * Settings might have changed during negotiation of client / server feature
 * support.
 *
 * Set up local framebuffers and painting callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
static BOOL pf_client_post_connect(freerdp* instance)
{
	rdpContext* context;
	rdpSettings* settings;
	rdpUpdate* update;
	rdpContext* ps;
	pClientContext* pc;
	const proxyConfig* config;

	WINPR_ASSERT(instance);
	context = instance->context;
	WINPR_ASSERT(context);
	settings = context->settings;
	WINPR_ASSERT(settings);
	update = context->update;
	WINPR_ASSERT(update);
	pc = (pClientContext*)context;
	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	ps = (rdpContext*)pc->pdata->ps;
	WINPR_ASSERT(ps);
	config = pc->pdata->config;
	WINPR_ASSERT(config);

	if (!pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_POST_CONNECT, pc->pdata, pc))
		return FALSE;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	WINPR_ASSERT(settings->SoftwareGdi);

	pf_client_register_update_callbacks(update);

	/* virtual channels receive data hook */
	pc->client_receive_channel_data_original = instance->ReceiveChannelData;
	instance->ReceiveChannelData = pf_client_receive_channel_data_hook;

	instance->heartbeat->ServerHeartbeat = pf_client_on_server_heartbeat;

	pc->connected = TRUE;

	/* Send cached channel data */
	sendQueuedChannelData(pc);

	/*
	 * after the connection fully established and settings were negotiated with target server,
	 * send a reactivation sequence to the client with the negotiated settings. This way,
	 * settings are synchorinized between proxy's peer and and remote target.
	 */
	return proxy_server_reactivate(ps, context);
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void pf_client_post_disconnect(freerdp* instance)
{
	pClientContext* pc;
	proxyData* pdata;

	if (!instance)
		return;

	if (!instance->context)
		return;

	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
	pf_channel_smartcard_client_free(pc);
#endif

	pf_channel_rdpdr_client_free(pc);

	pc->connected = FALSE;
	pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_POST_DISCONNECT, pc->pdata, pc);

	PubSub_UnsubscribeErrorInfo(instance->context->pubSub, pf_client_on_error_info);
	gdi_free(instance);

	/* Only close the connection if NLA fallback process is done */
	if (!pc->allow_next_conn_failure)
		proxy_data_abort_connect(pdata);
}

static BOOL pf_client_redirect(freerdp* instance)
{
	pClientContext* pc;
	proxyData* pdata;

	if (!instance)
		return FALSE;

	if (!instance->context)
		return FALSE;

	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);

#if defined(WITH_PROXY_EMULATE_SMARTCARD)
	pf_channel_smartcard_client_reset(pc);
#endif
	pf_channel_rdpdr_client_reset(pc);

	return pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_REDIRECT, pc->pdata, pc);
}

/*
 * pf_client_should_retry_without_nla:
 *
 * returns TRUE if in case of connection failure, the client should try again without NLA.
 * Otherwise, returns FALSE.
 */
static BOOL pf_client_should_retry_without_nla(pClientContext* pc)
{
	rdpSettings* settings;
	const proxyConfig* config;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	settings = pc->context.settings;
	WINPR_ASSERT(settings);
	config = pc->pdata->config;
	WINPR_ASSERT(config);

	if (!config->ClientAllowFallbackToTls || !settings->NlaSecurity)
		return FALSE;

	return config->ClientTlsSecurity || config->ClientRdpSecurity;
}

static void pf_client_set_security_settings(pClientContext* pc)
{
	rdpSettings* settings;
	const proxyConfig* config;

	WINPR_ASSERT(pc);
	WINPR_ASSERT(pc->pdata);
	settings = pc->context.settings;
	WINPR_ASSERT(settings);
	config = pc->pdata->config;
	WINPR_ASSERT(config);

	freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, config->ClientRdpSecurity);
	freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, config->ClientTlsSecurity);
	freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, config->ClientNlaSecurity);

	/* Smartcard authentication currently does not work with NLA */
	if (pf_client_use_proxy_smartcard_auth(settings))
	{
		freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE);
		freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE);
		freerdp_settings_set_bool(settings, FreeRDP_RedirectSmartCards, TRUE);
	}

	if (!config->ClientNlaSecurity)
		return;

	if (!settings->Username || !settings->Password)
		return;
}

static BOOL pf_client_connect_without_nla(pClientContext* pc)
{
	freerdp* instance;
	rdpSettings* settings;

	WINPR_ASSERT(pc);
	instance = pc->context.instance;
	WINPR_ASSERT(instance);
	settings = pc->context.settings;
	WINPR_ASSERT(settings);

	/* If already disabled abort early. */
	if (!settings->NlaSecurity)
		return FALSE;

	/* disable NLA */
	settings->NlaSecurity = FALSE;

	/* do not allow next connection failure */
	pc->allow_next_conn_failure = FALSE;
	return freerdp_reconnect(instance);
}

static BOOL pf_client_connect(freerdp* instance)
{
	pClientContext* pc;
	rdpSettings* settings;
	BOOL rc = FALSE;
	BOOL retry = FALSE;

	WINPR_ASSERT(instance);
	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);
	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	PROXY_LOG_INFO(TAG, pc, "connecting using client info: Username: %s, Domain: %s",
	               settings->Username, settings->Domain);

	pf_client_set_security_settings(pc);
	if (pf_client_should_retry_without_nla(pc))
		retry = pc->allow_next_conn_failure = TRUE;

	PROXY_LOG_INFO(TAG, pc, "connecting using security settings: rdp=%d, tls=%d, nla=%d",
	               settings->RdpSecurity, settings->TlsSecurity, settings->NlaSecurity);

	if (!freerdp_connect(instance))
	{
		pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_LOGIN_FAILURE, pc->pdata, pc);

		if (!retry)
			goto out;

		PROXY_LOG_ERR(TAG, pc, "failed to connect with NLA. retrying to connect without NLA");
		if (!pf_client_connect_without_nla(pc))
		{
			PROXY_LOG_ERR(TAG, pc, "pf_client_connect_without_nla failed!");
			goto out;
		}
	}

	rc = TRUE;
out:
	pc->allow_next_conn_failure = FALSE;
	return rc;
}

/**
 * RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends.
 */
static DWORD WINAPI pf_client_thread_proc(pClientContext* pc)
{
	freerdp* instance;
	proxyData* pdata;
	DWORD nCount = 0;
	DWORD status;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };

	WINPR_ASSERT(pc);

	instance = pc->context.instance;
	WINPR_ASSERT(instance);

	pdata = pc->pdata;
	WINPR_ASSERT(pdata);
	/*
	 * during redirection, freerdp's abort event might be overriden (reset) by the library, after
	 * the server set it in order to shutdown the connection. it means that the server might signal
	 * the client to abort, but the library code will override the signal and the client will
	 * continue its work instead of exiting. That's why the client must wait on `pdata->abort_event`
	 * too, which will never be modified by the library.
	 */
	handles[nCount++] = pdata->abort_event;

	if (!pf_modules_run_hook(pdata->module, HOOK_TYPE_CLIENT_INIT_CONNECT, pdata, pc))
	{
		proxy_data_abort_connect(pdata);
		goto end;
	}

	if (!pf_client_connect(instance))
	{
		proxy_data_abort_connect(pdata);
		goto end;
	}
	handles[nCount++] = Queue_Event(pc->cached_server_channel_data);

	while (!freerdp_shall_disconnect_context(instance->context))
	{
		UINT32 tmp = freerdp_get_event_handles(instance->context, &handles[nCount],
		                                       ARRAYSIZE(handles) - nCount);

		if (tmp == 0)
		{
			PROXY_LOG_ERR(TAG, pc, "freerdp_get_event_handles failed!");
			break;
		}

		status = WaitForMultipleObjects(nCount + tmp, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__,
			         status);
			break;
		}

		/* abort_event triggered */
		if (status == WAIT_OBJECT_0)
			break;

		if (freerdp_shall_disconnect_context(instance->context))
			break;

		if (proxy_data_shall_disconnect(pdata))
			break;

		if (!freerdp_check_event_handles(instance->context))
		{
			if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
				WLog_ERR(TAG, "Failed to check FreeRDP event handles");

			break;
		}
		sendQueuedChannelData(pc);
	}

	freerdp_disconnect(instance);

end:
	pf_modules_run_hook(pdata->module, HOOK_TYPE_CLIENT_UNINIT_CONNECT, pdata, pc);

	return 0;
}

static int pf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

static void pf_client_context_free(freerdp* instance, rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	WINPR_UNUSED(instance);

	if (!pc)
		return;

	pc->sendChannelData = NULL;
	Queue_Free(pc->cached_server_channel_data);
	Stream_Free(pc->remote_pem, TRUE);
	free(pc->remote_hostname);
	free(pc->computerName.v);
	HashTable_Free(pc->interceptContextMap);
}

static int pf_client_verify_X509_certificate(freerdp* instance, const BYTE* data, size_t length,
                                             const char* hostname, UINT16 port, DWORD flags)
{
	pClientContext* pc;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(data);
	WINPR_ASSERT(length > 0);
	WINPR_ASSERT(hostname);

	pc = (pClientContext*)instance->context;
	WINPR_ASSERT(pc);

	if (!Stream_EnsureCapacity(pc->remote_pem, length))
		return 0;
	Stream_SetPosition(pc->remote_pem, 0);

	free(pc->remote_hostname);
	pc->remote_hostname = NULL;

	if (length > 0)
		Stream_Write(pc->remote_pem, data, length);

	if (hostname)
		pc->remote_hostname = _strdup(hostname);
	pc->remote_port = port;
	pc->remote_flags = flags;

	Stream_SealLength(pc->remote_pem);
	if (!pf_modules_run_hook(pc->pdata->module, HOOK_TYPE_CLIENT_VERIFY_X509, pc->pdata, pc))
		return 0;
	return 1;
}

void channel_data_free(void* obj)
{
	union
	{
		const void* cpv;
		void* pv;
	} cnv;
	proxyChannelDataEventInfo* dst = obj;
	if (dst)
	{
		cnv.cpv = dst->data;
		free(cnv.pv);

		cnv.cpv = dst->channel_name;
		free(cnv.pv);
		free(dst);
	}
}

static void* channel_data_copy(const void* obj)
{
	union
	{
		const void* cpv;
		void* pv;
	} cnv;
	const proxyChannelDataEventInfo* src = obj;
	proxyChannelDataEventInfo* dst;

	WINPR_ASSERT(src);

	dst = calloc(1, sizeof(proxyChannelDataEventInfo));
	if (!dst)
		goto fail;

	*dst = *src;
	if (src->channel_name)
	{
		dst->channel_name = _strdup(src->channel_name);
		if (!dst->channel_name)
			goto fail;
	}
	dst->data = malloc(src->data_len);
	if (!dst->data)
		goto fail;

	cnv.cpv = dst->data;
	memcpy(cnv.pv, src->data, src->data_len);
	return dst;

fail:
	channel_data_free(dst);
	return NULL;
}

static BOOL pf_client_client_new(freerdp* instance, rdpContext* context)
{
	wObject* obj;
	pClientContext* pc = (pClientContext*)context;

	if (!instance || !context)
		return FALSE;

	instance->LoadChannels = pf_client_load_channels;
	instance->PreConnect = pf_client_pre_connect;
	instance->PostConnect = pf_client_post_connect;
	instance->PostDisconnect = pf_client_post_disconnect;
	instance->Redirect = pf_client_redirect;
	instance->LogonErrorInfo = pf_logon_error_info;
	instance->VerifyX509Certificate = pf_client_verify_X509_certificate;

	pc->remote_pem = Stream_New(NULL, 4096);
	if (!pc->remote_pem)
		return FALSE;

	pc->sendChannelData = pf_client_send_channel_data;
	pc->cached_server_channel_data = Queue_New(TRUE, -1, -1);
	if (!pc->cached_server_channel_data)
		return FALSE;
	obj = Queue_Object(pc->cached_server_channel_data);
	WINPR_ASSERT(obj);
	obj->fnObjectNew = channel_data_copy;
	obj->fnObjectFree = channel_data_free;

	pc->interceptContextMap = HashTable_New(FALSE);
	if (!pc->interceptContextMap)
		return FALSE;

	if (!HashTable_SetupForStringData(pc->interceptContextMap, FALSE))
		return FALSE;

	obj = HashTable_ValueObject(pc->interceptContextMap);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = intercept_context_entry_free;

	return TRUE;
}

static int pf_client_client_stop(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata;

	WINPR_ASSERT(pc);
	pdata = pc->pdata;
	WINPR_ASSERT(pdata);

	PROXY_LOG_DBG(TAG, pc, "aborting client connection");
	proxy_data_abort_connect(pdata);
	freerdp_abort_connect_context(context);

	return 0;
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	WINPR_ASSERT(pEntryPoints);

	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->ContextSize = sizeof(pClientContext);
	/* Client init and finish */
	pEntryPoints->ClientNew = pf_client_client_new;
	pEntryPoints->ClientFree = pf_client_context_free;
	pEntryPoints->ClientStop = pf_client_client_stop;
	return 0;
}

/**
 * Starts running a client connection towards target server.
 */
DWORD WINAPI pf_client_start(LPVOID arg)
{
	DWORD rc = 1;
	pClientContext* pc = (pClientContext*)arg;

	WINPR_ASSERT(pc);
	if (freerdp_client_start(&pc->context) == 0)
		rc = pf_client_thread_proc(pc);
	freerdp_client_stop(&pc->context);
	return rc;
}
