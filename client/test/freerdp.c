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
#include <freerdp/chanman/chanman.h>
#include <freerdp/plugins/cliprdr.h>

#define SET_TFI(_instance, _tfi) (_instance)->param1 = _tfi
#define GET_TFI(_instance) ((tfInfo*) ((_instance)->param1))

#define SET_CHANMAN(_instance, _chanman) (_instance)->param2 = _chanman
#define GET_CHANMAN(_instance) ((rdpChanMan*) ((_instance)->param2))

struct tf_info
{
	void* data;
};
typedef struct tf_info tfInfo;

freerdp_sem g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

#include <freerdp/freerdp.h>
#include <freerdp/utils/args.h>

void tf_begin_paint(rdpUpdate* update)
{
	GDI* gdi = GET_GDI(update);
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void tf_end_paint(rdpUpdate* update)
{
	GDI* gdi;
	tfInfo* tfi;

	gdi = GET_GDI(update);
	tfi = GET_TFI(update);

	if (gdi->primary->hdc->hwnd->invalid->null)
		return;
}

int tf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_chanman_data(instance, channelId, data, size, flags, total_size);
}

int tf_process_plugin_args(rdpSettings* settings, const char* name, FRDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChanMan* chanman = (rdpChanMan*) user_data;

	printf("Load plugin %s\n", name);
	freerdp_chanman_load_plugin(chanman, settings, name, plugin_data);

	return 1;
}

void tf_process_cb_sync_event(rdpChanMan* chanman, freerdp* instance)
{
	FRDP_EVENT* event;
	FRDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(FRDP_EVENT_CLASS_CLIPRDR, FRDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	format_list_event = (FRDP_CB_FORMAT_LIST_EVENT*)event;
	format_list_event->num_formats = 0;

	freerdp_chanman_send_event(chanman, event);
}

void tf_process_channel_event(rdpChanMan* chanman, freerdp* instance)
{
	FRDP_EVENT* event;

	event = freerdp_chanman_pop_event(chanman);
	if (event)
	{
		switch (event->event_type)
		{
			case FRDP_EVENT_TYPE_CB_SYNC:
				tf_process_cb_sync_event(chanman, instance);
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
	rdpSettings* settings;

	tfi = (tfInfo*) xzalloc(sizeof(tfInfo));
	SET_TFI(instance, tfi);

	settings = instance->settings;

	settings->order_support[NEG_DSTBLT_INDEX] = True;
	settings->order_support[NEG_PATBLT_INDEX] = True;
	settings->order_support[NEG_SCRBLT_INDEX] = True;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = True;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = True;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = True;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = True;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = True;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = True;
	settings->order_support[NEG_LINETO_INDEX] = True;
	settings->order_support[NEG_POLYLINE_INDEX] = True;
	settings->order_support[NEG_MEMBLT_INDEX] = True;
	settings->order_support[NEG_MEM3BLT_INDEX] = True;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
	settings->order_support[NEG_POLYGON_SC_INDEX] = True;
	settings->order_support[NEG_POLYGON_CB_INDEX] = True;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = True;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = True;

	freerdp_chanman_pre_connect(GET_CHANMAN(instance), instance);

	return True;
}

boolean tf_post_connect(freerdp* instance)
{
	GDI* gdi;
	tfInfo* tfi;

	tfi = GET_TFI(instance);
	SET_TFI(instance->update, tfi);

	gdi_init(instance, CLRCONV_ALPHA | CLRBUF_16BPP | CLRBUF_32BPP);
	gdi = GET_GDI(instance->update);

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;

	freerdp_chanman_post_connect(GET_CHANMAN(instance), instance);

	return True;
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
	rdpChanMan* chanman;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	chanman = GET_CHANMAN(instance);

	instance->Connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_chanman_get_fds(chanman, instance, rfds, &rcount, wfds, &wcount) != True)
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

		if (instance->CheckFileDescriptor(instance) != True)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_chanman_check_fds(chanman, instance) != True)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		tf_process_channel_event(chanman, instance);
	}

	freerdp_chanman_close(chanman, instance);
	freerdp_chanman_free(chanman);
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
	rdpChanMan* chanman;

	freerdp_chanman_global_init();

	g_sem = freerdp_sem_new(1);

	instance = freerdp_new();
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;
	instance->ReceiveChannelData = tf_receive_channel_data;

	chanman = freerdp_chanman_new();
	SET_CHANMAN(instance, chanman);

	freerdp_parse_args(instance->settings, argc, argv, tf_process_plugin_args, chanman, NULL, NULL);

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	freerdp_chanman_global_uninit();

	return 0;
}
