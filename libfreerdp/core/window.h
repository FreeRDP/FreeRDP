/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windowing Alternate Secondary Orders
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

#ifndef __WINDOW_H
#define __WINDOW_H

#include "update.h"

#include <winpr/stream.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

FREERDP_LOCAL void update_free_window_icon_info(ICON_INFO* iconInfo);
FREERDP_LOCAL BOOL update_recv_altsec_window_order(rdpUpdate* update,
        wStream* s);

#define WND_TAG FREERDP_TAG("core.wnd")
#ifdef WITH_DEBUG_WND
#define DEBUG_WND(...) WLog_DBG(WND_TAG, __VA_ARGS__)
#else
#define DEBUG_WND(...) do { } while (0)
#endif

#endif /* __WINDOW_H */
