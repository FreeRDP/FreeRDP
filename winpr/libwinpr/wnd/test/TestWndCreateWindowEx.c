
#include <winpr/crt.h>
#include <winpr/wnd.h>
#include <winpr/wtsapi.h>
#include <winpr/library.h>

const char* WM_WTS_STRINGS[] =
{
	"",
	"WTS_CONSOLE_CONNECT",
	"WTS_CONSOLE_DISCONNECT",
	"WTS_REMOTE_CONNECT",
	"WTS_REMOTE_DISCONNECT",
	"WTS_SESSION_LOGON",
	"WTS_SESSION_LOGOFF",
	"WTS_SESSION_LOCK",
	"WTS_SESSION_UNLOCK",
	"WTS_SESSION_REMOTE_CONTROL",
	"WTS_SESSION_CREATE",
	"WTS_SESSION_TERMINATE",
	""
};

static LRESULT CALLBACK TestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_WTSSESSION_CHANGE:
			if (wParam && (wParam < 13))
			{
				PWTSSESSION_NOTIFICATION pNotification = (PWTSSESSION_NOTIFICATION) lParam;

				printf("WM_WTSSESSION_CHANGE: %s SessionId: %d\n",
						WM_WTS_STRINGS[wParam], (int) pNotification->dwSessionId);
			}
			break;

		default:
			printf("TestWndProc: uMsg: 0x%04X\n", uMsg);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;
	}

	return 0;
}

int TestWndCreateWindowEx(int argc, char* argv[])
{
	HWND hWnd;
	HMODULE hModule;
	HINSTANCE hInstance;
	WNDCLASSEX wndClassEx;
	WTSSESSION_NOTIFICATION wtsSessionNotification;

	hModule = GetModuleHandle(NULL);

	ZeroMemory(&wndClassEx, sizeof(WNDCLASSEX));
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.style = 0;
	wndClassEx.lpfnWndProc = TestWndProc;
	wndClassEx.cbClsExtra = 0;
	wndClassEx.cbWndExtra = 0;
	wndClassEx.hInstance = hModule;
	wndClassEx.hIcon = NULL;
	wndClassEx.hCursor = NULL;
	wndClassEx.hbrBackground = NULL;
	wndClassEx.lpszMenuName = _T("TestWndMenu");
	wndClassEx.lpszClassName = _T("TestWndClass");
	wndClassEx.hIconSm = NULL;

	if (!RegisterClassEx(&wndClassEx))
	{
		printf("RegisterClassEx failure\n");
		return -1;
	}

	hInstance = wndClassEx.hInstance;

	hWnd = CreateWindowEx(0, wndClassEx.lpszClassName,
		0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, NULL);

	if (!hWnd)
	{
		printf("CreateWindowEx failure\n");
		return -1;
	}

	wtsSessionNotification.cbSize = sizeof(WTSSESSION_NOTIFICATION);
	wtsSessionNotification.dwSessionId = 123;

	SendMessage(hWnd, WM_WTSSESSION_CHANGE, WTS_SESSION_LOGON, (LPARAM) &wtsSessionNotification);

	DestroyWindow(hWnd);

	return 0;
}

