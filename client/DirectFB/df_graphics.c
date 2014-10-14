/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
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



void df_fullscreen_cursor_unpaint(dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int src_left = 0, src_top = 0;
	int left, top, length, count, src_pitch;
	uint8 *dst, *src;
	DFBRectangle rect;

	if (dfi->contents_under_cursor)
	{
		if (dfi->primary_data)
		{//Blit doesn't work on locked surface so just restore image by direct memory copy
			df_fullscreen_cursor_bounds(gdi, dfi, &left, &top, &length, &count);
			length-= left;
			count-= top;

			if (count > 0 && length > 0 &&
				dfi->contents_under_cursor->Lock(dfi->contents_under_cursor, DSLF_READ, (void **)&src, &src_pitch)==DFB_OK)
			{
				src_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
				src_top = top - (dfi->cursor_y - dfi->cursor_hot_y);

				src += src_pitch * src_top;
				dst = dfi->primary_data + (top * dfi->primary_pitch);
				if (gdi->dstBpp == 16 || gdi->dstBpp == 15)
				{
					src += src_left * 2;
					dst += left * 2;
					length *= 2;
				}
				else if (gdi->dstBpp != 8)
				{
					src += src_left * 4;
					dst += left * 4;
					length *= 4;
				}
				else
				{
					src += src_left;
					dst += left;
				}

				for (; count; --count, dst += dfi->primary_pitch, src += src_pitch)
				{
					PREFETCH_WRITE(dst + dfi->primary_pitch);
					memcpy(dst, src, length);
				}

				dfi->contents_under_cursor->Unlock(dfi->contents_under_cursor);
			}
		}
		else
		{
			rect.x = rect.y = 0;
			rect.w = dfi->cursor_w;
			rect.h = dfi->cursor_h;

			dfi->primary->Blit(dfi->primary, dfi->contents_under_cursor, &rect, 
				dfi->cursor_x - dfi->cursor_hot_x, dfi->cursor_y - dfi->cursor_hot_y);			
		}
	}

	dfi->cursor_hot_x = dfi->cursor_new_hot_x;
	dfi->cursor_hot_y = dfi->cursor_new_hot_y;
	dfi->cursor_x = dfi->pointer_x;
	dfi->cursor_y = dfi->pointer_y;
}


static void df_save_locked_image_under_cursor(dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int dst_left = 0, dst_top = 0;
	int length, count, left, top, dst_pitch;
	uint8 *src, *dst;

	df_fullscreen_cursor_bounds(gdi, dfi, &left, &top, &length, &count);
	length-= left;
	count-= top;
	
	if (count > 0 && length > 0 &&
		dfi->contents_under_cursor->Lock(dfi->contents_under_cursor, DSLF_WRITE, (void **)&dst, &dst_pitch)==DFB_OK)
	{
		dst_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
		dst_top = top - (dfi->cursor_y - dfi->cursor_hot_y);
		dst += dst_pitch * dst_top;
		src = dfi->primary_data + (top * dfi->primary_pitch);

		if (gdi->dstBpp == 16 || gdi->dstBpp == 15)
		{
			dst += dst_left * 2;
			src += left * 2;
			length *= 2;
		}
		else if (gdi->dstBpp != 8)
		{
			dst += dst_left * 4;
			src += left * 4;
			length *= 4;
		}
		else
		{
			dst += dst_left;
			src += left;
		}

		for (; count; --count, src += dfi->primary_pitch, dst += dst_pitch)
		{
			PREFETCH_WRITE(dst + dst_pitch);
			memcpy(dst, src, length);
		}

		dfi->contents_under_cursor->Unlock(dfi->contents_under_cursor);
	}
}

void df_fullscreen_cursor_paint(dfContext *context)
{
	rdpGdi* gdi = context->_p.gdi;
	dfInfo* dfi = context->dfi;
	int src_left = 0, src_top = 0;
	int length, count, left, top, src_pitch, i, j;
	uint32 pixel;
	uint8 pixel_size, *src, *dst;
	DFBRectangle rect;

	if (dfi->contents_under_cursor && (dfi->cursor_w != dfi->cursor_new_w || dfi->cursor_h != dfi->cursor_new_h))
	{
		dfi->contents_under_cursor->Release(dfi->contents_under_cursor);
		dfi->contents_under_cursor = 0;
	}

	if (!dfi->contents_under_cursor)
	{
		dfi->cursor_w = dfi->cursor_new_w;
		dfi->cursor_h = dfi->cursor_new_h;
		if (!dfi->cursor_w || !dfi->cursor_h)
			return;

		df_create_temp_surface(dfi, dfi->cursor_w, dfi->cursor_h, gdi->dstBpp, &(dfi->contents_under_cursor));
	}

	if (dfi->primary_data)
	{
		df_save_locked_image_under_cursor(context);
	}
	else
	{
		rect.x = dfi->cursor_x - dfi->cursor_hot_x;
		rect.y = dfi->cursor_y - dfi->cursor_hot_y;
		rect.w = dfi->cursor_w;
		rect.h = dfi->cursor_h;
		dfi->contents_under_cursor->Blit(dfi->contents_under_cursor, dfi->primary, &rect, 0, 0);
	}

	if (!dfi->contents_of_cursor)
		return;

	if (dfi->primary_data)
	{
		df_fullscreen_cursor_bounds(gdi, dfi, &left, &top, &length, &count);
		length-= left;
		count-= top;

		if (count > 0 && length > 0 &&
			dfi->contents_of_cursor->Lock(dfi->contents_of_cursor, DSLF_READ, (void **)&src, &src_pitch)==DFB_OK)
		{
			src_left = left - (dfi->cursor_x - dfi->cursor_hot_x);
			src_top = top - (dfi->cursor_y - dfi->cursor_hot_y);

			src += src_pitch * src_top;
			dst = dfi->primary_data + (top * dfi->primary_pitch);

			src += src_left * 4;

			if (gdi->dstBpp == 16 || gdi->dstBpp == 15)
				pixel_size = 2;
			else if (gdi->dstBpp != 8)
				pixel_size = 4;
			else
				pixel_size = 1;

			length *= pixel_size;
			dst += left * pixel_size;

			for (; count; --count, dst += dfi->primary_pitch, src += src_pitch)
			{
				PREFETCH_WRITE(dst + dfi->primary_pitch);
				for (i = 0, j = 0; i < length; i += pixel_size, j += 4)
				{
					pixel = *(uint32 *)(&src[j]);
					if ( ( pixel & 0xff000000) > 0x10000000)
					{
						pixel = freerdp_color_convert_rgb( pixel | 0xff000000, 32, gdi->dstBpp, dfi->clrconv);
						memcpy(&dst[i], &pixel, pixel_size);
					}
				}
			}
			dfi->contents_of_cursor->Unlock(dfi->contents_of_cursor);
		}
	}
	else
	{
		rect.x = 0;
		rect.y = 0;
		rect.w = dfi->cursor_w;
		rect.h = dfi->cursor_h;
		dfi->primary->SetBlittingFlags(dfi->primary, DSBLIT_BLEND_ALPHACHANNEL);
		dfi->primary->Blit(dfi->primary, dfi->contents_of_cursor, &rect, dfi->cursor_x - dfi->cursor_hot_x, dfi->cursor_y - dfi->cursor_hot_y);
		dfi->primary->SetBlittingFlags(dfi->primary, DSBLIT_NOFX);
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
			dfi->contents_of_cursor->Release(dfi->contents_of_cursor);
		result = dfi->dfb->CreateSurface(dfi->dfb, &dsc, &(dfi->contents_of_cursor));
		if (result==DFB_OK)
		{
			dfi->contents_of_cursor->Blit(dfi->contents_of_cursor, df_pointer->surface, 0, 0, 0);
			dfi->cursor_new_w = pointer->width;
			dfi->cursor_new_h = pointer->height;
			dfi->cursor_new_hot_x = df_pointer->xhot;
			dfi->cursor_new_hot_y = df_pointer->yhot;
		}
		else
			dfi->contents_of_cursor = 0;
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
		return;
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


