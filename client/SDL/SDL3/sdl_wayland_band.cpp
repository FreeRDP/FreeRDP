/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native Wayland RAIL resize band (wl_subsurface ring)
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
#include "sdl_wayland_band.hpp"
#include "sdl_utils.hpp"

#include <cstring>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("sdl.wayland.band")

#if defined(WITH_SDL_WAYLAND_NATIVE)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

/* Wayland pointer button codes are Linux evdev codes by protocol definition; define the one value
 * we use instead of pulling <linux/input-event-codes.h> (its path differs on BSD). */
#ifndef BTN_LEFT
#define BTN_LEFT 0x110
#endif

#if defined(WITH_WAYLAND_CURSOR)
#include <wayland-cursor.h>
#endif

namespace
{
	/* Fully transparent: the resize band is input-only, its ring must stay invisible. */
	constexpr uint32_t kBandFill = 0x00000000;

	/* One outside resize band: a wl_subsurface ring around an SDL window's surface. SDL ignores
	 * pointer events on foreign surfaces (proxy tag check), so the ring needs our own pointer. */
	struct WlBand
	{
		SDL_WindowID id = 0;
		wl_surface* parent = nullptr;
		wl_surface* surface = nullptr;
		wl_subsurface* sub = nullptr;
		wl_buffer* buffer = nullptr;
		int cw = 0; /* content (SDL window) size */
		int ch = 0;
		int l = 0; /* insets: left top right bottom */
		int t = 0;
		int r = 0;
		int b = 0;
	};

	/* Self-contained Wayland context for the band: its own registry/seat/pointer + the globals the
	 * ring needs. Independent of the move code in sdl_wayland.cpp (which keeps its own pointer) so
	 * this module is a clean drop-in/drop-out unit. The compositor sends the same input serials to
	 * every pointer resource of the client, so the resize the RAIL layer starts (via the move
	 * code's begin_resize) uses the same press serial this pointer saw. */
	struct WlBandCtx
	{
		wl_display* display = nullptr;
		wl_registry* registry = nullptr;
		wl_seat* seat = nullptr;
		wl_pointer* pointer = nullptr;
		wl_compositor* compositor = nullptr;
		wl_subcompositor* subcompositor = nullptr;
		wl_shm* shm = nullptr;
		uint32_t enterSerial = 0; /* our pointer's last enter, needed by set_cursor */
		bool initTried = false;
		std::vector<std::unique_ptr<WlBand>> bands;
		WlBand* focus = nullptr; /* band under our pointer; px/py/hoverEdge only valid with it */
		double px = -1;
		double py = -1;
		uint32_t hoverEdge = 0; /* XDG_TOPLEVEL_RESIZE_EDGE_* under the pointer */
#if defined(WITH_WAYLAND_CURSOR)
		wl_cursor_theme* cursorTheme = nullptr;
		bool cursorThemeTried = false;
		wl_surface* cursorSurface = nullptr;
		const char* cursorName = nullptr; /* currently shown shape (dedup) */
#endif
	};
	WlBandCtx g_band;

	WlBand* bandBySurface(wl_surface* surface)
	{
		for (auto& bd : g_band.bands)
			if (bd->surface == surface)
				return bd.get();
		return nullptr;
	}

	WlBand* bandByWindow(SDL_WindowID id)
	{
		for (auto& bd : g_band.bands)
			if (bd->id == id)
				return bd.get();
		return nullptr;
	}

	/* XDG_TOPLEVEL_RESIZE_EDGE_* under a band-local point (corners = OR of the two edges).
	 * Pure-edge hits near a corner widen to the corner, like a real WM frame. */
	uint32_t bandEdge(const WlBand& bd, double fx, double fy)
	{
		const int x = static_cast<int>(fx);
		const int y = static_cast<int>(fy);
		const int W = bd.l + bd.cw + bd.r;
		const int H = bd.t + bd.ch + bd.b;
		constexpr int corner = 24;
		uint32_t e = 0;
		if (x < bd.l)
			e |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
		else if (x >= bd.l + bd.cw)
			e |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
		if (y < bd.t)
			e |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
		else if (y >= bd.t + bd.ch)
			e |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
		if ((e == XDG_TOPLEVEL_RESIZE_EDGE_TOP) || (e == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM))
		{
			if (x < bd.l + corner)
				e |= XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
			else if (x >= W - bd.r - corner)
				e |= XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
		}
		else if ((e == XDG_TOPLEVEL_RESIZE_EDGE_LEFT) || (e == XDG_TOPLEVEL_RESIZE_EDGE_RIGHT))
		{
			if (y < bd.t + corner)
				e |= XDG_TOPLEVEL_RESIZE_EDGE_TOP;
			else if (y >= H - bd.b - corner)
				e |= XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
		}
		return e;
	}

	/* Resize cursor over the band: the pointer is ours, so SDL cannot show one - load the theme
	 * cursor and set it on our pointer with our enter serial. On leaving the band, SDL's own
	 * enter into its surface re-sets its cursor, so no reset is needed here. */
	void setBandCursor(uint32_t edge)
	{
#if defined(WITH_WAYLAND_CURSOR)
		struct Shape
		{
			uint32_t edge;
			const char* name;
			const char* legacy;
		};
		static const Shape shapes[] = {
			{ XDG_TOPLEVEL_RESIZE_EDGE_LEFT, "w-resize", "left_side" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_RIGHT, "e-resize", "right_side" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_TOP, "n-resize", "top_side" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM, "s-resize", "bottom_side" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT, "nw-resize", "top_left_corner" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT, "ne-resize", "top_right_corner" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT, "sw-resize", "bottom_left_corner" },
			{ XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT, "se-resize", "bottom_right_corner" },
		};
		const Shape* shape = nullptr;
		for (const auto& sh : shapes)
			if (sh.edge == edge)
				shape = &sh;
		if (!shape || !g_band.pointer || !g_band.shm)
			return;
		if (!g_band.cursorThemeTried)
		{
			g_band.cursorThemeTried = true;
			int size = 24;
			if (const char* env = getenv("XCURSOR_SIZE"))
				size = std::max(atoi(env), 1);
			g_band.cursorTheme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), size, g_band.shm);
			WLog_DBG(TAG, "cursor theme %s (size %d)",
			         g_band.cursorTheme ? "loaded" : "unavailable", size);
		}
		if (!g_band.cursorTheme)
			return;
		if (g_band.cursorName == shape->name)
			return; /* already shown */
		wl_cursor* cursor = wl_cursor_theme_get_cursor(g_band.cursorTheme, shape->name);
		if (!cursor)
			cursor = wl_cursor_theme_get_cursor(g_band.cursorTheme, shape->legacy);
		if (!cursor || (cursor->image_count == 0))
			return;
		wl_cursor_image* image = cursor->images[0];
		wl_buffer* buffer = wl_cursor_image_get_buffer(image);
		if (!buffer)
			return;
		if (!g_band.cursorSurface)
			g_band.cursorSurface = wl_compositor_create_surface(g_band.compositor);
		if (!g_band.cursorSurface)
			return;
		wl_surface_attach(g_band.cursorSurface, buffer, 0, 0);
		wl_surface_damage(g_band.cursorSurface, 0, 0, static_cast<int32_t>(image->width),
		                  static_cast<int32_t>(image->height));
		wl_surface_commit(g_band.cursorSurface);
		wl_pointer_set_cursor(g_band.pointer, g_band.enterSerial, g_band.cursorSurface,
		                      static_cast<int32_t>(image->hotspot_x),
		                      static_cast<int32_t>(image->hotspot_y));
		g_band.cursorName = shape->name;
#else
		(void)edge;
#endif
	}

	/* A sized anonymous shared-memory fd for a band buffer: memfd_create on Linux/FreeBSD 13+, else
	 * an immediately-unlinked temp file (POSIX, no librt link). */
	int bandShmFd(size_t size)
	{
		int fd = -1;
#if defined(__linux__) || \
    (defined(__FreeBSD__) && defined(__FreeBSD_version) && (__FreeBSD_version >= 1300000))
		fd = memfd_create("sdl-rail-band", MFD_CLOEXEC);
#endif
		if (fd < 0)
		{
			const char* dir = getenv("XDG_RUNTIME_DIR");
			char tmpl[256];
			(void)snprintf(tmpl, sizeof(tmpl), "%s/sdl-rail-band-XXXXXX",
			               (dir && *dir) ? dir : "/tmp");
			fd = mkstemp(tmpl);
			if (fd >= 0)
				(void)unlink(tmpl);
		}
		if (fd < 0)
			return -1;
		if (ftruncate(fd, static_cast<off_t>(size)) != 0)
		{
			close(fd);
			return -1;
		}
		return fd;
	}

	/* The shm data is munmapped right after the fill (the compositor has its own mapping) and
	 * the wl_buffer is destroyed ONLY by us on replace/remove: a release listener would race the
	 * compositor releasing the current buffer on unmap (hidden window) and leave a dangling
	 * bd->buffer behind. */
	wl_buffer* createBandBuffer(const WlBand& bd)
	{
		const int W = bd.l + bd.cw + bd.r;
		const int H = bd.t + bd.ch + bd.b;
		const size_t stride = static_cast<size_t>(W) * 4;
		const size_t size = stride * static_cast<size_t>(H);
		const int fd = bandShmFd(size);
		if (fd < 0)
			return nullptr;
		auto* px =
		    static_cast<uint32_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
		if (px == MAP_FAILED)
		{
			close(fd);
			return nullptr;
		}
		/* Transparent center, tinted ring. */
		for (int y = 0; y < H; y++)
		{
			uint32_t* row = px + static_cast<size_t>(y) * static_cast<size_t>(W);
			if ((y < bd.t) || (y >= bd.t + bd.ch))
			{
				for (int x = 0; x < W; x++)
					row[x] = kBandFill;
				continue;
			}
			for (int x = 0; x < bd.l; x++)
				row[x] = kBandFill;
			for (int x = bd.l + bd.cw; x < W; x++)
				row[x] = kBandFill;
		}
		wl_shm_pool* pool = wl_shm_create_pool(g_band.shm, fd, static_cast<int32_t>(size));
		close(fd);
		if (!pool)
		{
			munmap(px, size);
			return nullptr;
		}
		wl_buffer* buffer = wl_shm_pool_create_buffer(pool, 0, W, H, static_cast<int32_t>(stride),
		                                              WL_SHM_FORMAT_ARGB8888);
		wl_shm_pool_destroy(pool);
		munmap(px, size);
		return buffer;
	}

	void removeBand(WlBand* bd)
	{
		if (!bd)
			return;
		WLog_DBG(TAG, "remove sdl=%u", bd->id);
		if (g_band.focus == bd)
			g_band.focus = nullptr;
		if (bd->buffer)
			wl_buffer_destroy(bd->buffer);
		if (bd->sub)
			wl_subsurface_destroy(bd->sub);
		if (bd->surface)
			wl_surface_destroy(bd->surface);
		g_band.bands.erase(std::remove_if(g_band.bands.begin(), g_band.bands.end(),
		                                  [bd](const auto& b) { return b.get() == bd; }),
		                   g_band.bands.end());
	}

	void pointer_enter(void*, wl_pointer*, uint32_t serial, wl_surface* surface, wl_fixed_t sx,
	                   wl_fixed_t sy)
	{
		g_band.focus = surface ? bandBySurface(surface) : nullptr;
		g_band.enterSerial = serial;
#if defined(WITH_WAYLAND_CURSOR)
		g_band.cursorName = nullptr; /* new enter = new cursor state */
#endif
		if (!g_band.focus)
			return;
		WlBand* bd = g_band.focus;
		g_band.px = wl_fixed_to_double(sx);
		g_band.py = wl_fixed_to_double(sy);
		g_band.hoverEdge = bandEdge(*bd, g_band.px, g_band.py);
		setBandCursor(g_band.hoverEdge);
		WLog_DBG(TAG, "enter sdl=%u pos=%.0f,%.0f edge=%u serial=%u", bd->id, g_band.px, g_band.py,
		         g_band.hoverEdge, serial);
		/* A pointer enter is how we learn a compositor resize grab ended over the band (SDL sees
		 * no enter there); code 0 = completion probe, a no-op when no resize is active. */
		std::ignore =
		    sdl_push_user_event(SDL_EVENT_USER_RAIL_BAND, static_cast<uint32_t>(bd->id), 0);
	}
	void pointer_leave(void*, wl_pointer*, uint32_t, wl_surface* surface)
	{
		if (g_band.focus && surface && (g_band.focus->surface == surface))
		{
			WLog_DBG(TAG, "leave sdl=%u", g_band.focus->id);
			g_band.focus = nullptr;
		}
	}
	void pointer_motion(void*, wl_pointer*, uint32_t, wl_fixed_t sx, wl_fixed_t sy)
	{
		WlBand* bd = g_band.focus;
		if (!bd)
			return;
		g_band.px = wl_fixed_to_double(sx);
		g_band.py = wl_fixed_to_double(sy);
		const uint32_t edge = bandEdge(*bd, g_band.px, g_band.py);
		if (edge != g_band.hoverEdge)
		{
			g_band.hoverEdge = edge;
			setBandCursor(edge);
		}
	}
	void pointer_button(void*, wl_pointer*, uint32_t serial, uint32_t, uint32_t button,
	                    uint32_t state)
	{
		if (state != WL_POINTER_BUTTON_STATE_PRESSED)
			return;
		WlBand* bd = g_band.focus;
		if (!bd || (button != BTN_LEFT))
			return;
		const uint32_t edge = bandEdge(*bd, g_band.px, g_band.py);
		WLog_DBG(TAG, "press sdl=%u pos=%.0f,%.0f edge=%u serial=%u", bd->id, g_band.px, g_band.py,
		         edge, serial);
		if (edge == 0)
			return;
		/* Start the compositor resize with OUR seat + this press serial: the move code's pointer
		 * (a separate wl_pointer) never saw this press, so its serial would be stale. Then notify
		 * RAIL to set up the completion state. */
		SDL_Window* window = SDL_GetWindowFromID(bd->id);
		auto* toplevel = window ? static_cast<xdg_toplevel*>(SDL_GetPointerProperty(
		                              SDL_GetWindowProperties(window),
		                              SDL_PROP_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, nullptr))
		                        : nullptr;
		if (!toplevel || !g_band.seat)
		{
			WLog_WARN(TAG, "resize start failed (toplevel=%p seat=%p) sdl=%u",
			          static_cast<void*>(toplevel), static_cast<void*>(g_band.seat), bd->id);
			return;
		}
		xdg_toplevel_resize(toplevel, g_band.seat, serial, edge);
		wl_display_flush(g_band.display);
		std::ignore = sdl_push_user_event(SDL_EVENT_USER_RAIL_BAND, static_cast<uint32_t>(bd->id),
		                                  static_cast<int>(edge));
	}
	void pointer_axis(void*, wl_pointer*, uint32_t, uint32_t, wl_fixed_t)
	{
	}
	void pointer_frame(void*, wl_pointer*)
	{
	}
	void pointer_axis_source(void*, wl_pointer*, uint32_t)
	{
	}
	void pointer_axis_stop(void*, wl_pointer*, uint32_t, uint32_t)
	{
	}
	void pointer_axis_discrete(void*, wl_pointer*, uint32_t, int32_t)
	{
	}

	/* Positional init in wl_pointer_listener member order; members beyond axis_discrete belong to
	 * seat versions > 5 (we bind at most version 5, so they never fire) and zero-init to null. */
	const wl_pointer_listener s_pointer_listener = {
		pointer_enter, pointer_leave,       pointer_motion,    pointer_button,        pointer_axis,
		pointer_frame, pointer_axis_source, pointer_axis_stop, pointer_axis_discrete,
	};

	void seat_capabilities(void*, wl_seat* seat, uint32_t caps)
	{
		if (((caps & WL_SEAT_CAPABILITY_POINTER) != 0) && !g_band.pointer)
		{
			g_band.pointer = wl_seat_get_pointer(seat);
			if (g_band.pointer)
			{
				wl_pointer_add_listener(g_band.pointer, &s_pointer_listener, nullptr);
				WLog_DBG(TAG, "ready pointer=1 compositor=%d subcompositor=%d shm=%d",
				         g_band.compositor ? 1 : 0, g_band.subcompositor ? 1 : 0,
				         g_band.shm ? 1 : 0);
			}
		}
	}
	void seat_name(void*, wl_seat*, const char*)
	{
	}
	const wl_seat_listener s_seat_listener = { seat_capabilities, seat_name };

	void registry_global(void*, wl_registry* reg, uint32_t name, const char* iface,
	                     uint32_t version)
	{
		if (!g_band.seat && (strcmp(iface, wl_seat_interface.name) == 0))
		{
			const uint32_t v = (version < 5) ? version : 5;
			g_band.seat = static_cast<wl_seat*>(wl_registry_bind(reg, name, &wl_seat_interface, v));
			if (g_band.seat)
				wl_seat_add_listener(g_band.seat, &s_seat_listener, nullptr);
		}
		else if (!g_band.compositor && (strcmp(iface, wl_compositor_interface.name) == 0))
		{
			const uint32_t v = (version < 4) ? version : 4;
			g_band.compositor = static_cast<wl_compositor*>(
			    wl_registry_bind(reg, name, &wl_compositor_interface, v));
		}
		else if (!g_band.subcompositor && (strcmp(iface, wl_subcompositor_interface.name) == 0))
		{
			g_band.subcompositor = static_cast<wl_subcompositor*>(
			    wl_registry_bind(reg, name, &wl_subcompositor_interface, 1));
		}
		else if (!g_band.shm && (strcmp(iface, wl_shm_interface.name) == 0))
		{
			g_band.shm = static_cast<wl_shm*>(wl_registry_bind(reg, name, &wl_shm_interface, 1));
		}
	}
	void registry_global_remove(void*, wl_registry*, uint32_t)
	{
	}
	const wl_registry_listener s_registry_listener = { registry_global, registry_global_remove };

	/* Lazy, non-blocking init: register the listener and flush; the globals and seat capabilities
	 * then arrive through SDL's own event pump within a frame or two (callers just retry). NO
	 * roundtrip here: reading the fd ourselves at window-creation time queues stale configures for
	 * freshly created/destroyed libdecor frames -> use-after-free in SDL. */
	bool ensure_init(SDL_Window* window)
	{
		if (g_band.pointer && g_band.compositor && g_band.subcompositor && g_band.shm)
			return true;
		if (!g_band.initTried)
		{
			g_band.initTried = true;
			auto props = SDL_GetWindowProperties(window);
			g_band.display = static_cast<wl_display*>(
			    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
			if (!g_band.display)
				return false;
			g_band.registry = wl_display_get_registry(g_band.display);
			if (!g_band.registry)
				return false;
			wl_registry_add_listener(g_band.registry, &s_registry_listener, nullptr);
			wl_display_flush(g_band.display);
			WLog_DBG(TAG, "init requested (lazy, via SDL pump)");
		}
		return false; /* not ready this call; the pump delivers the globals shortly */
	}
} // namespace

bool sdl_wayland_band_sync(SDL_Window* window, int left, int top, int right, int bottom)
{
	if (!window || !sdl::utils::isWaylandDriver())
		return false;
	const SDL_WindowID id = SDL_GetWindowID(window);
	WlBand* bd = bandByWindow(id);
	if ((left <= 0) && (top <= 0) && (right <= 0) && (bottom <= 0))
	{
		removeBand(bd);
		return false;
	}
	if (!ensure_init(window) || !g_band.compositor || !g_band.subcompositor || !g_band.shm)
		return false;

	int cw = 0;
	int ch = 0;
	if (!SDL_GetWindowSize(window, &cw, &ch) || (cw <= 0) || (ch <= 0))
		return false;

	/* Steady-state exit before the property lookups: this runs per window per paint. */
	if (bd && (bd->cw == cw) && (bd->ch == ch) && (bd->l == left) && (bd->t == top) &&
	    (bd->r == right) && (bd->b == bottom))
		return false;

	auto props = SDL_GetWindowProperties(window);
	auto parent = static_cast<wl_surface*>(
	    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
	if (!parent)
		return false;

	if (bd && (bd->parent != parent))
	{
		/* The SDL window was re-created; the old parent surface is gone. */
		removeBand(bd);
		bd = nullptr;
	}

	bool created = false;
	if (!bd)
	{
		auto band = std::make_unique<WlBand>();
		band->id = id;
		band->parent = parent;
		band->surface = wl_compositor_create_surface(g_band.compositor);
		if (!band->surface)
			return false;
		band->sub = wl_subcompositor_get_subsurface(g_band.subcompositor, band->surface, parent);
		if (!band->sub)
		{
			wl_surface_destroy(band->surface);
			return false;
		}
		/* Desync: band commits apply without waiting for a parent commit. Below: the band's
		 * overlap with the content area stays hidden under the parent. */
		wl_subsurface_set_desync(band->sub);
		wl_subsurface_place_below(band->sub, parent);
		bd = band.get();
		g_band.bands.push_back(std::move(band));
		created = true;
	}
	bd->cw = cw;
	bd->ch = ch;
	bd->l = left;
	bd->t = top;
	bd->r = right;
	bd->b = bottom;

	wl_buffer* buffer = createBandBuffer(*bd);
	if (!buffer)
		return false;
	wl_buffer* oldBuffer = bd->buffer;
	bd->buffer = buffer;

	/* Ring-only input region: interior clicks fall through to the SDL surface below. */
	const int W = left + cw + right;
	const int H = top + ch + bottom;
	wl_region* region = wl_compositor_create_region(g_band.compositor);
	if (region)
	{
		wl_region_add(region, 0, 0, W, top);
		wl_region_add(region, 0, top + ch, W, bottom);
		wl_region_add(region, 0, top, left, ch);
		wl_region_add(region, left + cw, top, right, ch);
		wl_surface_set_input_region(bd->surface, region);
		wl_region_destroy(region);
	}
	wl_subsurface_set_position(bd->sub, -left, -top);
	wl_surface_attach(bd->surface, buffer, 0, 0);
	wl_surface_damage(bd->surface, 0, 0, W, H);
	wl_surface_commit(bd->surface);
	if (oldBuffer)
		wl_buffer_destroy(oldBuffer);
	wl_display_flush(g_band.display);
	WLog_DBG(TAG, "sync sdl=%u content=%dx%d insets=%d,%d,%d,%d outer=%dx%d created=%d", id, cw, ch,
	         left, top, right, bottom, W, H, created ? 1 : 0);
	return true;
}

void sdl_wayland_band_remove(SDL_Window* window)
{
	if (!window)
		return;
	removeBand(bandByWindow(SDL_GetWindowID(window)));
}

#else /* !WITH_SDL_WAYLAND_NATIVE */

bool sdl_wayland_band_sync(SDL_Window* /*window*/, int /*left*/, int /*top*/, int /*right*/,
                           int /*bottom*/)
{
	return false;
}

void sdl_wayland_band_remove(SDL_Window* /*window*/)
{
}

#endif
