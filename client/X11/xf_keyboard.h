/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/kbd/kbd.h>
#include <freerdp/kbd/vkcodes.h>

#include "xfreerdp.h"

void xf_kbd_init(xfInfo* xfi);
void xf_kbd_set_keypress(xfInfo* xfi, uint8 keycode, KeySym keysym);
void xf_kbd_unset_keypress(xfInfo* xfi, uint8 keycode);
boolean xf_kbd_key_pressed(xfInfo* xfi, KeySym keysym);
void xf_kbd_send_key(xfInfo* xfi, boolean down, uint8 keycode);
int xf_kbd_read_keyboard_state(xfInfo* xfi);
boolean xf_kbd_get_key_state(xfInfo* xfi, int state, int keysym);
int xf_kbd_get_toggle_keys_state(xfInfo* xfi);
void xf_kbd_focus_in(xfInfo* xfi);
boolean xf_kbd_handle_special_keys(xfInfo* xfi, KeySym keysym);

#endif /* __XF_KEYBOARD_H */
