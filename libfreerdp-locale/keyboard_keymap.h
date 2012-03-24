/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Keyboard Localization - loading of keymap files
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

#ifndef __KEYBOARD_KEYMAP_H
#define __KEYBOARD_KEYMAP_H

#include <freerdp/types.h>

int freerdp_keyboard_load_map(uint32 keycode_to_vkcode[256], char* name);
void freerdp_keyboard_load_maps(uint32 keycode_to_vkcode[256], char* names);

#endif /* __KEYBOARD_KEYMAP_H */
