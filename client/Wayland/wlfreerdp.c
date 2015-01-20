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

int wl_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();

	return 0;
}

void wl_context_free(freerdp* instance, rdpContext* context)
{

}

void wl_begin_paint(rdpContext* context)
{
	rdpGdi* gdi;

	gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void wl_end_paint(rdpContext* context)
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
		return;

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

	wlf_RefreshDisplay(display);
}

BOOL wl_pre_connect(freerdp* instance)
{
	wlfDisplay* display;
	wlfInput* input;
	wlfContext* context;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	context = (wlfContext*) instance->context;

	display = wlf_CreateDisplay();
	context->display = display;

	input = wlf_CreateInput(context);
	context->input = input;

	return TRUE;
}

BOOL wl_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	wlfWindow* window;
	wlfContext* context;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	context = (wlfContext*) instance->context;
	window = wlf_CreateDesktopWindow(context, "FreeRDP", gdi->width, gdi->height, FALSE);

	 /* fill buffer with first image here */
	window->data = malloc (gdi->width * gdi->height *4);
	memcpy(window->data, (void*) gdi->primary_buffer, gdi->width * gdi->height * 4);
	instance->update->BeginPaint = wl_begin_paint;
	instance->update->EndPaint = wl_end_paint;

	 /* put Wayland data in the context here */
	context->window = window;

	freerdp_channels_post_connect(instance->context->channels, instance);

	wlf_UpdateWindowArea(context, window, 0, 0, gdi->width, gdi->height);

	return TRUE;
}

BOOL wl_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	char answer;

	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired. "
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	while (1)
	{
		printf("Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.");
			if (instance->settings->CredentialsFromStdin)
				printf(" - Run without parameter \"--from-stdin\" to set trust.");
			printf("\n");
			return FALSE;
		}

		if (answer == 'y' || answer == 'Y')
		{
			return TRUE;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
		printf("\n");
	}

	return FALSE;
}

int wlfreerdp_run(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(wfds, sizeof(wfds));

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;
		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor");
			break;
		}
		if (freerdp_channels_get_fds(instance->context->channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, NULL) == -1)
		{
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR)))
			{
				printf("wlfreerdp_run: select failed\n");
				break;
			}
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(instance->context->channels, instance) != TRUE)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
	}

	wlfContext* context;

	context = (wlfContext*) instance->context;
	wlf_DestroyWindow(context, context->window);
	wlf_DestroyInput(context, context->input);
	wlf_DestroyDisplay(context, context->display);

	freerdp_channels_disconnect(instance->context->channels, instance);
	freerdp_disconnect(instance);

	freerdp_channels_close(instance->context->channels, instance);
	freerdp_channels_free(instance->context->channels);
	freerdp_free(instance);

	return 0;
}

int main(int argc, char* argv[])
{
	int status;
	freerdp* instance;

	instance = freerdp_new();
	instance->PreConnect = wl_pre_connect;
	instance->PostConnect = wl_post_connect;
	instance->VerifyCertificate = wl_verify_certificate;

	instance->ContextSize = sizeof(wlfContext);
	instance->ContextNew = wl_context_new;
	instance->ContextFree = wl_context_free;
	freerdp_context_new(instance);

	status = freerdp_client_settings_parse_command_line_arguments(instance->settings, argc, argv);

	status = freerdp_client_settings_command_line_status_print(instance->settings, status, argc, argv);

	if (status)
		exit(0);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	wlfreerdp_run(instance);

	return 0;
}
