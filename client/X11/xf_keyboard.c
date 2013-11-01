/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <winpr/crt.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <freerdp/locale/keyboard.h>

#include "xf_keyboard.h"

void xf_kbd_init(xfContext* xfc)
{
	xf_kbd_clear(xfc);

	xfc->keyboard_layout_id = xfc->instance->settings->KeyboardLayout;
	xfc->keyboard_layout_id = freerdp_keyboard_init(xfc->keyboard_layout_id);
	xfc->instance->settings->KeyboardLayout = xfc->keyboard_layout_id;

	if (xfc->modifier_map)
		XFreeModifiermap(xfc->modifier_map);

	xfc->modifier_map = XGetModifierMapping(xfc->display);
}

void xf_kbd_clear(xfContext* xfc)
{
	ZeroMemory(xfc->pressed_keys, 256 * sizeof(BOOL));
}

void xf_kbd_set_keypress(xfContext* xfc, BYTE keycode, KeySym keysym)
{
	if (keycode >= 8)
		xfc->pressed_keys[keycode] = keysym;
	else
		return;
}

void xf_kbd_unset_keypress(xfContext* xfc, BYTE keycode)
{
	if (keycode >= 8)
		xfc->pressed_keys[keycode] = NoSymbol;
	else
		return;
}

void xf_kbd_release_all_keypress(xfContext* xfc)
{
	int keycode;
	DWORD rdp_scancode;

	for (keycode = 0; keycode < ARRAYSIZE(xfc->pressed_keys); keycode++)
	{
		if (xfc->pressed_keys[keycode] != NoSymbol)
		{
			rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);
			freerdp_input_send_keyboard_event_ex(xfc->instance->input, FALSE, rdp_scancode);
			xfc->pressed_keys[keycode] = NoSymbol;
		}
	}
}

BOOL xf_kbd_key_pressed(xfContext* xfc, KeySym keysym)
{
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);
	return (xfc->pressed_keys[keycode] == keysym);
}

void xf_kbd_send_key(xfContext* xfc, BOOL down, BYTE keycode)
{
	DWORD rdp_scancode;
	rdpInput* input;

	input = xfc->instance->input;
	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
	{
		fprintf(stderr, "Unknown key with X keycode 0x%02x\n", keycode);
	}
	else if (rdp_scancode == RDP_SCANCODE_PAUSE &&
			!xf_kbd_key_pressed(xfc, XK_Control_L) && !xf_kbd_key_pressed(xfc, XK_Control_R))
	{
		/* Pause without Ctrl has to be sent as Ctrl + NumLock. */
		if (down)
		{
			freerdp_input_send_keyboard_event_ex(input, TRUE, RDP_SCANCODE_LCONTROL);
			freerdp_input_send_keyboard_event_ex(input, TRUE, RDP_SCANCODE_NUMLOCK);
			freerdp_input_send_keyboard_event_ex(input, FALSE, RDP_SCANCODE_LCONTROL);
			freerdp_input_send_keyboard_event_ex(input, FALSE, RDP_SCANCODE_NUMLOCK);
		}
	}
	else
	{
		freerdp_input_send_keyboard_event_ex(input, down, rdp_scancode);

		if ((rdp_scancode == RDP_SCANCODE_CAPSLOCK) && (down == FALSE))
		{
			UINT32 syncFlags;
			syncFlags = xf_kbd_get_toggle_keys_state(xfc);
			input->SynchronizeEvent(input, syncFlags);
		}
	}
}

int xf_kbd_read_keyboard_state(xfContext* xfc)
{
	int dummy;
	Window wdummy;
	UINT32 state = 0;

	if (!xfc->remote_app)
	{
		XQueryPointer(xfc->display, xfc->window->handle,
			&wdummy, &wdummy, &dummy, &dummy, &dummy, &dummy, &state);
	}
	else
	{
		XQueryPointer(xfc->display, DefaultRootWindow(xfc->display),
			&wdummy, &wdummy, &dummy, &dummy, &dummy, &dummy, &state);
  	}
	return state;
}

BOOL xf_kbd_get_key_state(xfContext* xfc, int state, int keysym)
{
	int offset;
	int modifierpos, key, keysymMask = 0;
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);

	if (keycode == NoSymbol)
		return FALSE;

	for (modifierpos = 0; modifierpos < 8; modifierpos++)
	{
		offset = xfc->modifier_map->max_keypermod * modifierpos;

		for (key = 0; key < xfc->modifier_map->max_keypermod; key++)
		{
			if (xfc->modifier_map->modifiermap[offset + key] == keycode)
			{
				keysymMask |= 1 << modifierpos;
			}
		}
	}

	return (state & keysymMask) ? TRUE : FALSE;
}

int xf_kbd_get_toggle_keys_state(xfContext* xfc)
{
	int state;
	int toggle_keys_state = 0;

	state = xf_kbd_read_keyboard_state(xfc);

	if (xf_kbd_get_key_state(xfc, state, XK_Scroll_Lock))
		toggle_keys_state |= KBD_SYNC_SCROLL_LOCK;
	if (xf_kbd_get_key_state(xfc, state, XK_Num_Lock))
		toggle_keys_state |= KBD_SYNC_NUM_LOCK;
	if (xf_kbd_get_key_state(xfc, state, XK_Caps_Lock))
		toggle_keys_state |= KBD_SYNC_CAPS_LOCK;
	if (xf_kbd_get_key_state(xfc, state, XK_Kana_Lock))
		toggle_keys_state |= KBD_SYNC_KANA_LOCK;

	return toggle_keys_state;
}

void xf_kbd_focus_in(xfContext* xfc)
{
	rdpInput* input;
	UINT32 syncFlags;
	int dummy, mouseX, mouseY;
	Window wdummy;
	UINT32 state = 0;

    if (xfc->display && xfc->window)
    {
	    input = xfc->instance->input;
	    syncFlags = xf_kbd_get_toggle_keys_state(xfc);
	    XQueryPointer(xfc->display, xfc->window->handle, &wdummy, &wdummy, &mouseX, &mouseY, &dummy, &dummy, &state);
	    input->FocusInEvent(input, syncFlags, mouseX, mouseY);
	}
}

BOOL xf_kbd_handle_special_keys(xfContext* xfc, KeySym keysym)
{
	if (keysym == XK_Return)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L) || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L) || xf_kbd_key_pressed(xfc, XK_Control_R)))
		{
			/* Ctrl-Alt-Enter: toggle full screen */
			xf_toggle_fullscreen(xfc);
			return TRUE;
		}
	}
	
	if (keysym == XK_period)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			//Zoom in (scale larger)
			double s = xfc->settings->ScalingFactor;
			s += 0.1;
			if (s > 2.0)
				s = 2.0;
			
			xfc->settings->ScalingFactor = s;
			
			xfc->currentWidth = xfc->originalWidth * s;
			xfc->currentHeight = xfc->originalHeight * s;
			
			xf_transform_window(xfc);
			
			{
				ResizeWindowEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.width = (int) xfc->originalWidth * xfc->settings->ScalingFactor;
				e.height = (int) xfc->originalHeight * xfc->settings->ScalingFactor;
				PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			xf_draw_screen_scaled(xfc, 0, 0, 0, 0, FALSE);
			return TRUE;
		}
	}
	
	if (keysym == XK_comma)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			//Zoom out (scale smaller)
			double s = xfc->settings->ScalingFactor;
			s -= 0.1;
			if (s < 0.5)
				s = 0.5;
			
			xfc->settings->ScalingFactor = s;
			
			xfc->currentWidth = xfc->originalWidth * s;
			xfc->currentHeight = xfc->originalHeight * s;
			
			xf_transform_window(xfc);
			
			{
				ResizeWindowEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.width = (int) xfc->originalWidth * xfc->settings->ScalingFactor;
				e.height = (int) xfc->originalHeight * xfc->settings->ScalingFactor;
				PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			xf_draw_screen_scaled(xfc, 0, 0, 0, 0, FALSE);
			return TRUE;
		}
	}
	
	if (keysym == XK_KP_4)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = -5;
				e.YPan = 0;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			
			return TRUE;
		}
	}
	
	if (keysym == XK_KP_6)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 5;
				e.YPan = 0;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			return TRUE;
		}
	}
	
	if (keysym == XK_KP_8)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 0;
				e.YPan = -5;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			return TRUE;
		}
	}
	
	if (keysym == XK_KP_2)
	{
		if ((xf_kbd_key_pressed(xfc, XK_Alt_L)
		     || xf_kbd_key_pressed(xfc, XK_Alt_R))
		    && (xf_kbd_key_pressed(xfc, XK_Control_L)
			|| xf_kbd_key_pressed(xfc,
					      XK_Control_R)))
		{
			{
				PanningChangeEventArgs e;
				
				EventArgsInit(&e, "xfreerdp");
				e.XPan = 0;
				e.YPan = 5;
				PubSub_OnPanningChange(((rdpContext*) xfc)->pubSub, xfc, &e);
			}
			return TRUE;
		}
	}
	
	
	return FALSE;
}

