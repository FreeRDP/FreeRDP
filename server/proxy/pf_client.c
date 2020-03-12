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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>

#include "pf_channels.h"
#include "pf_gdi.h"
#include "pf_graphics.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_update.h"
#include "pf_log.h"
#include "pf_modules.h"
#include "pf_capture.h"

#define TAG PROXY_TAG("client")

static pReceiveChannelData client_receive_channel_data_original = NULL;

static BOOL proxy_server_reactivate(rdpContext* ps, const rdpContext* pc)
{
	if (!pf_context_copy_settings(ps->settings, pc->settings))
		return FALSE;

	/*
	 * DesktopResize causes internal function rdp_server_reactivate to be called,
	 * which causes the reactivation.
	 */
	if (!ps->update->DesktopResize(ps))
		return FALSE;

	return TRUE;
}

static void pf_client_on_error_info(void* ctx, ErrorInfoEventArgs* e)
{
	pClientContext* pc = (pClientContext*)ctx;
	pServerContext* ps = pc->pdata->ps;

	if (e->code == ERRINFO_NONE)
		return;

	LOG_WARN(TAG, pc, "received ErrorInfo PDU. code=0x%08" PRIu32 ", message: %s", e->code,
	         freerdp_get_error_info_string(e->code));

	/* forward error back to client */
	freerdp_set_error_info(ps->context.rdp, e->code);
	freerdp_send_error_info(ps->context.rdp);
}

static BOOL pf_client_load_rdpsnd(pClientContext* pc, proxyConfig* config)
{
	rdpContext* context = (rdpContext*)pc;
	pServerContext* ps = pc->pdata->ps;

	/*
	 * if AudioOutput is enabled in proxy and client connected with rdpsnd, use proxy as rdpsnd
	 * backend. Otherwise, use sys:fake.
	 */
	if (!freerdp_static_channel_collection_find(context->settings, "rdpsnd"))
	{
		char* params[2];
		params[0] = "rdpsnd";

		if (config->AudioOutput && WTSVirtualChannelManagerIsChannelJoined(ps->vcm, "rdpsnd"))
			params[1] = "sys:proxy";
		else
			params[1] = "sys:fake";

		if (!freerdp_client_add_static_channel(context->settings, 2, (char**)params))
			return FALSE;
	}

	return TRUE;
}

/**
 * Called before a connection is established.
 *
 * TODO: Take client to proxy settings and use channel whitelist to filter out
 * unwanted channels.
 */
static BOOL pf_client_pre_connect(freerdp* instance)
{
	pClientContext* pc = (pClientContext*)instance->context;
	pServerContext* ps = pc->pdata->ps;
	proxyConfig* config = ps->pdata->config;
	rdpSettings* settings = instance->settings;

	/*
	 * as the client's settings are copied from the server's, GlyphSupportLevel might not be
	 * GLYPH_SUPPORT_NONE. the proxy currently do not support GDI & GLYPH_SUPPORT_CACHE, so
	 * GlyphCacheSupport must be explicitly set to GLYPH_SUPPORT_NONE.
	 *
	 * Also, OrderSupport need to be zeroed, because it is currently not supported.
	 */
	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;
	ZeroMemory(instance->settings->OrderSupport, 32);

	settings->SupportDynamicChannels = TRUE;

	/* Multimon */
	settings->UseMultimon = TRUE;

	/* Sound */
	settings->AudioPlayback = FALSE;
	settings->DeviceRedirection = TRUE;

	/* Display control */
	settings->SupportDisplayControl = config->DisplayControl;
	settings->DynamicResolutionUpdate = config->DisplayControl;

	settings->AutoReconnectionEnabled = TRUE;
	/**
	 * Register the channel listeners.
	 * They are required to set up / tear down channels if they are loaded.
	 */
	PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                 pf_channels_on_client_channel_connect);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    pf_channels_on_client_channel_disconnect);
	PubSub_SubscribeErrorInfo(instance->context->pubSub, pf_client_on_error_info);

	/**
	 * Load all required plugins / channels / libraries specified by current
	 * settings.
	 */
	LOG_INFO(TAG, pc, "Loading addins");

	{
		/* add passthrough channels to channel def array */
		size_t i;

		if (settings->ChannelCount + config->PassthroughCount >= settings->ChannelDefArraySize)
		{
			LOG_ERR(TAG, pc, "too many channels");
			return FALSE;
		}

		for (i = 0; i < config->PassthroughCount; i++)
		{
			const char* channel_name = config->Passthrough[i];
			CHANNEL_DEF channel = { 0 };

			/* only connect connect this channel if already joined in peer connection */
			if (!WTSVirtualChannelManagerIsChannelJoined(ps->vcm, channel_name))
			{
				LOG_INFO(TAG, ps, "client did not connected with channel %s, skipping passthrough",
				         channel_name);

				continue;
			}

			channel.options = CHANNEL_OPTION_INITIALIZED; /* TODO: Export to config. */
			strncpy(channel.name, channel_name, CHANNEL_NAME_LEN);

			settings->ChannelDefArray[settings->ChannelCount++] = channel;
		}
	}

	if (!pf_client_load_rdpsnd(pc, config))
	{
		LOG_ERR(TAG, pc, "Failed to load rdpsnd client");
		return FALSE;
	}

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
	{
		LOG_ERR(TAG, pc, "Failed to load addins");
		return FALSE;
	}

	return TRUE;
}

static BOOL pf_client_receive_channel_data_hook(freerdp* instance, UINT16 channelId,
                                                const BYTE* data, size_t size, UINT32 flags,
                                                size_t totalSize)
{
	pClientContext* pc = (pClientContext*)instance->context;
	pServerContext* ps = pc->pdata->ps;
	proxyData* pdata = ps->pdata;
	proxyConfig* config = pdata->config;
	size_t i;

	const char* channel_name = freerdp_channels_get_name_by_id(instance, channelId);

	for (i = 0; i < config->PassthroughCount; i++)
	{
		if (strncmp(channel_name, config->Passthrough[i], CHANNEL_NAME_LEN) == 0)
		{
			proxyChannelDataEventInfo ev;
			UINT64 server_channel_id;

			ev.channel_id = channelId;
			ev.channel_name = channel_name;
			ev.data = data;
			ev.data_len = size;

			if (!pf_modules_run_filter(FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA, pdata, &ev))
				return FALSE;

			server_channel_id = (UINT64)HashTable_GetItemValue(ps->vc_ids, (void*)channel_name);
			return ps->context.peer->SendChannelData(ps->context.peer, (UINT16)server_channel_id,
			                                         data, size);
		}
	}

	return client_receive_channel_data_original(instance, channelId, data, size, flags, totalSize);
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
	proxyConfig* config;

	context = instance->context;
	settings = instance->settings;
	update = instance->update;
	pc = (pClientContext*)context;
	ps = (rdpContext*)pc->pdata->ps;
	config = pc->pdata->config;

	if (config->SessionCapture)
	{
		if (!pf_capture_create_session_directory(pc))
		{
			LOG_ERR(TAG, pc, "pf_capture_create_session_directory failed!");
			return FALSE;
		}

		LOG_ERR(TAG, pc, "frames dir created: %s", pc->frames_dir);
	}

	if (!gdi_init(instance, PIXEL_FORMAT_BGRA32))
		return FALSE;

	if (!pf_register_pointer(context->graphics))
		return FALSE;

	if (!settings->SoftwareGdi)
	{
		if (!pf_register_graphics(context->graphics))
		{
			LOG_ERR(TAG, pc, "failed to register graphics");
			return FALSE;
		}

		pf_gdi_register_update_callbacks(update);
		brush_cache_register_callbacks(update);
		glyph_cache_register_callbacks(update);
		bitmap_cache_register_callbacks(update);
		offscreen_cache_register_callbacks(update);
		palette_cache_register_callbacks(update);
	}

	pf_client_register_update_callbacks(update);

	/* virtual channels receive data hook */
	client_receive_channel_data_original = instance->ReceiveChannelData;
	instance->ReceiveChannelData = pf_client_receive_channel_data_hook;

	/* populate channel name -> channel ids map */
	{
		size_t i;
		for (i = 0; i < config->PassthroughCount; i++)
		{
			char* channel_name = config->Passthrough[i];
			UINT64 channel_id = (UINT64)freerdp_channels_get_id_by_name(instance, channel_name);
			HashTable_Add(pc->vc_ids, (void*)channel_name, (void*)channel_id);
		}
	}

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
	pClientContext* context;
	proxyData* pdata;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (pClientContext*)instance->context;
	pdata = context->pdata;

	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   pf_channels_on_client_channel_connect);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      pf_channels_on_client_channel_disconnect);
	PubSub_UnsubscribeErrorInfo(instance->context->pubSub, pf_client_on_error_info);
	gdi_free(instance);

	/* Only close the connection if NLA fallback process is done */
	if (!context->allow_next_conn_failure)
		proxy_data_abort_connect(pdata);
}

/*
 * pf_client_should_retry_without_nla:
 *
 * returns TRUE if in case of connection failure, the client should try again without NLA.
 * Otherwise, returns FALSE.
 */
static BOOL pf_client_should_retry_without_nla(pClientContext* pc)
{
	rdpSettings* settings = pc->context.settings;
	proxyConfig* config = pc->pdata->config;

	if (!config->ClientAllowFallbackToTls || !settings->NlaSecurity)
		return FALSE;

	return config->ClientTlsSecurity || config->ClientRdpSecurity;
}

static void pf_client_set_security_settings(pClientContext* pc)
{
	rdpSettings* settings = pc->context.settings;
	proxyConfig* config = pc->pdata->config;

	settings->RdpSecurity = config->ClientRdpSecurity;
	settings->TlsSecurity = config->ClientTlsSecurity;
	settings->NlaSecurity = FALSE;

	if (!config->ClientNlaSecurity)
		return;

	if (!settings->Username || !settings->Password)
		return;

	settings->NlaSecurity = TRUE;
}

static BOOL pf_client_connect_without_nla(pClientContext* pc)
{
	freerdp* instance = pc->context.instance;
	rdpSettings* settings = pc->context.settings;

	/* disable NLA */
	settings->NlaSecurity = FALSE;

	/* do not allow next connection failure */
	pc->allow_next_conn_failure = FALSE;
	return freerdp_connect(instance);
}

static BOOL pf_client_connect(freerdp* instance)
{
	pClientContext* pc = (pClientContext*)instance->context;
	rdpSettings* settings = instance->settings;
	BOOL rc = FALSE;
	BOOL retry = FALSE;

	LOG_INFO(TAG, pc, "connecting using client info: Username: %s, Domain: %s", settings->Username,
	         settings->Domain);

	pf_client_set_security_settings(pc);
	if (pf_client_should_retry_without_nla(pc))
		retry = pc->allow_next_conn_failure = TRUE;

	if (!freerdp_connect(instance))
	{
		if (!retry)
			goto out;

		LOG_ERR(TAG, pc, "failed to connect with NLA. retrying to connect without NLA");
		pf_modules_run_hook(HOOK_TYPE_CLIENT_LOGIN_FAILURE, pc->pdata);

		if (!pf_client_connect_without_nla(pc))
		{
			LOG_ERR(TAG, pc, "pf_client_connect_without_nla failed!");
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
static DWORD WINAPI pf_client_thread_proc(LPVOID arg)
{
	freerdp* instance = (freerdp*)arg;
	pClientContext* pc = (pClientContext*)instance->context;
	proxyData* pdata = pc->pdata;
	DWORD nCount;
	DWORD status;
	HANDLE handles[65];

	/*
	 * during redirection, freerdp's abort event might be overriden (reset) by the library, after
	 * the server set it in order to shutdown the connection. it means that the server might signal
	 * the client to abort, but the library code will override the signal and the client will
	 * continue its work instead of exiting. That's why the client must wait on `pdata->abort_event`
	 * too, which will never be modified by the library.
	 */
	handles[64] = pdata->abort_event;

	if (!pf_modules_run_hook(HOOK_TYPE_CLIENT_PRE_CONNECT, pdata))
	{
		proxy_data_abort_connect(pdata);
		return FALSE;
	}

	if (!pf_client_connect(instance))
	{
		proxy_data_abort_connect(pdata);
		return FALSE;
	}

	while (!freerdp_shall_disconnect(instance))
	{
		nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

		if (nCount == 0)
		{
			LOG_ERR(TAG, pc, "freerdp_get_event_handles failed!");
			break;
		}

		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__,
			         status);
			break;
		}

		if (freerdp_shall_disconnect(instance))
			break;

		if (proxy_data_shall_disconnect(pdata))
			break;

		if (!freerdp_check_event_handles(instance->context))
		{
			if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_SUCCESS)
				WLog_ERR(TAG, "Failed to check FreeRDP event handles");

			break;
		}
	}

	freerdp_disconnect(instance);
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

/**
 * Callback set in the rdp_freerdp structure, and used to make a certificate validation
 * when the connection requires it.
 * This function will actually be called by tls_verify_certificate().
 * @see rdp_client_connect() and tls_connect()
 * @param instance     pointer to the rdp_freerdp structure that contains the connection settings
 * @param host         The host currently connecting to
 * @param port         The port currently connecting to
 * @param common_name  The common name of the certificate, should match host or an alias of it
 * @param subject      The subject of the certificate
 * @param issuer       The certificate issuer name
 * @param fingerprint  The fingerprint of the certificate
 * @param flags        See VERIFY_CERT_FLAG_* for possible values.
 *
 * @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
static DWORD pf_client_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                             const char* common_name, const char* subject,
                                             const char* issuer, const char* fingerprint,
                                             DWORD flags)
{
	/* TODO: Add trust level to proxy configurable settings */
	return 1;
}

/**
 * Callback set in the rdp_freerdp structure, and used to make a certificate validation
 * when a stored certificate does not match the remote counterpart.
 * This function will actually be called by tls_verify_certificate().
 * @see rdp_client_connect() and tls_connect()
 * @param instance        pointer to the rdp_freerdp structure that contains the connection settings
 * @param host            The host currently connecting to
 * @param port            The port currently connecting to
 * @param common_name     The common name of the certificate, should match host or an alias of it
 * @param subject         The subject of the certificate
 * @param issuer          The certificate issuer name
 * @param fingerprint     The fingerprint of the certificate
 * @param old_subject     The subject of the previous certificate
 * @param old_issuer      The previous certificate issuer name
 * @param old_fingerprint The fingerprint of the previous certificate
 * @param flags           See VERIFY_CERT_FLAG_* for possible values.
 *
 * @return 1 if the certificate is trusted, 2 if temporary trusted, 0 otherwise.
 */
static DWORD pf_client_verify_changed_certificate_ex(
    freerdp* instance, const char* host, UINT16 port, const char* common_name, const char* subject,
    const char* issuer, const char* fingerprint, const char* old_subject, const char* old_issuer,
    const char* old_fingerprint, DWORD flags)
{
	/* TODO: Add trust level to proxy configurable settings */
	return 1;
}

static void pf_client_context_free(freerdp* instance, rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;

	if (!pc)
		return;

	free(pc->frames_dir);
	pc->frames_dir = NULL;

	HashTable_Free(pc->vc_ids);
}

static BOOL pf_client_client_new(freerdp* instance, rdpContext* context)
{
	if (!instance || !context)
		return FALSE;

	instance->PreConnect = pf_client_pre_connect;
	instance->PostConnect = pf_client_post_connect;
	instance->PostDisconnect = pf_client_post_disconnect;
	instance->VerifyCertificateEx = pf_client_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = pf_client_verify_changed_certificate_ex;
	instance->LogonErrorInfo = pf_logon_error_info;
	instance->ContextFree = pf_client_context_free;

	return TRUE;
}

static int pf_client_client_stop(rdpContext* context)
{
	pClientContext* pc = (pClientContext*)context;
	proxyData* pdata = pc->pdata;

	LOG_DBG(TAG, pc, "aborting client connection");
	proxy_data_abort_connect(pdata);
	freerdp_abort_connect(context->instance);

	if (pdata->client_thread)
	{
		/*
		 * Wait for client thread to finish. No need to call CloseHandle() here, as
		 * it is the responsibility of `proxy_data_free`.
		 */
		LOG_DBG(TAG, pc, "waiting for client thread to finish");
		WaitForSingleObject(pdata->client_thread, INFINITE);
		LOG_DBG(TAG, pc, "thread finished");
	}

	return 0;
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->ContextSize = sizeof(pClientContext);
	/* Client init and finish */
	pEntryPoints->ClientNew = pf_client_client_new;
	pEntryPoints->ClientStop = pf_client_client_stop;
	return 0;
}

/**
 * Starts running a client connection towards target server.
 */
DWORD WINAPI pf_client_start(LPVOID arg)
{
	rdpContext* context = (rdpContext*)arg;

	if (freerdp_client_start(context) != 0)
		return 1;

	return pf_client_thread_proc(context->instance);
}
