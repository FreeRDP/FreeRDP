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

/* Hardcoded mapping from xkb layout names and variants to RDP layout ids */

#ifndef __LAYOUTS_X_H
#define __LAYOUTS_X_H

unsigned int find_keyboard_layout_in_xorg_rules(char* layout, char* variant);

#if defined(sun)
unsigned int detect_keyboard_type_and_layout_sunos(char* xkbfile, int length);
#endif

#endif
