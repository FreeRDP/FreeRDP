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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/signal.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>
#include <freerdp/log.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "pf_channels.h"
#include "pf_gdi.h"
#include "pf_graphics.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_update.h"
#include "pf_log.h"
#include "pf_modules.h"

#define TAG PROXY_TAG("client")

/**
 * Re-negotiate with original client after negotiation between the proxy
 * and the target has finished.
 */
static void proxy_server_reactivate(rdpContext* ps, const rdpContext* target)
{
	pf_context_copy_settings(ps->settings, target->settings, TRUE);

	/* DesktopResize causes internal function rdp_server_reactivate to be called,
	 * which causes the reactivation.
	 */
	ps->update->DesktopResize(ps);
}

static void pf_OnErrorInfo(void* ctx, ErrorInfoEventArgs* e)
{
	pClientContext* pc = (pClientContext*) ctx;
	proxyData* pdata = pc->pdata;
	rdpContext* ps = (rdpContext*)pdata->ps;

	if (e->code != ERRINFO_NONE)
	{
		const char* errorMessage = freerdp_get_error_info_string(e->code);
		WLog_WARN(TAG, "Proxy's client received error info pdu from server: (0x%08"PRIu32"): %s", e->code, errorMessage);
		/* forward error back to client */
		freerdp_set_error_info(ps->rdp, e->code);
		freerdp_send_error_info(ps->rdp);
	}
}

static BOOL pf_client_load_rdpsnd(pClientContext* pc, proxyConfig* config)
{
	rdpContext* context = (rdpContext*) pc;
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
		
		if (!freerdp_client_add_static_channel(context->settings, 2, (char**) params))
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
	pClientContext* pc = (pClientContext*) instance->context;
	pServerContext* ps = pc->pdata->ps;
	proxyConfig* config = ps->pdata->config;
	rdpSettings* settings = instance->settings;

	if (!pf_modules_run_hook(HOOK_TYPE_CLIENT_PRE_CONNECT, (rdpContext*)ps))
		return FALSE;

	/*
	 * as the client's settings are copied from the server's, GlyphSupportLevel might not be
	 * GLYPH_SUPPORT_NONE. the proxy currently do not support GDI & GLYPH_SUPPORT_CACHE, so
	 * GlyphCacheSupport must be explicitly set to GLYPH_SUPPORT_NONE.
	 */
	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;

	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;

	/**
	 * settings->OrderSupport is initialized at this point.
	 * Only override it if you plan to implement custom order
	 * callbacks or deactiveate certain features.
	 */

	/* currently not supporting GDI orders */
	ZeroMemory(instance->settings->OrderSupport, 32);

	
	/**
	 * Register the channel listeners.
	 * They are required to set up / tear down channels if they are loaded.
	 */
	PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                 pf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    pf_OnChannelDisconnectedEventHandler);
	PubSub_SubscribeErrorInfo(instance->context->pubSub, pf_OnErrorInfo);

	/**
	 * Load all required plugins / channels / libraries specified by current
	 * settings.
	 */
	WLog_INFO(TAG, "Loading addins");

	if (!pf_client_load_rdpsnd(pc, config))
	{
		WLog_ERR(TAG, "Failed to load rdpsnd client!");
		return FALSE;
	}

	if (!freerdp_client_load_addins(instance->context->channels,
	                                instance->settings))
	{
		WLog_ERR(TAG, "Failed to load addins");
		return FALSE;
	}

	return TRUE;
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
	pClientContext* pc;
	rdpContext* ps;

	context = instance->context;
	settings = instance->settings;
	update = instance->update;
	pc = (pClientContext*) context;
	ps = (rdpContext*) pc->pdata->ps;

	if (!gdi_init(instance, PIXEL_FORMAT_XRGB32))
		return FALSE;

	if (!pf_register_pointer(context->graphics))
		return FALSE;

	if (!settings->SoftwareGdi)
	{
		if (!pf_register_graphics(context->graphics))
		{
			WLog_ERR(TAG, "failed to register graphics");
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
	proxy_server_reactivate(ps, context);
	return TRUE;
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

	context = (pClientContext*) instance->context;
	pdata = context->pdata;

	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   pf_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      pf_OnChannelDisconnectedEventHandler);
	PubSub_UnsubscribeErrorInfo(instance->context->pubSub, pf_OnErrorInfo);
	gdi_free(instance);

	/* Only close the connection if NLA fallback process is done */
	if (!context->during_connect_process)
		proxy_data_abort_connect(pdata);
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

	/* on first try, proxy client should always try to connect with NLA */
	instance->settings->NlaSecurity = TRUE;

	/*
	 * Only set the `during_connect_process` flag if NlaSecurity is enabled.
	 * If NLASecurity isn't enabled, the connection should be closed right after the first failure.
	 */
	if (instance->settings->NlaSecurity)
		pc->during_connect_process = TRUE;

	if (!freerdp_connect(instance))
	{
		if (instance->settings->NlaSecurity)
		{
			WLog_ERR(TAG, "freerdp_connect() failed, trying to connect without NLA");

			/* disable NLA, enable TLS */
			instance->settings->NlaSecurity = FALSE;
			instance->settings->RdpSecurity = TRUE;
			instance->settings->TlsSecurity = TRUE;

			pc->during_connect_process = FALSE;
			if (!freerdp_connect(instance))
			{
				WLog_ERR(TAG, "connection failure");
				return 0;
			}
		}
		else
		{
			WLog_ERR(TAG, "connection failure");
			return 0;
		}
	}

	pc->during_connect_process = FALSE;

	while (!freerdp_shall_disconnect(instance))
	{
		nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

		if (nCount == 0)
		{
			WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
			break;
		}

		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %"PRIu32"", __FUNCTION__,
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

/**
 * Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available.
 * */
static BOOL pf_client_global_init(void)
{
	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
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
        const char* common_name,
        const char* subject, const char* issuer,
        const char* fingerprint, DWORD flags)
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
static DWORD pf_client_verify_changed_certificate_ex(freerdp* instance,
        const char* host, UINT16 port,
        const char* common_name,
        const char* subject, const char* issuer,
        const char* fingerprint,
        const char* old_subject, const char* old_issuer,
        const char* old_fingerprint, DWORD flags)
{
	/* TODO: Add trust level to proxy configurable settings */
	return 1;
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
	return TRUE;
}

static int pf_client_client_stop(rdpContext* context)
{
	pClientContext* pc = (pClientContext*) context;
	proxyData* pdata = pc->pdata;

	WLog_DBG(TAG, "aborting client connection");
	proxy_data_abort_connect(pdata);
	freerdp_abort_connect(context->instance);

	if (pdata->client_thread)
	{
		/*
		 * Wait for client thread to finish. No need to call CloseHandle() here, as
		 * it is the responsibility of `proxy_data_free`.
		 */
		WLog_DBG(TAG, "pf_client_client_stop(): waiting for thread to finish");
		WaitForSingleObject(pdata->client_thread, INFINITE);
		WLog_DBG(TAG, "pf_client_client_stop(): thread finished");
	}

	return 0;
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = pf_client_global_init;
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
