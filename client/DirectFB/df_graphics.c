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

#include "df_graphics.h"



void df_create_temp_surface(dfInfo *dfi, int width, int height, int bpp, IDirectFBSurface **ppsurf)
{
	dfi->dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
	dfi->dsc.caps = 0;//DSCAPS_SYSTEMONLY;
	dfi->dsc.width = width;
	dfi->dsc.height = height;

	if (bpp == 32 || bpp == 24)
		dfi->dsc.pixelformat = DSPF_AiRGB;
	else if (bpp == 16 || bpp == 15)
		dfi->dsc.pixelformat = DSPF_RGB16;
	else if (bpp == 8)
		dfi->dsc.pixelformat = DSPF_RGB332;
	else
		dfi->dsc.pixelformat = DSPF_AiRGB;

	dfi->dfb->CreateSurface(dfi->dfb, &(dfi->dsc), ppsurf);
}

void df_fullscreen_cursor_bounds(rdpGdi* gdi, dfInfo* dfi, int *cursor_left, int *cursor_top, int *cursor_right, int *cursor_bottom)
{
	*cursor_left = dfi->cursor_x - dfi->cursor_hot_x;
	*cursor_top = dfi->cursor_y - dfi->cursor_hot_y;
	*cursor_right = *cursor_left + dfi->cursor_w;
	*cursor_bottom = *cursor_top + dfi->cursor_h;

	if (*cursor_left < 0)
		*cursor_left = 0;

	if (*cursor_top < 0)
		*cursor_top = 0;

	if (*cursor_right > gdi->width)
		*cursor_right = gdi->width;

	if (*cursor_bottom > gdi->height)
		*cursor_bottom = gdi->height;
}

#define DF_FULLSCREEN_CURSOR_RECT(gdi_, dfi_, left_, top_, width_, height_) \
	df_fullscreen_cursor_bounds(gdi_, dfi_, &(left_), &(top_), &(width_), &(height_)); \
	width_ -= left_; \
	height_ -= top_; 

#define BPP2PIXELSIZE(bpp_, pixel_length_) \
	if (bpp_ == 16 || bpp_ == 15) pixel_length_ = 2; \
	else if (bpp_ != 8) pixel_length_ = 4; \
	else pixel_length_ = 1;


void df_fullscreen_cursor_unpaint(uint8 *surface, int pitch, dfContext *context, boolean update_pos)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int src_left = 0, src_top = 0;
	int left, top, length, count, src_pitch, pixel_length;
	uint8 *dst, *src;

	if (dfi->contents_under_cursor)
	{
		DF_FULLSCREEN_CURSOR_RECT(gdi, dfi, left, top, length, count);

		if (count > 0 && length > 0)
		{
			BPP2PIXELSIZE(gdi->dstBpp, pixel_length);
			if (!pitch) pitch = gdi->width * pixel_length;

			src_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
			src_top = top - (dfi->cursor_y - dfi->cursor_hot_y);
			src = dfi->contents_under_cursor + (src_top * dfi->cursor_w + src_left) * pixel_length;
			dst = surface + (top * pitch) + left * pixel_length;
			src_pitch = dfi->cursor_w * pixel_length;
			length *= pixel_length;

			for (--count; count; --count, dst += pitch, src += src_pitch)
			{
				PREFETCH_WRITE(dst + pitch);
				memcpy(dst, src, length);
			}
			memcpy(dst, src, length);
		}
	}
	if (update_pos)
	{
		dfi->cursor_hot_x = dfi->cursor_new_hot_x;
		dfi->cursor_hot_y = dfi->cursor_new_hot_y;
		dfi->cursor_x = dfi->pointer_x;
		dfi->cursor_y = dfi->pointer_y;
	}
}


void df_fullscreen_cursor_save_image_under(uint8 *surface, int pitch, dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int dst_left = 0, dst_top = 0;
	int length, count, left, top, dst_pitch, pixel_length;
	uint8 *src, *dst;

	if (dfi->contents_under_cursor && (dfi->cursor_w != dfi->cursor_new_w || dfi->cursor_h != dfi->cursor_new_h))
	{
		xfree(dfi->contents_under_cursor);
		dfi->contents_under_cursor = 0;
	}

	dfi->cursor_w = dfi->cursor_new_w;
	dfi->cursor_h = dfi->cursor_new_h;

	if (!dfi->cursor_w || !dfi->cursor_h)
		return;

	DF_FULLSCREEN_CURSOR_RECT(gdi, dfi, left, top, length, count);

	if (count > 0 && length > 0)
	{
		if (!dfi->contents_under_cursor)
		{
			dfi->contents_under_cursor = (uint8*)xmalloc(dfi->cursor_w * dfi->cursor_h * sizeof(uint32));
			if (!dfi->contents_under_cursor)
				return;
		}	

		BPP2PIXELSIZE(gdi->dstBpp, pixel_length);
		if (!pitch) pitch = gdi->width * pixel_length;

		dst_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
		dst_top = top - (dfi->cursor_y - dfi->cursor_hot_y);
		src = surface + top * pitch + left * pixel_length;
		dst = dfi->contents_under_cursor + (dst_top * dfi->cursor_w + dst_left) * pixel_length;
		dst_pitch = dfi->cursor_w * pixel_length;
		length *= pixel_length;

		for (--count; count; --count, src += pitch, dst += dst_pitch)
		{
			PREFETCH_WRITE(dst + dst_pitch);
			memcpy(dst, src, length);
		}
		memcpy(dst, src, length);
	}
}

INLINE static void cursor_linecpy(uint8 *dst, uint8 *src, uint8 pixel_length, int pixels_count, int bpp, HCLRCONV clrconv)
{
	uint32 pixel;
	for (; pixels_count; --pixels_count, dst += pixel_length, src += sizeof(uint32))
	{
		pixel = *(uint32 *)src;
		if ( ( pixel & 0xff000000) > 0x10000000)
		{
			pixel = freerdp_color_convert_rgb( pixel | 0xff000000, 32, bpp, clrconv);
			memcpy(dst, &pixel, pixel_length);
		}
	}
}

void df_fullscreen_cursor_paint(uint8 *surface, int pitch, dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int src_left = 0, src_top = 0;
	int line_pixels, line_count, left, top, src_pitch;
	uint8 pixel_length, *src, *dst;

	dfi->cursor_id = dfi->cursor_new_id;

	if (!dfi->contents_of_cursor)
		return;

	DF_FULLSCREEN_CURSOR_RECT(gdi, dfi, left, top, line_pixels, line_count);

	if (line_count > 0 && line_pixels > 0)
	{
		BPP2PIXELSIZE(gdi->dstBpp, pixel_length);
		if (!pitch) pitch = gdi->width * pixel_length;

		src_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
		src_top = top - (dfi->cursor_y - dfi->cursor_hot_y);
		src_pitch = dfi->cursor_w * sizeof(uint32);
		src = dfi->contents_of_cursor + src_pitch * src_top + src_left * sizeof(uint32);
		dst = surface + (top * pitch) + left * pixel_length;

		for (--line_count; line_count; --line_count, dst += pitch, src += src_pitch)
		{
			PREFETCH_WRITE(dst + pitch);
			cursor_linecpy(dst, src, pixel_length, line_pixels, gdi->dstBpp, dfi->clrconv);
		}
		cursor_linecpy(dst, src, pixel_length, line_pixels, gdi->dstBpp, dfi->clrconv);
	}
}


/* Pointer Class */

void df_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	dfInfo* dfi;
	DFBResult result;
	dfPointer* df_pointer;
	DFBSurfaceDescription dsc;

	dfi = ((dfContext*) context)->dfi;
	df_pointer = (dfPointer*) pointer;

	dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
	dsc.caps = DSCAPS_SYSTEMONLY;
	dsc.width = pointer->width;
	dsc.height = pointer->height;
	dsc.pixelformat = DSPF_ARGB;

	result = dfi->dfb->CreateSurface(dfi->dfb, &dsc, &(df_pointer->surface));

	if (result == DFB_OK)
	{
		int pitch;
		uint8* point = NULL;

		df_pointer->xhot = pointer->xPos;
		df_pointer->yhot = pointer->yPos;

		result = df_pointer->surface->Lock(df_pointer->surface,
				DSLF_WRITE, (void**) &point, &pitch);

		if (result != DFB_OK)
		{
			DirectFBErrorFatal("Error while creating pointer surface", result);
			return;
		}

		if ((pointer->andMaskData != 0) && (pointer->xorMaskData != 0))
		{
			freerdp_alpha_cursor_convert(point, pointer->xorMaskData, pointer->andMaskData,
					pointer->width, pointer->height, pointer->xorBpp, dfi->clrconv);
		}

		if (pointer->xorBpp > 24)
		{
			freerdp_image_swap_color_order(point, pointer->width, pointer->height);
		}

		df_pointer->surface->Unlock(df_pointer->surface);
	}
}

void df_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	dfInfo* dfi;

	dfi = ((dfContext*) context)->dfi;
	dfPointer* df_pointer = (dfPointer*) pointer;
	df_pointer->surface->Release(df_pointer->surface);
}

void df_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	dfInfo* dfi;
	DFBResult result;
	dfPointer* df_pointer;
	int i, pitch;
	uint8* src, *dst;

	dfi = ((dfContext*) context)->dfi;
	df_pointer = (dfPointer*) pointer;

	if (context->instance->settings->fullscreen)
	{
		DFBSurfaceDescription dsc;
		dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
		dsc.caps = 0;
		dsc.width = pointer->width;
		dsc.height = pointer->height;
		dsc.pixelformat = DSPF_ARGB;
		if (dfi->contents_of_cursor)
			xfree(dfi->contents_of_cursor);

		dfi->contents_of_cursor = (uint8 *)xmalloc(pointer->width * pointer->height * sizeof(uint32));
		if (dfi->contents_of_cursor)
		{
//			dfi->contents_of_cursor->Blit(dfi->contents_of_cursor, df_pointer->surface, 0, 0, 0);
			dfi->cursor_new_w = pointer->width;
			dfi->cursor_new_h = pointer->height;
			dfi->cursor_new_hot_x = df_pointer->xhot;
			dfi->cursor_new_hot_y = df_pointer->yhot;
			dfi->cursor_new_id++;

			result = df_pointer->surface->Lock(df_pointer->surface,
					DSLF_READ, (void**) &src, &pitch);
			if (result == DFB_OK)
			{
				dst = dfi->contents_of_cursor;
				for (i = 0; i<pointer->height; ++i, dst += pointer->width * sizeof(uint32), src += pitch )
					memcpy(dst, src, pointer->width * sizeof(uint32));
				df_pointer->surface->Unlock(df_pointer->surface);
			}
		}
		else
			result = -1;
	}
	else
	{
		dfi->layer->SetCooperativeLevel(dfi->layer, DLSCL_ADMINISTRATIVE);

		result = dfi->layer->SetCursorShape(dfi->layer,
				df_pointer->surface, df_pointer->xhot, df_pointer->yhot);

		dfi->layer->SetCooperativeLevel(dfi->layer, DLSCL_SHARED);
	}
	if (result != DFB_OK)
	{
		DirectFBErrorFatal("SetCursorShape Error", result);
	}
}

/* Graphics Module */

void df_register_graphics(rdpGraphics* graphics)
{
	rdpPointer* pointer;

	pointer = xnew(rdpPointer);
	pointer->size = sizeof(dfPointer);

	pointer->New = df_Pointer_New;
	pointer->Free = df_Pointer_Free;
	pointer->Set = df_Pointer_Set;

	graphics_register_pointer(graphics, pointer);
	xfree(pointer);
}

//////////////////////////////////////////////


