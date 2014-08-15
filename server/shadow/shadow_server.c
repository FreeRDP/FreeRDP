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

#include <freerdp/version.h>

#include <winpr/tools/makecert.h>

#ifdef _WIN32
#include <openssl/applink.c>
#endif

#ifndef _WIN32
#include <sys/select.h>
#include <sys/signal.h>
#endif

#include "shadow.h"

#ifdef _WIN32
static BOOL g_MessagePump = TRUE;
#else
static BOOL g_MessagePump = FALSE;
#endif

#ifdef WITH_SHADOW_X11
extern rdpShadowSubsystem* X11_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

#ifdef WITH_SHADOW_MAC
extern rdpShadowSubsystem* Mac_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

#ifdef WITH_SHADOW_WIN
extern rdpShadowSubsystem* Win_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

static COMMAND_LINE_ARGUMENT_A shadow_args[] =
{
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", NULL, NULL, -1, NULL, "Server port" },
	{ "ipc-socket", COMMAND_LINE_VALUE_REQUIRED, "<ipc-socket>", NULL, NULL, -1, NULL, "Server IPC socket" },
	{ "monitors", COMMAND_LINE_VALUE_OPTIONAL, "<0,1,2...>", NULL, NULL, -1, NULL, "Select or list monitors" },
	{ "may-view", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Clients may view without prompt" },
	{ "may-interact", COMMAND_LINE_VALUE_BOOL, NULL, BoolValueTrue, NULL, -1, NULL, "Clients may interact without prompt" },
	{ "version", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_VERSION, NULL, NULL, NULL, -1, NULL, "Print version" },
	{ "help", COMMAND_LINE_VALUE_FLAG | COMMAND_LINE_PRINT_HELP, NULL, NULL, NULL, -1, "?", "Print help" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

int shadow_server_print_command_line_help(int argc, char** argv)
{
	char* str;
	int length;
	COMMAND_LINE_ARGUMENT_A* arg;

	printf("Usage: %s [options]\n", argv[0]);
	printf("\n");

	printf("Syntax:\n");
	printf("    /flag (enables flag)\n");
	printf("    /option:<value> (specifies option with value)\n");
	printf("    +toggle -toggle (enables or disables toggle, where '/' is a synonym of '+')\n");
	printf("\n");

	arg = shadow_args;

	do
	{
		if (arg->Flags & COMMAND_LINE_VALUE_FLAG)
		{
			printf("    %s", "/");
			printf("%-20s", arg->Name);
			printf("\t%s\n", arg->Text);
		}
		else if ((arg->Flags & COMMAND_LINE_VALUE_REQUIRED) || (arg->Flags & COMMAND_LINE_VALUE_OPTIONAL))
		{
			printf("    %s", "/");

			if (arg->Format)
			{
				length = (int) (strlen(arg->Name) + strlen(arg->Format) + 2);
				str = (char*) malloc(length + 1);
				sprintf_s(str, length + 1, "%s:%s", arg->Name, arg->Format);
				printf("%-20s", str);
				free(str);
			}
			else
			{
				printf("%-20s", arg->Name);
			}

			printf("\t%s\n", arg->Text);
		}
		else if (arg->Flags & COMMAND_LINE_VALUE_BOOL)
		{
			length = (int) strlen(arg->Name) + 32;
			str = (char*) malloc(length + 1);
			sprintf_s(str, length + 1, "%s (default:%s)", arg->Name,
					arg->Default ? "on" : "off");

			printf("    %s", arg->Default ? "-" : "+");

			printf("%-20s", str);
			free(str);

			printf("\t%s\n", arg->Text);
		}
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return 1;
}

int shadow_server_command_line_status_print(rdpShadowServer* server, int argc, char** argv, int status)
{
	if (status == COMMAND_LINE_STATUS_PRINT_VERSION)
	{
		printf("FreeRDP version %s (git %s)\n", FREERDP_VERSION_FULL, GIT_REVISION);
		return COMMAND_LINE_STATUS_PRINT_VERSION;
	}
	else if (status == COMMAND_LINE_STATUS_PRINT)
	{
		return COMMAND_LINE_STATUS_PRINT;
	}
	else if (status < 0)
	{
		shadow_server_print_command_line_help(argc, argv);
		return COMMAND_LINE_STATUS_PRINT_HELP;
	}

	return 1;
}

int shadow_server_parse_command_line(rdpShadowServer* server, int argc, char** argv)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	if (argc < 2)
		return 1;

	CommandLineClearArgumentsA(shadow_args);

	flags = COMMAND_LINE_SEPARATOR_COLON;
	flags |= COMMAND_LINE_SIGIL_SLASH | COMMAND_LINE_SIGIL_PLUS_MINUS;

	status = CommandLineParseArgumentsA(argc, (const char**) argv, shadow_args, flags, server, NULL, NULL);

	if (status < 0)
		return status;

	arg = shadow_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "port")
		{
			server->port = (DWORD) atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "ipc-socket")
		{
			server->ipcSocket = _strdup(arg->Value);
		}
		CommandLineSwitchCase(arg, "may-view")
		{
			server->mayView = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchCase(arg, "may-interact")
		{
			server->mayInteract = arg->Value ? TRUE : FALSE;
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	arg = CommandLineFindArgumentA(shadow_args, "monitors");

	if (arg && (arg->Flags & COMMAND_LINE_ARGUMENT_PRESENT))
	{
		if (arg->Flags & COMMAND_LINE_VALUE_PRESENT)
		{
			/* Select monitors */
		}
		else
		{
			int index;
			int width, height;
			MONITOR_DEF* monitor;
			rdpShadowSubsystem* subsystem = server->subsystem;

			/* List monitors */

			for (index = 0; index < subsystem->monitorCount; index++)
			{
				monitor = &(subsystem->monitors[index]);

				width = monitor->right - monitor->left;
				height = monitor->bottom - monitor->top;

				printf("      %s [%d] %dx%d\t+%d+%d\n",
					(monitor->flags == 1) ? "*" : " ", index,
					width, height, monitor->left, monitor->top);
			}

			status = COMMAND_LINE_STATUS_PRINT;
		}
	}

	return status;
}

int shadow_server_surface_update(rdpShadowSubsystem* subsystem, REGION16* region)
{
	int index;
	int count;
	wArrayList* clients;
	rdpShadowServer* server;
	rdpShadowClient* client;

	server = subsystem->server;
	clients = server->clients;

	ArrayList_Lock(clients);

	count = ArrayList_Count(clients);

	for (index = 0; index < count; index++)
	{
		client = ArrayList_GetItem(clients, index);
		shadow_client_surface_update(client, region);
	}

	ArrayList_Unlock(clients);

	return 1;
}

void* shadow_server_thread(rdpShadowServer* server)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;
	freerdp_listener* listener;
	rdpShadowSubsystem* subsystem;

	listener = server->listener;
	StopEvent = server->StopEvent;
	subsystem = server->subsystem;

	if (subsystem->Start)
	{
		subsystem->Start(subsystem);
	}

	while (1)
	{
		nCount = 0;

		if (listener->GetEventHandles(listener, events, &nCount) < 0)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
			break;
		}

		events[nCount++] = server->StopEvent;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(server->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (!listener->CheckFileDescriptor(listener))
		{
			fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
			break;
		}

#ifdef _WIN32
		Sleep(100); /* FIXME: listener event handles */
#endif
	}

	listener->Close(listener);

	if (subsystem->Stop)
	{
		subsystem->Stop(subsystem);
	}

	ExitThread(0);

	return NULL;
}

int shadow_server_start(rdpShadowServer* server)
{
	BOOL status;
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	if (!server->ipcSocket)
		status = server->listener->Open(server->listener, NULL, (UINT16) server->port);
	else
		status = server->listener->OpenLocal(server->listener, server->ipcSocket);

	if (status)
	{
		server->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				shadow_server_thread, (void*) server, 0, NULL);
	}

	return 0;
}

int shadow_server_stop(rdpShadowServer* server)
{
	if (server->thread)
	{
		SetEvent(server->StopEvent);
		WaitForSingleObject(server->thread, INFINITE);
		CloseHandle(server->thread);
		server->thread = NULL;

		server->listener->Close(server->listener);
	}

	return 0;
}

int shadow_server_init_certificate(rdpShadowServer* server)
{
	char* filepath;
	MAKECERT_CONTEXT* makecert;

	const char* makecert_argv[6] =
	{
		"makecert",
		"-rdp",
		"-live",
		"-silent",
		"-y", "5"
	};

	int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

	if (!PathFileExistsA(server->ConfigPath))
		CreateDirectoryA(server->ConfigPath, 0);

	filepath = GetCombinedPath(server->ConfigPath, "shadow");

	if (!filepath)
		return -1;

	if (!PathFileExistsA(filepath))
		CreateDirectoryA(filepath, 0);

	server->CertificateFile = GetCombinedPath(filepath, "shadow.crt");
	server->PrivateKeyFile = GetCombinedPath(filepath, "shadow.key");

	if ((!PathFileExistsA(server->CertificateFile)) ||
			(!PathFileExistsA(server->PrivateKeyFile)))
	{
		makecert = makecert_context_new();

		makecert_context_process(makecert, makecert_argc, (char**) makecert_argv);

		makecert_context_set_output_file_name(makecert, "shadow");

		if (!PathFileExistsA(server->CertificateFile))
			makecert_context_output_certificate_file(makecert, filepath);

		if (!PathFileExistsA(server->PrivateKeyFile))
			makecert_context_output_private_key_file(makecert, filepath);

		makecert_context_free(makecert);
	}

	free(filepath);

	return 1;
}

int shadow_server_init(rdpShadowServer* server)
{
	int status;

	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());

	server->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	status = shadow_server_init_certificate(server);

	if (status < 0)
		return -1;

	server->listener = freerdp_listener_new();

	if (!server->listener)
		return -1;

	server->listener->info = (void*) server;
	server->listener->PeerAccepted = shadow_client_accepted;

#ifdef WITH_SHADOW_X11
	server->CreateSubsystem = X11_ShadowCreateSubsystem;
#endif

#ifdef WITH_SHADOW_MAC
	server->CreateSubsystem = Mac_ShadowCreateSubsystem;
#endif

#ifdef WITH_SHADOW_WIN
	server->CreateSubsystem = Win_ShadowCreateSubsystem;
#endif
	
	if (server->CreateSubsystem)
		server->subsystem = server->CreateSubsystem(server);

	if (!server->subsystem)
		return -1;

	server->subsystem->SurfaceUpdate = shadow_server_surface_update;

	if (server->subsystem->Init)
	{
		status = server->subsystem->Init(server->subsystem);

		if (status < 0)
			fprintf(stderr, "subsystem init failure: %d\n", status);
	}

	server->screen = shadow_screen_new(server);

	if (!server->screen)
		return -1;

	server->capture = shadow_capture_new(server);

	if (!server->capture)
		return -1;

	return 1;
}

int shadow_server_uninit(rdpShadowServer* server)
{
	shadow_server_stop(server);

	if (server->listener)
	{
		freerdp_listener_free(server->listener);
		server->listener = NULL;
	}

	if (server->subsystem)
	{
		server->subsystem->Free(server->subsystem);
		server->subsystem = NULL;
	}

	if (server->CertificateFile)
	{
		free(server->CertificateFile);
		server->CertificateFile = NULL;
	}
	
	if (server->PrivateKeyFile)
	{
		free(server->PrivateKeyFile);
		server->PrivateKeyFile = NULL;
	}

	if (server->ipcSocket)
	{
		free(server->ipcSocket);
		server->ipcSocket = NULL;
	}

	return 1;
}

rdpShadowServer* shadow_server_new()
{
	rdpShadowServer* server;

	server = (rdpShadowServer*) calloc(1, sizeof(rdpShadowServer));

	if (!server)
		return NULL;

	server->port = 3389;
	server->mayView = TRUE;
	server->mayInteract = TRUE;

#ifdef _WIN32
	server->ConfigPath = GetEnvironmentSubPath("LOCALAPPDATA", "freerdp");
#endif

	if (!server->ConfigPath)
		server->ConfigPath = GetKnownSubPath(KNOWN_PATH_XDG_CONFIG_HOME, "freerdp");

	InitializeCriticalSectionAndSpinCount(&(server->lock), 4000);

	server->clients = ArrayList_New(TRUE);

	return server;
}

void shadow_server_free(rdpShadowServer* server)
{
	if (!server)
		return;

	DeleteCriticalSection(&(server->lock));

	if (server->clients)
	{
		ArrayList_Free(server->clients);
		server->clients = NULL;
	}

	shadow_server_uninit(server);

	free(server);
}

int main(int argc, char** argv)
{
	MSG msg;
	int status;
	DWORD dwExitCode;
	rdpShadowServer* server;

	server = shadow_server_new();

	if (!server)
		return 0;

	if (shadow_server_init(server) < 0)
		return 0;

	status = shadow_server_parse_command_line(server, argc, argv);

	status = shadow_server_command_line_status_print(server, argc, argv, status);

	if (status < 0)
		return 0;

	if (shadow_server_start(server) < 0)
		return 0;

	if (g_MessagePump)
	{
		while (GetMessage(&msg, 0, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	WaitForSingleObject(server->thread, INFINITE);

	GetExitCodeThread(server->thread, &dwExitCode);

	shadow_server_free(server);

	return 0;
}
