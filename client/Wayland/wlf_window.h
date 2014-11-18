/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Windows
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
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

#ifndef __WLF_WINDOW_H
#define __WLF_WINDOW_H

#include <wayland-client.h>

typedef struct wlf_window wlfWindow;

#include "wlfreerdp.h"

struct wlf_buffer
{
	struct wl_buffer* buffer;
	void* shm_data;
	BOOL busy;
};
typedef struct wlf_buffer wlfBuffer;

struct wlf_window
{
	int width;
	int height;
	struct wl_surface* surface;
	struct wl_shell_surface* shell_surface;
	struct wl_callback* callback;
	wlfBuffer buffers[2];
	wlfDisplay* display;
	void* data;
	BOOL fullscreen;
};

wlfWindow* wlf_CreateDesktopWindow(wlfContext* wlfc, char* name, int width, int height, BOOL decorations);
void wlf_ResizeDesktopWindow(wlfContext* wlfc, wlfWindow* window, int width, int height);
void wlf_SetWindowText(wlfContext* wlfc, wlfWindow* window, char* name);
void wlf_SetWindowFullscreen(wlfContext* wlfc, wlfWindow* window, BOOL fullscree);
void wlf_ShowWindow(wlfContext* wlfc, wlfWindow* window, BYTE state);
void wlf_UpdateWindowArea(wlfContext* wlfc, wlfWindow* window, int x, int y, int width, int height);
void wlf_DestroyWindow(wlfContext* wlfc, wlfWindow* window);

#endif /* __WLF_WINDOW_H */
