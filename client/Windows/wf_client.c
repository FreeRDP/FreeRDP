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
 *	 http://www.apache.org/licenses/LICENSE-2.0
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

#include <freerdp/log.h>
#include <freerdp/event.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/codec/region.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "wf_gdi.h"
#include "wf_rail.h"
#include "wf_channels.h"
#include "wf_graphics.h"
#include "wf_cliprdr.h"

#include "wf_client.h"

#include "resource.h"

#define TAG CLIENT_TAG("windows")

static BOOL wf_create_console(void)
{
#if defined(WITH_WIN_CONSOLE)
	if (!AttachConsole(ATTACH_PARENT_PROCESS))
		return FALSE;

	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	clearerr(stdout);
	clearerr(stderr);
	fflush(stdout);
	fflush(stderr);

	freopen("CONIN$", "r", stdin);
	clearerr(stdin);

	WLog_INFO(TAG, "Debug console created.");

	return TRUE;
#else
	return FALSE;
#endif
}

static BOOL wf_end_paint(rdpContext* context)
{
	int i;
	rdpGdi* gdi;
	int ninvalid;
	RECT updateRect;
	HGDI_RGN cinvalid;
	REGION16 invalidRegion;
	RECTANGLE_16 invalidRect;
	const RECTANGLE_16* extents;
	wfContext* wfc = (wfContext*)context;
	gdi = context->gdi;
	ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	if (ninvalid < 1)
		return TRUE;

	region16_init(&invalidRegion);

	for (i = 0; i < ninvalid; i++)
	{
		invalidRect.left = cinvalid[i].x;
		invalidRect.top = cinvalid[i].y;
		invalidRect.right = cinvalid[i].x + cinvalid[i].w;
		invalidRect.bottom = cinvalid[i].y + cinvalid[i].h;
		region16_union_rect(&invalidRegion, &invalidRegion, &invalidRect);
	}

	if (!region16_is_empty(&invalidRegion))
	{
		extents = region16_extents(&invalidRegion);
		updateRect.left = extents->left;
		updateRect.top = extents->top;
		updateRect.right = extents->right;
		updateRect.bottom = extents->bottom;
		InvalidateRect(wfc->hwnd, &updateRect, FALSE);

		if (wfc->rail)
			wf_rail_invalidate_region(wfc, &invalidRegion);
	}

	region16_uninit(&invalidRegion);
	return TRUE;
}

static BOOL wf_begin_paint(rdpContext* context)
{
	HGDI_DC hdc;

	if (!context || !context->gdi || !context->gdi->primary || !context->gdi->primary->hdc)
		return FALSE;

	hdc = context->gdi->primary->hdc;

	if (!hdc || !hdc->hwnd || !hdc->hwnd->invalid)
		return FALSE;

	hdc->hwnd->invalid->null = TRUE;
	hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL wf_desktop_resize(rdpContext* context)
{
	BOOL same;
	RECT rect;
	rdpSettings* settings;
	wfContext* wfc = (wfContext*)context;

	if (!context || !context->settings)
		return FALSE;

	settings = context->settings;

	if (wfc->primary)
	{
		same = (wfc->primary == wfc->drawing) ? TRUE : FALSE;
		wf_image_free(wfc->primary);
		wfc->primary = wf_image_new(wfc, settings->DesktopWidth, settings->DesktopHeight,
		                            context->gdi->dstFormat, NULL);
	}

	if (!gdi_resize_ex(context->gdi, settings->DesktopWidth, settings->DesktopHeight, 0,
	                   context->gdi->dstFormat, wfc->primary->pdata, NULL))
		return FALSE;

	if (same)
		wfc->drawing = wfc->primary;

	if (wfc->fullscreen != TRUE)
	{
		if (wfc->hwnd)
			SetWindowPos(wfc->hwnd, HWND_TOP, -1, -1, settings->DesktopWidth + wfc->diff.x,
			             settings->DesktopHeight + wfc->diff.y, SWP_NOMOVE);
	}
	else
	{
		wf_update_offset(wfc);
		GetWindowRect(wfc->hwnd, &rect);
		InvalidateRect(wfc->hwnd, &rect, TRUE);
	}

	return TRUE;
}

static BOOL wf_pre_connect(freerdp* instance)
{
	wfContext* wfc;
	int desktopWidth;
	int desktopHeight;
	rdpContext* context;
	rdpSettings* settings;

	if (!instance || !instance->context || !instance->settings)
		return FALSE;

	context = instance->context;
	wfc = (wfContext*)instance->context;
	settings = instance->settings;
	settings->OsMajorType = OSMAJORTYPE_WINDOWS;
	settings->OsMinorType = OSMINORTYPE_WINDOWS_NT;
	wfc->fullscreen = settings->Fullscreen;
	wfc->fullscreen_toggle = settings->ToggleFullscreen;
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
			desktopWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			desktopHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		}
		else
		{
			desktopWidth = GetSystemMetrics(SM_CXSCREEN);
			desktopHeight = GetSystemMetrics(SM_CYSCREEN);
		}
	}

	/* FIXME: desktopWidth has a limitation that it should be divisible by 4,
	 *        otherwise the screen will crash when connecting to an XP desktop.*/
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
		WLog_ERR(TAG, "invalid dimensions %lu %lu", settings->DesktopWidth,
		         settings->DesktopHeight);
		return FALSE;
	}

	if (!freerdp_client_load_addins(context->channels, instance->settings))
		return -1;

	freerdp_set_param_uint32(settings, FreeRDP_KeyboardLayout,
	                         (int)GetKeyboardLayout(0) & 0x0000FFFF);
	PubSub_SubscribeChannelConnected(instance->context->pubSub, wf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    wf_OnChannelDisconnectedEventHandler);
	return TRUE;
}

static void wf_add_system_menu(wfContext* wfc)
{
	HMENU hMenu;
	MENUITEMINFO item_info;

	if (wfc->fullscreen && !wfc->fullscreen_toggle)
	{
		return;
	}

	hMenu = GetSystemMenu(wfc->hwnd, FALSE);
	ZeroMemory(&item_info, sizeof(MENUITEMINFO));
	item_info.fMask = MIIM_CHECKMARKS | MIIM_FTYPE | MIIM_ID | MIIM_STRING | MIIM_DATA;
	item_info.cbSize = sizeof(MENUITEMINFO);
	item_info.wID = SYSCOMMAND_ID_SMARTSIZING;
	item_info.fType = MFT_STRING;
	item_info.dwTypeData = _wcsdup(_T("Smart sizing"));
	item_info.cch = (UINT)_wcslen(_T("Smart sizing"));
	item_info.dwItemData = (ULONG_PTR)wfc;
	InsertMenuItem(hMenu, 6, TRUE, &item_info);

	if (wfc->context.settings->SmartSizing)
	{
		CheckMenuItem(hMenu, SYSCOMMAND_ID_SMARTSIZING, MF_CHECKED);
	}
}

static WCHAR* wf_window_get_title(rdpSettings* settings)
{
	BOOL port;
	WCHAR* windowTitle = NULL;
	size_t size;
	char* name;
	WCHAR prefix[] = L"FreeRDP:";

	if (!settings)
		return NULL;

	name = settings->ServerHostname;

	if (settings->WindowTitle)
	{
		ConvertToUnicode(CP_UTF8, 0, settings->WindowTitle, -1, &windowTitle, 0);
		return windowTitle;
	}

	port = (settings->ServerPort != 3389);
	size = wcslen(name) + 16 + wcslen(prefix);
	windowTitle = calloc(size, sizeof(WCHAR));

	if (!windowTitle)
		return NULL;

	if (!port)
		_snwprintf_s(windowTitle, size, _TRUNCATE, L"%s %S", prefix, name);
	else
		_snwprintf_s(windowTitle, size, _TRUNCATE, L"%s %S:%u", prefix, name, settings->ServerPort);

	return windowTitle;
}

static BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	DWORD dwStyle;
	rdpCache* cache;
	wfContext* wfc;
	rdpContext* context;
	rdpSettings* settings;
	EmbedWindowEventArgs e;
	const UINT32 format = PIXEL_FORMAT_BGRX32;
	settings = instance->settings;
	context = instance->context;
	wfc = (wfContext*)instance->context;
	cache = instance->context->cache;
	wfc->primary = wf_image_new(wfc, settings->DesktopWidth, settings->DesktopHeight, format, NULL);

	if (!gdi_init_ex(instance, format, 0, wfc->primary->pdata, NULL))
		return FALSE;

	gdi = instance->context->gdi;

	if (!settings->SoftwareGdi)
	{
		wf_gdi_register_update_callbacks(instance->update);
	}

	wfc->window_title = wf_window_get_title(settings);

	if (!wfc->window_title)
		return FALSE;

	if (settings->EmbeddedWindow)
		settings->Decorations = FALSE;

	if (wfc->fullscreen)
		dwStyle = WS_POPUP;
	else if (!settings->Decorations)
		dwStyle = WS_CHILD | WS_BORDER;
	else
		dwStyle =
		    WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_MAXIMIZEBOX;

	if (!wfc->hwnd)
	{
		wfc->hwnd = CreateWindowEx((DWORD)NULL, wfc->wndClassName, wfc->window_title, dwStyle, 0, 0,
		                           0, 0, wfc->hWndParent, NULL, wfc->hInstance, NULL);
		SetWindowLongPtr(wfc->hwnd, GWLP_USERDATA, (LONG_PTR)wfc);
	}

	wf_resize_window(wfc);
	wf_add_system_menu(wfc);
	BitBlt(wfc->primary->hdc, 0, 0, settings->DesktopWidth, settings->DesktopHeight, NULL, 0, 0,
	       BLACKNESS);
	wfc->drawing = wfc->primary;
	EventArgsInit(&e, "wfreerdp");
	e.embed = FALSE;
	e.handle = (void*)wfc->hwnd;
	PubSub_OnEmbedWindow(context->pubSub, context, &e);
	ShowWindow(wfc->hwnd, SW_SHOWNORMAL);
	UpdateWindow(wfc->hwnd);
	instance->update->BeginPaint = wf_begin_paint;
	instance->update->DesktopResize = wf_desktop_resize;
	instance->update->EndPaint = wf_end_paint;
	wf_register_pointer(context->graphics);

	if (!settings->SoftwareGdi)
	{
		wf_register_graphics(context->graphics);
		wf_gdi_register_update_callbacks(instance->update);
		brush_cache_register_callbacks(instance->update);
		glyph_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
		palette_cache_register_callbacks(instance->update);
	}

	wfc->floatbar = wf_floatbar_new(wfc, wfc->hInstance, settings->Floatbar);
	return TRUE;
}

static void wf_post_disconnect(freerdp* instance)
{
	wfContext* wfc;

	if (!instance || !instance->context || !instance->settings)
		return;

	wfc = (wfContext*)instance->context;
	free(wfc->window_title);
}

static CREDUI_INFOA wfUiInfo = { sizeof(CREDUI_INFOA), NULL, "Enter your credentials",
	                             "Remote Desktop Security", NULL };

static BOOL wf_authenticate_raw(freerdp* instance, const char* title, char** username,
                                char** password, char** domain)
{
	wfContext* wfc;
	BOOL fSave;
	DWORD status;
	DWORD dwFlags;
	char UserName[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 };
	char Password[CREDUI_MAX_PASSWORD_LENGTH + 1] = { 0 };
	char User[CREDUI_MAX_USERNAME_LENGTH + 1] = { 0 };
	char Domain[CREDUI_MAX_DOMAIN_TARGET_LENGTH + 1] = { 0 };

	if (!instance || !instance->context)
		return FALSE;
	wfc = (wfContext*)instance->context;

	fSave = FALSE;
	dwFlags = CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_EXCLUDE_CERTIFICATES;

	if (wfc->isConsole)
		status = CredUICmdLinePromptForCredentialsA(
		    title, NULL, 0, UserName, CREDUI_MAX_USERNAME_LENGTH + 1, Password,
		    CREDUI_MAX_PASSWORD_LENGTH + 1, &fSave, dwFlags);
	else
		status = CredUIPromptForCredentialsA(&wfUiInfo, title, NULL, 0, UserName,
		                                     CREDUI_MAX_USERNAME_LENGTH + 1, Password,
		                                     CREDUI_MAX_PASSWORD_LENGTH + 1, &fSave, dwFlags);

	if (status != NO_ERROR)
	{
		WLog_ERR(TAG, "CredUIPromptForCredentials unexpected status: 0x%08lX", status);
		return FALSE;
	}

	status = CredUIParseUserNameA(UserName, User, sizeof(User), Domain, sizeof(Domain));
	// WLog_ERR(TAG, "User: %s Domain: %s Password: %s", User, Domain, Password);
	*username = _strdup(User);

	if (!(*username))
	{
		WLog_ERR(TAG, "strdup failed", status);
		return FALSE;
	}

	if (strlen(Domain) > 0)
		*domain = _strdup(Domain);
	else
		*domain = _strdup("\0");

	if (!(*domain))
	{
		free(*username);
		WLog_ERR(TAG, "strdup failed", status);
		return FALSE;
	}

	*password = _strdup(Password);

	if (!(*password))
	{
		free(*username);
		free(*domain);
		return FALSE;
	}

	return TRUE;
}

static BOOL wf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	return wf_authenticate_raw(instance, instance->settings->ServerHostname, username, password,
	                           domain);
}

static BOOL wf_gw_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	char tmp[MAX_PATH];
	sprintf_s(tmp, sizeof(tmp), "Gateway %s", instance->settings->GatewayHostname);
	return wf_authenticate_raw(instance, tmp, username, password, domain);
}

static WCHAR* wf_format_text(const WCHAR* fmt, ...)
{
	int rc;
	size_t size = 1024;
	WCHAR* buffer = calloc(size, sizeof(WCHAR));
	if (!buffer)
		return NULL;

	do
	{
		WCHAR* tmp;
		va_list ap;
		va_start(ap, fmt);
		rc = vswprintf_s(buffer, size, fmt, ap);
		va_end(ap);
		if (rc <= 0)
			goto fail;

		if ((size_t)rc < size)
			return buffer;

		size = (size_t)rc + 1;
		tmp = realloc(buffer, size * sizeof(WCHAR));
		if (!tmp)
			goto fail;

		buffer = tmp;
	} while (TRUE);

fail:
	free(buffer);
	return NULL;
}

static DWORD wf_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                      const char* common_name, const char* subject,
                                      const char* issuer, const char* fingerprint, DWORD flags)
{
	WCHAR* buffer;
	WCHAR* caption;
	int what = IDCANCEL;

	buffer = wf_format_text(
	    L"Certificate details:\n"
	    L"\tCommonName: %S\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S\n"
	    L"\tHostMismatch: %S\n"
	    L"\n"
	    L"The above X.509 certificate could not be verified, possibly because you do not have "
	    L"the CA certificate in your certificate store, or the certificate has expired. "
	    L"Please look at the OpenSSL documentation on how to add a private CA to the store.\n"
	    L"\n"
	    L"YES\tAccept permanently\n"
	    L"NO\tAccept for this session only\n"
	    L"CANCEL\tAbort connection\n",
	    common_name, subject, issuer, fingerprint,
	    flags & VERIFY_CERT_FLAG_MISMATCH ? "Yes" : "No");
	caption = wf_format_text(L"Verify certificate for %S:%hu", host, port);

	if (!buffer || !caption)
		goto fail;

	what = MessageBoxW(NULL, buffer, caption, MB_YESNOCANCEL);
fail:
	free(buffer);
	free(caption);

	/* return 1 to accept and store a certificate, 2 to accept
	 * a certificate only for this session, 0 otherwise */
	switch (what)
	{
		case IDYES:
			return 1;
		case IDNO:
			return 2;
		default:
			return 0;
	}
}

static DWORD wf_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                              const char* common_name, const char* subject,
                                              const char* issuer, const char* new_fingerprint,
                                              const char* old_subject, const char* old_issuer,
                                              const char* old_fingerprint, DWORD flags)
{
	WCHAR* buffer;
	WCHAR* caption;
	int what = IDCANCEL;

	buffer = wf_format_text(
	    L"New Certificate details:\n"
	    L"\tCommonName: %S\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S\n"
	    L"\tHostMismatch: %S\n"
	    L"\n"
	    L"Old Certificate details:\n"
	    L"\tSubject: %S\n"
	    L"\tIssuer: %S\n"
	    L"\tThumbprint: %S"
	    L"The above X.509 certificate could not be verified, possibly because you do not have "
	    L"the CA certificate in your certificate store, or the certificate has expired. "
	    L"Please look at the OpenSSL documentation on how to add a private CA to the store.\n"
	    L"\n"
	    L"YES\tAccept permanently\n"
	    L"NO\tAccept for this session only\n"
	    L"CANCEL\tAbort connection\n",
	    common_name, subject, issuer, new_fingerprint,
	    flags & VERIFY_CERT_FLAG_MISMATCH ? "Yes" : "No", old_subject, old_issuer, old_fingerprint);
	caption = wf_format_text(L"Verify certificate change for %S:%hu", host, port);

	if (!buffer || !caption)
		goto fail;

	what = MessageBoxW(NULL, buffer, caption, MB_YESNOCANCEL);
fail:
	free(buffer);
	free(caption);

	/* return 1 to accept and store a certificate, 2 to accept
	 * a certificate only for this session, 0 otherwise */
	switch (what)
	{
		case IDYES:
			return 1;
		case IDNO:
			return 2;
		default:
			return 0;
	}
}

static DWORD WINAPI wf_input_thread(LPVOID arg)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	freerdp* instance = (freerdp*)arg;
	assert(NULL != instance);
	status = 1;
	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);

	while (MessageQueue_Wait(queue))
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(instance, FREERDP_INPUT_MESSAGE_QUEUE,
			                                               &message);

			if (!status)
				break;
		}

		if (!status)
			break;
	}

	ExitThread(0);
	return 0;
}

static DWORD WINAPI wf_client_thread(LPVOID lpParam)
{
	MSG msg;
	int width;
	int height;
	BOOL msg_ret;
	int quit_msg;
	DWORD nCount;
	DWORD error;
	HANDLE handles[64];
	wfContext* wfc;
	freerdp* instance;
	rdpContext* context;
	rdpChannels* channels;
	rdpSettings* settings;
	BOOL async_input;
	HANDLE input_thread;
	instance = (freerdp*)lpParam;
	context = instance->context;
	wfc = (wfContext*)instance->context;

	if (!freerdp_connect(instance))
		goto end;

	channels = instance->context->channels;
	settings = instance->context->settings;
	async_input = settings->AsyncInput;

	if (async_input)
	{
		if (!(input_thread = CreateThread(NULL, 0, wf_input_thread, instance, 0, NULL)))
		{
			WLog_ERR(TAG, "Failed to create async input thread.");
			goto disconnect;
		}
	}

	while (1)
	{
		nCount = 0;

		if (freerdp_focus_required(instance))
		{
			wf_event_focus_in(wfc);
			wf_event_focus_in(wfc);
		}

		{
			DWORD tmp = freerdp_get_event_handles(context, &handles[nCount], 64 - nCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "freerdp_get_event_handles failed");
				break;
			}

			nCount += tmp;
		}

		if (MsgWaitForMultipleObjects(nCount, handles, FALSE, 1000, QS_ALLINPUT) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "wfreerdp_run: WaitForMultipleObjects failed: 0x%08lX", GetLastError());
			break;
		}

		{
			if (!freerdp_check_event_handles(context))
			{
				if (client_auto_reconnect(instance))
					continue;

				WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
				break;
			}
		}

		if (freerdp_shall_disconnect(instance))
			break;

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
	if (async_input)
	{
		wMessageQueue* input_queue;
		input_queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);

		if (MessageQueue_PostQuit(input_queue, 0))
			WaitForSingleObject(input_thread, INFINITE);
	}

disconnect:
	freerdp_disconnect(instance);

	if (async_input)
		CloseHandle(input_thread);

end:
	error = freerdp_get_last_error(instance->context);
	WLog_DBG(TAG, "Main thread exited with %" PRIu32, error);
	ExitThread(error);
	return error;
}

static DWORD WINAPI wf_keyboard_thread(LPVOID lpParam)
{
	MSG msg;
	BOOL status;
	wfContext* wfc;
	HHOOK hook_handle;
	wfc = (wfContext*)lpParam;
	assert(NULL != wfc);
	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, wfc->hInstance, 0);

	if (hook_handle)
	{
		while ((status = GetMessage(&msg, NULL, 0, 0)) != 0)
		{
			if (status == -1)
			{
				WLog_ERR(TAG, "keyboard thread error getting message");
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
		WLog_ERR(TAG, "failed to install keyboard hook");
	}

	WLog_DBG(TAG, "Keyboard thread exited.");
	ExitThread(0);
	return (DWORD)NULL;
}

static rdpSettings* freerdp_client_get_settings(wfContext* wfc)
{
	return wfc->context.settings;
}

static int freerdp_client_focus_in(wfContext* wfc)
{
	PostThreadMessage(wfc->mainThreadId, WM_SETFOCUS, 0, 1);
	return 0;
}

static int freerdp_client_focus_out(wfContext* wfc)
{
	PostThreadMessage(wfc->mainThreadId, WM_KILLFOCUS, 0, 1);
	return 0;
}

int freerdp_client_set_window_size(wfContext* wfc, int width, int height)
{
	WLog_DBG(TAG, "freerdp_client_set_window_size %d, %d", width, height);

	if ((width != wfc->client_width) || (height != wfc->client_height))
	{
		PostThreadMessage(wfc->mainThreadId, WM_SIZE, SIZE_RESTORED,
		                  ((UINT)height << 16) | (UINT)width);
	}

	return 0;
}

void wf_size_scrollbars(wfContext* wfc, UINT32 client_width, UINT32 client_height)
{
	if (wfc->disablewindowtracking)
		return;

	// prevent infinite message loop
	wfc->disablewindowtracking = TRUE;

	if (wfc->context.settings->SmartSizing)
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
		BOOL vert = wfc->yScrollVisible;

		if (!horiz && client_width < wfc->context.settings->DesktopWidth)
		{
			horiz = TRUE;
		}
		else if (horiz &&
		         client_width >=
		             wfc->context.settings->DesktopWidth /* - GetSystemMetrics(SM_CXVSCROLL)*/)
		{
			horiz = FALSE;
		}

		if (!vert && client_height < wfc->context.settings->DesktopHeight)
		{
			vert = TRUE;
		}
		else if (vert &&
		         client_height >=
		             wfc->context.settings->DesktopHeight /* - GetSystemMetrics(SM_CYHSCROLL)*/)
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
			wfc->xMaxScroll = MAX(wfc->context.settings->DesktopWidth - client_width, 0);
			wfc->xCurrentScroll = MIN(wfc->xCurrentScroll, wfc->xMaxScroll);
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
			si.nMin = wfc->xMinScroll;
			si.nMax = wfc->context.settings->DesktopWidth;
			si.nPage = client_width;
			si.nPos = wfc->xCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_HORZ, &si, TRUE);
		}

		if (vert)
		{
			// The vertical scrolling range is defined by
			// (bitmap_height) - (client_height). The current vertical
			// scroll value remains within the vertical scrolling range.
			wfc->yMaxScroll = MAX(wfc->context.settings->DesktopHeight - client_height, 0);
			wfc->yCurrentScroll = MIN(wfc->yCurrentScroll, wfc->yMaxScroll);
			si.cbSize = sizeof(si);
			si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
			si.nMin = wfc->yMinScroll;
			si.nMax = wfc->context.settings->DesktopHeight;
			si.nPage = client_height;
			si.nPos = wfc->yCurrentScroll;
			SetScrollInfo(wfc->hwnd, SB_VERT, &si, TRUE);
		}
	}

	wfc->disablewindowtracking = FALSE;
	wf_update_canvas_diff(wfc);
}

static BOOL wfreerdp_client_global_init(void)
{
	WSADATA wsaData;

	WSAStartup(0x101, &wsaData);

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);
	return TRUE;
}

static void wfreerdp_client_global_uninit(void)
{
	WSACleanup();
}

static BOOL wfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;
	if (!wfc)
		return FALSE;

	// AttachConsole and stdin do not work well.
	// Use GUI input dialogs instead of command line ones.
	wfc->isConsole = wf_create_console();

	if (!(wfreerdp_client_global_init()))
		return FALSE;

	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->PostDisconnect = wf_post_disconnect;
	instance->Authenticate = wf_authenticate;
	instance->GatewayAuthenticate = wf_gw_authenticate;
	if (wfc->isConsole)
	{
		instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
		instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	}
	else
	{
		instance->VerifyCertificateEx = wf_verify_certificate_ex;
		instance->VerifyChangedCertificateEx = wf_verify_changed_certificate_ex;
	}

	return TRUE;
}

static void wfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	if (!context)
		return;
}

static int wfreerdp_client_start(rdpContext* context)
{
	HWND hWndParent;
	HINSTANCE hInstance;
	wfContext* wfc = (wfContext*)context;
	freerdp* instance = context->instance;
	hInstance = GetModuleHandle(NULL);
	hWndParent = (HWND)instance->settings->ParentWindowId;
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
	wfc->wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wfc->wndClass.lpszMenuName = NULL;
	wfc->wndClass.lpszClassName = wfc->wndClassName;
	wfc->wndClass.hInstance = hInstance;
	wfc->wndClass.hIcon = wfc->icon;
	wfc->wndClass.hIconSm = wfc->icon;
	RegisterClassEx(&(wfc->wndClass));
	wfc->keyboardThread =
	    CreateThread(NULL, 0, wf_keyboard_thread, (void*)wfc, 0, &wfc->keyboardThreadId);

	if (!wfc->keyboardThread)
		return -1;

	wfc->thread = CreateThread(NULL, 0, wf_client_thread, (void*)instance, 0, &wfc->mainThreadId);

	if (!wfc->thread)
		return -1;

	return 0;
}

static int wfreerdp_client_stop(rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;

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
