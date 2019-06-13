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
#include "pf_common.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_update.h"
#include "pf_log.h"

#define TAG PROXY_TAG("client")

/**
 * Re-negotiate with original client after negotiation between the proxy
 * and the target has finished.
 */
static void proxy_server_reactivate(rdpContext* client, rdpContext* target)
{
	pf_common_copy_settings(client->settings, target->settings);
	/* DesktopResize causes internal function rdp_server_reactivate to be called,
	 * which causes the reactivation.
	 */
	client->update->DesktopResize(client);
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

/**
 * Called before a connection is established.
 *
 * TODO: Take client to proxy settings and use channel whitelist to filter out
 * unwanted channels.
 */
static BOOL pf_client_pre_connect(freerdp* instance)
{
	rdpSettings* settings = instance->settings;
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

	if (!proxy_data_set_connection_info(pc->pdata, ps->settings, settings))
	{
		WLog_ERR(TAG, "proxy_data_set_connection_info failed!");
		return FALSE;
	}

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

	SetEvent(pdata->connectionClosed);
	/* It's important to avoid calling `freerdp_peer_context_free` and `freerdp_peer_free` here,
	 * in order to avoid double-free. Those objects will be freed by the server when needed.
	 */
}

/**
 * RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends.
 */
static DWORD WINAPI pf_client_thread_proc(LPVOID arg)
{
	freerdp* instance = (freerdp*)arg;
	DWORD nCount;
	DWORD status;
	HANDLE handles[64];

	if (!freerdp_connect(instance))
	{
		WLog_ERR(TAG, "connection failure");
		return 0;
	}

	while (!freerdp_shall_disconnect(instance))
	{
		nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

		if (nCount == 0)
		{
			WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
			break;
		}

		status = WaitForMultipleObjects(nCount, handles, FALSE, 100);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %"PRIu32"", __FUNCTION__,
			         status);
			break;
		}

		if (freerdp_shall_disconnect(instance))
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
	pClientContext* pc;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	pc = (pClientContext*) instance->context;
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
	pServerContext* ps = pc->pdata->ps;
	freerdp_abort_connect(context->instance);

	if (ps->thread)
	{
		WaitForSingleObject(ps->thread, INFINITE);
		CloseHandle(ps->thread);
		ps->thread = NULL;
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
