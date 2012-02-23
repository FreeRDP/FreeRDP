/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * XKB Keyboard Mapping
 *
 * Copyright 2009-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __LOCALE_KEYBOARD_XKB_H
#define __LOCALE_KEYBOARD_XKB_H

#include <freerdp/types.h>
#include <freerdp/locale/keyboard.h>

struct _VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME
{
	uint32 vkcode; /* virtual key code */
	const char* xkb_keyname; /* XKB keyname */
	const char* xkb_keyname_extended; /* XKB keyname (extended) */
};
typedef struct _VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME VIRTUAL_KEY_CODE_TO_XKB_KEY_NAME;

uint32 freerdp_keyboard_init_xkb(uint32 keyboardLayoutId);
uint32 detect_keyboard_layout_from_xkb(void* display);
int freerdp_keyboard_load_map_from_xkb(void* display);

#endif /* __LOCALE_KEYBOARD_XKB_H */
