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

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/svc_plugin.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>

#include "wf_interface.h"

HANDLE g_done_event;
HINSTANCE g_hInstance;
HCURSOR g_default_cursor;
int g_thread_count = 0;
LPCTSTR g_wnd_class_name = L"wfreerdp";

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	freerdp* instance;
	WSADATA wsa_data;
	WNDCLASSEX wnd_cls;

	if (!getenv("HOME"))
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
	wnd_cls.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
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

		freerdp_register_addin_provider(freerdp_channels_load_static_addin_entry, 0);

		status = freerdp_client_parse_command_line_arguments(__argc, __argv, instance->settings);

		freerdp_client_load_addins(instance->context->channels, instance->settings);

		if (status < 0)
		{
			printf("failed to parse arguments.\n");
#ifdef _DEBUG
			system("pause");
#endif
			exit(-1);
		}

		if (CreateThread(NULL, 0, thread_func, instance, 0, NULL) != 0)
			g_thread_count++;
	}

	if (g_thread_count > 0)
	{
		WaitForSingleObject(g_done_event, INFINITE);
	}
	else
	{
		MessageBox(GetConsoleWindow(),
			L"Failed to start wfreerdp.\n\nPlease check the debug output.",
			L"FreeRDP Error", MB_ICONSTOP);
	}

	freerdp_context_free(instance);
	freerdp_free(instance);

	WSACleanup();

#ifdef _DEBUG
	system("pause");
#endif

	return 0;
}
