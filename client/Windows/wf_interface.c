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
#include <assert.h>
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

void wf_size_scrollbars(wfContext* wfc, int client_width, int client_height);

int wf_create_console(void)
{
	if (!AllocConsole())
		return 1;

	freopen("CONOUT$", "w", stdout);
	fprintf(stderr, "Debug console created.\n");

	return 0;
}

void wf_sw_begin_paint(wfContext* wfc)
{
	rdpGdi* gdi = ((rdpContext*) wfc)->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void wf_sw_end_paint(wfContext* wfc)
{
	int i;
	rdpGdi* gdi;
	INT32 x, y;
	UINT32 w, h;
	int ninvalid;
	RECT update_rect;
	HGDI_RGN cinvalid;

	gdi = ((rdpContext*) wfc)->gdi;

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

		InvalidateRect(wfc->hwnd, &update_rect, FALSE);
	}
}

void wf_sw_desktop_resize(wfContext* wfc)
{
	rdpGdi* gdi;
	rdpContext* context;
	rdpSettings* settings;

	context = (rdpContext*) wfc;
	settings = wfc->instance->settings;
	gdi = context->gdi;

	wfc->width = settings->DesktopWidth;
	wfc->height = settings->DesktopHeight;
	gdi_resize(gdi, wfc->width, wfc->height);

	if (wfc->primary)
	{
		wf_image_free(wfc->primary);
		wfc->primary = wf_image_new(wfc, wfc->width, wfc->height, wfc->dstBpp, gdi->primary_buffer);
	}
}

void wf_hw_begin_paint(wfContext* wfc)
{
	wfc->hdc->hwnd->invalid->null = 1;
	wfc->hdc->hwnd->ninvalid = 0;
}

void wf_hw_end_paint(wfContext* wfc)
{

}

void wf_hw_desktop_resize(wfContext* wfc)
{
	BOOL same;
	RECT rect;
	rdpSettings* settings;

	settings = wfc->instance->settings;

	wfc->width = settings->DesktopWidth;
	wfc->height = settings->DesktopHeight;

	if (wfc->primary)
	{
		same = (wfc->primary == wfc->drawing) ? TRUE : FALSE;

		wf_image_free(wfc->primary);

		wfc->primary = wf_image_new(wfc, wfc->width, wfc->height, wfc->dstBpp, NULL);

		if (same)
			wfc->drawing = wfc->primary;
	}

	if (wfc->fullscreen != TRUE)
	{
		if (wfc->hwnd)
			SetWindowPos(wfc->hwnd, HWND_TOP, -1, -1, wfc->width + wfc->diff.x, wfc->height + wfc->diff.y, SWP_NOMOVE);
	}
	else
	{
		wf_update_offset(wfc);
		GetWindowRect(wfc->hwnd, &rect);
		InvalidateRect(wfc->hwnd, &rect, TRUE);
	}
}

BOOL wf_pre_connect(freerdp* instance)
{
	wfContext* wfc;
	int desktopWidth;
	int desktopHeight;
	rdpContext* context;
	rdpSettings* settings;

	context = instance->context;
	wfc = (wfContext*) instance->context;
	wfc->instance = instance;

	settings = instance->settings;

	if (settings->ConnectionFile)
	{
		if (wfc->connectionRdpFile)
		{
			freerdp_client_rdp_file_free(wfc->connectionRdpFile);
		}

		wfc->connectionRdpFile = freerdp_client_rdp_file_new();

		fprintf(stderr, "Using connection file: %s\n", settings->ConnectionFile);

		freerdp_client_parse_rdp_file(wfc->connectionRdpFile, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(wfc->connectionRdpFile, settings);
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

	wfc->fullscreen = settings->Fullscreen;
	wfc->fs_toggle = 1;
	wfc->sw_gdi = settings->SoftwareGdi;

	wfc->clrconv = (HCLRCONV) malloc(sizeof(CLRCONV));
	ZeroMemory(wfc->clrconv, sizeof(CLRCONV));

	wfc->clrconv->palette = NULL;
	wfc->clrconv->alpha = FALSE;

	instance->context->cache = cache_new(settings);

	desktopWidth = settings->DesktopWidth;
	desktopHeight = settings->DesktopHeight;

	if (wfc->percentscreen > 0)
	{
		desktopWidth = (GetSystemMetrics(SM_CXSCREEN) * wfc->percentscreen) / 100;
		settings->DesktopWidth = desktopWidth;

		desktopHeight = (GetSystemMetrics(SM_CYSCREEN) * wfc->percentscreen) / 100;
		settings->DesktopHeight = desktopHeight;
	}
	
	if (wfc->fullscreen)
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

void wf_add_system_menu(wfContext* wfc)
{
	HMENU hMenu = GetSystemMenu(wfc->hwnd, FALSE);

	MENUITEMINFO item_info;
	ZeroMemory(&item_info, sizeof(MENUITEMINFO));

	item_info.fMask = MIIM_CHECKMARKS | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_DATA;
	item_info.cbSize = sizeof(MENUITEMINFO);
	item_info.wID = SYSCOMMAND_ID_SMARTSIZING;
	item_info.fType = MFT_STRING;
	item_info.dwTypeData = _wcsdup(_T("Smart sizing"));
	item_info.cch = _wcslen(_T("Smart sizing"));
	item_info.dwItemData = (ULONG_PTR) wfc;

	InsertMenuItem(hMenu, 6, TRUE, &item_info);

	if (wfc->instance->settings->SmartSizing)
	{
		CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, MF_CHECKED);
	}
}

BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	DWORD dwStyle;
	rdpCache* cache;
	wfContext* wfc;
	rdpContext* context;
	WCHAR lpWindowName[64];
	rdpSettings* settings;

	settings = instance->settings;
	context = instance->context;
	wfc = (wfContext*) instance->context;
	cache = instance->context->cache;

	wfc->dstBpp = 32;
	wfc->width = settings->DesktopWidth;
	wfc->height = settings->DesktopHeight;

	if (wfc->sw_gdi)
	{
		gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_32BPP, NULL);
		gdi = instance->context->gdi;
		wfc->hdc = gdi->primary->hdc;
		wfc->primary = wf_image_new(wfc, wfc->width, wfc->height, wfc->dstBpp, gdi->primary_buffer);
	}
	else
	{
		wf_gdi_register_update_callbacks(instance->update);
		wfc->srcBpp = instance->settings->ColorDepth;
		wfc->primary = wf_image_new(wfc, wfc->width, wfc->height, wfc->dstBpp, NULL);

		wfc->hdc = gdi_GetDC();
		wfc->hdc->bitsPerPixel = wfc->dstBpp;
		wfc->hdc->bytesPerPixel = wfc->dstBpp / 8;

		wfc->hdc->alpha = wfc->clrconv->alpha;
		wfc->hdc->invert = wfc->clrconv->invert;

		wfc->hdc->hwnd = (HGDI_WND) malloc(sizeof(GDI_WND));
		wfc->hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
		wfc->hdc->hwnd->invalid->null = 1;

		wfc->hdc->hwnd->count = 32;
		wfc->hdc->hwnd->cinvalid = (HGDI_RGN) malloc(sizeof(GDI_RGN) * wfc->hdc->hwnd->count);
		wfc->hdc->hwnd->ninvalid = 0;

		if (settings->RemoteFxCodec)
		{
			wfc->tile = wf_image_new(wfc, 64, 64, 32, NULL);
			wfc->rfx_context = rfx_context_new(FALSE);
		}

		if (settings->NSCodec)
		{
			wfc->nsc_context = nsc_context_new();
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

	if (!wfc->hwnd)
	{
		wfc->hwnd = CreateWindowEx((DWORD) NULL, wfc->wndClassName, lpWindowName, dwStyle,
			0, 0, 0, 0, wfc->hWndParent, NULL, wfc->hInstance, NULL);

		SetWindowLongPtr(wfc->hwnd, GWLP_USERDATA, (LONG_PTR) wfc);
	}

	wf_resize_window(wfc);

	wf_add_system_menu(wfc);

	BitBlt(wfc->primary->hdc, 0, 0, wfc->width, wfc->height, NULL, 0, 0, BLACKNESS);
	wfc->drawing = wfc->primary;

	ShowWindow(wfc->hwnd, SW_SHOWNORMAL);
	UpdateWindow(wfc->hwnd);

	if (wfc->sw_gdi)
	{											
		instance->update->BeginPaint = (pBeginPaint) wf_sw_begin_paint;
		instance->update->EndPaint = (pEndPaint) wf_sw_end_paint;
		instance->update->DesktopResize = (pDesktopResize) wf_sw_desktop_resize;
	}
	else
	{
		instance->update->BeginPaint = (pBeginPaint) wf_hw_begin_paint;
		instance->update->EndPaint = (pEndPaint) wf_hw_end_paint;
		instance->update->DesktopResize = (pDesktopResize) wf_hw_desktop_resize;
	}

	pointer_cache_register_callbacks(instance->update);

	if (wfc->sw_gdi != TRUE)
	{
		brush_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
	}

	wf_register_graphics(instance->context->graphics);

	freerdp_channels_post_connect(instance->context->channels, instance);

	wf_cliprdr_init(wfc, instance->context->channels);

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

DWORD WINAPI wf_client_thread(LPVOID lpParam)
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
	wfContext* wfc;
	freerdp* instance;
	rdpChannels* channels;

	instance = (freerdp*) lpParam;
	assert(NULL != instance);

	wfc = (wfContext*) instance->context;
	assert(NULL != wfc);

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
					PostMessage(wfc->hwnd, WM_SETFOCUS, 0, 0);
				}
				else if ((msg.message == WM_KILLFOCUS) && (msg.lParam == 1))
				{
					PostMessage(wfc->hwnd, WM_KILLFOCUS, 0, 0);
				}
			}

			if (msg.message == WM_SIZE)
			{
				width = LOWORD(msg.lParam);
				height = HIWORD(msg.lParam);

				//wfc->client_width = width;
				//wfc->client_height = height;

				SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, width, height, SWP_FRAMECHANGED);
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
	freerdp_channels_close(channels, instance);
	freerdp_disconnect(instance);

	printf("Main thread exited.\n");

	ExitThread(0);
	
	return 0;
}

DWORD WINAPI wf_keyboard_thread(LPVOID lpParam)
{
	MSG msg;
	BOOL status;
	wfContext* wfc;
	HHOOK hook_handle;

	wfc = (wfContext*) lpParam;
	assert(NULL != wfc);

	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, wfc->hInstance, 0);

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

	printf("Keyboard thread exited.\n");

	ExitThread(0);
	return (DWORD) NULL;
}

rdpSettings* freerdp_client_get_settings(wfContext* wfc)
{
	return wfc->instance->settings;
}

int freerdp_client_focus_in(wfContext* wfc)
{
	PostThreadMessage(wfc->mainThreadId, WM_SETFOCUS, 0, 1);
	return 0;
}

int freerdp_client_focus_out(wfContext* wfc)
{
	PostThreadMessage(wfc->mainThreadId, WM_KILLFOCUS, 0, 1);
	return 0;
}

int freerdp_client_set_window_size(wfContext* wfc, int width, int height)
{
	if ((width != wfc->client_width) || (height != wfc->client_height))
	{
		PostThreadMessage(wfc->mainThreadId, WM_SIZE, SIZE_RESTORED, ((UINT) height << 16) | (UINT) width);
	}

	return 0;
}

void wf_on_param_change(freerdp* instance, int id)
{
	RECT rect;
	HMENU hMenu;
	wfContext* wfc = (wfContext*) instance->context;

	// specific processing here
	switch (id)
	{
		case FreeRDP_SmartSizing:
			fprintf(stderr, "SmartSizing changed.\n");

			if (!instance->settings->SmartSizing && (wfc->client_width > instance->settings->DesktopWidth || wfc->client_height > instance->settings->DesktopHeight))
			{
				GetWindowRect(wfc->hwnd, &rect);
				SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, MIN(wfc->client_width + wfc->offset_x, rect.right - rect.left), MIN(wfc->client_height + wfc->offset_y, rect.bottom - rect.top), SWP_NOMOVE | SWP_FRAMECHANGED);
				wf_update_canvas_diff(wfc);
			}

			hMenu = GetSystemMenu(wfc->hwnd, FALSE);
			CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, instance->settings->SmartSizing);
			wf_size_scrollbars(wfc, wfc->client_width, wfc->client_height);
			GetClientRect(wfc->hwnd, &rect);
			InvalidateRect(wfc->hwnd, &rect, TRUE);
			break;

		case FreeRDP_ConnectionType:
			fprintf(stderr, "ConnectionType changed.\n");
			freerdp_set_connection_type(wfc->instance->settings, wfc->instance->settings->ConnectionType);
			break;
	}
}

// TODO: Some of that code is a duplicate of wf_pre_connect. Refactor?
int freerdp_client_load_settings_from_rdp_file(wfContext* wfc, char* filename)
{
	rdpSettings* settings;

	settings = wfc->instance->settings;

	if (filename)
	{
		settings->ConnectionFile = _strdup(filename);

		// free old settings file
		freerdp_client_rdp_file_free(wfc->connectionRdpFile);
		wfc->connectionRdpFile = freerdp_client_rdp_file_new();

		fprintf(stderr, "Using connection file: %s\n", settings->ConnectionFile);

		if (!freerdp_client_parse_rdp_file(wfc->connectionRdpFile, settings->ConnectionFile))
		{
			return 1;
		}

		if (!freerdp_client_populate_settings_from_rdp_file(wfc->connectionRdpFile, settings))
		{
			return 2;
		}
	}

	return 0;
}

int freerdp_client_save_settings_to_rdp_file(wfContext* wfc, char* filename)
{
	if (!filename)
		return 1;

	if (wfc->instance->settings->ConnectionFile)
	{
		free(wfc->instance->settings->ConnectionFile);
	}

	wfc->instance->settings->ConnectionFile = _strdup(filename);

	// Reuse existing rdpFile structure if available, to preserve unsupported settings when saving to disk.
	if (wfc->connectionRdpFile == NULL)
	{
		wfc->connectionRdpFile = freerdp_client_rdp_file_new();
	}

	if (!freerdp_client_populate_rdp_file_from_settings(wfc->connectionRdpFile, wfc->instance->settings))
	{
		return 1;
	}

	if (!freerdp_client_write_rdp_file(wfc->connectionRdpFile, filename, UNICODE));
	{
		return 2;
	}

	return 0;
}

void wf_size_scrollbars(wfContext* wfc, int client_width, int client_height)
{
	BOOL rc;

	if (wfc->disablewindowtracking == TRUE)
	{
		return;
	}

	// prevent infinite message loop
	wfc->disablewindowtracking = TRUE;

	if (wfc->instance->settings->SmartSizing)
	{
		wfc->xCurrentScroll = 0;
		wfc->yCurrentScroll = 0;

		if (wfc->xScrollVisible || wfc->yScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_BOTH, FALSE))
			{
				wfc->xScrollVisible = FALSE;
				wfc->yScrollVisible = FALSE;
			}
		}
	}
	else
	{
		SCROLLINFO si;
		BOOL horiz = wfc->xScrollVisible;
		BOOL vert = wfc->yScrollVisible;;

		if (!horiz && client_width < wfc->instance->settings->DesktopWidth)
		{
			horiz = TRUE;		
		}
		else if (horiz && client_width >= wfc->instance->settings->DesktopWidth/* - GetSystemMetrics(SM_CXVSCROLL)*/)
		{
			horiz = FALSE;		
		}

		if (!vert && client_height < wfc->instance->settings->DesktopHeight)
		{
			vert = TRUE;
		}
		else if (vert && client_height >= wfc->instance->settings->DesktopHeight/* - GetSystemMetrics(SM_CYHSCROLL)*/)
		{
			vert = FALSE;
		}

		if (horiz == vert && (horiz != wfc->xScrollVisible && vert != wfc->yScrollVisible))
		{
			if (ShowScrollBar(wfc->hwnd, SB_BOTH, horiz))
			{
				wfc->xScrollVisible = horiz;
				wfc->yScrollVisible = vert;
			}
		}

		if (horiz != wfc->xScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_HORZ, horiz))
			{
				wfc->xScrollVisible = horiz;
			}
		}

		if (vert != wfc->yScrollVisible)
		{
			if (ShowScrollBar(wfc->hwnd, SB_VERT, vert))
			{
				wfc->yScrollVisible = vert;
			}
		}

		if (horiz)
		{
			// The horizontal scrolling range is defined by 
			// (bitmap_width) - (client_width). The current horizontal 
			// scroll value remains within the horizontal scrolling range. 
			wfc->xMaxScroll = MAX(wfc->instance->settings->DesktopWidth - client_width, 0);
			wfc->xCurrentScroll = MIN(wfc->xCurrentScroll, wfc->xMaxScroll);
			si.cbSize = sizeof(si); 
			si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
			si.nMin   = wfc->xMinScroll;
			si.nMax   = wfc->instance->settings->DesktopWidth;
			si.nPage  = client_width; 
			si.nPos   = wfc->xCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_HORZ, &si, TRUE);
		}

		if (vert)
		{
			// The vertical scrolling range is defined by 
			// (bitmap_height) - (client_height). The current vertical 
			// scroll value remains within the vertical scrolling range. 
			wfc->yMaxScroll = MAX(wfc->instance->settings->DesktopHeight - client_height, 0);
			wfc->yCurrentScroll = MIN(wfc->yCurrentScroll, wfc->yMaxScroll);
			si.cbSize = sizeof(si); 
			si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS; 
			si.nMin   = wfc->yMinScroll;
			si.nMax   = wfc->instance->settings->DesktopHeight;
			si.nPage  = client_height; 
			si.nPos   = wfc->yCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_VERT, &si, TRUE);
		}
	}

	wfc->disablewindowtracking = FALSE;
	wf_update_canvas_diff(wfc);
}

void wfreerdp_client_global_init(void)
{
	WSADATA wsaData;

	if (!getenv("HOME"))
	{
		char home[MAX_PATH * 2] = "HOME=";
		strcat(home, getenv("HOMEDRIVE"));
		strcat(home, getenv("HOMEPATH"));
		_putenv(home);
	}

	WSAStartup(0x101, &wsaData);

#if defined(WITH_DEBUG) || defined(_DEBUG)
	wf_create_console();
#endif

	freerdp_channels_global_init();

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
}

void wfreerdp_client_global_uninit(void)
{
	WSACleanup();
}

int wfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	wfContext* wfc = (wfContext*) context;

	wfreerdp_client_global_init();

	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->Authenticate = wf_authenticate;
	instance->VerifyCertificate = wf_verify_certificate;
	instance->ReceiveChannelData = wf_receive_channel_data;

	wfc->instance = instance;
	context->channels = freerdp_channels_new();

	return 0;
}

void wfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	if (context->cache)
		cache_free(context->cache);

	freerdp_channels_free(context->channels);
}

int wfreerdp_client_start(rdpContext* context)
{
	HWND hWndParent;
	HINSTANCE hInstance;
	wfContext* wfc = (wfContext*) context;
	freerdp* instance = context->instance;

	hInstance = GetModuleHandle(NULL);
	hWndParent = (HWND) instance->settings->ParentWindowId;
	instance->settings->EmbeddedWindow = (hWndParent) ? TRUE : FALSE;

	wfc->hWndParent = hWndParent;
	wfc->hInstance = hInstance;
	wfc->cursor = LoadCursor(NULL, IDC_ARROW);
	wfc->icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
	wfc->wndClassName = _tcsdup(_T("FreeRDP"));

	wfc->wndClass.cbSize = sizeof(WNDCLASSEX);
	wfc->wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wfc->wndClass.lpfnWndProc = wf_event_proc;
	wfc->wndClass.cbClsExtra = 0;
	wfc->wndClass.cbWndExtra = 0;
	wfc->wndClass.hCursor = wfc->cursor;
	wfc->wndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
	wfc->wndClass.lpszMenuName = NULL;
	wfc->wndClass.lpszClassName = wfc->wndClassName;
	wfc->wndClass.hInstance = hInstance;
	wfc->wndClass.hIcon = wfc->icon;
	wfc->wndClass.hIconSm = wfc->icon;
	RegisterClassEx(&(wfc->wndClass));

	wfc->keyboardThread = CreateThread(NULL, 0, wf_keyboard_thread, (void*) wfc, 0, &wfc->keyboardThreadId);

	if (!wfc->keyboardThread)
		return -1;

	freerdp_client_load_addins(context->channels, instance->settings);

	wfc->thread = CreateThread(NULL, 0, wf_client_thread, (void*) instance, 0, &wfc->mainThreadId);

	if (!wfc->thread)
		return -1;

	return 0;
}

int wfreerdp_client_stop(rdpContext* context)
{
	wfContext* wfc = (wfContext*) context;

	if (wfc->thread)
	{
		PostThreadMessage(wfc->mainThreadId, WM_QUIT, 0, 0);

		WaitForSingleObject(wfc->thread, INFINITE);
		CloseHandle(wfc->thread);
		wfc->thread = NULL;
		wfc->mainThreadId = 0;
	}

	if (wfc->keyboardThread)
	{
		PostThreadMessage(wfc->keyboardThreadId, WM_QUIT, 0, 0);

		WaitForSingleObject(wfc->keyboardThread, INFINITE);
		CloseHandle(wfc->keyboardThread);

		wfc->keyboardThread = NULL;
		wfc->keyboardThreadId = 0;
	}

	return 0;
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);

	pEntryPoints->GlobalInit = wfreerdp_client_global_init;
	pEntryPoints->GlobalUninit = wfreerdp_client_global_uninit;

	pEntryPoints->ContextSize = sizeof(wfContext);
	pEntryPoints->ClientNew = wfreerdp_client_new;
	pEntryPoints->ClientFree = wfreerdp_client_free;

	pEntryPoints->ClientStart = wfreerdp_client_start;
	pEntryPoints->ClientStop = wfreerdp_client_stop;

	return 0;
}
