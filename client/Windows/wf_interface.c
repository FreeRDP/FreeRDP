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
#include <freerdp/utils/event.h>
#include <freerdp/utils/svc_plugin.h>

//#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "wf_gdi.h"
#include "wf_graphics.h"
#include "wf_cliprdr.h"

#include "wf_interface.h"

#include "resource.h"

wfInfo* wf_wfi_new()
{
	wfInfo* wfi;

	wfi = (wfInfo*) malloc(sizeof(wfInfo));
	ZeroMemory(wfi, sizeof(wfInfo));

	return wfi;
}

void wf_wfi_free(wfInfo* wfi)
{
	free(wfi);
}

void wf_context_new(freerdp* instance, rdpContext* context)
{
	wfInfo* wfi;

	context->channels = freerdp_channels_new();

	wfi = wf_wfi_new();

	((wfContext*) context)->wfi = wfi;
	wfi->instance = instance;

	// Register callbacks
	instance->context->client->OnParamChange = wf_on_param_change;
}

void wf_context_free(freerdp* instance, rdpContext* context)
{
	if (context->cache)
		cache_free(context->cache);

	freerdp_channels_free(context->channels);

	wf_wfi_free(((wfContext*) context)->wfi);
	((wfContext*) context)->wfi = NULL;
}

int wf_create_console(void)
{
	if (!AllocConsole())
		return 1;

	freopen("CONOUT$", "w", stdout);
	fprintf(stderr, "Debug console created.\n");

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

void wf_sw_desktop_resize(rdpContext* context)
{
	wfInfo* wfi;
	rdpGdi* gdi;
	rdpSettings* settings;

	wfi = ((wfContext*) context)->wfi;
	settings = wfi->instance->settings;
	gdi = context->gdi;

	wfi->width = settings->DesktopWidth;
	wfi->height = settings->DesktopHeight;
	gdi_resize(gdi, wfi->width, wfi->height);

	if (wfi->primary)
	{
		wf_image_free(wfi->primary);
		wfi->primary = wf_image_new(wfi, wfi->width, wfi->height, wfi->dstBpp, gdi->primary_buffer);
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

void wf_hw_desktop_resize(rdpContext* context)
{
	wfInfo* wfi;
	BOOL same;
	RECT rect;
	rdpSettings* settings;

	wfi = ((wfContext*) context)->wfi;
	settings = wfi->instance->settings;

	wfi->width = settings->DesktopWidth;
	wfi->height = settings->DesktopHeight;

	if (wfi->primary)
	{
		same = (wfi->primary == wfi->drawing) ? TRUE : FALSE;

		wf_image_free(wfi->primary);

		wfi->primary = wf_image_new(wfi, wfi->width, wfi->height, wfi->dstBpp, NULL);

		if (same)
			wfi->drawing = wfi->primary;
	}

	if (wfi->fullscreen != TRUE)
	{
		if (wfi->hwnd)
			SetWindowPos(wfi->hwnd, HWND_TOP, -1, -1, wfi->width + wfi->diff.x, wfi->height + wfi->diff.y, SWP_NOMOVE);
	}
	else
	{
		wf_update_offset(wfi);
		GetWindowRect(wfi->hwnd, &rect);
		InvalidateRect(wfi->hwnd, &rect, TRUE);
	}
}

BOOL wf_pre_connect(freerdp* instance)
{
	int desktopWidth, desktopHeight;
	wfInfo* wfi;
	wfContext* context;
	rdpSettings* settings;

	context = (wfContext*) instance->context;

	wfi = context->wfi;
	wfi->instance = instance;

	settings = instance->settings;

	if (settings->ConnectionFile)
	{
		if (wfi->connectionRdpFile)
		{
			freerdp_client_rdp_file_free(wfi->connectionRdpFile);
		}

		wfi->connectionRdpFile = freerdp_client_rdp_file_new();

		fprintf(stderr, "Using connection file: %s\n", settings->ConnectionFile);

		freerdp_client_parse_rdp_file(wfi->connectionRdpFile, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(wfi->connectionRdpFile, settings);
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

	wfi->fullscreen = settings->Fullscreen;
	wfi->fs_toggle = 1;
	wfi->sw_gdi = settings->SoftwareGdi;

	wfi->clrconv = (HCLRCONV) malloc(sizeof(CLRCONV));
	ZeroMemory(wfi->clrconv, sizeof(CLRCONV));

	wfi->clrconv->palette = NULL;
	wfi->clrconv->alpha = FALSE;

	instance->context->cache = cache_new(settings);

	desktopWidth = settings->DesktopWidth;
	desktopHeight = settings->DesktopHeight;

	if (wfi->percentscreen > 0)
	{
		desktopWidth = (GetSystemMetrics(SM_CXSCREEN) * wfi->percentscreen) / 100;
		settings->DesktopWidth = desktopWidth;

		desktopHeight = (GetSystemMetrics(SM_CYSCREEN) * wfi->percentscreen) / 100;
		settings->DesktopHeight = desktopHeight;
	}
	
	if (wfi->fullscreen)
	{
		if (settings->UseMultimon)
		{
			settings->DesktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			settings->DesktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		}
		else
		{
			settings->DesktopWidth = GetSystemMetrics(SM_CXSCREEN);
			settings->DesktopHeight = GetSystemMetrics(SM_CYSCREEN);
		}
	}

	desktopWidth = (desktopWidth + 3) & (~3);

	if (desktopWidth != settings->DesktopWidth)
	{
		freerdp_set_param_uint32(settings, FreeRDP_DesktopWidth, desktopWidth);
	}

	if (desktopHeight != settings->DesktopHeight)
	{
		freerdp_set_param_uint32(settings, FreeRDP_DesktopHeight, desktopHeight);
	}

	if ((settings->DesktopWidth < 64) || (settings->DesktopHeight < 64) ||
		(settings->DesktopWidth > 4096) || (settings->DesktopHeight > 4096))
	{
		fprintf(stderr, "wf_pre_connect: invalid dimensions %d %d\n", settings->DesktopWidth, settings->DesktopHeight);
		return 1;
	}

	freerdp_set_param_uint32(settings, FreeRDP_KeyboardLayout, (int) GetKeyboardLayout(0) & 0x0000FFFF);
	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

void wf_add_system_menu(wfInfo* wfi)
{
	HMENU hMenu = GetSystemMenu(wfi->hwnd, FALSE);

	MENUITEMINFO item_info;
	ZeroMemory(&item_info, sizeof(MENUITEMINFO));

	item_info.fMask = MIIM_CHECKMARKS | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_DATA;
	item_info.cbSize = sizeof(MENUITEMINFO);
	item_info.wID = SYSCOMMAND_ID_SMARTSIZING;
	item_info.fType = MFT_STRING;
	item_info.dwTypeData = _wcsdup(_T("Smart sizing"));
	item_info.cch = _wcslen(_T("Smart sizing"));
	item_info.dwItemData = (ULONG_PTR) wfi;

	InsertMenuItem(hMenu, 6, TRUE, &item_info);

	if (wfi->instance->settings->SmartSizing)
	{
		CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, MF_CHECKED);
	}
}

BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	wfInfo* wfi;
	DWORD dwStyle;
	rdpCache* cache;
	wfContext* context;
	WCHAR lpWindowName[64];
	rdpSettings* settings;

	settings = instance->settings;
	context = (wfContext*) instance->context;
	cache = instance->context->cache;
	wfi = context->wfi;

	wfi->dstBpp = 32;
	wfi->width = settings->DesktopWidth;
	wfi->height = settings->DesktopHeight;

	if (wfi->sw_gdi)
	{
		gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_32BPP, NULL);
		gdi = instance->context->gdi;
		wfi->hdc = gdi->primary->hdc;
		wfi->primary = wf_image_new(wfi, wfi->width, wfi->height, wfi->dstBpp, gdi->primary_buffer);
	}
	else
	{
		wf_gdi_register_update_callbacks(instance->update);
		wfi->srcBpp = instance->settings->ColorDepth;
		wfi->primary = wf_image_new(wfi, wfi->width, wfi->height, wfi->dstBpp, NULL);

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

		if (settings->RemoteFxCodec)
		{
			wfi->tile = wf_image_new(wfi, 64, 64, 32, NULL);
			wfi->rfx_context = rfx_context_new();
		}

		if (settings->NSCodec)
		{
			wfi->nsc_context = nsc_context_new();
		}
	}

	if (settings->WindowTitle != NULL)
		_snwprintf(lpWindowName, ARRAYSIZE(lpWindowName), L"%S", settings->WindowTitle);
	else if (settings->ServerPort == 3389)
		_snwprintf(lpWindowName, ARRAYSIZE(lpWindowName), L"FreeRDP: %S", settings->ServerHostname);
	else
		_snwprintf(lpWindowName, ARRAYSIZE(lpWindowName), L"FreeRDP: %S:%d", settings->ServerHostname, settings->ServerPort);

	if (!settings->Decorations)
		dwStyle = WS_CHILD | WS_BORDER;
	else
		dwStyle = 0;

	if (!wfi->hwnd)
	{
		wfi->hwnd = CreateWindowEx((DWORD) NULL, wfi->wndClassName, lpWindowName, dwStyle,
			0, 0, 0, 0, wfi->hWndParent, NULL, wfi->hInstance, NULL);

		SetWindowLongPtr(wfi->hwnd, GWLP_USERDATA, (LONG_PTR) wfi);
	}

	wf_resize_window(wfi);

	wf_add_system_menu(wfi);

	BitBlt(wfi->primary->hdc, 0, 0, wfi->width, wfi->height, NULL, 0, 0, BLACKNESS);
	wfi->drawing = wfi->primary;

	ShowWindow(wfi->hwnd, SW_SHOWNORMAL);
	UpdateWindow(wfi->hwnd);

	if (wfi->sw_gdi)
	{											
		instance->update->BeginPaint = wf_sw_begin_paint;
		instance->update->EndPaint = wf_sw_end_paint;
		instance->update->DesktopResize = wf_sw_desktop_resize;
	}
	else
	{
		instance->update->BeginPaint = wf_hw_begin_paint;
		instance->update->EndPaint = wf_hw_end_paint;
		instance->update->DesktopResize = wf_hw_desktop_resize;
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

	// Callback
	if (wfi->client_callback_func != NULL)
	{
		wfi->client_callback_func(wfi, CALLBACK_TYPE_CONNECTED, 0, 0);
	}

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
		fprintf(stderr, "CredUIPromptForCredentials unexpected status: 0x%08X\n", status);
		return FALSE;
	}

	ZeroMemory(User, sizeof(User));
	ZeroMemory(Domain, sizeof(Domain));

	status = CredUIParseUserNameA(UserName, User, sizeof(User), Domain, sizeof(Domain));

	//fprintf(stderr, "User: %s Domain: %s Password: %s\n", User, Domain, Password);

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
	wMessage* event;

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

DWORD WINAPI wf_thread(LPVOID lpParam)
{
	MSG msg;
	int index;
	int rcount;
	int wcount;
	int width;
	int height;
	BOOL msg_ret;
	int quit_msg;
	void* rfds[32];
	void* wfds[32];
	int fds_count;
	HANDLE fds[64];
	freerdp* instance;
	rdpChannels* channels;

	instance = (freerdp*) lpParam;

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(wfds, sizeof(wfds));

	if (freerdp_connect(instance) != TRUE)
		return 0;

	channels = instance->context->channels;

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (wf_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			fprintf(stderr, "Failed to get wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			fprintf(stderr, "Failed to get channel manager file descriptor\n");
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
			fprintf(stderr, "wfreerdp_run: fds_count is zero\n");
			break;
		}

		/* do the wait */
		if (MsgWaitForMultipleObjects(fds_count, fds, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			fprintf(stderr, "wfreerdp_run: WaitForMultipleObjects failed: 0x%04X\n", GetLastError());
			break;
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_shall_disconnect(instance))	
		{
			break;
		}
		if (wf_check_fds(instance) != TRUE)
		{
			fprintf(stderr, "Failed to check wfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			fprintf(stderr, "Failed to check channel manager file descriptor\n");
			break;
		}
		wf_process_channel_event(channels, instance);

		quit_msg = FALSE;

		while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			msg_ret = GetMessage(&msg, NULL, 0, 0);

			if (instance->settings->EmbeddedWindow)
			{
				if ((msg.message == WM_SETFOCUS) && (msg.lParam == 1))
				{
					PostMessage(((wfContext*) instance->context)->wfi->hwnd, WM_SETFOCUS, 0, 0); 
				}
				else if ((msg.message == WM_KILLFOCUS) && (msg.lParam == 1))
				{
					PostMessage(((wfContext*) instance->context)->wfi->hwnd, WM_KILLFOCUS, 0, 0); 
				}
			}

			if (msg.message == WM_SIZE)
			{
				width = LOWORD(msg.lParam);
				height = HIWORD(msg.lParam);

				//((wfContext*) instance->context)->wfi->client_width = width;
				//((wfContext*) instance->context)->wfi->client_height = height;

				SetWindowPos(((wfContext*) instance->context)->wfi->hwnd, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED);
			}

			if ((msg_ret == 0) || (msg_ret == -1))
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
	((wfContext*) instance->context)->wfi->mainThreadId = 0;

	freerdp_channels_close(channels, instance);
	freerdp_disconnect(instance);	

	// Callback
	if (((wfContext*) instance->context)->wfi->client_callback_func != NULL)
	{
		((wfContext*) instance->context)->wfi->client_callback_func(((wfContext*) instance->context)->wfi, CALLBACK_TYPE_DISCONNECTED, 12, 34);
	}

	return 0;
}

DWORD WINAPI wf_keyboard_thread(LPVOID lpParam)
{
	MSG msg;
	BOOL status;
	wfInfo* wfi;
	HHOOK hook_handle;

	wfi = (wfInfo*) lpParam;

	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, wfi->hInstance, 0);

	if (hook_handle)
	{
		while ((status = GetMessage(&msg, NULL, 0, 0)) != 0)
		{
			if (status == -1)
			{
				fprintf(stderr, "keyboard thread error getting message\n");
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
	{
		fprintf(stderr, "failed to install keyboard hook\n");
	}

	wfi->keyboardThreadId = 0;
	printf("Keyboard thread exited.\n");
	return (DWORD) NULL;
}

int freerdp_client_global_init()
{
	WSADATA wsaData;

	if (!getenv("HOME"))
	{
		char home[MAX_PATH * 2] = "HOME=";
		strcat(home, getenv("HOMEDRIVE"));
		strcat(home, getenv("HOMEPATH"));
		_putenv(home);
	}

	if (WSAStartup(0x101, &wsaData) != 0)
		return 1;

#if defined(WITH_DEBUG) || defined(_DEBUG)
	wf_create_console();
#endif

	freerdp_channels_global_init();

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

	return 0;
}

int freerdp_client_global_uninit()
{
	WSACleanup();

	return 0;
}

wfInfo* freerdp_client_new(int argc, char** argv)
{
	int index;
	int status;
	wfInfo* wfi;
	freerdp* instance;

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

	wfi = ((wfContext*) (instance->context))->wfi;

	wfi->instance = instance;
	wfi->client = instance->context->client;

	instance->context->argc = argc;
	instance->context->argv = (char**) malloc(sizeof(char*) * argc);

	for (index = 0; index < argc; index++)
		instance->context->argv[index] = _strdup(argv[index]);

	status = freerdp_client_parse_command_line_arguments(instance->context->argc, instance->context->argv, instance->settings);

	return wfi;
}

rdpSettings* freerdp_client_get_settings(wfInfo* wfi)
{
	return wfi->instance->settings;
}

int freerdp_client_start(wfInfo* wfi)
{
	HWND hWndParent;
	HINSTANCE hInstance;
	freerdp* instance = wfi->instance;

	hInstance = GetModuleHandle(NULL);
	hWndParent = (HWND) instance->settings->ParentWindowId;
	instance->settings->EmbeddedWindow = (hWndParent) ? TRUE : FALSE;

	wfi->hWndParent = hWndParent;
	wfi->hInstance = hInstance;
	wfi->cursor = LoadCursor(NULL, IDC_ARROW);
	wfi->icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wfi->wndClassName = _tcsdup(_T("FreeRDP"));

	wfi->wndClass.cbSize = sizeof(WNDCLASSEX);
	wfi->wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wfi->wndClass.lpfnWndProc = wf_event_proc;
	wfi->wndClass.cbClsExtra = 0;
	wfi->wndClass.cbWndExtra = 0;
	wfi->wndClass.hCursor = wfi->cursor;
	wfi->wndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
	wfi->wndClass.lpszMenuName = NULL;
	wfi->wndClass.lpszClassName = wfi->wndClassName;
	wfi->wndClass.hInstance = hInstance;
	wfi->wndClass.hIcon = wfi->icon;
	wfi->wndClass.hIconSm = wfi->icon;
	RegisterClassEx(&(wfi->wndClass));

	wfi->keyboardThread = CreateThread(NULL, 0, wf_keyboard_thread, (void*) wfi, 0, &wfi->keyboardThreadId);

	if (!wfi->keyboardThread)
		return -1;

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	wfi->thread = CreateThread(NULL, 0, wf_thread, (void*) instance, 0, &wfi->mainThreadId);

	if (!wfi->thread)
		return -1;

	printf("Main thread exited.\n");
	return 0;
}

int freerdp_client_stop(wfInfo* wfi)
{
	if (wfi->mainThreadId)
		PostThreadMessage(wfi->mainThreadId, WM_QUIT, 0, 0);

	if (wfi->keyboardThreadId)
		PostThreadMessage(wfi->keyboardThreadId, WM_QUIT, 0, 0);
	return 0;
}

HANDLE freerdp_client_get_thread(wfInfo* cfi)
{
    return cfi->thread;
}

freerdp* freerdp_client_get_instance(wfInfo* cfi)
{
    return cfi->instance;
}

rdpClient* freerdp_client_get_interface(wfInfo* cfi)
{
    return cfi->client;
}

int freerdp_client_focus_in(wfInfo* wfi)
{
	PostThreadMessage(wfi->mainThreadId, WM_SETFOCUS, 0, 1);
	return 0;
}

int freerdp_client_focus_out(wfInfo* wfi)
{
	PostThreadMessage(wfi->mainThreadId, WM_KILLFOCUS, 0, 1);
	return 0;
}

int freerdp_client_set_window_size(wfInfo* wfi, int width, int height)
{
	if ((width != wfi->client_width) || (height != wfi->client_height))
	{
		PostThreadMessage(wfi->mainThreadId, WM_SIZE, SIZE_RESTORED, ((UINT) height << 16) | (UINT) width);
	}

	return 0;
}

int freerdp_client_free(wfInfo* wfi)
{
	freerdp* instance = wfi->instance;

	freerdp_context_free(instance);
	freerdp_free(instance);

	return 0;
}

void wf_on_param_change(freerdp* instance, int id)
{
	wfInfo* cfi = ((wfContext*) instance->context)->wfi;
	RECT rect;
	HMENU hMenu;

	// specific processing here
	switch(id)
	{
		case FreeRDP_SmartSizing:
			fprintf(stderr, "SmartSizing changed.\n");

			if (!instance->settings->SmartSizing && (cfi->client_width > instance->settings->DesktopWidth || cfi->client_height > instance->settings->DesktopHeight))
			{
				GetWindowRect(cfi->hwnd, &rect);
				SetWindowPos(cfi->hwnd, HWND_TOP, 0, 0, MIN(cfi->client_width + cfi->offset_x, rect.right - rect.left), MIN(cfi->client_height + cfi->offset_y, rect.bottom - rect.top), SWP_NOMOVE | SWP_FRAMECHANGED);
				wf_update_canvas_diff(cfi);
			}

			hMenu = GetSystemMenu(cfi->hwnd, FALSE);
			CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, instance->settings->SmartSizing);
			wf_size_scrollbars(cfi, cfi->client_width, cfi->client_height);
			GetClientRect(cfi->hwnd, &rect);
			InvalidateRect(cfi->hwnd, &rect, TRUE);
			break;

		case FreeRDP_ConnectionType:
			fprintf(stderr, "ConnectionType changed.\n");
			freerdp_set_connection_type(cfi->instance->settings, cfi->instance->settings->ConnectionType);
			break;
	}

	// trigger callback to client

	if (cfi->client_callback_func != NULL)
	{
		fprintf(stderr, "Notifying client...");
		cfi->client_callback_func(cfi, CALLBACK_TYPE_PARAM_CHANGE, id, 0);
	}
}

int freerdp_client_set_client_callback_function(wfInfo* cfi, callbackFunc callbackFunc)
{
	cfi->client_callback_func = callbackFunc;
	return 0;
}

// TODO: Some of that code is a duplicate of wf_pre_connect. Refactor?
int freerdp_client_load_settings_from_rdp_file(wfInfo* cfi, char* filename)
{
	rdpSettings* settings;

	settings = cfi->instance->settings;

	if (filename)
	{
		settings->ConnectionFile = _strdup(filename);

		// free old settings file
		freerdp_client_rdp_file_free(cfi->connectionRdpFile);
		cfi->connectionRdpFile = freerdp_client_rdp_file_new();

		fprintf(stderr, "Using connection file: %s\n", settings->ConnectionFile);

		if (!freerdp_client_parse_rdp_file(cfi->connectionRdpFile, settings->ConnectionFile))
		{
			return 1;
		}

		if (!freerdp_client_populate_settings_from_rdp_file(cfi->connectionRdpFile, settings))
		{
			return 2;
		}
	}

	return 0;
}

int freerdp_client_save_settings_to_rdp_file(wfInfo* cfi, char* filename)
{
	if (filename == NULL)
		return 1;

	if (cfi->instance->settings->ConnectionFile)
	{
		free(cfi->instance->settings->ConnectionFile);
	}

	cfi->instance->settings->ConnectionFile = _strdup(filename);

	// Reuse existing rdpFile structure if available, to preserve unsupported settings when saving to disk.
	if (cfi->connectionRdpFile == NULL)
	{
		cfi->connectionRdpFile = freerdp_client_rdp_file_new();
	}

	if (!freerdp_client_populate_rdp_file_from_settings(cfi->connectionRdpFile, cfi->instance->settings))
	{
		return 1;
	}

	if (!freerdp_client_write_rdp_file(cfi->connectionRdpFile, filename, UNICODE));
	{
		return 2;
	}

	return 0;
}


void wf_size_scrollbars(wfInfo* wfi, int client_width, int client_height)
{
	BOOL rc;
	if (wfi->disablewindowtracking == TRUE)
	{
		return;
	}


	// prevent infinite message loop
	wfi->disablewindowtracking = TRUE;

	if (wfi->instance->settings->SmartSizing)
	{
		wfi->xCurrentScroll = 0;
		wfi->yCurrentScroll = 0;

		if (wfi->xScrollVisible || wfi->yScrollVisible)
		{
			if (ShowScrollBar(wfi->hwnd, SB_BOTH, FALSE))
			{
				wfi->xScrollVisible = FALSE;
				wfi->yScrollVisible = FALSE;
			}
		}
	}
	else
	{
		SCROLLINFO si;
		BOOL horiz = wfi->xScrollVisible;
		BOOL vert = wfi->yScrollVisible;;

		if (!horiz && client_width < wfi->instance->settings->DesktopWidth)
		{
			horiz = TRUE;		
		}
		else if (horiz && client_width >= wfi->instance->settings->DesktopWidth/* - GetSystemMetrics(SM_CXVSCROLL)*/)
		{
			horiz = FALSE;		
		}

		if (!vert && client_height < wfi->instance->settings->DesktopHeight)
		{
			vert = TRUE;
		}
		else if (vert && client_height >= wfi->instance->settings->DesktopHeight/* - GetSystemMetrics(SM_CYHSCROLL)*/)
		{
			vert = FALSE;
		}

		if (horiz == vert && (horiz != wfi->xScrollVisible && vert != wfi->yScrollVisible))
		{
			if (ShowScrollBar(wfi->hwnd, SB_BOTH, horiz))
			{
				wfi->xScrollVisible = horiz;
				wfi->yScrollVisible = vert;
			}
		}

		if (horiz != wfi->xScrollVisible)
		{
			if (ShowScrollBar(wfi->hwnd, SB_HORZ, horiz))
			{
				wfi->xScrollVisible = horiz;
			}
		}

		if (vert != wfi->yScrollVisible)
		{
			if (ShowScrollBar(wfi->hwnd, SB_VERT, vert))
			{
				wfi->yScrollVisible = vert;
			}
		}

		if (horiz)
		{
			// The horizontal scrolling range is defined by 
			// (bitmap_width) - (client_width). The current horizontal 
			// scroll value remains within the horizontal scrolling range. 
			wfi->xMaxScroll = MAX(wfi->instance->settings->DesktopWidth - client_width, 0); 
			wfi->xCurrentScroll = MIN(wfi->xCurrentScroll, wfi->xMaxScroll); 
			si.cbSize = sizeof(si); 
			si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
			si.nMin   = wfi->xMinScroll; 
			si.nMax   = wfi->instance->settings->DesktopWidth; 
			si.nPage  = client_width; 
			si.nPos   = wfi->xCurrentScroll; 
			SetScrollInfo(wfi->hwnd, SB_HORZ, &si, TRUE); 
		}

		if (vert)
		{
			// The vertical scrolling range is defined by 
			// (bitmap_height) - (client_height). The current vertical 
			// scroll value remains within the vertical scrolling range. 
			wfi->yMaxScroll = MAX(wfi->instance->settings->DesktopHeight - client_height, 0); 
			wfi->yCurrentScroll = MIN(wfi->yCurrentScroll, wfi->yMaxScroll); 
			si.cbSize = sizeof(si); 
			si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
			si.nMin   = wfi->yMinScroll; 
			si.nMax   = wfi->instance->settings->DesktopHeight; 
			si.nPage  = client_height; 
			si.nPos   = wfi->yCurrentScroll; 
			SetScrollInfo(wfi->hwnd, SB_VERT, &si, TRUE); 
		}
	}

	wfi->disablewindowtracking = FALSE;
	wf_update_canvas_diff(wfi);		
}
