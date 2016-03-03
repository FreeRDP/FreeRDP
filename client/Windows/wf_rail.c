/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/log.h>
#include <winpr/tchar.h>
#include <winpr/print.h>

#include "wf_rail.h"

#define TAG CLIENT_TAG("windows")

#define GET_X_LPARAM(lParam) ((UINT16) (lParam & 0xFFFF))
#define GET_Y_LPARAM(lParam) ((UINT16) ((lParam >> 16) & 0xFFFF))

/* RemoteApp Core Protocol Extension */

struct _WINDOW_STYLE
{
	UINT32 style;
	const char* name;
	BOOL multi;
};
typedef struct _WINDOW_STYLE WINDOW_STYLE;

static const WINDOW_STYLE WINDOW_STYLES[] =
{
	{ WS_BORDER, "WS_BORDER", FALSE },
	{ WS_CAPTION, "WS_CAPTION", FALSE },
	{ WS_CHILD, "WS_CHILD", FALSE },
	{ WS_CLIPCHILDREN, "WS_CLIPCHILDREN", FALSE },
	{ WS_CLIPSIBLINGS, "WS_CLIPSIBLINGS", FALSE },
	{ WS_DISABLED, "WS_DISABLED", FALSE },
	{ WS_DLGFRAME, "WS_DLGFRAME", FALSE },
	{ WS_GROUP, "WS_GROUP", FALSE },
	{ WS_HSCROLL, "WS_HSCROLL", FALSE },
	{ WS_ICONIC, "WS_ICONIC", FALSE },
	{ WS_MAXIMIZE, "WS_MAXIMIZE", FALSE },
	{ WS_MAXIMIZEBOX, "WS_MAXIMIZEBOX", FALSE },
	{ WS_MINIMIZE, "WS_MINIMIZE", FALSE },
	{ WS_MINIMIZEBOX, "WS_MINIMIZEBOX", FALSE },
	{ WS_OVERLAPPED, "WS_OVERLAPPED", FALSE },
	{ WS_OVERLAPPEDWINDOW, "WS_OVERLAPPEDWINDOW", TRUE },
	{ WS_POPUP, "WS_POPUP", FALSE },
	{ WS_POPUPWINDOW, "WS_POPUPWINDOW", TRUE },
	{ WS_SIZEBOX, "WS_SIZEBOX", FALSE },
	{ WS_SYSMENU, "WS_SYSMENU", FALSE },
	{ WS_TABSTOP, "WS_TABSTOP", FALSE },
	{ WS_THICKFRAME, "WS_THICKFRAME", FALSE },
	{ WS_VISIBLE, "WS_VISIBLE", FALSE }
};

static const WINDOW_STYLE EXTENDED_WINDOW_STYLES[] =
{
	{ WS_EX_ACCEPTFILES, "WS_EX_ACCEPTFILES", FALSE },
	{ WS_EX_APPWINDOW, "WS_EX_APPWINDOW", FALSE },
	{ WS_EX_CLIENTEDGE, "WS_EX_CLIENTEDGE", FALSE },
	{ WS_EX_COMPOSITED, "WS_EX_COMPOSITED", FALSE },
	{ WS_EX_CONTEXTHELP, "WS_EX_CONTEXTHELP", FALSE },
	{ WS_EX_CONTROLPARENT, "WS_EX_CONTROLPARENT", FALSE },
	{ WS_EX_DLGMODALFRAME, "WS_EX_DLGMODALFRAME", FALSE },
	{ WS_EX_LAYERED, "WS_EX_LAYERED", FALSE },
	{ WS_EX_LAYOUTRTL, "WS_EX_LAYOUTRTL", FALSE },
	{ WS_EX_LEFT, "WS_EX_LEFT", FALSE },
	{ WS_EX_LEFTSCROLLBAR, "WS_EX_LEFTSCROLLBAR", FALSE },
	{ WS_EX_LTRREADING, "WS_EX_LTRREADING", FALSE },
	{ WS_EX_MDICHILD, "WS_EX_MDICHILD", FALSE },
	{ WS_EX_NOACTIVATE, "WS_EX_NOACTIVATE", FALSE },
	{ WS_EX_NOINHERITLAYOUT, "WS_EX_NOINHERITLAYOUT", FALSE },
	{ WS_EX_NOPARENTNOTIFY, "WS_EX_NOPARENTNOTIFY", FALSE },
	{ WS_EX_OVERLAPPEDWINDOW, "WS_EX_OVERLAPPEDWINDOW", TRUE },
	{ WS_EX_PALETTEWINDOW, "WS_EX_PALETTEWINDOW", TRUE },
	{ WS_EX_RIGHT, "WS_EX_RIGHT", FALSE },
	{ WS_EX_RIGHTSCROLLBAR, "WS_EX_RIGHTSCROLLBAR", FALSE },
	{ WS_EX_RTLREADING, "WS_EX_RTLREADING", FALSE },
	{ WS_EX_STATICEDGE, "WS_EX_STATICEDGE", FALSE },
	{ WS_EX_TOOLWINDOW, "WS_EX_TOOLWINDOW", FALSE },
	{ WS_EX_TOPMOST, "WS_EX_TOPMOST", FALSE },
	{ WS_EX_TRANSPARENT, "WS_EX_TRANSPARENT", FALSE },
	{ WS_EX_WINDOWEDGE, "WS_EX_WINDOWEDGE", FALSE }
};

void PrintWindowStyles(UINT32 style)
{
	int i;
	
	WLog_INFO(TAG, "\tWindow Styles:\t{");
	for (i = 0; i < ARRAYSIZE(WINDOW_STYLES); i++)
	{
		if (style & WINDOW_STYLES[i].style)
		{
			if (WINDOW_STYLES[i].multi)
			{
				if ((style & WINDOW_STYLES[i].style) != WINDOW_STYLES[i].style)
					continue;
			}
			
			WLog_INFO(TAG, "\t\t%s", WINDOW_STYLES[i].name);
		}
	}
}

void PrintExtendedWindowStyles(UINT32 style)
{
	int i;
	
	WLog_INFO(TAG, "\tExtended Window Styles:\t{");
	for (i = 0; i < ARRAYSIZE(EXTENDED_WINDOW_STYLES); i++)
	{
		if (style & EXTENDED_WINDOW_STYLES[i].style)
		{
			if (EXTENDED_WINDOW_STYLES[i].multi)
			{
				if ((style & EXTENDED_WINDOW_STYLES[i].style) != EXTENDED_WINDOW_STYLES[i].style)
					continue;
			}
			
			WLog_INFO(TAG, "\t\t%s", EXTENDED_WINDOW_STYLES[i].name);
		}
	}
}

void PrintRailWindowState(WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_STATE_NEW)
		WLog_INFO(TAG, "WindowCreate: WindowId: 0x%04X", orderInfo->windowId);
	else
		WLog_INFO(TAG, "WindowUpdate: WindowId: 0x%04X", orderInfo->windowId);

	WLog_INFO(TAG, "{");

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		WLog_INFO(TAG, "\tOwnerWindowId: 0x%04X", windowState->ownerWindowId);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		WLog_INFO(TAG, "\tStyle: 0x%04X ExtendedStyle: 0x%04X",
			windowState->style, windowState->extendedStyle);
		
		PrintWindowStyles(windowState->style);
		PrintExtendedWindowStyles(windowState->extendedStyle);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		WLog_INFO(TAG, "\tShowState: %d", windowState->showState);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL);

		WLog_INFO(TAG, "\tTitleInfo: %s (length = %d)", title,
			windowState->titleInfo.length);

		free(title);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{
		WLog_INFO(TAG, "\tClientOffsetX: %d ClientOffsetY: %d",
			windowState->clientOffsetX, windowState->clientOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{
		WLog_INFO(TAG, "\tClientAreaWidth: %d ClientAreaHeight: %d",
			windowState->clientAreaWidth, windowState->clientAreaHeight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{
		WLog_INFO(TAG, "\tRPContent: %d", windowState->RPContent);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{
		WLog_INFO(TAG, "\tRootParentHandle: 0x%04X", windowState->rootParentHandle);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
	{
		WLog_INFO(TAG, "\tWindowOffsetX: %d WindowOffsetY: %d",
			windowState->windowOffsetX, windowState->windowOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{
		WLog_INFO(TAG, "\tWindowClientDeltaX: %d WindowClientDeltaY: %d",
			windowState->windowClientDeltaX, windowState->windowClientDeltaY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
	{
		WLog_INFO(TAG, "\tWindowWidth: %d WindowHeight: %d",
			windowState->windowWidth, windowState->windowHeight);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		UINT32 index;
		RECTANGLE_16* rect;

		WLog_INFO(TAG, "\tnumWindowRects: %d", windowState->numWindowRects);

		for (index = 0; index < windowState->numWindowRects; index++)
		{
			rect = &windowState->windowRects[index];

			WLog_INFO(TAG, "\twindowRect[%d]: left: %d top: %d right: %d bottom: %d",
				index, rect->left, rect->top, rect->right, rect->bottom);
		}
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{
		WLog_INFO(TAG, "\tvisibileOffsetX: %d visibleOffsetY: %d",
			windowState->visibleOffsetX, windowState->visibleOffsetY);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{
		UINT32 index;
		RECTANGLE_16* rect;

		WLog_INFO(TAG, "\tnumVisibilityRects: %d", windowState->numVisibilityRects);

		for (index = 0; index < windowState->numVisibilityRects; index++)
		{
			rect = &windowState->visibilityRects[index];

			WLog_INFO(TAG, "\tvisibilityRect[%d]: left: %d top: %d right: %d bottom: %d",
				index, rect->left, rect->top, rect->right, rect->bottom);
		}
	}

	WLog_INFO(TAG, "}");
}

static void PrintRailIconInfo(WINDOW_ORDER_INFO* orderInfo, ICON_INFO* iconInfo)
{
	WLog_INFO(TAG, "ICON_INFO");
	WLog_INFO(TAG, "{");

	WLog_INFO(TAG, "\tbigIcon: %s", (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? "true" : "false");
	WLog_INFO(TAG, "\tcacheEntry; 0x%04X", iconInfo->cacheEntry);
	WLog_INFO(TAG, "\tcacheId: 0x%04X", iconInfo->cacheId);
	WLog_INFO(TAG, "\tbpp: %d", iconInfo->bpp);
	WLog_INFO(TAG, "\twidth: %d", iconInfo->width);
	WLog_INFO(TAG, "\theight: %d", iconInfo->height);
	WLog_INFO(TAG, "\tcbColorTable: %d", iconInfo->cbColorTable);
	WLog_INFO(TAG, "\tcbBitsMask: %d", iconInfo->cbBitsMask);
	WLog_INFO(TAG, "\tcbBitsColor: %d", iconInfo->cbBitsColor);
	WLog_INFO(TAG, "\tcolorTable: %p", iconInfo->colorTable);
	WLog_INFO(TAG, "\tbitsMask: %p", iconInfo->bitsMask);
	WLog_INFO(TAG, "\tbitsColor: %p", iconInfo->bitsColor);

	WLog_INFO(TAG, "}");
}

LRESULT CALLBACK wf_RailWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int x, y;
	int width;
	int height;
	UINT32 xPos;
	UINT32 yPos;
	PAINTSTRUCT ps;
	UINT32 inputFlags;
	wfContext* wfc = NULL;
	rdpInput* input = NULL;
	rdpContext* context = NULL;
	wfRailWindow* railWindow;

	railWindow = (wfRailWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	if (railWindow)
		wfc = railWindow->wfc;

	if (wfc)
		context = (rdpContext*) wfc;

	if (context)
		input = context->input;

	switch (msg)
	{
		case WM_PAINT:
			{
				if (!wfc)
					return 0;

				hDC = BeginPaint(hWnd, &ps);

				x = ps.rcPaint.left;
				y = ps.rcPaint.top;
				width = ps.rcPaint.right - ps.rcPaint.left + 1;
				height = ps.rcPaint.bottom - ps.rcPaint.top + 1;

				BitBlt(hDC, x, y, width, height, wfc->primary->hdc,
					railWindow->x + x, railWindow->y + y, SRCCOPY);

				EndPaint(hWnd, &ps);
			}
			break;

		case WM_LBUTTONDOWN:
			{
				if (!railWindow || !input)
					return 0;

				xPos = GET_X_LPARAM(lParam) + railWindow->x;
				yPos = GET_Y_LPARAM(lParam) + railWindow->y;
				inputFlags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1;

				if (input)
					input->MouseEvent(input, inputFlags, xPos, yPos);
			}
			break;

		case WM_LBUTTONUP:
			{
				if (!railWindow || !input)
					return 0;

				xPos = GET_X_LPARAM(lParam) + railWindow->x;
				yPos = GET_Y_LPARAM(lParam) + railWindow->y;
				inputFlags = PTR_FLAGS_BUTTON1;

				if (input)
					input->MouseEvent(input, inputFlags, xPos, yPos);
			}
			break;

		case WM_RBUTTONDOWN:
			{
				if (!railWindow || !input)
					return 0;

				xPos = GET_X_LPARAM(lParam) + railWindow->x;
				yPos = GET_Y_LPARAM(lParam) + railWindow->y;
				inputFlags = PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2;

				if (input)
					input->MouseEvent(input, inputFlags, xPos, yPos);
			}
			break;

		case WM_RBUTTONUP:
			{
				if (!railWindow || !input)
					return 0;

				xPos = GET_X_LPARAM(lParam) + railWindow->x;
				yPos = GET_Y_LPARAM(lParam) + railWindow->y;
				inputFlags = PTR_FLAGS_BUTTON2;

				if (input)
					input->MouseEvent(input, inputFlags, xPos, yPos);
			}
			break;

		case WM_MOUSEMOVE:
			{
				if (!railWindow || !input)
					return 0;

				xPos = GET_X_LPARAM(lParam) + railWindow->x;
				yPos = GET_Y_LPARAM(lParam) + railWindow->y;
				inputFlags = PTR_FLAGS_MOVE;

				if (input)
					input->MouseEvent(input, inputFlags, xPos, yPos);
			}
			break;

		case WM_MOUSEWHEEL:
			break;

		case WM_CLOSE:
			DestroyWindow(hWnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return 0;
}

#define RAIL_DISABLED_WINDOW_STYLES (WS_BORDER | WS_THICKFRAME | WS_DLGFRAME | WS_CAPTION | \
	WS_OVERLAPPED | WS_VSCROLL | WS_HSCROLL | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define RAIL_DISABLED_EXTENDED_WINDOW_STYLES (WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE)

static BOOL wf_rail_window_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_STATE_ORDER* windowState)
{
	wfRailWindow* railWindow = NULL;
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;
	UINT32 fieldFlags = orderInfo->fieldFlags;

	PrintRailWindowState(orderInfo, windowState);

	if (fieldFlags & WINDOW_ORDER_STATE_NEW)
	{
		HANDLE hInstance;
		WCHAR* titleW = NULL;
		WNDCLASSEX wndClassEx;

		railWindow = (wfRailWindow*) calloc(1, sizeof(wfRailWindow));

		if (!railWindow)
			return FALSE;

		railWindow->wfc = wfc;

		railWindow->dwStyle = windowState->style;
		railWindow->dwStyle &= ~RAIL_DISABLED_WINDOW_STYLES;
		railWindow->dwExStyle = windowState->extendedStyle;
		railWindow->dwExStyle &= ~RAIL_DISABLED_EXTENDED_WINDOW_STYLES;

		railWindow->x = windowState->windowOffsetX;
		railWindow->y = windowState->windowOffsetY;
		railWindow->width = windowState->windowWidth;
		railWindow->height = windowState->windowHeight;

		if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
		{
			char* title = NULL;

			if (windowState->titleInfo.length == 0)
			{
				if (!(title = _strdup("")))
				{
					WLog_ERR(TAG, "failed to duplicate empty window title string");
					/* error handled below */
				}
			}
			else if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
				   windowState->titleInfo.length / 2, &title, 0, NULL, NULL) < 1)
			{
				WLog_ERR(TAG, "failed to convert window title");
				/* error handled below */
			}

			railWindow->title = title;
		}
		else
		{
			if (!(railWindow->title = _strdup("RdpRailWindow")))
				WLog_ERR(TAG, "failed to duplicate default window title string");
		}

		if (!railWindow->title)
		{
			free(railWindow);
			return FALSE;
		}

		ConvertToUnicode(CP_UTF8, 0, railWindow->title, -1, &titleW, 0);

		hInstance = GetModuleHandle(NULL);

		ZeroMemory(&wndClassEx, sizeof(WNDCLASSEX));
		wndClassEx.cbSize = sizeof(WNDCLASSEX);
		wndClassEx.style = 0;
		wndClassEx.lpfnWndProc = wf_RailWndProc;
		wndClassEx.cbClsExtra = 0;
		wndClassEx.cbWndExtra = 0;
		wndClassEx.hIcon = NULL;
		wndClassEx.hCursor = NULL;
		wndClassEx.hbrBackground = NULL;
		wndClassEx.lpszMenuName = NULL;
		wndClassEx.lpszClassName = _T("RdpRailWindow");
		wndClassEx.hInstance = hInstance;
		wndClassEx.hIconSm = NULL;

		RegisterClassEx(&wndClassEx);

		railWindow->hWnd = CreateWindowExW(
				railWindow->dwExStyle, /* dwExStyle */
				_T("RdpRailWindow"), /* lpClassName */
				titleW, /* lpWindowName */
				railWindow->dwStyle, /* dwStyle */
				railWindow->x, /* x */
				railWindow->y, /* y */
				railWindow->width, /* nWidth */
				railWindow->height, /* nHeight */
				NULL, /* hWndParent */
				NULL, /* hMenu */
				hInstance, /* hInstance */
				NULL /* lpParam */
				);

		SetWindowLongPtr(railWindow->hWnd, GWLP_USERDATA, (LONG_PTR) railWindow);

		HashTable_Add(wfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId, (void*) railWindow);

		free(titleW);

		UpdateWindow(railWindow->hWnd);

		return TRUE;
	}
	else
	{
		railWindow = (wfRailWindow*) HashTable_GetItemValue(wfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);
	}

	if (!railWindow)
		return TRUE;

	if ((fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET) ||
		(fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE))
	{
		if (fieldFlags & WINDOW_ORDER_FIELD_WND_OFFSET)
		{
			railWindow->x = windowState->windowOffsetX;
			railWindow->y = windowState->windowOffsetY;
		}

		if (fieldFlags & WINDOW_ORDER_FIELD_WND_SIZE)
		{
			railWindow->width = windowState->windowWidth;
			railWindow->height = windowState->windowHeight;
		}

		SetWindowPos(railWindow->hWnd, NULL,
			railWindow->x,
			railWindow->y,
			railWindow->width,
			railWindow->height,
			0);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_OWNER)
	{
		
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_STYLE)
	{
		railWindow->dwStyle = windowState->style;
		railWindow->dwStyle &= ~RAIL_DISABLED_WINDOW_STYLES;
		railWindow->dwExStyle = windowState->extendedStyle;
		railWindow->dwExStyle &= ~RAIL_DISABLED_EXTENDED_WINDOW_STYLES;

		SetWindowLongPtr(railWindow->hWnd, GWL_STYLE, (LONG) railWindow->dwStyle);
		SetWindowLongPtr(railWindow->hWnd, GWL_EXSTYLE, (LONG) railWindow->dwExStyle);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_SHOW)
	{
		ShowWindow(railWindow->hWnd, windowState->showState);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_TITLE)
	{
		char* title = NULL;
		WCHAR* titleW = NULL;

		if (windowState->titleInfo.length == 0)
		{
			if (!(title = _strdup("")))
			{
				WLog_ERR(TAG, "failed to duplicate empty window title string");
				return FALSE;
			}
		}
		else if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) windowState->titleInfo.string,
			windowState->titleInfo.length / 2, &title, 0, NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert window title");
			return FALSE;
		}

		free(railWindow->title);
		railWindow->title = title;

		ConvertToUnicode(CP_UTF8, 0, railWindow->title, -1, &titleW, 0);

		SetWindowTextW(railWindow->hWnd, titleW);

		free(titleW);
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_OFFSET)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_CLIENT_AREA_SIZE)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_CLIENT_DELTA)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_RP_CONTENT)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_ROOT_PARENT)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_WND_RECTS)
	{
		UINT32 index;
		HRGN hWndRect;
		HRGN hWndRects;
		RECTANGLE_16* rect;

		if (windowState->numWindowRects > 0)
		{
			rect = &(windowState->windowRects[0]);
			hWndRects = CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);

			for (index = 1; index < windowState->numWindowRects; index++)
			{
				rect = &(windowState->windowRects[index]);
				hWndRect = CreateRectRgn(rect->left, rect->top, rect->right, rect->bottom);
				CombineRgn(hWndRects, hWndRects, hWndRect, RGN_OR);
				DeleteObject(hWndRect);
			}

			SetWindowRgn(railWindow->hWnd, hWndRects, TRUE);
			DeleteObject(hWndRects);
		}
	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VIS_OFFSET)
	{

	}

	if (fieldFlags & WINDOW_ORDER_FIELD_VISIBILITY)
	{

	}

	UpdateWindow(railWindow->hWnd);
	return TRUE;
}

static BOOL wf_rail_window_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfRailWindow* railWindow = NULL;
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailWindowDelete");

	railWindow = (wfRailWindow*) HashTable_GetItemValue(wfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!railWindow)
		return TRUE;

	HashTable_Remove(wfc->railWindows, (void*) (UINT_PTR) orderInfo->windowId);

	DestroyWindow(railWindow->hWnd);

	free(railWindow);
	return TRUE;
}

static BOOL wf_rail_window_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_ICON_ORDER* windowIcon)
{
	HDC hDC;
	int bpp;
	int width;
	int height;
	HICON hIcon;
	BOOL bigIcon;
	ICONINFO iconInfo;
	BITMAPINFO bitmapInfo;
	wfRailWindow* railWindow;
	BITMAPINFOHEADER* bitmapInfoHeader;
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailWindowIcon");

	PrintRailIconInfo(orderInfo, windowIcon->iconInfo);

	railWindow = (wfRailWindow*) HashTable_GetItemValue(wfc->railWindows,
			(void*) (UINT_PTR) orderInfo->windowId);

	if (!railWindow)
		return TRUE;

	bigIcon = (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_ICON_BIG) ? TRUE : FALSE;

	hDC = GetDC(railWindow->hWnd);

	iconInfo.fIcon = TRUE;
	iconInfo.xHotspot = 0;
	iconInfo.yHotspot = 0;

	ZeroMemory(&bitmapInfo, sizeof(BITMAPINFO));
	bitmapInfoHeader = &(bitmapInfo.bmiHeader);

	bpp = windowIcon->iconInfo->bpp;
	width = windowIcon->iconInfo->width;
	height = windowIcon->iconInfo->height;

	bitmapInfoHeader->biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfoHeader->biWidth = width;
	bitmapInfoHeader->biHeight = height;
	bitmapInfoHeader->biPlanes = 1;
	bitmapInfoHeader->biBitCount = bpp;
	bitmapInfoHeader->biCompression = 0;
	bitmapInfoHeader->biSizeImage = height * width * ((bpp + 7) / 8);
	bitmapInfoHeader->biXPelsPerMeter = width;
	bitmapInfoHeader->biYPelsPerMeter = height;
	bitmapInfoHeader->biClrUsed = 0;
	bitmapInfoHeader->biClrImportant = 0;

	iconInfo.hbmMask = CreateDIBitmap(hDC,
		bitmapInfoHeader, CBM_INIT,
		windowIcon->iconInfo->bitsMask,
		&bitmapInfo, DIB_RGB_COLORS);

	iconInfo.hbmColor = CreateDIBitmap(hDC,
		bitmapInfoHeader, CBM_INIT,
		windowIcon->iconInfo->bitsColor,
		&bitmapInfo, DIB_RGB_COLORS);

	hIcon = CreateIconIndirect(&iconInfo);

	if (hIcon)
	{
		WPARAM wParam;
		LPARAM lParam;

		wParam = (WPARAM) bigIcon ? ICON_BIG : ICON_SMALL;
		lParam = (LPARAM) hIcon;

		SendMessage(railWindow->hWnd, WM_SETICON, wParam, lParam);
	}

	ReleaseDC(NULL, hDC);

	if (windowIcon->iconInfo->cacheEntry != 0xFFFF)
	{
		/* icon should be cached */
	}
	return TRUE;
}

static BOOL wf_rail_window_cached_icon(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, WINDOW_CACHED_ICON_ORDER* windowCachedIcon)
{
	WLog_DBG(TAG, "RailWindowCachedIcon");
	return TRUE;
}

static void wf_rail_notify_icon_common(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_VERSION)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_TIP)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_INFO_TIP)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_FIELD_NOTIFY_STATE)
	{

	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_ICON)
	{
		ICON_INFO* iconInfo = &(notifyIconState->icon);

		PrintRailIconInfo(orderInfo, iconInfo);
	}

	if (orderInfo->fieldFlags & WINDOW_ORDER_CACHED_ICON)
	{

	}
}

static BOOL wf_rail_notify_icon_create(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailNotifyIconCreate");

	wf_rail_notify_icon_common(context, orderInfo, notifyIconState);
	return TRUE;
}

static BOOL wf_rail_notify_icon_update(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, NOTIFY_ICON_STATE_ORDER* notifyIconState)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailNotifyIconUpdate");

	wf_rail_notify_icon_common(context, orderInfo, notifyIconState);
	return TRUE;
}

static BOOL wf_rail_notify_icon_delete(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailNotifyIconDelete");
	return TRUE;
}

static BOOL wf_rail_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo, MONITORED_DESKTOP_ORDER* monitoredDesktop)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailMonitorDesktop");
	return TRUE;
}

static BOOL wf_rail_non_monitored_desktop(rdpContext* context, WINDOW_ORDER_INFO* orderInfo)
{
	wfContext* wfc = (wfContext*) context;
	RailClientContext* rail = wfc->rail;

	WLog_DBG(TAG, "RailNonMonitorDesktop");
	return TRUE;
}

void wf_rail_register_update_callbacks(rdpUpdate* update)
{
	rdpWindowUpdate* window = update->window;

	window->WindowCreate = wf_rail_window_common;
	window->WindowUpdate = wf_rail_window_common;
	window->WindowDelete = wf_rail_window_delete;
	window->WindowIcon = wf_rail_window_icon;
	window->WindowCachedIcon = wf_rail_window_cached_icon;
	window->NotifyIconCreate = wf_rail_notify_icon_create;
	window->NotifyIconUpdate = wf_rail_notify_icon_update;
	window->NotifyIconDelete = wf_rail_notify_icon_delete;
	window->MonitoredDesktop = wf_rail_monitored_desktop;
	window->NonMonitoredDesktop = wf_rail_non_monitored_desktop;
}

/* RemoteApp Virtual Channel Extension */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_execute_result(RailClientContext* context, RAIL_EXEC_RESULT_ORDER* execResult)
{
	WLog_DBG(TAG, "RailServerExecuteResult: 0x%04X", execResult->rawResult);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_system_param(RailClientContext* context, RAIL_SYSPARAM_ORDER* sysparam)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_handshake(RailClientContext* context, RAIL_HANDSHAKE_ORDER* handshake)
{
	RAIL_EXEC_ORDER exec;
	RAIL_SYSPARAM_ORDER sysparam;
	RAIL_HANDSHAKE_ORDER clientHandshake;
	RAIL_CLIENT_STATUS_ORDER clientStatus;
	wfContext* wfc = (wfContext*) context->custom;
	rdpSettings* settings = wfc->settings;

	clientHandshake.buildNumber = 0x00001DB0;
	context->ClientHandshake(context, &clientHandshake);

	ZeroMemory(&clientStatus, sizeof(RAIL_CLIENT_STATUS_ORDER));
	clientStatus.flags = RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE;
	context->ClientInformation(context, &clientStatus);

	if (settings->RemoteAppLanguageBarSupported)
	{
		RAIL_LANGBAR_INFO_ORDER langBarInfo;
		langBarInfo.languageBarStatus = 0x00000008; /* TF_SFT_HIDDEN */
		context->ClientLanguageBarInfo(context, &langBarInfo);
	}

	ZeroMemory(&sysparam, sizeof(RAIL_SYSPARAM_ORDER));

	sysparam.params = 0;

	sysparam.params |= SPI_MASK_SET_HIGH_CONTRAST;
	sysparam.highContrast.colorScheme.string = NULL;
	sysparam.highContrast.colorScheme.length = 0;
	sysparam.highContrast.flags = 0x7E;

	sysparam.params |= SPI_MASK_SET_MOUSE_BUTTON_SWAP;
	sysparam.mouseButtonSwap = FALSE;

	sysparam.params |= SPI_MASK_SET_KEYBOARD_PREF;
	sysparam.keyboardPref = FALSE;

	sysparam.params |= SPI_MASK_SET_DRAG_FULL_WINDOWS;
	sysparam.dragFullWindows = FALSE;

	sysparam.params |= SPI_MASK_SET_KEYBOARD_CUES;
	sysparam.keyboardCues = FALSE;

	sysparam.params |= SPI_MASK_SET_WORK_AREA;
	sysparam.workArea.left = 0;
	sysparam.workArea.top = 0;
	sysparam.workArea.right = settings->DesktopWidth;
	sysparam.workArea.bottom = settings->DesktopHeight;

	sysparam.dragFullWindows = FALSE;

	context->ClientSystemParam(context, &sysparam);

	ZeroMemory(&exec, sizeof(RAIL_EXEC_ORDER));

	exec.RemoteApplicationProgram = settings->RemoteApplicationProgram;
	exec.RemoteApplicationWorkingDir = settings->ShellWorkingDirectory;
	exec.RemoteApplicationArguments = settings->RemoteApplicationCmdLine;

	context->ClientExecute(context, &exec);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_handshake_ex(RailClientContext* context, RAIL_HANDSHAKE_EX_ORDER* handshakeEx)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_local_move_size(RailClientContext* context, RAIL_LOCALMOVESIZE_ORDER* localMoveSize)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_min_max_info(RailClientContext* context, RAIL_MINMAXINFO_ORDER* minMaxInfo)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_language_bar_info(RailClientContext* context, RAIL_LANGBAR_INFO_ORDER* langBarInfo)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wf_rail_server_get_appid_response(RailClientContext* context, RAIL_GET_APPID_RESP_ORDER* getAppIdResp)
{
	return CHANNEL_RC_OK;
}

void wf_rail_invalidate_region(wfContext* wfc, REGION16* invalidRegion)
{
	int index;
	int count;
	RECT updateRect;
	RECTANGLE_16 windowRect;
	ULONG_PTR* pKeys = NULL;
	wfRailWindow* railWindow;
	const RECTANGLE_16* extents;
	REGION16 windowInvalidRegion;

	region16_init(&windowInvalidRegion);

	count = HashTable_GetKeys(wfc->railWindows, &pKeys);

	for (index = 0; index < count; index++)
	{
		railWindow = (wfRailWindow*) HashTable_GetItemValue(wfc->railWindows, (void*) pKeys[index]);

		if (railWindow)
		{
			windowRect.left = railWindow->x;
			windowRect.top = railWindow->y;
			windowRect.right = railWindow->x + railWindow->width;
			windowRect.bottom = railWindow->y + railWindow->height;

			region16_clear(&windowInvalidRegion);
			region16_intersect_rect(&windowInvalidRegion, invalidRegion, &windowRect);

			if (!region16_is_empty(&windowInvalidRegion))
			{
				extents = region16_extents(&windowInvalidRegion);

				updateRect.left = extents->left - railWindow->x;
				updateRect.top = extents->top - railWindow->y;
				updateRect.right = extents->right - railWindow->x;
				updateRect.bottom = extents->bottom - railWindow->y;

				InvalidateRect(railWindow->hWnd, &updateRect, FALSE);
			}
		}
	}

	region16_uninit(&windowInvalidRegion);
}

BOOL wf_rail_init(wfContext* wfc, RailClientContext* rail)
{
	rdpContext* context = (rdpContext*) wfc;

	wfc->rail = rail;
	rail->custom = (void*) wfc;

	rail->ServerExecuteResult = wf_rail_server_execute_result;
	rail->ServerSystemParam = wf_rail_server_system_param;
	rail->ServerHandshake = wf_rail_server_handshake;
	rail->ServerHandshakeEx = wf_rail_server_handshake_ex;
	rail->ServerLocalMoveSize = wf_rail_server_local_move_size;
	rail->ServerMinMaxInfo = wf_rail_server_min_max_info;
	rail->ServerLanguageBarInfo = wf_rail_server_language_bar_info;
	rail->ServerGetAppIdResponse = wf_rail_server_get_appid_response;

	wf_rail_register_update_callbacks(context->update);

	wfc->railWindows = HashTable_New(TRUE);
	return (wfc->railWindows != NULL);
}

void wf_rail_uninit(wfContext* wfc, RailClientContext* rail)
{
	wfc->rail = NULL;
	rail->custom = NULL;

	HashTable_Free(wfc->railWindows);
}
