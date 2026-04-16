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
#include <windowsx.h>
#include <wchar.h>

#include "wf_client.h"
#include "wf_floatbar.h"

#include "resource/resource.h"
#include "wf_gdi.h"
#ifdef _MSC_VER
#pragma comment(lib, "Msimg32.lib")
#endif

#define TAG CLIENT_TAG("windows.floatbar")

#define TIMER_HIDE 1
#define TIMER_SHOW 2

#define FLOATBAR_PEEK_HEIGHT 2
#define FLOATBAR_MIN_WIDTH 576
#define FLOATBAR_TEXT_RIGHT_PAD 104
#define FLOATBAR_MAX_MARGIN 20

#define BUTTON_LOCKPIN 0
#define BUTTON_MINIMIZE 1
#define BUTTON_RESTORE 2
#define BUTTON_CLOSE 3
#define BTN_MAX 4

#define BACKGROUND_H 27
#define BUTTON_OFFSET 5
#define BUTTON_Y 2
#define BUTTON_WIDTH 23
#define BUTTON_HEIGHT 21
#define BUTTON_SPACING 1

#define LOCK_X (BACKGROUND_H + BUTTON_OFFSET)
#define TEXT_X (BACKGROUND_H + ((BUTTON_WIDTH + BUTTON_SPACING) * 3) + 5)

typedef struct
{
	wfFloatBar* floatbar;
	int type;
	int x;
	int y;
	int w;
	int h;
	BOOL active;
	HBITMAP bmp;
	HBITMAP bmp_act;
	HBITMAP locked_bmp;
	HBITMAP locked_bmp_act;
	HBITMAP unlocked_bmp;
	HBITMAP unlocked_bmp_act;
} FloatbarButton;

struct s_FloatBar
{
	HINSTANCE root_window;
	DWORD flags;
	HWND parent;
	HWND hwnd;
	RECT rect;
	RECT textRect;
	LONG width;
	LONG height;
	wfContext* wfc;
	FloatbarButton* buttons[BTN_MAX];
	FloatbarButton* hoverButton;
	BOOL shown;
	BOOL locked;
	BOOL mouseInside;
	HDC hdcmem;
	HFONT titleFont;
};

static BOOL floatbar_is_visible_in_mode(const wfFloatBar* floatbar, BOOL fullscreen)
{
	const BOOL showFs = (floatbar->flags & 0x0010) != 0;
	const BOOL showWn = (floatbar->flags & 0x0020) != 0;
	return (showFs && fullscreen) || (showWn && !fullscreen);
}

static BOOL floatbar_parent_rect(const wfFloatBar* floatbar, RECT* rect)
{
	POINT origin = { 0 };

	if (!floatbar || !floatbar->parent || !rect)
		return FALSE;

	if (!GetClientRect(floatbar->parent, rect))
		return FALSE;

	if (!ClientToScreen(floatbar->parent, &origin))
		return FALSE;

	OffsetRect(rect, origin.x, origin.y);
	return TRUE;
}

static LONG floatbar_measure_title_width(wfFloatBar* floatbar)
{
	const WCHAR* title;
	HDC hdc = nullptr;
	HGDIOBJECT oldFont = nullptr;
	SIZE size = { 0 };
	LONG width = 0;

	if (!floatbar || !floatbar->parent || !floatbar->wfc)
		return 0;

	title = floatbar->wfc->window_title;
	if (!title)
		return 0;

	hdc = GetDC(floatbar->parent);
	if (!hdc)
		return 0;

	if (floatbar->titleFont)
		oldFont = SelectObject(hdc, floatbar->titleFont);

	if (GetTextExtentPoint32W(hdc, title, (int)wcslen(title), &size))
		width = size.cx;

	if (oldFont)
		SelectObject(hdc, oldFont);
	ReleaseDC(floatbar->parent, hdc);
	return width;
}

static void floatbar_update_region(wfFloatBar* floatbar)
{
	POINT points[4];
	HRGN region;

	if (!floatbar || !floatbar->hwnd)
		return;

	points[0].x = 0;
	points[0].y = 0;
	points[1].x = floatbar->width;
	points[1].y = 0;
	points[2].x = floatbar->width - BACKGROUND_H;
	points[2].y = BACKGROUND_H;
	points[3].x = BACKGROUND_H;
	points[3].y = BACKGROUND_H;

	region = CreatePolygonRgn(points, ARRAYSIZE(points), ALTERNATE);
	if (region)
		SetWindowRgn(floatbar->hwnd, region, TRUE);
}

static void floatbar_update_layout(wfFloatBar* floatbar)
{
	RECT parentRect = { 0 };
	LONG rightButtonsWidth;
	LONG requestedWidth;
	LONG maxWidth;
	LONG closeX;
	LONG restoreX;
	LONG minimizeX;

	if (!floatbar)
		return;

	rightButtonsWidth =
	    (BACKGROUND_H + BUTTON_OFFSET) + (BUTTON_WIDTH * 3) + (BUTTON_SPACING * 2);
	requestedWidth = TEXT_X + floatbar_measure_title_width(floatbar) + FLOATBAR_TEXT_RIGHT_PAD;
	if (requestedWidth < FLOATBAR_MIN_WIDTH)
		requestedWidth = FLOATBAR_MIN_WIDTH;

	if (floatbar_parent_rect(floatbar, &parentRect))
	{
		maxWidth = (parentRect.right - parentRect.left) - FLOATBAR_MAX_MARGIN;
		if (maxWidth > FLOATBAR_MIN_WIDTH && requestedWidth > maxWidth)
			requestedWidth = maxWidth;
	}

	floatbar->width = requestedWidth;
	floatbar->height = BACKGROUND_H;

	closeX = floatbar->width - (BACKGROUND_H + BUTTON_OFFSET) - BUTTON_WIDTH;
	restoreX = closeX - (BUTTON_WIDTH + BUTTON_SPACING);
	minimizeX = restoreX - (BUTTON_WIDTH + BUTTON_SPACING);

	if (floatbar->buttons[BUTTON_MINIMIZE])
	{
		floatbar->buttons[BUTTON_MINIMIZE]->x = minimizeX;
		floatbar->buttons[BUTTON_MINIMIZE]->y = BUTTON_Y;
	}
	if (floatbar->buttons[BUTTON_RESTORE])
	{
		floatbar->buttons[BUTTON_RESTORE]->x = restoreX;
		floatbar->buttons[BUTTON_RESTORE]->y = BUTTON_Y;
	}
	if (floatbar->buttons[BUTTON_CLOSE])
	{
		floatbar->buttons[BUTTON_CLOSE]->x = closeX;
		floatbar->buttons[BUTTON_CLOSE]->y = BUTTON_Y;
	}
	if (floatbar->buttons[BUTTON_LOCKPIN])
	{
		floatbar->buttons[BUTTON_LOCKPIN]->x = LOCK_X;
		floatbar->buttons[BUTTON_LOCKPIN]->y = BUTTON_Y;
	}

	SetRect(&floatbar->textRect, TEXT_X, 0, floatbar->width - rightButtonsWidth, BACKGROUND_H);

	if (floatbar->hwnd)
		floatbar_update_region(floatbar);
}

static BOOL floatbar_set_geometry(wfFloatBar* floatbar, LONG x, LONG y, LONG height)
{
	if (!floatbar || !floatbar->hwnd)
		return FALSE;

	if ((floatbar->rect.left == x) && (floatbar->rect.top == y) &&
	    ((floatbar->rect.bottom - floatbar->rect.top) == height) &&
	    ((floatbar->rect.right - floatbar->rect.left) == floatbar->width))
		return TRUE;

	if (!SetWindowPos(floatbar->hwnd, nullptr, x, y, floatbar->width, height,
	                  SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER))
	{
		WLog_ERR(TAG, "SetWindowPos failed with %08" PRIx32, GetLastError());
		return FALSE;
	}

	floatbar->rect.left = x;
	floatbar->rect.top = y;
	floatbar->rect.right = x + floatbar->width;
	floatbar->rect.bottom = y + height;
	return TRUE;
}

static void floatbar_clear_hover(wfFloatBar* floatbar)
{
	if (!floatbar)
		return;

	for (size_t i = 0; i < ARRAYSIZE(floatbar->buttons); i++)
	{
		if (floatbar->buttons[i])
			floatbar->buttons[i]->active = FALSE;
	}
	floatbar->hoverButton = nullptr;
}

static BOOL floatbar_kill_timers(wfFloatBar* floatbar)
{
	if (!floatbar)
		return FALSE;

	KillTimer(floatbar->hwnd, TIMER_HIDE);
	KillTimer(floatbar->hwnd, TIMER_SHOW);
	return TRUE;
}

static BOOL floatbar_trigger_hide(wfFloatBar* floatbar)
{
	if (!floatbar_kill_timers(floatbar))
		return FALSE;

	if (!floatbar->locked && floatbar->shown && !floatbar->mouseInside)
	{
		if (SetTimer(floatbar->hwnd, TIMER_HIDE, 3000, nullptr) == 0)
		{
			WLog_ERR(TAG, "SetTimer failed with %08" PRIx32, GetLastError());
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL floatbar_hide(wfFloatBar* floatbar)
{
	RECT parentRect = { 0 };

	if (!floatbar_kill_timers(floatbar))
		return FALSE;
	if (!floatbar_parent_rect(floatbar, &parentRect))
		return FALSE;

	floatbar_update_layout(floatbar);
	floatbar->shown = FALSE;
	floatbar->mouseInside = FALSE;
	floatbar_clear_hover(floatbar);

	if (!floatbar_set_geometry(floatbar, floatbar->rect.left, parentRect.top, FLOATBAR_PEEK_HEIGHT))
		return FALSE;

	InvalidateRect(floatbar->hwnd, nullptr, FALSE);
	return TRUE;
}

static BOOL floatbar_show(wfFloatBar* floatbar)
{
	RECT parentRect = { 0 };

	if (!floatbar_kill_timers(floatbar))
		return FALSE;
	if (!floatbar_parent_rect(floatbar, &parentRect))
		return FALSE;

	floatbar_update_layout(floatbar);
	floatbar->shown = TRUE;

	if (!floatbar_set_geometry(floatbar, floatbar->rect.left, parentRect.top, floatbar->height))
		return FALSE;

	InvalidateRect(floatbar->hwnd, nullptr, FALSE);
	return floatbar_trigger_hide(floatbar);
}

static BOOL floatbar_update_locked_button(FloatbarButton* button, BOOL locked)
{
	if (!button)
		return FALSE;

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
	return TRUE;
}

static int floatbar_button_hit(FloatbarButton* button)
{
	wfFloatBar* floatbar;

	if (!button)
		return 0;

	floatbar = button->floatbar;

	switch (button->type)
	{
		case BUTTON_LOCKPIN:
			floatbar->locked = !floatbar->locked;
			floatbar_update_locked_button(button, floatbar->locked);
			break;

		case BUTTON_MINIMIZE:
			ShowWindow(floatbar->parent, SW_MINIMIZE);
			break;

		case BUTTON_RESTORE:
			floatbar_kill_timers(floatbar);
			ReleaseCapture();
			wf_toggle_fullscreen(floatbar->wfc);
			break;

		case BUTTON_CLOSE:
			PostMessage(floatbar->parent, WM_CLOSE, 0, 0);
			break;

		default:
			break;
	}

	return 0;
}

static int floatbar_button_paint(const FloatbarButton* button, HDC hdc)
{
	wfFloatBar* floatbar;
	BLENDFUNCTION blend = { 0 };

	if (!button)
		return 0;

	floatbar = button->floatbar;
	if (!button->bmp || !button->bmp_act)
		return 0;
	SelectObject(floatbar->hdcmem, button->active ? button->bmp_act : button->bmp);
	blend.BlendOp = AC_SRC_OVER;
	blend.SourceConstantAlpha = 255;
	blend.AlphaFormat = AC_SRC_ALPHA;
	AlphaBlend(hdc, button->x, button->y, button->w, button->h, floatbar->hdcmem, 0, 0, button->w,
	           button->h, blend);
	return 0;
}

static FloatbarButton* floatbar_create_button(wfFloatBar* floatbar, int type, int resid,
                                              int residAct, int x, int y, int h, int w)
{
	FloatbarButton* button = (FloatbarButton*)calloc(1, sizeof(FloatbarButton));

	if (!button)
		return nullptr;

	button->floatbar = floatbar;
	button->type = type;
	button->x = x;
	button->y = y;
	button->w = w;
	button->h = h;
	button->bmp = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(resid), IMAGE_BITMAP, 0,
	                                 0, LR_DEFAULTCOLOR);
	button->bmp_act = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(residAct),
	                                     IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	return button;
}

static FloatbarButton* floatbar_create_lock_button(wfFloatBar* floatbar, int unlockResid,
                                                   int unlockResidAct, int lockResid,
                                                   int lockResidAct, int x, int y, int h, int w)
{
	FloatbarButton* button =
	    floatbar_create_button(floatbar, BUTTON_LOCKPIN, unlockResid, unlockResidAct, x, y, h, w);

	if (!button)
		return nullptr;

	button->unlocked_bmp = button->bmp;
	button->unlocked_bmp_act = button->bmp_act;
	button->locked_bmp = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(lockResid),
	                                        IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	button->locked_bmp_act = (HBITMAP)LoadImage(floatbar->root_window, MAKEINTRESOURCE(lockResidAct),
	                                            IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	return button;
}

static FloatbarButton* floatbar_get_button(const wfFloatBar* floatbar, int x, int y)
{
	if (!floatbar || !floatbar->shown)
		return nullptr;

	if ((y <= BUTTON_Y) || (y >= (BUTTON_Y + BUTTON_HEIGHT)))
		return nullptr;

	for (size_t i = 0; i < ARRAYSIZE(floatbar->buttons); i++)
	{
		FloatbarButton* button = floatbar->buttons[i];
		if (!button)
			continue;

		if ((x > button->x) && (x < (button->x + button->w)))
			return button;
	}

	return nullptr;
}

static BOOL floatbar_paint(wfFloatBar* floatbar, HDC hdc)
{
	HPEN pen;
	HGDIOBJECT oldPen;
	RECT client = { 0 };
	GRADIENT_RECT gradientRect = { 0, 1 };
	COLORREF rgbTop = RGB(117, 154, 198);
	COLORREF rgbBottom = RGB(6, 55, 120);
	int left = 0;
	int right = 0;
	int bottom = 0;
	const int top = 0;
	const int angleOffset = BACKGROUND_H - 1;
	const WCHAR* title;
	size_t titleLength;
	TRIVERTEX vertices[2];

	if (!floatbar)
		return FALSE;

	GetClientRect(floatbar->hwnd, &client);
	if ((client.bottom - client.top) <= FLOATBAR_PEEK_HEIGHT)
	{
		HBRUSH strip = CreateSolidBrush(rgbBottom);
		if (strip)
		{
			FillRect(hdc, &client, strip);
			DeleteObject(strip);
		}
		return TRUE;
	}

	right = floatbar->width - 1;
	bottom = floatbar->height - 1;
	vertices[0].x = left;
	vertices[0].y = top;
	vertices[0].Red = GetRValue(rgbTop) << 8;
	vertices[0].Green = GetGValue(rgbTop) << 8;
	vertices[0].Blue = GetBValue(rgbTop) << 8;
	vertices[0].Alpha = 0x0000;
	vertices[1].x = right;
	vertices[1].y = bottom;
	vertices[1].Red = GetRValue(rgbBottom) << 8;
	vertices[1].Green = GetGValue(rgbBottom) << 8;
	vertices[1].Blue = GetBValue(rgbBottom) << 8;
	vertices[1].Alpha = 0x0000;
	GradientFill(hdc, vertices, 2, &gradientRect, 1, GRADIENT_FILL_RECT_V);

	pen = CreatePen(PS_SOLID, 1, RGB(71, 71, 71));
	oldPen = SelectObject(hdc, pen);
	MoveToEx(hdc, left, top, nullptr);
	LineTo(hdc, left + angleOffset, bottom);
	LineTo(hdc, right - angleOffset, bottom);
	LineTo(hdc, right + 1, top - 1);
	SelectObject(hdc, oldPen);
	DeleteObject(pen);

	pen = CreatePen(PS_SOLID, 1, RGB(107, 141, 184));
	oldPen = SelectObject(hdc, pen);
	left += 1;
	right -= 1;
	bottom -= 1;
	MoveToEx(hdc, left, top, nullptr);
	LineTo(hdc, left + angleOffset - 1, bottom);
	LineTo(hdc, right - angleOffset + 1, bottom);
	LineTo(hdc, right + 1, top - 1);
	SelectObject(hdc, oldPen);
	DeleteObject(pen);

	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));
	if (floatbar->titleFont)
		SelectObject(hdc, floatbar->titleFont);

	title = floatbar->wfc ? floatbar->wfc->window_title : nullptr;
	titleLength = title ? wcslen(title) : 0;
	if (titleLength > 0)
	{
		DrawTextW(hdc, title, (int)titleLength, &floatbar->textRect,
		          DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | DT_SINGLELINE);
	}

	for (size_t i = 0; i < ARRAYSIZE(floatbar->buttons); i++)
		floatbar_button_paint(floatbar->buttons[i], hdc);

	return TRUE;
}

static LRESULT CALLBACK floatbar_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static BOOL dragging = FALSE;
	static BOOL leftButtonDown = FALSE;
	static int dragOffsetX = 0;
	static wfFloatBar* floatbar = nullptr;
	static TRACKMOUSEEVENT tme = { 0 };
	PAINTSTRUCT ps = { 0 };
	HDC hdc;
	FloatbarButton* button;
	int posX;
	int posY;
	int xScreen = GetSystemMetrics(SM_CXSCREEN);

	switch (msg)
	{
		case WM_CREATE:
		{
			NONCLIENTMETRICS ncm = { 0 };

			floatbar = ((wfFloatBar*)((CREATESTRUCT*)lParam)->lpCreateParams);
			floatbar->hwnd = hWnd;
			GetWindowRect(hWnd, &floatbar->rect);
			floatbar->width = floatbar->rect.right - floatbar->rect.left;
			floatbar->height = BACKGROUND_H;

			hdc = GetDC(hWnd);
			floatbar->hdcmem = CreateCompatibleDC(hdc);
			ncm.cbSize = sizeof(NONCLIENTMETRICS);
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
			floatbar->titleFont = CreateFontIndirect(&ncm.lfCaptionFont);
			ReleaseDC(hWnd, hdc);

			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			floatbar_update_layout(floatbar);
			floatbar_trigger_hide(floatbar);
			return 0;
		}

		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			floatbar_paint(floatbar, hdc);
			EndPaint(hWnd, &ps);
			return 0;

		case WM_LBUTTONDOWN:
			posX = GET_X_LPARAM(lParam);
			posY = GET_Y_LPARAM(lParam);
			floatbar->mouseInside = TRUE;
			floatbar_kill_timers(floatbar);
			TrackMouseEvent(&tme);

			if (!floatbar->locked && !floatbar->shown)
			{
				floatbar_show(floatbar);
				posX = GET_X_LPARAM(lParam);
				posY = GET_Y_LPARAM(lParam);
			}

			button = floatbar_get_button(floatbar, posX, posY);
			if (button)
				leftButtonDown = TRUE;
			else if (floatbar->shown)
			{
				SetCapture(hWnd);
				dragging = TRUE;
				dragOffsetX = posX;
			}
			return 0;

		case WM_LBUTTONUP:
			posX = GET_X_LPARAM(lParam);
			posY = GET_Y_LPARAM(lParam);
			ReleaseCapture();
			dragging = FALSE;

			if (leftButtonDown)
			{
				button = floatbar_get_button(floatbar, posX, posY);
				if (button)
					floatbar_button_hit(button);
				leftButtonDown = FALSE;
			}
			return 0;

		case WM_CAPTURECHANGED:
			dragging = FALSE;
			return 0;

		case WM_MOUSEMOVE:
			posX = GET_X_LPARAM(lParam);
			posY = GET_Y_LPARAM(lParam);
			floatbar->mouseInside = TRUE;
			TrackMouseEvent(&tme);

			if (!floatbar->shown)
			{
				KillTimer(floatbar->hwnd, TIMER_HIDE);
				if (SetTimer(floatbar->hwnd, TIMER_SHOW, 150, nullptr) == 0)
					WLog_WARN(TAG, "SetTimer failed with %08" PRIx32, GetLastError());
			}
			else
			{
				KillTimer(floatbar->hwnd, TIMER_SHOW);
			}

			if (dragging && floatbar->shown)
			{
				RECT parentRect = { 0 };
				LONG leftEdge;
				int y;

				floatbar->rect.left = floatbar->rect.left + posX - dragOffsetX;
				leftEdge = xScreen - floatbar->width;
				if (floatbar->rect.left < 0)
					floatbar->rect.left = 0;
				else if (floatbar->rect.left > leftEdge)
					floatbar->rect.left = leftEdge;

				y = floatbar->rect.top;
				if (floatbar_parent_rect(floatbar, &parentRect))
					y = parentRect.top;
				floatbar_set_geometry(floatbar, floatbar->rect.left, y,
				                      floatbar->shown ? floatbar->height : FLOATBAR_PEEK_HEIGHT);
			}
			else
			{
				FloatbarButton* hover = floatbar_get_button(floatbar, posX, posY);
				if (hover != floatbar->hoverButton)
				{
					floatbar_clear_hover(floatbar);
					if (hover)
						hover->active = TRUE;
					floatbar->hoverButton = hover;
					InvalidateRect(hWnd, nullptr, FALSE);
				}
			}
			return 0;

		case WM_MOUSELEAVE:
			floatbar->mouseInside = FALSE;
			floatbar_clear_hover(floatbar);
			InvalidateRect(hWnd, nullptr, FALSE);
			floatbar_trigger_hide(floatbar);
			return 0;

		case WM_TIMER:
			if (wParam == TIMER_HIDE)
				floatbar_hide(floatbar);
			else if (wParam == TIMER_SHOW)
				floatbar_show(floatbar);
			return 0;

		case WM_DESTROY:
			floatbar_kill_timers(floatbar);
			DeleteDC(floatbar->hdcmem);
			floatbar->hdcmem = nullptr;
			floatbar->hwnd = nullptr;
			return 0;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

static BOOL floatbar_window_create(wfFloatBar* floatbar)
{
	WNDCLASSEX wndCls = { 0 };
	HWND barWnd;
	RECT parentRect = { 0 };
	LONG x;

	if (!floatbar || !floatbar_parent_rect(floatbar, &parentRect))
		return FALSE;

	floatbar_update_layout(floatbar);
	x = parentRect.left + ((parentRect.right - parentRect.left - floatbar->width) / 2);

	wndCls.cbSize = sizeof(WNDCLASSEX);
	wndCls.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wndCls.lpfnWndProc = floatbar_proc;
	wndCls.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wndCls.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wndCls.hbrBackground = nullptr;
	wndCls.lpszClassName = L"floatbar";
	wndCls.hInstance = floatbar->root_window;
	wndCls.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
	RegisterClassEx(&wndCls);

	barWnd = CreateWindowEx(WS_EX_TOOLWINDOW, L"floatbar", L"floatbar", WS_POPUP, x, parentRect.top,
	                        floatbar->width, BACKGROUND_H, floatbar->parent, nullptr,
	                        floatbar->root_window, floatbar);
	if (!barWnd)
		return FALSE;

	floatbar_update_region(floatbar);
	return TRUE;
}

void wf_floatbar_free(wfFloatBar* floatbar)
{
	if (!floatbar)
		return;

	floatbar_kill_timers(floatbar);
	if (floatbar->hwnd)
		DestroyWindow(floatbar->hwnd);

	for (size_t i = 0; i < ARRAYSIZE(floatbar->buttons); i++)
	{
		FloatbarButton* button = floatbar->buttons[i];
		if (!button)
			continue;

		DeleteObject(button->bmp);
		DeleteObject(button->bmp_act);
		if (button->type == BUTTON_LOCKPIN)
		{
			DeleteObject(button->locked_bmp);
			DeleteObject(button->locked_bmp_act);
		}
		free(button);
	}

	DeleteObject(floatbar->titleFont);
	free(floatbar);
}

wfFloatBar* wf_floatbar_new(wfContext* wfc, HINSTANCE window, DWORD flags)
{
	wfFloatBar* floatbar;

	if ((flags & 0x0001) == 0)
		return nullptr;
	if (!wfc)
		return nullptr;

	floatbar = (wfFloatBar*)calloc(1, sizeof(wfFloatBar));
	if (!floatbar)
		return nullptr;

	floatbar->root_window = window;
	floatbar->flags = flags;
	floatbar->wfc = wfc;
	floatbar->locked = (flags & 0x0002) != 0;
	floatbar->shown = (flags & 0x0006) != 0;
	floatbar->parent = wfc->hwnd;

	if (wfc->fullscreen_toggle)
	{
		floatbar->buttons[BUTTON_MINIMIZE] = floatbar_create_button(
		    floatbar, BUTTON_MINIMIZE, IDB_MINIMIZE, IDB_MINIMIZE_ACT, 0, BUTTON_Y, BUTTON_HEIGHT,
		    BUTTON_WIDTH);
		floatbar->buttons[BUTTON_RESTORE] = floatbar_create_button(
		    floatbar, BUTTON_RESTORE, IDB_RESTORE, IDB_RESTORE_ACT, 0, BUTTON_Y, BUTTON_HEIGHT,
		    BUTTON_WIDTH);
	}

	floatbar->buttons[BUTTON_CLOSE] = floatbar_create_button(floatbar, BUTTON_CLOSE, IDB_CLOSE,
	                                                         IDB_CLOSE_ACT, 0, BUTTON_Y,
	                                                         BUTTON_HEIGHT, BUTTON_WIDTH);
	floatbar->buttons[BUTTON_LOCKPIN] =
	    floatbar_create_lock_button(floatbar, IDB_UNLOCK, IDB_UNLOCK_ACT, IDB_LOCK, IDB_LOCK_ACT,
	                                LOCK_X, BUTTON_Y, BUTTON_HEIGHT, BUTTON_WIDTH);

	floatbar_update_layout(floatbar);

	if (!floatbar_window_create(floatbar))
		goto fail;
	if (!floatbar_update_locked_button(floatbar->buttons[BUTTON_LOCKPIN], floatbar->locked))
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
	if (!floatbar)
		return FALSE;

	if (floatbar_is_visible_in_mode(floatbar, fullscreen))
	{
		ShowWindow(floatbar->hwnd, SW_SHOWNORMAL);
		if (floatbar->shown)
			floatbar_show(floatbar);
		else
			floatbar_hide(floatbar);
	}
	else
	{
		ShowWindow(floatbar->hwnd, SW_HIDE);
	}

	return wf_floatbar_reset_position(floatbar);
}

BOOL wf_floatbar_reset_position(wfFloatBar* floatbar)
{
	RECT parentRect = { 0 };
	LONG x;
	LONG height;

	if (!floatbar || !floatbar->hwnd)
		return FALSE;
	if (!floatbar_parent_rect(floatbar, &parentRect))
		return FALSE;

	floatbar_update_layout(floatbar);
	x = parentRect.left + ((parentRect.right - parentRect.left - floatbar->width) / 2);
	height = floatbar->shown ? floatbar->height : FLOATBAR_PEEK_HEIGHT;
	return floatbar_set_geometry(floatbar, x, parentRect.top, height);
}
