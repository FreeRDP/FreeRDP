/**
 * WinPR: Windows Portable Runtime
 * Window Notification System
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/wnd.h>

#ifndef _WIN32

WORD WINAPI GetWindowWord(HWND hWnd, int nIndex)
{
	return 0;
}

WORD WINAPI SetWindowWord(HWND hWnd, int nIndex, WORD wNewWord)
{
	return 0;
}

LONG WINAPI GetWindowLongA(HWND hWnd, int nIndex)
{
	return 0;
}

LONG WINAPI GetWindowLongW(HWND hWnd, int nIndex)
{
	return 0;
}

LONG WINAPI SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
	return 0;
}

LONG WINAPI SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong)
{
	return 0;
}

LONG_PTR WINAPI GetWindowLongPtrA(HWND hWnd, int nIndex)
{
	return 0;
}

LONG_PTR WINAPI GetWindowLongPtrW(HWND hWnd, int nIndex)
{
	return 0;
}

LONG_PTR WINAPI SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
	return 0;
}

LONG_PTR WINAPI SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
	return 0;
}

BOOL WINAPI DestroyWindow(HWND hWnd)
{
	return TRUE;
}

VOID WINAPI PostQuitMessage(int nExitCode)
{

}

ATOM WINAPI RegisterClassA(CONST WNDCLASSA* lpWndClass)
{
	return 0;
}

ATOM WINAPI RegisterClassW(CONST WNDCLASSW* lpWndClass)
{
	return 0;
}

ATOM WINAPI RegisterClassExA(CONST WNDCLASSEXA* lpwcx)
{
	return 0;
}

ATOM WINAPI RegisterClassExW(CONST WNDCLASSEXW* lpwcx)
{
	return 0;
}

BOOL WINAPI UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance)
{
	return TRUE;
}

BOOL WINAPI UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance)
{
	return TRUE;
}

HWND WINAPI CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
		LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
		HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	return NULL;
}

HWND WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName,
		LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
		HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	return NULL;
}

BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	return TRUE;
}

BOOL WINAPI GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax)
{
	return TRUE;
}

BOOL WINAPI TranslateMessage(CONST MSG* lpMsg)
{
	return TRUE;
}

LRESULT WINAPI DispatchMessageA(CONST MSG* lpMsg)
{
	return 0;
}

LRESULT WINAPI DispatchMessageW(CONST MSG* lpMsg)
{
	return 0;
}

LRESULT WINAPI CallWindowProcA(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT WINAPI CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

#endif
