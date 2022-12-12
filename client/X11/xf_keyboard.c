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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/assert.h>
#include <winpr/collections.h>

#include <freerdp/utils/string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include <freerdp/locale/keyboard.h>

#include "xf_event.h"

#include "xf_keyboard.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static void xf_keyboard_modifier_map_free(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	if (xfc->modifierMap)
	{
		XFreeModifiermap(xfc->modifierMap);
		xfc->modifierMap = NULL;
	}
}

BOOL xf_keyboard_update_modifier_map(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	xf_keyboard_modifier_map_free(xfc);
	xfc->modifierMap = XGetModifierMapping(xfc->display);
	return xfc->modifierMap != NULL;
}

static void xf_keyboard_send_key(xfContext* xfc, BOOL down, BOOL repeat, const XKeyEvent* ev);

static BOOL xf_sync_kbd_state(xfContext* xfc)
{
	const UINT32 syncFlags = xf_keyboard_get_toggle_keys_state(xfc);

	WINPR_ASSERT(xfc);
	return freerdp_input_send_synchronize_event(xfc->common.context.input, syncFlags);
}

static void xf_keyboard_clear(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	ZeroMemory(xfc->KeyboardState, sizeof(xfc->KeyboardState));
}

static BOOL xf_keyboard_action_script_init(xfContext* xfc)
{
	wObject* obj;
	FILE* keyScript;
	char* keyCombination;
	char buffer[1024] = { 0 };
	char command[1024] = { 0 };
	const rdpSettings* settings;
	const char* ActionScript;
	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	ActionScript = freerdp_settings_get_string(settings, FreeRDP_ActionScript);
	xfc->actionScriptExists = winpr_PathFileExists(ActionScript);

	if (!xfc->actionScriptExists)
		return FALSE;

	xfc->keyCombinations = ArrayList_New(TRUE);

	if (!xfc->keyCombinations)
		return FALSE;

	obj = ArrayList_Object(xfc->keyCombinations);
	obj->fnObjectFree = free;
	sprintf_s(command, sizeof(command), "%s key", ActionScript);
	keyScript = popen(command, "r");

	if (!keyScript)
	{
		xfc->actionScriptExists = FALSE;
		return FALSE;
	}

	while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
	{
		char* context = NULL;
		strtok_s(buffer, "\n", &context);
		keyCombination = _strdup(buffer);

		if (!keyCombination || !ArrayList_Append(xfc->keyCombinations, keyCombination))
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
	rdpSettings* settings;

	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	xf_keyboard_clear(xfc);
	xfc->KeyboardLayout = settings->KeyboardLayout;
	xfc->KeyboardLayout =
	    freerdp_keyboard_init_ex(xfc->KeyboardLayout, settings->KeyboardRemappingList);
	settings->KeyboardLayout = xfc->KeyboardLayout;

	if (!xf_keyboard_update_modifier_map(xfc))
		return FALSE;

	xf_keyboard_action_script_init(xfc);
	return TRUE;
}

void xf_keyboard_free(xfContext* xfc)
{
	xf_keyboard_modifier_map_free(xfc);
	xf_keyboard_action_script_free(xfc);
}

void xf_keyboard_key_press(xfContext* xfc, const XKeyEvent* event, KeySym keysym)
{
	BOOL last;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);

	if (event->keycode < 8)
		return;

	last = xfc->KeyboardState[event->keycode];
	xfc->KeyboardState[event->keycode] = TRUE;

	if (xf_keyboard_handle_special_keys(xfc, keysym))
		return;

	xf_keyboard_send_key(xfc, TRUE, last, event);
}

void xf_keyboard_key_release(xfContext* xfc, const XKeyEvent* event, KeySym keysym)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);

	if (event->keycode < 8)
		return;

	BOOL last = xfc->KeyboardState[event->keycode];
	xfc->KeyboardState[event->keycode] = FALSE;
	xf_keyboard_handle_special_keys_release(xfc, keysym);
	xf_keyboard_send_key(xfc, FALSE, last, event);
}

void xf_keyboard_release_all_keypress(xfContext* xfc)
{
	size_t keycode;
	DWORD rdp_scancode;

	WINPR_ASSERT(xfc);

	for (keycode = 0; keycode < ARRAYSIZE(xfc->KeyboardState); keycode++)
	{
		if (xfc->KeyboardState[keycode])
		{
			rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(keycode);

			// release tab before releasing the windows key.
			// this stops the start menu from opening on unfocus event.
			if (rdp_scancode == RDP_SCANCODE_LWIN)
				freerdp_input_send_keyboard_event_ex(xfc->common.context.input, FALSE, FALSE,
				                                     RDP_SCANCODE_TAB);

			freerdp_input_send_keyboard_event_ex(xfc->common.context.input, FALSE, FALSE,
			                                     rdp_scancode);
			xfc->KeyboardState[keycode] = FALSE;
		}
	}
	xf_sync_kbd_state(xfc);
}

BOOL xf_keyboard_key_pressed(xfContext* xfc, KeySym keysym)
{
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);
	return xfc->KeyboardState[keycode];
}

void xf_keyboard_send_key(xfContext* xfc, BOOL down, BOOL repeat, const XKeyEvent* event)
{
	DWORD rdp_scancode;
	rdpInput* input;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(event);

	input = xfc->common.context.input;
	WINPR_ASSERT(input);

	rdp_scancode = freerdp_keyboard_get_rdp_scancode_from_x11_keycode(event->keycode);
	if (rdp_scancode == RDP_SCANCODE_PAUSE && !xf_keyboard_key_pressed(xfc, XK_Control_L) &&
	    !xf_keyboard_key_pressed(xfc, XK_Control_R))
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
		if (freerdp_settings_get_bool(xfc->common.context.settings, FreeRDP_UnicodeInput))
		{
			wchar_t buffer[32] = { 0 };
			int xwc = -1;

			switch (rdp_scancode)
			{
				case RDP_SCANCODE_RETURN:
					break;
				default:
				{
					XIM xim = XOpenIM(xfc->display, 0, 0, 0);
					XIC xic =
					    XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, NULL);

					KeySym ignore = { 0 };
					Status return_status;
					XKeyEvent ev = *event;
					ev.type = KeyPress;
					xwc = XwcLookupString(xic, &ev, buffer, ARRAYSIZE(buffer), &ignore,
					                      &return_status);
				}
				break;
			}

			if (xwc < 1)
			{
				if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
					WLog_ERR(TAG, "Unknown key with X keycode 0x%02" PRIx8 "", event->keycode);
				else
					freerdp_input_send_keyboard_event_ex(input, down, repeat, rdp_scancode);
			}
			else
				freerdp_input_send_unicode_keyboard_event(input, down ? KBD_FLAGS_RELEASE : 0,
				                                          buffer[0]);
		}
		else if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
			WLog_ERR(TAG, "Unknown key with X keycode 0x%02" PRIx8 "", event->keycode);
		else
			freerdp_input_send_keyboard_event_ex(input, down, repeat, rdp_scancode);

		if ((rdp_scancode == RDP_SCANCODE_CAPSLOCK) && (down == FALSE))
		{
			xf_sync_kbd_state(xfc);
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
		XQueryPointer(xfc->display, xfc->window->handle, &wdummy, &wdummy, &dummy, &dummy, &dummy,
		              &dummy, &state);
	}
	else
	{
		XQueryPointer(xfc->display, DefaultRootWindow(xfc->display), &wdummy, &wdummy, &dummy,
		              &dummy, &dummy, &dummy, &state);
	}

	return state;
}

static int xf_keyboard_get_keymask(xfContext* xfc, int keysym)
{
	int modifierpos, key, keysymMask = 0;
	KeyCode keycode = XKeysymToKeycode(xfc->display, keysym);

	if (keycode == NoSymbol)
		return 0;

	WINPR_ASSERT(xfc->modifierMap);
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

	return XkbLockModifiers(xfc->display, XkbUseCoreKbd, keysymMask, on ? keysymMask : 0);
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

static void xk_keyboard_update_modifier_keys(xfContext* xfc)
{
	int state;
	size_t i;
	KeyCode keycode;
	int keysyms[] = { XK_Shift_L,   XK_Shift_R,   XK_Alt_L,   XK_Alt_R,
		              XK_Control_L, XK_Control_R, XK_Super_L, XK_Super_R };

	xf_keyboard_clear(xfc);

	state = xf_keyboard_read_keyboard_state(xfc);

	for (i = 0; i < ARRAYSIZE(keysyms); i++)
	{
		if (xf_keyboard_get_key_state(xfc, state, keysyms[i]))
		{
			keycode = XKeysymToKeycode(xfc->display, keysyms[i]);
			xfc->KeyboardState[keycode] = TRUE;
		}
	}
}

void xf_keyboard_focus_in(xfContext* xfc)
{
	rdpInput* input;
	UINT32 syncFlags, state;
	Window w;
	int d, x, y;

	WINPR_ASSERT(xfc);
	if (!xfc->display || !xfc->window)
		return;

	input = xfc->common.context.input;
	WINPR_ASSERT(input);

	syncFlags = xf_keyboard_get_toggle_keys_state(xfc);
	freerdp_input_send_focus_in_event(input, syncFlags);
	xk_keyboard_update_modifier_keys(xfc);

	/* finish with a mouse pointer position like mstsc.exe if required */

	if (xfc->remote_app)
		return;

	if (XQueryPointer(xfc->display, xfc->window->handle, &w, &w, &d, &d, &x, &y, &state))
	{
		if ((x >= 0) && (x < xfc->window->width) && (y >= 0) && (y < xfc->window->height))
		{
			xf_event_adjust_coordinates(xfc, &x, &y);
			freerdp_client_send_button_event(&xfc->common, FALSE, PTR_FLAGS_MOVE, x, y);
		}
	}
}

static int xf_keyboard_execute_action_script(xfContext* xfc, XF_MODIFIER_KEYS* mod, KeySym keysym)
{
	int index;
	int count;
	int status = 1;
	FILE* keyScript;
	const char* keyStr;
	BOOL match = FALSE;
	char* keyCombination;
	char buffer[1024] = { 0 };
	char command[2048] = { 0 };
	char combination[1024] = { 0 };
	const char* ActionScript;

	if (!xfc->actionScriptExists)
		return 1;

	if ((keysym == XK_Shift_L) || (keysym == XK_Shift_R) || (keysym == XK_Alt_L) ||
	    (keysym == XK_Alt_R) || (keysym == XK_Control_L) || (keysym == XK_Control_R))
	{
		return 1;
	}

	keyStr = XKeysymToString(keysym);

	if (keyStr == 0)
	{
		return 1;
	}

	if (mod->Shift)
		winpr_str_append("Shift", combination, sizeof(combination), "+");

	if (mod->Ctrl)
		winpr_str_append("Ctrl", combination, sizeof(combination), "+");

	if (mod->Alt)
		winpr_str_append("Alt", combination, sizeof(combination), "+");

	if (mod->Super)
		winpr_str_append("Super", combination, sizeof(combination), "+");

	winpr_str_append(keyStr, combination, sizeof(combination), NULL);

	count = ArrayList_Count(xfc->keyCombinations);

	for (index = 0; index < count; index++)
	{
		keyCombination = (char*)ArrayList_GetItem(xfc->keyCombinations, index);

		if (_stricmp(keyCombination, combination) == 0)
		{
			match = TRUE;
			break;
		}
	}

	if (!match)
		return 1;

	ActionScript = freerdp_settings_get_string(xfc->common.context.settings, FreeRDP_ActionScript);
	sprintf_s(command, sizeof(command), "%s key %s", ActionScript, combination);
	keyScript = popen(command, "r");

	if (!keyScript)
		return -1;

	while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
	{
		char* context = NULL;
		strtok_s(buffer, "\n", &context);

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

	// remember state of RightCtrl to ungrab keyboard if next action is release of RightCtrl
	// do not return anything such that the key could be used by client if ungrab is not the goal
	if (keysym == XK_Control_R)
	{
		if (mod.RightCtrl && xfc->firstPressRightCtrl)
		{
			// Right Ctrl is pressed, getting ready to ungrab
			xfc->ungrabKeyboardWithRightCtrl = TRUE;
			xfc->firstPressRightCtrl = FALSE;
		}
	}
	else
	{
		// some other key has been pressed, abort ungrabbing
		if (xfc->ungrabKeyboardWithRightCtrl)
			xfc->ungrabKeyboardWithRightCtrl = FALSE;
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
			if (freerdp_client_encomsp_toggle_control(xfc->common.encomsp))
				return TRUE;
		}
	}

#if 0 /* set to 1 to enable multi touch gesture simulation via keyboard */
#ifdef WITH_XRENDER

	if (!xfc->remote_app && xfc->settings->MultiTouchGestures)
	{
		rdpContext* ctx = &xfc->common.context;

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

	xfc->firstPressRightCtrl = TRUE;

	if (!xfc->ungrabKeyboardWithRightCtrl)
		return;

	// all requirements for ungrab are fulfilled, ungrabbing now
	XF_MODIFIER_KEYS mod = { 0 };
	xk_keyboard_get_modifier_keys(xfc, &mod);

	if (!mod.RightCtrl)
	{
		if (!xfc->fullscreen)
		{
			freerdp_client_encomsp_toggle_control(xfc->common.encomsp);
		}

		xfc->mouse_active = FALSE;
		xf_ungrab(xfc);
	}

	// ungrabbed
	xfc->ungrabKeyboardWithRightCtrl = FALSE;
}

BOOL xf_keyboard_set_indicators(rdpContext* context, UINT16 led_flags)
{
	xfContext* xfc = (xfContext*)context;
	xf_keyboard_set_key_state(xfc, led_flags & KBD_SYNC_SCROLL_LOCK, XK_Scroll_Lock);
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
	          "KeyboardSetImeStatus(unitId=%04" PRIx16 ", imeState=%08" PRIx32
	          ", imeConvMode=%08" PRIx32 ") ignored",
	          imeId, imeState, imeConvMode);
	return TRUE;
}

BOOL xf_ungrab(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	XUngrabKeyboard(xfc->display, CurrentTime);
	XUngrabPointer(xfc->display, CurrentTime);
	xfc->common.mouse_grabbed = FALSE;
	return TRUE;
}
