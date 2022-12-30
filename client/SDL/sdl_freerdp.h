/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#ifndef FREERDP_CLIENT_SDL_H
#define FREERDP_CLIENT_SDL_H

#include <freerdp/freerdp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>

#include <SDL.h>
#include <SDL_video.h>

typedef struct s_sdlDispContext sdlDispContext;
typedef struct
{
	SDL_Window* window;
	int offset_x;
	int offset_y;
} sdl_window_t;

typedef struct
{
	rdpClientContext common;

	/* SDL */
	BOOL fullscreen;
	BOOL resizeable;
	BOOL grab_mouse;
	BOOL grab_kbd;
	BOOL highDpi;

	size_t windowCount;
	sdl_window_t windows[16];

	HANDLE thread;

	SDL_Surface* primary;

	sdlDispContext* disp;
	Uint32 sdl_pixel_format;
} sdlContext;

void update_resizeable(sdlContext* sdl, BOOL enable);
void update_fullscreen(sdlContext* sdl, BOOL enter);

#endif /* FREERDP_CLIENT_SDL_H */
