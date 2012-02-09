/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Server Input
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

#ifndef __XF_INPUT_H
#define __XF_INPUT_H

#include <pthread.h>

#include "xfreerdp.h"

void xf_input_synchronize_event(rdpInput* input, uint32 flags);
void xf_input_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void xf_input_unicode_keyboard_event(rdpInput* input, uint16 flags, uint16 code);
void xf_input_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);
void xf_input_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y);
void xf_input_register_callbacks(rdpInput* input);

#endif /* __XF_INPUT_H */
