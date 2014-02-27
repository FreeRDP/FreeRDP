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

#include <Windows.h>
#include <stdlib.h>

#include "wf_interface.h"
#include "wf_floatbar.h"
#include "wf_window.h"
#include "wf_gdi.h"
#include "resource.h"

typedef struct _Button Button;

/* TIMERs */
#define TIMER_HIDE 1
#define TIMER_ANIMAT_SHOW 2
#define TIMER_ANIMAT_HIDE 3

/* Button Type */
#define BUTTON_MINIMIZE     1
#define BUTTON_RESTORE      2
#define BUTTON_CLOSE        3
#define BUTTON_LOCKPIN      4
#define BTN_MAX 4

/* bmp size */
#define BACKGROUND_X 581
#define BACKGROUND_Y 29
#define LOCK_X 13
#define MINIMIZE_X (BACKGROUND_X - 91)
#define CLOSE_X (BACKGROUND_X - 37)
#define RESTORE_X (BACKGROUND_X - 64)

#define BUTTON_Y 2
#define BUTTON_WIDTH 24
#define BUTTON_HEIGHT 24

struct _Button {
	FloatBar* floatbar;
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
};

struct _FloatBar {
	HWND parent;
	HWND hwnd;
	RECT rect;
	LONG width;
	LONG height;
	wfContext* wfc;
	Button* buttons[BTN_MAX];
	BOOL shown;
	BOOL locked;
	HBITMAP background;
};

static int button_hit(Button* button)
{
	FloatBar* floatbar = button->floatbar;

	switch (button->type)
	{
		case BUTTON_LOCKPIN:
			if (!floatbar->locked)
			{
				button->bmp = button->locked_bmp;
				button->bmp_act = button->locked_bmp_act;
			}
			else
			{
				button->bmp = button->unlocked_bmp;
				button->bmp_act = button->unlocked_bmp_act;
			}

			floatbar->locked = ~floatbar->locked;
			InvalidateRect(button->floatbar->hwnd, NULL, FALSE);
			UpdateWindow(button->floatbar->hwnd);
			break;

		case BUTTON_MINIMIZE:
			ShowWindow(floatbar->parent, SW_MINIMIZE);
			break;

		case BUTTON_RESTORE:
			wf_toggle_fullscreen(floatbar->wfc);
			break;

		case BUTTON_CLOSE:
			SendMessage(floatbar->parent, WM_DESTROY, 0 , 0);
			break;

		default:
			return 0;
	}

	return 0;
}

static int button_paint(Button* button)
{
	FloatBar* floatbar = button->floatbar;
	HDC hDC = GetDC(floatbar->hwnd);
	HDC hMemDC = CreateCompatibleDC(hDC);
	HBITMAP hOldBitmap;
	BITMAP bm;

	hOldBitmap = (HBITMAP)SelectObject(hMemDC, button->active ? button->bmp_act : button->bmp);

	StretchBlt(hDC, button->x, button->y, button->w, button->h, hMemDC, 0, 0, button->w, button->h, SRCCOPY);

	SelectObject(hMemDC, hOldBitmap);

	DeleteDC(hMemDC);
	ReleaseDC(floatbar->hwnd, hDC);

	return 0;
}

static Button* floatbar_create_button(FloatBar* floatbar, int type, int resid, int resid_act, int x, int y, int h, int w)
{
	Button *button;
	HDC hDC;
	HDC hMemDC;
	HBITMAP hBitmap;
	HBITMAP hOldBitmap;
	BITMAP bm;

	button = (Button *)malloc(sizeof(Button));

	if (!button)
		return NULL;

	button->floatbar = floatbar;
	button->type = type;
	button->x = x;
	button->y = y;
	button->w = w;
	button->h = h;
	button->active = FALSE;

	button->bmp = (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(resid), IMAGE_BITMAP, w, h, LR_DEFAULTCOLOR);
	button->bmp_act = (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(resid_act), IMAGE_BITMAP, w, h, LR_DEFAULTCOLOR);

	return button;
}

static Button* floatbar_create_lock_button(FloatBar* floatbar,
									int unlock_resid, int unlock_resid_act,
									int lock_resid, int lock_resid_act,
									int x, int y, int h, int w)
{
	Button* button;

	button = floatbar_create_button(floatbar, BUTTON_LOCKPIN, unlock_resid, unlock_resid_act, x, y, h, w);

	if (!button)
		return NULL;

	button->unlocked_bmp = button->bmp;
	button->unlocked_bmp_act = button->bmp_act;
	button->locked_bmp = (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(lock_resid), IMAGE_BITMAP, w, h, LR_DEFAULTCOLOR);
	button->locked_bmp_act = (HBITMAP)LoadImage(floatbar->wfc->hInstance, MAKEINTRESOURCE(lock_resid_act), IMAGE_BITMAP, w, h, LR_DEFAULTCOLOR);

	return button;
}

static Button* floatbar_get_button(FloatBar* floatbar, int x, int y)
{
	int i;

	if (y > BUTTON_Y && y < BUTTON_Y + BUTTON_HEIGHT)
		for (i = 0; i < BTN_MAX; i++)
			if (x > floatbar->buttons[i]->x && x < floatbar->buttons[i]->x + floatbar->buttons[i]->w)
				return floatbar->buttons[i];

	return NULL;
}

static int floatbar_paint(FloatBar* floatbar)
{
	int i;
	HDC hDC;
	HDC hMemDC;
	HBITMAP hOldBitmap;
	BITMAP bm;

	if (!floatbar->wfc->fullscreen)
		return -1;

	/* paint background */
	hDC = GetDC(floatbar->hwnd);
	hMemDC = CreateCompatibleDC(hDC);
	GetObject(floatbar->background, sizeof(bm), &bm);

	hOldBitmap = (HBITMAP)SelectObject(hMemDC, floatbar->background);
	StretchBlt(hDC, 0, 0, BACKGROUND_X, BACKGROUND_Y, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	(HBITMAP)SelectObject(hMemDC, hOldBitmap);

	DeleteDC(hMemDC);
	ReleaseDC(floatbar->hwnd, hDC);

	/* paint buttons */
	for (i = 0; i < BTN_MAX; i++)
		button_paint(floatbar->buttons[i]);

	return 0;
}

static int floatbar_animation(FloatBar* floatbar, BOOL show)
{
	LONG width, height, y;

	SetTimer(floatbar->hwnd, show ? TIMER_ANIMAT_SHOW : TIMER_ANIMAT_HIDE, 10, NULL);

	floatbar->shown = show;

	return 0;
}

LRESULT CALLBACK floatbar_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	RECT rect;
	static int dragging = FALSE;
	static int lbtn_dwn = FALSE;
	static int btn_dwn_x = 0;

	int nWidth, nHeight;
	int drag_left = 0;
	int xScreen = GetSystemMetrics(SM_CXSCREEN);
	HWND hParentWnd = GetParent(hWnd);

	static FloatBar* floatbar;
	Button* button;
	int pos_x;
	int pos_y;

	TRACKMOUSEEVENT tme;

	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hWnd;
	tme.dwHoverTime = HOVER_DEFAULT;

	GetWindowRect(hWnd, &rect);
	nWidth = rect.right - rect.left;
	nHeight = rect.bottom - rect.top;

	switch(Msg)
	{
		case WM_CREATE:
			floatbar = (FloatBar *)((CREATESTRUCT *)lParam)->lpCreateParams;
			floatbar->hwnd = hWnd;
			floatbar->parent = hParentWnd;
			GetWindowRect(floatbar->hwnd, &floatbar->rect);
			floatbar->width = floatbar->rect.right - floatbar->rect.left;
			floatbar->height = floatbar->rect.bottom - floatbar->rect.top;
			SetTimer(hWnd, TIMER_HIDE, 3000, NULL);
			break;

		case WM_PAINT:
			BeginPaint(hWnd, &ps);
			floatbar_paint(floatbar);
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
			KillTimer(hWnd, TIMER_HIDE);
			pos_x = lParam & 0xffff;
			pos_y = (lParam >> 16) & 0xffff;

			if (!floatbar->shown)
					floatbar_animation(floatbar, TRUE);

			if (dragging)
			{
				drag_left = rect.left + (lParam & 0xffff) - btn_dwn_x;

				if (drag_left < 0)
					drag_left = 0;
				else if (drag_left > xScreen - floatbar->width)
					drag_left = xScreen - floatbar->width;

				MoveWindow(hWnd, drag_left, rect.top, floatbar->width, floatbar->height, TRUE);
			}
			else
			{
				int i;

				for (i = 0; i < BTN_MAX; i++)
					floatbar->buttons[i]->active = FALSE;

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
				floatbar->buttons[i]->active = FALSE;

			InvalidateRect(hWnd, NULL, FALSE);
			UpdateWindow(hWnd);

			SetTimer(hWnd, TIMER_HIDE, 3000, NULL);
			break;
		}
		case WM_TIMER:
			switch (wParam)
			{
				case TIMER_HIDE:
				{
					KillTimer(hWnd, TIMER_HIDE);
					floatbar_animation(floatbar, FALSE);
					break;
				}
				case TIMER_ANIMAT_SHOW:
				{
					static int y = 0;

					MoveWindow(floatbar->hwnd, rect.left, (y++ - floatbar->height), floatbar->width, floatbar->height, TRUE);
					if (y == floatbar->height)
					{
						y = 0;
						KillTimer(hWnd, wParam);
					}
					break;
				}
				case TIMER_ANIMAT_HIDE:
				{
					static int y = 0;

					MoveWindow(floatbar->hwnd, rect.left, -y++, floatbar->width, floatbar->height, TRUE);
					if (y == floatbar->height)
					{
						y = 0;
						KillTimer(hWnd, wParam);
						printf("kill timer hide\n");
					}
					break;
				}
				default:
					break;
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}
	return 0;
}

static FloatBar* floatbar_create(wfContext* wfc)
{
	FloatBar* floatbar;

	floatbar = (FloatBar *)malloc(sizeof(FloatBar));

	if (!floatbar)
		return NULL;


	floatbar->locked = FALSE;
	floatbar->shown = TRUE;
	floatbar->hwnd = NULL;
	floatbar->parent = wfc->hwnd;
	floatbar->wfc = wfc;

	floatbar->background = (HBITMAP)LoadImage(wfc->hInstance, MAKEINTRESOURCE(IDB_BACKGROUND), IMAGE_BITMAP, BACKGROUND_X, BACKGROUND_Y, LR_DEFAULTCOLOR);
	floatbar->buttons[0] = floatbar_create_button(floatbar, BUTTON_MINIMIZE, IDB_MINIMIZE, IDB_MINIMIZE_ACT, MINIMIZE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
	floatbar->buttons[1] = floatbar_create_button(floatbar, BUTTON_RESTORE, IDB_RESTORE, IDB_RESTORE_ACT, RESTORE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
	floatbar->buttons[2] = floatbar_create_button(floatbar, BUTTON_CLOSE, IDB_CLOSE, IDB_CLOSE_ACT, CLOSE_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);
	floatbar->buttons[3] = floatbar_create_lock_button(floatbar, IDB_UNLOCK, IDB_UNLOCK_ACT, IDB_LOCK, IDB_LOCK_ACT, LOCK_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);

	return floatbar;
}

int floatbar_hide(FloatBar* floatbar)
{
	MoveWindow(floatbar->hwnd, floatbar->rect.left, -floatbar->height, floatbar->width, floatbar->height, TRUE);
	return 0;
}

int floatbar_show(FloatBar* floatbar)
{
	MoveWindow(floatbar->hwnd, floatbar->rect.left, floatbar->rect.top, floatbar->width, floatbar->height, TRUE);
	return 0;
}

void floatbar_window_create(wfContext *wfc)
{
	WNDCLASSEX wnd_cls;
	HWND barWnd;
	int x = (GetSystemMetrics(SM_CXSCREEN) - BACKGROUND_X) / 2;

	wnd_cls.cbSize        = sizeof(WNDCLASSEX);
	wnd_cls.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wnd_cls.lpfnWndProc   = floatbar_proc;
	wnd_cls.cbClsExtra    = 0;
	wnd_cls.cbWndExtra    = 0;
	wnd_cls.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wnd_cls.hCursor       = LoadCursor(wfc->hInstance, IDC_ARROW);
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszMenuName  = NULL;
	wnd_cls.lpszClassName = L"floatbar";
	wnd_cls.hInstance     = wfc->hInstance;
	wnd_cls.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	RegisterClassEx(&wnd_cls);

	wfc->floatbar = floatbar_create(wfc);

	barWnd = CreateWindowEx(WS_EX_TOPMOST, L"floatbar", L"floatbar", WS_CHILD, x, 0, BACKGROUND_X, BACKGROUND_Y, wfc->hwnd, NULL, wfc->hInstance, wfc->floatbar);
	if (barWnd == NULL)
		return;
	ShowWindow(barWnd, SW_SHOWNORMAL);
}
