/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Event Handling
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/freerdp.h>

#include "wf_interface.h"

#include "wf_gdi.h"
#include "wf_event.h"

static HWND g_focus_hWnd;

#define X_POS(lParam) (lParam & 0xFFFF)
#define Y_POS(lParam) ((lParam >> 16) & 0xFFFF)

BOOL wf_scale_blt(wfInfo* wfi, HDC hdc, int x, int y, int w, int h, HDC hdcSrc, int x1, int y1, DWORD rop);
void wf_scale_mouse_event(wfInfo* wfi, rdpInput* input, UINT16 flags, UINT16 x, UINT16 y);

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	wfInfo* wfi;
	DWORD rdp_scancode;
	rdpInput* input;
	PKBDLLHOOKSTRUCT p;

	DEBUG_KBD("Low-level keyboard hook, hWnd %X nCode %X wParam %X", g_focus_hWnd, nCode, wParam);

	if (g_focus_hWnd && (nCode == HC_ACTION))
	{
		switch (wParam)
		{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
				wfi = (wfInfo*) GetWindowLongPtr(g_focus_hWnd, GWLP_USERDATA);
				p = (PKBDLLHOOKSTRUCT) lParam;

				if (!wfi || !p)
					return 1;
				
				input = wfi->instance->input;
				rdp_scancode = MAKE_RDP_SCANCODE((BYTE) p->scanCode, p->flags & LLKHF_EXTENDED);

				DEBUG_KBD("keydown %d scanCode %04X flags %02X vkCode %02X",
					(wParam == WM_KEYDOWN), (BYTE) p->scanCode, p->flags, p->vkCode);

				if (wfi->fs_toggle &&
					((p->vkCode == VK_RETURN) || (p->vkCode == VK_CANCEL)) &&
					(GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
					(GetAsyncKeyState(VK_MENU) & 0x8000)) /* could also use flags & LLKHF_ALTDOWN */
				{
					if (wParam == WM_KEYDOWN)
					{
						wf_toggle_fullscreen(wfi);
						return 1;
					}
				}

				if (rdp_scancode == RDP_SCANCODE_NUMLOCK_EXTENDED)
				{
					/* Windows sends NumLock as extended - rdp doesn't */
					DEBUG_KBD("hack: NumLock (x45) should not be extended");
					rdp_scancode = RDP_SCANCODE_NUMLOCK;
				}
				else if (rdp_scancode == RDP_SCANCODE_NUMLOCK)
				{
					/* Windows sends Pause as if it was a RDP NumLock (handled above).
					 * It must however be sent as a one-shot Ctrl+NumLock */
					if (wParam == WM_KEYDOWN)
					{
						DEBUG_KBD("Pause, sent as Ctrl+NumLock");
						freerdp_input_send_keyboard_event_ex(input, TRUE, RDP_SCANCODE_LCONTROL);
						freerdp_input_send_keyboard_event_ex(input, TRUE, RDP_SCANCODE_NUMLOCK);
						freerdp_input_send_keyboard_event_ex(input, FALSE, RDP_SCANCODE_LCONTROL);
						freerdp_input_send_keyboard_event_ex(input, FALSE, RDP_SCANCODE_NUMLOCK);
					}
					else
					{
						DEBUG_KBD("Pause up");
					}

					return 1;
				}
				else if (rdp_scancode == RDP_SCANCODE_RSHIFT_EXTENDED)
				{
					DEBUG_KBD("right shift (x36) should not be extended");
					rdp_scancode = RDP_SCANCODE_RSHIFT;
				}

				freerdp_input_send_keyboard_event_ex(input, !(p->flags & LLKHF_UP), rdp_scancode);

				if (p->vkCode == VK_CAPITAL)
					DEBUG_KBD("caps lock is processed on client side too to toggle caps lock indicator");
				else
					return 1;

				break;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static int wf_event_process_WM_MOUSEWHEEL(wfInfo* wfi, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	int delta;
	int flags;
	rdpInput* input;

	DefWindowProc(hWnd, Msg, wParam, lParam);
	input = wfi->instance->input;
	delta = ((signed short) HIWORD(wParam)); /* GET_WHEEL_DELTA_WPARAM(wParam); */

	if (delta > 0)
	{
		flags = PTR_FLAGS_WHEEL | 0x0078;
	}
	else
	{
		flags = PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
	}

	input->MouseEvent(input, flags, 0, 0);
	
	return 0;
}

LRESULT CALLBACK wf_event_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	LONG ptr;
	wfInfo* wfi;
	int x, y, w, h;
	PAINTSTRUCT ps;
	rdpInput* input;
	BOOL processed;

	processed = TRUE;
	ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	wfi = (wfInfo*) ptr;

	if (wfi != NULL)
	{
		input = wfi->instance->input;

		switch (Msg)
		{
			case WM_ERASEBKGND:
				/* Say we handled it - prevents flickering */
				return (LRESULT) 1;

			case WM_PAINT:
				hdc = BeginPaint(hWnd, &ps);

				x = ps.rcPaint.left;
				y = ps.rcPaint.top;
				w = ps.rcPaint.right - ps.rcPaint.left + 1;
				h = ps.rcPaint.bottom - ps.rcPaint.top + 1;

				wf_scale_blt(wfi, hdc, x, y, w, h, wfi->primary->hdc, x - wfi->offset_x, y - wfi->offset_y, SRCCOPY);

				EndPaint(hWnd, &ps);
				break;

			case WM_LBUTTONDOWN:
				wf_scale_mouse_event(wfi, input,PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, X_POS(lParam) - wfi->offset_x, Y_POS(lParam) - wfi->offset_y);
				break;

			case WM_LBUTTONUP:
				wf_scale_mouse_event(wfi, input, PTR_FLAGS_BUTTON1, X_POS(lParam) - wfi->offset_x, Y_POS(lParam) - wfi->offset_y);
				break;

			case WM_RBUTTONDOWN:
				wf_scale_mouse_event(wfi, input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, X_POS(lParam) - wfi->offset_x, Y_POS(lParam) - wfi->offset_y);
				break;

			case WM_RBUTTONUP:
				wf_scale_mouse_event(wfi, input, PTR_FLAGS_BUTTON2, X_POS(lParam) - wfi->offset_x, Y_POS(lParam) - wfi->offset_y);
				break;

			case WM_MOUSEMOVE:
				wf_scale_mouse_event(wfi, input, PTR_FLAGS_MOVE, X_POS(lParam) - wfi->offset_x, Y_POS(lParam) - wfi->offset_y);
				break;

			case WM_MOUSEWHEEL:
				wf_event_process_WM_MOUSEWHEEL(wfi, hWnd, Msg, wParam, lParam);
				break;

			case WM_SETCURSOR:
				if (LOWORD(lParam) == HTCLIENT)
					SetCursor(wfi->cursor);
				else
					DefWindowProc(hWnd, Msg, wParam, lParam);
				break;

			default:
				processed = FALSE;
				break;
		}
	}
	else
	{
		processed = FALSE;
	}

	if (processed)
		return 0;

	switch (Msg)
	{
		case WM_DESTROY:
			PostQuitMessage(WM_QUIT);
			break;

		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT)
				SetCursor(wfi->hDefaultCursor);
			else
				DefWindowProc(hWnd, Msg, wParam, lParam);
			break;

		case WM_SETFOCUS:
			DEBUG_KBD("getting focus %X", hWnd);
			g_focus_hWnd = hWnd;
			break;

		case WM_KILLFOCUS:
			DEBUG_KBD("loosing focus %X", hWnd);
			if (g_focus_hWnd == hWnd)
				g_focus_hWnd = NULL;
			break;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
			break;
	}

	return 0;
}

BOOL wf_scale_blt(wfInfo* wfi, HDC hdc, int x, int y, int w, int h, HDC hdcSrc, int x1, int y1, DWORD rop)
{
	int ww, wh, dw, dh;

	if (!wfi->client_width)
		wfi->client_width = wfi->width;

	if (!wfi->client_height)
		wfi->client_height = wfi->height;

	ww = wfi->client_width;
	wh = wfi->client_height;
	dw = wfi->instance->settings->DesktopWidth;
	dh = wfi->instance->settings->DesktopHeight;

	if (!ww)
		ww = dw;

	if (!wh)
		wh = dh;

	if (!wfi->instance->settings->SmartSizing || (ww == dw && wh == dh))
	{
		return BitBlt(hdc, x, y, w, h, wfi->primary->hdc, x1, y1, SRCCOPY);
	}
	else
	{
		SetStretchBltMode(hdc, HALFTONE);
		SetBrushOrgEx(hdc, 0, 0, NULL);

		return StretchBlt(hdc, x * ww / dw, y * wh / dh, ww, wh, wfi->primary->hdc, x1, y1, dw, dh, SRCCOPY);
	}

	return TRUE;
}

void wf_scale_mouse_event(wfInfo* wfi, rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	int ww, wh, dw, dh;

	if (!wfi->client_width)
		wfi->client_width = wfi->width;

	if (!wfi->client_height)
		wfi->client_height = wfi->height;

	ww = wfi->client_width;
	wh = wfi->client_height;
	dw = wfi->instance->settings->DesktopWidth;
	dh = wfi->instance->settings->DesktopHeight;

	if ((ww == dw) && (wh == dh))
		input->MouseEvent(input, flags, x, y);
	else
		input->MouseEvent(input, flags, x * dw / ww, y * dh / wh);
}
