/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/wnd.h>
#include <winpr/path.h>
#include <winpr/cmdline.h>
#include <winpr/winsock.h>

#include <winpr/tools/makecert.h>

#ifdef _WIN32
static BOOL g_MessagePump = TRUE;
#else
static BOOL g_MessagePump = FALSE;
#endif

#include <freerdp/server/shadow.h>

int main(int argc, char** argv)
{
	MSG msg;
	int status = 0;
	DWORD dwExitCode;
	rdpShadowServer* server;

	shadow_subsystem_set_entry_builtin(NULL);

	server = shadow_server_new();

	if (!server)
	{
		status = -1;
		goto fail_server_new;
	}

#ifdef WITH_SHADOW_X11
	server->authentication = TRUE;
#else
	server->authentication = FALSE;
#endif

	if ((status = shadow_server_parse_command_line(server, argc, argv)) < 0)
	{
		shadow_server_command_line_status_print(server, argc, argv, status);
		goto fail_parse_command_line;
	}

	if ((status = shadow_server_init(server)) < 0)
		goto fail_server_init;

	if ((status = shadow_server_start(server)) < 0)
		goto fail_server_start;

	if (g_MessagePump)
	{
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WaitForSingleObject(server->thread, INFINITE);

	if (!GetExitCodeThread(server->thread, &dwExitCode))
		status = -1;
	else
		status = (int)dwExitCode;


fail_server_start:
	shadow_server_uninit(server);
fail_server_init:
fail_parse_command_line:
	shadow_server_free(server);
fail_server_new:
	return status;
}

