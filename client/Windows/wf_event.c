/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Event Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Jason Post <jason.v.post@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>

#include "wf_event.h"
#include "wf_client.h"
#include "wf_cliprdr.h"

#include <windowsx.h>

#define TAG CLIENT_TAG("windows")

#ifdef WITH_DEBUG_KBD
#define DEBUG_KBD(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_KBD(...) \
	do                 \
	{                  \
	} while (0)
#endif

static HWND g_focus_hWnd = NULL;

void wf_event_set_focus_hwnd(HWND hWnd)
{
	g_focus_hWnd = hWnd;
}

#ifndef VK_KANA
#define VK_KANA 0x15
#endif

static BOOL g_flipping_in = FALSE;
static BOOL g_flipping_out = FALSE;

static BOOL g_keystates[256] = { 0 };

static BOOL ctrl_down(void)
{
	return g_keystates[VK_CONTROL] || g_keystates[VK_LCONTROL] || g_keystates[VK_RCONTROL];
}

static BOOL alt_ctrl_down(void)
{
	const BOOL altDown = g_keystates[VK_MENU] || g_keystates[VK_LMENU] || g_keystates[VK_RMENU];
	return altDown && ctrl_down();
}

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	wfContext* wfc = NULL;
	rdpInput* input;
	PKBDLLHOOKSTRUCT p;
	DWORD rdp_scancode;
	BOOL keystate;

	if (nCode == HC_ACTION)
	{
		switch (wParam)
		{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (!wfc)
					wfc = (wfContext*)GetWindowLongPtr(g_focus_hWnd, GWLP_USERDATA);
				p = (PKBDLLHOOKSTRUCT)lParam;

				if (!wfc || !p)
					return 1;

				input = wfc->common.context.input;
				rdp_scancode = MAKE_RDP_SCANCODE((BYTE)p->scanCode, p->flags & LLKHF_EXTENDED);

				/* Fix: g_keystates was previously updated using scan codes,
				   but shortcut detection uses Virtual Key codes, causing
				   Ctrl+Alt+Enter to fail. Syncing to Virtual Key codes fix this. */
				keystate = g_keystates[p->vkCode & 0xFF];

				switch (wParam)
				{
					case WM_KEYDOWN:
					case WM_SYSKEYDOWN:
						if (p->vkCode < 256)
							g_keystates[p->vkCode] = TRUE;
						break;
					case WM_KEYUP:
					case WM_SYSKEYUP:
					default:
						if (p->vkCode < 256)
							g_keystates[p->vkCode] = FALSE;
						break;
				}
				DEBUG_KBD("keydown %d scanCode 0x%08lX flags 0x%08lX vkCode 0x%08lX",
				          (wParam == WM_KEYDOWN), p->scanCode, p->flags, p->vkCode);

				if (p->vkCode == VK_RETURN && alt_ctrl_down())
				{
					if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
					{
						if (!wfc->fullscreen_pending)
						{
							wfc->fullscreen_pending = TRUE;
							PostMessage(wfc->hwnd, WM_USER_CH_FULLSCREEN, 0, 0);
						}
					}

					return 1;
				}

				if (rdp_scancode == RDP_SCANCODE_UNKNOWN)
					return 1;

				input->KeyboardEvent(input, (UINT16)p->flags, (UINT16)rdp_scancode);
				break;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void wf_event_process_focus_in(wfContext* wfc)
{
	rdpInput* input;
	UINT32 syncFlags = 0;
	POINT pt;
	RECT rc;

	WINPR_ASSERT(wfc);

	input = wfc->common.context.input;
	WINPR_ASSERT(input);

	if (GetKeyState(VK_NUMLOCK) & 0x0001)
		syncFlags |= KBD_SYNC_NUM_LOCK;

	if (GetKeyState(VK_CAPITAL) & 0x0001)
		syncFlags |= KBD_SYNC_CAPS_LOCK;

	if (GetKeyState(VK_SCROLL) & 0x0001)
		syncFlags |= KBD_SYNC_SCROLL_LOCK;

	if (GetKeyState(VK_KANA))
		syncFlags |= KBD_SYNC_KANA_LOCK;

	input->FocusInEvent(input, syncFlags);
	/* send pointer position if the cursor is currently inside our client area */
	GetCursorPos(&pt);
	ScreenToClient(wfc->hwnd, &pt);
	GetClientRect(wfc->hwnd, &rc);

	if (pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom)
		input->MouseEvent(input, PTR_FLAGS_MOVE, (UINT16)pt.x, (UINT16)pt.y);
}

static BOOL wf_event_process_WM_MOUSEWHEEL(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam,
                                           LPARAM lParam, BOOL horizontal, INT32 x, INT32 y)
{
	int delta;
	UINT16 flags = 0;
	rdpInput* input;

	WINPR_ASSERT(wfc);

	input = wfc->common.context.input;
	WINPR_ASSERT(input);

	DefWindowProc(hWnd, Msg, wParam, lParam);
	delta = ((signed short)HIWORD(wParam)); /* GET_WHEEL_DELTA_WPARAM(wParam); */

	if (horizontal)
		flags |= PTR_FLAGS_HWHEEL;
	else
		flags |= PTR_FLAGS_WHEEL;

	if (delta < 0)
	{
		flags |= PTR_FLAGS_WHEEL_NEGATIVE;
		delta = -delta;
	}

	{
		UINT16 val = (UINT16)delta;
		if (val > 0xFF)
			val = 0xFF; /* Cap at max 8-bit value to prevent flag corruption */
		flags |= val;
	}

	return wf_scale_mouse_event(wfc, flags, x, y);
}

LRESULT CALLBACK wf_event_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	wfContext* wfc = (wfContext*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if (wfc)
	{
		switch (Msg)
		{
			case WM_MOVE:
				if (!wfc->fullscreen && !wfc->sw_fullscreen)
				{
					int x = GET_X_LPARAM(lParam);
					int y = GET_Y_LPARAM(lParam);

					if (wfc->client_x != x || wfc->client_y != y)
					{
						wfc->client_x = x;
						wfc->client_y = y;
					}
				}

				break;

			case WM_SIZE:
				if (!wfc->fullscreen && !wfc->sw_fullscreen)
				{
					if (wParam != SIZE_MINIMIZED)
					{
						int width = LOWORD(lParam);
						int height = HIWORD(lParam);

						if (wfc->client_width != width || wfc->client_height != height)
						{
							wfc->client_width = width;
							wfc->client_height = height;
						}
					}
				}

				break;

			case WM_SYSCOMMAND:
				if (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER)
					return 0;

				break;

			case WM_ERASEBKGND:
				return 1;

			case WM_LBUTTONDOWN:
				wf_scale_mouse_event(wfc, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1,
				                     GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_LBUTTONUP:
				wf_scale_mouse_event(wfc, PTR_FLAGS_BUTTON1, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_RBUTTONDOWN:
				wf_scale_mouse_event(wfc, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2,
				                     GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_RBUTTONUP:
				wf_scale_mouse_event(wfc, PTR_FLAGS_BUTTON2, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_MBUTTONDOWN:
				wf_scale_mouse_event(wfc, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3,
				                     GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_MBUTTONUP:
				wf_scale_mouse_event(wfc, PTR_FLAGS_BUTTON3, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_XBUTTONDOWN:
				wf_scale_mouse_event(
				    wfc,
				    PTR_FLAGS_DOWN | (HIWORD(wParam) == XBUTTON1 ? PTR_FLAGS_BUTTON1 : PTR_FLAGS_BUTTON2),
				    GET_X_LPARAM(lParam) - wfc->offset_x, GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_XBUTTONUP:
				wf_scale_mouse_event(
				    wfc, (HIWORD(wParam) == XBUTTON1 ? PTR_FLAGS_BUTTON1 : PTR_FLAGS_BUTTON2),
				    GET_X_LPARAM(lParam) - wfc->offset_x, GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_MOUSEMOVE:
				wf_scale_mouse_event(wfc, PTR_FLAGS_MOVE, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;
#if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)

			case WM_MOUSEWHEEL:
				wf_event_process_WM_MOUSEWHEEL(wfc, hWnd, Msg, wParam, lParam, FALSE,
				                               GET_X_LPARAM(lParam) - wfc->offset_x,
				                               GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;
#endif
#if (_WIN32_WINNT >= 0x0600)

			case WM_MOUSEHWHEEL:
				wf_event_process_WM_MOUSEWHEEL(wfc, hWnd, Msg, wParam, lParam, TRUE,
				                               GET_X_LPARAM(lParam) - wfc->offset_x,
				                               GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;
#endif

			case WM_SETCURSOR:
				if (LOWORD(lParam) == HTCLIENT)
					SetCursor(wfc->cursor);
				else
					DefWindowProc(hWnd, Msg, wParam, lParam);

				break;

			case WM_HSCROLL:
			{
				int r;
				input_ibus_process_xevent(wfc->common.context.input, (void*)Msg, (void*)wParam,
				                          (void*)lParam);
				r = (int)DefWindowProc(hWnd, Msg, wParam, lParam);
				return r;
			}

			case WM_VSCROLL:
			{
				int r;
				input_ibus_process_xevent(wfc->common.context.input, (void*)Msg, (void*)wParam,
				                          (void*)lParam);
				r = (int)DefWindowProc(hWnd, Msg, wParam, lParam);
				return r;
			}

			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hWnd, &ps);
				EndPaint(hWnd, &ps);
				return 0;
			}

			case WM_SETFOCUS:
				wf_event_process_focus_in(wfc);
				break;

			case WM_KILLFOCUS:
				break;

			case WM_CLOSE:
				PostQuitMessage(0);
				break;

			case WM_USER_CH_FULLSCREEN:
				wf_toggle_fullscreen(wfc);
				wfc->fullscreen_pending = FALSE;
				break;

			default:
				return DefWindowProc(hWnd, Msg, wParam, lParam);
		}
	}
	else
	{
		return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return 0;
}