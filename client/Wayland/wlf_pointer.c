/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Mouse Pointer
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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

#include "wlf_pointer.h"
#include "wlfreerdp.h"

#define TAG CLIENT_TAG("wayland.pointer")

struct wlf_pointer
{
	rdpPointer pointer;
	size_t size;
	void* data;
};
typedef struct wlf_pointer wlfPointer;

static BOOL wlf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	wlfPointer* ptr = (wlfPointer*)pointer;

	if (!ptr)
		return FALSE;

	ptr->size = pointer->width * pointer->height * 4;
	ptr->data = _aligned_malloc(ptr->size, 16);

	if (!ptr->data)
		return FALSE;

	if (!freerdp_image_copy_from_pointer_data(
	        ptr->data, PIXEL_FORMAT_BGRA32, 0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask, pointer->andMaskData,
	        pointer->lengthAndMask, pointer->xorBpp, &context->gdi->palette))
	{
		_aligned_free(ptr->data);
		return FALSE;
	}

	return TRUE;
}

static void wlf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	wlfPointer* ptr = (wlfPointer*)pointer;
	WINPR_UNUSED(context);

	if (ptr)
		_aligned_free(ptr->data);
}

static BOOL wlf_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
	wlfContext* wlf = (wlfContext*)context;
	wlfPointer* ptr = (wlfPointer*)pointer;
	void* data;
	UINT32 w, h, x, y;
	size_t size;
	UwacReturnCode rc;
	BOOL res = FALSE;
	RECTANGLE_16 area;

	if (!wlf || !wlf->seat)
		return FALSE;

	x = pointer->xPos;
	y = pointer->yPos;
	w = pointer->width;
	h = pointer->height;

	if (!wlf_scale_coordinates(context, &x, &y, FALSE) ||
	    !wlf_scale_coordinates(context, &w, &h, FALSE))
		return FALSE;

	size = w * h * 4;
	data = malloc(size);

	if (!data)
		return FALSE;

	area.top = 0;
	area.left = 0;
	area.right = (UINT16)pointer->width;
	area.bottom = (UINT16)pointer->height;

	if (!wlf_copy_image(ptr->data, pointer->width * 4, pointer->width, pointer->height, data, w * 4,
	                    w, h, &area, context->settings->SmartSizing))
		goto fail;

	rc = UwacSeatSetMouseCursor(wlf->seat, data, size, w, h, x, y);

	if (rc == UWAC_SUCCESS)
		res = TRUE;

fail:
	free(data);
	return res;
}

static BOOL wlf_Pointer_SetNull(rdpContext* context)
{
	wlfContext* wlf = (wlfContext*)context;

	if (!wlf || !wlf->seat)
		return FALSE;

	if (UwacSeatSetMouseCursor(wlf->seat, NULL, 0, 0, 0, 0, 0) != UWAC_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL wlf_Pointer_SetDefault(rdpContext* context)
{
	wlfContext* wlf = (wlfContext*)context;

	if (!wlf || !wlf->seat)
		return FALSE;

	if (UwacSeatSetMouseCursor(wlf->seat, NULL, 1, 0, 0, 0, 0) != UWAC_SUCCESS)
		return FALSE;

	return TRUE;
}

static BOOL wlf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	// TODO
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	return TRUE;
}

BOOL wlf_register_pointer(rdpGraphics* graphics)
{
	rdpPointer* pointer = NULL;

	if (!(pointer = (rdpPointer*)calloc(1, sizeof(rdpPointer))))
		return FALSE;

	pointer->size = sizeof(wlfPointer);
	pointer->New = wlf_Pointer_New;
	pointer->Free = wlf_Pointer_Free;
	pointer->Set = wlf_Pointer_Set;
	pointer->SetNull = wlf_Pointer_SetNull;
	pointer->SetDefault = wlf_Pointer_SetDefault;
	pointer->SetPosition = wlf_Pointer_SetPosition;
	graphics_register_pointer(graphics, pointer);
	free(pointer);
	return TRUE;
}
