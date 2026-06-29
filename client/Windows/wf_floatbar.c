/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Float Bar
 *
 * Copyright 2013 Zhang Zhaolong <zhangzl2013@126.com>
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

#include <winpr/crt.h>
#include <winpr/windows.h>

#include "wf_client.h"
#include "wf_floatbar.h"

#include "resource/resource.h"
#include "wf_gdi.h"
#ifdef _MSC_VER
#pragma comment(lib, "Msimg32.lib")
#endif

#define TAG CLIENT_TAG("windows.floatbar")

/* TIMERs */
#define TIMER_HIDE 1
#define TIMER_ANIMAT_SHOW 2
#define TIMER_ANIMAT_HIDE 3

/* Button Type */
#define BUTTON_LOCKPIN 0
#define BUTTON_MINIMIZE 1
#define BUTTON_RESTORE 2
#define BUTTON_CLOSE 3
#define BTN_MAX 4

/* bmp size */
#define BACKGROUND_W 576
#define BACKGROUND_H 27
#define BUTTON_OFFSET 5
#define BUTTON_Y 2
#define BUTTON_WIDTH 23
#define BUTTON_HEIGHT 21
#define BUTTON_SPACING 1
#define RESTORE_DRAG_BAR_MULTIPLIER 2
#define RESIZE_GRIP_WIDTH 8
#define MIN_LABEL_WIDTH 260

#define LOCK_X (BACKGROUND_H + BUTTON_OFFSET)
#define CLOSE_X ((BACKGROUND_W - (BACKGROUND_H + BUTTON_OFFSET)) - BUTTON_WIDTH)
#define RESTORE_X (CLOSE_X - (BUTTON_WIDTH + BUTTON_SPACING))
#define MINIMIZE_X (RESTORE_X - (BUTTON_WIDTH + BUTTON_SPACING))
#define TEXT_X (BACKGROUND_H + ((BUTTON_WIDTH + BUTTON_SPACING) * 3) + 5)
#define MIN_FLOATBAR_WIDTH ((TEXT_X * 2) + MIN_LABEL_WIDTH)

#define DRAG_NONE 0
#define DRAG_MOVE 1
#define DRAG_RESIZE_LEFT 2
#define DRAG_RESIZE_RIGHT 3

typedef struct
{
	wfFloatBar* floatbar;
	int type;
	int x, y, h, w;
	int active;
	HBITMAP bmp;
	HBITMAP bmp_act;

	/* Lock Specified */
	HBITMAP locked_bmp;
	HBITMAP locked_bmp_act;
	HBITMAP unlocked_bmp;
	HBITMAP unlocked_bmp_act;
} Button;

struct s_FloatBar
{
	HINSTANCE root_window;
	DWORD flags;
	HWND parent;
	HWND hwnd;
	RECT rect;
	LONG width;
	LONG height;
	LONG offset;
	wfContext* wfc;
	Button* buttons[BTN_MAX];
	BOOL shown;
	BOOL locked;
	HDC hdcmem;
	RECT textRect;
	UINT_PTR animating;
};

static BOOL floatbar_kill_timers(wfFloatBar* floatbar)
{
	UINT_PTR timers[] = { TIMER_HIDE, TIMER_ANIMAT_HIDE, TIMER_ANIMAT_SHOW };

	if (!floatbar)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(timers); x++)
		KillTimer(floatbar->hwnd, timers[x]);

	floatbar->animating = 0;
	return TRUE;
}

static BOOL floatbar_animation(wfFloatBar* const floatbar, const BOOL show)
{
	UINT_PTR timer = show ? TIMER_ANIMAT_SHOW : TIMER_ANIMAT_HIDE;

	if (!floatbar)
		return FALSE;

	if (floatbar->shown == show)
		return TRUE;

	if (floatbar->animating == timer)
		return TRUE;

	floatbar->animating = timer;

	if (SetTimer(floatbar->hwnd, timer, USER_TIMER_MINIMUM, nullptr) == 0)
	{
		DWORD err = GetLastError();
		WLog_ERR(TAG, "SetTimer failed with %08" PRIx32, err);
		return FALSE;
	}

	return TRUE;
}

static BOOL floatbar_trigger_hide(wfFloatBar* floatbar)
{
	if (!floatbar_kill_timers(floatbar))
		return FALSE;

	if (!floatbar->locked && floatbar->shown)
	{
		if (SetTimer(floatbar->hwnd, TIMER_HIDE, 3000, nullptr) == 0)
		{
			DWORD err = GetLastError();
			WLog_ERR(TAG, "SetTimer failed with %08" PRIx32, err);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL floatbar_hide(wfFloatBar* floatbar)
{
	if (!floatbar_kill_timers(floatbar))
		return FALSE;

	floatbar->offset = floatbar->height - 2;

	if (!MoveWindow(floatbar->hwnd, floatbar->rect.left, -floatbar->offset, floatbar->width,
	                floatbar->height, TRUE))
	{
		DWORD err = GetLastError();
		WLog_ERR(TAG, "MoveWindow failed with %08" PRIx32, err);
		return FALSE;
	}

	floatbar->shown = FALSE;

	if (!floatbar_trigger_hide(floatbar))
		return FALSE;

	return TRUE;
}

static BOOL floatbar_show(wfFloatBar* floatbar)
{
	if (!floatbar_kill_timers(floatbar))
		return FALSE;

	floatbar->offset = 0;

	if (!MoveWindow(floatbar->hwnd, floatbar->rect.left, -floatbar->offset, floatbar->width,
	                floatbar->height, TRUE))
	{
		DWORD err = GetLastError();
		WLog_ERR(TAG, "MoveWindow failed with %08" PRIx32, err);
		return FALSE;
	}

	floatbar->shown = TRUE;

	if (!floatbar_trigger_hide(floatbar))
		return FALSE;

	return TRUE;
}

static BOOL button_set_locked(Button* button, BOOL locked)
{
	if (locked)
	{
		button->bmp = button->locked_bmp;
		button->bmp_act = button->locked_bmp_act;
	}
	else
	{
		button->bmp = button->unlocked_bmp;
		button->bmp_act = button->unlocked_bmp_act;
	}

	InvalidateRect(button->floatbar->hwnd, nullptr, FALSE);
	UpdateWindow(button->floatbar->hwnd);
	return TRUE;
}

static BOOL update_locked_state(wfFloatBar* floatbar)
{
	Button* button;

	if (!floatbar)
		return FALSE;

	button = floatbar->buttons[3];

	if (!button_set_locked(button, floatbar->locked))
		return FALSE;

	return TRUE;
}

static int button_hit(Button* const button)
{
	wfFloatBar* const floatbar = button->floatbar;

	switch (button->type)
	{
		case BUTTON_LOCKPIN:
			floatbar->locked = !floatbar->locked;
			update_locked_state(floatbar);
			break;

		case BUTTON_MINIMIZE:
			ShowWindow(floatbar->parent, SW_MINIMIZE);
			break;

		case BUTTON_RESTORE:
			wf_toggle_fullscreen(floatbar->wfc);
			break;

		case BUTTON_CLOSE:
			SendMessage(floatbar->parent, WM_DESTROY, 0, 0);
			break;

		default:
			return 0;
	}

	return 0;
}

static int button_paint(const Button* const button, const HDC hdc)
{
	if (button != nullptr)
	{
		wfFloatBar* floatbar = button->floatbar;
		BLENDFUNCTION bf;
		SelectObject(floatbar->hdcmem, button->active ? button->bmp_act : button->bmp);
		bf.BlendOp = AC_SRC_OVER;
		bf.BlendFlags = 0;
		bf.SourceConstantAlpha = 255;
		bf.AlphaFormat = AC_SRC_ALPHA;
		AlphaBlend(hdc, button->x, button->y, button->w, button->h, floatbar->hdcmem, 0, 0,
		           button->w, button->h, bf);
	}

	return 0;
}

static Button* floatbar_create_button(wfFloatBar* const floatbar, const int type, const int resid,
                                      const int resid_act, const int x, const int y, const int h,
                                      const int w)
{
	Button* button = (Button*)calloc(1, sizeof(Button));

	if (!button)
		return nullptr;

	button->floatbar = floatbar;
	button->type = type;
	button->x = x;
	button->y = y;
	button->w = w;
	button->h = h;
	button->active = FALSE;
	button->bmp = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(resid), IMAGE_BITMAP, 0,
	                                 0, LR_DEFAULTCOLOR);
	button->bmp_act = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(resid_act),
	                                     IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	return button;
}

static Button* floatbar_create_lock_button(wfFloatBar* const floatbar, const int unlock_resid,
                                           const int unlock_resid_act, const int lock_resid,
                                           const int lock_resid_act, const int x, const int y,
                                           const int h, const int w)
{
	Button* button = floatbar_create_button(floatbar, BUTTON_LOCKPIN, unlock_resid,
	                                        unlock_resid_act, x, y, h, w);

	if (!button)
		return nullptr;

	button->unlocked_bmp = button->bmp;
	button->unlocked_bmp_act = button->bmp_act;
	button->locked_bmp = (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(lock_resid),
	                                        IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	button->locked_bmp_act =
	    (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(lock_resid_act), IMAGE_BITMAP,
	                       0, 0, LR_DEFAULTCOLOR);
	return button;
}

static Button* floatbar_get_button(const wfFloatBar* const floatbar, const int x, const int y)
{
	if ((y > BUTTON_Y) && (y < BUTTON_Y + BUTTON_HEIGHT))
	{
		for (int i = 0; i < BTN_MAX; i++)
		{
			if ((floatbar->buttons[i] != nullptr) && (x > floatbar->buttons[i]->x) &&
			    (x < floatbar->buttons[i]->x + floatbar->buttons[i]->w))
			{
				return floatbar->buttons[i];
			}
		}
	}

	return nullptr;
}

static Button* floatbar_get_button_by_type(const wfFloatBar* const floatbar, const int type)
{
	if (!floatbar)
		return nullptr;

	for (int i = 0; i < BTN_MAX; i++)
	{
		Button* button = floatbar->buttons[i];

		if (button && (button->type == type))
			return button;
	}

	return nullptr;
}

static LONG floatbar_parent_width(const wfFloatBar* const floatbar)
{
	RECT rect = WINPR_C_ARRAY_INIT;

	if (!floatbar || !floatbar->parent || !GetWindowRect(floatbar->parent, &rect))
		return GetSystemMetrics(SM_CXSCREEN);

	return rect.right - rect.left;
}

static BOOL floatbar_update_layout(wfFloatBar* floatbar)
{
	if (!floatbar)
		return FALSE;

	Button* close = floatbar_get_button_by_type(floatbar, BUTTON_CLOSE);
	Button* restore = floatbar_get_button_by_type(floatbar, BUTTON_RESTORE);
	Button* minimize = floatbar_get_button_by_type(floatbar, BUTTON_MINIMIZE);

	if (close)
		close->x = (floatbar->width - (BACKGROUND_H + BUTTON_OFFSET)) - BUTTON_WIDTH;

	if (restore && close)
		restore->x = close->x - (BUTTON_WIDTH + BUTTON_SPACING);

	if (minimize && restore)
		minimize->x = restore->x - (BUTTON_WIDTH + BUTTON_SPACING);

	SetRect(&floatbar->textRect, TEXT_X, 0, floatbar->width - TEXT_X, floatbar->height);
	return TRUE;
}

static BOOL floatbar_update_region(wfFloatBar* floatbar)
{
	if (!floatbar || !floatbar->hwnd)
		return FALSE;

	HRGN hRgn = CreateRectRgn(0, 0, floatbar->width, floatbar->height);
	if (!hRgn)
		return FALSE;

	if (!SetWindowRgn(floatbar->hwnd, hRgn, TRUE))
	{
		DeleteObject(hRgn);
		return FALSE;
	}

	return TRUE;
}

static BOOL floatbar_resize(wfFloatBar* floatbar, LONG left, LONG width)
{
	if (!floatbar)
		return FALSE;

	LONG parentWidth = floatbar_parent_width(floatbar);

	if (width < MIN_FLOATBAR_WIDTH)
		width = MIN_FLOATBAR_WIDTH;

	if (width > parentWidth)
		width = parentWidth;

	if (left < 0)
	{
		width += left;
		left = 0;
	}

	if (left + width > parentWidth)
		width = parentWidth - left;

	if (width < MIN_FLOATBAR_WIDTH)
	{
		width = MIN_FLOATBAR_WIDTH;
		left = parentWidth - width;

		if (left < 0)
			left = 0;
	}

	floatbar->rect.left = left;
	floatbar->width = width;

	if (!floatbar_update_layout(floatbar))
		return FALSE;

	if (!MoveWindow(floatbar->hwnd, floatbar->rect.left,
	                -WINPR_ASSERTING_INT_CAST(int, floatbar->offset), floatbar->width,
	                floatbar->height, TRUE))
	{
		DWORD err = GetLastError();
		WLog_ERR(TAG, "MoveWindow failed with %08" PRIx32, err);
		return FALSE;
	}

	if (!floatbar_update_region(floatbar))
		return FALSE;

	InvalidateRect(floatbar->hwnd, nullptr, FALSE);
	return TRUE;
}

static int floatbar_get_resize_mode(const wfFloatBar* const floatbar, const int x, const int y)
{
	if (!floatbar || (y < 0) || (y > floatbar->height))
		return DRAG_NONE;

	if (floatbar_get_button(floatbar, x, y))
		return DRAG_NONE;

	if (x <= RESIZE_GRIP_WIDTH)
		return DRAG_RESIZE_LEFT;

	if (x >= floatbar->width - RESIZE_GRIP_WIDTH)
		return DRAG_RESIZE_RIGHT;

	return DRAG_NONE;
}

static BOOL floatbar_update_hover(wfFloatBar* floatbar, const Button* activeButton)
{
	BOOL changed = FALSE;

	if (!floatbar)
		return FALSE;

	for (int i = 0; i < BTN_MAX; i++)
	{
		Button* button = floatbar->buttons[i];
		const int active = (button == activeButton) ? TRUE : FALSE;

		if (button && (button->active != active))
		{
			button->active = active;
			changed = TRUE;
		}
	}

	if (changed)
	{
		InvalidateRect(floatbar->hwnd, nullptr, FALSE);
		UpdateWindow(floatbar->hwnd);
	}

	return TRUE;
}

static BOOL floatbar_paint(wfFloatBar* const floatbar, const HDC hdc)
{
	if (!floatbar)
		return FALSE;

	/* paint background */
	GRADIENT_RECT gradientRect = { 0, 1 };
	COLORREF rgbTop = RGB(117, 154, 198);
	COLORREF rgbBottom = RGB(6, 55, 120);
	const int top = 0;
	int bottom = BACKGROUND_H - 1;
	int left = 0;
	const int angleOffset = BACKGROUND_H - 1;

	int right = floatbar->width - 1;
	POINT pt[4];
	pt[0].x = 0;
	pt[0].y = 0;
	pt[1].x = floatbar->width;
	pt[1].y = 0;
	pt[2].x = floatbar->width - BACKGROUND_H;
	pt[2].y = BACKGROUND_H;
	pt[3].x = BACKGROUND_H;
	pt[3].y = BACKGROUND_H;
	HRGN clipRgn = CreatePolygonRgn(pt, 4, ALTERNATE);
	if (!clipRgn)
		return FALSE;

	SelectClipRgn(hdc, clipRgn);

	TRIVERTEX triVertext[2];
	triVertext[0].x = left;
	triVertext[0].y = top;
	triVertext[0].Red = GetRValue(rgbTop) << 8;
	triVertext[0].Green = GetGValue(rgbTop) << 8;
	triVertext[0].Blue = GetBValue(rgbTop) << 8;
	triVertext[0].Alpha = 0x0000;
	triVertext[1].x = right;
	triVertext[1].y = bottom;
	triVertext[1].Red = GetRValue(rgbBottom) << 8;
	triVertext[1].Green = GetGValue(rgbBottom) << 8;
	triVertext[1].Blue = GetBValue(rgbBottom) << 8;
	triVertext[1].Alpha = 0x0000;

	GradientFill(hdc, triVertext, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);
	/* paint shadow */
	HPEN hpen = CreatePen(PS_SOLID, 1, RGB(71, 71, 71));
	HGDIOBJECT orig = SelectObject(hdc, hpen);
	MoveToEx(hdc, left, top, nullptr);
	LineTo(hdc, left + angleOffset, bottom);
	LineTo(hdc, right - angleOffset, bottom);
	LineTo(hdc, right + 1, top - 1);
	DeleteObject(hpen);
	hpen = CreatePen(PS_SOLID, 1, RGB(107, 141, 184));
	SelectObject(hdc, hpen);
	left += 1;
	bottom -= 1;
	right -= 1;
	MoveToEx(hdc, left, top, nullptr);
	LineTo(hdc, left + (angleOffset - 1), bottom);
	LineTo(hdc, right - (angleOffset - 1), bottom);
	LineTo(hdc, right + 1, top - 1);
	DeleteObject(hpen);
	SelectObject(hdc, orig);

	const size_t wlen = wcslen(floatbar->wfc->window_title);
	DrawText(hdc, floatbar->wfc->window_title, WINPR_ASSERTING_INT_CAST(int, wlen),
	         &floatbar->textRect,
	         DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | DT_SINGLELINE);

	/* paint buttons */

	for (int i = 0; i < BTN_MAX; i++)
		button_paint(floatbar->buttons[i], hdc);

	SelectClipRgn(hdc, nullptr);
	DeleteObject(clipRgn);
	return TRUE;
}

static LRESULT CALLBACK floatbar_proc(const HWND hWnd, const UINT Msg, const WPARAM wParam,
                                      const LPARAM lParam)
{
	static int drag_mode = DRAG_NONE;
	static int lbtn_dwn = FALSE;
	static int btn_dwn_x = 0;
	static int btn_dwn_y = 0;
	static int btn_dwn_screen_x = 0;
	static LONG drag_start_left = 0;
	static LONG drag_start_width = 0;
	static wfFloatBar* floatbar = nullptr;
	static TRACKMOUSEEVENT tme = WINPR_C_ARRAY_INIT;
	PAINTSTRUCT ps = WINPR_C_ARRAY_INIT;
	POINT pt = WINPR_C_ARRAY_INIT;
	Button* button = nullptr;
	HDC hdc = nullptr;
	int pos_x = 0;
	int pos_y = 0;
	NONCLIENTMETRICS ncm = WINPR_C_ARRAY_INIT;

	switch (Msg)
	{
		case WM_CREATE:
			floatbar = ((wfFloatBar*)((CREATESTRUCT*)lParam)->lpCreateParams);
			floatbar->hwnd = hWnd;
			GetWindowRect(floatbar->hwnd, &floatbar->rect);
			floatbar->width = floatbar->rect.right - floatbar->rect.left;
			floatbar->height = floatbar->rect.bottom - floatbar->rect.top;
			floatbar_update_layout(floatbar);
			hdc = GetDC(hWnd);
			floatbar->hdcmem = CreateCompatibleDC(hdc);
			ReleaseDC(hWnd, hdc);
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			// Use caption font, white, draw transparent
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255, 255, 255));
			ncm.cbSize = sizeof(NONCLIENTMETRICS);
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
			SelectObject(hdc, CreateFontIndirect(&ncm.lfCaptionFont));
			floatbar_trigger_hide(floatbar);
			break;

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			floatbar_paint(floatbar, hdc);
			EndPaint(hWnd, &ps);
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_LBUTTONDOWN:
			pos_x = LOWORD(lParam);
			pos_y = HIWORD(lParam);
			button = floatbar_get_button(floatbar, pos_x, pos_y);

			if (!button)
			{
				pt.x = pos_x;
				pt.y = pos_y;
				ClientToScreen(hWnd, &pt);
				SetCapture(hWnd);
				floatbar_kill_timers(floatbar);
				drag_mode = floatbar_get_resize_mode(floatbar, pos_x, pos_y);
				if (drag_mode == DRAG_NONE)
					drag_mode = DRAG_MOVE;
				else
					SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
				btn_dwn_x = pos_x;
				btn_dwn_y = pos_y;
				btn_dwn_screen_x = pt.x;
				drag_start_left = floatbar->rect.left;
				drag_start_width = floatbar->width;
			}
			else
				lbtn_dwn = TRUE;

			break;

		case WM_LBUTTONUP:
			pos_x = LOWORD(lParam);
			pos_y = HIWORD(lParam);
			const BOOL was_dragging = drag_mode != DRAG_NONE;
			drag_mode = DRAG_NONE;
			ReleaseCapture();
			if (was_dragging)
				floatbar_trigger_hide(floatbar);

			if (lbtn_dwn)
			{
				button = floatbar_get_button(floatbar, pos_x, pos_y);

				if (button)
					button_hit(button);

				lbtn_dwn = FALSE;
			}

			break;

		case WM_LBUTTONDBLCLK:
			pos_x = LOWORD(lParam);
			pos_y = HIWORD(lParam);

			if (!floatbar_get_button(floatbar, pos_x, pos_y) && floatbar->wfc->fullscreen_toggle &&
			    floatbar->wfc->fullscreen)
				wf_toggle_fullscreen(floatbar->wfc);
			break;

		case WM_MOUSEMOVE:
			pos_x = LOWORD(lParam);
			pos_y = HIWORD(lParam);

			if (!floatbar->locked)
				floatbar_animation(floatbar, TRUE);

			if (drag_mode != DRAG_NONE)
			{
				// Use screen pos during left-side resize to sync new position and avoid jittering.
				pt.x = pos_x;
				pt.y = pos_y;
				ClientToScreen(hWnd, &pt);

				if (drag_mode == DRAG_MOVE)
				{
					const int dy = pos_y - btn_dwn_y;
					const int dy_threshold = floatbar->height * RESTORE_DRAG_BAR_MULTIPLIER;

					if (dy > dy_threshold && floatbar->wfc->fullscreen_toggle &&
					    floatbar->wfc->fullscreen)
					{
						drag_mode = DRAG_NONE;
						ReleaseCapture();
						wf_toggle_fullscreen(floatbar->wfc);
						break;
					}

					const int xScreen = floatbar_parent_width(floatbar);
					floatbar->rect.left = floatbar->rect.left + pos_x - btn_dwn_x;

					if (floatbar->rect.left < 0)
						floatbar->rect.left = 0;
					else if (floatbar->rect.left > xScreen - floatbar->width)
						floatbar->rect.left = xScreen - floatbar->width;

					MoveWindow(hWnd, floatbar->rect.left, 0, floatbar->width, floatbar->height,
					           TRUE);
				}
				else
				{
					const int dx = pt.x - btn_dwn_screen_x;
					LONG left = drag_start_left;
					LONG width = drag_start_width;

					if (drag_mode == DRAG_RESIZE_LEFT)
					{
						left = drag_start_left + dx;
						width = drag_start_width - dx;

						if (width < MIN_FLOATBAR_WIDTH)
						{
							width = MIN_FLOATBAR_WIDTH;
							left = drag_start_left + drag_start_width - width;
						}
					}
					else if (drag_mode == DRAG_RESIZE_RIGHT)
					{
						width = drag_start_width + dx;
					}

					floatbar_resize(floatbar, left, width);
				}
				break;
			}
			else
			{
				button = floatbar_get_button(floatbar, pos_x, pos_y);
				floatbar_update_hover(floatbar, button);
			}

			TrackMouseEvent(&tme);
			break;

		case WM_CAPTURECHANGED:
			if (drag_mode != DRAG_NONE)
				floatbar_trigger_hide(floatbar);

			drag_mode = DRAG_NONE;
			break;

		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT)
			{
				GetCursorPos(&pt);
				ScreenToClient(hWnd, &pt);

				if ((drag_mode == DRAG_RESIZE_LEFT) || (drag_mode == DRAG_RESIZE_RIGHT) ||
				    (floatbar_get_resize_mode(floatbar, pt.x, pt.y) != DRAG_NONE))
					SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
				else
					SetCursor(LoadCursor(nullptr, IDC_ARROW));

				return TRUE;
			}

			return DefWindowProc(hWnd, Msg, wParam, lParam);

		case WM_MOUSELEAVE:
		{
			floatbar_update_hover(floatbar, nullptr);
			floatbar_trigger_hide(floatbar);
			break;
		}

		case WM_TIMER:
			switch (wParam)
			{
				case TIMER_HIDE:
					floatbar_animation(floatbar, FALSE);
					break;

				case TIMER_ANIMAT_SHOW:
				{
					floatbar->offset--;
					MoveWindow(floatbar->hwnd, floatbar->rect.left, -floatbar->offset,
					           floatbar->width, floatbar->height, TRUE);

					if (floatbar->offset <= 0)
						floatbar_show(floatbar);

					break;
				}

				case TIMER_ANIMAT_HIDE:
				{
					floatbar->offset++;
					MoveWindow(floatbar->hwnd, floatbar->rect.left, -floatbar->offset,
					           floatbar->width, floatbar->height, TRUE);

					if (floatbar->offset >= floatbar->height - 2)
						floatbar_hide(floatbar);

					break;
				}

				default:
					break;
			}

			break;

		case WM_DESTROY:
			DeleteDC(floatbar->hdcmem);
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return 0;
}

static BOOL floatbar_window_create(wfFloatBar* floatbar)
{
	WNDCLASSEX wnd_cls;
	HWND barWnd;
	RECT rect;
	LONG x;

	if (!floatbar)
		return FALSE;

	if (!GetWindowRect(floatbar->parent, &rect))
		return FALSE;

	x = (rect.right - rect.left - BACKGROUND_W) / 2;
	wnd_cls.cbSize = sizeof(WNDCLASSEX);
	wnd_cls.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wnd_cls.lpfnWndProc = floatbar_proc;
	wnd_cls.cbClsExtra = 0;
	wnd_cls.cbWndExtra = 0;
	wnd_cls.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wnd_cls.hCursor = LoadCursor(floatbar->root_window, IDC_ARROW);
	wnd_cls.hbrBackground = nullptr;
	wnd_cls.lpszMenuName = nullptr;
	wnd_cls.lpszClassName = L"floatbar";
	wnd_cls.hInstance = floatbar->root_window;
	wnd_cls.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	RegisterClassEx(&wnd_cls);
	barWnd =
	    CreateWindowEx(WS_EX_TOPMOST, L"floatbar", L"floatbar", WS_CHILD, x, 0, BACKGROUND_W,
	                   BACKGROUND_H, floatbar->parent, nullptr, floatbar->root_window, floatbar);

	if (barWnd == nullptr)
		return FALSE;

	return floatbar_update_region(floatbar);
}

void wf_floatbar_free(wfFloatBar* floatbar)
{
	if (!floatbar)
		return;

	free(floatbar);
}

wfFloatBar* wf_floatbar_new(wfContext* wfc, HINSTANCE window, DWORD flags)
{
	wfFloatBar* floatbar;

	/* Floatbar not enabled */
	if ((flags & 0x0001) == 0)
		return nullptr;

	if (!wfc)
		return nullptr;

	// TODO: Disable for remote app
	floatbar = (wfFloatBar*)calloc(1, sizeof(wfFloatBar));

	if (!floatbar)
		return nullptr;

	floatbar->root_window = window;
	floatbar->flags = flags;
	floatbar->wfc = wfc;
	floatbar->locked = (flags & 0x0002) != 0;
	floatbar->shown = (flags & 0x0006) != 0; /* If it is loked or shown show it */
	floatbar->hwnd = nullptr;
	floatbar->parent = wfc->hwnd;
	floatbar->hdcmem = nullptr;

	if (wfc->fullscreen_toggle)
	{
		floatbar->buttons[0] =
		    floatbar_create_button(floatbar, BUTTON_MINIMIZE, IDB_MINIMIZE, IDB_MINIMIZE_ACT,
		                           MINIMIZE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
		floatbar->buttons[1] =
		    floatbar_create_button(floatbar, BUTTON_RESTORE, IDB_RESTORE, IDB_RESTORE_ACT,
		                           RESTORE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
	}
	else
	{
		floatbar->buttons[0] = nullptr;
		floatbar->buttons[1] = nullptr;
	}

	floatbar->buttons[2] = floatbar_create_button(floatbar, BUTTON_CLOSE, IDB_CLOSE, IDB_CLOSE_ACT,
	                                              CLOSE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
	floatbar->buttons[3] =
	    floatbar_create_lock_button(floatbar, IDB_UNLOCK, IDB_UNLOCK_ACT, IDB_LOCK, IDB_LOCK_ACT,
	                                LOCK_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);

	if (!floatbar_window_create(floatbar))
		goto fail;

	if (!update_locked_state(floatbar))
		goto fail;

	if (!wf_floatbar_toggle_fullscreen(
	        floatbar, freerdp_settings_get_bool(wfc->common.context.settings, FreeRDP_Fullscreen)))
		goto fail;

	return floatbar;
fail:
	wf_floatbar_free(floatbar);
	return nullptr;
}

BOOL wf_floatbar_toggle_fullscreen(wfFloatBar* floatbar, BOOL fullscreen)
{
	BOOL show_fs, show_wn;

	if (!floatbar)
		return FALSE;

	show_fs = (floatbar->flags & 0x0010) != 0;
	show_wn = (floatbar->flags & 0x0020) != 0;

	if ((show_fs && fullscreen) || (show_wn && !fullscreen))
	{
		ShowWindow(floatbar->hwnd, SW_SHOWNORMAL);
		Sleep(10);

		if (floatbar->shown)
			floatbar_show(floatbar);
		else
			floatbar_hide(floatbar);
	}
	else
	{
		ShowWindow(floatbar->hwnd, SW_HIDE);
	}

	return TRUE;
}

BOOL wf_floatbar_reset_position(wfFloatBar* floatbar)
{
	RECT rect = WINPR_C_ARRAY_INIT;

	if (!floatbar || !floatbar->parent || !floatbar->hwnd)
		return FALSE;

	const int y = -WINPR_ASSERTING_INT_CAST(int, floatbar->offset);

	if (!GetWindowRect(floatbar->parent, &rect))
		return FALSE;

	floatbar->rect.left = ((rect.right - rect.left) - floatbar->width) / 2;
	if (floatbar->rect.left < 0)
		floatbar->rect.left = 0;

	if (!MoveWindow(floatbar->hwnd, floatbar->rect.left, y, floatbar->width, floatbar->height,
	                TRUE))
	{
		DWORD err = GetLastError();
		WLog_ERR(TAG, "MoveWindow failed with %08" PRIx32, err);
		return FALSE;
	}

	return TRUE;
}
