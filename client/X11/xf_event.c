/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Event Handling
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

#include "keyboard.h"
#include <freerdp/kbd.h>

#include "xf_event.h"

void xf_keyboard_init()
{

}

void xf_send_mouse_event(rdpInput* input, boolean down, uint32 button, uint16 x, uint16 y)
{
	uint16 flags;

	flags = (down) ? PTR_FLAGS_DOWN : 0;

	if (flags != 0)
		input->MouseEvent(input, flags, x, y);
}

void xf_send_keyboard_event(rdpInput* input, boolean down, uint8 keycode)
{
	uint16 flags;
	uint8 scancode;
	boolean extended;

	input->KeyboardEvent(input, flags, scancode);
}

boolean xf_event_process(freerdp* instance, void* event)
{
	return True;
}
