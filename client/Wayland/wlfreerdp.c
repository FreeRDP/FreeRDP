/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Client
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
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

#include <stdio.h>
#include <errno.h>

#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>
#include <freerdp/gdi/gdi.h>
#include <linux/input.h>

#include "wlfreerdp.h"
#include "wlf_input.h"

UwacDisplay *g_display;
HANDLE g_displayHandle;

static BOOL wl_context_new(freerdp* instance, rdpContext* context)
{
	if (!(context->channels = freerdp_channels_new()))
		return FALSE;

	return TRUE;
}

static void wl_context_free(freerdp* instance, rdpContext* context)
{
	if (context && context->channels)
	{
		freerdp_channels_close(context->channels, instance);
		freerdp_channels_free(context->channels);
		context->channels = NULL;
	}
}

BOOL wl_update_content(wlfContext *context_w)
{
	if (!context_w->waitingFrameDone && context_w->haveDamage)
	{
		UwacWindowSubmitBuffer(context_w->window, true);
		context_w->waitingFrameDone = TRUE;
		context_w->haveDamage = FALSE;
	}

	return TRUE;
}

static BOOL wl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;

	gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	return TRUE;
}


static BOOL wl_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	char *data;
	wlfContext *context_w;
	INT32 x, y;
	UINT32 w, h;
	int i;

	gdi = context->gdi;
	if (gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	x = gdi->primary->hdc->hwnd->invalid->x;
	y = gdi->primary->hdc->hwnd->invalid->y;
	w = gdi->primary->hdc->hwnd->invalid->w;
	h = gdi->primary->hdc->hwnd->invalid->h;

	context_w = (wlfContext*) context;

	data = UwacWindowGetDrawingBuffer(context_w->window);
	if (!data)
		return FALSE;

	for (i = 0; i < h; i++)
	{
		memcpy(data + ((i+y)*(gdi->width*4)) + x*4,
		       gdi->primary_buffer + ((i+y)*(gdi->width*4)) + x*4,
		       w*4);
	}

	if (UwacWindowAddDamage(context_w->window, x, y, w, h) != UWAC_SUCCESS)
		return FALSE;

	context_w->haveDamage = TRUE;
	return wl_update_content(context_w);
}


static BOOL wl_pre_connect(freerdp* instance)
{
	wlfContext* context;

	if (freerdp_channels_pre_connect(instance->context->channels, instance) != CHANNEL_RC_OK)
		return FALSE;

	context = (wlfContext*) instance->context;
	if (!context)
		return FALSE;

	context->display = g_display;

	return TRUE;
}

static BOOL wl_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	UwacWindow* window;
	wlfContext* context;

	if (!gdi_init(instance, CLRCONV_ALPHA | CLRBUF_32BPP, NULL))
		return FALSE;

	gdi = instance->context->gdi;
	if (!gdi)
		return FALSE;

	context = (wlfContext*) instance->context;
	context->window = window = UwacCreateWindowShm(context->display, gdi->width, gdi->height, WL_SHM_FORMAT_XRGB8888);
	if (!window)
		return FALSE;

	UwacWindowSetTitle(window, "FreeRDP");

	instance->update->BeginPaint = wl_begin_paint;
	instance->update->EndPaint = wl_end_paint;

	if (freerdp_channels_post_connect(instance->context->channels, instance) != CHANNEL_RC_OK)
		return FALSE;


	memcpy(UwacWindowGetDrawingBuffer(context->window), gdi->primary_buffer, gdi->width * gdi->height * 4);
	UwacWindowAddDamage(context->window, 0, 0, gdi->width, gdi->height);
	context->haveDamage = TRUE;
	return wl_update_content(context);
}

static void wl_post_disconnect(freerdp* instance)
{
	wlfContext *context;
	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (wlfContext*) instance->context;

	gdi_free(instance);
	if (context->window)
		UwacDestroyWindow(&context->window);

	if (context->display)
		UwacCloseDisplay(&context->display);

}

static BOOL handle_uwac_events(freerdp* instance, UwacDisplay *display) {
	UwacEvent event;
	wlfContext *context;

	if (UwacDisplayDispatch(display, 1) < 0)
		return FALSE;

	while (UwacHasEvent(display))
	{
		if (UwacNextEvent(display, &event) != UWAC_SUCCESS)
			return FALSE;

		/*printf("UWAC event type %d\n", event.type);*/
		switch (event.type) {
		case UWAC_EVENT_FRAME_DONE:
			if (!instance)
				continue;

			context = (wlfContext *)instance->context;
			context->waitingFrameDone = FALSE;
			if (context->haveDamage && !wl_end_paint(instance->context))
				return FALSE;
			break;
		case UWAC_EVENT_POINTER_ENTER:
			if (!wlf_handle_pointer_enter(instance, &event.mouse_enter_leave))
				return FALSE;
			break;
		case UWAC_EVENT_POINTER_MOTION:
			if (!wlf_handle_pointer_motion(instance, &event.mouse_motion))
				return FALSE;
			break;
		case UWAC_EVENT_POINTER_BUTTONS:
			if (!wlf_handle_pointer_buttons(instance, &event.mouse_button))
				return FALSE;
			break;
		case UWAC_EVENT_POINTER_AXIS:
			if (!wlf_handle_pointer_axis(instance, &event.mouse_axis))
				return FALSE;
			break;
		case UWAC_EVENT_KEY:
			if (!wlf_handle_key(instance, &event.key))
				return FALSE;
			break;
		case UWAC_EVENT_KEYBOARD_ENTER:
			if (!wlf_keyboard_enter(instance, &event.keyboard_enter_leave))
				return FALSE;
			break;
		default:
			break;
		}
	}
	return TRUE;
}

static int wlfreerdp_run(freerdp* instance)
{
	DWORD count;
	HANDLE handles[64];
	DWORD status;

	if (!freerdp_connect(instance))
	{
			printf("Failed to connect\n");
			return -1;
	}

	handle_uwac_events(instance, g_display);

	while (!freerdp_shall_disconnect(instance))
	{
		handles[0] = g_displayHandle;

		count = freerdp_get_event_handles(instance->context, &handles[1], 63);
		if (!count)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		status = WaitForMultipleObjects(count+1, handles, FALSE, INFINITE);
		if (WAIT_FAILED == status)
		{
			printf("%s: WaitForMultipleObjects failed\n", __FUNCTION__);
			break;
		}

		if (!handle_uwac_events(instance, g_display)) {
			printf("error handling UWAC events\n");
			break;
		}

		//if (WaitForMultipleObjects(count, &handles[1], FALSE, INFINITE)) {
			if (freerdp_check_event_handles(instance->context) != TRUE)
			{
				printf("Failed to check FreeRDP file descriptor\n");
				break;
			}
		//}

	}

	freerdp_channels_disconnect(instance->context->channels, instance);
	freerdp_disconnect(instance);

	return 0;
}

int main(int argc, char* argv[])
{
	UwacReturnCode status;
	freerdp* instance;

	g_display = UwacOpenDisplay(NULL, &status);
	if (!g_display)
		exit(1);

	g_displayHandle = CreateFileDescriptorEvent(NULL, FALSE, FALSE, UwacDisplayGetFd(g_display), WINPR_FD_READ);
	if (!g_displayHandle)
		exit(1);

	//if (!handle_uwac_events(NULL, g_display))
	//	exit(1);

	instance = freerdp_new();
	instance->PreConnect = wl_pre_connect;
	instance->PostConnect = wl_post_connect;
	instance->PostDisconnect = wl_post_disconnect;
	instance->Authenticate = client_cli_authenticate;
	instance->GatewayAuthenticate = client_cli_gw_authenticate;
	instance->VerifyCertificate = client_cli_verify_certificate;
	instance->VerifyChangedCertificate = client_cli_verify_changed_certificate;

	instance->ContextSize = sizeof(wlfContext);
	instance->ContextNew = wl_context_new;
	instance->ContextFree = wl_context_free;

	freerdp_context_new(instance);

	status = freerdp_client_settings_parse_command_line(instance->settings, argc, argv, FALSE);

	status = freerdp_client_settings_command_line_status_print(instance->settings, status, argc, argv);

	if (status)
		exit(0);

	if (!freerdp_client_load_addins(instance->context->channels, instance->settings))
		exit(-1);

	wlfreerdp_run(instance);

	freerdp_context_free(instance);

	freerdp_free(instance);

	return 0;
}
