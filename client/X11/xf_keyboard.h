/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Keyboard Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_XF_KEYBOARD_H
#define FREERDP_CLIENT_X11_XF_KEYBOARD_H

#include <freerdp/locale/keyboard.h>

#include "xf_client.h"
#include "xfreerdp.h"

typedef struct
{
	BOOL Shift;
	BOOL LeftShift;
	BOOL RightShift;
	BOOL Alt;
	BOOL LeftAlt;
	BOOL RightAlt;
	BOOL Ctrl;
	BOOL LeftCtrl;
	BOOL RightCtrl;
	BOOL Super;
	BOOL LeftSuper;
	BOOL RightSuper;
} XF_MODIFIER_KEYS;

BOOL xf_keyboard_init(xfContext* xfc);
void xf_keyboard_free(xfContext* xfc);

void xf_keyboard_key_press(xfContext* xfc, const XKeyEvent* event, KeySym keysym);
void xf_keyboard_key_release(xfContext* xfc, const XKeyEvent* event, KeySym keysym);

void xf_keyboard_release_all_keypress(xfContext* xfc);
BOOL xf_keyboard_key_pressed(xfContext* xfc, KeySym keysym);

int xf_keyboard_read_keyboard_state(xfContext* xfc);
BOOL xf_keyboard_get_key_state(xfContext* xfc, int state, int keysym);
UINT32 xf_keyboard_get_toggle_keys_state(xfContext* xfc);
void xf_keyboard_focus_in(xfContext* xfc);
BOOL xf_keyboard_handle_special_keys(xfContext* xfc, KeySym keysym);
void xf_keyboard_handle_special_keys_release(xfContext* xfc, KeySym keysym);
BOOL xf_keyboard_set_indicators(rdpContext* context, UINT16 led_flags);
BOOL xf_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                UINT32 imeConvMode);

BOOL xf_ungrab(xfContext* xfc);

#endif /* FREERDP_CLIENT_X11_XF_KEYBOARD_H */
