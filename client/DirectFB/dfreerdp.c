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

	dfi->update_rect.x = gdi->primary->hdc->hwnd->invalid->x;
	dfi->update_rect.y = gdi->primary->hdc->hwnd->invalid->y;
	dfi->update_rect.w = gdi->primary->hdc->hwnd->invalid->w;
	dfi->update_rect.h = gdi->primary->hdc->hwnd->invalid->h;

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

	dfi = (dfInfo*) xzalloc(sizeof(dfInfo));
	SET_DFI(instance, dfi);

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

	return True;
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

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	printf("DirectFB Run\n");

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
	}

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

	g_sem = freerdp_sem_new(1);

	instance = freerdp_new();
	instance->PreConnect = df_pre_connect;
	instance->PostConnect = df_post_connect;

	DirectFBInit(&argc, &argv);
	freerdp_parse_args(instance->settings, argc, argv, NULL, NULL, NULL, NULL);

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	return 0;
}
