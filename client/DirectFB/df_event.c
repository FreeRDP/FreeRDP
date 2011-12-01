/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Event Handling
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

#include <freerdp/kbd/kbd.h>
#include <freerdp/kbd/vkcodes.h>

#include "df_event.h"

static uint8 keymap[256];
static uint8 functionmap[128];

void df_keyboard_init()
{
	memset(keymap, 0, sizeof(keymap));

	/* Map DirectFB keycodes to Virtual Key Codes */

	keymap[DIKI_A - DIKI_UNKNOWN] = VK_KEY_A;
	keymap[DIKI_B - DIKI_UNKNOWN] = VK_KEY_B;
	keymap[DIKI_C - DIKI_UNKNOWN] = VK_KEY_C;
	keymap[DIKI_D - DIKI_UNKNOWN] = VK_KEY_D;
	keymap[DIKI_E - DIKI_UNKNOWN] = VK_KEY_E;
	keymap[DIKI_F - DIKI_UNKNOWN] = VK_KEY_F;
	keymap[DIKI_G - DIKI_UNKNOWN] = VK_KEY_G;
	keymap[DIKI_H - DIKI_UNKNOWN] = VK_KEY_H;
	keymap[DIKI_I - DIKI_UNKNOWN] = VK_KEY_I;
	keymap[DIKI_J - DIKI_UNKNOWN] = VK_KEY_J;
	keymap[DIKI_K - DIKI_UNKNOWN] = VK_KEY_K;
	keymap[DIKI_L - DIKI_UNKNOWN] = VK_KEY_L;
	keymap[DIKI_M - DIKI_UNKNOWN] = VK_KEY_M;
	keymap[DIKI_N - DIKI_UNKNOWN] = VK_KEY_N;
	keymap[DIKI_O - DIKI_UNKNOWN] = VK_KEY_O;
	keymap[DIKI_P - DIKI_UNKNOWN] = VK_KEY_P;
	keymap[DIKI_Q - DIKI_UNKNOWN] = VK_KEY_Q;
	keymap[DIKI_R - DIKI_UNKNOWN] = VK_KEY_R;
	keymap[DIKI_S - DIKI_UNKNOWN] = VK_KEY_S;
	keymap[DIKI_T - DIKI_UNKNOWN] = VK_KEY_T;
	keymap[DIKI_U - DIKI_UNKNOWN] = VK_KEY_U;
	keymap[DIKI_V - DIKI_UNKNOWN] = VK_KEY_V;
	keymap[DIKI_W - DIKI_UNKNOWN] = VK_KEY_W;
	keymap[DIKI_X - DIKI_UNKNOWN] = VK_KEY_X;
	keymap[DIKI_Y - DIKI_UNKNOWN] = VK_KEY_Y;
	keymap[DIKI_Z - DIKI_UNKNOWN] = VK_KEY_Z;

	keymap[DIKI_0 - DIKI_UNKNOWN] = VK_KEY_0;
	keymap[DIKI_1 - DIKI_UNKNOWN] = VK_KEY_1;
	keymap[DIKI_2 - DIKI_UNKNOWN] = VK_KEY_2;
	keymap[DIKI_3 - DIKI_UNKNOWN] = VK_KEY_3;
	keymap[DIKI_4 - DIKI_UNKNOWN] = VK_KEY_4;
	keymap[DIKI_5 - DIKI_UNKNOWN] = VK_KEY_5;
	keymap[DIKI_6 - DIKI_UNKNOWN] = VK_KEY_6;
	keymap[DIKI_7 - DIKI_UNKNOWN] = VK_KEY_7;
	keymap[DIKI_8 - DIKI_UNKNOWN] = VK_KEY_8;
	keymap[DIKI_9 - DIKI_UNKNOWN] = VK_KEY_9;

	keymap[DIKI_F1 - DIKI_UNKNOWN] = VK_F1;
	keymap[DIKI_F2 - DIKI_UNKNOWN] = VK_F2;
	keymap[DIKI_F3 - DIKI_UNKNOWN] = VK_F3;
	keymap[DIKI_F4 - DIKI_UNKNOWN] = VK_F4;
	keymap[DIKI_F5 - DIKI_UNKNOWN] = VK_F5;
	keymap[DIKI_F6 - DIKI_UNKNOWN] = VK_F6;
	keymap[DIKI_F7 - DIKI_UNKNOWN] = VK_F7;
	keymap[DIKI_F8 - DIKI_UNKNOWN] = VK_F8;
	keymap[DIKI_F9 - DIKI_UNKNOWN] = VK_F9;
	keymap[DIKI_F10 - DIKI_UNKNOWN] = VK_F10;
	keymap[DIKI_F11 - DIKI_UNKNOWN] = VK_F11;
	keymap[DIKI_F12 - DIKI_UNKNOWN] = VK_F12;

	keymap[DIKI_COMMA - DIKI_UNKNOWN] = VK_OEM_COMMA;
	keymap[DIKI_PERIOD - DIKI_UNKNOWN] = VK_OEM_PERIOD;
	keymap[DIKI_MINUS_SIGN - DIKI_UNKNOWN] = VK_OEM_MINUS;
	keymap[DIKI_EQUALS_SIGN - DIKI_UNKNOWN] = VK_OEM_PLUS;

	keymap[DIKI_ESCAPE - DIKI_UNKNOWN] = VK_ESCAPE;
	keymap[DIKI_LEFT - DIKI_UNKNOWN] = VK_LEFT;
	keymap[DIKI_RIGHT - DIKI_UNKNOWN] = VK_RIGHT;
	keymap[DIKI_UP - DIKI_UNKNOWN] = VK_UP;
	keymap[DIKI_DOWN - DIKI_UNKNOWN] = VK_DOWN;
	keymap[DIKI_CONTROL_L - DIKI_UNKNOWN] = VK_LCONTROL;
	keymap[DIKI_CONTROL_R - DIKI_UNKNOWN] = VK_RCONTROL;
	keymap[DIKI_SHIFT_L - DIKI_UNKNOWN] = VK_LSHIFT;
	keymap[DIKI_SHIFT_R - DIKI_UNKNOWN] = VK_RSHIFT;
	keymap[DIKI_ALT_L - DIKI_UNKNOWN] = VK_LMENU;
	keymap[DIKI_ALT_R - DIKI_UNKNOWN] = VK_RMENU;
	keymap[DIKI_TAB - DIKI_UNKNOWN] = VK_TAB;
	keymap[DIKI_ENTER - DIKI_UNKNOWN] = VK_RETURN;
	keymap[DIKI_SPACE - DIKI_UNKNOWN] = VK_SPACE;
	keymap[DIKI_BACKSPACE - DIKI_UNKNOWN] = VK_BACK;
	keymap[DIKI_INSERT - DIKI_UNKNOWN] = VK_INSERT;
	keymap[DIKI_DELETE - DIKI_UNKNOWN] = VK_DELETE;
	keymap[DIKI_HOME - DIKI_UNKNOWN] = VK_HOME;
	keymap[DIKI_END - DIKI_UNKNOWN] = VK_END;
	keymap[DIKI_PAGE_UP - DIKI_UNKNOWN] = VK_PRIOR;
	keymap[DIKI_PAGE_DOWN - DIKI_UNKNOWN] = VK_NEXT;
	keymap[DIKI_CAPS_LOCK - DIKI_UNKNOWN] = VK_CAPITAL;
	keymap[DIKI_NUM_LOCK - DIKI_UNKNOWN] = VK_NUMLOCK;
	keymap[DIKI_SCROLL_LOCK - DIKI_UNKNOWN] = VK_SCROLL;
	keymap[DIKI_PRINT - DIKI_UNKNOWN] = VK_PRINT;
	keymap[DIKI_PAUSE - DIKI_UNKNOWN] = VK_PAUSE;
	keymap[DIKI_KP_DIV - DIKI_UNKNOWN] = VK_DIVIDE;
	keymap[DIKI_KP_MULT - DIKI_UNKNOWN] = VK_MULTIPLY;
	keymap[DIKI_KP_MINUS - DIKI_UNKNOWN] = VK_SUBTRACT;
	keymap[DIKI_KP_PLUS - DIKI_UNKNOWN] = VK_ADD;
	keymap[DIKI_KP_ENTER - DIKI_UNKNOWN] = VK_RETURN;
	keymap[DIKI_KP_DECIMAL - DIKI_UNKNOWN] = VK_DECIMAL;
	
	keymap[DIKI_QUOTE_LEFT - DIKI_UNKNOWN] = VK_OEM_3;
	keymap[DIKI_BRACKET_LEFT - DIKI_UNKNOWN] = VK_OEM_4;
	keymap[DIKI_BRACKET_RIGHT - DIKI_UNKNOWN] = VK_OEM_6;
	keymap[DIKI_BACKSLASH - DIKI_UNKNOWN] = VK_OEM_5;
	keymap[DIKI_SEMICOLON - DIKI_UNKNOWN] = VK_OEM_1;
	keymap[DIKI_QUOTE_RIGHT - DIKI_UNKNOWN] = VK_OEM_7;
	keymap[DIKI_COMMA - DIKI_UNKNOWN] = VK_OEM_COMMA;
	keymap[DIKI_PERIOD - DIKI_UNKNOWN] = VK_OEM_PERIOD;
	keymap[DIKI_SLASH - DIKI_UNKNOWN] = VK_OEM_2;

	keymap[DIKI_LESS_SIGN - DIKI_UNKNOWN] = 0;

	keymap[DIKI_KP_0 - DIKI_UNKNOWN] = VK_NUMPAD0;
	keymap[DIKI_KP_1 - DIKI_UNKNOWN] = VK_NUMPAD1;
	keymap[DIKI_KP_2 - DIKI_UNKNOWN] = VK_NUMPAD2;
	keymap[DIKI_KP_3 - DIKI_UNKNOWN] = VK_NUMPAD3;
	keymap[DIKI_KP_4 - DIKI_UNKNOWN] = VK_NUMPAD4;
	keymap[DIKI_KP_5 - DIKI_UNKNOWN] = VK_NUMPAD5;
	keymap[DIKI_KP_6 - DIKI_UNKNOWN] = VK_NUMPAD6;
	keymap[DIKI_KP_7 - DIKI_UNKNOWN] = VK_NUMPAD7;
	keymap[DIKI_KP_8 - DIKI_UNKNOWN] = VK_NUMPAD8;
	keymap[DIKI_KP_9 - DIKI_UNKNOWN] = VK_NUMPAD9;

	keymap[DIKI_META_L - DIKI_UNKNOWN] = VK_LWIN;
	keymap[DIKI_META_R - DIKI_UNKNOWN] = VK_RWIN;
	keymap[DIKI_SUPER_L - DIKI_UNKNOWN] = VK_APPS;
	
	
	memset(functionmap, 0, sizeof(functionmap));
	
	functionmap[DFB_FUNCTION_KEY(23) - DFB_FUNCTION_KEY(0)] = VK_HANGUL;
	functionmap[DFB_FUNCTION_KEY(24) - DFB_FUNCTION_KEY(0)] = VK_HANJA;

}

void df_send_mouse_button_event(rdpInput* input, boolean down, uint32 button, uint16 x, uint16 y)
{
	uint16 flags;

	flags = (down) ? PTR_FLAGS_DOWN : 0;

	if (button == DIBI_LEFT)
		flags |= PTR_FLAGS_BUTTON1;
	else if (button == DIBI_RIGHT)
		flags |= PTR_FLAGS_BUTTON2;
	else if (button == DIBI_MIDDLE)
		flags |= PTR_FLAGS_BUTTON3;

	if (flags != 0)
		input->MouseEvent(input, flags, x, y);
}

void df_send_mouse_motion_event(rdpInput* input, uint16 x, uint16 y)
{
	input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
}

void df_send_mouse_wheel_event(rdpInput* input, sint16 axisrel, uint16 x, uint16 y)
{
	uint16 flags = PTR_FLAGS_WHEEL;

	if (axisrel < 0)
		flags |= 0x0078;
	else
		flags |= PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;

	input->MouseEvent(input, flags, x, y);
}

void df_send_keyboard_event(rdpInput* input, boolean down, uint8 keycode, uint8 function)
{
	uint16 flags;
	uint8 vkcode;
	uint8 scancode;
	boolean extended;
	
	if (keycode)
		vkcode = keymap[keycode];
	else if (function)
		vkcode = functionmap[function];
	else
		return;
	
	scancode = freerdp_kbd_get_scancode_by_virtualkey(vkcode, &extended);

	flags = (extended) ? KBD_FLAGS_EXTENDED : 0;
	flags |= (down) ? KBD_FLAGS_DOWN : KBD_FLAGS_RELEASE;

	input->KeyboardEvent(input, flags, scancode);
}

boolean df_event_process(freerdp* instance, DFBEvent* event)
{
	int flags;
	rdpGdi* gdi;
	dfInfo* dfi;
	int pointer_x;
	int pointer_y;
	DFBInputEvent* input_event;

	gdi = instance->context->gdi;
	dfi = ((dfContext*) instance->context)->dfi;

	dfi->layer->GetCursorPosition(dfi->layer, &pointer_x, &pointer_y);

	if (event->clazz == DFEC_INPUT)
	{
		flags = 0;
		input_event = (DFBInputEvent*) event;

		switch (input_event->type)
		{
			case DIET_AXISMOTION:

				if (pointer_x > (gdi->width - 1))
					pointer_x = gdi->width - 1;

				if (pointer_y > (gdi->height - 1))
					pointer_y = gdi->height - 1;

				if (input_event->axis == DIAI_Z)
				{
					df_send_mouse_wheel_event(instance->input, input_event->axisrel, pointer_x, pointer_y);
				}
				else
				{
					df_send_mouse_motion_event(instance->input, pointer_x, pointer_y);
				}
				break;

			case DIET_BUTTONPRESS:
				df_send_mouse_button_event(instance->input, true, input_event->button, pointer_x, pointer_y);
				break;

			case DIET_BUTTONRELEASE:
				df_send_mouse_button_event(instance->input, false, input_event->button, pointer_x, pointer_y);
				break;

			case DIET_KEYPRESS:
				df_send_keyboard_event(instance->input, true, input_event->key_id - DIKI_UNKNOWN, input_event->key_symbol - DFB_FUNCTION_KEY(0));
				break;

			case DIET_KEYRELEASE:
				df_send_keyboard_event(instance->input, false, input_event->key_id - DIKI_UNKNOWN, input_event->key_symbol - DFB_FUNCTION_KEY(0));
				break;

			case DIET_UNKNOWN:
				break;
		}
	}

	return true;
}
