
#include <winpr/crt.h>
#include <winpr/wnd.h>
#include <winpr/library.h>

LRESULT CALLBACK TestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	printf("TestWndProc: uMsg: 0x%04X\n", uMsg);

	switch (uMsg)
	{
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	return 0;
}

int TestWndCreateWindowEx(int argc, char* argv[])
{
	MSG msg;
	HWND hWnd;
	HMODULE hModule;
	HINSTANCE hInstance;
	WNDCLASSEX wndClassEx;

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

	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

