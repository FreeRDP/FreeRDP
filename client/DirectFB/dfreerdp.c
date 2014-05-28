/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * DirectFB Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <locale.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/utils/event.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "df_event.h"
#include "df_graphics.h"

#include "dfreerdp.h"

struct thread_data
{
	freerdp* instance;
};

int df_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
	return 0;
}

void df_context_free(freerdp* instance, rdpContext* context)
{

}

void df_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void df_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	dfInfo* dfi;

	gdi = context->gdi;
	dfi = ((dfContext*) context)->dfi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return;

#if 1
	dfi->update_rect.x = gdi->primary->hdc->hwnd->invalid->x;
	dfi->update_rect.y = gdi->primary->hdc->hwnd->invalid->y;
	dfi->update_rect.w = gdi->primary->hdc->hwnd->invalid->w;
	dfi->update_rect.h = gdi->primary->hdc->hwnd->invalid->h;
#else
	dfi->update_rect.x = 0;
	dfi->update_rect.y = 0;
	dfi->update_rect.w = gdi->width;
	dfi->update_rect.h = gdi->height;
#endif

	dfi->primary->Blit(dfi->primary, dfi->surface, &(dfi->update_rect), dfi->update_rect.x, dfi->update_rect.y);
}

HANDLE df_get_handle(freerdp* instance)
{
	dfInfo* dfi;

	dfi = ((dfContext*) instance->context)->dfi;

	return CreateFileDescriptorEvent(NULL, FALSE, FALSE, dfi->read_fds);
}

BOOL df_check_handle(freerdp* instance, HANDLE handle)
{
	dfInfo* dfi;

	dfi = ((dfContext*) instance->context)->dfi;

	if (WaitForSingleObject(handle, 0) != WAIT_OBJECT_0)
		return TRUE;

	if (read(dfi->read_fds, &(dfi->event), sizeof(dfi->event)) > 0)
		df_event_process(instance, &(dfi->event));

	return TRUE;
}

BOOL df_pre_connect(freerdp* instance)
{
	dfInfo* dfi;
	BOOL bitmap_cache;
	dfContext* context;
	rdpSettings* settings;

	dfi = (dfInfo*) malloc(sizeof(dfInfo));
	ZeroMemory(dfi, sizeof(dfInfo));

	context = ((dfContext*) instance->context);
	context->dfi = dfi;

	settings = instance->settings;
	bitmap_cache = settings->BitmapCacheEnabled;

	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = FALSE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = FALSE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	dfi->clrconv = (CLRCONV*) malloc(sizeof(CLRCONV));
	ZeroMemory(dfi->clrconv, sizeof(CLRCONV));

	dfi->clrconv->alpha = 1;
	dfi->clrconv->invert = 0;
	dfi->clrconv->rgb555 = 0;

	dfi->clrconv->palette = (rdpPalette*) malloc(sizeof(rdpPalette));
	ZeroMemory(dfi->clrconv->palette, sizeof(rdpPalette));

	freerdp_channels_pre_connect(instance->context->channels, instance);

	instance->context->cache = cache_new(instance->settings);

	return TRUE;
}

BOOL df_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	dfInfo* dfi;
	dfContext* context;

	context = ((dfContext*) instance->context);
	dfi = context->dfi;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	dfi->err = DirectFBCreate(&(dfi->dfb));

	dfi->dsc.flags = DSDESC_CAPS;
	dfi->dsc.caps = DSCAPS_PRIMARY;
	dfi->err = dfi->dfb->CreateSurface(dfi->dfb, &(dfi->dsc), &(dfi->primary));
	dfi->err = dfi->primary->GetSize(dfi->primary, &(gdi->width), &(gdi->height));
	dfi->dfb->SetVideoMode(dfi->dfb, gdi->width, gdi->height, gdi->dstBpp);
	dfi->dfb->CreateInputEventBuffer(dfi->dfb, DICAPS_ALL, DFB_TRUE, &(dfi->event_buffer));
	dfi->event_buffer->CreateFileDescriptor(dfi->event_buffer, &(dfi->read_fds));

	dfi->dfb->GetDisplayLayer(dfi->dfb, 0, &(dfi->layer));
	dfi->layer->EnableCursor(dfi->layer, 1);

	dfi->dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PREALLOCATED | DSDESC_PIXELFORMAT;
	dfi->dsc.caps = DSCAPS_SYSTEMONLY;
	dfi->dsc.width = gdi->width;
	dfi->dsc.height = gdi->height;

	if (gdi->dstBpp == 32 || gdi->dstBpp == 24)
		dfi->dsc.pixelformat = DSPF_AiRGB;
	else if (gdi->dstBpp == 16 || gdi->dstBpp == 15)
		dfi->dsc.pixelformat = DSPF_RGB16;
	else if (gdi->dstBpp == 8)
		dfi->dsc.pixelformat = DSPF_RGB332;
	else
		dfi->dsc.pixelformat = DSPF_AiRGB;

	dfi->dsc.preallocated[0].data = gdi->primary_buffer;
	dfi->dsc.preallocated[0].pitch = gdi->width * gdi->bytesPerPixel;
	dfi->dfb->CreateSurface(dfi->dfb, &(dfi->dsc), &(dfi->surface));

	instance->update->BeginPaint = df_begin_paint;
	instance->update->EndPaint = df_end_paint;

	df_keyboard_init();

	pointer_cache_register_callbacks(instance->update);
	df_register_graphics(instance->context->graphics);

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

BOOL df_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
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

		if (answer == 'y' || answer == 'Y')
		{
			return TRUE;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
	}

	return FALSE;
}

static int df_receive_channel_data(freerdp* instance, UINT16 channelId, BYTE* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

static void df_process_cb_monitor_ready_event(rdpChannels* channels, freerdp* instance)
{
	wMessage* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_FormatList, NULL, NULL);

	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 0;

	freerdp_channels_send_event(channels, event);
}

static void df_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	wMessage* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (GetMessageType(event->id))
		{
			case CliprdrChannel_MonitorReady:
				df_process_cb_monitor_ready_event(channels, instance);
				break;

			default:
				fprintf(stderr, "df_process_channel_event: unknown event type %d\n", GetMessageType(event->id));
				break;
		}

		freerdp_event_free(event);
	}
}

static void df_free(dfInfo* dfi)
{
	dfi->dfb->Release(dfi->dfb);
	free(dfi);
}

int dfreerdp_run(freerdp* instance)
{
	int exit_code = 0;
	DWORD count = 0;
	DWORD event;
	HANDLE *events, dfhandle;
	dfInfo* dfi;
	dfContext* context;
	rdpChannels* channels;

	channels = instance->context->channels;
	events = malloc(5 * sizeof(HANDLE));

	if (!events)
	{
		fprintf(stderr, "malloc failed %s (%d)", strerror(errno), errno);
		exit_code = -1;
		goto cleanup;
	}

	if (!freerdp_connect(instance))
	{
		fprintf(stderr, "freerdp_connect failed!");
		exit_code = -1;
		goto cleanup;
	}

	context = (dfContext*) instance->context;
	channels = instance->context->channels;

	dfi = context->dfi;

	events[count++] = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE);
	events[count++] = freerdp_channels_get_event_handle(instance);
	dfhandle = df_get_handle(instance);
	events[count++] = dfhandle;
	{
		HANDLE *hdl = freerdp_get_event_handles(instance, events, &count);

		if (!hdl)
		{
			fprintf(stderr, "Failed to get freerdp event handles\n");
			exit_code = -1;
			goto disconnect;
		}

		events = hdl;
	}

	while (!freerdp_shall_disconnect(instance))
	{
		event = WaitForMultipleObjects(count, events, FALSE, INFINITE);
		if (WAIT_FAILED == event)
			break;
		if (WAIT_TIMEOUT == event)
			continue;

		if (freerdp_check_fds(instance) != TRUE)
		{
			fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (df_check_handle(instance, dfhandle) != TRUE)
		{
			fprintf(stderr, "Failed to check dfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			fprintf(stderr, "Failed to check channel manager file descriptor\n");
			break;
		}
		df_process_channel_event(channels, instance);
	}

disconnect:
	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	df_free(dfi);
	gdi_free(instance);
	freerdp_disconnect(instance);
	if (!exit_code)
		exit_code = freerdp_error_info(instance);
	freerdp_free(instance);

cleanup:
	if (events)
		free(events);

	return exit_code;
}

void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	dfreerdp_run(data->instance);

	ExitThread(0);
	return NULL;
}

int main(int argc, char* argv[])
{
	int status;
	HANDLE thread;
	freerdp* instance;
	dfContext* context;
	rdpChannels* channels;
	struct thread_data* data;

	setlocale(LC_ALL, "");

	freerdp_channels_global_init();

	instance = freerdp_new();
	instance->PreConnect = df_pre_connect;
	instance->PostConnect = df_post_connect;
	instance->VerifyCertificate = df_verify_certificate;
	instance->ReceiveChannelData = df_receive_channel_data;

	instance->ContextSize = sizeof(dfContext);
	instance->ContextNew = df_context_new;
	instance->ContextFree = df_context_free;
	freerdp_context_new(instance);

	context = (dfContext*) instance->context;
	channels = instance->context->channels;

	DirectFBInit(&argc, &argv);

	instance->context->argc = argc;
	instance->context->argv = argv;

	status = freerdp_client_settings_parse_command_line(instance->settings, argc, argv);

	if (status < 0)
		exit(0);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	data = (struct thread_data*) calloc(1, sizeof(struct thread_data));

	data->instance = instance;

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_func, data, 0, NULL);

	WaitForSingleObject(thread, INFINITE);

	free(data);

	freerdp_channels_global_uninit();

	return 0;
}
