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
#include <freerdp/freerdp.h>

#include "wf_client.h"

#include "wf_gdi.h"
#include "wf_event.h"

#include <freerdp/event.h>

#include <windowsx.h>

static HWND g_focus_hWnd = NULL;
static HWND g_main_hWnd = NULL;
static HWND g_parent_hWnd = NULL;

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

static BOOL alt_ctrl_down(void)
{
	return ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000));
}

LRESULT CALLBACK wf_ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	DWORD ext_proc_id = 0;

	wfContext* wfc = NULL;
	DWORD rdp_scancode;
	BOOL keystate;
	rdpInput* input;
	PKBDLLHOOKSTRUCT p;

	static BOOL keystates[256] = { 0 };
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
				keystate = keystates[p->scanCode & 0xFF];

				switch (wParam)
				{
					case WM_KEYDOWN:
					case WM_SYSKEYDOWN:
						keystates[p->scanCode & 0xFF] = TRUE;
						break;
					case WM_KEYUP:
					case WM_SYSKEYUP:
					default:
						keystates[p->scanCode & 0xFF] = FALSE;
						break;
				}
				DEBUG_KBD("keydown %d scanCode 0x%08lX flags 0x%08lX vkCode 0x%08lX",
				          (wParam == WM_KEYDOWN), p->scanCode, p->flags, p->vkCode);

				if (wfc->fullscreen_toggle &&
				    ((p->vkCode == VK_RETURN) || (p->vkCode == VK_CANCEL)) &&
				    (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
				    (GetAsyncKeyState(VK_MENU) & 0x8000)) /* could also use flags & LLKHF_ALTDOWN */
				{
					if (wParam == WM_KEYDOWN)
					{
						wf_toggle_fullscreen(wfc);
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
						freerdp_input_send_keyboard_event_ex(input, TRUE, FALSE,
						                                     RDP_SCANCODE_LCONTROL);
						freerdp_input_send_keyboard_event_ex(input, TRUE, FALSE,
						                                     RDP_SCANCODE_NUMLOCK);
						freerdp_input_send_keyboard_event_ex(input, FALSE, FALSE,
						                                     RDP_SCANCODE_LCONTROL);
						freerdp_input_send_keyboard_event_ex(input, FALSE, FALSE,
						                                     RDP_SCANCODE_NUMLOCK);
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

				freerdp_input_send_keyboard_event_ex(input, !(p->flags & LLKHF_UP), keystate,
				                                     rdp_scancode);

				if (p->vkCode == VK_NUMLOCK || p->vkCode == VK_CAPITAL || p->vkCode == VK_SCROLL ||
				    p->vkCode == VK_KANA)
					DEBUG_KBD(
					    "lock keys are processed on client side too to toggle their indicators");
				else
					return 1;

				break;
			default:
				break;
		}
	}

	if (g_flipping_out)
	{
		if (!alt_ctrl_down())
		{
			g_flipping_out = FALSE;
			g_focus_hWnd = NULL;
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
	input = wfc->common.context.input;
	syncFlags = 0;

	if (GetKeyState(VK_NUMLOCK))
		syncFlags |= KBD_SYNC_NUM_LOCK;

	if (GetKeyState(VK_CAPITAL))
		syncFlags |= KBD_SYNC_CAPS_LOCK;

	if (GetKeyState(VK_SCROLL))
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
		/* 9bit twos complement, delta already negative */
		delta = 0x100 + delta;
	}

	flags |= delta;
	return wf_scale_mouse_event(wfc, flags, x, y);
}

static void wf_sizing(wfContext* wfc, WPARAM wParam, LPARAM lParam)
{
	rdpSettings* settings = wfc->common.context.settings;
	// Holding the CTRL key down while resizing the window will force the desktop aspect ratio.
	LPRECT rect;

	if ((settings->SmartSizing || settings->DynamicResolutionUpdate) &&
	    (GetAsyncKeyState(VK_CONTROL) & 0x8000))
	{
		rect = (LPRECT)wParam;

		switch (lParam)
		{
			case WMSZ_LEFT:
			case WMSZ_RIGHT:
			case WMSZ_BOTTOMRIGHT:
				// Adjust height
				rect->bottom = rect->top + settings->DesktopHeight * (rect->right - rect->left) /
				                               settings->DesktopWidth;
				break;

			case WMSZ_TOP:
			case WMSZ_BOTTOM:
			case WMSZ_TOPRIGHT:
				// Adjust width
				rect->right = rect->left + settings->DesktopWidth * (rect->bottom - rect->top) /
				                               settings->DesktopHeight;
				break;

			case WMSZ_BOTTOMLEFT:
			case WMSZ_TOPLEFT:
				// adjust width
				rect->left = rect->right - (settings->DesktopWidth * (rect->bottom - rect->top) /
				                            settings->DesktopHeight);
				break;
		}
	}
}

static void wf_send_resize(wfContext* wfc)
{
	RECT windowRect;
	int targetWidth = wfc->client_width;
	int targetHeight = wfc->client_height;
	rdpSettings* settings = wfc->common.context.settings;

	if (settings->DynamicResolutionUpdate && wfc->disp != NULL)
	{
		if (GetTickCount64() - wfc->lastSentDate > RESIZE_MIN_DELAY)
		{
			if (wfc->fullscreen)
			{
				GetWindowRect(wfc->hwnd, &windowRect);
				targetWidth = windowRect.right - windowRect.left;
				targetHeight = windowRect.bottom - windowRect.top;
			}
			if (settings->SmartSizingWidth != targetWidth ||
			    settings->SmartSizingHeight != targetHeight)
			{
				DISPLAY_CONTROL_MONITOR_LAYOUT layout = { 0 };

				layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
				layout.Top = layout.Left = 0;
				layout.Width = targetWidth;
				layout.Height = targetHeight;
				layout.Orientation = settings->DesktopOrientation;
				layout.DesktopScaleFactor = settings->DesktopScaleFactor;
				layout.DeviceScaleFactor = settings->DeviceScaleFactor;
				layout.PhysicalWidth = targetWidth;
				layout.PhysicalHeight = targetHeight;

				if (IFCALLRESULT(CHANNEL_RC_OK, wfc->disp->SendMonitorLayout, wfc->disp, 1,
				                 &layout) != CHANNEL_RC_OK)
				{
					WLog_ERR("", "SendMonitorLayout failed.");
				}
				settings->SmartSizingWidth = targetWidth;
				settings->SmartSizingHeight = targetHeight;
			}
			wfc->lastSentDate = GetTickCount64();
		}
	}
}

LRESULT CALLBACK wf_event_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	LONG_PTR ptr;
	wfContext* wfc;
	int x, y, w, h;
	PAINTSTRUCT ps;
	BOOL processed;
	RECT windowRect;
	MINMAXINFO* minmax;
	SCROLLINFO si;
	processed = TRUE;
	ptr = GetWindowLongPtr(hWnd, GWLP_USERDATA);
	wfc = (wfContext*)ptr;

	if (wfc != NULL)
	{
		rdpInput* input = wfc->common.context.input;
		rdpSettings* settings = wfc->common.context.settings;

		if (!g_parent_hWnd && wfc->hWndParent)
			g_parent_hWnd = wfc->hWndParent;

		if (!g_main_hWnd)
			g_main_hWnd = wfc->hwnd;

		switch (Msg)
		{
			case WM_MOVE:
				if (!wfc->disablewindowtracking)
				{
					int x = (int)(short)LOWORD(lParam);
					int y = (int)(short)HIWORD(lParam);
					wfc->client_x = x;
					wfc->client_y = y;
				}

				break;

			case WM_GETMINMAXINFO:
				if (wfc->common.context.settings->SmartSizing ||
				    wfc->common.context.settings->DynamicResolutionUpdate)
				{
					processed = FALSE;
				}
				else
				{
					// Set maximum window size for resizing
					minmax = (MINMAXINFO*)lParam;

					// always use the last determined canvas diff, because it could be
					// that the window is minimized when this gets called
					// wf_update_canvas_diff(wfc);

					if (!wfc->fullscreen)
					{
						// add window decoration
						minmax->ptMaxTrackSize.x = settings->DesktopWidth + wfc->diff.x;
						minmax->ptMaxTrackSize.y = settings->DesktopHeight + wfc->diff.y;
					}
				}

				break;

			case WM_SIZING:
				wf_sizing(wfc, lParam, wParam);
				break;

			case WM_SIZE:
				GetWindowRect(wfc->hwnd, &windowRect);

				if (!wfc->fullscreen)
				{
					wfc->client_width = LOWORD(lParam);
					wfc->client_height = HIWORD(lParam);
					wfc->client_x = windowRect.left;
					wfc->client_y = windowRect.top;
				}
				else
				{
					wfc->wasMaximized = TRUE;
					wf_send_resize(wfc);
				}

				if (wfc->client_width && wfc->client_height)
				{
					wf_size_scrollbars(wfc, LOWORD(lParam), HIWORD(lParam));

					// Workaround: when the window is maximized, the call to "ShowScrollBars"
					// returns TRUE but has no effect.
					if (wParam == SIZE_MAXIMIZED && !wfc->fullscreen)
					{
						SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, windowRect.right - windowRect.left,
						             windowRect.bottom - windowRect.top,
						             SWP_NOMOVE | SWP_FRAMECHANGED);
						wfc->wasMaximized = TRUE;
						wf_send_resize(wfc);
					}
					else if (wParam == SIZE_RESTORED && !wfc->fullscreen && wfc->wasMaximized)
					{
						wfc->wasMaximized = FALSE;
						wf_send_resize(wfc);
					}
					else if (wParam == SIZE_MINIMIZED)
					{
						g_focus_hWnd = NULL;
					}
				}

				break;

			case WM_EXITSIZEMOVE:
				wf_size_scrollbars(wfc, wfc->client_width, wfc->client_height);
				wf_send_resize(wfc);
				break;

			case WM_ERASEBKGND:
				/* Say we handled it - prevents flickering */
				return (LRESULT)1;

			case WM_PAINT:
				hdc = BeginPaint(hWnd, &ps);
				x = ps.rcPaint.left;
				y = ps.rcPaint.top;
				w = ps.rcPaint.right - ps.rcPaint.left + 1;
				h = ps.rcPaint.bottom - ps.rcPaint.top + 1;
				wf_scale_blt(wfc, hdc, x, y, w, h, wfc->primary->hdc,
				             x - wfc->offset_x + wfc->xCurrentScroll,
				             y - wfc->offset_y + wfc->yCurrentScroll, SRCCOPY);
				EndPaint(hWnd, &ps);
				break;
#if (_WIN32_WINNT >= 0x0500)

			case WM_XBUTTONDOWN:
				wf_scale_mouse_event_ex(wfc, PTR_XFLAGS_DOWN, GET_XBUTTON_WPARAM(wParam),
				                        GET_X_LPARAM(lParam) - wfc->offset_x,
				                        GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_XBUTTONUP:
				wf_scale_mouse_event_ex(wfc, 0, GET_XBUTTON_WPARAM(wParam),
				                        GET_X_LPARAM(lParam) - wfc->offset_x,
				                        GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;
#endif

			case WM_MBUTTONDOWN:
				wf_scale_mouse_event(wfc, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3,
				                     GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_MBUTTONUP:
				wf_scale_mouse_event(wfc, PTR_FLAGS_BUTTON3, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				break;

			case WM_LBUTTONDOWN:
				wf_scale_mouse_event(wfc, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1,
				                     GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				SetCapture(wfc->hwnd);
				break;

			case WM_LBUTTONUP:
				wf_scale_mouse_event(wfc, PTR_FLAGS_BUTTON1, GET_X_LPARAM(lParam) - wfc->offset_x,
				                     GET_Y_LPARAM(lParam) - wfc->offset_y);
				ReleaseCapture();
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
				int xDelta;  // xDelta = new_pos - current_pos
				int xNewPos; // new position
				int yDelta = 0;

				switch (LOWORD(wParam))
				{
					// User clicked the scroll bar shaft left of the scroll box.
					case SB_PAGEUP:
						xNewPos = wfc->xCurrentScroll - 50;
						break;

					// User clicked the scroll bar shaft right of the scroll box.
					case SB_PAGEDOWN:
						xNewPos = wfc->xCurrentScroll + 50;
						break;

					// User clicked the left arrow.
					case SB_LINEUP:
						xNewPos = wfc->xCurrentScroll - 5;
						break;

					// User clicked the right arrow.
					case SB_LINEDOWN:
						xNewPos = wfc->xCurrentScroll + 5;
						break;

					// User dragged the scroll box.
					case SB_THUMBPOSITION:
						xNewPos = HIWORD(wParam);
						break;

					// user is dragging the scrollbar
					case SB_THUMBTRACK:
						xNewPos = HIWORD(wParam);
						break;

					default:
						xNewPos = wfc->xCurrentScroll;
				}

				// New position must be between 0 and the screen width.
				xNewPos = MAX(0, xNewPos);
				xNewPos = MIN(wfc->xMaxScroll, xNewPos);

				// If the current position does not change, do not scroll.
				if (xNewPos == wfc->xCurrentScroll)
					break;

				// Determine the amount scrolled (in pixels).
				xDelta = xNewPos - wfc->xCurrentScroll;
				// Reset the current scroll position.
				wfc->xCurrentScroll = xNewPos;
				// Scroll the window. (The system repaints most of the
				// client area when ScrollWindowEx is called; however, it is
				// necessary to call UpdateWindow in order to repaint the
				// rectangle of pixels that were invalidated.)
				ScrollWindowEx(wfc->hwnd, -xDelta, -yDelta, (CONST RECT*)NULL, (CONST RECT*)NULL,
				               (HRGN)NULL, (PRECT)NULL, SW_INVALIDATE);
				UpdateWindow(wfc->hwnd);
				// Reset the scroll bar.
				si.cbSize = sizeof(si);
				si.fMask = SIF_POS;
				si.nPos = wfc->xCurrentScroll;
				SetScrollInfo(wfc->hwnd, SB_HORZ, &si, TRUE);
			}
			break;

			case WM_VSCROLL:
			{
				int xDelta = 0;
				int yDelta;  // yDelta = new_pos - current_pos
				int yNewPos; // new position

				switch (LOWORD(wParam))
				{
					// User clicked the scroll bar shaft above the scroll box.
					case SB_PAGEUP:
						yNewPos = wfc->yCurrentScroll - 50;
						break;

					// User clicked the scroll bar shaft below the scroll box.
					case SB_PAGEDOWN:
						yNewPos = wfc->yCurrentScroll + 50;
						break;

					// User clicked the top arrow.
					case SB_LINEUP:
						yNewPos = wfc->yCurrentScroll - 5;
						break;

					// User clicked the bottom arrow.
					case SB_LINEDOWN:
						yNewPos = wfc->yCurrentScroll + 5;
						break;

					// User dragged the scroll box.
					case SB_THUMBPOSITION:
						yNewPos = HIWORD(wParam);
						break;

					// user is dragging the scrollbar
					case SB_THUMBTRACK:
						yNewPos = HIWORD(wParam);
						break;

					default:
						yNewPos = wfc->yCurrentScroll;
				}

				// New position must be between 0 and the screen height.
				yNewPos = MAX(0, yNewPos);
				yNewPos = MIN(wfc->yMaxScroll, yNewPos);

				// If the current position does not change, do not scroll.
				if (yNewPos == wfc->yCurrentScroll)
					break;

				// Determine the amount scrolled (in pixels).
				yDelta = yNewPos - wfc->yCurrentScroll;
				// Reset the current scroll position.
				wfc->yCurrentScroll = yNewPos;
				// Scroll the window. (The system repaints most of the
				// client area when ScrollWindowEx is called; however, it is
				// necessary to call UpdateWindow in order to repaint the
				// rectangle of pixels that were invalidated.)
				ScrollWindowEx(wfc->hwnd, -xDelta, -yDelta, (CONST RECT*)NULL, (CONST RECT*)NULL,
				               (HRGN)NULL, (PRECT)NULL, SW_INVALIDATE);
				UpdateWindow(wfc->hwnd);
				// Reset the scroll bar.
				si.cbSize = sizeof(si);
				si.fMask = SIF_POS;
				si.nPos = wfc->yCurrentScroll;
				SetScrollInfo(wfc->hwnd, SB_VERT, &si, TRUE);
			}
			break;

			case WM_SYSCOMMAND:
			{
				if (wParam == SYSCOMMAND_ID_SMARTSIZING)
				{
					HMENU hMenu = GetSystemMenu(wfc->hwnd, FALSE);
					freerdp_settings_set_bool(wfc->common.context.settings, FreeRDP_SmartSizing,
					                          !wfc->common.context.settings->SmartSizing);
					CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING,
					              wfc->common.context.settings->SmartSizing ? MF_CHECKED
					                                                        : MF_UNCHECKED);
					if (!wfc->common.context.settings->SmartSizing)
					{
						SetWindowPos(wfc->hwnd, HWND_TOP, -1, -1,
						             wfc->common.context.settings->DesktopWidth + wfc->diff.x,
						             wfc->common.context.settings->DesktopHeight + wfc->diff.y,
						             SWP_NOMOVE);
					}
					else
					{
						wf_size_scrollbars(wfc, wfc->client_width, wfc->client_height);
						wf_send_resize(wfc);
					}
				}
				else if (wParam == SYSCOMMAND_ID_REQUEST_CONTROL)
				{
					freerdp_client_encomsp_set_control(wfc->common.encomsp, TRUE);
				}
				else
				{
					processed = FALSE;
				}
			}
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

		case WM_SETFOCUS:
			DEBUG_KBD("getting focus %X", hWnd);

			freerdp_settings_set_bool(wfc->common.context.settings, FreeRDP_SuspendInput, FALSE);

			if (alt_ctrl_down())
				g_flipping_in = TRUE;

			g_focus_hWnd = hWnd;
			freerdp_set_focus(wfc->common.context.instance);
			break;

		case WM_KILLFOCUS:
			freerdp_settings_set_bool(wfc->common.context.settings, FreeRDP_SuspendInput, TRUE);

			if (g_focus_hWnd == hWnd && wfc && !wfc->fullscreen)
			{
				DEBUG_KBD("loosing focus %X", hWnd);

				if (alt_ctrl_down())
					g_flipping_out = TRUE;
				else
					g_focus_hWnd = NULL;
			}

			break;

		case WM_ACTIVATE:
		{
			int activate = (int)(short)LOWORD(wParam);

			if (activate != WA_INACTIVE)
			{
				if (alt_ctrl_down())
					g_flipping_in = TRUE;

				g_focus_hWnd = hWnd;
			}
			else
			{
				if (alt_ctrl_down())
					g_flipping_out = TRUE;
				else
					g_focus_hWnd = NULL;
			}
		}

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
			break;
	}

	return 0;
}

BOOL wf_scale_blt(wfContext* wfc, HDC hdc, int x, int y, int w, int h, HDC hdcSrc, int x1, int y1,
                  DWORD rop)
{
	rdpSettings* settings;
	UINT32 ww, wh, dw, dh;
	WINPR_ASSERT(wfc);

	settings = wfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (!wfc->client_width)
		wfc->client_width = settings->DesktopWidth;

	if (!wfc->client_height)
		wfc->client_height = settings->DesktopHeight;

	ww = wfc->client_width;
	wh = wfc->client_height;
	dw = settings->DesktopWidth;
	dh = settings->DesktopHeight;

	if (!ww)
		ww = dw;

	if (!wh)
		wh = dh;

	if (wfc->fullscreen || !settings->SmartSizing || (ww == dw && wh == dh))
	{
		return BitBlt(hdc, x, y, w, h, wfc->primary->hdc, x1, y1, SRCCOPY);
	}
	else
	{
		SetStretchBltMode(hdc, HALFTONE);
		SetBrushOrgEx(hdc, 0, 0, NULL);
		return StretchBlt(hdc, 0, 0, ww, wh, wfc->primary->hdc, 0, 0, dw, dh, SRCCOPY);
	}

	return TRUE;
}

static BOOL wf_scale_mouse_pos(wfContext* wfc, INT32 x, INT32 y, UINT16* px, UINT16* py)
{
	int ww, wh, dw, dh;
	rdpSettings* settings;

	if (!wfc || !px || !py)
		return FALSE;

	settings = wfc->common.context.settings;

	if (!settings)
		return FALSE;

	if (!wfc->client_width)
		wfc->client_width = settings->DesktopWidth;

	if (!wfc->client_height)
		wfc->client_height = settings->DesktopHeight;

	ww = wfc->client_width;
	wh = wfc->client_height;
	dw = settings->DesktopWidth;
	dh = settings->DesktopHeight;

	if (!settings->SmartSizing || ((ww == dw) && (wh == dh)))
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

	if (freerdp_client_send_button_event(&wfc->common, FALSE, flags, px, py))
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

	if (freerdp_client_send_extended_button_event(&wfc->common, FALSE, flags, px, py))
		return FALSE;

	return wf_pub_mouse_event(wfc, flags, px, py);
}
#endif
