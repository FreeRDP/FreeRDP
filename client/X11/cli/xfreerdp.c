/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 HP Development Company, LLC
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
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>

#include "xf_interface.h"

#include "xfreerdp.h"

int main(int argc, char* argv[])
{
	xfContext* xfc;
	DWORD dwExitCode;
	freerdp* instance;

	freerdp_client_global_init();

	xfc = freerdp_client_new(argc, argv);

	if (xfc == NULL)
	{
		return 1;
	}

	instance = xfc->instance;

	freerdp_client_start(xfc);

	WaitForSingleObject(xfc->thread, INFINITE);

	GetExitCodeThread(xfc->thread, &dwExitCode);

	freerdp_client_free(xfc);

	freerdp_client_global_uninit();

	return xf_exit_code_from_disconnect_reason(dwExitCode);
}
