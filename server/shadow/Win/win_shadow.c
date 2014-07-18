/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "../shadow_screen.h"
#include "../shadow_surface.h"

#include "win_shadow.h"

#ifdef WITH_DXGI_1_2

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include <D3D11.h>
#include <dxgi1_2.h>

static D3D_DRIVER_TYPE DriverTypes[] =
{
	D3D_DRIVER_TYPE_HARDWARE,
	D3D_DRIVER_TYPE_WARP,
	D3D_DRIVER_TYPE_REFERENCE,
};

static UINT NumDriverTypes = ARRAYSIZE(DriverTypes);

static D3D_FEATURE_LEVEL FeatureLevels[] =
{
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_1
};

static UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);

static D3D_FEATURE_LEVEL FeatureLevel;

static ID3D11Device* gDevice = NULL;
static ID3D11DeviceContext* gContext = NULL;
static IDXGIOutputDuplication* gOutputDuplication = NULL;
static ID3D11Texture2D* gAcquiredDesktopImage = NULL;

static IDXGISurface* surf;
static ID3D11Texture2D* sStage;

static DXGI_OUTDUPL_FRAME_INFO FrameInfo;

static HMODULE d3d11_module = NULL;

typedef HRESULT (WINAPI * fnD3D11CreateDevice)(
	IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
	CONST D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
	ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);

static fnD3D11CreateDevice pfnD3D11CreateDevice = NULL;

#undef DEFINE_GUID
#define INITGUID

#include <initguid.h>

/* dxgi.h GUIDs */

DEFINE_GUID(IID_IDXGIObject, 0xaec22fb8, 0x76f3, 0x4639, 0x9b, 0xe0, 0x28, 0xeb, 0x43, 0xa6, 0x7a, 0x2e);
DEFINE_GUID(IID_IDXGIDeviceSubObject, 0x3d3e0379, 0xf9de, 0x4d58, 0xbb, 0x6c, 0x18, 0xd6, 0x29, 0x92, 0xf1, 0xa6);
DEFINE_GUID(IID_IDXGIResource, 0x035f3ab4, 0x482e, 0x4e50, 0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b);
DEFINE_GUID(IID_IDXGIKeyedMutex, 0x9d8e1289, 0xd7b3, 0x465f, 0x81, 0x26, 0x25, 0x0e, 0x34, 0x9a, 0xf8, 0x5d);
DEFINE_GUID(IID_IDXGISurface, 0xcafcb56c, 0x6ac3, 0x4889, 0xbf, 0x47, 0x9e, 0x23, 0xbb, 0xd2, 0x60, 0xec);
DEFINE_GUID(IID_IDXGISurface1, 0x4AE63092, 0x6327, 0x4c1b, 0x80, 0xAE, 0xBF, 0xE1, 0x2E, 0xA3, 0x2B, 0x86);
DEFINE_GUID(IID_IDXGIAdapter, 0x2411e7e1, 0x12ac, 0x4ccf, 0xbd, 0x14, 0x97, 0x98, 0xe8, 0x53, 0x4d, 0xc0);
DEFINE_GUID(IID_IDXGIOutput, 0xae02eedb, 0xc735, 0x4690, 0x8d, 0x52, 0x5a, 0x8d, 0xc2, 0x02, 0x13, 0xaa);
DEFINE_GUID(IID_IDXGISwapChain, 0x310d36a0, 0xd2e7, 0x4c0a, 0xaa, 0x04, 0x6a, 0x9d, 0x23, 0xb8, 0x88, 0x6a);
DEFINE_GUID(IID_IDXGIFactory, 0x7b7166ec, 0x21c7, 0x44ae, 0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69);
DEFINE_GUID(IID_IDXGIDevice, 0x54ec77fa, 0x1377, 0x44e6, 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c);
DEFINE_GUID(IID_IDXGIFactory1, 0x770aae78, 0xf26f, 0x4dba, 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87);
DEFINE_GUID(IID_IDXGIAdapter1, 0x29038f61, 0x3839, 0x4626, 0x91, 0xfd, 0x08, 0x68, 0x79, 0x01, 0x1a, 0x05);
DEFINE_GUID(IID_IDXGIDevice1, 0x77db970f, 0x6276, 0x48ba, 0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c);

/* dxgi1_2.h GUIDs */

DEFINE_GUID(IID_IDXGIDisplayControl, 0xea9dbf1a, 0xc88e, 0x4486, 0x85, 0x4a, 0x98, 0xaa, 0x01, 0x38, 0xf3, 0x0c);
DEFINE_GUID(IID_IDXGIOutputDuplication, 0x191cfac3, 0xa341, 0x470d, 0xb2, 0x6e, 0xa8, 0x64, 0xf4, 0x28, 0x31, 0x9c);
DEFINE_GUID(IID_IDXGISurface2, 0xaba496dd, 0xb617, 0x4cb8, 0xa8, 0x66, 0xbc, 0x44, 0xd7, 0xeb, 0x1f, 0xa2);
DEFINE_GUID(IID_IDXGIResource1, 0x30961379, 0x4609, 0x4a41, 0x99, 0x8e, 0x54, 0xfe, 0x56, 0x7e, 0xe0, 0xc1);
DEFINE_GUID(IID_IDXGIDevice2, 0x05008617, 0xfbfd, 0x4051, 0xa7, 0x90, 0x14, 0x48, 0x84, 0xb4, 0xf6, 0xa9);
DEFINE_GUID(IID_IDXGISwapChain1, 0x790a45f7, 0x0d42, 0x4876, 0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa);
DEFINE_GUID(IID_IDXGIFactory2, 0x50c83a1c, 0xe072, 0x4c48, 0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0);
DEFINE_GUID(IID_IDXGIAdapter2, 0x0AA1AE0A, 0xFA0E, 0x4B84, 0x86, 0x44, 0xE0, 0x5F, 0xF8, 0xE5, 0xAC, 0xB5);
DEFINE_GUID(IID_IDXGIOutput1, 0x00cddea8, 0x939b, 0x4b83, 0xa3, 0x40, 0xa6, 0x85, 0x22, 0x66, 0x66, 0xcc);

static void win_shadow_d3d11_module_init()
{
	if (d3d11_module)
		return;

	d3d11_module = LoadLibraryA("d3d11.dll");

	if (!d3d11_module)
		return;

	pfnD3D11CreateDevice = (fnD3D11CreateDevice) GetProcAddress(d3d11_module, "D3D11CreateDevice");
}

int win_shadow_dxgi_init(winShadowSubsystem* subsystem)
{
	HRESULT status;
	UINT dTop, i = 0;
	UINT DriverTypeIndex;
	DXGI_OUTPUT_DESC desc;
	IDXGIOutput* pOutput;
	IDXGIDevice* DxgiDevice = NULL;
	IDXGIAdapter* DxgiAdapter = NULL;
	IDXGIOutput* DxgiOutput = NULL;
	IDXGIOutput1* DxgiOutput1 = NULL;

	win_shadow_d3d11_module_init();

	if (!pfnD3D11CreateDevice)
		return -1;

	for (DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex)
	{
		status = pfnD3D11CreateDevice(NULL, DriverTypes[DriverTypeIndex], NULL, 0, FeatureLevels,
			NumFeatureLevels, D3D11_SDK_VERSION, &gDevice, &FeatureLevel, &gContext);

		if (SUCCEEDED(status))
			break;
	}

	if (FAILED(status))
		return -1;

	status = gDevice->lpVtbl->QueryInterface(gDevice, &IID_IDXGIDevice, (void**) &DxgiDevice);

	if (FAILED(status))
		return -1;

	status = DxgiDevice->lpVtbl->GetParent(DxgiDevice, &IID_IDXGIAdapter, (void**) &DxgiAdapter);
	DxgiDevice->lpVtbl->Release(DxgiDevice);
	DxgiDevice = NULL;

	if (FAILED(status))
		return -1;

	ZeroMemory(&desc, sizeof(desc));
	pOutput = NULL;

	while (DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, i, &pOutput) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC* pDesc = &desc;

		status = pOutput->lpVtbl->GetDesc(pOutput, pDesc);

		if (FAILED(status))
			return -1;

		if (pDesc->AttachedToDesktop)
			dTop = i;

		pOutput->lpVtbl->Release(pOutput);
		++i;
	}

	dTop = 0; /* screen id */

	status = DxgiAdapter->lpVtbl->EnumOutputs(DxgiAdapter, dTop, &DxgiOutput);
	DxgiAdapter->lpVtbl->Release(DxgiAdapter);
	DxgiAdapter = NULL;

	if (FAILED(status))
		return -1;

	status = DxgiOutput->lpVtbl->QueryInterface(DxgiOutput, &IID_IDXGIOutput1, (void**) &DxgiOutput1);
	DxgiOutput->lpVtbl->Release(DxgiOutput);
	DxgiOutput = NULL;

	if (FAILED(status))
		return -1;

	status = DxgiOutput1->lpVtbl->DuplicateOutput(DxgiOutput1, (IUnknown*) gDevice, &gOutputDuplication);
	DxgiOutput1->lpVtbl->Release(DxgiOutput1);
	DxgiOutput1 = NULL;

	if (FAILED(status))
		return -1;

	return 1;
}

#endif

void win_shadow_input_synchronize_event(winShadowSubsystem* subsystem, UINT32 flags)
{

}

void win_shadow_input_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_SCANCODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	if (flags & KBD_FLAGS_EXTENDED)
		event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_unicode_keyboard_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
	INPUT event;

	event.type = INPUT_KEYBOARD;
	event.ki.wVk = 0;
	event.ki.wScan = code;
	event.ki.dwFlags = KEYEVENTF_UNICODE;
	event.ki.dwExtraInfo = 0;
	event.ki.time = 0;

	if (flags & KBD_FLAGS_RELEASE)
		event.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &event, sizeof(INPUT));
}

void win_shadow_input_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	event.type = INPUT_MOUSE;

	if (flags & PTR_FLAGS_WHEEL)
	{
		event.mi.dwFlags = MOUSEEVENTF_WHEEL;
		event.mi.mouseData = flags & WheelRotationMask;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			event.mi.mouseData *= -1;

		SendInput(1, &event, sizeof(INPUT));
	}
	else
	{
		width = (float) GetSystemMetrics(SM_CXSCREEN);
		height = (float) GetSystemMetrics(SM_CYSCREEN);

		event.mi.dx = (LONG) ((float) x * (65535.0f / width));
		event.mi.dy = (LONG) ((float) y * (65535.0f / height));
		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_MOVE)
		{
			event.mi.dwFlags |= MOUSEEVENTF_MOVE;
			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE;

		if (flags & PTR_FLAGS_BUTTON1)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON2)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

			SendInput(1, &event, sizeof(INPUT));
		}
		else if (flags & PTR_FLAGS_BUTTON3)
		{
			if (flags & PTR_FLAGS_DOWN)
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
			else
				event.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

			SendInput(1, &event, sizeof(INPUT));
		}
	}
}

void win_shadow_input_extended_mouse_event(winShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
	INPUT event;
	float width;
	float height;

	ZeroMemory(&event, sizeof(INPUT));

	if ((flags & PTR_XFLAGS_BUTTON1) || (flags & PTR_XFLAGS_BUTTON2))
	{
		event.type = INPUT_MOUSE;

		if (flags & PTR_FLAGS_MOVE)
		{
			width = (float) GetSystemMetrics(SM_CXSCREEN);
			height = (float) GetSystemMetrics(SM_CYSCREEN);

			event.mi.dx = (LONG)((float) x * (65535.0f / width));
			event.mi.dy = (LONG)((float) y * (65535.0f / height));
			event.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;

			SendInput(1, &event, sizeof(INPUT));
		}

		event.mi.dx = event.mi.dy = event.mi.dwFlags = 0;

		if (flags & PTR_XFLAGS_DOWN)
			event.mi.dwFlags |= MOUSEEVENTF_XDOWN;
		else
			event.mi.dwFlags |= MOUSEEVENTF_XUP;

		if (flags & PTR_XFLAGS_BUTTON1)
			event.mi.mouseData = XBUTTON1;
		else if (flags & PTR_XFLAGS_BUTTON2)
			event.mi.mouseData = XBUTTON2;

		SendInput(1, &event, sizeof(INPUT));
	}
}


int win_shadow_invalidate_region(winShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	rdpShadowServer* server;
	rdpShadowScreen* screen;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	screen = server->screen;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	EnterCriticalSection(&(screen->lock));
	region16_union_rect(&(screen->invalidRegion), &(screen->invalidRegion), &invalidRect);
	LeaveCriticalSection(&(screen->lock));

	return 1;
}

int win_shadow_surface_copy(winShadowSubsystem* subsystem)
{
	return 1;
}

void* win_shadow_subsystem_thread(winShadowSubsystem* subsystem)
{
	int fps;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;

	fps = 16;
	dwInterval = 1000 / fps;
	frameTime = GetTickCount64() + dwInterval;

	while (1)
	{
		dwTimeout = INFINITE;

		cTime = GetTickCount64();
		dwTimeout = (DWORD) ((cTime > frameTime) ? 0 : frameTime - cTime);

		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			win_shadow_invalidate_region(subsystem, 0, 0, subsystem->width, subsystem->height);

			dwInterval = 1000 / fps;
			frameTime += dwInterval;
		}
	}

	ExitThread(0);
	return NULL;
}

int win_shadow_subsystem_init(winShadowSubsystem* subsystem)
{
	HDC hdc;
	int status;
	DWORD iDevNum = 0;
	MONITOR_DEF* virtualScreen;
	DISPLAY_DEVICE DisplayDevice;

	ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
	DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

	if (!EnumDisplayDevices(NULL, iDevNum, &DisplayDevice, 0))
		return -1;

	hdc = CreateDC(DisplayDevice.DeviceName, NULL, NULL, NULL);

	subsystem->width = GetDeviceCaps(hdc, HORZRES);
	subsystem->height = GetDeviceCaps(hdc, VERTRES);
	subsystem->bpp = GetDeviceCaps(hdc, BITSPIXEL);

	DeleteDC(hdc);

#ifdef WITH_DXGI_1_2
	status = win_shadow_dxgi_init(subsystem);
#endif

	virtualScreen = &(subsystem->virtualScreen);

	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;

	if (subsystem->monitorCount < 1)
	{
		subsystem->monitorCount = 1;
		subsystem->monitors[0].left = virtualScreen->left;
		subsystem->monitors[0].top = virtualScreen->top;
		subsystem->monitors[0].right = virtualScreen->right;
		subsystem->monitors[0].bottom = virtualScreen->bottom;
		subsystem->monitors[0].flags = 1;
	}

	printf("width: %d height: %d\n", subsystem->width, subsystem->height);

	return 1;
}

int win_shadow_subsystem_uninit(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

int win_shadow_subsystem_start(winShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) win_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL);

	return 1;
}

int win_shadow_subsystem_stop(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

void win_shadow_subsystem_free(winShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	win_shadow_subsystem_uninit(subsystem);

	free(subsystem);
}

winShadowSubsystem* win_shadow_subsystem_new(rdpShadowServer* server)
{
	winShadowSubsystem* subsystem;

	subsystem = (winShadowSubsystem*) calloc(1, sizeof(winShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	subsystem->Init = (pfnShadowSubsystemInit) win_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) win_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) win_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) win_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) win_shadow_subsystem_free;

	subsystem->SurfaceCopy = (pfnShadowSurfaceCopy) win_shadow_surface_copy;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) win_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) win_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) win_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) win_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) win_shadow_input_extended_mouse_event;

	return subsystem;
}

rdpShadowSubsystem* Win_ShadowCreateSubsystem(rdpShadowServer* server)
{
	return (rdpShadowSubsystem*) win_shadow_subsystem_new(server);
}
