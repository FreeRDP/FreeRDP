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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "xf_keyboard.h"

void xf_kbd_init(xfInfo* xfi)
{
	xf_kbd_clear(xfi);
	xfi->keyboard_layout_id = xfi->instance->settings->kbd_layout;
	xfi->keyboard_layout_id = freerdp_keyboard_init(xfi->keyboard_layout_id);
	xfi->instance->settings->kbd_layout = xfi->keyboard_layout_id;
}

void xf_kbd_clear(xfInfo* xfi)
{
	memset(xfi->pressed_keys, 0, 256 * sizeof(boolean));
}

void xf_kbd_set_keypress(xfInfo* xfi, uint8 keycode, KeySym keysym)
{
	if (keycode >= 8)
		xfi->pressed_keys[keycode] = keysym;
	else
		return;
}

void xf_kbd_unset_keypress(xfInfo* xfi, uint8 keycode)
{
	if (keycode >= 8)
		xfi->pressed_keys[keycode] = NoSymbol;
	else
		return;
}

void xf_kbd_release_all_keypress(xfInfo* xfi)
{
	int keycode;
	RDP_SCANCODE rdp_scancode;

	for (keycode = 0; keycode < ARRAY_SIZE(xfi->pressed_keys); keycode++)
	{
		if (xfi->pressed_keys[keycode] != NoSymbol)
		{
			rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);
			freerdp_input_send_keyboard_event_2(xfi->instance->input, false, rdp_scancode);
			xfi->pressed_keys[keycode] = NoSymbol;
		}
	}
}

boolean xf_kbd_key_pressed(xfInfo* xfi, KeySym keysym)
{
	KeyCode keycode = XKeysymToKeycode(xfi->display, keysym);
	return (xfi->pressed_keys[keycode] == keysym);
}

void xf_kbd_send_key(xfInfo* xfi, boolean down, uint8 keycode)
{
	RDP_SCANCODE rdp_scancode;
	rdpInput* input;

	input = xfi->instance->input;
	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
	{
		printf("Unknown key with X keycode 0x%02x\n", keycode);
	}
	else if (rdp_scancode == RDP_SCANCODE_PAUSE &&
			!xf_kbd_key_pressed(xfi, XK_Control_L) && !xf_kbd_key_pressed(xfi, XK_Control_R))
	{
		/* Pause without Ctrl has to be sent as Ctrl + NumLock. */
		if (down)
		{
			freerdp_input_send_keyboard_event_2(input, true, RDP_SCANCODE_LCONTROL);
			freerdp_input_send_keyboard_event_2(input, true, RDP_SCANCODE_NUMLOCK);
			freerdp_input_send_keyboard_event_2(input, false, RDP_SCANCODE_LCONTROL);
			freerdp_input_send_keyboard_event_2(input, false, RDP_SCANCODE_NUMLOCK);
		}
	}
	else
	{
		freerdp_input_send_keyboard_event_2(input, down, rdp_scancode);

		if ((rdp_scancode == RDP_SCANCODE_CAPSLOCK) && (down == false))
		{
			uint32 syncFlags;
			syncFlags = xf_kbd_get_toggle_keys_state(xfi);
			input->SynchronizeEvent(input, syncFlags);
		}
	}
}

int xf_kbd_read_keyboard_state(xfInfo* xfi)
{
	int dummy;
	Window wdummy;
	uint32 state = 0;

	if (xfi->remote_app != true)
	{
		XQueryPointer(xfi->display, xfi->window->handle,
			&wdummy, &wdummy, &dummy, &dummy, &dummy, &dummy, &state);
	}
	else
	{
		XQueryPointer(xfi->display, DefaultRootWindow(xfi->display),
			&wdummy, &wdummy, &dummy, &dummy, &dummy, &dummy, &state);
  	}
	return state;
}

boolean xf_kbd_get_key_state(xfInfo* xfi, int state, int keysym)
{
	int offset;
	int modifierpos, key, keysymMask = 0;
	KeyCode keycode = XKeysymToKeycode(xfi->display, keysym);

	if (keycode == NoSymbol)
		return false;

	for (modifierpos = 0; modifierpos < 8; modifierpos++)
	{
		offset = xfi->modifier_map->max_keypermod * modifierpos;
		for (key = 0; key < xfi->modifier_map->max_keypermod; key++)
		{
			if (xfi->modifier_map->modifiermap[offset + key] == keycode)
			{
				keysymMask |= 1 << modifierpos;
			}
		}
	}

	return (state & keysymMask) ? true : false;
}

int xf_kbd_get_toggle_keys_state(xfInfo* xfi)
{
	int state;
	int toggle_keys_state = 0;

	state = xf_kbd_read_keyboard_state(xfi);
	if (xf_kbd_get_key_state(xfi, state, XK_Scroll_Lock))
		toggle_keys_state |= KBD_SYNC_SCROLL_LOCK;
	if (xf_kbd_get_key_state(xfi, state, XK_Num_Lock))
		toggle_keys_state |= KBD_SYNC_NUM_LOCK;
	if (xf_kbd_get_key_state(xfi, state, XK_Caps_Lock))
		toggle_keys_state |= KBD_SYNC_CAPS_LOCK;
	if (xf_kbd_get_key_state(xfi, state, XK_Kana_Lock))
		toggle_keys_state |= KBD_SYNC_KANA_LOCK;

	return toggle_keys_state;
}

void xf_kbd_focus_in(xfInfo* xfi)
{
	rdpInput* input;
	uint32 syncFlags;

	input = xfi->instance->input;

	/* on focus in send a tab up like mstsc.exe */
	input->KeyboardEvent(input, KBD_FLAGS_RELEASE, 0x0F);

	/* synchronize toggle keys */
	syncFlags = xf_kbd_get_toggle_keys_state(xfi);
	input->SynchronizeEvent(input, syncFlags);
}

boolean xf_kbd_handle_special_keys(xfInfo* xfi, KeySym keysym)
{
	if (keysym == XK_Return)
	{
		if ((xf_kbd_key_pressed(xfi, XK_Alt_L) || xf_kbd_key_pressed(xfi, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfi, XK_Control_L) || xf_kbd_key_pressed(xfi, XK_Control_R)))
		{
			/* Ctrl-Alt-Enter: toggle full screen */
			xf_toggle_fullscreen(xfi);
			return true;
		}
	}

	return false;
}

