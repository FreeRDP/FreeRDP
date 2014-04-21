/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test UI
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/event.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

struct tf_info
{
	void* data;
};
typedef struct tf_info tfInfo;

struct tf_context
{
	rdpContext _p;

	tfInfo* tfi;
};
typedef struct tf_context tfContext;

HANDLE g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

int tf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
	return 0;
}

void tf_context_free(freerdp* instance, rdpContext* context)
{

}

void tf_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void tf_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return;
}

int tf_receive_channel_data(freerdp* instance, UINT16 channelId, BYTE* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

void tf_process_cb_monitor_ready_event(rdpChannels* channels, freerdp* instance)
{
	wMessage* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_FormatList, NULL, NULL);

	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 0;

	freerdp_channels_send_event(channels, event);
}

void tf_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	wMessage* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (GetMessageType(event->id))
		{
			case CliprdrChannel_MonitorReady:
				tf_process_cb_monitor_ready_event(channels, instance);
				break;

			default:
				printf("tf_process_channel_event: unknown event type %d\n", GetMessageType(event->id));
				break;
		}

		freerdp_event_free(event);
	}
}

BOOL tf_pre_connect(freerdp* instance)
{
	tfInfo* tfi;
	tfContext* context;
	rdpSettings* settings;

	context = (tfContext*) instance->context;

	tfi = (tfInfo*) malloc(sizeof(tfInfo));
	ZeroMemory(tfi, sizeof(tfInfo));

	context->tfi = tfi;

	settings = instance->settings;

	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

BOOL tf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

int tfreerdp_run(freerdp* instance)
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
	rdpChannels* channels;

	channels = instance->context->channels;

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		ZeroMemory(rfds, sizeof(rfds));
		ZeroMemory(wfds, sizeof(wfds));
		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get channel manager file descriptor\n");
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
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("tfreerdp_run: select failed\n");
				break;
			}
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		tf_process_channel_event(channels, instance);
	}

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_free(instance);

	return 0;
}

void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	tfreerdp_run(data->instance);

	free(data);

	pthread_detach(pthread_self());

	g_thread_count--;

        if (g_thread_count < 1)
        	ReleaseSemaphore(g_sem, 1, NULL);

	return NULL;
}

int main(int argc, char* argv[])
{
	int status;
	pthread_t thread;
	freerdp* instance;
	rdpChannels* channels;
	struct thread_data* data;

	freerdp_channels_global_init();

	g_sem = CreateSemaphore(NULL, 0, 1, NULL);

	instance = freerdp_new();
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;
	instance->ReceiveChannelData = tf_receive_channel_data;

	instance->ContextSize = sizeof(tfContext);
	instance->ContextNew = tf_context_new;
	instance->ContextFree = tf_context_free;
	freerdp_context_new(instance);

	channels = instance->context->channels;

	status = freerdp_client_settings_parse_command_line(instance->settings, argc, argv);

	if (status < 0)
		exit(0);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	data = (struct thread_data*) malloc(sizeof(struct thread_data));
	ZeroMemory(data, sizeof(sizeof(struct thread_data)));

	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
		WaitForSingleObject(g_sem, INFINITE);
	}

	freerdp_channels_global_uninit();

	return 0;
}
