/**
 * FreeRDP: A Remote Desktop Protocol Client
 * XKB-based Keyboard Mapping to Microsoft Keyboard System
 *
 * Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __LAYOUTS_XKB_H
#define __LAYOUTS_XKB_H

typedef unsigned char KeycodeToVkcode[256];

typedef struct
{
	unsigned char extended;
	unsigned char keycode;
	char* keyname;
} RdpKeycodeRec, RdpScancodes[256];

#ifdef WITH_XKBFILE

int init_xkb(void *dpy);
unsigned int detect_keyboard_layout_from_xkb(void *dpy);
int init_keycodes_from_xkb(void* dpy, RdpScancodes x_keycode_to_rdp_scancode, uint8 rdp_scancode_to_x_keycode[256][2]);

#else

void load_keyboard_map(KeycodeToVkcode keycodeToVkcode, char *xkbfile);

#endif

#endif
