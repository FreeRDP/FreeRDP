/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Keyboard layout ID detection from common X11 xkb keyboard layout names
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

#ifndef __FREERDP_XKB_LAYOUT_IDS_H
#define __FREERDP_XKB_LAYOUT_IDS_H

#include <freerdp/types.h>

UINT32 find_keyboard_layout_in_xorg_rules(char* layout, char* variant);

#endif /* __FREERDP_XKB_LAYOUT_IDS_H */
