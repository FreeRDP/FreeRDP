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
typedef struct xf_floatbar_button xfFloatbarButton;

#include "xfreerdp.h"

#define XF_FLOATBAR_MODE_NONE 0
#define XF_FLOATBAR_MODE_DRAGGING 1
#define XF_FLOATBAR_MODE_RESIZE_LEFT 2
#define XF_FLOATBAR_MODE_RESIZE_RIGHT 3

struct xf_floatbar
{
	int x;
	int y;
	int width;
	int height;
	int mode;
	int last_motion_x_root;
	bool locked;
	xfFloatbarButton* buttons[5];
	Window handle;
};

#define XF_FLOATBAR_BUTTON_CLOSE 1
#define XF_FLOATBAR_BUTTON_RESTORE 2
#define XF_FLOATBAR_BUTTON_MINIMIZE 3
#define XF_FLOATBAR_BUTTON_LOCKED 4
#define XF_FLOATBAR_BUTTON_UNLOCKED 5

typedef void(*OnClick)(xfContext*);

struct xf_floatbar_button
{
	int x;
	int y;
	int type;
	bool focus;
	bool clicked;
	OnClick onclick;
	Window handle;
};

xfFloatbar* xf_floatbar_new(xfContext* xfc, Window window, int width);
void xf_floatbar_event_process(xfContext* xfc, XEvent* event);
void xf_floatbar_toggle_visibility(xfContext* xfc, bool visible);

#endif /* FREERDP_CLIENT_X11_FLOATBAR_H */