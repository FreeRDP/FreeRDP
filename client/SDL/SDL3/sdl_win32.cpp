/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - native Win32 helpers for RAIL
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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
#include "sdl_win32.hpp"
#include "sdl_utils.hpp"

#include <algorithm>
#include <mutex>
#include <tuple>

#include <winpr/library.h>
#include <winpr/platform.h>
#include <freerdp/log.h>

#if defined(_WIN32)

#define TAG CLIENT_TAG("sdl.win32")

#include <windows.h>
#include <windowsx.h>

/* Window currently in a native size/move loop. */
static HWND g_sizeMoveHwnd = nullptr;
/* WMSZ_* edge of the running sizing loop, 0 = none or not reported yet. */
static int g_sizingEdge = 0;
/* Set on the RDP thread (channel init), read by the window procedure on the main thread. */
static std::mutex g_sizeMoveLock;
static SdlWin32SizeMoveHandler g_sizeMove;
static const wchar_t kFrameProp[] = L"FreeRDP.RailNativeFrame";
static const wchar_t kIdProp[] = L"FreeRDP.RailWindowId";

[[nodiscard]] static SdlWin32SizeMoveHandler sdl_win32_size_move_handler()
{
	std::scoped_lock lock(g_sizeMoveLock);
	return g_sizeMove;
}

[[nodiscard]] static SDL_WindowID sdl_win32_window_id(HWND hwnd)
{
	return static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(GetPropW(hwnd, kIdProp)));
}

[[nodiscard]] static LRESULT sdl_win32_frame_next(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto next = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kFrameProp));
	return next ? CallWindowProcW(next, hwnd, msg, wParam, lParam)
	            : DefWindowProcW(hwnd, msg, wParam, lParam);
}

[[nodiscard]] static HWND sdl_win32_hwnd(SDL_Window* window)
{
	if (!window || !sdl::utils::isWindowsDriver())
		return nullptr;
	return static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),
	                                                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
}

bool sdl_win32_restack_windows(const std::vector<SDL_Window*>& topToBottom)
{
	struct Entry
	{
		HWND hwnd;
		bool topmost;
	};
	std::vector<Entry> stack;
	stack.reserve(topToBottom.size());
	for (auto* window : topToBottom)
	{
		HWND hwnd = sdl_win32_hwnd(window);
		if (hwnd)
			stack.push_back({ hwnd, (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0 });
	}
	if (stack.size() < 2)
		return false;

	HDWP dwp = BeginDeferWindowPos(static_cast<int>(stack.size() - 1));
	if (!dwp)
		return false;
	/* Restack within each band: below a topmost window, a window becomes topmost too. */
	HWND prevTopmost = nullptr;
	HWND prevNormal = nullptr;
	for (const auto& e : stack)
	{
		HWND& prev = e.topmost ? prevTopmost : prevNormal;
		if (prev)
		{
			dwp = DeferWindowPos(dwp, e.hwnd, prev, 0, 0, 0, 0,
			                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
			if (!dwp)
				return false;
		}
		prev = e.hwnd;
	}
	return EndDeferWindowPos(dwp) != FALSE;
}

bool sdl_win32_can_move_size(SDL_Window* window, bool move, int wmszEdge)
{
	return sdl_win32_hwnd(window) &&
	       (move || ((wmszEdge >= WMSZ_LEFT) && (wmszEdge <= WMSZ_BOTTOMRIGHT)));
}

void sdl_win32_run_move_size(SDL_Window* window, bool move, int wmszEdge)
{
	HWND hwnd = sdl_win32_hwnd(window);
	if (!hwnd)
		return;
	/* Released already (a quick click): the loop would wait in keyboard mode instead. */
	const int button = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
	if (GetAsyncKeyState(button) >= 0)
		return;
	/* The press captured the mouse for SDL; the loop needs it. */
	if (!ReleaseCapture())
		WLog_DBG(TAG, "ReleaseCapture failed: %" PRIu32, static_cast<uint32_t>(GetLastError()));
	const WPARAM cmd = move ? (SC_MOVE | HTCAPTION) : (SC_SIZE | static_cast<WPARAM>(wmszEdge));
	SendMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
}

[[nodiscard]] static bool sdl_win32_frame_insets(HWND hwnd, RECT& wr, RECT& insets)
{
	RECT cr = {};
	POINT origin = {};
	if (!GetWindowRect(hwnd, &wr) || !GetClientRect(hwnd, &cr) || !ClientToScreen(hwnd, &origin))
		return false;
	insets.left = origin.x - wr.left;
	insets.top = origin.y - wr.top;
	insets.right = (wr.right - wr.left) - insets.left - cr.right;
	insets.bottom = (wr.bottom - wr.top) - insets.top - cr.bottom;
	return true;
}

static LRESULT CALLBACK sdl_win32_frame_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		/* DWM draws the frame; theme painting would show as glass where the top frame was. */
		case WM_NCPAINT:
			return 0;
		case WM_NCACTIVATE:
			return sdl_win32_frame_next(hwnd, msg, wParam, -1);
		case WM_NCCALCSIZE:
			/* DWM draws the top sizing frame of a captionless window: keep only the others. */
			if (wParam && !IsZoomed(hwnd))
			{
				auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				const LONG top = params->rgrc[0].top;
				const LRESULT rc = sdl_win32_frame_next(hwnd, msg, wParam, lParam);
				params->rgrc[0].top = top;
				return rc;
			}
			break;
		case WM_NCHITTEST:
		{
			/* Native top-edge hit test without caption. */
			const LRESULT hit = sdl_win32_frame_next(hwnd, msg, wParam, lParam);
			if (IsZoomed(hwnd) || ((hit != HTCLIENT) && (hit != HTLEFT) && (hit != HTRIGHT)))
				return hit;
			RECT wr = {};
			RECT in = {};
			const int frame = sdl_win32_frame_insets(hwnd, wr, in) ? in.left : 0;
			if (frame <= 0)
				return hit;
			const int x = GET_X_LPARAM(lParam);
			if (GET_Y_LPARAM(lParam) >= wr.top + frame)
				return hit;
			if ((hit == HTLEFT) || (x < wr.left + 2 * frame))
				return HTTOPLEFT;
			return ((hit == HTRIGHT) || (x >= wr.right - 2 * frame)) ? HTTOPRIGHT : HTTOP;
		}
		case WM_GETMINMAXINFO:
		{
			/* SDL adds a top sizing frame to the track sizes, but WM_NCCALCSIZE drops it. */
			const LRESULT rc = sdl_win32_frame_next(hwnd, msg, wParam, lParam);
			RECT wr = {};
			RECT in = {};
			if (!IsZoomed(hwnd) && sdl_win32_frame_insets(hwnd, wr, in) && (in.top == 0))
			{
				auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
				info->ptMinTrackSize.y = std::max<LONG>(info->ptMinTrackSize.y - in.bottom, 0);
				/* Without a maximum size the maximized window must fit the system default. */
				int maxW = 0;
				int maxH = 0;
				SDL_Window* window = SDL_GetWindowFromID(sdl_win32_window_id(hwnd));
				if (window && SDL_GetWindowMaximumSize(window, &maxW, &maxH) && (maxH > 0))
					info->ptMaxTrackSize.y -= in.bottom;
			}
			return rc;
		}
		case WM_STYLECHANGING:
			/* Keep the sizing frame, drop the caption: the server draws the title bar. */
			if (wParam == static_cast<WPARAM>(GWL_STYLE))
				reinterpret_cast<STYLESTRUCT*>(lParam)->styleNew &= ~WS_CAPTION;
			break;
		case WM_ENTERSIZEMOVE:
			g_sizeMoveHwnd = hwnd;
			g_sizingEdge = 0;
			break;
		case WM_SIZING:
		{
			const auto handler = sdl_win32_size_move_handler();
			if ((g_sizingEdge == 0) && handler.sizing)
			{
				g_sizingEdge = static_cast<int>(wParam);
				handler.sizing(handler.userdata, sdl_win32_window_id(hwnd), g_sizingEdge, false);
			}
			break;
		}
		case WM_TIMER:
			/* Redraw from the loop timer: SDL sends expose under its event watch lock. */
			if (hwnd == g_sizeMoveHwnd)
			{
				const auto handler = sdl_win32_size_move_handler();
				const LRESULT rc = sdl_win32_frame_next(hwnd, msg, wParam, lParam);
				if (handler.redraw)
					handler.redraw(handler.userdata);
				return rc;
			}
			break;
		case WM_EXITSIZEMOVE:
		{
			g_sizeMoveHwnd = nullptr;
			const LRESULT rc = sdl_win32_frame_next(hwnd, msg, wParam, lParam);
			const int edge = g_sizingEdge;
			g_sizingEdge = 0;
			const auto handler = sdl_win32_size_move_handler();
			if ((edge != 0) && handler.sizing)
				handler.sizing(handler.userdata, sdl_win32_window_id(hwnd), edge, true);
			return rc;
		}
		case WM_NCDESTROY:
		{
			/* Destroyed inside its loop: no WM_EXITSIZEMOVE follows. */
			if (hwnd == g_sizeMoveHwnd)
			{
				g_sizeMoveHwnd = nullptr;
				g_sizingEdge = 0;
			}
			RemovePropW(hwnd, kIdProp);
			auto next = reinterpret_cast<WNDPROC>(RemovePropW(hwnd, kFrameProp));
			if (next && !SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(next)))
				WLog_WARN(TAG, "restoring the window procedure failed: %" PRIu32,
				          static_cast<uint32_t>(GetLastError()));
			return next ? CallWindowProcW(next, hwnd, msg, wParam, lParam)
			            : DefWindowProcW(hwnd, msg, wParam, lParam);
		}
		default:
			break;
	}
	return sdl_win32_frame_next(hwnd, msg, wParam, lParam);
}

/* Disable Win11 rounded corners and borders (drawn by server). */
static void sdl_win32_plain_frame(HWND hwnd)
{
	using SetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
	static const auto setAttr = []
	{
		HMODULE dwm = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
		return dwm ? GetProcAddressAs(dwm, "DwmSetWindowAttribute", SetAttrFn) : nullptr;
	}();
	if (!setAttr)
		return;
	const DWORD none = 0xFFFFFFFE; /* DWMWA_COLOR_NONE */
	const DWORD borderColor = 34;  /* DWMWA_BORDER_COLOR */
	if (FAILED(setAttr(hwnd, borderColor, &none, sizeof(none))))
		WLog_DBG(TAG, "DWMWA_BORDER_COLOR not supported");
	const DWORD square = 1;      /* DWMWCP_DONOTROUND */
	const DWORD cornerPref = 33; /* DWMWA_WINDOW_CORNER_PREFERENCE */
	if (FAILED(setAttr(hwnd, cornerPref, &square, sizeof(square))))
		WLog_DBG(TAG, "DWMWA_WINDOW_CORNER_PREFERENCE not supported");
}

bool sdl_win32_use_native_frame(SDL_Window* window)
{
	HWND hwnd = sdl_win32_hwnd(window);
	if (!hwnd)
		return false;
	if (!GetPropW(hwnd, kFrameProp))
	{
		/* Window procedure chaining without comctl32 v6 dependency. */
		const auto next =
		    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(sdl_win32_frame_proc));
		if (!next)
			return false;
		if (!SetPropW(hwnd, kFrameProp, reinterpret_cast<HANDLE>(next)))
		{
			if (!SetWindowLongPtrW(hwnd, GWLP_WNDPROC, next))
				WLog_WARN(TAG, "restoring the window procedure failed: %" PRIu32,
				          static_cast<uint32_t>(GetLastError()));
			return false;
		}
		const auto id = static_cast<uintptr_t>(SDL_GetWindowID(window));
		if (!SetPropW(hwnd, kIdProp, reinterpret_cast<HANDLE>(id)))
			WLog_WARN(TAG, "SetPropW failed: %" PRIu32, static_cast<uint32_t>(GetLastError()));
	}
	sdl_win32_plain_frame(hwnd);
	SetLastError(ERROR_SUCCESS);
	if (!SetWindowLongPtr(hwnd, GWL_STYLE, GetWindowLongPtr(hwnd, GWL_STYLE) & ~WS_CAPTION) &&
	    (GetLastError() != ERROR_SUCCESS))
		return false;
	return SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
	                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
	                        SWP_NOACTIVATE) != FALSE;
}

void sdl_win32_set_size_move_handler(const SdlWin32SizeMoveHandler& handler)
{
	std::scoped_lock lock(g_sizeMoveLock);
	g_sizeMove = handler;
}

bool sdl_win32_set_client_rect(SDL_Window* window, const SDL_Rect& rect)
{
	HWND hwnd = sdl_win32_hwnd(window);
	if (!hwnd || !GetPropW(hwnd, kFrameProp))
		return false;
	/* SDL assumes a standard frame; take the real one from the window itself. */
	RECT wr = {};
	RECT in = {};
	if (!sdl_win32_frame_insets(hwnd, wr, in))
		return false;
	return SetWindowPos(hwnd, nullptr, rect.x - in.left, rect.y - in.top,
	                    rect.w + in.left + in.right, rect.h + in.top + in.bottom,
	                    SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

bool sdl_win32_send_left_button_release(SDL_Window* window)
{
	HWND hwnd = sdl_win32_hwnd(window);
	POINT pt = {};
	if (!hwnd || !GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt))
		return false;
	const LPARAM pos = MAKELPARAM(static_cast<WORD>(pt.x), static_cast<WORD>(pt.y));
	return PostMessage(hwnd, WM_LBUTTONUP, 0, pos) != FALSE;
}

#else /* !_WIN32 */

bool sdl_win32_restack_windows(WINPR_ATTR_UNUSED const std::vector<SDL_Window*>& topToBottom)
{
	return false;
}

bool sdl_win32_can_move_size(WINPR_ATTR_UNUSED SDL_Window* window, WINPR_ATTR_UNUSED bool move,
                             WINPR_ATTR_UNUSED int wmszEdge)
{
	return false;
}

void sdl_win32_run_move_size(WINPR_ATTR_UNUSED SDL_Window* window, WINPR_ATTR_UNUSED bool move,
                             WINPR_ATTR_UNUSED int wmszEdge)
{
}

bool sdl_win32_use_native_frame(WINPR_ATTR_UNUSED SDL_Window* window)
{
	return false;
}

void sdl_win32_set_size_move_handler(WINPR_ATTR_UNUSED const SdlWin32SizeMoveHandler& handler)
{
}

bool sdl_win32_set_client_rect(WINPR_ATTR_UNUSED SDL_Window* window,
                               WINPR_ATTR_UNUSED const SDL_Rect& rect)
{
	return false;
}

bool sdl_win32_send_left_button_release(WINPR_ATTR_UNUSED SDL_Window* window)
{
	return false;
}

#endif
