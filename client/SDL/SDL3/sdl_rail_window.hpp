/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client RAIL window
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <freerdp/types.h>

class SdlWindow;

/**
 * A single RemoteApp window, backed by a borderless SDL window.
 *
 * Geometry is server-authoritative: `_windowRect` mirrors the server's WINDOW_ORDER
 * offset/size and the local SDL window is moved/resized to follow it.
 */
class SdlRailWindow
{
  public:
	SdlRailWindow(uint64_t id, const SDL_Rect& rect);
	SdlRailWindow(SdlRailWindow&&) = delete;
	SdlRailWindow(const SdlRailWindow&) = delete;
	SdlRailWindow& operator=(const SdlRailWindow&) = delete;
	SdlRailWindow& operator=(SdlRailWindow&&) = delete;
	~SdlRailWindow();

	[[nodiscard]] uint64_t id() const;
	[[nodiscard]] SDL_WindowID sdlId() const;
	[[nodiscard]] SDL_Window* window() const;
	[[nodiscard]] SDL_Renderer* renderer() const;

	/* --- state setters, safe to call from the RDP thread (no SDL window calls) --- */

	/* Server-driven geometry (from WINDOW_ORDER). */
	void updateWindowRect(const SDL_Rect& rect);
	[[nodiscard]] SDL_Rect windowRect() const;

	/* Deferred destroy: mark here (RDP thread), erased on the main thread in SdlRail::paint. */
	void markDeleted();
	[[nodiscard]] bool isDeleted() const;

	/* Visible sub-rects (window-relative); only these are painted so windows on top don't bleed. */
	void setVisibilityRects(std::vector<SDL_Rect> rects);

	/* Owning window id (WINDOW_ORDER_FIELD_OWNER); popups position relative to it. */
	void setOwner(uint64_t ownerId);
	[[nodiscard]] uint64_t owner() const;
	/* Popup = caption-less transient (menu/dropdown/tooltip); created as an SDL popup. */
	[[nodiscard]] bool isPopup() const;

	void setStyle(uint32_t style, uint32_t exStyle);
	void setTitle(const std::string& title);
	void setTitle(const char16_t* str, size_t lenBytes);
	void setVisible(bool visible);

	/* --- main thread only (SDL window ops) --- */

	/* Window-mapped GFX surface (RDP thread): references the gdi pixels, rendered in paint(). */
	void updateGfxSurface(const void* data, uint32_t stride, uint32_t width, uint32_t height);
	[[nodiscard]] bool hasGfx();

	/* Create/move/show the local SDL window to match pending state; popups use parent+rect. */
	bool reconcile(SDL_Window* parent, const SDL_Rect& parentRect);
	/* Render: GFX surface if mapped, else the shared desktop region. `damage` = updated rects. */
	bool paint(SDL_Surface* primary, SDL_PixelFormat fallbackFormat,
	           const std::vector<SDL_Rect>& damage, SDL_Window* parent = nullptr,
	           const SDL_Rect& parentRect = {});

  private:
	bool create(SDL_Window* parent, const SDL_Rect& parentRect);
	bool paintGfx(SDL_PixelFormat format);
	bool paintLegacy(SDL_Surface* primary, const std::vector<SDL_Rect>& damage);

	uint64_t _id;
	std::unique_ptr<SdlWindow> _win; /* local window+renderer (lazy, main thread) */
	SDL_Rect _windowRect;            /* server offset/size */
	uint32_t _style = 0;
	uint64_t _ownerId = 0;
	bool _isPopup = false;
	bool _layered = false;         /* WS_EX_LAYERED shadow/glass decoration: never rendered */
	bool _popupClassified = false; /* isPopup/_layered frozen after the first (creation) style */
	std::string _title = "RdpRailWindow";
	bool _visible = false;
	bool _geometryDirty = true;
	bool _titleDirty = false;
	bool _painted = false;    /* full copy done; afterwards only damage regions are re-copied */

	/* Guards all state shared between the RDP thread (setters) and the main thread
	 * (reconcile/paint): geometry, title, visibility, flags, vis rects and the gfx snapshot. */
	mutable std::mutex _gfxLock;
	std::vector<SDL_Rect> _visRects;
	bool _deleted = false;
	const void* _gfxData = nullptr;
	uint32_t _gfxStride = 0;
	uint32_t _gfxW = 0;
	uint32_t _gfxH = 0;
	bool _hasGfx = false;
};
