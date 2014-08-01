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

#ifndef WINPR_WND_H
#define WINPR_WND_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/windows.h>

#ifndef _WIN32

#define WM_NULL					0x0000
#define WM_CREATE				0x0001
#define WM_DESTROY				0x0002
#define WM_MOVE					0x0003
#define WM_SIZE					0x0005
#define WM_ACTIVATE				0x0006
#define WM_SETFOCUS				0x0007
#define WM_KILLFOCUS				0x0008
#define WM_ENABLE				0x000A
#define WM_SETREDRAW				0x000B
#define WM_SETTEXT				0x000C
#define WM_GETTEXT				0x000D
#define WM_GETTEXTLENGTH			0x000E
#define WM_PAINT				0x000F
#define WM_CLOSE				0x0010
#define WM_QUERYENDSESSION			0x0011
#define WM_QUERYOPEN				0x0013
#define WM_ENDSESSION				0x0016
#define WM_QUIT					0x0012
#define WM_ERASEBKGND				0x0014
#define WM_SYSCOLORCHANGE			0x0015
#define WM_SHOWWINDOW				0x0018
#define WM_WININICHANGE				0x001A
#define WM_SETTINGCHANGE			0x001A
#define WM_DEVMODECHANGE			0x001B
#define WM_ACTIVATEAPP				0x001C
#define WM_FONTCHANGE				0x001D
#define WM_TIMECHANGE				0x001E
#define WM_CANCELMODE				0x001F
#define WM_SETCURSOR				0x0020
#define WM_MOUSEACTIVATE			0x0021
#define WM_CHILDACTIVATE			0x0022
#define WM_QUEUESYNC				0x0023
#define WM_GETMINMAXINFO			0x0024
#define WM_PAINTICON				0x0026
#define WM_ICONERASEBKGND			0x0027
#define WM_NEXTDLGCTL				0x0028
#define WM_SPOOLERSTATUS			0x002A
#define WM_DRAWITEM				0x002B
#define WM_MEASUREITEM				0x002C
#define WM_DELETEITEM				0x002D
#define WM_VKEYTOITEM				0x002E
#define WM_CHARTOITEM				0x002F
#define WM_SETFONT				0x0030
#define WM_GETFONT				0x0031
#define WM_SETHOTKEY				0x0032
#define WM_GETHOTKEY				0x0033
#define WM_QUERYDRAGICON			0x0037
#define WM_COMPAREITEM				0x0039
#define WM_GETOBJECT				0x003D
#define WM_COMPACTING				0x0041
#define WM_COMMNOTIFY				0x0044
#define WM_WINDOWPOSCHANGING			0x0046
#define WM_WINDOWPOSCHANGED			0x0047
#define WM_POWER				0x0048
#define WM_COPYDATA				0x004A
#define WM_CANCELJOURNAL			0x004B
#define WM_NOTIFY				0x004E
#define WM_INPUTLANGCHANGEREQUEST		0x0050
#define WM_INPUTLANGCHANGE			0x0051
#define WM_TCARD				0x0052
#define WM_HELP					0x0053
#define WM_USERCHANGED				0x0054
#define WM_NOTIFYFORMAT				0x0055
#define WM_CONTEXTMENU				0x007B
#define WM_STYLECHANGING			0x007C
#define WM_STYLECHANGED				0x007D
#define WM_DISPLAYCHANGE			0x007E
#define WM_GETICON				0x007F
#define WM_SETICON				0x0080
#define WM_NCCREATE				0x0081
#define WM_NCDESTROY				0x0082
#define WM_NCCALCSIZE				0x0083
#define WM_NCHITTEST				0x0084
#define WM_NCPAINT				0x0085
#define WM_NCACTIVATE				0x0086
#define WM_GETDLGCODE				0x0087
#define WM_SYNCPAINT				0x0088
#define WM_NCMOUSEMOVE				0x00A0
#define WM_NCLBUTTONDOWN			0x00A1
#define WM_NCLBUTTONUP				0x00A2
#define WM_NCLBUTTONDBLCLK			0x00A3
#define WM_NCRBUTTONDOWN			0x00A4
#define WM_NCRBUTTONUP				0x00A5
#define WM_NCRBUTTONDBLCLK			0x00A6
#define WM_NCMBUTTONDOWN			0x00A7
#define WM_NCMBUTTONUP				0x00A8
#define WM_NCMBUTTONDBLCLK			0x00A9
#define WM_NCXBUTTONDOWN			0x00AB
#define WM_NCXBUTTONUP				0x00AC
#define WM_NCXBUTTONDBLCLK			0x00AD
#define WM_INPUT_DEVICE_CHANGE			0x00FE
#define WM_INPUT				0x00FF
#define WM_KEYFIRST				0x0100
#define WM_KEYDOWN				0x0100
#define WM_KEYUP				0x0101
#define WM_CHAR					0x0102
#define WM_DEADCHAR				0x0103
#define WM_SYSKEYDOWN				0x0104
#define WM_SYSKEYUP				0x0105
#define WM_SYSCHAR				0x0106
#define WM_SYSDEADCHAR				0x0107
#define WM_UNICHAR				0x0109
#define WM_KEYLAST				0x0109
#define WM_IME_STARTCOMPOSITION			0x010D
#define WM_IME_ENDCOMPOSITION			0x010E
#define WM_IME_COMPOSITION			0x010F
#define WM_IME_KEYLAST				0x010F
#define WM_INITDIALOG				0x0110
#define WM_COMMAND				0x0111
#define WM_SYSCOMMAND				0x0112
#define WM_TIMER				0x0113
#define WM_HSCROLL				0x0114
#define WM_VSCROLL				0x0115
#define WM_INITMENU				0x0116
#define WM_INITMENUPOPUP			0x0117
#define WM_GESTURE				0x0119
#define WM_GESTURENOTIFY			0x011A
#define WM_MENUSELECT				0x011F
#define WM_MENUCHAR				0x0120
#define WM_ENTERIDLE				0x0121
#define WM_MENURBUTTONUP			0x0122
#define WM_MENUDRAG				0x0123
#define WM_MENUGETOBJECT			0x0124
#define WM_UNINITMENUPOPUP			0x0125
#define WM_MENUCOMMAND				0x0126
#define WM_CHANGEUISTATE			0x0127
#define WM_UPDATEUISTATE			0x0128
#define WM_QUERYUISTATE				0x0129
#define WM_CTLCOLORMSGBOX			0x0132
#define WM_CTLCOLOREDIT				0x0133
#define WM_CTLCOLORLISTBOX			0x0134
#define WM_CTLCOLORBTN				0x0135
#define WM_CTLCOLORDLG				0x0136
#define WM_CTLCOLORSCROLLBAR			0x0137
#define WM_CTLCOLORSTATIC			0x0138
#define WM_MOUSEFIRST				0x0200
#define WM_MOUSEMOVE				0x0200
#define WM_LBUTTONDOWN				0x0201
#define WM_LBUTTONUP				0x0202
#define WM_LBUTTONDBLCLK			0x0203
#define WM_RBUTTONDOWN				0x0204
#define WM_RBUTTONUP				0x0205
#define WM_RBUTTONDBLCLK			0x0206
#define WM_MBUTTONDOWN				0x0207
#define WM_MBUTTONUP				0x0208
#define WM_MBUTTONDBLCLK			0x0209
#define WM_MOUSEWHEEL				0x020A
#define WM_XBUTTONDOWN				0x020B
#define WM_XBUTTONUP				0x020C
#define WM_XBUTTONDBLCLK			0x020D
#define WM_MOUSEHWHEEL				0x020E
#define WM_MOUSELAST				0x020E
#define WM_PARENTNOTIFY				0x0210
#define WM_ENTERMENULOOP			0x0211
#define WM_EXITMENULOOP				0x0212
#define WM_NEXTMENU				0x0213
#define WM_SIZING				0x0214
#define WM_CAPTURECHANGED			0x0215
#define WM_MOVING				0x0216
#define WM_POWERBROADCAST			0x0218
#define WM_DEVICECHANGE				0x0219
#define WM_MDICREATE				0x0220
#define WM_MDIDESTROY				0x0221
#define WM_MDIACTIVATE				0x0222
#define WM_MDIRESTORE				0x0223
#define WM_MDINEXT				0x0224
#define WM_MDIMAXIMIZE				0x0225
#define WM_MDITILE				0x0226
#define WM_MDICASCADE				0x0227
#define WM_MDIICONARRANGE			0x0228
#define WM_MDIGETACTIVE				0x0229
#define WM_MDISETMENU				0x0230
#define WM_ENTERSIZEMOVE			0x0231
#define WM_EXITSIZEMOVE				0x0232
#define WM_DROPFILES				0x0233
#define WM_MDIREFRESHMENU			0x0234
#define WM_POINTERDEVICECHANGE			0x0238
#define WM_POINTERDEVICEINRANGE			0x0239
#define WM_POINTERDEVICEOUTOFRANGE		0x023A
#define WM_TOUCH				0x0240
#define WM_NCPOINTERUPDATE			0x0241
#define WM_NCPOINTERDOWN			0x0242
#define WM_NCPOINTERUP				0x0243
#define WM_POINTERUPDATE			0x0245
#define WM_POINTERDOWN				0x0246
#define WM_POINTERUP				0x0247
#define WM_POINTERENTER				0x0249
#define WM_POINTERLEAVE				0x024A
#define WM_POINTERACTIVATE			0x024B
#define WM_POINTERCAPTURECHANGED		0x024C
#define WM_TOUCHHITTESTING			0x024D
#define WM_POINTERWHEEL				0x024E
#define WM_POINTERHWHEEL			0x024F
#define WM_IME_SETCONTEXT			0x0281
#define WM_IME_NOTIFY				0x0282
#define WM_IME_CONTROL				0x0283
#define WM_IME_COMPOSITIONFULL			0x0284
#define WM_IME_SELECT				0x0285
#define WM_IME_CHAR				0x0286
#define WM_IME_REQUEST				0x0288
#define WM_IME_KEYDOWN				0x0290
#define WM_IME_KEYUP				0x0291
#define WM_MOUSEHOVER				0x02A1
#define WM_MOUSELEAVE				0x02A3
#define WM_NCMOUSEHOVER				0x02A0
#define WM_NCMOUSELEAVE				0x02A2
#define WM_WTSSESSION_CHANGE			0x02B1
#define WM_TABLET_FIRST				0x02c0
#define WM_TABLET_LAST				0x02df
#define WM_CUT					0x0300
#define WM_COPY					0x0301
#define WM_PASTE				0x0302
#define WM_CLEAR				0x0303
#define WM_UNDO					0x0304
#define WM_RENDERFORMAT				0x0305
#define WM_RENDERALLFORMATS			0x0306
#define WM_DESTROYCLIPBOARD			0x0307
#define WM_DRAWCLIPBOARD			0x0308
#define WM_PAINTCLIPBOARD			0x0309
#define WM_VSCROLLCLIPBOARD			0x030A
#define WM_SIZECLIPBOARD			0x030B
#define WM_ASKCBFORMATNAME			0x030C
#define WM_CHANGECBCHAIN			0x030D
#define WM_HSCROLLCLIPBOARD			0x030E
#define WM_QUERYNEWPALETTE			0x030F
#define WM_PALETTEISCHANGING			0x0310
#define WM_PALETTECHANGED			0x0311
#define WM_HOTKEY				0x0312
#define WM_PRINT				0x0317
#define WM_PRINTCLIENT				0x0318
#define WM_APPCOMMAND				0x0319
#define WM_THEMECHANGED				0x031A
#define WM_CLIPBOARDUPDATE			0x031D
#define WM_DWMCOMPOSITIONCHANGED		0x031E
#define WM_DWMNCRENDERINGCHANGED		0x031F
#define WM_DWMCOLORIZATIONCOLORCHANGED		0x0320
#define WM_DWMWINDOWMAXIMIZEDCHANGE		0x0321
#define WM_DWMSENDICONICTHUMBNAIL		0x0323
#define WM_DWMSENDICONICLIVEPREVIEWBITMAP	0x0326
#define WM_GETTITLEBARINFOEX			0x033F
#define WM_HANDHELDFIRST			0x0358
#define WM_HANDHELDLAST				0x035F
#define WM_AFXFIRST				0x0360
#define WM_AFXLAST				0x037F
#define WM_PENWINFIRST				0x0380
#define WM_PENWINLAST				0x038F
#define WM_APP					0x8000
#define WM_USER					0x0400

#define HWND_BROADCAST				((HWND)0xFFFF)
#define HWND_MESSAGE				((HWND)-3)
#define HWND_DESKTOP				((HWND)0)
#define HWND_TOP				((HWND)0)
#define HWND_BOTTOM				((HWND)1)
#define HWND_TOPMOST				((HWND)-1)
#define HWND_NOTOPMOST				((HWND)-2)

typedef WORD ATOM;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;

typedef FARPROC SENDASYNCPROC;
typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct tagWNDCLASSA
{
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
} WNDCLASSA, *PWNDCLASSA, NEAR *NPWNDCLASSA, FAR *LPWNDCLASSA;

typedef struct tagWNDCLASSW
{
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCWSTR lpszMenuName;
	LPCWSTR lpszClassName;
} WNDCLASSW, *PWNDCLASSW, NEAR *NPWNDCLASSW, FAR *LPWNDCLASSW;

typedef struct tagWNDCLASSEXA
{
	UINT cbSize;
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
	HICON hIconSm;
} WNDCLASSEXA, *PWNDCLASSEXA, NEAR *NPWNDCLASSEXA, FAR *LPWNDCLASSEXA;

typedef struct tagWNDCLASSEXW
{
	UINT cbSize;
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCWSTR lpszMenuName;
	LPCWSTR lpszClassName;
	HICON hIconSm;
} WNDCLASSEXW, *PWNDCLASSEXW, NEAR *NPWNDCLASSEXW, FAR *LPWNDCLASSEXW;

#ifdef UNICODE
typedef WNDCLASSW WNDCLASS;
typedef PWNDCLASSW PWNDCLASS;
typedef NPWNDCLASSW NPWNDCLASS;
typedef LPWNDCLASSW LPWNDCLASS;
typedef WNDCLASSEXW	WNDCLASSEX;
typedef PWNDCLASSEXW	PWNDCLASSEX;
typedef NPWNDCLASSEXW	NPWNDCLASSEX;
typedef LPWNDCLASSEXW	LPWNDCLASSEX;
#else
typedef WNDCLASSA WNDCLASS;
typedef PWNDCLASSA PWNDCLASS;
typedef NPWNDCLASSA NPWNDCLASS;
typedef LPWNDCLASSA LPWNDCLASS;
typedef WNDCLASSEXA	WNDCLASSEX;
typedef PWNDCLASSEXA	PWNDCLASSEX;
typedef NPWNDCLASSEXA	NPWNDCLASSEX;
typedef LPWNDCLASSEXA	LPWNDCLASSEX;
#endif

typedef struct tagPOINT
{
	LONG x;
	LONG y;
} POINT, *PPOINT, NEAR *NPPOINT, FAR *LPPOINT;

typedef struct tagMSG
{
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
} MSG, *PMSG, NEAR *NPMSG, FAR *LPMSG;

typedef struct tagCOPYDATASTRUCT
{
	ULONG_PTR dwData;
	DWORD cbData;
	PVOID lpData;
} COPYDATASTRUCT, *PCOPYDATASTRUCT;

typedef struct tagWTSSESSION_NOTIFICATION
{
	DWORD cbSize;
	DWORD dwSessionId;
} WTSSESSION_NOTIFICATION, *PWTSSESSION_NOTIFICATION;

#define WTS_CONSOLE_CONNECT			0x1
#define WTS_CONSOLE_DISCONNECT			0x2
#define WTS_REMOTE_CONNECT			0x3
#define WTS_REMOTE_DISCONNECT			0x4
#define WTS_SESSION_LOGON			0x5
#define WTS_SESSION_LOGOFF			0x6
#define WTS_SESSION_LOCK			0x7
#define WTS_SESSION_UNLOCK			0x8
#define WTS_SESSION_REMOTE_CONTROL		0x9
#define WTS_SESSION_CREATE			0xA
#define WTS_SESSION_TERMINATE			0xB

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API WORD WINAPI GetWindowWord(HWND hWnd, int nIndex);

WINPR_API WORD WINAPI SetWindowWord(HWND hWnd, int nIndex, WORD wNewWord);

WINPR_API LONG WINAPI GetWindowLongA(HWND hWnd, int nIndex);
WINPR_API LONG WINAPI GetWindowLongW(HWND hWnd, int nIndex);

WINPR_API LONG WINAPI SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
WINPR_API LONG WINAPI SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong);

WINPR_API LONG_PTR WINAPI GetWindowLongPtrA(HWND hWnd, int nIndex);
WINPR_API LONG_PTR WINAPI GetWindowLongPtrW(HWND hWnd, int nIndex);

WINPR_API LONG_PTR WINAPI SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
WINPR_API LONG_PTR WINAPI SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong);

WINPR_API BOOL WINAPI DestroyWindow(HWND hWnd);

WINPR_API VOID WINAPI PostQuitMessage(int nExitCode);

WINPR_API ATOM WINAPI RegisterClassA(CONST WNDCLASSA* lpWndClass);
WINPR_API ATOM WINAPI RegisterClassW(CONST WNDCLASSW* lpWndClass);

WINPR_API ATOM WINAPI RegisterClassExA(CONST WNDCLASSEXA* lpwcx);
WINPR_API ATOM WINAPI RegisterClassExW(CONST WNDCLASSEXW* lpwcx);

WINPR_API BOOL WINAPI UnregisterClassA(LPCSTR lpClassName, HINSTANCE hInstance);
WINPR_API BOOL WINAPI UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance);

WINPR_API HWND WINAPI CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName,
		LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
		HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
WINPR_API HWND WINAPI CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName,
		LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
		HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

#define CreateWindowA(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) \
		CreateWindowExA(0L, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)
#define CreateWindowW(lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam) \
		CreateWindowExW(0L, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)

WINPR_API HWND WINAPI FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName);
WINPR_API HWND WINAPI FindWindowW(LPCWSTR lpClassName, LPCWSTR lpWindowName);

WINPR_API HWND WINAPI FindWindowExA(HWND hWndParent, HWND hWndChildAfter, LPCSTR lpszClass, LPCSTR lpszWindow);
WINPR_API HWND WINAPI FindWindowExW(HWND hWndParent, HWND hWndChildAfter, LPCWSTR lpszClass, LPCWSTR lpszWindow);

WINPR_API BOOL WINAPI GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
WINPR_API BOOL WINAPI GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);

WINPR_API DWORD WINAPI GetMessagePos(VOID);

WINPR_API LONG WINAPI GetMessageTime(VOID);

WINPR_API LPARAM WINAPI GetMessageExtraInfo(VOID);

WINPR_API LPARAM WINAPI SetMessageExtraInfo(LPARAM lParam);

WINPR_API BOOL WINAPI SetMessageQueue(int cMessagesMax);

WINPR_API LRESULT WINAPI SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WINPR_API LRESULT WINAPI SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

WINPR_API LRESULT WINAPI SendMessageTimeoutA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam,
		UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);
WINPR_API LRESULT WINAPI SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam,
		UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult);

WINPR_API BOOL WINAPI SendNotifyMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WINPR_API BOOL WINAPI SendNotifyMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

WINPR_API BOOL WINAPI SendMessageCallbackA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam,
		SENDASYNCPROC lpResultCallBack, ULONG_PTR dwData);
WINPR_API BOOL WINAPI SendMessageCallbackW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam,
		SENDASYNCPROC lpResultCallBack, ULONG_PTR dwData);

WINPR_API BOOL WINAPI TranslateMessage(CONST MSG* lpMsg);

WINPR_API LRESULT WINAPI DispatchMessageA(CONST MSG* lpMsg);
WINPR_API LRESULT WINAPI DispatchMessageW(CONST MSG* lpMsg);

WINPR_API BOOL WINAPI PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
WINPR_API BOOL WINAPI PeekMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);

WINPR_API BOOL WINAPI ReplyMessage(LRESULT lResult);

WINPR_API BOOL WINAPI WaitMessage(VOID);

WINPR_API LRESULT WINAPI CallWindowProcA(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WINPR_API LRESULT WINAPI CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

WINPR_API LRESULT WINAPI DefWindowProcA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
WINPR_API LRESULT WINAPI DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#ifdef UNICODE
#define GetWindowLong		GetWindowLongW
#define SetWindowLong		SetWindowLongW
#define GetWindowLongPtr	GetWindowLongPtrW
#define SetWindowLongPtr	SetWindowLongPtrW
#define RegisterClass		RegisterClassW
#define RegisterClassEx		RegisterClassExW
#define UnregisterClass		UnregisterClassW
#define CreateWindow		CreateWindowW
#define CreateWindowEx		CreateWindowExW
#define FindWindow		FindWindowW
#define FindWindowEx		FindWindowExW
#define GetMessage		GetMessageW
#define SendMessage		SendMessageW
#define SendMessageTimeout	SendMessageTimeoutW
#define SendNotifyMessage	SendNotifyMessageW
#define SendMessageCallback	SendMessageCallbackW
#define DispatchMessage		DispatchMessageW
#define PeekMessage		PeekMessageW
#define CallWindowProc		CallWindowProcW
#define DefWindowProc		DefWindowProcW
#else
#define GetWindowLong		GetWindowLongA
#define SetWindowLong		SetWindowLongA
#define GetWindowLongPtr	GetWindowLongPtrA
#define SetWindowLongPtr	SetWindowLongPtrA
#define RegisterClass		RegisterClassA
#define RegisterClassEx		RegisterClassExA
#define UnregisterClass		UnregisterClassA
#define CreateWindow		CreateWindowA
#define CreateWindowEx		CreateWindowExA
#define FindWindow		FindWindowA
#define FindWindowEx		FindWindowExA
#define GetMessage		GetMessageA
#define SendMessage		SendMessageA
#define SendMessageTimeout	SendMessageTimeoutA
#define SendNotifyMessage	SendNotifyMessageA
#define SendMessageCallback	SendMessageCallbackA
#define DispatchMessage		DispatchMessageA
#define PeekMessage		PeekMessageA
#define CallWindowProc		CallWindowProcA
#define DefWindowProc		DefWindowProcA
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_WND_H */

