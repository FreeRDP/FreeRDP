/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP X11 Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "xf_interface.h"

int main(int argc, char* argv[])
{
	HANDLE thread;
	xfServer* server;
	DWORD dwExitCode;

	freerdp_server_global_init();

	server = freerdp_server_new(argc, argv);

	if (!server)
		return 0;

	freerdp_server_start(server);

	thread = freerdp_server_get_thread(server);

	WaitForSingleObject(thread, INFINITE);

	GetExitCodeThread(thread, &dwExitCode);

	freerdp_server_free(server);

	freerdp_server_global_uninit();

	return 0;
}
