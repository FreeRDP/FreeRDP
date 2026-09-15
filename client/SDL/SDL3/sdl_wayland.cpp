/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native Wayland interactive window move
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
#include "sdl_wayland.hpp"
#include "sdl_utils.hpp"

#include <cstring>

#include <winpr/platform.h>

#if defined(WITH_SDL_WAYLAND_NATIVE)

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

/* Dedicated seat/pointer on private queue to track configure serials independently of SDL. */
struct WlMoveCtx
{
	wl_display* display = nullptr;
	wl_event_queue* queue = nullptr;
	wl_registry* registry = nullptr;
	wl_seat* seat = nullptr;
	wl_pointer* pointer = nullptr;
	uint32_t buttonSerial = 0;
	bool initTried = false;
};
static WlMoveCtx g_wl;

static void pointer_enter(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                          WINPR_ATTR_UNUSED uint32_t serial, WINPR_ATTR_UNUSED wl_surface* surface,
                          WINPR_ATTR_UNUSED wl_fixed_t sx, WINPR_ATTR_UNUSED wl_fixed_t sy)
{
}
static void pointer_leave(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                          WINPR_ATTR_UNUSED uint32_t serial, WINPR_ATTR_UNUSED wl_surface* surface)
{
}
static void pointer_motion(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                           WINPR_ATTR_UNUSED uint32_t time, WINPR_ATTR_UNUSED wl_fixed_t sx,
                           WINPR_ATTR_UNUSED wl_fixed_t sy)
{
}
static void pointer_button(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                           uint32_t serial, WINPR_ATTR_UNUSED uint32_t time,
                           WINPR_ATTR_UNUSED uint32_t button, uint32_t state)
{
	if (state == WL_POINTER_BUTTON_STATE_PRESSED)
		g_wl.buttonSerial = serial;
}
static void pointer_axis(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                         WINPR_ATTR_UNUSED uint32_t time, WINPR_ATTR_UNUSED uint32_t axis,
                         WINPR_ATTR_UNUSED wl_fixed_t value)
{
}
static void pointer_frame(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer)
{
}
static void pointer_axis_source(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                                WINPR_ATTR_UNUSED uint32_t axisSource)
{
}
static void pointer_axis_stop(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_pointer* pointer,
                              WINPR_ATTR_UNUSED uint32_t time, WINPR_ATTR_UNUSED uint32_t axis)
{
}
static void pointer_axis_discrete(WINPR_ATTR_UNUSED void* data,
                                  WINPR_ATTR_UNUSED wl_pointer* pointer,
                                  WINPR_ATTR_UNUSED uint32_t axis,
                                  WINPR_ATTR_UNUSED int32_t discrete)
{
}

/* Zero-initialized pointer listener for seat versions <= 5. */
static const wl_pointer_listener& pointerListener()
{
	static const wl_pointer_listener listener = []
	{
		wl_pointer_listener l = {};
		l.enter = pointer_enter;
		l.leave = pointer_leave;
		l.motion = pointer_motion;
		l.button = pointer_button;
		l.axis = pointer_axis;
		l.frame = pointer_frame;
		l.axis_source = pointer_axis_source;
		l.axis_stop = pointer_axis_stop;
		l.axis_discrete = pointer_axis_discrete;
		return l;
	}();
	return listener;
}

static void seat_capabilities(WINPR_ATTR_UNUSED void* data, wl_seat* seat, uint32_t caps)
{
	if (((caps & WL_SEAT_CAPABILITY_POINTER) != 0) && !g_wl.pointer)
	{
		g_wl.pointer = wl_seat_get_pointer(seat);
		if (g_wl.pointer)
			wl_pointer_add_listener(g_wl.pointer, &pointerListener(), nullptr);
	}
}
static void seat_name(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_seat* seat,
                      WINPR_ATTR_UNUSED const char* name)
{
}
static const wl_seat_listener s_seat_listener = { seat_capabilities, seat_name };

static void registry_global(WINPR_ATTR_UNUSED void* data, wl_registry* reg, uint32_t name,
                            const char* iface, uint32_t version)
{
	if (!g_wl.seat && (strcmp(iface, wl_seat_interface.name) == 0))
	{
		const uint32_t v = (version < 5) ? version : 5;
		g_wl.seat = static_cast<wl_seat*>(wl_registry_bind(reg, name, &wl_seat_interface, v));
		if (g_wl.seat)
			wl_seat_add_listener(g_wl.seat, &s_seat_listener, nullptr);
	}
}
static void registry_global_remove(WINPR_ATTR_UNUSED void* data, WINPR_ATTR_UNUSED wl_registry* reg,
                                   WINPR_ATTR_UNUSED uint32_t name)
{
}
static const wl_registry_listener s_registry_listener = { registry_global, registry_global_remove };

static bool ensure_init(SDL_Window* window)
{
	if (g_wl.pointer)
		return true;
	if (g_wl.initTried)
		return false;
	g_wl.initTried = true;

	auto props = SDL_GetWindowProperties(window);
	g_wl.display = static_cast<wl_display*>(
	    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr));
	if (!g_wl.display)
		return false;

	g_wl.queue = wl_display_create_queue(g_wl.display);
	if (!g_wl.queue)
		return false;

	auto wrapped = static_cast<wl_display*>(wl_proxy_create_wrapper(g_wl.display));
	if (!wrapped)
		return false;
	wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), g_wl.queue);
	g_wl.registry = wl_display_get_registry(wrapped);
	wl_proxy_wrapper_destroy(wrapped);
	if (!g_wl.registry)
		return false;

	wl_registry_add_listener(g_wl.registry, &s_registry_listener, nullptr);
	/* First roundtrip delivers globals (seat), second the seat capabilities (pointer). */
	wl_display_roundtrip_queue(g_wl.display, g_wl.queue);
	wl_display_roundtrip_queue(g_wl.display, g_wl.queue);
	return g_wl.pointer != nullptr;
}

void sdl_wayland_move_prepare(SDL_Window* window)
{
	if (window && sdl::utils::isWaylandDriver())
		(void)ensure_init(window);
}

static bool sdl_wayland_begin_interactive(SDL_Window* window, bool move, uint32_t xdgEdge)
{
	if (!window || !sdl::utils::isWaylandDriver())
		return false;
	if (!ensure_init(window))
		return false;

	/* Drain queue so buttonSerial reflects the triggering press. */
	wl_display_roundtrip_queue(g_wl.display, g_wl.queue);

	auto props = SDL_GetWindowProperties(window);
	auto toplevel = static_cast<xdg_toplevel*>(
	    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER, nullptr));
	if (!toplevel || !g_wl.seat || (g_wl.buttonSerial == 0))
		return false;

	if (move)
		xdg_toplevel_move(toplevel, g_wl.seat, g_wl.buttonSerial);
	else
		xdg_toplevel_resize(toplevel, g_wl.seat, g_wl.buttonSerial, xdgEdge);
	wl_display_flush(g_wl.display);
	return true;
}

bool sdl_wayland_begin_move(SDL_Window* window)
{
	return sdl_wayland_begin_interactive(window, true, 0);
}

bool sdl_wayland_begin_resize(SDL_Window* window, uint32_t xdgEdge)
{
	return sdl_wayland_begin_interactive(window, false, xdgEdge);
}

#else /* !WITH_SDL_WAYLAND_NATIVE */

void sdl_wayland_move_prepare(WINPR_ATTR_UNUSED SDL_Window* window)
{
}

bool sdl_wayland_begin_move(WINPR_ATTR_UNUSED SDL_Window* window)
{
	return false;
}

bool sdl_wayland_begin_resize(WINPR_ATTR_UNUSED SDL_Window* window,
                              WINPR_ATTR_UNUSED uint32_t xdgEdge)
{
	return false;
}

#endif
