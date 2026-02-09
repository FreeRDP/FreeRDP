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

#include <freerdp/config.h>

#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/library.h>
#include <winpr/winsock.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/settings.h>
#include <freerdp/gdi/gdi.h>

#include <freerdp/locale/locale.h>
#include <freerdp/locale/keyboard.h>

#include "wf_gdi.h"
#include "wf_event.h"
#include "wf_channels.h"
#include "wf_graphics.h"
#include "wf_cliprdr.h"
#include "wf_rail.h"
#include "wf_disp.h"

#include "wf_client.h"

#include <winpr/print.h>

#include <windowsx.h>

#define TAG CLIENT_TAG("windows")

static BOOL wf_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	wfContext* wfc = (wfContext*)instance->context;

	WINPR_ASSERT(wfc);

	settings = instance->settings;
	WINPR_ASSERT(settings);

	wfc->instance = instance;
	wfc->settings = instance->settings;

	if (!freerdp_settings_get_bool(settings, FreeRDP_AuthenticationOnly))
	{
		wfc->fullscreen = freerdp_settings_get_bool(settings, FreeRDP_Fullscreen);
		wfc->percentscreen = freerdp_settings_get_uint32(settings, FreeRDP_PercentScreen);
	}

	if (!wfc->isConsole)
	{
		wfc->window_title = wf_window_get_title(settings);
		if (!wfc->window_title)
			return FALSE;
	}

	wfc->mainThreadId = GetCurrentThreadId();

	if (!wf_hwnd_register_class(wfc))
		return FALSE;

	PubSub_SubscribeChannelConnected(instance->context->pubSub, wf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    wf_OnChannelDisconnectedEventHandler);

	return TRUE;
}

static BOOL wf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	UINT32 flags;
	wfContext* wfc = (wfContext*)instance->context;

	WINPR_ASSERT(wfc);

	flags = CLRCONV_ALPHA | CLRCONV_INVERT;

	if (freerdp_settings_get_uint32(wfc->common.context.settings, FreeRDP_ColorDepth) <= 16)
		flags |= CLRCONV_16BPP;

	if (!gdi_init(instance, flags))
		return FALSE;

	gdi = instance->context->gdi;
	WINPR_ASSERT(gdi);

	if (!wfc->isConsole)
	{
		if (!wf_create_main_window(wfc))
			return FALSE;
	}

	wfc->keyboardThread =
	    CreateThread(NULL, 0, wf_keyboard_thread, (void*)wfc, 0, &wfc->keyboardThreadId);

	if (!wfc->keyboardThread)
		return FALSE;

	return TRUE;
}

static void wf_post_disconnect(freerdp* instance)
{
	wfContext* wfc;

	if (!instance || !instance->context)
		return;

	wfc = (wfContext*)instance->context;
	free(wfc->window_title);
	wfc->window_title = NULL;

	/* Unsubscribe channel handlers registered in pre_connect */
	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   wf_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      wf_OnChannelDisconnectedEventHandler);

	/* Free GDI resources and our backing surface */
	if (instance->context->gdi)
		gdi_free(instance);

	if (wfc->primary)
	{
		wf_image_free(wfc->primary);
		wfc->primary = NULL;
		wfc->drawing = NULL;
	}
}

static CREDUI_INFOW wfUiInfo = { sizeof(CREDUI_INFOW), NULL, L"Enter your credentials",
	                             L"Remote Desktop Security", NULL };

static BOOL wf_authenticate_ex(freerdp* instance, char** username, char** password, char** domain,
                               rdp_auth_reason reason)
{
	wfContext* wfc;
	BOOL fSave;
	DWORD status;
	WCHAR* buffer = NULL;
	WCHAR* caption = NULL;
	CREDUI_INFOW uiInfo;
	const char* host = NULL;
	UINT16 port = 0;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(username);
	WINPR_ASSERT(password);
	WINPR_ASSERT(domain);

	wfc = (wfContext*)instance->context;
	WINPR_ASSERT(wfc);

	host = freerdp_settings_get_string(instance->settings, FreeRDP_ServerHostname);
	port = (UINT16)freerdp_settings_get_uint32(instance->settings, FreeRDP_ServerPort);

	uiInfo = wfUiInfo;
	uiInfo.hwndParent = wfc->hwnd;

	caption = wf_format_text(L"Verify certificate for %S:%hu", host, port);
	uiInfo.pszCaptionText = caption;

	if (*username)
	{
		size_t size = 0;
		WCHAR* wuser = ConvertUtf8ToWCharAlloc(*username, &size);
		if (wuser)
		{
			(void)StringCchCopyW(wfc->username, ARRAYSIZE(wfc->username), wuser);
			free(wuser);
		}
	}

	if (*domain)
	{
		size_t size = 0;
		WCHAR* wdomain = ConvertUtf8ToWCharAlloc(*domain, &size);
		if (wdomain)
		{
			(void)StringCchCopyW(wfc->domain, ARRAYSIZE(wfc->domain), wdomain);
			free(wdomain);
		}
	}

	status = CredUIPromptForWindowsCredentialsW(&uiInfo, 0, &wfc->auth_flags, NULL, 0, &buffer,
	                                            &wfc->auth_buffer_len, &fSave, 0);

	free(caption);

	if (status != ERROR_SUCCESS)
	{
		return (status == ERROR_CANCELLED) ? FALSE : FALSE;
	}

	CredUnPackAuthenticationBufferW(0, buffer, wfc->auth_buffer_len, wfc->username,
	                                &wfc->username_len, wfc->domain, &wfc->domain_len,
	                                wfc->password, &wfc->password_len);

	SecureZeroMemory(buffer, wfc->auth_buffer_len);
	CoTaskMemFree(buffer);

	*username = ConvertWCharToUtf8Alloc(wfc->username, NULL);
	*password = ConvertWCharToUtf8Alloc(wfc->password, NULL);
	*domain = ConvertWCharToUtf8Alloc(wfc->domain, NULL);

	return TRUE;
}

static DWORD wf_verify_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                      const char* common_name, const char* fingerprint,
                                      const char* issuer_common_name, const char* full_details,
                                      UINT32 flags)
{
	int result;
	WCHAR* buffer = NULL;
	WCHAR* caption = NULL;
	wfContext* wfc = (wfContext*)instance->context;

	WINPR_ASSERT(wfc);

	buffer = wf_format_text(
	    L"The server could not be authenticated. Do you want to connect anyway?\n\n"
	    L"Server: %S:%hu\n"
	    L"Common Name: %S\n"
	    L"Fingerprint: %S\n"
	    L"Issuer: %S\n\n"
	    L"Details:\n%S",
	    host, port, common_name, fingerprint, issuer_common_name, full_details);

	if (!buffer)
		return 0;

	caption = wf_format_text(L"Verify certificate for %S:%hu", host, port);

	result = MessageBoxW(wfc->hwnd, buffer, caption, MB_ICONWARNING | MB_YESNO);

	free(buffer);
	free(caption);

	return (result == IDYES) ? 1 : 0;
}

static DWORD wf_verify_changed_certificate_ex(freerdp* instance, const char* host, UINT16 port,
                                              const char* common_name, const char* fingerprint,
                                              const char* old_common_name,
                                              const char* old_fingerprint,
                                              const char* issuer_common_name,
                                              const char* old_issuer_common_name,
                                              const char* full_details, UINT32 flags)
{
	int result;
	WCHAR* buffer = NULL;
	WCHAR* caption = NULL;
	wfContext* wfc = (wfContext*)instance->context;

	WINPR_ASSERT(wfc);

	buffer = wf_format_text(
	    L"The server certificate has changed. Do you want to connect anyway?\n\n"
	    L"Server: %S:%hu\n"
	    L"New Common Name: %S\n"
	    L"New Fingerprint: %S\n"
	    L"New Issuer: %S\n\n"
	    L"Old Common Name: %S\n"
	    L"Old Fingerprint: %S\n"
	    L"Old Issuer: %S\n\n"
	    L"Details:\n%S",
	    host, port, common_name, fingerprint, issuer_common_name, old_common_name, old_fingerprint,
	    old_issuer_common_name, full_details);

	if (!buffer)
		return 0;

	caption = wf_format_text(L"Verify certificate change for %S:%hu", host, port);

	result = MessageBoxW(wfc->hwnd, buffer, caption, MB_ICONWARNING | MB_YESNO);

	free(buffer);
	free(caption);

	return (result == IDYES) ? 1 : 0;
}

static BOOL wf_present_gateway_message(freerdp* instance, const char* message)
{
	wfContext* wfc = (wfContext*)instance->context;
	WCHAR* wmsg = NULL;
	int result;

	WINPR_ASSERT(wfc);

	wmsg = ConvertUtf8ToWCharAlloc(message, NULL);
	if (!wmsg)
		return FALSE;

	result = MessageBoxW(wfc->hwnd, wmsg, L"Remote Desktop Gateway Message", MB_ICONINFORMATION | MB_OK);
	free(wmsg);

	return (result == IDOK);
}

static DWORD WINAPI wf_keyboard_thread(LPVOID lpParam)
{
	MSG msg;
	wfContext* wfc;
	HHOOK hook_handle;
	wfc = (wfContext*)lpParam;
	WINPR_ASSERT(NULL != wfc);
	hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, wf_ll_kbd_proc, wfc->hInstance, 0);

	if (hook_handle)
	{
		while (TRUE)
		{
			DWORD result = MsgWaitForMultipleObjects(1, &wfc->stopEvent, FALSE, INFINITE, QS_ALLINPUT);

			if (result == WAIT_OBJECT_0)
			{
				/* stopEvent signaled, exit loop immediately */
				break;
			}
			else if (result == WAIT_OBJECT_0 + 1)
			{
				/* Process pending messages */
				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (msg.message == WM_QUIT)
						goto loop_exit;

					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			else
			{
				WLog_ERR(TAG, "keyboard thread wait error: 0x%08lX", result);
				break;
			}
		}

	loop_exit:
		UnhookWindowsHookEx(hook_handle);
	}
	else
	{
		WLog_ERR(TAG, "failed to install keyboard hook");
	}

	WLog_DBG(TAG, "Keyboard thread exited.");
	ExitThread(0);
	return 0;
}

static BOOL wfreerdp_client_global_init(void)
{
	WSADATA wsaData;

	WSAStartup(0x101, &wsaData);

	freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

	wf_event_init();

	return TRUE;
}

static void wfreerdp_client_global_uninit(void)
{
	wf_event_uninit();
	WSACleanup();
}

static BOOL wfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;
	if (!wfc)
		return FALSE;

	wfc->isConsole = wf_has_console();

	wfc->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!wfc->stopEvent)
		return FALSE;

	if (!(wfreerdp_client_global_init()))
		return FALSE;

	WINPR_ASSERT(instance);
	instance->PreConnect = wf_pre_connect;
	instance->PostConnect = wf_post_connect;
	instance->PostDisconnect = wf_post_disconnect;
	instance->AuthenticateEx = wf_authenticate_ex;

#ifdef WITH_WINDOWS_CERT_STORE
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_CertificateCallbackPreferPEM, TRUE))
		return FALSE;
#endif

	if (!wfc->isConsole)
	{
		instance->VerifyCertificateEx = wf_verify_certificate_ex;
		instance->VerifyChangedCertificateEx = wf_verify_changed_certificate_ex;
		instance->PresentGatewayMessage = wf_present_gateway_message;
	}

#ifdef WITH_PROGRESS_BAR
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_ALL, &IID_ITaskbarList3,
	                 (void**)&wfc->taskBarList);
#endif

	return TRUE;
}

static void wfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;
	WINPR_UNUSED(instance);
	if (!wfc)
		return;

#ifdef WITH_PROGRESS_BAR
	if (wfc->taskBarList)
	{
		wfc->taskBarList->lpVtbl->Release(wfc->taskBarList);
		wfc->taskBarList = NULL;
	}
	CoUninitialize();
#endif

	if (wfc->stopEvent)
	{
		CloseHandle(wfc->stopEvent);
		wfc->stopEvent = NULL;
	}

	if (wfc->wndClassName)
	{
		UnregisterClass(wfc->wndClassName, wfc->hInstance);
		free((void*)wfc->wndClassName);
		wfc->wndClassName = NULL;
	}
}

static int wfreerdp_client_start(rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);

	freerdp* instance = context->instance;
	WINPR_ASSERT(instance);

	wfc->mainThreadId = GetCurrentThreadId();

	return freerdp_client_common_start(context);
}

static int wfreerdp_client_stop(rdpContext* context)
{
	wfContext* wfc = (wfContext*)context;

	WINPR_ASSERT(wfc);

	if (wfc->keyboardThread)
	{
		/* 1. Signal deterministic exit event */
		if (wfc->stopEvent)
			SetEvent(wfc->stopEvent);

		/* 2. Send standard quit message as backup */
		PostThreadMessage(wfc->keyboardThreadId, WM_QUIT, 0, 0);

		/* 3. Wait indefinitely since exit is now guaranteed by stopEvent */
		(void)WaitForSingleObject(wfc->keyboardThread, INFINITE);

		(void)CloseHandle(wfc->keyboardThread);
		wfc->keyboardThread = NULL;
		wfc->keyboardThreadId = 0;
	}

	PostThreadMessage(wfc->mainThreadId, WM_QUIT, 0, 0);
	return freerdp_client_common_stop(context);
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