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
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <winpr/tools/makecert.h>

#include "shadow.h"

void shadow_client_context_new(freerdp_peer* client, rdpShadowClient* context)
{

}

void shadow_client_context_free(freerdp_peer* client, rdpShadowClient* context)
{

}

void shadow_client_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(rdpShadowClient);
	client->ContextNew = (psPeerContextNew) shadow_client_context_new;
	client->ContextFree = (psPeerContextFree) shadow_client_context_free;
	freerdp_peer_context_new(client);
}

BOOL shadow_client_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	return TRUE;
}

BOOL shadow_client_check_fds(freerdp_peer* client)
{
	return TRUE;
}

BOOL shadow_client_capabilities(freerdp_peer* client)
{
	return TRUE;
}

BOOL shadow_client_post_connect(freerdp_peer* client)
{
	fprintf(stderr, "Client %s is activated", client->hostname);

	if (client->settings->AutoLogonEnabled)
	{
		fprintf(stderr, " and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Client requested desktop: %dx%dx%d\n",
		client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

	if (!client->settings->RemoteFxCodec)
	{
		fprintf(stderr, "Client does not support RemoteFX\n");
		return FALSE;
	}

	//client->settings->DesktopWidth = 1024;
	//client->settings->DesktopHeight = 768;

	client->update->DesktopResize(client->update->context);

	return TRUE;
}

BOOL shadow_client_activate(freerdp_peer* client)
{
	return TRUE;
}

static const char* makecert_argv[4] =
{
	"makecert",
	"-rdp",
	"-live",
	"-silent"
};

static int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

int shadow_generate_certificate(rdpSettings* settings)
{
	char* serverFilePath;
	MAKECERT_CONTEXT* context;

	serverFilePath = GetCombinedPath(settings->ConfigPath, "server");

	if (!PathFileExistsA(serverFilePath))
		CreateDirectoryA(serverFilePath, 0);

	settings->CertificateFile = GetCombinedPath(serverFilePath, "server.crt");
	settings->PrivateKeyFile = GetCombinedPath(serverFilePath, "server.key");

	if ((!PathFileExistsA(settings->CertificateFile)) ||
			(!PathFileExistsA(settings->PrivateKeyFile)))
	{
		context = makecert_context_new();

		makecert_context_process(context, makecert_argc, (char**) makecert_argv);

		makecert_context_set_output_file_name(context, "server");

		if (!PathFileExistsA(settings->CertificateFile))
			makecert_context_output_certificate_file(context, serverFilePath);

		if (!PathFileExistsA(settings->PrivateKeyFile))
			makecert_context_output_private_key_file(context, serverFilePath);

		makecert_context_free(context);
	}

	free(serverFilePath);

	return 0;
}

void* shadow_client_thread(void* param)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE ClientEvent;
	rdpSettings* settings;
	rdpShadowClient* context;
	freerdp_peer* client = (freerdp_peer*) param;

	shadow_client_init(client);

	settings = client->settings;
	context = (rdpShadowClient*) client->context;

	shadow_generate_certificate(settings);

	settings->RemoteFxCodec = TRUE;
	settings->ColorDepth = 32;

	settings->NlaSecurity = FALSE;
	settings->TlsSecurity = TRUE;
	settings->RdpSecurity = FALSE;

	client->Capabilities = shadow_client_capabilities;
	client->PostConnect = shadow_client_post_connect;
	client->Activate = shadow_client_activate;

	shadow_input_register_callbacks(client->input);

	client->Initialize(client);

	ClientEvent = client->GetEventHandle(client);

	while (1)
	{
		nCount = 0;
		events[nCount++] = ClientEvent;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(ClientEvent, 0) == WAIT_OBJECT_0)
		{
			if (!client->CheckFileDescriptor(client))
			{
				fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
				break;
			}
		}
	}

	client->Disconnect(client);
	
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	ExitThread(0);

	return NULL;
}

void shadow_client_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE thread;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) shadow_client_thread, client, 0, NULL);
}
