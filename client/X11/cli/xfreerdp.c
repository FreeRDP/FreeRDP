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
#include <freerdp/client/cmdline.h>

#include "xf_client.h"
#include "xfreerdp.h"

int main(int argc, char* argv[])
{
	int status;
	HANDLE thread;
	xfContext* xfc;
	DWORD dwExitCode;
	rdpContext* context;
	rdpSettings* settings;
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	context = freerdp_client_context_new(&clientEntryPoints);

	settings = context->settings;
	xfc = (xfContext*) context;

	status = freerdp_client_settings_parse_command_line(context->settings, argc, argv);

	status = freerdp_client_settings_command_line_status_print(settings, status, argc, argv);

	if (status)
	{
		if (settings->ListMonitors)
			xf_list_monitors(xfc);

		freerdp_client_context_free(context);
		return 0;
	}

	/* Check, if we want to read missing arguments from command line. */
	if (settings->CredentialsFromStdin)
	{
		if (!xf_authenticate_(settings, &settings->Username,
				&settings->Password, &settings->Domain))
		{
			DEBUG_WARN("[context->Authenticate] failed.");
			status = -1;
		}

		if (settings->GatewayUseSameCredentials)
		{
			if (!settings->GatewayUsername)
				settings->GatewayUsername = _strdup(settings->Username);
			if (!settings->GatewayDomain)
				settings->GatewayDomain = _strdup(settings->Domain);
			if (!settings->GatewayPassword)
				settings->GatewayPassword = _strdup(settings->Password);
		}
		else if (settings->GatewayEnabled)
		{
			printf("Gateway settings\n");
			if (!xf_authenticate_(settings, &settings->GatewayUsername,
					&settings->GatewayPassword, &settings->GatewayDomain))
			{
				DEBUG_WARN("[context->Authenticate] failed.");
				status = -1;
			}
		}

		/* Read the host, if requested. */
		if (!settings->ServerHostname)
		{
			size_t len = 0;
			char *p = NULL;
			char *in = NULL;

			printf("Hostname: ");
			getline(&in, &len, stdin);

			if (in)
			{
				p = strchr(in, '\r');
				if (p)
					*p = '\0';
				p = strchr(in, '\n');
				if (p)
					*p = '\0';

				p = strchr(in, ':');
				if (p)
				{
					size_t length = p - in;
					settings->ServerPort = atoi(&p[1]);
					settings->ServerHostname = (char*) malloc(length + 1);
					strncpy(settings->ServerHostname, in, length);
					settings->ServerHostname[length] = '\0';
				}
				else
				{
					settings->ServerHostname = _strdup(in);
				}

				free(in);
			}
		}

		/* Read the gateway hostname, if requested. */
		if (!settings->GatewayHostname && settings->GatewayEnabled)
		{
			size_t len = 0;
			char *p = NULL;
			char *in = NULL;

			printf("Gateway: ");
			getline(&in, &len, stdin);
			if (in)
			{
				p = strchr(in, '\r');
				if (p)
					*p = '\0';
				p = strchr(in, '\n');
				if (p)
					*p = '\0';

				p = strchr(in, ':');
				if (p)
				{
					size_t length = p - in;
					settings->GatewayPort = atoi(&p[1]);
					settings->GatewayHostname = (char*) malloc(length + 1);
					strncpy(settings->GatewayHostname, in, length);
					settings->GatewayHostname[length] = '\0';
				}
				else
				{
					settings->GatewayHostname = _strdup(in);
				}

				free(in);
			}
		}
	}

	if (!status)
	{
		freerdp_client_start(context);

		thread = freerdp_client_get_thread(context);

		WaitForSingleObject(thread, INFINITE);

		GetExitCodeThread(thread, &dwExitCode);

		freerdp_client_stop(context);

		status = xf_exit_code_from_disconnect_reason(dwExitCode);
	}

	freerdp_client_context_free(context);

	return status;
}
