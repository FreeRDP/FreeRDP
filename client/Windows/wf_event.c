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

#include <freerdp/config.h>

#include <stdio.h>

#include <winpr/assert.h>
#include <winpr/sysinfo.h>
#include <winpr/synch.h>
#include <freerdp/freerdp.h>

#include "wf_client.h"

#include "wf_gdi.h"
#include "wf_event.h"

#include <freerdp/event.h>

#include <windowsx.h>

#define TAG CLIENT_TAG("windows")

static HWND g_focus_hWnd = NULL;
static HWND g_main_hWnd = NULL;
static HWND g_parent_hWnd = NULL;

static CRITICAL_SECTION g_event_cs;
static LONG g_event_cs_refcount = 0;

#define RESIZE_MIN_DELAY 200 /* minimum delay in ms between two resizes */

static BOOL wf_scale_blt(wfContext* wfc, HDC hdc, int x, int y, int w, int h, HDC hdcSrc, int x1,
                         int y1, DWORD rop);
static BOOL wf_scale_mouse_event(wfContext* wfc, UINT16 flags, INT32 x, INT32 y);
#if (_WIN32_WINNT >= 0x0500)
static BOOL wf_scale_mouse_event_ex(wfContext* wfc, UINT16 flags, UINT16 buttonMask, INT32 x,
                                    INT32 y);
#endif

static BOOL g_flipping_in = FALSE;
static BOOL g_flipping_out = FALSE;

static BOOL g_keystates[256] = { 0 };

void wf_event_init(void)
{
	/* Initialize global critical section once for all RDP instances */
	if (InterlockedIncrement(&g_event_cs_refcount) == 1)
		InitializeCriticalSection(&g_event_cs);
}

void wf_event_uninit(void)
{
	/* Delete critical section only when the last RDP instance is closed */
	if (InterlockedDecrement(&g_event_cs_refcount) == 0)
		DeleteCriticalSection(&g_event_cs);
}

static BOOL ctrl_down(void)
{
	/* Fix: Use Critical Section to protect shared keystate access */
	EnterCriticalSection(&g_event_cs);
	const BOOL result = g_keystates[VK_CONTROL] || g_keystates[VK_LCONTROL] || g_keystates[VK_RCONTROL];
	LeaveCriticalSection(&g_event_cs);
	return result;
}

static BOOL alt_ctrl_down(void)
{
	/* Fix: Use Critical Section to protect shared keystate access */
	EnterCriticalSection(&g_event_cs);
	const BOOL altDown = g_keystates[VK_MENU] || g_keystates[VK_LMENU] || g_keystates[VK_RMENU];
	const BOOL ctrlDown =
	    g_keystates[VK_CONTROL] || g_keystates[VK_LCONTROL] || g_keystates[VK_RCONTROL];
	LeaveCriticalSection(&g_event_cs);
	return altDown && ctrlDown;
}

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	DWORD ext_proc_id = 0;

	wfContext* wfc = NULL;
	DWORD rdp_scancode;
	BOOL keystate;
	rdpInput* input;
	PKBDLLHOOKSTRUCT p;

	DEBUG_KBD("Low-level keyboard hook, hWnd %X nCode %X wParam %X", g_focus_hWnd, nCode, wParam);

	if (g_flipping_in)
	{
		if (!alt_ctrl_down())
			g_flipping_in = FALSE;

		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}

	if (g_parent_hWnd && g_main_hWnd)
	{
		wfc = (wfContext*)GetWindowLongPtr(g_main_hWnd, GWLP_USERDATA);
		GUITHREADINFO gui_thread_info;
		gui_thread_info.cbSize = sizeof(GUITHREADINFO);
		HWND fg_win_hwnd = GetForegroundWindow();
		DWORD fg_win_thread_id = GetWindowThreadProcessId(fg_win_hwnd, &ext_proc_id);
		BOOL result = GetGUIThreadInfo(fg_win_thread_id, &gui_thread_info);
		if (gui_thread_info.hwndFocus != wfc->hWndParent)
		{
			g_focus_hWnd = NULL;
			return CallNextHookEx(NULL, nCode, wParam, lParam);
		}

		g_focus_hWnd = g_main_hWnd;
	}

	if (g_focus_hWnd && (nCode == HC_ACTION))
	{
		/* Fix: ensure window handle is still valid to prevent hangs during exit */
		if (!IsWindow(g_focus_hWnd))
		{
			g_focus_hWnd = NULL;
			return CallNextHookEx(NULL, nCode, wParam, lParam);
		}

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

				if (p->vkCode >= 256)
				{
					WLog_Print(TAG, WLOG_DEBUG, "Processing VK code out of tracking range: 0x%08lX",
					           p->vkCode);
				}

				input = wfc->common.context.input;
				if (!input || !input->KeyboardEvent)
					return 1;

				rdp_scancode = MAKE_RDP_SCANCODE((BYTE)p->scanCode, p->flags & LLKHF_EXTENDED);

				/* Fix: keystate tracking was using scan codes, but shortcut detection 
				   uses Virtual Key codes, preventing Ctrl+Alt+Enter from triggering. 
				   Syncing to Virtual Key codes ensures consistent shortcut detection. */
				
				EnterCriticalSection(&g_event_cs);
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
				LeaveCriticalSection(&g_event_cs);

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

void wf_event_focus_in(wfContext* wfc)
{
	UINT16 syncFlags;
	rdpInput* input;
	POINT pt;
	RECT rc;

	/* Fix: added NULL and validity checks to prevent crashes and hangs during focus changes */
	if (!wfc || !wfc->hwnd || !IsWindow(wfc->hwnd))
		return;

	input = wfc->common.context.input;
	if (!input || !input->FocusInEvent || !input->MouseEvent)
		return;

	syncFlags = 0;

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

static BOOL wf_scale_mouse_pos(wfContext* wfc, INT32 x, INT32 y, UINT16* px, UINT16* py)
{
	rdpSettings* settings;
	UINT32 ww, wh, dw, dh;

	WINPR_ASSERT(wfc);
	WINPR_ASSERT(px);
	WINPR_ASSERT(py);

	settings = wfc->common.context.settings;
	WINPR_ASSERT(settings);

	ww = (UINT32)wfc->client_width;
	wh = (UINT32)wfc->client_height;
	dw = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	dh = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);

	if (!freerdp_settings_get_bool(settings, FreeRDP_SmartSizing) || ((ww == dw) && (wh == dh)))
	{
		x += wfc->xCurrentScroll;
		y += wfc->yCurrentScroll;
	}
	else
	{
		x = x * dw / ww + wfc->xCurrentScroll;
		y = y * dh / wh + wfc->yCurrentScroll;
	}

	*px = MIN(UINT16_MAX, MAX(0, x));
	*py = MIN(UINT16_MAX, MAX(0, y));

	return TRUE;
}

static BOOL wf_pub_mouse_event(wfContext* wfc, UINT16 flags, UINT16 x, UINT16 y)
{
	MouseEventEventArgs eventArgs = { 0 };

	eventArgs.flags = flags;
	eventArgs.x = x;
	eventArgs.y = y;
	PubSub_OnMouseEvent(wfc->common.context.pubSub, &wfc->common.context, &eventArgs);
	return TRUE;
}

static BOOL wf_scale_mouse_event(wfContext* wfc, UINT16 flags, INT32 x, INT32 y)
{
	UINT16 px, py;

	WINPR_ASSERT(wfc);

	if (!wf_scale_mouse_pos(wfc, x, y, &px, &py))
		return FALSE;

	/* Fix: correct return value check (TRUE means success). */
	if (!freerdp_client_send_button_event(&wfc->common, FALSE, flags, px, py))
		return FALSE;

	return wf_pub_mouse_event(wfc, flags, px, py);
}

#if (_WIN32_WINNT >= 0x0500)
static BOOL wf_scale_mouse_event_ex(wfContext* wfc, UINT16 flags, UINT16 buttonMask, INT32 x,
                                    INT32 y)
{
	UINT16 px, py;

	WINPR_ASSERT(wfc);

	if (buttonMask & XBUTTON1)
		flags |= PTR_XFLAGS_BUTTON1;

	if (buttonMask & XBUTTON2)
		flags |= PTR_XFLAGS_BUTTON2;

	if (!wf_scale_mouse_pos(wfc, x, y, &px, &py))
		return FALSE;

	/* Fix: correct return value check for extended mouse events. */
	if (!freerdp_client_send_extended_button_event(&wfc->common, FALSE, flags, px, py))
		return FALSE;

	return wf_pub_mouse_event(wfc, flags, px, py);
}
#endif

static BOOL wf_scale_blt(wfContext* wfc, HDC hdc, int x, int y, int w, int h, HDC hdcSrc, int x1,
                         int y1, DWORD rop)
{
	rdpSettings* settings;
	UINT32 ww, wh, dw, dh;

	WINPR_ASSERT(wfc);
	settings = wfc->common.context.settings;
	WINPR_ASSERT(settings);

	ww = (UINT32)wfc->client_width;
	wh = (UINT32)wfc->client_height;
	dw = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	dh = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);

	if (!freerdp_settings_get_bool(settings, FreeRDP_SmartSizing) || ((ww == dw) && (wh == dh)))
	{
		return BitBlt(hdc, x, y, w, h, hdcSrc, x1, y1, rop);
	}
	else
	{
		return StretchBlt(hdc, x * ww / dw, y * wh / dh, w * ww / dw, h * wh / dh, hdcSrc, x, y, w,
		                  h, rop);
	}
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
				    PTR_FLAGS_DOWN | (HIWORD(wParam) == XBUTTON1 ? PTR_FLAGS_BUTTON1 : PTR_XFLAGS_BUTTON2),
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
				wf_event_focus_in(wfc);
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