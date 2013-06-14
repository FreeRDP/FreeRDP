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
	rdpContext* context;
	rdpSettings* settings;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	clientEntryPoints.GlobalInit();

	instance = freerdp_new();
	instance->ContextSize = clientEntryPoints.ContextSize;
	instance->ContextNew = clientEntryPoints.ClientNew;
	instance->ContextFree = clientEntryPoints.ClientFree;
	freerdp_context_new(instance);

	context = instance->context;
	settings = context->settings;
	xfc = (xfContext*) context;

	if (freerdp_client_parse_command_line(context, argc, argv) < 0)
	{
		if (settings->ConnectionFile)
		{
			freerdp_client_parse_connection_file(context, settings->ConnectionFile);
		}
		else
		{
			if (settings->ListMonitors)
				xf_list_monitors(xfc);

			freerdp_context_free(instance);
			freerdp_free(instance);
			return 0;
		}
	}

	clientEntryPoints.ClientStart(context);

	WaitForSingleObject(xfc->thread, INFINITE);

	GetExitCodeThread(xfc->thread, &dwExitCode);

	clientEntryPoints.ClientStop(context);

	clientEntryPoints.GlobalUninit();

	return xf_exit_code_from_disconnect_reason(dwExitCode);
}
