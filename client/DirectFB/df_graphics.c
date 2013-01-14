/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <winpr/crt.h>

#include "df_graphics.h"

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
		BYTE* point = NULL;

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

	dfi->layer->SetCooperativeLevel(dfi->layer, DLSCL_ADMINISTRATIVE);

	dfi->layer->SetCursorOpacity(dfi->layer, df_pointer ? 255: 0);

	if(df_pointer != NULL)
	{
		result = dfi->layer->SetCursorShape(dfi->layer,
			df_pointer->surface, df_pointer->xhot, df_pointer->yhot);

		if (result != DFB_OK)
		{
			DirectFBErrorFatal("SetCursorShape Error", result);
			return;
		}
	}

	dfi->layer->SetCooperativeLevel(dfi->layer, DLSCL_SHARED);
}

void df_Pointer_SetNull(rdpContext* context)
{
	df_Pointer_Set(context, NULL);
}

void df_Pointer_SetDefault(rdpContext* context)
{

}

/* Graphics Module */

void df_register_graphics(rdpGraphics* graphics)
{
	rdpPointer* pointer;

	pointer = (rdpPointer*) malloc(sizeof(rdpPointer));
	ZeroMemory(pointer, sizeof(rdpPointer));
	pointer->size = sizeof(dfPointer);

	pointer->New = df_Pointer_New;
	pointer->Free = df_Pointer_Free;
	pointer->Set = df_Pointer_Set;
	pointer->SetNull = df_Pointer_SetNull;
	pointer->SetDefault = df_Pointer_SetDefault;

	graphics_register_pointer(graphics, pointer);
	free(pointer);
}

