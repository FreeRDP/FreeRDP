
#include <winpr/crt.h>
#include <winpr/wnd.h>
#include <winpr/library.h>

static LRESULT CALLBACK TestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PCOPYDATASTRUCT pCopyData;

	switch (uMsg)
	{
		case WM_COPYDATA:
			{
				pCopyData = (PCOPYDATASTRUCT) lParam;

				if (!pCopyData)
					break;

				printf("WM_COPYDATA: cbData: %d dwData: %d\n",
						(int) pCopyData->cbData, (int) pCopyData->dwData);
			}
			break;

		case WM_CLOSE:
			printf("WM_CLOSE\n");
			break;

		default:
			printf("TestWndProc: uMsg: 0x%04X\n", uMsg);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
			break;
	}

	return 0;
}

int TestWndWmCopyData(int argc, char* argv[])
{
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

	SendMessage(hWnd, WM_CLOSE, 0, 0);

	DestroyWindow(hWnd);

	return 0;
}

