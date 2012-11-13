/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/windows.h>

#include <winpr/crt.h>
#include <winpr/credui.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <sys/types.h>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "wf_gdi.h"
#include "wf_graphics.h"
#include "wf_cliprdr.h"

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

void wf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
}

void wf_context_free(freerdp* instance, rdpContext* context)
{

}

int wf_create_console(void)
{
	if (!AllocConsole())
		return 1;

	freopen("CONOUT$", "w", stdout);
	printf("Debug console created.\n");

	return 0;
}

void wf_sw_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void wf_sw_end_paint(rdpContext* context)
{
	int i;
	rdpGdi* gdi;
	wfInfo* wfi;
	INT32 x, y;
	UINT32 w, h;
	int ninvalid;
	RECT update_rect;
	HGDI_RGN cinvalid;

	gdi = context->gdi;
	wfi = ((wfContext*) context)->wfi;

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

		InvalidateRect(wfi->hwnd, &update_rect, FALSE);
	}
}

void wf_hw_begin_paint(rdpContext* context)
{
	wfInfo* wfi = ((wfContext*) context)->wfi;
	wfi->hdc->hwnd->invalid->null = 1;
	wfi->hdc->hwnd->ninvalid = 0;
}

void wf_hw_end_paint(rdpContext* context)
{

}

BOOL wf_pre_connect(freerdp* instance)
{
	int i1;
	wfInfo* wfi;
	rdpFile* file;
	wfContext* context;
	rdpSettings* settings;

	wfi = (wfInfo*) xzalloc(sizeof(wfInfo));
	context = (wfContext*) instance->context;
	wfi->instance = instance;
	context->wfi = wfi;

	settings = instance->settings;

	settings = instance->settings;

	if (settings->ConnectionFile)
	{
		file = freerdp_client_rdp_file_new();

		printf("Using connection file: %s\n", settings->ConnectionFile);

		freerdp_client_parse_rdp_file(file, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(file, settings);
	}

	settings->OsMajorType = OSMAJORTYPE_WINDOWS;
	settings->OsMinorType = OSMINORTYPE_WINDOWS_NT;
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = FALSE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = FALSE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	settings->GlyphSupportLevel = GLYPH_SUPPORT_NONE;

	wfi->cursor = g_default_cursor;

	wfi->fullscreen = settings->Fullscreen;
	wfi->fs_toggle = wfi->fullscreen;
	wfi->sw_gdi = settings->SoftwareGdi;

	wfi->clrconv = (HCLRCONV) xzalloc(sizeof(CLRCONV));
	wfi->clrconv->palette = NULL;
	wfi->clrconv->alpha = FALSE;

	instance->context->cache = cache_new(settings);

	if (wfi->percentscreen > 0)
	{
		i1 = (GetSystemMetrics(SM_CXSCREEN) * wfi->percentscreen) / 100;
		settings->DesktopWidth = i1;

		i1 = (GetSystemMetrics(SM_CYSCREEN) * wfi->percentscreen) / 100;
		settings->DesktopHeight = i1;
	}

	if (wfi->fs_toggle)
	{
		settings->DesktopWidth = GetSystemMetrics(SM_CXSCREEN);
		settings->DesktopHeight = GetSystemMetrics(SM_CYSCREEN);
	}

	i1 = settings->DesktopWidth;
	i1 = (i1 + 3) & (~3);
	settings->DesktopWidth = i1;

	if ((settings->DesktopWidth < 64) || (settings->DesktopHeight < 64) ||
		(settings->DesktopWidth > 4096) || (settings->DesktopHeight > 4096))
	{
		printf("wf_pre_connect: invalid dimensions %d %d\n", settings->DesktopWidth, settings->DesktopHeight);
		return 1;
	}

	settings->KeyboardLayout = (int) GetKeyboardLayout(0) & 0x0000FFFF;
	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

void cpuid(unsigned info, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
#if defined(__GNUC__)
#if defined(__i386__) || defined(__x86_64__)
	*eax = info;
	__asm volatile
		("mov %%ebx, %%edi;" /* 32bit PIC: don't clobber ebx */
		 "cpuid;"
		 "mov %%ebx, %%esi;"
		 "mov %%edi, %%ebx;"
		 :"+a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
		 : :"edi");
#endif
#elif defined(_MSC_VER)
	int a[4];
	__cpuid(a, info);
	*eax = a[0];
	*ebx = a[1];
	*ecx = a[2];
	*edx = a[3];
#endif
}
 
UINT32 wfi_detect_cpu()
{
	UINT32 cpu_opt = 0;
	unsigned int eax, ebx, ecx, edx = 0;

	cpuid(1, &eax, &ebx, &ecx, &edx);

	if (edx & (1<<26))
	{
		cpu_opt |= CPU_SSE2;
	}

	return cpu_opt;
}

BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	wfInfo* wfi;
	rdpCache* cache;
	wfContext* context;
	int width, height;
	wchar_t win_title[64];
	rdpSettings* settings;

	settings = instance->settings;
	context = (wfContext*) instance->context;
	cache = instance->context->cache;
	wfi = context->wfi;

	wfi->dstBpp = 32;
	width = settings->DesktopWidth;
	height = settings->DesktopHeight;

	if (wfi->sw_gdi)
	{
		gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_32BPP, NULL);
		gdi = instance->context->gdi;
		wfi->hdc = gdi->primary->hdc;
		wfi->primary = wf_image_new(wfi, width, height, wfi->dstBpp, gdi->primary_buffer);

		rfx_context_set_cpu_opt((RFX_CONTEXT*) gdi->rfx_context, wfi_detect_cpu());
	}
	else
	{
		wf_gdi_register_update_callbacks(instance->update);
		wfi->srcBpp = instance->settings->ColorDepth;
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

		wfi->image = wf_image_new(wfi, 64, 64, 32, NULL);
		wfi->image->_bitmap.data = NULL;

		if (settings->RemoteFxCodec)
		{
			wfi->tile = wf_image_new(wfi, 64, 64, 32, NULL);
			wfi->rfx_context = rfx_context_new();
			rfx_context_set_cpu_opt(wfi->rfx_context, wfi_detect_cpu());
		}

		if (settings->NSCodec)
			wfi->nsc_context = nsc_context_new();
	}

	if (settings->WindowTitle != NULL)
		_snwprintf(win_title, ARRAY_SIZE(win_title), L"%S", settings->WindowTitle);
	else if (settings->ServerPort == 3389)
		_snwprintf(win_title, ARRAY_SIZE(win_title), L"FreeRDP: %S", settings->ServerHostname);
	else
		_snwprintf(win_title, ARRAY_SIZE(win_title), L"FreeRDP: %S:%d", settings->ServerHostname, settings->ServerPort);

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

	pointer_cache_register_callbacks(instance->update);

	if (wfi->sw_gdi != TRUE)
	{
		brush_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
	}

	wf_register_graphics(instance->context->graphics);

	freerdp_channels_post_connect(instance->context->channels, instance);

	wf_cliprdr_init(wfi, instance->context->channels);

	return TRUE;
}

static const char wfTargetName[] = "TARGET";

static CREDUI_INFOA wfUiInfo =
{
	sizeof(CREDUI_INFOA),
	NULL,
	"Enter your credentials",
	"Remote Desktop Security",
	NULL
};

BOOL wf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	BOOL fSave;
	DWORD status;
	DWORD dwFlags;
	char UserName[CREDUI_MAX_USERNAME_LENGTH + 1];
	char Password[CREDUI_MAX_PASSWORD_LENGTH + 1];
	char User[CREDUI_MAX_USERNAME_LENGTH + 1];
	char Domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1];

	fSave = FALSE;
	ZeroMemory(UserName, sizeof(UserName));
	ZeroMemory(Password, sizeof(Password));
	dwFlags = CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_EXCLUDE_CERTIFICATES;

	status = CredUIPromptForCredentialsA(&wfUiInfo, wfTargetName, NULL, 0,
		UserName, CREDUI_MAX_USERNAME_LENGTH + 1,
		Password, CREDUI_MAX_PASSWORD_LENGTH + 1, &fSave, dwFlags);

	if (status != NO_ERROR)
	{
		printf("CredUIPromptForCredentials unexpected status: 0x%08X\n", status);
		return FALSE;
	}

	ZeroMemory(User, sizeof(User));
	ZeroMemory(Domain, sizeof(Domain));

	status = CredUIParseUserNameA(UserName, User, sizeof(User), Domain, sizeof(Domain));

	//printf("User: %s Domain: %s Password: %s\n", User, Domain, Password);

	*username = _strdup(User);

	if (strlen(Domain) > 0)
		*domain = _strdup(Domain);

	*password = _strdup(Password);

	return TRUE;
}

BOOL wf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
#if 0
	DWORD mode;
	int read_size;
	DWORD read_count;
	TCHAR answer[2];
	TCHAR* read_buffer;
	HANDLE input_handle;
#endif

	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired. "
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	/* TODO: ask for user validation */
#if 0
	input_handle = GetStdHandle(STD_INPUT_HANDLE);

	GetConsoleMode(input_handle, &mode);
	mode |= ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT;
	SetConsoleMode(input_handle, mode);
#endif

	return TRUE;
}

int wf_receive_channel_data(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

void wf_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	RDP_EVENT* event;

	event = freerdp_channels_pop_event(channels);

	if (event)
		freerdp_event_free(event);
}

BOOL wf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	return TRUE;
}

BOOL wf_check_fds(freerdp* instance)
{
	return TRUE;
}

int wf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	void* entry = NULL;
	rdpChannels* channels = (rdpChannels*) user_data;

	entry = freerdp_channels_client_find_static_entry("VirtualChannelEntry", name);

	if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, plugin_data) == 0)
		{
			printf("loading channel %s (static)\n", name);
			return 1;
		}
	}

	printf("loading channel %s (plugin)\n", name);
	freerdp_channels_load_plugin(channels, settings, name, plugin_data);

	return 1;
}

int wf_process_client_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	return 0;
}

int wfreerdp_run(freerdp* instance)
{
	MSG msg;
	int index;
	int rcount;
	int wcount;
	BOOL msg_ret;
	int quit_msg;
	void* rfds[32];
	void* wfds[32];
	int fds_count;
	HANDLE fds[64];
	rdpChannels* channels;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (freerdp_connect(instance) != TRUE)
		return 0;

	channels = instance->context->channels;

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (wf_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
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
		if (MsgWaitForMultipleObjects(fds_count, fds, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			printf("wfreerdp_run: WaitForMultipleObjects failed: 0x%04X\n", GetLastError());
			break;
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_shall_disconnect(instance))	
		{
			break;
		}
		if (wf_check_fds(instance) != TRUE)
		{
			printf("Failed to check wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		wf_process_channel_event(channels, instance);

		quit_msg = FALSE;
		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			msg_ret = GetMessage(&msg, NULL, 0, 0);

			if (msg_ret == 0 || msg_ret == -1)
			{
				quit_msg = TRUE;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (quit_msg)
			break;
	}

	/* cleanup */

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_disconnect(instance);
	
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
	((wfContext*) instance->context)->wfi = wfi;
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
		while ((status = GetMessage( &msg, NULL, 0, 0 )) != 0)
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

	if (NULL == getenv("HOME"))
	{
		char home[MAX_PATH * 2] = "HOME=";
		strcat(home, getenv("HOMEDRIVE"));
		strcat(home, getenv("HOMEPATH"));
		_putenv(home);
	}

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
	freerdp_channels_global_init();

	instance = freerdp_new();
	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->Authenticate = wf_authenticate;
	instance->VerifyCertificate = wf_verify_certificate;
	instance->ReceiveChannelData = wf_receive_channel_data;

	instance->context_size = sizeof(wfContext);
	instance->ContextNew = wf_context_new;
	instance->ContextFree = wf_context_free;
	freerdp_context_new(instance);

	instance->context->argc = __argc;
	instance->context->argv = __argv;

        if (!CreateThread(NULL, 0, kbd_thread_func, NULL, 0, NULL))
		printf("error creating keyboard handler thread");

	//while (1)
	{
		int status;
		int arg_parse_result;

		data = (thread_data*) xzalloc(sizeof(thread_data)); 
		data->instance = instance;

		if (freerdp_detect_new_command_line_syntax(__argc, __argv))
		{
			printf("Using new command-line syntax\n");

			status = freerdp_client_parse_command_line_arguments(instance->context->argc,instance->context->argv, instance->settings);
			arg_parse_result = status;

			freerdp_client_load_addins(instance->context->channels, instance->settings);
		}
		else
		{
			arg_parse_result = freerdp_parse_args(instance->settings, __argc, __argv,
				wf_process_plugin_args, instance->context->channels, wf_process_client_args, NULL);
		}

		if (arg_parse_result < 0)
		{
			if (arg_parse_result == FREERDP_ARGS_PARSE_FAILURE)
				printf("failed to parse arguments.\n");

#ifdef _DEBUG
			system("pause");
#endif
			exit(-1);
		}

		if (CreateThread(NULL, 0, thread_func, data, 0, NULL) != 0)
			g_thread_count++;
	}

	if (g_thread_count > 0)
		WaitForSingleObject(g_done_event, INFINITE);
	else
		MessageBox(GetConsoleWindow(),
			L"Failed to start wfreerdp.\n\nPlease check the debug output.",
			L"FreeRDP Error", MB_ICONSTOP);

	freerdp_context_free(instance);
	freerdp_free(instance);

	WSACleanup();

#ifdef _DEBUG
	system("pause");
#endif

	return 0;
}
