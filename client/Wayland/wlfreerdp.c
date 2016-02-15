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

#include "wlfreerdp.h"

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
	wlfDisplay* display;
	wlfWindow* window;
	wlfContext* context_w;
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
	display = context_w->display;
	window = context_w->window;

	for (i = 0; i < h; i++)
		memcpy(window->data + ((i+y)*(gdi->width*4)) + x*4,
		       gdi->primary_buffer + ((i+y)*(gdi->width*4)) + x*4,
		       w*4);

	return wlf_RefreshDisplay(display);
}

static BOOL wl_pre_connect(freerdp* instance)
{
	wlfDisplay* display;
	wlfInput* input;
	wlfContext* context;

	if (freerdp_channels_pre_connect(instance->context->channels, instance))
		return FALSE;

	context = (wlfContext*) instance->context;
	if (!context)
		return FALSE;

	display = wlf_CreateDisplay();
	if (!display)
		return FALSE;

	context->display = display;

	input = wlf_CreateInput(context);
	if (!input)
		return FALSE;

	context->input = input;

	return TRUE;
}

static BOOL wl_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	wlfWindow* window;
	wlfContext* context;

	if (!gdi_init(instance, CLRCONV_ALPHA | CLRBUF_32BPP, NULL))
		return FALSE;

	gdi = instance->context->gdi;
	if (!gdi)
		return FALSE;

	context = (wlfContext*) instance->context;
	window = wlf_CreateDesktopWindow(context, "FreeRDP", gdi->width, gdi->height, FALSE);
	if (!window)
		return FALSE;

	 /* fill buffer with first image here */
	window->data = malloc (gdi->width * gdi->height *4);
	if (!window->data)
		return FALSE;

	memcpy(window->data, (void*) gdi->primary_buffer, gdi->width * gdi->height * 4);
	instance->update->BeginPaint = wl_begin_paint;
	instance->update->EndPaint = wl_end_paint;

	 /* put Wayland data in the context here */
	context->window = window;

	if (freerdp_channels_post_connect(instance->context->channels, instance) < 0)
		return FALSE;

	wlf_UpdateWindowArea(context, window, 0, 0, gdi->width, gdi->height);

	return TRUE;
}

static void wl_post_disconnect(freerdp* instance)
{
	wlfContext *context;
	if (!instance)
		return;

	if (!instance->context)
		return;

	context = (wlfContext*) instance->context;

	if (context->display)
		wlf_DestroyDisplay(context, context->display);

	if (context->input)
		wlf_DestroyInput(context, context->input);

	gdi_free(instance);
	if (context->window)
		wlf_DestroyWindow(context, context->window);
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

	while (!freerdp_shall_disconnect(instance))
	{
		count = freerdp_get_event_handles(instance->context, handles, 64);
		if (!count)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
		if (WAIT_FAILED == status)
		{
			printf("%s: WaitForMultipleObjects failed\n", __FUNCTION__);
			break;
		}

		if (freerdp_check_event_handles(instance->context) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	freerdp_channels_disconnect(instance->context->channels, instance);
	freerdp_disconnect(instance);

	return 0;
}

int main(int argc, char* argv[])
{
	int status;
	freerdp* instance;

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

	status = freerdp_client_settings_parse_command_line_arguments(instance->settings, argc, argv, FALSE);

	status = freerdp_client_settings_command_line_status_print(instance->settings, status, argc, argv);

	if (status)
		exit(0);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	wlfreerdp_run(instance);

	freerdp_context_free(instance);

	freerdp_free(instance);

	return 0;
}
