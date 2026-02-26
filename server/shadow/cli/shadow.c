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
#include <freerdp/settings.h>

#include <freerdp/log.h>
#define TAG SERVER_TAG("shadow")

int main(int argc, char** argv)
{
	int status = 0;
	DWORD dwExitCode = 0;
	COMMAND_LINE_ARGUMENT_A shadow_args[] = {
		{ "log-filters", COMMAND_LINE_VALUE_REQUIRED, "<tag>:<level>[,<tag>:<level>[,...]]",
		  nullptr, nullptr, -1, nullptr, "Set logger filters, see wLog(7) for details" },
		{ "log-level", COMMAND_LINE_VALUE_REQUIRED, "[OFF|FATAL|ERROR|WARN|INFO|DEBUG|TRACE]",
		  nullptr, nullptr, -1, nullptr, "Set the default log level, see wLog(7) for details" },
		{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", nullptr, nullptr, -1, nullptr,
		  "Server port" },
		{ "ipc-socket", COMMAND_LINE_VALUE_REQUIRED, "<ipc-socket>", nullptr, nullptr, -1, nullptr,
		  "Server IPC socket" },
		{ "bind-address", COMMAND_LINE_VALUE_REQUIRED, "<bind-address>[,<another address>, ...]",
		  nullptr, nullptr, -1, nullptr,
		  "An address to bind to. Use '[<ipv6>]' for IPv6 addresses, e.g. '[::1]' for "
		  "localhost" },
		{ "server-side-cursor", COMMAND_LINE_VALUE_BOOL, nullptr, nullptr, nullptr, -1, nullptr,
		  "hide mouse cursor in RDP client." },
		{ "monitors", COMMAND_LINE_VALUE_OPTIONAL, "<0,1,2...>", nullptr, nullptr, -1, nullptr,
		  "Select or list monitors" },
		{ "max-connections", COMMAND_LINE_VALUE_REQUIRED, "<number>", nullptr, nullptr, -1, nullptr,
		  "maximum connections allowed to server, 0 to deactivate" },
		{ "mouse-relative", COMMAND_LINE_VALUE_BOOL, nullptr, nullptr, nullptr, -1, nullptr,
		  "enable support for relative mouse events" },
		{ "rect", COMMAND_LINE_VALUE_REQUIRED, "<x,y,w,h>", nullptr, nullptr, -1, nullptr,
		  "Select rectangle within monitor to share" },
		{ "auth", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Clients must authenticate" },
		{ "remote-guard", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "Remote credential guard" },
		{ "restricted-admin", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Restricted Admin" },
		{ "vmconnect", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse,
		  nullptr, -1, nullptr, "Hyper-V console server (bind on vsock://1)" },
		{ "may-view", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Clients may view without prompt" },
		{ "may-interact", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Clients may interact without prompt" },
		{ "sec", COMMAND_LINE_VALUE_REQUIRED, "<rdp|tls|nla|ext>", nullptr, nullptr, -1, nullptr,
		  "force specific protocol security" },
		{ "sec-rdp", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "rdp protocol security" },
		{ "sec-tls", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "tls protocol security" },
		{ "sec-nla", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "nla protocol security" },
		{ "sec-ext", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "nla extended protocol security" },
		{ "sam-file", COMMAND_LINE_VALUE_REQUIRED, "<file>", nullptr, nullptr, -1, nullptr,
		  "NTLM SAM file for NLA authentication" },
		{ "keytab", COMMAND_LINE_VALUE_REQUIRED, "<file>", nullptr, nullptr, -1, nullptr,
		  "Kerberos keytab file for NLA authentication" },
		{ "ccache", COMMAND_LINE_VALUE_REQUIRED, "<file>", nullptr, nullptr, -1, nullptr,
		  "Kerberos host ccache file for NLA authentication" },
		{ "tls-secrets-file", COMMAND_LINE_VALUE_REQUIRED, "<file>", nullptr, nullptr, -1, nullptr,
		  "file where tls secrets shall be stored" },
		{ "nsc", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow NSC codec" },
		{ "rfx", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow RFX surface bits" },
		{ "gfx", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX pipeline" },
		{ "gfx-progressive", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX progressive codec" },
		{ "gfx-rfx", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX RFX codec" },
		{ "gfx-planar", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX planar codec" },
		{ "gfx-avc420", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX AVC420 codec" },
		{ "gfx-avc444", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueTrue, nullptr, -1, nullptr,
		  "Allow GFX AVC444 codec" },
		{ "bitmap-compat", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
		  "Limit BitmapUpdate to 1 rectangle (fixes broken windows 11 24H2 clients)" },
		{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, nullptr, nullptr,
		  nullptr, -1, nullptr, "Print version" },
		{ "buildconfig", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_BUILDCONFIG, nullptr, nullptr,
		  nullptr, -1, nullptr, "Print the build configuration" },
		{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, nullptr, nullptr, nullptr, -1,
		  "?", "Print help" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
	};

	shadow_subsystem_set_entry_builtin(nullptr);

	rdpShadowServer* server = shadow_server_new();

	if (!server)
	{
		status = -1;
		WLog_ERR(TAG, "Server new failed");
		goto fail;
	}

	{
		rdpSettings* settings = server->settings;
		WINPR_ASSERT(settings);

		if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE))
			goto fail;

		/* By default allow all GFX modes.
		 * This can be changed with command line flags [+|-]gfx-CODEC
		 */
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_RemoteFxImageCodec, TRUE) ||
		    !freerdp_settings_set_uint32(settings, FreeRDP_RemoteFxRlgrMode, RLGR3) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GfxH264, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive, TRUE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_GfxProgressiveV2, TRUE))
			goto fail;

		if (!freerdp_settings_set_bool(settings, FreeRDP_MouseUseRelativeMove, FALSE) ||
		    !freerdp_settings_set_bool(settings, FreeRDP_HasRelativeMouseEvent, FALSE))
			goto fail;
	}

	if ((status = shadow_server_parse_command_line(server, argc, argv, shadow_args)) < 0)
	{
		status = shadow_server_command_line_status_print(server, argc, argv, status, shadow_args);
		goto fail;
	}

	if ((status = shadow_server_init(server)) < 0)
	{
		WLog_ERR(TAG, "Server initialization failed.");
		goto fail;
	}

	if ((status = shadow_server_start(server)) < 0)
	{
		WLog_ERR(TAG, "Failed to start server.");
		goto fail;
	}

#ifdef _WIN32
	{
		MSG msg = WINPR_C_ARRAY_INIT;
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#endif

	(void)WaitForSingleObject(server->thread, INFINITE);

	if (!GetExitCodeThread(server->thread, &dwExitCode))
		status = -1;
	else
		status = (int)dwExitCode;

fail:
	shadow_server_uninit(server);
	shadow_server_free(server);
	return status;
}
