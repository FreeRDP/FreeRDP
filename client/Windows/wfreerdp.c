/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Windows Client
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <freerdp/freerdp.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/event.h>
#include <freerdp/chanman/chanman.h>

#include "wf_gdi.h"

#include "wfreerdp.h"

struct _thread_data
{
	freerdp* instance;
};
typedef struct _thread_data thread_data;

HANDLE g_done_event;
HINSTANCE g_hInstance;
HCURSOR g_default_cursor;
volatile int g_thread_count = 0;
LPCTSTR g_wnd_class_name = L"wfreerdp";

int wf_create_console(void)
{
	if (!AllocConsole())
		return 1;

	freopen("CONOUT$", "w", stdout);
	printf("Debug console created.\n");

	return 0;
}

void wf_sw_begin_paint(rdpUpdate* update)
{
	GDI* gdi;
	gdi = GET_GDI(update);
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void wf_sw_end_paint(rdpUpdate* update)
{
	int i;
	GDI* gdi;
	wfInfo* wfi;
	sint32 x, y;
	uint32 w, h;
	int ninvalid;
	RECT update_rect;
	HGDI_RGN cinvalid;

	gdi = GET_GDI(update);
	wfi = GET_WFI(update);

	if (gdi->primary->hdc->hwnd->ninvalid < 1)
		return;

	ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	for (i = 0; i < ninvalid; i++)
	{
		x = cinvalid[i].x;
		y = cinvalid[i].y;
		w = cinvalid[i].w;
		h = cinvalid[i].h;

		update_rect.left = x;
		update_rect.top = y;
		update_rect.right = x + w - 1;
		update_rect.bottom = y + h - 1;

		//printf("InvalidateRect: x:%d y:%d w:%d h:%d\n", x, y, w, h);

		InvalidateRect(wfi->hwnd, &update_rect, FALSE);
	}
}

void wf_hw_begin_paint(rdpUpdate* update)
{
	wfInfo* wfi = GET_WFI(update);
	wfi->hdc->hwnd->invalid->null = 1;
	wfi->hdc->hwnd->ninvalid = 0;
}

void wf_hw_end_paint(rdpUpdate* update)
{

}

boolean wf_pre_connect(freerdp* instance)
{
	int i1;
	wfInfo* wfi;
	rdpSettings* settings;

	wfi = (wfInfo*) xzalloc(sizeof(wfInfo));
	SET_WFI(instance, wfi);
	wfi->instance = instance;

	settings = instance->settings;

	settings->order_support[NEG_DSTBLT_INDEX] = True;
	settings->order_support[NEG_PATBLT_INDEX] = True;
	settings->order_support[NEG_SCRBLT_INDEX] = True;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = True;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = False;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = False;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = False;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_LINETO_INDEX] = True;
	settings->order_support[NEG_POLYLINE_INDEX] = True;
	settings->order_support[NEG_MEMBLT_INDEX] = False;
	settings->order_support[NEG_MEM3BLT_INDEX] = False;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
	settings->order_support[NEG_POLYGON_SC_INDEX] = False;
	settings->order_support[NEG_POLYGON_CB_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = False;

	wfi->cursor = g_default_cursor;

	wfi->fullscreen = settings->fullscreen;
	wfi->fs_toggle = wfi->fullscreen;
	wfi->sw_gdi = settings->sw_gdi;

	wfi->clrconv = (HCLRCONV) xzalloc(sizeof(CLRCONV));
	wfi->clrconv->alpha = 1;
	wfi->clrconv->palette = NULL;

	if (wfi->sw_gdi)
	{
		wfi->cache = cache_new(instance->settings);
	}

	if (wfi->percentscreen > 0)
	{
		i1 = (GetSystemMetrics(SM_CXSCREEN) * wfi->percentscreen) / 100;
		settings->width = i1;

		i1 = (GetSystemMetrics(SM_CYSCREEN) * wfi->percentscreen) / 100;
		settings->height = i1;
	}

	if (wfi->fs_toggle)
	{
		settings->width = GetSystemMetrics(SM_CXSCREEN);
		settings->height = GetSystemMetrics(SM_CYSCREEN);
	}

	i1 = settings->width;
	i1 = (i1 + 3) & (~3);
	settings->width = i1;

	if ((settings->width < 64) || (settings->height < 64) ||
		(settings->width > 4096) || (settings->height > 4096))
	{
		printf("wf_pre_connect: invalid dimensions %d %d\n", settings->width, settings->height);
		return 1;
	}

	settings->kbd_layout = (int) GetKeyboardLayout(0) & 0x0000FFFF;
	freerdp_chanman_pre_connect(GET_CHANMAN(instance), instance);

	return True;
}

boolean wf_post_connect(freerdp* instance)
{
	GDI* gdi;
	wfInfo* wfi;
	int width, height;
	wchar_t win_title[64];
	rdpSettings* settings;

	wfi = GET_WFI(instance);
	SET_WFI(instance->update, wfi);
	settings = instance->settings;

	wfi->dstBpp = 32;
	width = settings->width;
	height = settings->height;

	if (wfi->sw_gdi)
	{
		gdi_init(instance, CLRCONV_ALPHA | CLRBUF_32BPP, NULL);
		gdi = GET_GDI(instance->update);
		wfi->hdc = gdi->primary->hdc;
		wfi->primary = wf_image_new(wfi, width, height, wfi->dstBpp, gdi->primary_buffer);
	}
	else
	{
		wf_gdi_register_update_callbacks(instance->update);
		wfi->srcBpp = instance->settings->color_depth;
		wfi->primary = wf_image_new(wfi, width, height, wfi->dstBpp, NULL);

		wfi->hdc = gdi_GetDC();
		wfi->hdc->bitsPerPixel = wfi->dstBpp;
		wfi->hdc->bytesPerPixel = wfi->dstBpp / 8;

		wfi->hdc->alpha = wfi->clrconv->alpha;
		wfi->hdc->invert = wfi->clrconv->invert;

		wfi->hdc->hwnd = (HGDI_WND) malloc(sizeof(GDI_WND));
		wfi->hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
		wfi->hdc->hwnd->invalid->null = 1;

		wfi->hdc->hwnd->count = 32;
		wfi->hdc->hwnd->cinvalid = (HGDI_RGN) malloc(sizeof(GDI_RGN) * wfi->hdc->hwnd->count);
		wfi->hdc->hwnd->ninvalid = 0;
	}

	if (strlen(wfi->window_title) > 0)
		_snwprintf(win_title, sizeof(win_title), L"%S", wfi->window_title);
	else if (settings->port == 3389)
		_snwprintf(win_title, sizeof(win_title) / sizeof(win_title[0]), L"%S - FreeRDP", settings->hostname);
	else
		_snwprintf(win_title, sizeof(win_title) / sizeof(win_title[0]), L"%S:%d - FreeRDP", settings->hostname, settings->port);

	if (wfi->hwnd == 0)
	{
		wfi->hwnd = CreateWindowEx((DWORD) NULL, g_wnd_class_name, win_title,
				0, 0, 0, 0, 0, NULL, NULL, g_hInstance, NULL);

		SetWindowLongPtr(wfi->hwnd, GWLP_USERDATA, (LONG_PTR) wfi);
	}

	if (wfi->fullscreen)
	{
		SetWindowLongPtr(wfi->hwnd, GWL_STYLE, WS_POPUP);
		SetWindowPos(wfi->hwnd, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED);
	}
	else
	{
		POINT diff;
		RECT rc_client, rc_wnd;

		SetWindowLongPtr(wfi->hwnd, GWL_STYLE, WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX);
		/* Now resize to get full canvas size and room for caption and borders */
		SetWindowPos(wfi->hwnd, HWND_TOP, 10, 10, width, height, SWP_FRAMECHANGED);
		GetClientRect(wfi->hwnd, &rc_client);
		GetWindowRect(wfi->hwnd, &rc_wnd);
		diff.x = (rc_wnd.right - rc_wnd.left) - rc_client.right;
		diff.y = (rc_wnd.bottom - rc_wnd.top) - rc_client.bottom;
		SetWindowPos(wfi->hwnd, HWND_TOP, -1, -1, width + diff.x, height + diff.y, SWP_NOMOVE | SWP_FRAMECHANGED);
	}

	BitBlt(wfi->primary->hdc, 0, 0, width, height, NULL, 0, 0, BLACKNESS);
	wfi->drawing = wfi->primary;

	ShowWindow(wfi->hwnd, SW_SHOWNORMAL);
	UpdateWindow(wfi->hwnd);

	if (wfi->sw_gdi)
	{
		instance->update->BeginPaint = wf_sw_begin_paint;
		instance->update->EndPaint = wf_sw_end_paint;
	}
	else
	{
		instance->update->BeginPaint = wf_hw_begin_paint;
		instance->update->EndPaint = wf_hw_end_paint;
	}

	freerdp_chanman_post_connect(GET_CHANMAN(instance), instance);

	return True;
}

int wf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_chanman_data(instance, channelId, data, size, flags, total_size);
}

void wf_process_channel_event(rdpChanMan* chanman, freerdp* instance)
{
	wfInfo* wfi;
	RDP_EVENT* event;

	wfi = GET_WFI(instance);

	event = freerdp_chanman_pop_event(chanman);

	if (event)
		freerdp_event_free(event);
}

boolean wf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	wfInfo* wfi = GET_WFI(instance);
	return True;
}

boolean wf_check_fds(freerdp* instance)
{
	wfInfo* wfi = GET_WFI(instance);
	return True;
}

int wf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChanMan* chanman = (rdpChanMan*) user_data;

	printf("loading plugin %s\n", name);
	freerdp_chanman_load_plugin(chanman, settings, name, plugin_data);

	return 1;
}

int wf_process_ui_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	return 0;
}

int wfreerdp_run(freerdp* instance)
{
	MSG msg;
	int index;
	int gmcode;
	int alldone;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	int fds_count;
	HANDLE fds[64];
	rdpChanMan* chanman;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (instance->Connect(instance) != True)
		return 0;

	chanman = GET_CHANMAN(instance);

	/* program main loop */
	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (wf_get_fds(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_chanman_get_fds(chanman, instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get channel manager file descriptor\n");
			break;
		}

		fds_count = 0;
		/* setup read fds */
		for (index = 0; index < rcount; index++)
		{
			fds[fds_count++] = rfds[index];
		}
		/* setup write fds */
		for (index = 0; index < wcount; index++)
		{
			fds[fds_count++] = wfds[index];
		}
		/* exit if nothing to do */
		if (fds_count == 0)
		{
			printf("wfreerdp_run: fds_count is zero\n");
			break;
		}
		/* do the wait */
		if (MsgWaitForMultipleObjects(fds_count, fds, FALSE, INFINITE, QS_ALLINPUT) == WAIT_FAILED)
		{
			printf("wfreerdp_run: WaitForMultipleObjects failed: 0x%04X\n", GetLastError());
			break;
		}

		if (instance->CheckFileDescriptor(instance) != True)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (wf_check_fds(instance) != True)
		{
			printf("Failed to check wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_chanman_check_fds(chanman, instance) != True)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		wf_process_channel_event(chanman, instance);

		alldone = FALSE;
		while (PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
		{
			gmcode = GetMessage(&msg, 0, 0, 0);
			if (gmcode == 0 || gmcode == -1)
			{
				alldone = TRUE;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (alldone)
		{
			break;
		}
	}

	/* cleanup */
	freerdp_chanman_free(chanman);
	freerdp_free(instance);
	
	return 0;
}

static DWORD WINAPI thread_func(LPVOID lpParam)
{
	wfInfo* wfi;
	freerdp* instance;
	thread_data* data;

	data = (thread_data*) lpParam;
	instance = data->instance;

	wfi = (wfInfo*) xzalloc(sizeof(wfInfo));
	SET_WFI(instance, wfi);
	wfi->instance = instance;

	wfreerdp_run(instance);

	g_thread_count--;

	if (g_thread_count < 1)
		SetEvent(g_done_event);

	return (DWORD) NULL;
}

static DWORD WINAPI kbd_thread_func(LPVOID lpParam)
{
	MSG msg;
	BOOL status;
	HHOOK hook_handle;

	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, g_hInstance, 0);

	if (hook_handle)
	{
		while( (status = GetMessage( &msg, NULL, 0, 0 )) != 0)
		{
			if (status == -1)
			{
				printf("keyboard thread error getting message\n");
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		UnhookWindowsHookEx(hook_handle);
	}
	else
		printf("failed to install keyboard hook\n");

	return (DWORD) NULL;
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	freerdp* instance;
	thread_data* data;
	WSADATA wsa_data;
	WNDCLASSEX wnd_cls;
	rdpChanMan* chanman;

	if (WSAStartup(0x101, &wsa_data) != 0)
		return 1;

	g_done_event = CreateEvent(0, 1, 0, 0);

#if defined(WITH_DEBUG) || defined(_DEBUG)
	wf_create_console();
#endif

	g_default_cursor = LoadCursor(NULL, IDC_ARROW);

	wnd_cls.cbSize        = sizeof(WNDCLASSEX);
	wnd_cls.style         = CS_HREDRAW | CS_VREDRAW;
	wnd_cls.lpfnWndProc   = wf_event_proc;
	wnd_cls.cbClsExtra    = 0;
	wnd_cls.cbWndExtra    = 0;
	wnd_cls.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wnd_cls.hCursor       = g_default_cursor;
	wnd_cls.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wnd_cls.lpszMenuName  = NULL;
	wnd_cls.lpszClassName = g_wnd_class_name;
	wnd_cls.hInstance     = hInstance;
	wnd_cls.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	RegisterClassEx(&wnd_cls);

	g_hInstance = hInstance;
	freerdp_chanman_global_init();

	instance = freerdp_new();
	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->ReceiveChannelData = wf_receive_channel_data;

	chanman = freerdp_chanman_new();
	SET_CHANMAN(instance, chanman);

	if (!CreateThread(NULL, 0, kbd_thread_func, NULL, 0, NULL))
		printf("error creating keyboard handler thread");

	//while (1)
	{
		data = (thread_data*) xzalloc(sizeof(thread_data)); 
		data->instance = instance;

		freerdp_parse_args(instance->settings, __argc, __argv,
			wf_process_plugin_args, chanman, wf_process_ui_args, NULL);

		if (CreateThread(NULL, 0, thread_func, data, 0, NULL) != 0)
			g_thread_count++;
	}

	if (g_thread_count > 0)
		WaitForSingleObject(g_done_event, INFINITE);
	else
		MessageBox(GetConsoleWindow(),
			L"Failed to start wfreerdp.\n\nPlease check the debug output.",
			L"FreeRDP Error", MB_ICONSTOP);

	WSACleanup();

#ifdef _DEBUG
	system("pause");
#endif

	return 0;
}
