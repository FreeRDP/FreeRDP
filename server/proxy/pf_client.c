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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <freerdp/log.h>

#include "pf_channels.h"
#include "pf_client.h"
#include "pf_context.h"
#include "pf_log.h"

#define TAG PROXY_TAG("client")

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL pf_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL pf_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	return TRUE;
}

/**
 * Called before a connection is established.
 *
 * TODO: Take client to proxy settings and use channel whitelist to filter out
 * unwanted channels.
 */
static BOOL pf_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	settings = instance->settings;

	/* TODO: Consider forwarding this from client. */
	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;

	/**
	 * settings->OrderSupport is initialized at this point.
	 * Only override it if you plan to implement custom order
	 * callbacks or deactiveate certain features.
	 */

	/**
	 * Register the channel listeners.
	 * They are required to set up / tear down channels if they are loaded.
	 */
	PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                 pf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    pf_OnChannelDisconnectedEventHandler);

	/**
	 * Load all required plugins / channels / libraries specified by current
	 * settings.
	 */
	if (!freerdp_client_load_addins(instance->context->channels,
	                                instance->settings))
		return FALSE;

	return TRUE;
}

/**
 * Called after a RDP connection was successfully established.
 * Settings might have changed during negociation of client / server feature
 * support.
 *
 * Set up local framebuffers and painting callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
static BOOL pf_post_connect(freerdp* instance)
{
	if (!gdi_init(instance, PIXEL_FORMAT_XRGB32))
		return FALSE;

	instance->update->BeginPaint = pf_begin_paint;
	instance->update->EndPaint = pf_end_paint;
	return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void pf_post_disconnect(freerdp* instance)
{
	proxyToServerContext* context;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (proxyToServerContext*) instance->context;
	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   pf_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      pf_OnChannelDisconnectedEventHandler);
	gdi_free(instance);
	/* TODO : Clean up custom stuff */
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

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL pf_client_global_init(void)
{
	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

/* Optional global tear down */
static void pf_client_global_uninit(void)
{
}

static int pf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	proxyToServerContext* tf;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	tf = (proxyToServerContext*) instance->context;
	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

static BOOL pf_client_new(freerdp* instance, rdpContext* context)
{
	if (!instance || !context)
		return FALSE;

	instance->PreConnect = pf_pre_connect;
	instance->PostConnect = pf_post_connect;
	instance->PostDisconnect = pf_post_disconnect;
	instance->Authenticate = client_cli_authenticate;

	/* TODO: Use a different auth methods, these are interactive with the client */
	instance->GatewayAuthenticate = client_cli_gw_authenticate;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->LogonErrorInfo = pf_logon_error_info;
	return TRUE;
}


static void pf_client_free(freerdp* instance, rdpContext* context) {}

static int pf_client_start(rdpContext* context) { return 0; }

static int pf_client_stop(rdpContext* context) { return 0; }

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = pf_client_global_init;
	pEntryPoints->GlobalUninit = pf_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(proxyToServerContext);

	/* Client init and finish */
	pEntryPoints->ClientNew = pf_client_new;
	pEntryPoints->ClientFree = pf_client_free;
	pEntryPoints->ClientStart = pf_client_start;
	pEntryPoints->ClientStop = pf_client_stop;
	return 0;
}

/**
 * Starts running a client connection towards target server.
 */
DWORD WINAPI proxy_client_start(LPVOID arg)
{
	rdpContext* context = (rdpContext*)arg;
	int rc = 0;

	if (freerdp_client_start(context) != 0)
		goto fail;

	rc = pf_client_thread_proc(context->instance);

	if (freerdp_client_stop(context) != 0)
		rc = -1;

fail:
	freerdp_client_context_free(context);
	return rc;
}
