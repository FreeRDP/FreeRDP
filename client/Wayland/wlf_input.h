/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Input
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

#ifndef __WLF_INPUT_H
#define __WLF_INPUT_H

#include <wayland-client.h>

typedef struct wlf_input wlfInput;

#include "wlfreerdp.h"

struct wlf_input
{
	rdpInput* input;
	UINT16 last_x;
	UINT16 last_y;

	struct wl_pointer* pointer;
	struct wl_keyboard* keyboard;
};

wlfInput* wlf_CreateInput(wlfContext* wlfc);
void wlf_DestroyInput(wlfContext* wlfc, wlfInput* input);

#endif /* __WLF_INPUT_H */
