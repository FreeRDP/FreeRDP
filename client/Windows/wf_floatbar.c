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

#define LOCK_X (BACKGROUND_H + BUTTON_OFFSET)
#define CLOSE_X ((BACKGROUND_W - (BACKGROUND_H + BUTTON_OFFSET)) - BUTTON_WIDTH)
#define RESTORE_X (CLOSE_X - (BUTTON_WIDTH + BUTTON_SPACING))
#define MINIMIZE_X (RESTORE_X - (BUTTON_WIDTH + BUTTON_SPACING))
#define TEXT_X (BACKGROUND_H + ((BUTTON_WIDTH + BUTTON_SPACING) * 3) + 5)

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
	size_t x;
	UINT_PTR timers[] = { TIMER_HIDE, TIMER_ANIMAT_HIDE, TIMER_ANIMAT_SHOW };

	if (!floatbar)
		return FALSE;

	for (x = 0; x < ARRAYSIZE(timers); x++)
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

	if (SetTimer(floatbar->hwnd, timer, USER_TIMER_MINIMUM, NULL) == 0)
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
		if (SetTimer(floatbar->hwnd, TIMER_HIDE, 3000, NULL) == 0)
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

	InvalidateRect(button->floatbar->hwnd, NULL, FALSE);
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
	if (button != NULL)
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
		return NULL;

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
		return NULL;

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
	int i;

	if ((y > BUTTON_Y) && (y < BUTTON_Y + BUTTON_HEIGHT))
	{
		for (i = 0; i < BTN_MAX; i++)
		{
			if ((floatbar->buttons[i] != NULL) && (x > floatbar->buttons[i]->x) &&
			    (x < floatbar->buttons[i]->x + floatbar->buttons[i]->w))
			{
				return floatbar->buttons[i];
			}
		}
	}

	return NULL;
}

static BOOL floatbar_paint(wfFloatBar* const floatbar, const HDC hdc)
{
	int i;
	HPEN hpen;
	HGDIOBJECT orig;
	/* paint background */
	GRADIENT_RECT gradientRect = { 0, 1 };
	COLORREF rgbTop = RGB(117, 154, 198);
	COLORREF rgbBottom = RGB(6, 55, 120);
	const int top = 0;
	int left = 0;
	int bottom = BACKGROUND_H - 1;
	int right = BACKGROUND_W - 1;
	const int angleOffset = BACKGROUND_H - 1;
	TRIVERTEX triVertext[2] = { { left, top, GetRValue(rgbTop) << 8, GetGValue(rgbTop) << 8,
		                          GetBValue(rgbTop) << 8, 0x0000 },
		                        { right, bottom, GetRValue(rgbBottom) << 8,
		                          GetGValue(rgbBottom) << 8, GetBValue(rgbBottom) << 8, 0x0000 } };

	if (!floatbar)
		return FALSE;

	GradientFill(hdc, triVertext, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);
	/* paint shadow */
	hpen = CreatePen(PS_SOLID, 1, RGB(71, 71, 71));
	orig = SelectObject(hdc, hpen);
	MoveToEx(hdc, left, top, NULL);
	LineTo(hdc, left + angleOffset, bottom);
	LineTo(hdc, right - angleOffset, bottom);
	LineTo(hdc, right + 1, top - 1);
	DeleteObject(hpen);
	hpen = CreatePen(PS_SOLID, 1, RGB(107, 141, 184));
	SelectObject(hdc, hpen);
	left += 1;
	bottom -= 1;
	right -= 1;
	MoveToEx(hdc, left, top, NULL);
	LineTo(hdc, left + (angleOffset - 1), bottom);
	LineTo(hdc, right - (angleOffset - 1), bottom);
	LineTo(hdc, right + 1, top - 1);
	DeleteObject(hpen);
	SelectObject(hdc, orig);
	DrawText(hdc, floatbar->wfc->window_title, wcslen(floatbar->wfc->window_title),
	         &floatbar->textRect,
	         DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | DT_SINGLELINE);

	/* paint buttons */

	for (i = 0; i < BTN_MAX; i++)
		button_paint(floatbar->buttons[i], hdc);

	return TRUE;
}

static LRESULT CALLBACK floatbar_proc(const HWND hWnd, const UINT Msg, const WPARAM wParam,
                                      const LPARAM lParam)
{
	static int dragging = FALSE;
	static int lbtn_dwn = FALSE;
	static int btn_dwn_x = 0;
	static wfFloatBar* floatbar;
	static TRACKMOUSEEVENT tme;
	PAINTSTRUCT ps;
	Button* button;
	HDC hdc;
	int pos_x;
	int pos_y;
	NONCLIENTMETRICS ncm;
	int xScreen = GetSystemMetrics(SM_CXSCREEN);

	switch (Msg)
	{
		case WM_CREATE:
			floatbar = ((wfFloatBar*)((CREATESTRUCT*)lParam)->lpCreateParams);
			floatbar->hwnd = hWnd;
			GetWindowRect(floatbar->hwnd, &floatbar->rect);
			floatbar->width = floatbar->rect.right - floatbar->rect.left;
			floatbar->height = floatbar->rect.bottom - floatbar->rect.top;
			hdc = GetDC(hWnd);
			floatbar->hdcmem = CreateCompatibleDC(hdc);
			ReleaseDC(hWnd, hdc);
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			// Use caption font, white, draw transparent
			GetClientRect(hWnd, &floatbar->textRect);
			InflateRect(&floatbar->textRect, -TEXT_X, 0);
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

		case WM_LBUTTONDOWN:
			pos_x = lParam & 0xffff;
			pos_y = (lParam >> 16) & 0xffff;
			button = floatbar_get_button(floatbar, pos_x, pos_y);

			if (!button)
			{
				SetCapture(hWnd);
				dragging = TRUE;
				btn_dwn_x = lParam & 0xffff;
			}
			else
				lbtn_dwn = TRUE;

			break;

		case WM_LBUTTONUP:
			pos_x = lParam & 0xffff;
			pos_y = (lParam >> 16) & 0xffff;
			ReleaseCapture();
			dragging = FALSE;

			if (lbtn_dwn)
			{
				button = floatbar_get_button(floatbar, pos_x, pos_y);

				if (button)
					button_hit(button);

				lbtn_dwn = FALSE;
			}

			break;

		case WM_MOUSEMOVE:
			pos_x = lParam & 0xffff;
			pos_y = (lParam >> 16) & 0xffff;

			if (!floatbar->locked)
				floatbar_animation(floatbar, TRUE);

			if (dragging)
			{
				floatbar->rect.left = floatbar->rect.left + (lParam & 0xffff) - btn_dwn_x;

				if (floatbar->rect.left < 0)
					floatbar->rect.left = 0;
				else if (floatbar->rect.left > xScreen - floatbar->width)
					floatbar->rect.left = xScreen - floatbar->width;

				MoveWindow(hWnd, floatbar->rect.left, 0, floatbar->width, floatbar->height, TRUE);
			}
			else
			{
				int i;

				for (i = 0; i < BTN_MAX; i++)
				{
					if (floatbar->buttons[i] != NULL)
					{
						floatbar->buttons[i]->active = FALSE;
					}
				}

				button = floatbar_get_button(floatbar, pos_x, pos_y);

				if (button)
					button->active = TRUE;

				InvalidateRect(hWnd, NULL, FALSE);
				UpdateWindow(hWnd);
			}

			TrackMouseEvent(&tme);
			break;

		case WM_CAPTURECHANGED:
			dragging = FALSE;
			break;

		case WM_MOUSELEAVE:
		{
			int i;

			for (i = 0; i < BTN_MAX; i++)
			{
				if (floatbar->buttons[i] != NULL)
				{
					floatbar->buttons[i]->active = FALSE;
				}
			}

			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);
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
	HRGN hRgn;
	POINT pt[4];
	RECT rect;
	LONG x;

	if (!floatbar)
		return FALSE;

	if (!GetWindowRect(floatbar->parent, &rect))
		return FALSE;

	x = (rect.right - rect.left - BACKGROUND_W) / 2;
	wnd_cls.cbSize = sizeof(WNDCLASSEX);
	wnd_cls.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wnd_cls.lpfnWndProc = floatbar_proc;
	wnd_cls.cbClsExtra = 0;
	wnd_cls.cbWndExtra = 0;
	wnd_cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wnd_cls.hCursor = LoadCursor(floatbar->root_window, IDC_ARROW);
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszMenuName = NULL;
	wnd_cls.lpszClassName = L"floatbar";
	wnd_cls.hInstance = floatbar->root_window;
	wnd_cls.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wnd_cls);
	barWnd = CreateWindowEx(WS_EX_TOPMOST, L"floatbar", L"floatbar", WS_CHILD, x, 0, BACKGROUND_W,
	                        BACKGROUND_H, floatbar->parent, NULL, floatbar->root_window, floatbar);

	if (barWnd == NULL)
		return FALSE;

	pt[0].x = 0;
	pt[0].y = 0;
	pt[1].x = BACKGROUND_W;
	pt[1].y = 0;
	pt[2].x = BACKGROUND_W - BACKGROUND_H;
	pt[2].y = BACKGROUND_H;
	pt[3].x = BACKGROUND_H;
	pt[3].y = BACKGROUND_H;
	hRgn = CreatePolygonRgn(pt, 4, ALTERNATE);
	SetWindowRgn(barWnd, hRgn, TRUE);
	return TRUE;
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
		return NULL;

	if (!wfc)
		return NULL;

	// TODO: Disable for remote app
	floatbar = (wfFloatBar*)calloc(1, sizeof(wfFloatBar));

	if (!floatbar)
		return NULL;

	floatbar->root_window = window;
	floatbar->flags = flags;
	floatbar->wfc = wfc;
	floatbar->locked = (flags & 0x0002) != 0;
	floatbar->shown = (flags & 0x0006) != 0; /* If it is loked or shown show it */
	floatbar->hwnd = NULL;
	floatbar->parent = wfc->hwnd;
	floatbar->hdcmem = NULL;

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
		floatbar->buttons[0] = NULL;
		floatbar->buttons[1] = NULL;
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

	if (!wf_floatbar_toggle_fullscreen(floatbar, wfc->common.context.settings->Fullscreen))
		goto fail;

	return floatbar;
fail:
	wf_floatbar_free(floatbar);
	return NULL;
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
