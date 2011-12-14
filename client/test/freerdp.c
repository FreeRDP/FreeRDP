/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/event.h>
#include <freerdp/constants.h>
#include <freerdp/channels/channels.h>
#include <freerdp/plugins/cliprdr.h>

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

freerdp_sem g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

#include <freerdp/freerdp.h>
#include <freerdp/utils/args.h>

void tf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
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

int tf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

int tf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChannels* channels = (rdpChannels*) user_data;

	printf("Load plugin %s\n", name);
	freerdp_channels_load_plugin(channels, settings, name, plugin_data);

	return 1;
}

void tf_process_cb_monitor_ready_event(rdpChannels* channels, freerdp* instance)
{
	RDP_EVENT* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 0;

	freerdp_channels_send_event(channels, event);
}

void tf_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	RDP_EVENT* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (event->event_type)
		{
			case RDP_EVENT_TYPE_CB_MONITOR_READY:
				tf_process_cb_monitor_ready_event(channels, instance);
				break;
			default:
				printf("tf_process_channel_event: unknown event type %d\n", event->event_type);
				break;
		}

		freerdp_event_free(event);
	}
}

boolean tf_pre_connect(freerdp* instance)
{
	tfInfo* tfi;
	tfContext* context;
	rdpSettings* settings;

	context = (tfContext*) instance->context;
	tfi = (tfInfo*) xzalloc(sizeof(tfInfo));
	context->tfi = tfi;

	settings = instance->settings;

	settings->order_support[NEG_DSTBLT_INDEX] = true;
	settings->order_support[NEG_PATBLT_INDEX] = true;
	settings->order_support[NEG_SCRBLT_INDEX] = true;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = true;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = true;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = true;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = true;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = true;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = true;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = true;
	settings->order_support[NEG_LINETO_INDEX] = true;
	settings->order_support[NEG_POLYLINE_INDEX] = true;
	settings->order_support[NEG_MEMBLT_INDEX] = true;
	settings->order_support[NEG_MEM3BLT_INDEX] = true;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = true;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = true;
	settings->order_support[NEG_FAST_INDEX_INDEX] = true;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = true;
	settings->order_support[NEG_POLYGON_SC_INDEX] = true;
	settings->order_support[NEG_POLYGON_CB_INDEX] = true;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = true;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = true;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return true;
}

boolean tf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;

	freerdp_channels_post_connect(instance->context->channels, instance);

	return true;
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

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	channels = instance->context->channels;

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get channel manager file descriptor\n");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

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

		if (freerdp_check_fds(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != true)
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

	xfree(data);

	pthread_detach(pthread_self());

	g_thread_count--;

        if (g_thread_count < 1)
                freerdp_sem_signal(&g_sem);

	return NULL;
}

int main(int argc, char* argv[])
{
	pthread_t thread;
	freerdp* instance;
	struct thread_data* data;
	rdpChannels* channels;

	freerdp_channels_global_init();

	g_sem = freerdp_sem_new(1);

	instance = freerdp_new();
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;
	instance->ReceiveChannelData = tf_receive_channel_data;

	instance->context_size = sizeof(tfContext);
	instance->ContextNew = tf_context_new;
	instance->ContextFree = tf_context_free;
	freerdp_context_new(instance);

	channels = instance->context->channels;
	freerdp_parse_args(instance->settings, argc, argv, tf_process_plugin_args, channels, NULL, NULL);

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	freerdp_channels_global_uninit();

	return 0;
}
