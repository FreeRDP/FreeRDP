/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include <freerdp/listener.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/locale/keyboard.h>

#include "wf_input.h"
#include "wf_peer.h"

#include "wfreerdp.h"

HANDLE g_done_event;
int g_thread_count = 0;

BOOL derp = false;

static DWORD WINAPI wf_peer_main_loop(LPVOID lpParam)
{
	int rcount;
	void* rfds[32];
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	memset(rfds, 0, sizeof(rfds));

	wf_peer_init(client);

	/* Initialize the real server settings here */
	client->settings->cert_file = xstrdup("server.crt");
	client->settings->privatekey_file = xstrdup("server.key");

	client->PostConnect = wf_peer_post_connect;
	client->Activate = wf_peer_activate;

	client->input->SynchronizeEvent = wf_peer_synchronize_event;
	client->input->KeyboardEvent = wf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = wf_peer_unicode_keyboard_event;
	client->input->MouseEvent = wf_peer_mouse_event;
	client->input->ExtendedMouseEvent = wf_peer_extended_mouse_event;

	client->Initialize(client);
	context = (wfPeerContext*) client->context;

	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		if (client->CheckFileDescriptor(client) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}

		if(client->activated)
			wf_peer_send_changes(client->update);
	}

	printf("Client %s disconnected.\n", client->local ? "(local)" : client->hostname);

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return 0;
}

static void wf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	/* start peer main loop thread */

	if (CreateThread(NULL, 0, wf_peer_main_loop, client, 0, NULL) != 0)
		g_thread_count++;
}

static void wf_server_main_loop(freerdp_listener* instance)
{
	int rcount;
	void* rfds[32];

	memset(rfds, 0, sizeof(rfds));

	while (1)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		if (instance->CheckFileDescriptor(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		Sleep(20);
	}

	instance->Close(instance);
}

int main(int argc, char* argv[])
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	int port = 3389;
	WSADATA wsa_data;
	freerdp_listener* instance;

	instance = freerdp_listener_new();

	instance->PeerAccepted = wf_peer_accepted;

	if (WSAStartup(0x101, &wsa_data) != 0)
		return 1;

	g_done_event = CreateEvent(0, 1, 0, 0);

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		if (RegQueryValueEx(hKey, _T("DefaultPort"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			port = dwValue;
	}

	RegCloseKey(hKey);

	if (argc == 2)
		port = atoi(argv[1]);

	/* Open the server socket and start listening. */

	if (instance->Open(instance, NULL, port))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		wf_server_main_loop(instance);
	}

	if (g_thread_count > 0)
		WaitForSingleObject(g_done_event, INFINITE);
	else
		MessageBox(GetConsoleWindow(),
			L"Failed to start wfreerdp-server.\n\nPlease check the debug output.",
			L"FreeRDP Error", MB_ICONSTOP);

	WSACleanup();

	freerdp_listener_free(instance);

	return 0;
}
