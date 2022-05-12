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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/streamdump.h>
#include <freerdp/freerdp.h>
#include <freerdp/client/cmdline.h>

#include "../xf_client.h"
#include "../xfreerdp.h"

int main(int argc, char* argv[])
{
	int rc = 1;
	int status;
	HANDLE thread;
	xfContext* xfc;
	DWORD dwExitCode;
	rdpContext* context;
	rdpSettings* settings;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = { 0 };

	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	context = freerdp_client_context_new(&clientEntryPoints);
	if (!context)
		return 1;

	settings = context->settings;
	xfc = (xfContext*)context;

	status = freerdp_client_settings_parse_command_line(context->settings, argc, argv, FALSE);
	if (status)
	{
		rc = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);

		if (settings->ListMonitors)
			xf_list_monitors(xfc);

		goto out;
	}

	if (!stream_dump_register_handlers(context, CONNECTION_STATE_MCS_CONNECT))
		goto out;

	if (freerdp_client_start(context) != 0)
		goto out;

	thread = freerdp_client_get_thread(context);

	WaitForSingleObject(thread, INFINITE);
	GetExitCodeThread(thread, &dwExitCode);
	rc = xf_exit_code_from_disconnect_reason(dwExitCode);

	freerdp_client_stop(context);

out:
	freerdp_client_context_free(context);

	return rc;
}
