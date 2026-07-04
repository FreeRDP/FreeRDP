/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native X11 interactive window move
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
#include "sdl_x11.hpp"
#include "sdl_utils.hpp"

#include <cstdio>

#if defined(WITH_SDL_X11_NATIVE)

#include <X11/Xlib.h>

bool sdl_x11_has_compositor()
{
	if (!sdl::utils::isX11Driver())
		return true;
	/* Short-lived connection: caps are detected once, and no SDL window may exist yet. */
	Display* dpy = XOpenDisplay(nullptr);
	if (!dpy)
		return true; /* can't tell - assume composited (matches historic default) */
	char sel[32];
	(void)snprintf(sel, sizeof(sel), "_NET_WM_CM_S%d", DefaultScreen(dpy));
	const Atom atom = XInternAtom(dpy, sel, False);
	const bool composited = (XGetSelectionOwner(dpy, atom) != None);
	XCloseDisplay(dpy);
	return composited;
}

bool sdl_x11_begin_move_size(SDL_Window* window, int netDirection)
{
	if (!window || !sdl::utils::isX11Driver())
		return false;

	auto props = SDL_GetWindowProperties(window);
	auto dpy = static_cast<Display*>(
	    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
	const auto xwin =
	    static_cast<Window>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
	if (!dpy || (xwin == 0))
		return false;

	/* One display per process; resolve the atom once instead of a roundtrip per move. */
	static Atom s_moveResize = None;
	if (s_moveResize == None)
		s_moveResize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);

	float gx = 0;
	float gy = 0;
	SDL_GetGlobalMouseState(&gx, &gy);

	XClientMessageEvent xev = {};
	xev.type = ClientMessage;
	xev.window = xwin;
	xev.message_type = s_moveResize;
	xev.format = 32;
	xev.data.l[0] = static_cast<long>(gx);
	xev.data.l[1] = static_cast<long>(gy);
	xev.data.l[2] = netDirection;
	xev.data.l[3] = Button1;
	xev.data.l[4] = 1; /* source: normal application */

	/* Release the implicit pointer grab of the pressed button, or the WM cannot take over. */
	XUngrabPointer(dpy, CurrentTime);
	XSendEvent(dpy, DefaultRootWindow(dpy), False,
	           SubstructureRedirectMask | SubstructureNotifyMask, reinterpret_cast<XEvent*>(&xev));
	XFlush(dpy);
	return true;
}

static Window sdl_x11_xwindow(SDL_Window* window)
{
	if (!window)
		return 0;
	auto props = SDL_GetWindowProperties(window);
	return static_cast<Window>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
}

bool sdl_x11_restack_windows(const std::vector<SDL_Window*>& topToBottom)
{
	if (!sdl::utils::isX11Driver() || (topToBottom.size() < 2))
		return false;

	auto props = SDL_GetWindowProperties(topToBottom.front());
	auto dpy = static_cast<Display*>(
	    SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr));
	if (!dpy)
		return false;

	static Atom s_restack = None;
	if (s_restack == None)
		s_restack = XInternAtom(dpy, "_NET_RESTACK_WINDOW", False);

	const Window root = DefaultRootWindow(dpy);
	/* Place each window just below its predecessor: realizing the whole top-to-bottom order with
	 * WM-mediated, focus-neutral restacks. data.l = { source=2 (pager), sibling, detail=Below }. */
	for (size_t i = 1; i < topToBottom.size(); i++)
	{
		const Window win = sdl_x11_xwindow(topToBottom[i]);
		const Window sibling = sdl_x11_xwindow(topToBottom[i - 1]);
		if ((win == 0) || (sibling == 0))
			continue;

		XClientMessageEvent xev = {};
		xev.type = ClientMessage;
		xev.window = win;
		xev.message_type = s_restack;
		xev.format = 32;
		xev.data.l[0] = 2; /* source indication: pager/direct (authoritative) */
		xev.data.l[1] = static_cast<long>(sibling);
		xev.data.l[2] = Below;
		XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask,
		           reinterpret_cast<XEvent*>(&xev));
	}
	XFlush(dpy);
	return true;
}

#else /* !WITH_SDL_X11_NATIVE */

bool sdl_x11_has_compositor()
{
	return true;
}

bool sdl_x11_begin_move_size(SDL_Window* /*window*/, int /*netDirection*/)
{
	return false;
}

bool sdl_x11_restack_windows(const std::vector<SDL_Window*>& /*topToBottom*/)
{
	return false;
}

#endif
