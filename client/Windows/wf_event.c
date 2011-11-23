/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <freerdp/freerdp.h>
#include "wfreerdp.h"

#include "wf_event.h"

static HWND g_focus_hWnd;
extern HCURSOR g_default_cursor;

#define X_POS(lParam) (lParam & 0xffff)
#define Y_POS(lParam) ((lParam >> 16) & 0xffff)

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	DWORD flags;
	wfInfo* wfi;
	uint8 scanCode;
	rdpInput* input;
	uint16 kbdFlags;
	PKBDLLHOOKSTRUCT p;

	DEBUG_KBD("Low-level keyboard hook, hWnd %X nCode %X wParam %X\n", g_focus_hWnd, nCode, wParam);

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
				scanCode = (uint8) p->scanCode;
				input = wfi->instance->input;
				flags = p->flags;
				kbdFlags = 0;

				DEBUG_KBD("keydown %d scanCode %04X flags %02X vkCode %02X\n",
					(wParam == WM_KEYDOWN), scanCode, flags, p->vkCode);

				if (wfi->fs_toggle &&
					((p->vkCode == VK_RETURN) || (p->vkCode == VK_CANCEL)) &&
					(GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
					(GetAsyncKeyState(VK_MENU) & 0x8000)) /* could also use flags & LLKHF_ALTDOWN */
				{
					if (wParam == WM_KEYDOWN)
						//wf_toggle_fullscreen(wfi);
					return 1;
				}

				if (scanCode == 0x45) /* NumLock-ish */
				{
					if (flags & LLKHF_EXTENDED)
					{
						/* Windows sends NumLock as extended - rdp doesn't */
						DEBUG_KBD("hack: NumLock (x45) should not be extended\n");
						flags &= ~LLKHF_EXTENDED;
					}
					else
					{
						/* Windows sends Pause as if it was a RDP NumLock (handled above).
						 * It must however be sent as a one-shot Ctrl+NumLock */
						if (wParam == WM_KEYDOWN)
						{
							DEBUG_KBD("Pause, sent as Ctrl+NumLock\n");
							input->KeyboardEvent(input, KBD_FLAGS_DOWN, 0x1D); /* Ctrl down */
							input->KeyboardEvent(input, KBD_FLAGS_DOWN, 0x45); /* NumLock down */
							input->KeyboardEvent(input, KBD_FLAGS_RELEASE, 0x1D); /* Ctrl up */
							input->KeyboardEvent(input, KBD_FLAGS_RELEASE, 0x45); /* NumLock up */
						}
						else
						{
							DEBUG_KBD("Pause up\n");
						}

						return 1;
					}
				}

				if ((scanCode == 0x36) && (flags & LLKHF_EXTENDED))
				{
					DEBUG_KBD("hack: right shift (x36) should not be extended\n");
					flags &= ~LLKHF_EXTENDED;
				}

				kbdFlags |= (flags & LLKHF_UP) ? KBD_FLAGS_RELEASE : KBD_FLAGS_DOWN;
				kbdFlags |= (flags & LLKHF_EXTENDED) ? KBD_FLAGS_EXTENDED : 0;
				input->KeyboardEvent(input, kbdFlags, scanCode);

				if (p->vkCode == VK_CAPITAL)
					DEBUG_KBD("caps lock is processed on client side too to toggle caps lock indicator\n");
				else
					return 1;

				break;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK wf_event_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	LONG ptr;
	wfInfo* wfi;
	int x, y, w, h;
	PAINTSTRUCT ps;
	rdpInput* input;
	boolean processed;

	processed = true;
	ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	wfi = (wfInfo*) ptr;

	if (wfi != NULL)
	{
		input = wfi->instance->input;

		switch (Msg)
		{
			case WM_PAINT:
				hdc = BeginPaint(hWnd, &ps);
				
				x = ps.rcPaint.left;
				y = ps.rcPaint.top;
				w = ps.rcPaint.right - ps.rcPaint.left + 1;
				h = ps.rcPaint.bottom - ps.rcPaint.top + 1;

				//printf("WM_PAINT: x:%d y:%d w:%d h:%d\n", x, y, w, h);

				BitBlt(hdc, x, y, w, h, wfi->primary->hdc, x, y, SRCCOPY);

				EndPaint(hWnd, &ps);
				break;

			case WM_LBUTTONDOWN:
				input->MouseEvent(input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, X_POS(lParam), Y_POS(lParam));
				break;

			case WM_LBUTTONUP:
				input->MouseEvent(input, PTR_FLAGS_BUTTON1, X_POS(lParam), Y_POS(lParam));
				break;

			case WM_RBUTTONDOWN:
				input->MouseEvent(input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, X_POS(lParam), Y_POS(lParam));
				break;

			case WM_RBUTTONUP:
				input->MouseEvent(input, PTR_FLAGS_BUTTON2, X_POS(lParam), Y_POS(lParam));
				break;

			case WM_MOUSEMOVE:
				input->MouseEvent(input, PTR_FLAGS_MOVE, X_POS(lParam), Y_POS(lParam));
				break;

			case WM_SETCURSOR:
				SetCursor(wfi->cursor);
				break;

			default:
				processed = false;
				break;
		}
	}
	else
	{
		processed = false;
	}

	if (processed)
		return 0;

	switch (Msg)
	{
		case WM_DESTROY:
			PostQuitMessage(WM_QUIT);
			break;

		case WM_SETCURSOR:
			SetCursor(g_default_cursor);
			break;

		case WM_SETFOCUS:
			DEBUG_KBD("getting focus %X\n", hWnd);
			g_focus_hWnd = hWnd;
			break;

		case WM_KILLFOCUS:
			DEBUG_KBD("loosing focus %X\n", hWnd);
			if (g_focus_hWnd == hWnd)
				g_focus_hWnd = NULL;
			break;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
			break;
	}

	return 0;
}
