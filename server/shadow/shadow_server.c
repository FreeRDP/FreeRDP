/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <errno.h>

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/path.h>
#include <winpr/cmdline.h>
#include <winpr/winsock.h>

#include <freerdp/log.h>
#include <freerdp/version.h>

#include <winpr/tools/makecert.h>

#ifndef _WIN32
#include <sys/select.h>
#include <signal.h>
#endif

#include "shadow.h"

#define TAG SERVER_TAG("shadow")

static const char bind_address[] = "bind-address,";

static int shadow_server_print_command_line_help(int argc, char** argv,
                                                 COMMAND_LINE_ARGUMENT_A* largs)
{
	char* str;
	size_t length;
	const COMMAND_LINE_ARGUMENT_A* arg;
	if ((argc < 1) || !largs || !argv)
		return -1;

	WLog_INFO(TAG, "Usage: %s [options]", argv[0]);
	WLog_INFO(TAG, "");
	WLog_INFO(TAG, "Syntax:");
	WLog_INFO(TAG, "    /flag (enables flag)");
	WLog_INFO(TAG, "    /option:<value> (specifies option with value)");
	WLog_INFO(TAG,
	          "    +toggle -toggle (enables or disables toggle, where '/' is a synonym of '+')");
	WLog_INFO(TAG, "");
	arg = largs;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			WLog_INFO(TAG, "    %s", "/");
			WLog_INFO(TAG, "%-20s", arg->Name);
			WLog_INFO(TAG, "\t%s", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) ||
		         (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			WLog_INFO(TAG, "    %s", "/");

			if (arg->Format)
			{
				length = (strlen(arg->Name) + strlen(arg->Format) + 2);
				str = (char*)malloc(length + 1);

				if (!str)
					return -1;

				sprintf_s(str, length + 1, "%s:%s", arg->Name, arg->Format);
				WLog_INFO(TAG, "%-20s", str);
				free(str);
			}
			else
			{
				WLog_INFO(TAG, "%-20s", arg->Name);
			}

			WLog_INFO(TAG, "\t%s", arg->Text);
		}
		else if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			length = strlen(arg->Name) + 32;
			str = (char*)malloc(length + 1);

			if (!str)
				return -1;

			sprintf_s(str, length + 1, "%s (default:%s)", arg->Name, arg->Default ? "on" : "off");
			WLog_INFO(TAG, "    %s", arg->Default ? "-" : "+");
			WLog_INFO(TAG, "%-20s", str);
			free(str);
			WLog_INFO(TAG, "\t%s", arg->Text);
		}
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

int shadow_server_command_line_status_print(rdpShadowServer* server, int argc, char** argv,
                                            int status, COMMAND_LINE_ARGUMENT_A* cargs)
{
	WINPR_UNUSED(server);

	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		WLog_INFO(TAG, "FreeRDP version %s (git %s)", FREERDP_VERSION_FULL, FREERDP_GIT_REVISION);
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT_BUILDCONFIG)
	{
		WLog_INFO(TAG, "%s", freerdp_get_build_config());
		return COMMAND_LINE_STATUS_PRINT_BUILDCONFIG;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		return COMMAND_LINE_STATUS_PRINT;
	}
	else if (status < 0)
	{
		if (shadow_server_print_command_line_help(argc, argv, cargs) < 0)
			return -1;

		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	return 1;
}

int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv,
                                     COMMAND_LINE_ARGUMENT_A* cargs)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	rdpSettings* settings = server->settings;

	if ((argc < 2) || !argv || !cargs)
		return 1;

	CommandLineClearArgumentsA(cargs);
	flags = COMMAND_LINE_SEPARATOR_COLON;
	flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;
	status = CommandLineParseArgumentsA(argc, argv, cargs, flags, server, NULL, NULL);

	if (status < 0)
		return status;

	arg = cargs;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "port")
		{
			long val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val <= 0) || (val > UINT16_MAX))
				return -1;

			server->port = (DWORD)val;
		}
		CommandLineSwitchCase(arg, "ipc-socket")
		{
			/* /bind-address is incompatible */
			if (server->ipcSocket)
				return -1;
			server->ipcSocket = _strdup(arg->Value);

			if (!server->ipcSocket)
				return -1;
		}
		CommandLineSwitchCase(arg, "bind-address")
		{
			int rc;
			size_t len = strlen(arg->Value) + sizeof(bind_address);
			/* /ipc-socket is incompatible */
			if (server->ipcSocket)
				return -1;
			server->ipcSocket = calloc(len, sizeof(CHAR));

			if (!server->ipcSocket)
				return -1;

			rc = _snprintf(server->ipcSocket, len, "%s%s", bind_address, arg->Value);
			if ((rc < 0) || ((size_t)rc != len - 1))
				return -1;
		}
		CommandLineSwitchCase(arg, "may-view")
		{
			server->mayView = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "may-interact")
		{
			server->mayInteract = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "rect")
		{
			char* p;
			char* tok[4];
			long x = -1, y = -1, w = -1, h = -1;
			char* str = _strdup(arg->Value);

			if (!str)
				return -1;

			tok[0] = p = str;
			p = strchr(p + 1, ',');

			if (!p)
			{
				free(str);
				return -1;
			}

			*p++ = '\0';
			tok[1] = p;
			p = strchr(p + 1, ',');

			if (!p)
			{
				free(str);
				return -1;
			}

			*p++ = '\0';
			tok[2] = p;
			p = strchr(p + 1, ',');

			if (!p)
			{
				free(str);
				return -1;
			}

			*p++ = '\0';
			tok[3] = p;
			x = strtol(tok[0], NULL, 0);

			if (errno != 0)
				goto fail;

			y = strtol(tok[1], NULL, 0);

			if (errno != 0)
				goto fail;

			w = strtol(tok[2], NULL, 0);

			if (errno != 0)
				goto fail;

			h = strtol(tok[3], NULL, 0);

			if (errno != 0)
				goto fail;

		fail:
			free(str);

			if ((x < 0) || (y < 0) || (w < 1) || (h < 1) || (errno != 0))
				return -1;

			if ((x > UINT16_MAX) || (y > UINT16_MAX) || (x + w > UINT16_MAX) ||
			    (y + h > UINT16_MAX))
				return -1;
			server->subRect.left = (UINT16)x;
			server->subRect.top = (UINT16)y;
			server->subRect.right = (UINT16)(x + w);
			server->subRect.bottom = (UINT16)(y + h);
			server->shareSubRect = TRUE;
		}
		CommandLineSwitchCase(arg, "auth")
		{
			server->authentication = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec")
		{
			if (strcmp("rdp", arg->Value) == 0) /* Standard RDP */
			{
				settings->RdpSecurity = TRUE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = FALSE;
				settings->UseRdpSecurityLayer = TRUE;
			}
			else if (strcmp("tls", arg->Value) == 0) /* TLS */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = TRUE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = FALSE;
			}
			else if (strcmp("nla", arg->Value) == 0) /* NLA */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = TRUE;
				settings->ExtSecurity = FALSE;
			}
			else if (strcmp("ext", arg->Value) == 0) /* NLA Extended */
			{
				settings->RdpSecurity = FALSE;
				settings->TlsSecurity = FALSE;
				settings->NlaSecurity = FALSE;
				settings->ExtSecurity = TRUE;
			}
			else
			{
				WLog_ERR(TAG, "unknown protocol security: %s", arg->Value);
			}
		}
		CommandLineSwitchCase(arg, "sec-rdp")
		{
			settings->RdpSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-tls")
		{
			settings->TlsSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-nla")
		{
			settings->NlaSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sec-ext")
		{
			settings->ExtSecurity = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "sam-file")
		{
			freerdp_settings_set_string(settings, FreeRDP_NtlmSamFile, arg->Value);
		}
		CommandLineSwitchCase(arg, "log-level")
		{
			wLog* root = WLog_GetRoot();

			if (!WLog_SetStringLogLevel(root, arg->Value))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "log-filters")
		{
			if (!WLog_AddStringLogFilters(arg->Value))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "gfx-progressive")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxProgressive,
			                               arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "gfx-rfx")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec,
			                               arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "gfx-planar")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxPlanar, arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "gfx-avc420")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxH264, arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "gfx-avc444")
		{
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444v2,
			                               arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
			if (!freerdp_settings_set_bool(settings, FreeRDP_GfxAVC444, arg->Value ? TRUE : FALSE))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchCase(arg, "keytab")
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_KerberosKeytab, arg->Value))
				return COMMAND_LINE_ERROR;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	arg = CommandLineFindArgumentA(cargs, "monitors");

	if (arg && (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
	{
		UINT32 index;
		UINT32 numMonitors;
		MONITOR_DEF monitors[16] = { 0 };
		numMonitors = shadow_enum_monitors(monitors, 16);

		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			/* Select monitors */
			long val = strtol(arg->Value, NULL, 0);

			if ((val < 0) || (errno != 0) || ((UINT32)val >= numMonitors))
				status = COMMAND_LINE_STATUS_PRINT;

			server->selectedMonitor = (UINT32)val;
		}
		else
		{
			/* List monitors */

			for (index = 0; index < numMonitors; index++)
			{
				const MONITOR_DEF* monitor = &monitors[index];
				const INT64 width = monitor->right - monitor->left + 1;
				const INT64 height = monitor->bottom - monitor->top + 1;
				WLog_INFO(TAG, "      %s [%d] %" PRId64 "x%" PRId64 "\t+%" PRId32 "+%" PRId32 "",
				          (monitor->flags == 1) ? "*" : " ", index, width, height, monitor->left,
				          monitor->top);
			}

			status = COMMAND_LINE_STATUS_PRINT;
		}
	}

	return status;
}

static DWORD WINAPI shadow_server_thread(LPVOID arg)
{
	rdpShadowServer* server = (rdpShadowServer*)arg;
	BOOL running = TRUE;
	DWORD status;
	freerdp_listener* listener = server->listener;
	shadow_subsystem_start(server->subsystem);

	while (running)
	{
		HANDLE events[32];
		DWORD nCount = 0;
		events[nCount++] = server->StopEvent;
		nCount += listener->GetEventHandles(listener, &events[nCount], 32 - nCount);

		if (nCount <= 1)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		switch (status)
		{
			case WAIT_FAILED:
			case WAIT_OBJECT_0:
				running = FALSE;
				break;

			default:
			{
				if (!listener->CheckFileDescriptor(listener))
				{
					WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
					running = FALSE;
				}
				else
				{
#ifdef _WIN32
					Sleep(100); /* FIXME: listener event handles */
#endif
				}
			}
			break;
		}
	}

	listener->Close(listener);
	shadow_subsystem_stop(server->subsystem);

	/* Signal to the clients that server is being stopped and wait for them
	 * to disconnect. */
	if (shadow_client_boardcast_quit(server, 0))
	{
		while (ArrayList_Count(server->clients) > 0)
		{
			Sleep(100);
		}
	}

	ExitThread(0);
	return 0;
}

static BOOL open_port(rdpShadowServer* server, char* address)
{
	BOOL status;
	char* modaddr = address;

	if (modaddr)
	{
		if (modaddr[0] == '[')
		{
			char* end = strchr(address, ']');
			if (!end)
			{
				WLog_ERR(TAG, "Could not parse bind-address %s", address);
				return -1;
			}
			*end++ = '\0';
			if (strlen(end) > 0)
			{
				WLog_ERR(TAG, "Excess data after IPv6 address: '%s'", end);
				return -1;
			}
			modaddr++;
		}
	}
	status = server->listener->Open(server->listener, modaddr, (UINT16)server->port);

	if (!status)
	{
		WLog_ERR(TAG,
		         "Problem creating TCP listener. (Port already used or insufficient permissions?)");
	}

	return status;
}

int shadow_server_start(rdpShadowServer* server)
{
	BOOL ipc;
	BOOL status;
	WSADATA wsaData;

	if (!server)
		return -1;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	server->screen = shadow_screen_new(server);

	if (!server->screen)
	{
		WLog_ERR(TAG, "screen_new failed");
		return -1;
	}

	server->capture = shadow_capture_new(server);

	if (!server->capture)
	{
		WLog_ERR(TAG, "capture_new failed");
		return -1;
	}

	/* Bind magic:
	 *
	 * emtpy                 ... bind TCP all
	 * <local path>          ... bind local (IPC)
	 * bind-socket,<address> ... bind TCP to specified interface
	 */
	ipc = server->ipcSocket && (strncmp(bind_address, server->ipcSocket,
	                                    strnlen(bind_address, sizeof(bind_address))) != 0);
	if (!ipc)
	{
		size_t x, count;
		char** list = CommandLineParseCommaSeparatedValuesEx(NULL, server->ipcSocket, &count);
		if (!list || (count <= 1))
		{
			if (server->ipcSocket == NULL)
			{
				if (!open_port(server, NULL))
				{
					free(list);
					return -1;
				}
			}
			else
			{
				free(list);
				return -1;
			}
		}

		WINPR_ASSERT(list || (count == 0));
		for (x = 1; x < count; x++)
		{
			BOOL success = open_port(server, list[x]);
			if (!success)
			{
				free(list);
				return -1;
			}
		}
		free(list);
	}
	else
	{
		status = server->listener->OpenLocal(server->listener, server->ipcSocket);

		if (!status)
		{
			WLog_ERR(TAG, "Problem creating local socket listener. (Port already used or "
			              "insufficient permissions?)");
			return -1;
		}
	}

	if (!(server->thread = CreateThread(NULL, 0, shadow_server_thread, (void*)server, 0, NULL)))
	{
		return -1;
	}

	return 0;
}

int shadow_server_stop(rdpShadowServer* server)
{
	if (!server)
		return -1;

	if (server->thread)
	{
		SetEvent(server->StopEvent);
		WaitForSingleObject(server->thread, INFINITE);
		CloseHandle(server->thread);
		server->thread = NULL;
		server->listener->Close(server->listener);
	}

	if (server->screen)
	{
		shadow_screen_free(server->screen);
		server->screen = NULL;
	}

	if (server->capture)
	{
		shadow_capture_free(server->capture);
		server->capture = NULL;
	}

	return 0;
}

static int shadow_server_init_config_path(rdpShadowServer* server)
{
#ifdef _WIN32

	if (!server->ConfigPath)
	{
		server->ConfigPath = GetEnvironmentSubPath("LOCALAPPDATA", "freerdp");
	}

#endif
#ifdef __APPLE__

	if (!server->ConfigPath)
	{
		char* userLibraryPath;
		char* userApplicationSupportPath;
		userLibraryPath = GetKnownSubPath(KNOWN_PATH_HOME, "Library");

		if (userLibraryPath)
		{
			if (!winpr_PathFileExists(userLibraryPath) && !winpr_PathMakePath(userLibraryPath, 0))
			{
				WLog_ERR(TAG, "Failed to create directory '%s'", userLibraryPath);
				free(userLibraryPath);
				return -1;
			}

			userApplicationSupportPath = GetCombinedPath(userLibraryPath, "Application Support");

			if (userApplicationSupportPath)
			{
				if (!winpr_PathFileExists(userApplicationSupportPath) &&
				    !winpr_PathMakePath(userApplicationSupportPath, 0))
				{
					WLog_ERR(TAG, "Failed to create directory '%s'", userApplicationSupportPath);
					free(userLibraryPath);
					free(userApplicationSupportPath);
					return -1;
				}

				server->ConfigPath = GetCombinedPath(userApplicationSupportPath, "freerdp");
			}

			free(userLibraryPath);
			free(userApplicationSupportPath);
		}
	}

#endif

	if (!server->ConfigPath)
	{
		char* configHome;
		configHome = GetKnownPath(KNOWN_PATH_XDG_CONFIG_HOME);

		if (configHome)
		{
			if (!winpr_PathFileExists(configHome) && !winpr_PathMakePath(configHome, 0))
			{
				WLog_ERR(TAG, "Failed to create directory '%s'", configHome);
				free(configHome);
				return -1;
			}

			server->ConfigPath = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, "freerdp");
			free(configHome);
		}
	}

	if (!server->ConfigPath)
		return -1; /* no usable config path */

	return 1;
}

static BOOL shadow_server_init_certificate(rdpShadowServer* server)
{
	char* filepath;
	MAKECERT_CONTEXT* makecert = NULL;
	BOOL ret = FALSE;
	char* makecert_argv[6] = { "makecert", "-rdp", "-live", "-silent", "-y", "5" };
	int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

	if (!winpr_PathFileExists(server->ConfigPath) && !winpr_PathMakePath(server->ConfigPath, 0))
	{
		WLog_ERR(TAG, "Failed to create directory '%s'", server->ConfigPath);
		return FALSE;
	}

	if (!(filepath = GetCombinedPath(server->ConfigPath, "shadow")))
		return FALSE;

	if (!winpr_PathFileExists(filepath) && !winpr_PathMakePath(filepath, 0))
	{
		if (!CreateDirectoryA(filepath, 0))
		{
			WLog_ERR(TAG, "Failed to create directory '%s'", filepath);
			goto out_fail;
		}
	}

	server->CertificateFile = GetCombinedPath(filepath, "shadow.crt");
	server->PrivateKeyFile = GetCombinedPath(filepath, "shadow.key");

	if (!server->CertificateFile || !server->PrivateKeyFile)
		goto out_fail;

	if ((!winpr_PathFileExists(server->CertificateFile)) ||
	    (!winpr_PathFileExists(server->PrivateKeyFile)))
	{
		makecert = makecert_context_new();

		if (!makecert)
			goto out_fail;

		if (makecert_context_process(makecert, makecert_argc, makecert_argv) < 0)
			goto out_fail;

		if (makecert_context_set_output_file_name(makecert, "shadow") != 1)
			goto out_fail;

		if (!winpr_PathFileExists(server->CertificateFile))
		{
			if (makecert_context_output_certificate_file(makecert, filepath) != 1)
				goto out_fail;
		}

		if (!winpr_PathFileExists(server->PrivateKeyFile))
		{
			if (makecert_context_output_private_key_file(makecert, filepath) != 1)
				goto out_fail;
		}
	}

	ret = TRUE;
out_fail:
	makecert_context_free(makecert);
	free(filepath);
	return ret;
}

int shadow_server_init(rdpShadowServer* server)
{
	int status;
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());

	if (!(server->clients = ArrayList_New(TRUE)))
		goto fail_client_array;

	if (!(server->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_stop_event;

	if (!InitializeCriticalSectionAndSpinCount(&(server->lock), 4000))
		goto fail_server_lock;

	status = shadow_server_init_config_path(server);

	if (status < 0)
		goto fail_config_path;

	status = shadow_server_init_certificate(server);

	if (status < 0)
		goto fail_certificate;

	server->listener = freerdp_listener_new();

	if (!server->listener)
		goto fail_listener;

	server->listener->info = (void*)server;
	server->listener->PeerAccepted = shadow_client_accepted;
	server->subsystem = shadow_subsystem_new();

	if (!server->subsystem)
		goto fail_subsystem_new;

	status = shadow_subsystem_init(server->subsystem, server);

	if (status >= 0)
		return status;

	shadow_subsystem_free(server->subsystem);
fail_subsystem_new:
	freerdp_listener_free(server->listener);
	server->listener = NULL;
fail_listener:
	free(server->CertificateFile);
	server->CertificateFile = NULL;
	free(server->PrivateKeyFile);
	server->PrivateKeyFile = NULL;
fail_certificate:
	free(server->ConfigPath);
	server->ConfigPath = NULL;
fail_config_path:
	DeleteCriticalSection(&(server->lock));
fail_server_lock:
	CloseHandle(server->StopEvent);
	server->StopEvent = NULL;
fail_stop_event:
	ArrayList_Free(server->clients);
	server->clients = NULL;
fail_client_array:
	WLog_ERR(TAG, "Failed to initialize shadow server");
	return -1;
}

int shadow_server_uninit(rdpShadowServer* server)
{
	if (!server)
		return -1;

	shadow_server_stop(server);
	shadow_subsystem_uninit(server->subsystem);
	shadow_subsystem_free(server->subsystem);
	freerdp_listener_free(server->listener);
	server->listener = NULL;
	free(server->CertificateFile);
	server->CertificateFile = NULL;
	free(server->PrivateKeyFile);
	server->PrivateKeyFile = NULL;
	free(server->ConfigPath);
	server->ConfigPath = NULL;
	DeleteCriticalSection(&(server->lock));
	CloseHandle(server->StopEvent);
	server->StopEvent = NULL;
	ArrayList_Free(server->clients);
	server->clients = NULL;
	return 1;
}

rdpShadowServer* shadow_server_new(void)
{
	rdpShadowServer* server;
	server = (rdpShadowServer*)calloc(1, sizeof(rdpShadowServer));

	if (!server)
		return NULL;

	server->port = 3389;
	server->mayView = TRUE;
	server->mayInteract = TRUE;
	server->rfxMode = RLGR3;
	server->h264RateControlMode = H264_RATECONTROL_VBR;
	server->h264BitRate = 10000000;
	server->h264FrameRate = 30;
	server->h264QP = 0;
	server->authentication = FALSE;
	server->settings = freerdp_settings_new(FREERDP_SETTINGS_SERVER_MODE);
	return server;
}

void shadow_server_free(rdpShadowServer* server)
{
	if (!server)
		return;

	free(server->ipcSocket);
	server->ipcSocket = NULL;
	freerdp_settings_free(server->settings);
	server->settings = NULL;
	free(server);
}
