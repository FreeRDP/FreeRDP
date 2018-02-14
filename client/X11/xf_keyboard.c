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
#include <winpr/path.h>
#include <winpr/collections.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <freerdp/locale/keyboard.h>

#include "xf_event.h"

#include "xf_keyboard.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static BOOL firstPressRightCtrl = TRUE;
static BOOL ungrabKeyboardWithRightCtrl = TRUE;

static BOOL xf_keyboard_action_script_init(xfContext* xfc)
{
	FILE* keyScript;
	char* keyCombination;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };
	xfc->actionScriptExists = PathFileExistsA(xfc->context.settings->ActionScript);

	if (!xfc->actionScriptExists)
		return FALSE;

	xfc->keyCombinations = ArrayList_New(TRUE);

	if (!xfc->keyCombinations)
		return FALSE;

	ArrayList_Object(xfc->keyCombinations)->fnObjectFree = free;
	sprintf_s(command, sizeof(command), "%s key", xfc->context.settings->ActionScript);
	keyScript = popen(command, "r");

	if (!keyScript)
	{
		xfc->actionScriptExists = FALSE;
		return FALSE;
	}

	while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
	{
		strtok(buffer, "\n");
		keyCombination = _strdup(buffer);

		if (!keyCombination || ArrayList_Add(xfc->keyCombinations, keyCombination) < 0)
		{
			ArrayList_Free(xfc->keyCombinations);
			xfc->actionScriptExists = FALSE;
			pclose(keyScript);
			return FALSE;
		}
	}

	pclose(keyScript);
	return xf_event_action_script_init(xfc);
}

static void xf_keyboard_action_script_free(xfContext* xfc)
{
	xf_event_action_script_free(xfc);

	if (xfc->keyCombinations)
	{
		ArrayList_Free(xfc->keyCombinations);
		xfc->keyCombinations = NULL;
		xfc->actionScriptExists = FALSE;
	}
}

BOOL xf_keyboard_init(xfContext* xfc)
{
	xf_keyboard_clear(xfc);
	xfc->KeyboardLayout = xfc->context.settings->KeyboardLayout;
	xfc->KeyboardLayout = freerdp_keyboard_init(xfc->KeyboardLayout);
	xfc->context.settings->KeyboardLayout = xfc->KeyboardLayout;

	if (xfc->modifierMap)
		XFreeModifiermap(xfc->modifierMap);

	if (!(xfc->modifierMap = XGetModifierMapping(xfc->display)))
		return FALSE;

	xf_keyboard_action_script_init(xfc);
	return TRUE;
}

void xf_keyboard_free(xfContext* xfc)
{
	if (xfc->modifierMap)
	{
		XFreeModifiermap(xfc->modifierMap);
		xfc->modifierMap = NULL;
	}

	xf_keyboard_action_script_free(xfc);
}

void xf_keyboard_clear(xfContext* xfc)
{
	ZeroMemory(xfc->KeyboardState, 256 * sizeof(BOOL));
}

void xf_keyboard_key_press(xfContext* xfc, BYTE keycode, KeySym keysym)
{
	if (keycode < 8)
		return;

	xfc->KeyboardState[keycode] = TRUE;

	if (xf_keyboard_handle_special_keys(xfc, keysym))
		return;

	xf_keyboard_send_key(xfc, TRUE, keycode);
}

void xf_keyboard_key_release(xfContext* xfc, BYTE keycode, KeySym keysym)
{
	if (keycode < 8)
		return;

	xfc->KeyboardState[keycode] = FALSE;
	xf_keyboard_handle_special_keys_release(xfc, keysym);
	xf_keyboard_send_key(xfc, FALSE, keycode);
}

void xf_keyboard_release_all_keypress(xfContext* xfc)
{
	int keycode;
	DWORD rdp_scancode;

	for (keycode = 0; keycode < ARRAYSIZE(xfc->KeyboardState); keycode++)
	{
		if (xfc->KeyboardState[keycode])
		{
			rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);

			// release tab before releasing the windows key.
			// this stops the start menu from opening on unfocus event.
			if (rdp_scancode == RDP_SCANCODE_LWIN)
				freerdp_input_send_keyboard_event_ex(xfc->context.input, FALSE,
				                                     RDP_SCANCODE_TAB);

			freerdp_input_send_keyboard_event_ex(xfc->context.input, FALSE, rdp_scancode);
			xfc->KeyboardState[keycode] = FALSE;
		}
	}
}

BOOL xf_keyboard_key_pressed(xfContext* xfc, KeySym keysym)
{
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);
	return xfc->KeyboardState[keycode];
}

void xf_keyboard_send_key(xfContext* xfc, BOOL down, BYTE keycode)
{
	DWORD rdp_scancode;
	rdpInput* input;
	input = xfc->context.input;
	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);

	if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
	{
		WLog_ERR(TAG,  "Unknown key with X keycode 0x%02"PRIx8"", keycode);
	}
	else if (rdp_scancode == RDP_SCANCODE_PAUSE &&
	         !xf_keyboard_key_pressed(xfc, XK_Control_L)
	         && !xf_keyboard_key_pressed(xfc, XK_Control_R))
	{
		/* Pause without Ctrl has to be sent as a series of keycodes
		 * in a single input PDU.  Pause only happens on "press";
		 * no code is sent on "release".
		 */
		if (down)
		{
			freerdp_input_send_keyboard_pause_event(input);
		}
	}
	else
	{
		freerdp_input_send_keyboard_event_ex(input, down, rdp_scancode);

		if ((rdp_scancode == RDP_SCANCODE_CAPSLOCK) && (down == FALSE))
		{
			UINT32 syncFlags;
			syncFlags = xf_keyboard_get_toggle_keys_state(xfc);
			input->SynchronizeEvent(input, syncFlags);
		}
	}
}

int xf_keyboard_read_keyboard_state(xfContext* xfc)
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

static int xf_keyboard_get_keymask(xfContext* xfc, int keysym)
{
	int modifierpos, key, keysymMask = 0;
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);

	if (keycode == NoSymbol)
		return 0;

	for (modifierpos = 0; modifierpos < 8; modifierpos++)
	{
		int offset = xfc->modifierMap->max_keypermod * modifierpos;

		for (key = 0; key < xfc->modifierMap->max_keypermod; key++)
		{
			if (xfc->modifierMap->modifiermap[offset + key] == keycode)
			{
				keysymMask |= 1 << modifierpos;
			}
		}
	}

	return keysymMask;
}

BOOL xf_keyboard_get_key_state(xfContext* xfc, int state, int keysym)
{
	int keysymMask = xf_keyboard_get_keymask(xfc, keysym);

	if (!keysymMask)
		return FALSE;

	return (state & keysymMask) ? TRUE : FALSE;
}

static BOOL xf_keyboard_set_key_state(xfContext* xfc, BOOL on, int keysym)
{
	int keysymMask;

	if (!xfc->xkbAvailable)
		return FALSE;

	keysymMask = xf_keyboard_get_keymask(xfc, keysym);

	if (!keysymMask)
	{
		return FALSE;
	}

	return XkbLockModifiers(xfc->display, XkbUseCoreKbd, keysymMask,
	                        on ? keysymMask : 0);
}

UINT32 xf_keyboard_get_toggle_keys_state(xfContext* xfc)
{
	int state;
	UINT32 toggleKeysState = 0;
	state = xf_keyboard_read_keyboard_state(xfc);

	if (xf_keyboard_get_key_state(xfc, state, XK_Scroll_Lock))
		toggleKeysState |= KBD_SYNC_SCROLL_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Num_Lock))
		toggleKeysState |= KBD_SYNC_NUM_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Caps_Lock))
		toggleKeysState |= KBD_SYNC_CAPS_LOCK;

	if (xf_keyboard_get_key_state(xfc, state, XK_Kana_Lock))
		toggleKeysState |= KBD_SYNC_KANA_LOCK;

	return toggleKeysState;
}

void xf_keyboard_focus_in(xfContext* xfc)
{
	rdpInput* input;
	UINT32 syncFlags, state;
	Window w;
	int d, x, y;

	if (!xfc->display || !xfc->window)
		return;

	input = xfc->context.input;
	syncFlags = xf_keyboard_get_toggle_keys_state(xfc);
	input->FocusInEvent(input, syncFlags);

	/* finish with a mouse pointer position like mstsc.exe if required */

	if (xfc->remote_app)
		return;

	if (XQueryPointer(xfc->display, xfc->window->handle, &w, &w, &d, &d, &x, &y,
	                  &state))
	{
		if (x >= 0 && x < xfc->window->width && y >= 0 && y < xfc->window->height)
		{
			xf_event_adjust_coordinates(xfc, &x, &y);
			input->MouseEvent(input, PTR_FLAGS_MOVE, x, y);
		}
	}
}

static int xf_keyboard_execute_action_script(xfContext* xfc,
        XF_MODIFIER_KEYS* mod,
        KeySym keysym)
{
	int index;
	int count;
	int status = 1;
	FILE* keyScript;
	const char* keyStr;
	BOOL match = FALSE;
	char* keyCombination;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };
	char combination[1024] = { 0 };

	if (!xfc->actionScriptExists)
		return 1;

	if ((keysym == XK_Shift_L) || (keysym == XK_Shift_R) ||
	    (keysym == XK_Alt_L) || (keysym == XK_Alt_R) ||
	    (keysym == XK_Control_L) || (keysym == XK_Control_R))
	{
		return 1;
	}

	keyStr = XKeysymToString(keysym);

	if (keyStr == 0)
	{
		return 1;
	}

	if (mod->Shift)
		strcat(combination, "Shift+");

	if (mod->Ctrl)
		strcat(combination, "Ctrl+");

	if (mod->Alt)
		strcat(combination, "Alt+");

	strcat(combination, keyStr);
	count = ArrayList_Count(xfc->keyCombinations);

	for (index = 0; index < count; index++)
	{
		keyCombination = (char*) ArrayList_GetItem(xfc->keyCombinations, index);

		if (_stricmp(keyCombination, combination) == 0)
		{
			match = TRUE;
			break;
		}
	}

	if (!match)
		return 1;

	sprintf_s(command, sizeof(command), "%s key %s",
	          xfc->context.settings->ActionScript, combination);
	keyScript = popen(command, "r");

	if (!keyScript)
		return -1;

	while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
	{
		strtok(buffer, "\n");

		if (strcmp(buffer, "key-local") == 0)
			status = 0;
	}

	if (pclose(keyScript) == -1)
		status = -1;

	return status;
}

static int xk_keyboard_get_modifier_keys(xfContext* xfc, XF_MODIFIER_KEYS* mod)
{
	mod->LeftShift = xf_keyboard_key_pressed(xfc, XK_Shift_L);
	mod->RightShift = xf_keyboard_key_pressed(xfc, XK_Shift_R);
	mod->Shift = mod->LeftShift || mod->RightShift;
	mod->LeftAlt = xf_keyboard_key_pressed(xfc, XK_Alt_L);
	mod->RightAlt = xf_keyboard_key_pressed(xfc, XK_Alt_R);
	mod->Alt = mod->LeftAlt || mod->RightAlt;
	mod->LeftCtrl = xf_keyboard_key_pressed(xfc, XK_Control_L);
	mod->RightCtrl = xf_keyboard_key_pressed(xfc, XK_Control_R);
	mod->Ctrl = mod->LeftCtrl || mod->RightCtrl;
	mod->LeftSuper = xf_keyboard_key_pressed(xfc, XK_Super_L);
	mod->RightSuper = xf_keyboard_key_pressed(xfc, XK_Super_R);
	mod->Super = mod->LeftSuper || mod->RightSuper;
	return 0;
}

BOOL xf_keyboard_handle_special_keys(xfContext* xfc, KeySym keysym)
{
	XF_MODIFIER_KEYS mod = { 0 };
	xk_keyboard_get_modifier_keys(xfc, &mod);
	rdpContext* ctx = &xfc->context;

	// remember state of RightCtrl to ungrab keyboard if next action is release of RightCtrl
	// do not return anything such that the key could be used by client if ungrab is not the goal
	if (keysym == XK_Control_R)
	{
		if (mod.RightCtrl && firstPressRightCtrl)
		{
			// Right Ctrl is pressed, getting ready to ungrab
			ungrabKeyboardWithRightCtrl = TRUE;
			firstPressRightCtrl = FALSE;
		}
	}
	else
	{
		// some other key has been pressed, abort ungrabbing
		if (ungrabKeyboardWithRightCtrl)
			ungrabKeyboardWithRightCtrl = FALSE;
	}

	if (!xf_keyboard_execute_action_script(xfc, &mod, keysym))
	{
		return TRUE;
	}

	if (!xfc->remote_app && xfc->fullscreen_toggle)
	{
		if (keysym == XK_Return)
		{
			if (mod.Ctrl && mod.Alt)
			{
				/* Ctrl-Alt-Enter: toggle full screen */
				xf_toggle_fullscreen(xfc);
				return TRUE;
			}
		}
	}

	if ((keysym == XK_c) || (keysym == XK_C))
	{
		if (mod.Ctrl && mod.Alt)
		{
			/* Ctrl-Alt-C: toggle control */
			xf_toggle_control(xfc);
			return TRUE;
		}
	}

#if 0 /* set to 1 to enable multi touch gesture simulation via keyboard */
#ifdef WITH_XRENDER

	if (!xfc->remote_app && xfc->settings->MultiTouchGestures)
	{
		if (mod.Ctrl && mod.Alt)
		{
			int pdx = 0;
			int pdy = 0;
			int zdx = 0;
			int zdy = 0;

			switch (keysym)
			{
				case XK_0:	/* Ctrl-Alt-0: Reset scaling and panning */
					xfc->scaledWidth = xfc->sessionWidth;
					xfc->scaledHeight = xfc->sessionHeight;
					xfc->offset_x = 0;
					xfc->offset_y = 0;

					if (!xfc->fullscreen && (xfc->sessionWidth != xfc->window->width ||
					                         xfc->sessionHeight != xfc->window->height))
					{
						xf_ResizeDesktopWindow(xfc, xfc->window, xfc->sessionWidth, xfc->sessionHeight);
					}

					xf_draw_screen(xfc, 0, 0, xfc->sessionWidth, xfc->sessionHeight);
					return TRUE;

				case XK_1:	/* Ctrl-Alt-1: Zoom in */
					zdx = zdy = 10;
					break;

				case XK_2:	/* Ctrl-Alt-2: Zoom out */
					zdx = zdy = -10;
					break;

				case XK_3:	/* Ctrl-Alt-3: Pan left */
					pdx = -10;
					break;

				case XK_4:	/* Ctrl-Alt-4: Pan right */
					pdx = 10;
					break;

				case XK_5:	/* Ctrl-Alt-5: Pan up */
					pdy = -10;
					break;

				case XK_6:	/* Ctrl-Alt-6: Pan up */
					pdy = 10;
					break;
			}

			if (pdx != 0 || pdy != 0)
			{
				PanningChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = pdx;
				e.dy = pdy;
				PubSub_OnPanningChange(ctx->pubSub, xfc, &e);
				return TRUE;
			}

			if (zdx != 0 || zdy != 0)
			{
				ZoomingChangeEventArgs e;
				EventArgsInit(&e, "xfreerdp");
				e.dx = zdx;
				e.dy = zdy;
				PubSub_OnZoomingChange(ctx->pubSub, xfc, &e);
				return TRUE;
			}
		}
	}

#endif /* WITH_XRENDER defined */
#endif /* pinch/zoom/pan simulation */
	return FALSE;
}

void xf_keyboard_handle_special_keys_release(xfContext* xfc, KeySym keysym)
{
	if (keysym != XK_Control_R)
		return;

	firstPressRightCtrl = TRUE;

	if (!ungrabKeyboardWithRightCtrl)
		return;

	// all requirements for ungrab are fulfilled, ungrabbing now
	XF_MODIFIER_KEYS mod = { 0 };
	xk_keyboard_get_modifier_keys(xfc, &mod);

	if (!mod.RightCtrl)
	{
		if (!xfc->fullscreen)
		{
			xf_toggle_control(xfc);
		}

		XUngrabKeyboard(xfc->display, CurrentTime);
	}

	// ungrabbed
	ungrabKeyboardWithRightCtrl = FALSE;
}

BOOL xf_keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	xfContext* xfc = (xfContext*) context;
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_SCROLL_LOCK,
	                          XK_Scroll_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_NUM_LOCK, XK_Num_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_CAPS_LOCK, XK_Caps_Lock);
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_KANA_LOCK, XK_Kana_Lock);
	return TRUE;
}

BOOL xf_keyboard_set_ime_status(rdpContext* context, UINT16 imeId, UINT32 imeState,
                                UINT32 imeConvMode)
{
	if (!context)
		return FALSE;

	WLog_WARN(TAG,
	          "KeyboardSetImeStatus(unitId=%04"PRIx16", imeState=%08"PRIx32", imeConvMode=%08"PRIx32") ignored",
	          imeId, imeState, imeConvMode);
	return TRUE;
}
