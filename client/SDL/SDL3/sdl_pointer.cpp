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

#include "sdl_pointer.hpp"
#include "sdl_context.hpp"
#include "sdl_touch.hpp"
#include "sdl_utils.hpp"

#include <SDL3/SDL_mouse.h>

typedef struct
{
	rdpPointer pointer;
	SDL_Cursor* cursor;
	SDL_Surface* image;
	size_t size;
	void* data;
} sdlPointer;

[[nodiscard]] static BOOL sdl_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);

	WINPR_ASSERT(context);
	if (!ptr)
		return FALSE;

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	ptr->size = 4ull * pointer->width * pointer->height;
	ptr->data = winpr_aligned_malloc(ptr->size, 16);

	if (!ptr->data)
		return FALSE;

	auto data = static_cast<BYTE*>(ptr->data);
	if (!freerdp_image_copy_from_pointer_data(
	        data, gdi->dstFormat, 0, 0, 0, pointer->width, pointer->height, pointer->xorMaskData,
	        pointer->lengthXorMask, pointer->andMaskData, pointer->lengthAndMask, pointer->xorBpp,
	        &context->gdi->palette))
	{
		winpr_aligned_free(ptr->data);
		ptr->data = nullptr;
		return FALSE;
	}

	return TRUE;
}

static void sdl_Pointer_Clear(sdlPointer* ptr)
{
	WINPR_ASSERT(ptr);
	SDL_DestroyCursor(ptr->cursor);
	SDL_DestroySurface(ptr->image);
	ptr->cursor = nullptr;
	ptr->image = nullptr;
}

static void sdl_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);
	WINPR_UNUSED(context);

	if (ptr)
	{
		sdl_Pointer_Clear(ptr);
		winpr_aligned_free(ptr->data);
		ptr->data = nullptr;
	}
}

[[nodiscard]] static BOOL sdl_Pointer_SetDefault(rdpContext* context)
{
	WINPR_UNUSED(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_DEFAULT);
}

[[nodiscard]] static BOOL sdl_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	WINPR_UNUSED(context);
	return sdl_push_user_event(SDL_EVENT_USER_POINTER_SET, pointer);
}

bool sdl_Pointer_Set_Process(SdlContext* sdl)
{
	WINPR_ASSERT(sdl);

	auto context = sdl->context();
	auto pointer = sdl->cursor();
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);
	if (!ptr)
		return true;

	rdpGdi* gdi = context->gdi;
	WINPR_ASSERT(gdi);

	auto ix = static_cast<float>(pointer->xPos);
	auto iy = static_cast<float>(pointer->yPos);
	auto isw = static_cast<float>(pointer->width);
	auto ish = static_cast<float>(pointer->height);

	auto window = SDL_GetMouseFocus();
	if (!window)
		return sdl_Pointer_SetDefault(context);

	const Uint32 id = SDL_GetWindowID(window);

	const SDL_FRect orig{ ix, iy, isw, ish };
	auto pos = sdl->pixelToScreen(id, orig);
	WLog_Print(sdl->getWLog(), WLOG_DEBUG, "cursor scale: pixel:%s, display:%s",
	           sdl::utils::toString(orig).c_str(), sdl::utils::toString(pos).c_str());

	sdl_Pointer_Clear(ptr);

	ptr->image =
	    SDL_CreateSurface(static_cast<int>(orig.w), static_cast<int>(orig.h), sdl->pixelFormat());
	if (!ptr->image)
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_CreateSurface failed");
		return false;
	}

	if (!SDL_LockSurface(ptr->image))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_LockSurface failed");
		return false;
	}

	auto pixels = static_cast<BYTE*>(ptr->image->pixels);
	auto data = static_cast<const BYTE*>(ptr->data);
	const BOOL rc = freerdp_image_scale(
	    pixels, gdi->dstFormat, static_cast<UINT32>(ptr->image->pitch), 0, 0,
	    static_cast<UINT32>(ptr->image->w), static_cast<UINT32>(ptr->image->h), data,
	    gdi->dstFormat, 0, 0, 0, static_cast<UINT32>(isw), static_cast<UINT32>(ish));
	SDL_UnlockSurface(ptr->image);
	if (!rc)
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "freerdp_image_scale failed");
		return false;
	}

	// create a cursor image in 100% display scale to trick SDL into creating the cursor with the
	// correct size
	auto fw = sdl->getFirstWindow();
	if (!fw)
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "sdl->getFirstWindow() NULL");
		return false;
	}

	const auto hidpi_scale =
	    sdl->pixelToScreen(fw->id(), SDL_FPoint{ static_cast<float>(ptr->image->w),
	                                             static_cast<float>(ptr->image->h) });
	std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> normal{
		SDL_CreateSurface(static_cast<int>(hidpi_scale.x), static_cast<int>(hidpi_scale.y),
		                  ptr->image->format),
		SDL_DestroySurface
	};
	assert(normal);
	if (!SDL_BlitSurfaceScaled(ptr->image, nullptr, normal.get(), nullptr,
	                           SDL_ScaleMode::SDL_SCALEMODE_LINEAR))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_BlitSurfaceScaled failed");
		return false;
	}
	if (!SDL_AddSurfaceAlternateImage(normal.get(), ptr->image))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_AddSurfaceAlternateImage failed");
		return false;
	}

	ptr->cursor =
	    SDL_CreateColorCursor(normal.get(), static_cast<int>(pos.x), static_cast<int>(pos.y));
	if (!ptr->cursor)
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_CreateColorCursor(display:%s, pixel:%s} failed",
		           sdl::utils::toString(pos).c_str(), sdl::utils::toString(orig).c_str());
		return false;
	}

	if (!SDL_SetCursor(ptr->cursor))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_SetCursor failed");
		return false;
	}
	if (!SDL_ShowCursor())
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_ShowCursor failed");
		return false;
	}
	sdl->setHasCursor(true);
	return true;
}

[[nodiscard]] static BOOL sdl_Pointer_SetNull(rdpContext* context)
{
	WINPR_UNUSED(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_NULL);
}

[[nodiscard]] static BOOL sdl_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);

	return sdl_push_user_event(SDL_EVENT_USER_POINTER_POSITION, x, y);
}

bool sdl_register_pointer(rdpGraphics* graphics)
{
	const rdpPointer pointer = { sizeof(sdlPointer),
		                         sdl_Pointer_New,
		                         sdl_Pointer_Free,
		                         sdl_Pointer_Set,
		                         sdl_Pointer_SetNull,
		                         sdl_Pointer_SetDefault,
		                         sdl_Pointer_SetPosition,
		                         {},
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         0,
		                         nullptr,
		                         nullptr,
		                         {} };
	graphics_register_pointer(graphics, &pointer);
	return true;
}
