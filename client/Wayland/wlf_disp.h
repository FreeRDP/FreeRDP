/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Display Control Channel
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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
#ifndef FREERDP_CLIENT_WAYLAND_DISP_H
#define FREERDP_CLIENT_WAYLAND_DISP_H

#include <freerdp/types.h>
#include <freerdp/client/disp.h>

#include "wlfreerdp.h"

FREERDP_API BOOL wlf_disp_init(wlfDispContext* xfDisp, DispClientContext* disp);
FREERDP_API BOOL wlf_disp_uninit(wlfDispContext* xfDisp, DispClientContext* disp);

wlfDispContext* wlf_disp_new(wlfContext* wlc);
void wlf_disp_free(wlfDispContext* disp);
BOOL wlf_disp_handle_configure(wlfDispContext* disp, int32_t width, int32_t height);
void wlf_disp_resized(wlfDispContext* disp);

int wlf_list_monitors(wlfContext* wlc);

#endif /* FREERDP_CLIENT_WAYLAND_DISP_H */
