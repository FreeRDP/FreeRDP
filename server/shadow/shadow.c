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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/path.h>
#include <winpr/cmdline.h>
#include <winpr/winsock.h>

#include <winpr/tools/makecert.h>

#include <freerdp/server/shadow.h>

#include <freerdp/log.h>
#define TAG SERVER_TAG("shadow")

int main(int argc, char** argv)
{
	int status = 0;
	DWORD dwExitCode;
	rdpSettings* settings;
	rdpShadowServer* server;
	COMMAND_LINE_ARGUMENT_A shadow_args[] = {
		{ "log-filters", COMMAND_LINE_VALUE_REQUIRED, "<tag>:<level>[,<tag>:<level>[,...]]", NULL,
		  NULL, -1, NULL, "Set logger filters, see wLog(7) for details" },
		{ "log-level", COMMAND_LINE_VALUE_REQUIRED, "[OFF|FATAL|ERROR|WARN|INFO|DEBUG|TRACE]", NULL,
		  NULL, -1, NULL, "Set the default log level, see wLog(7) for details" },
		{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Server port" },
		{ "ipc-socket", COMMAND_LINE_VALUE_REQUIRED, "<ipc-socket>", NULL, NULL, -1, NULL,
		  "Server IPC socket" },
		{ "bind-address", COMMAND_LINE_VALUE_REQUIRED, "<bind-address>[,<another address>, ...]",
		  NULL, NULL, -1, NULL,
		  "An address to bind to. Use '[<ipv6>]' for IPv6 addresses, e.g. '[::1]' for "
		  "localhost" },
		{ "monitors", COMMAND_LINE_VALUE_OPTIONAL, "<0,1,2...>", NULL, NULL, -1, NULL,
		  "Select or list monitors" },
		{ "rect", COMMAND_LINE_VALUE_REQUIRED, "<x,y,w,h>", NULL, NULL, -1, NULL,
		  "Select rectangle within monitor to share" },
		{ "auth", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "Clients must authenticate" },
		{ "may-view", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Clients may view without prompt" },
		{ "may-interact", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Clients may interact without prompt" },
		{ "sec", COMMAND_LINE_VALUE_REQUIRED, "<rdp|tls|nla|ext>", NULL, NULL, -1, NULL,
		  "force specific protocol security" },
		{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "rdp protocol security" },
		{ "sec-tls", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "tls protocol security" },
		{ "sec-nla", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "nla protocol security" },
		{ "sec-ext", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueFalse, NULL, -1, NULL,
		  "nla extended protocol security" },
		{ "sam-file", COMMAND_LINE_VALUE_REQUIRED, "<file>", NULL, NULL, -1, NULL,
		  "NTLM SAM file for NLA authentication" },
		{ "keytab", COMMAND_LINE_VALUE_REQUIRED, "<file>", NULL, NULL, -1, NULL,
		  "Kerberos keytab file for NLA authentication" },
		{ "gfx-progressive", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Allow GFX progressive codec" },
		{ "gfx-rfx", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Allow GFX RFX codec" },
		{ "gfx-planar", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Allow GFX planar codec" },
		{ "gfx-avc420", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Allow GFX AVC420 codec" },
		{ "gfx-avc444", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL,
		  "Allow GFX AVC444 codec" },
		{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1,
		  NULL, "Print version" },
		{ "buildconfig", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_BUILDCONFIG, NULL, NULL, NULL,
		  -1, NULL, "Print the build configuration" },
		{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?",
		  "Print help" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	shadow_subsystem_set_entry_builtin(NULL);

	server = shadow_server_new();

	if (!server)
	{
		status = -1;
		WLog_ERR(TAG, "Server new failed");
		goto fail_server_new;
	}

	settings = server->settings;

	settings->NlaSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->RdpSecurity = TRUE;

	/* By default allow all GFX modes.
	 * This can be changed with command line flags [+|-]gfx-CODEC
	 */
	freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32);
	freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, TRUE);
	freerdp_settings_set_bool(settings, FreeRDP_GfxProgressiveV2, TRUE);

#ifdef WITH_SHADOW_X11
	server->authentication = TRUE;
#else
	server->authentication = FALSE;
#endif

	if ((status = shadow_server_parse_command_line(server, argc, argv, shadow_args)) < 0)
	{
		shadow_server_command_line_status_print(server, argc, argv, status, shadow_args);
		goto fail_parse_command_line;
	}

	if ((status = shadow_server_init(server)) < 0)
	{
		WLog_ERR(TAG, "Server initialization failed.");
		goto fail_server_init;
	}

	if ((status = shadow_server_start(server)) < 0)
	{
		WLog_ERR(TAG, "Failed to start server.");
		goto fail_server_start;
	}

#ifdef _WIN32
	{
		MSG msg = { 0 };
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#endif

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
