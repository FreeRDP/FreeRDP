/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Windows
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

#ifndef FREERDP_CLIENT_X11_FLOATBAR_H
#define FREERDP_CLIENT_X11_FLOATBAR_H

typedef struct xf_floatbar xfFloatbar;

#include "xfreerdp.h"

typedef void(*OnClick)(xfContext*);
xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window);
BOOL xf_floatbar_event_process(xfContext* xfc, XEvent* event);
BOOL xf_floatbar_check_event(xfContext* xfc, XEvent* event);
void xf_floatbar_toggle_visibility(xfContext* xfc, bool visible);
void xf_floatbar_free(xfContext* xfc, xfWindow* window, xfFloatbar* floatbar);
void xf_floatbar_hide_and_show(xfContext* xfc);
void xf_floatbar_set_root_y(xfContext* xfc, int y);

#endif /* FREERDP_CLIENT_X11_FLOATBAR_H */
