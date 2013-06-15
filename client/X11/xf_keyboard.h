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

#ifndef __XF_KEYBOARD_H
#define __XF_KEYBOARD_H

#include <freerdp/locale/keyboard.h>

#include "xf_client.h"
#include "xfreerdp.h"

void xf_kbd_init(xfContext* xfc);
void xf_kbd_clear(xfContext* xfc);
void xf_kbd_set_keypress(xfContext* xfc, BYTE keycode, KeySym keysym);
void xf_kbd_unset_keypress(xfContext* xfc, BYTE keycode);
void xf_kbd_release_all_keypress(xfContext* xfc);
BOOL xf_kbd_key_pressed(xfContext* xfc, KeySym keysym);
void xf_kbd_send_key(xfContext* xfc, BOOL down, BYTE keycode);
int xf_kbd_read_keyboard_state(xfContext* xfc);
BOOL xf_kbd_get_key_state(xfContext* xfc, int state, int keysym);
int xf_kbd_get_toggle_keys_state(xfContext* xfc);
void xf_kbd_focus_in(xfContext* xfc);
BOOL xf_kbd_handle_special_keys(xfContext* xfc, KeySym keysym);

#endif /* __XF_KEYBOARD_H */
