/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Displays
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

#ifndef __WLF_DISPLAY_H
#define __WLF_DISPLAY_H

#include <wayland-client.h>

typedef struct wlf_display wlfDisplay;

#include "wlfreerdp.h"

struct wlf_display
{
	struct wl_display* display;
	struct wl_registry* registry;
	struct wl_compositor* compositor;
	struct wl_shell* shell;
	struct wl_shm* shm;
	struct wl_seat* seat;
};

wlfDisplay* wlf_CreateDisplay(void);
BOOL wlf_RefreshDisplay(wlfDisplay* display);
void wlf_DestroyDisplay(wlfContext* wlfc, wlfDisplay* display);

#endif /* __WLF_DISPLAY_H */
