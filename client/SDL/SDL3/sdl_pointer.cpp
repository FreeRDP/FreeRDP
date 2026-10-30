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

#include <algorithm>
#include <cmath>

#include <freerdp/config.h>

#include <freerdp/gdi/gdi.h>

#include "sdl_pointer.hpp"
#include "sdl_context.hpp"
#include "sdl_touch.hpp"
#include "sdl_utils.hpp"

#include <SDL3/SDL_mouse.h>

struct sdlPointer
{
	rdpPointer pointer{};
	SDL_Cursor* cursor = nullptr;
	SDL_Surface* image = nullptr;
	size_t size = 0;
	BYTE* data = nullptr;

	sdlPointer(const sdlPointer& other) = delete;
	sdlPointer(sdlPointer&& other) = delete;
	auto operator=(const sdlPointer& other) = delete;
	auto operator=(sdlPointer&& other) = delete;
	~sdlPointer() = delete;

	bool update(rdpContext* context)
	{
		assert(context);
		assert(context->gdi);

		size = 4ull * pointer.width * pointer.height;
		winpr_aligned_free(data);
		data = static_cast<BYTE*>(winpr_aligned_calloc(1, size, 16));

		if (!data)
			return false;

		return freerdp_image_copy_from_pointer_data(
		    data, context->gdi->dstFormat, 0, 0, 0, pointer.width, pointer.height,
		    pointer.xorMaskData, pointer.lengthXorMask, pointer.andMaskData, pointer.lengthAndMask,
		    pointer.xorBpp, &context->gdi->palette);
	}
};

[[nodiscard]] static BOOL sdl_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	std::unique_lock lock(sdl->lock());
	if (!ptr)
		return FALSE;
	sdl->pointers().push_back(pointer);

	return ptr->update(context);
}

static void sdl_Pointer_Clear(sdlPointer* ptr)
{
	WINPR_ASSERT(ptr);
	SDL_DestroyCursor(ptr->cursor);
	SDL_DestroySurface(ptr->image);
	ptr->cursor = nullptr;
	ptr->image = nullptr;
}

static void sdl_Pointer_Free(WINPR_ATTR_UNUSED rdpContext* context, rdpPointer* pointer)
{
	auto sdl = get_context(context);

	WINPR_ASSERT(sdl);
	std::unique_lock lock(sdl->lock());
	auto it = std::remove(sdl->pointers().begin(), sdl->pointers().end(), pointer);
	sdl->pointers().erase(it, sdl->pointers().end());

	sdl_Pointer_FreeCopy(pointer);
}

void sdl_Pointer_FreeCopy(rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<sdlPointer*>(pointer);

	if (!ptr)
		return;

	sdl_Pointer_Clear(ptr);
	winpr_aligned_free(ptr->data);
	ptr->data = nullptr;
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

	std::unique_lock lock(sdl->lock());
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
	const auto pos = sdl->pixelToScreen(id, orig, true);
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

	auto data = static_cast<const BYTE*>(ptr->data);
	if (data)
	{
		if (!SDL_LockSurface(ptr->image))
		{
			WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_LockSurface failed");
			return false;
		}

		auto pixels = static_cast<BYTE*>(ptr->image->pixels);
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
	}

	// create a cursor image in 100% display scale to trick SDL into creating the cursor with the
	// correct size
	auto fw = sdl->getFirstWindow();
	if (!fw)
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "sdl->getFirstWindow() nullptr");
		return false;
	}

	/* SDL cursor backends do not all interpret the surface size the same way: the Windows
	 * backend multiplies it by SDL_GetDisplayContentScale() when
	 * SDL_HINT_MOUSE_DPI_SCALE_CURSORS is enabled (we enable it in sdl_freerdp.cpp), while the
	 * Wayland, X11 and Cocoa backends use it as is. The code above assumes the window pixel
	 * density and the display content scale are equal, which does not hold on Windows: window
	 * sizes are tracked in physical pixels there, so SDL reports a pixel density of 1 on a 200%
	 * display while the display content scale is 2, and the Windows backend scales the cursor
	 * bitmap up a second time, displaying it twice as large as the rest of the session.
	 * Compensate for that backends own scaling so the final cursor still covers
	 * orig * (pos / orig) points, in line with the remote desktop. */
	float surfaceRatio = 1.0f;
	float displayScale = 1.0f;
	const char* const videoDriver = SDL_GetCurrentVideoDriver();
	if (videoDriver && (SDL_strcmp(videoDriver, "windows") == 0))
	{
		displayScale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window));
		const float pixelDensity = SDL_GetWindowPixelDensity(window);
		if ((displayScale > 0.0f) && (pixelDensity > 0.0f))
			surfaceRatio = pixelDensity / displayScale;
	}
	if (std::fabs(surfaceRatio - 1.0f) > 0.01f)
		WLog_Print(sdl->getWLog(), WLOG_DEBUG,
		           "cursor surface scaled by %.2f (driver %s, display scale %.2f)",
		           static_cast<double>(surfaceRatio), videoDriver,
		           static_cast<double>(displayScale));

	const auto w = static_cast<int>(std::lround(pos.w * surfaceRatio));
	const auto h = static_cast<int>(std::lround(pos.h * surfaceRatio));
	if ((w <= 0) || (h <= 0))
		return true;

	std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> normal{
		SDL_CreateSurface(w, h, ptr->image->format), SDL_DestroySurface
	};
	if (!normal)
		return true;
	if (!SDL_BlitSurfaceScaled(ptr->image, nullptr, normal.get(), nullptr,
	                           SDL_ScaleMode::SDL_SCALEMODE_LINEAR))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_BlitSurfaceScaled failed");
		return false;
	}

	/* The alternate image is the full resolution cursor. SDL backends that honour
	 * SDL_HINT_MOUSE_DPI_SCALE_CURSORS pick it for high DPI displays and look up the hotspot of
	 * the image they picked, so it needs one of its own: the hotspot of the scaled down copy
	 * does not apply to an image that is `display scale` times larger. Without this the Windows
	 * backend builds a cursor bitmap that is displayed shifted right and down by
	 * (hotspot * (1 - 1 / scale)), which is invisible for cursors anchored in the top left
	 * corner (e.g. the arrow) but clearly wrong for the ones anchored in the middle (e.g. the
	 * Excel cell cursor). */
	{
		const auto props = SDL_GetSurfaceProperties(ptr->image);
		const auto hx = static_cast<int>(ix);
		const auto hy = static_cast<int>(iy);
		if (!SDL_SetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_X_NUMBER, hx) ||
		    !SDL_SetNumberProperty(props, SDL_PROP_SURFACE_HOTSPOT_Y_NUMBER, hy))
			WLog_Print(sdl->getWLog(), WLOG_WARN,
			           "SDL_SetNumberProperty(hotspot %d,%d) failed", hx, hy);
	}

	if (!SDL_AddSurfaceAlternateImage(normal.get(), ptr->image))
	{
		WLog_Print(sdl->getWLog(), WLOG_ERROR, "SDL_AddSurfaceAlternateImage failed");
		return false;
	}

	auto x = static_cast<int>(std::lround(pos.x * surfaceRatio));
	auto y = static_cast<int>(std::lround(pos.y * surfaceRatio));
	if (x >= w)
		x = w - 1;
	if (y >= h)
		y = h - 1;

	ptr->cursor = SDL_CreateColorCursor(normal.get(), x, y);
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

rdpPointer* sdl_Pointer_Copy(const rdpPointer* pointer)
{
	auto ptr = reinterpret_cast<const sdlPointer*>(pointer);
	if (!pointer)
		return nullptr;

	auto copy = static_cast<sdlPointer*>(calloc(1, sizeof(sdlPointer)));
	if (!copy)
		return nullptr;

	copy->pointer.xPos = pointer->xPos;
	copy->pointer.yPos = pointer->yPos;
	copy->pointer.width = pointer->width;
	copy->pointer.height = pointer->height;
	copy->pointer.xorBpp = pointer->xorBpp;
	if (ptr->size > 0)
	{
		copy->data = static_cast<BYTE*>(winpr_aligned_calloc(1, ptr->size, 32));
		if (!copy->data)
		{
			free(copy);
			return nullptr;
		}
		copy->size = ptr->size;
		memcpy(copy->data, ptr->data, copy->size);
	}
	return &copy->pointer;
}

void sdl_PointerFreeCopyAll(rdpPointer* pointer)
{
	sdl_Pointer_FreeCopy(pointer);
	free(pointer);
}
