/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Killer{R} <support@killprog.com>
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

#include <freerdp/utils/memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <freerdp/gdi/region.h>

#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/event.h>
#include <freerdp/peer.h>
#include <freerdp/constants.h>
#include <freerdp/plugins/cliprdr.h>
#include <freerdp/gdi/region.h>

#include "df_event.h"
#include "df_run.h"
#include "df_graphics.h"
#include "df_vt.h"

#define BUSY_THRESHOLD					500 //milliseconds
#define BUSY_FRAMEDROP_INTERVAL			150 //milliseconds
#define BUSY_INPUT_DEFER_INTERVAL		500 //milliseconds

boolean df_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	dfInfo* dfi;

	dfi = ((dfContext*) instance->context)->dfi;

	rfds[*rcount] = (void*)(long)(dfi->read_fds);
	(*rcount)++;

	return true;
}

boolean df_check_fds(freerdp* instance, fd_set* set)
{
	dfInfo* dfi;
	int r;

	dfi = ((dfContext*) instance->context)->dfi;

	if (!FD_ISSET(dfi->read_fds, set))
		return true;

	for (;;)
	{
		r = read(dfi->read_fds, 
			((char *)&(dfi->event)) + dfi->read_len_pending, 
			sizeof(dfi->event) - dfi->read_len_pending);
		if (r<=0) break;
		dfi->read_len_pending+= r;
		if (dfi->read_len_pending>=sizeof(dfi->event))
		{
			df_event_process(instance, &(dfi->event));
			dfi->read_len_pending = 0;
		}
	}

	return true;
}



static void
df_process_cb_monitor_ready_event(rdpChannels* channels, freerdp* instance)
{
	RDP_EVENT* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*)event;
	format_list_event->num_formats = 0;

	freerdp_channels_send_event(channels, event);
}

void df_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	RDP_EVENT* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (event->event_type)
		{
			case RDP_EVENT_TYPE_CB_MONITOR_READY:
				df_process_cb_monitor_ready_event(channels, instance);
				break;
			default:
				printf("df_process_channel_event: unknown event type %d\n", event->event_type);
				break;
		}

		freerdp_event_free(event);
	}
}

//////////////////

boolean df_lock_fb(dfInfo *dfi, uint8 mask)
{
	if (!dfi->primary_locks)
	{
		dfi->err = dfi->secondary ? 
			dfi->secondary->Lock(dfi->secondary, DSLF_WRITE | DSLF_READ, (void **)&dfi->primary_data, &dfi->primary_pitch) : 
			dfi->primary->Lock(dfi->primary, DSLF_WRITE | DSLF_READ, (void **)&dfi->primary_data, &dfi->primary_pitch);

		if (dfi->err!=DFB_OK)
		{
			printf("DirectFB Lock failed! mask=0x%x err=0x%x\n",  mask, dfi->err);
			return false;
		}
	}

	dfi->primary_locks |= mask;
	return true;
}

boolean df_unlock_fb(dfInfo *dfi, uint8 mask)
{
	if ((dfi->primary_locks&mask)==0)
	{
		return false;
	}

	dfi->primary_locks &= ~mask;

	if (!dfi->primary_locks)
	{
		dfi->primary_data = 0;
		dfi->primary_pitch = 0;
		if (dfi->secondary)
			dfi->secondary->Unlock(dfi->secondary);
		else
			dfi->primary->Unlock(dfi->primary);
	}
	return true;
}


static uint64 get_ticks(void)
{
	uint64 rv;
	struct timeval tv ;
	gettimeofday(&tv, NULL);
	rv = tv.tv_sec;
	rv*= 1000;
	rv+= (uint64)(tv.tv_usec / 1000);
	return rv;
}


static void df_foreground(dfContext* context)
{
	RECTANGLE_16 rect;
	uint8 primary_locks;
	rdpGdi* gdi = ((rdpContext *)context)->gdi;
	dfInfo* dfi = context->dfi;
	rect.left = 0;
	rect.top = 0;
	rect.right = gdi->width - 1;
	rect.bottom = gdi->height - 1;

	((rdpContext *)context)->instance->update->SuppressOutput((rdpContext *)context, true, &rect);
	((rdpContext *)context)->instance->update->RefreshRect((rdpContext *)context, 1, &rect);

	if (dfi->secondary && context->direct_surface)
	{
		primary_locks = dfi->primary_locks;
		if (primary_locks)
			df_unlock_fb(dfi, primary_locks);

		dfi->primary->Blit(dfi->primary, dfi->secondary, NULL, 0, 0);
		dfi->secondary->Release(dfi->secondary);
		dfi->secondary = 0;

		if (primary_locks)
			df_lock_fb(dfi, primary_locks);
	}
	printf("Entered foreground\n");
}

static void df_background(dfContext* context)
{
	RECTANGLE_16 rect;
	uint8 primary_locks;
	rdpGdi* gdi = ((rdpContext *)context)->gdi;
	dfInfo* dfi = context->dfi;
	rect.left = 0;
	rect.top = 0;
	rect.right = gdi->width - 1;
	rect.bottom = gdi->height - 1;
	((rdpContext *)context)->instance->update->SuppressOutput((rdpContext *)context, false, &rect);

	if (((rdpContext *)context)->instance->settings->fullscreen && context->direct_surface)
	{
		primary_locks = dfi->primary_locks;
		if (primary_locks)
			df_unlock_fb(dfi, primary_locks);

		df_create_temp_surface(dfi, gdi->width, gdi->height, gdi->dstBpp, &(dfi->secondary));
		dfi->secondary->Blit(dfi->secondary, dfi->primary, NULL, 0, 0);

		if (primary_locks)
			df_lock_fb(dfi, primary_locks);
	}
	printf("Entered background\n");
}


INLINE static void df_check_for_background(dfContext *context)
{
	if (!context->dfi->tty_background && df_vt_is_disactivated_slow())
	{
		context->dfi->tty_background = true;
		df_background(context);
	}
}

INLINE static void df_check_for_background_while_locked(dfContext *context)
{
	if (!context->dfi->tty_background && df_vt_is_disactivated_fast_unreliable())
	{
		context->dfi->tty_background = true;
		df_background(context);
	}
}

INLINE static void df_update_cursor_region(dfInfo* dfi, rdpGdi* gdi)
{
	df_fullscreen_cursor_bounds(gdi, dfi, &dfi->cursor_region.x1, &dfi->cursor_region.y1, &dfi->cursor_region.x2, &dfi->cursor_region.y2);
}

static void df_fullscreen_cursor_unpaint2(dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;

	dfi->cursor_unpainted = true;
	if (!context->direct_surface)
	{
		gdi_InvalidateRegion(gdi->primary->hdc, dfi->cursor_region.x1, dfi->cursor_region.y1, 
				dfi->cursor_region.x2 - dfi->cursor_region.x1, dfi->cursor_region.y2 - dfi->cursor_region.y1);
		df_fullscreen_cursor_unpaint(gdi->primary_buffer, 0, (dfContext*) context, true);
	}
	else
		df_fullscreen_cursor_unpaint(dfi->primary_data, dfi->primary_pitch, (dfContext*) context, true);

	df_update_cursor_region(dfi, gdi);
}

INLINE static boolean is_cursor_needs_repaint(dfInfo *dfi)
{
	return (dfi->cursor_x != dfi->pointer_x || dfi->cursor_y != dfi->pointer_y || dfi->cursor_id != dfi->cursor_new_id );
}

void df_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	dfInfo* dfi = ((dfContext*) context)->dfi;

	if (!((dfContext*) context)->endpaint_defer_ts)
	{
		df_check_for_background((dfContext*)context);

		gdi->primary->hdc->hwnd->invalid->null = 1;
		gdi->primary->hdc->hwnd->ninvalid = 0;

		if (((dfContext*) context)->direct_surface)
		{
			if (!df_lock_fb(dfi, DF_LOCK_BIT_PAINT))
				abort();//will die anyway

			gdi->primary_buffer = dfi->primary_data;
			gdi->primary->bitmap->data = dfi->primary_data;
		}

		if (context->instance->settings->fullscreen)
		{
			df_update_cursor_region(dfi, gdi);
			if (is_cursor_needs_repaint(dfi))
				df_fullscreen_cursor_unpaint2((dfContext *)context);
		}
	}
}

static void df_end_paint_inner(rdpContext* context)
{
	rdpGdi* gdi;
	dfInfo* dfi;
	int ninvalid;
	HGDI_RGN cinvalid;

	gdi = context->gdi;
	dfi = ((dfContext*) context)->dfi;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;
	ninvalid = gdi->primary->hdc->hwnd->ninvalid;

	if (((dfContext*) context)->direct_surface)
	{
		if (context->instance->settings->fullscreen && dfi->cursor_unpainted && dfi->primary_data)
		{
			df_fullscreen_cursor_save_image_under(dfi->primary_data, dfi->primary_pitch, (dfContext *)context);
			df_fullscreen_cursor_paint(dfi->primary_data, dfi->primary_pitch, (dfContext *)context);
			dfi->cursor_unpainted = false;
		}

		if (df_unlock_fb(dfi, DF_LOCK_BIT_PAINT))
		{
			if (((dfContext*) context)->direct_flip)
			{
				gdi_DecomposeInvalidArea(gdi->primary->hdc);

				if (!gdi->primary->hdc->hwnd->invalid->null)
				{
					for (; ninvalid>0; --ninvalid, ++cinvalid)
					{
						if (cinvalid->w>0 && cinvalid->h>0)
						{
							DFBRegion fdbr = {cinvalid->x, cinvalid->y,
								cinvalid->x + cinvalid->w - 1,
								cinvalid->y + cinvalid->h - 1};
							dfi->primary->Flip(dfi->primary, &fdbr, DSFLIP_ONSYNC | DSFLIP_PIPELINE);
						}
					}
				}
			}
			gdi->primary_buffer = 0;
			gdi->primary->bitmap->data = 0;
		}
	}
	else
	{
		if (context->instance->settings->fullscreen && dfi->cursor_unpainted)
		{
			df_fullscreen_cursor_save_image_under(gdi->primary_buffer, 0, (dfContext *)context);
			df_fullscreen_cursor_paint(gdi->primary_buffer, 0, (dfContext *)context);
			gdi_InvalidateRegion(gdi->primary->hdc, dfi->cursor_region.x1, dfi->cursor_region.y1, 
				dfi->cursor_region.x2 - dfi->cursor_region.x1, dfi->cursor_region.y2 - dfi->cursor_region.y1);
			dfi->cursor_unpainted = false;
		}

		if (!gdi->primary->hdc->hwnd->invalid->null)
		{
			gdi_DecomposeInvalidArea(gdi->primary->hdc);

			for (; ninvalid>0; --ninvalid, ++cinvalid)
			{
				if (cinvalid->w>0 && cinvalid->h>0)
				{
					dfi->update_rect.x = cinvalid->x;
					dfi->update_rect.y = cinvalid->y;
					dfi->update_rect.w = cinvalid->w;
					dfi->update_rect.h = cinvalid->h;
					dfi->primary->Blit(dfi->primary, dfi->secondary, &(dfi->update_rect), dfi->update_rect.x, dfi->update_rect.y);
					if (((dfContext*) context)->direct_flip)
					{
						DFBRegion fdbr = {cinvalid->x, cinvalid->y,
							cinvalid->x + cinvalid->w - 1,
							cinvalid->y + cinvalid->h - 1};
						dfi->primary->Flip(dfi->primary, &fdbr, DSFLIP_ONSYNC | DSFLIP_PIPELINE);
					}
				}
			}
		}
	}

	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void df_end_paint(rdpContext* context)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	const uint64 now = get_ticks();
	if (((dfContext*) context)->endpaint_defer_ts)
	{
		if ((now - ((dfContext*) context)->endpaint_defer_ts)<BUSY_FRAMEDROP_INTERVAL)
		{
			if (!dfi->cursor_unpainted || !is_cursor_needs_repaint(dfi))
				return;
		}

		((dfContext*) context)->endpaint_defer_ts = 0;
	}
	else if (((dfContext*) context)->busy_ts)
	{
		if ((now - ((dfContext*) context)->busy_ts)>BUSY_THRESHOLD)
		{
			if (!dfi->cursor_unpainted || !is_cursor_needs_repaint(dfi))
			{
				((dfContext*) context)->endpaint_defer_ts = now;
				return;
			}
		}
	}
	else if (!context->gdi->primary->hdc->hwnd->invalid->null)
		((dfContext*) context)->busy_ts = now;

	df_end_paint_inner(context);
}


void df_run_register(freerdp* instance)
{
	instance->update->BeginPaint = df_begin_paint;
	instance->update->EndPaint = df_end_paint;
}

INLINE static boolean IsDrawingPrimary(rdpContext* context)
{
	return (context->gdi->drawing->hdc->selectedObject == context->gdi->primary->hdc->selectedObject);
//	return (context->gdi->drawing->hdc == context->gdi->primary->hdc);
}

static boolean df_check_for_cursor_unpaint(rdpContext* context, int x1, int y1, int x2, int y2)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	int i;

	if (dfi->cursor_unpainted)
		return true;
	
	dfi = ((dfContext*) context)->dfi;

	if (x2 < x1)
	{
		i = x1;
		x1 = x2;
		x2 = i;
	}

	if (y2 < y1)
	{
		i = y1;
		y1 = y2;
		y2 = i;
	}

	if (x1 < dfi->cursor_region.x2 && dfi->cursor_region.x1 < x2 &&
		y1 < dfi->cursor_region.y2 && dfi->cursor_region.y1 < y2)
	{
		df_fullscreen_cursor_unpaint2((dfContext *)context);
		return true;
	}
	return false;
}



void df_flt_dstblt(rdpContext* context, DSTBLT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
//	printf("df_flt_dstblt\n");
	if (IsDrawingPrimary(context))
	{
		df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
			order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
	}
	dfi->lower_primary_update.DstBlt(context, order);
}

void df_flt_patblt(rdpContext* context, PATBLT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
//	printf("df_flt_patblt\n");
	if (IsDrawingPrimary(context))
	{
		df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
			order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
	}
	dfi->lower_primary_update.PatBlt(context, order);
}

void df_flt_scrblt(rdpContext* context, SCRBLT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
//	printf("df_flt_scrblt\n");
	if (!df_check_for_cursor_unpaint(context, order->nXSrc, order->nYSrc, 
			order->nXSrc + order->nWidth, order->nYSrc + order->nHeight))
	{
		if (IsDrawingPrimary(context))
		{
			df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
				order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
		}
	}
	dfi->lower_primary_update.ScrBlt(context, order);
}


void df_flt_memblt(rdpContext* context, MEMBLT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
	if (IsDrawingPrimary(context))
	{
		df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
			order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
	}
	dfi->lower_primary_update.MemBlt(context, order);
}

void df_flt_mem3blt(rdpContext* context, MEM3BLT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
	if (IsDrawingPrimary(context))
	{
		df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
			order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
	}
	dfi->lower_primary_update.Mem3Blt(context, order);
}

void df_flt_opaque_rect(rdpContext* context, OPAQUE_RECT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
//	printf("df_flt_opaque_rect\n");
	if (IsDrawingPrimary(context))
	{
		df_check_for_cursor_unpaint(context, order->nLeftRect, order->nTopRect, 
			order->nLeftRect + order->nWidth, order->nTopRect + order->nHeight);
	}
	dfi->lower_primary_update.OpaqueRect(context, order);
}

void df_flt_multi_opaque_rect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	int i;
	df_check_for_background_while_locked((dfContext *)context);

	if (IsDrawingPrimary(context))
	{
		DELTA_RECT* rectangle;
		for (i = 1; i < (int) order->numRectangles + 1; i++)//WTF??? Why 1?
		{
			rectangle = &order->rectangles[i];
			if (df_check_for_cursor_unpaint(context, rectangle->left, rectangle->top, 
				rectangle->left + rectangle->width, rectangle->top + rectangle->height)) break;
		}
	}
	dfi->lower_primary_update.MultiOpaqueRect(context, order);
}

void df_flt_line_to(rdpContext* context, LINE_TO_ORDER* order)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
	if (IsDrawingPrimary(context))
		df_check_for_cursor_unpaint(context, order->nXStart, order->nYStart, order->nXEnd, order->nYEnd);
	dfi->lower_primary_update.LineTo(context, order);
}

void df_flt_polyline(rdpContext* context, POLYLINE_ORDER* order)
{
	int i;
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
	if (IsDrawingPrimary(context))
	{
		for (i = 1; i < (int) order->numPoints; i++)
		{
			if (df_check_for_cursor_unpaint(context, order->points[i - 1].x, 
				order->points[i - 1].y, order->points[i].x, order->points[i].y)) break;
		}
	}
	dfi->lower_primary_update.Polyline(context, order);
}

void df_flt_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* command)
{
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);
	df_check_for_cursor_unpaint(context, command->destLeft, command->destTop, command->destRight + 1, command->destBottom + 1);
	dfi->lower_surface_bits(context, command);
}

void df_flt_bitmap_update(rdpContext* context, BITMAP_UPDATE* command)
{
	uint32 i;
	BITMAP_DATA* bitmap_data;
	dfInfo* dfi = ((dfContext*) context)->dfi;
	df_check_for_background_while_locked((dfContext *)context);

	for (i = 0; i < command->number; i++)
	{
		bitmap_data = &command->rectangles[i];
		if (df_check_for_cursor_unpaint(context, bitmap_data->destLeft, bitmap_data->destTop, 
			bitmap_data->destLeft + bitmap_data->width, bitmap_data->destTop + bitmap_data->height)) break;
	}

	dfi->lower_bitmap_update(context, command);
}

#define ASSIGN_IF_NOT_NULL(dst, value) if (dst) {(dst) = (value); }

static void df_free(dfInfo* dfi)
{
	if (dfi->read_fds!=-1)
		close(dfi->read_fds);
	
	if (dfi->event_buffer)
		dfi->event_buffer->Release(dfi->event_buffer);

	if (dfi->layer)
		dfi->layer->Release(dfi->layer);

	if (dfi->secondary)
		dfi->secondary->Release(dfi->secondary);

	if (dfi->primary)
		dfi->primary->Release(dfi->primary);

	if (dfi->contents_of_cursor)
		xfree(dfi->contents_of_cursor);

	if (dfi->contents_under_cursor)
		xfree(dfi->contents_under_cursor);

	if (dfi->dfb)
		dfi->dfb->Release(dfi->dfb);

	xfree(dfi);
}





void df_run(freerdp* instance)
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
	struct timeval tv;
	dfInfo* dfi;
	dfContext* context;
	rdpChannels* channels;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (!freerdp_connect(instance))
		return;

	context = (dfContext*) instance->context;

	dfi = context->dfi;
	channels = instance->context->channels;

	if (((rdpContext *)context)->instance->settings->fullscreen)
	{
		df_vt_register();
		dfi->lower_surface_bits = instance->update->SurfaceBits;
		dfi->lower_bitmap_update = instance->update->BitmapUpdate;

		ASSIGN_IF_NOT_NULL(instance->update->SurfaceBits, df_flt_surface_bits);
		ASSIGN_IF_NOT_NULL(instance->update->BitmapUpdate, df_flt_bitmap_update);


		memcpy(&dfi->lower_primary_update, instance->update->primary, sizeof(dfi->lower_primary_update));

		ASSIGN_IF_NOT_NULL(instance->update->primary->DstBlt, df_flt_dstblt);
		ASSIGN_IF_NOT_NULL(instance->update->primary->PatBlt, df_flt_patblt);
		ASSIGN_IF_NOT_NULL(instance->update->primary->ScrBlt, df_flt_scrblt);
		ASSIGN_IF_NOT_NULL(instance->update->primary->OpaqueRect, df_flt_opaque_rect);
		ASSIGN_IF_NOT_NULL(instance->update->primary->MultiOpaqueRect, df_flt_multi_opaque_rect);
		ASSIGN_IF_NOT_NULL(instance->update->primary->LineTo, df_flt_line_to);
		ASSIGN_IF_NOT_NULL(instance->update->primary->Polyline, df_flt_polyline);
		ASSIGN_IF_NOT_NULL(instance->update->primary->MemBlt, df_flt_memblt);
		ASSIGN_IF_NOT_NULL(instance->update->primary->Mem3Blt, df_flt_mem3blt);
//		printf("multyscrbl=%p\n", instance->update->primary->MultiScrBlt);
	}

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

		if (context->input_defer_ts)
		{
			if (!context->busy_ts)
			{
				df_events_commit(instance);
				context->input_defer_ts = 0;
			}
			else if (get_ticks()-context->input_defer_ts>=BUSY_INPUT_DEFER_INTERVAL)
			{
				df_events_commit(instance);
				context->input_defer_ts = get_ticks();
			}
		}
		else if (context->busy_ts && context->endpaint_defer_ts)
		{
			context->input_defer_ts = context->busy_ts;
		}

		if (df_get_fds(instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get dfreerdp file descriptor\n");
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

		if (context->busy_ts)
		{
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			i = select(max_fds + 1, &rfds_set, &wfds_set, NULL, &tv);
			if ( i == 0 )
			{
				context->busy_ts = 0;
				if (context->endpaint_defer_ts)
				{
					context->endpaint_defer_ts = 0;
					df_end_paint_inner((rdpContext*)context);
				}
			}

			if (context->dfi->tty_background && !df_vt_is_disactivated_slow())
			{
				dfi->tty_background = false;
				df_foreground(context);
			}
		}
		else
		{
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			i = select(max_fds + 1, &rfds_set, &wfds_set, NULL, &tv);
			if (dfi->tty_background && !df_vt_is_disactivated_slow())
			{
				dfi->tty_background = false;
				df_foreground(context);
			} else if (i==0)
				df_check_for_background(context);
		}


		if (i == -1)
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

		if (df_check_fds(instance, &rfds_set) != true)
		{
			printf("Failed to check dfreerdp file descriptor\n");
			break;
		}

		if (freerdp_check_fds(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}

		if (!context->input_defer_ts)
			df_events_commit(instance);

		if (freerdp_channels_check_fds(channels, instance) != true)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}

		df_process_channel_event(channels, instance);

		if (instance->settings->fullscreen && !dfi->cursor_unpainted &&
			is_cursor_needs_repaint(dfi) && !context->dfi->tty_background && !context->endpaint_defer_ts)
		{
			int pitch;
			uint8* surface = NULL;
			DFBResult result = dfi->primary->Lock(dfi->primary,
					DSLF_READ | DSLF_WRITE, (void**)&surface, &pitch);
			if (result==DFB_OK)
			{
				if (!context->direct_surface)
				{
					df_fullscreen_cursor_unpaint(context->_p.gdi->primary_buffer, 0, context, false);
					df_fullscreen_cursor_unpaint(surface, pitch, context, true);
					df_fullscreen_cursor_save_image_under(context->_p.gdi->primary_buffer, 0, context);
					df_fullscreen_cursor_paint(context->_p.gdi->primary_buffer, 0, context);
				}
				else
				{
					df_fullscreen_cursor_unpaint(surface, pitch, context, true);
					df_fullscreen_cursor_save_image_under(surface, pitch, context);
				}
				df_fullscreen_cursor_paint(surface, pitch, context);
				df_update_cursor_region(dfi, context->_p.gdi);
				dfi->primary->Unlock(dfi->primary);
			}
		}/*else if (dfi->cursor_x != dfi->pointer_x || dfi->cursor_y != dfi->pointer_y) 
		{
			printf("skipping cursor repaint, unpained=%x defer=%x\n",dfi->cursor_unpainted, (uint32)context->endpaint_defer_ts);
		}*/
	}

	if (((rdpContext *)context)->instance->settings->fullscreen)
		df_vt_deregister();

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	gdi_free(instance);
	freerdp_disconnect(instance);
	if (context->_p.cache)
	{
		cache_free(context->_p.cache);
		context->_p.cache = 0;
	}
	freerdp_free(instance);
	df_unlock_fb(dfi, 0xff);
	df_free(dfi);
}

///////////////////////////////////////////

