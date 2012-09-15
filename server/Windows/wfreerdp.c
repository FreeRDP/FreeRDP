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

#include "wf_update.h"
#include "wf_input.h"
#include "wf_peer.h"

#include "wfreerdp.h"

HANDLE g_done_event;
int g_thread_count = 0;

static DWORD WINAPI wf_peer_socket_listener(LPVOID lpParam)
{
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	memset(rfds, 0, sizeof(rfds));
	context = (wfPeerContext*) client->context;

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != true)
		{
			printf("Failed to get peer file descriptor\n");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}
		
		if (max_fds == 0)
			break;
		
		select(max_fds + 1, &rfds_set, NULL, NULL, NULL);

		SetEvent(context->socketEvent);
		WaitForSingleObject(context->socketSemaphore, INFINITE);

		if (context->socketClose)
			break;
	}

	return 0;
}

static void wf_peer_read_settings(freerdp_peer* client)
{
	HKEY hKey;
	int length;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	TCHAR* PrivateKeyFile;
	TCHAR* CertificateFile;
	char* PrivateKeyFileA;
	char* CertificateFileA;

	PrivateKeyFile = CertificateFile = NULL;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return;

	status = RegQueryValueEx(hKey, _T("CertificateFile"), NULL, &dwType, NULL, &dwSize);

	if (status == ERROR_SUCCESS)
	{
		CertificateFile = (LPTSTR) malloc(dwSize + sizeof(TCHAR));
		status = RegQueryValueEx(hKey, _T("CertificateFile"), NULL, &dwType, (BYTE*) CertificateFile, &dwSize);
	}

	status = RegQueryValueEx(hKey, _T("PrivateKeyFile"), NULL, &dwType, NULL, &dwSize);

	if (status == ERROR_SUCCESS)
	{
		PrivateKeyFile = (LPTSTR) malloc(dwSize + sizeof(TCHAR));
		status = RegQueryValueEx(hKey, _T("PrivateKeyFile"), NULL, &dwType, (BYTE*) PrivateKeyFile, &dwSize);
	}

	if (CertificateFile)
	{
#ifdef UNICODE
		length = WideCharToMultiByte(CP_UTF8, 0, CertificateFile, lstrlenW(CertificateFile), NULL, 0, NULL, NULL);
		CertificateFileA = (char*) malloc(length + 1);
		WideCharToMultiByte(CP_UTF8, 0, CertificateFile, lstrlenW(CertificateFile), CertificateFileA, length, NULL, NULL);
		CertificateFileA[length] = '\0';
		free(CertificateFile);
#else
		CertificateFileA = (char*) CertificateFile;
#endif
		client->settings->cert_file = CertificateFileA;
	}
	else
	{
		client->settings->cert_file = _strdup("server.crt");
	}

	if (PrivateKeyFile)
	{
#ifdef UNICODE
		length = WideCharToMultiByte(CP_UTF8, 0, PrivateKeyFile, lstrlenW(PrivateKeyFile), NULL, 0, NULL, NULL);
		PrivateKeyFileA = (char*) malloc(length + 1);
		WideCharToMultiByte(CP_UTF8, 0, PrivateKeyFile, lstrlenW(PrivateKeyFile), PrivateKeyFileA, length, NULL, NULL);
		PrivateKeyFileA[length] = '\0';
		free(PrivateKeyFile);
#else
		PrivateKeyFileA = (char*) PrivateKeyFile;
#endif
		client->settings->privatekey_file = PrivateKeyFileA;
	}
	else
	{
		client->settings->privatekey_file = _strdup("server.key");
	}

	RegCloseKey(hKey);
}

static DWORD WINAPI wf_peer_main_loop(LPVOID lpParam)
{
	wfInfo* wfi;
	DWORD nCount;
	DWORD status;
	HANDLE handles[32];
	rdpSettings* settings;
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	wf_peer_init(client);

	settings = client->settings;
	settings->rfx_codec = true;
	settings->ns_codec = false;
	settings->jpeg_codec = false;
	wf_peer_read_settings(client);

	client->PostConnect = wf_peer_post_connect;
	client->Activate = wf_peer_activate;

	client->input->SynchronizeEvent = wf_peer_synchronize_event;
	client->input->KeyboardEvent = wf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = wf_peer_unicode_keyboard_event;
	client->input->MouseEvent = wf_peer_mouse_event;
	client->input->ExtendedMouseEvent = wf_peer_extended_mouse_event;

	client->Initialize(client);
	context = (wfPeerContext*) client->context;

	wfi = context->info;
	context->socketEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	context->socketSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
	context->socketThread = CreateThread(NULL, 0, wf_peer_socket_listener, client, 0, NULL);

	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);

	nCount = 0;
	handles[nCount++] = context->updateEvent;
	handles[nCount++] = context->socketEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (WaitForSingleObject(context->updateEvent, 0) == 0)
		{
			if (client->activated)
				wf_update_peer_send(wfi, context);

			ResetEvent(context->updateEvent);
			ReleaseSemaphore(wfi->updateSemaphore, 1, NULL);
		}

		if (WaitForSingleObject(context->socketEvent, 0) == 0)
		{
			if (client->CheckFileDescriptor(client) != true)
			{
				printf("Failed to check peer file descriptor\n");
				context->socketClose = TRUE;
			}

			ResetEvent(context->socketEvent);
			ReleaseSemaphore(context->socketSemaphore, 1, NULL);

			if (context->socketClose)
				break;
		}
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
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;

	memset(rfds, 0, sizeof(rfds));

	while (1)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		select(max_fds + 1, &rfds_set, NULL, NULL, NULL);

		if (instance->CheckFileDescriptor(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
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
