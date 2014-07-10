/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Shadow Server
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

#include "shadow.h"

#include "X11/x11_shadow.h"

void* shadow_server_thread(void* param)
{
	ExitThread(0);

	return NULL;
}

int shadow_server_start(xfServer* server)
{
	server->thread = NULL;

	if (server->listener->Open(server->listener, NULL, 3389))
	{
		server->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				shadow_server_thread, (void*) server, 0, NULL);
	}

	return 0;
}

int shadow_server_stop(xfServer* server)
{
	if (server->thread)
	{
		TerminateThread(server->thread, 0);
		WaitForSingleObject(server->thread, INFINITE);
		CloseHandle(server->thread);

		server->listener->Close(server->listener);
	}

	return 0;
}

HANDLE shadow_server_get_thread(xfServer* server)
{
	return server->thread;
}

xfServer* shadow_server_new(int argc, char** argv)
{
	xfServer* server;

	server = (xfServer*) malloc(sizeof(xfServer));

	if (server)
	{
		server->listener = freerdp_listener_new();
		server->listener->PeerAccepted = NULL;
	}

	return server;
}

void shadow_server_free(xfServer* server)
{
	if (server)
	{
		freerdp_listener_free(server->listener);
		free(server);
	}
}

int main(int argc, char* argv[])
{
	HANDLE thread;
	xfServer* server;
	DWORD dwExitCode;

	server = x11_shadow_server_new(argc, argv);

	if (!server)
		return 0;

	x11_shadow_server_start(server);

	thread = x11_shadow_server_get_thread(server);

	WaitForSingleObject(thread, INFINITE);

	GetExitCodeThread(thread, &dwExitCode);

	x11_shadow_server_free(server);

	return 0;
}
