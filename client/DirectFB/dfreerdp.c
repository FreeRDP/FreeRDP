/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <pthread.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/event.h>
#include <freerdp/constants.h>

#include "df_event.h"

#include "dfreerdp.h"

freerdp_sem g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

void df_begin_paint(rdpUpdate* update)
{
	GDI* gdi;

	gdi = GET_GDI(update);
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void df_end_paint(rdpUpdate* update)
{
	GDI* gdi;
	dfInfo* dfi;

	gdi = GET_GDI(update);
	dfi = GET_DFI(update);

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

boolean df_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	dfInfo* dfi;

	dfi = GET_DFI(instance);

	rfds[*rcount] = (void*)(long)(dfi->read_fds);
	(*rcount)++;

	return True;
}

boolean df_check_fds(freerdp* instance, fd_set* set)
{
	dfInfo* dfi;

	dfi = GET_DFI(instance);

	if (!FD_ISSET(dfi->read_fds, set))
		return True;

	if (read(dfi->read_fds, &(dfi->event), sizeof(dfi->event)) > 0)
		df_event_process(instance, &(dfi->event));

	return True;
}

boolean df_pre_connect(freerdp* instance)
{
	dfInfo* dfi;
	rdpSettings* settings;

	dfi = (dfInfo*) xzalloc(sizeof(dfInfo));
	SET_DFI(instance, dfi);

	settings = instance->settings;

	settings->order_support[NEG_DSTBLT_INDEX] = True;
	settings->order_support[NEG_PATBLT_INDEX] = True;
	settings->order_support[NEG_SCRBLT_INDEX] = True;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = True;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = False;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = False;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = False;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_LINETO_INDEX] = True;
	settings->order_support[NEG_POLYLINE_INDEX] = True;
	settings->order_support[NEG_MEMBLT_INDEX] = True;
	settings->order_support[NEG_MEM3BLT_INDEX] = False;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
	settings->order_support[NEG_POLYGON_SC_INDEX] = False;
	settings->order_support[NEG_POLYGON_CB_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = False;

	freerdp_chanman_pre_connect(GET_CHANMAN(instance), instance);

	return True;
}

boolean df_post_connect(freerdp* instance)
{
	GDI* gdi;
	dfInfo* dfi;

	dfi = GET_DFI(instance);
	SET_DFI(instance->update, dfi);

	gdi_init(instance, CLRCONV_ALPHA | CLRBUF_16BPP | CLRBUF_32BPP);
	gdi = GET_GDI(instance->update);

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

	freerdp_chanman_post_connect(GET_CHANMAN(instance), instance);

	return True;
}

static int df_process_plugin_args(rdpSettings* settings, const char* name,
	FRDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChanMan* chanman = (rdpChanMan*) user_data;

	printf("Load plugin %s\n", name);
	freerdp_chanman_load_plugin(chanman, settings, name, plugin_data);

	return 1;
}

static int
df_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_chanman_data(instance, channelId, data, size, flags, total_size);
}

static void
df_process_cb_sync_event(rdpChanMan* chanman, freerdp* instance)
{
	FRDP_EVENT* event;
	FRDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(FRDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	format_list_event = (FRDP_CB_FORMAT_LIST_EVENT*)event;
	format_list_event->num_formats = 0;

	freerdp_chanman_send_event(chanman, "cliprdr", event);
}

static void
df_process_channel_event(rdpChanMan* chanman, freerdp* instance)
{
	FRDP_EVENT* event;

	event = freerdp_chanman_pop_event(chanman);
	if (event)
	{
		switch (event->event_type)
		{
			case FRDP_EVENT_TYPE_CB_SYNC:
				df_process_cb_sync_event(chanman, instance);
				break;
			default:
				printf("df_process_channel_event: unknown event type %d\n", event->event_type);
				break;
		}
		freerdp_event_free(event);
	}
}

int dfreerdp_run(freerdp* instance)
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
		if (df_get_fds(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get dfreerdp file descriptor\n");
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
				printf("dfreerdp_run: select failed\n");
				break;
			}
		}

		if (instance->CheckFileDescriptor(instance) != True)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (df_check_fds(instance, &rfds_set) != True)
		{
			printf("Failed to check dfreerdp file descriptor\n");
			break;
		}
		if (freerdp_chanman_check_fds(chanman, instance) != True)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		df_process_channel_event(chanman, instance);
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

	dfreerdp_run(data->instance);

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
	instance->PreConnect = df_pre_connect;
	instance->PostConnect = df_post_connect;
	instance->ReceiveChannelData = df_receive_channel_data;

	chanman = freerdp_chanman_new();
	SET_CHANMAN(instance, chanman);

	DirectFBInit(&argc, &argv);
	freerdp_parse_args(instance->settings, argc, argv, df_process_plugin_args, chanman, NULL, NULL);

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
