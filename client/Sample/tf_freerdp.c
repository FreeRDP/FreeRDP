/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test UI
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016,2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016,2018 Thincast Technologies GmbH
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

#include "tf_channels.h"
#include "tf_freerdp.h"

#define TAG CLIENT_TAG("sample")

/* This function is called whenever a new frame starts.
 * It can be used to reset invalidated areas. */
static BOOL tf_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

/* This function is called when the library completed composing a new
 * frame. Read out the changed areas and blit them to your output device.
 * The image buffer will have the format specified by gdi_init
 */
static BOOL tf_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	return TRUE;
}

/* This function is called to output a System BEEP */
static BOOL tf_play_sound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound)
{
	/* TODO: Implement */
	WINPR_UNUSED(context);
	WINPR_UNUSED(play_sound);
	return TRUE;
}

/* This function is called to update the keyboard indocator LED */
static BOOL tf_keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	/* TODO: Set local keyboard indicator LED status */
	WINPR_UNUSED(context);
	WINPR_UNUSED(led_flags);
	return TRUE;
}

/* This function is called to set the IME state */
static BOOL tf_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                       UINT32 imeConvMode)
{
	if (!context)
		return FALSE;

	WLog_WARN(TAG,
	          "KeyboardSetImeStatus(unitId=%04" PRIx16 ", imeState=%08" PRIx32
	          ", imeConvMode=%08" PRIx32 ") ignored",
	          imeId, imeState, imeConvMode);
	return TRUE;
}

/* Called before a connection is established.
 * Set all configuration options to support and load channels here. */
static BOOL tf_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	settings = instance->settings;
	/* Optional OS identifier sent to server */
	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;
	/* settings->OrderSupport is initialized at this point.
	 * Only override it if you plan to implement custom order
	 * callbacks or deactiveate certain features. */
	/* Register the channel listeners.
	 * They are required to set up / tear down channels if they are loaded. */
	PubSub_SubscribeChannelConnected(instance->context->pubSub, tf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    tf_OnChannelDisconnectedEventHandler);

	/* Load all required plugins / channels / libraries specified by current
	 * settings. */
	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
		return FALSE;

	/* TODO: Any code your client requires */
	return TRUE;
}

/* Called after a RDP connection was successfully established.
 * Settings might have changed during negociation of client / server feature
 * support.
 *
 * Set up local framebuffers and paing callbacks.
 * If required, register pointer callbacks to change the local mouse cursor
 * when hovering over the RDP window
 */
static BOOL tf_post_connect(freerdp* instance)
{
	if (!gdi_init(instance, PIXEL_FORMAT_XRGB32))
		return FALSE;

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;
	instance->update->PlaySound = tf_play_sound;
	instance->update->SetKeyboardIndicators = tf_keyboard_set_indicators;
	instance->update->SetKeyboardImeStatus = tf_keyboard_set_ime_status;
	return TRUE;
}

/* This function is called whether a session ends by failure or success.
 * Clean up everything allocated by pre_connect and post_connect.
 */
static void tf_post_disconnect(freerdp* instance)
{
	tfContext* context;

	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (tfContext*)instance->context;
	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   tf_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      tf_OnChannelDisconnectedEventHandler);
	gdi_free(instance);
	/* TODO : Clean up custom stuff */
	WINPR_UNUSED(context);
}

/* RDP main loop.
 * Connects RDP, loops while running and handles event and dispatch, cleans up
 * after the connection ends. */
static DWORD WINAPI tf_client_thread_proc(LPVOID arg)
{
	freerdp* instance = (freerdp*)arg;
	DWORD nCount;
	DWORD status;
	DWORD result = 0;
	HANDLE handles[64];
	BOOL rc = freerdp_connect(instance);

	if (instance->settings->AuthenticationOnly)
	{
		result = freerdp_get_last_error(instance->context);
		freerdp_abort_connect(instance);
		WLog_ERR(TAG, "Authentication only, exit status 0x%08" PRIx32 "", result);
		goto disconnect;
	}

	if (!rc)
	{
		result = freerdp_get_last_error(instance->context);
		WLog_ERR(TAG, "connection failure 0x%08" PRIx32, result);
		return result;
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
			WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %" PRIu32 "", __FUNCTION__,
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

disconnect:
	freerdp_disconnect(instance);
	return result;
}

/* Optional global initializer.
 * Here we just register a signal handler to print out stack traces
 * if available. */
static BOOL tf_client_global_init(void)
{
	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

/* Optional global tear down */
static void tf_client_global_uninit(void)
{
}

static int tf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	tfContext* tf;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);

	if (!instance || !instance->context)
		return -1;

	tf = (tfContext*)instance->context;
	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	WINPR_UNUSED(tf);

	return 1;
}

static BOOL tf_client_new(freerdp* instance, rdpContext* context)
{
	tfContext* tf = (tfContext*)context;

	if (!instance || !context)
		return FALSE;

	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;
	instance->PostDisconnect = tf_post_disconnect;
	instance->Authenticate = client_cli_authenticate;
	instance->GatewayAuthenticate = client_cli_gw_authenticate;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->LogonErrorInfo = tf_logon_error_info;
	/* TODO: Client display set up */
	WINPR_UNUSED(tf);
	return TRUE;
}

static void tf_client_free(freerdp* instance, rdpContext* context)
{
	tfContext* tf = (tfContext*)instance->context;

	if (!context)
		return;

	/* TODO: Client display tear down */
	WINPR_UNUSED(tf);
}

static int tf_client_start(rdpContext* context)
{
	/* TODO: Start client related stuff */
	WINPR_UNUSED(context);
	return 0;
}

static int tf_client_stop(rdpContext* context)
{
	/* TODO: Stop client related stuff */
	WINPR_UNUSED(context);
	return 0;
}

static int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	ZeroMemory(pEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	pEntryPoints->Version = RDP_CLIENT_INTERFACE_VERSION;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = tf_client_global_init;
	pEntryPoints->GlobalUninit = tf_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(tfContext);
	pEntryPoints->ClientNew = tf_client_new;
	pEntryPoints->ClientFree = tf_client_free;
	pEntryPoints->ClientStart = tf_client_start;
	pEntryPoints->ClientStop = tf_client_stop;
	return 0;
}

int main(int argc, char* argv[])
{
	int rc = -1;
	DWORD status;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;
	rdpContext* context;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);

	if (!context)
		goto fail;

	status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE);
	status =
	    freerdp_client_settings_command_line_status_print(context->settings, status, argc, argv);

	if (status)
		return 0;

	if (freerdp_client_start(context) != 0)
		goto fail;

	rc = tf_client_thread_proc(context->instance);

	if (freerdp_client_stop(context) != 0)
		rc = -1;

fail:
	freerdp_client_context_free(context);
	return rc;
}
