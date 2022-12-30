/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Mouse Pointer
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#include <freerdp/config.h>

#include <freerdp/gdi/gdi.h>

#include "sdl_pointer.h"
#include "sdl_freerdp.h"
#include "sdl_touch.h"

#include <SDL_mouse.h>

#define TAG CLIENT_TAG("SDL.pointer")

typedef struct
{
	rdpPointer pointer;
	SDL_Cursor* cursor;
	SDL_Surface* image;
	size_t size;
	void* data;
} sdlPointer;

static BOOL sdl_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	sdlPointer* ptr = (sdlPointer*)pointer;

	WINPR_ASSERT(context);
	if (!ptr)
		return FALSE;

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	ptr->size = pointer->width * pointer->height * 4ULL;
	ptr->data = winpr_aligned_malloc(ptr->size, 16);

	if (!ptr->data)
		return FALSE;

	if (!freerdp_image_copy_from_pointer_data(
	        ptr->data, gdi->dstFormat, 0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask, pointer->andMaskData,
	        pointer->lengthAndMask, pointer->xorBpp, &context->gdi->palette))
	{
		winpr_aligned_free(ptr->data);
		return FALSE;
	}

	return TRUE;
}

static void sdl_Pointer_Clear(sdlPointer* ptr)
{
	WINPR_ASSERT(ptr);
	SDL_FreeCursor(ptr->cursor);
	SDL_FreeSurface(ptr->image);
	ptr->cursor = NULL;
	ptr->image = NULL;
}

static void sdl_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	sdlPointer* ptr = (sdlPointer*)pointer;
	WINPR_UNUSED(context);

	if (ptr)
	{
		sdl_Pointer_Clear(ptr);
		winpr_aligned_free(ptr->data);
		ptr->data = NULL;
	}
}

static BOOL sdl_Pointer_SetDefault(rdpContext* context)
{
	WINPR_UNUSED(context);

	SDL_Cursor* def = SDL_GetDefaultCursor();
	SDL_SetCursor(def);
	SDL_ShowCursor(SDL_ENABLE);
	return TRUE;
}

static BOOL sdl_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	sdlContext* sdl = (sdlContext*)context;
	sdlPointer* ptr = (sdlPointer*)pointer;
	INT32 w, h, x, y, sw, sh;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(ptr);

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	x = (INT32)pointer->xPos;
	y = (INT32)pointer->yPos;
	sw = w = (INT32)pointer->width;
	sh = h = (INT32)pointer->height;

	SDL_Window* window = SDL_GetMouseFocus();
	if (!window)
		return sdl_Pointer_SetDefault(context);

	const Uint32 id = SDL_GetWindowID(window);

	if (!sdl_scale_coordinates(sdl, id, &x, &y, FALSE, FALSE) ||
	    !sdl_scale_coordinates(sdl, id, &sw, &sh, FALSE, FALSE))
		return FALSE;

	sdl_Pointer_Clear(ptr);

	const DWORD bpp = FreeRDPGetBitsPerPixel(gdi->dstFormat);
	ptr->image = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, (int)bpp, sdl->sdl_pixel_format);
	if (!ptr->image)
		return FALSE;

	SDL_LockSurface(ptr->image);
	const BOOL rc =
	    freerdp_image_scale(ptr->image->pixels, gdi->dstFormat, ptr->image->pitch, 0, 0,
	                        ptr->image->w, ptr->image->h, ptr->data, gdi->dstFormat, 0, 0, 0, w, h);
	SDL_UnlockSurface(ptr->image);
	if (!rc)
		return FALSE;

	ptr->cursor = SDL_CreateColorCursor(ptr->image, x, y);
	if (!ptr->cursor)
		return FALSE;

	SDL_SetCursor(ptr->cursor);
	SDL_ShowCursor(SDL_ENABLE);
	return TRUE;
}

static BOOL sdl_Pointer_SetNull(rdpContext* context)
{
	WINPR_UNUSED(context);

	SDL_ShowCursor(SDL_DISABLE);

	return TRUE;
}

static BOOL sdl_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	sdlContext* sdl = (sdlContext*)context;
	WINPR_ASSERT(sdl);
	SDL_Window* window = SDL_GetMouseFocus();
	if (!window)
		return TRUE;

	const Uint32 id = SDL_GetWindowID(window);

	INT32 sx = (INT32)x;
	INT32 sy = (INT32)y;
	if (!sdl_scale_coordinates(sdl, id, &sx, &sy, FALSE, FALSE))
		return FALSE;
	SDL_WarpMouseInWindow(window, sx, sy);

	return TRUE;
}

BOOL sdl_register_pointer(rdpGraphics* graphics)
{
	rdpPointer* pointer = NULL;

	if (!(pointer = (rdpPointer*)calloc(1, sizeof(rdpPointer))))
		return FALSE;

	pointer->size = sizeof(sdlPointer);
	pointer->New = sdl_Pointer_New;
	pointer->Free = sdl_Pointer_Free;
	pointer->Set = sdl_Pointer_Set;
	pointer->SetNull = sdl_Pointer_SetNull;
	pointer->SetDefault = sdl_Pointer_SetDefault;
	pointer->SetPosition = sdl_Pointer_SetPosition;
	graphics_register_pointer(graphics, pointer);
	free(pointer);
	return TRUE;
}
