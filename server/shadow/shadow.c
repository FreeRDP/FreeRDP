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

#ifndef _WIN32
#include <sys/select.h>
#include <sys/signal.h>
#endif

#include "shadow.h"

#ifdef WITH_X11
#define WITH_SHADOW_X11
#endif

#ifdef WITH_SHADOW_X11
extern rdpShadowSubsystem* X11_ShadowCreateSubsystem(rdpShadowServer* server);
#endif

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
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif

	if (server->listener->Open(server->listener, NULL, server->port))
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

rdpShadowServer* shadow_server_new(int argc, char** argv)
{
	rdpShadowServer* server;

	server = (rdpShadowServer*) calloc(1, sizeof(rdpShadowServer));

	if (!server)
		return NULL;

	server->port = 3389;

	server->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	server->listener = freerdp_listener_new();

	if (!server->listener)
		return NULL;

	server->listener->info = (void*) server;
	server->listener->PeerAccepted = shadow_client_accepted;

#ifdef WITH_SHADOW_X11
	server->CreateSubsystem = X11_ShadowCreateSubsystem;
#endif

	if (server->CreateSubsystem)
		server->subsystem = server->CreateSubsystem(server);

	if (!server->subsystem)
		return NULL;

	server->screen = shadow_screen_new(server);

	if (!server->screen)
		return NULL;

	server->encoder = shadow_encoder_new(server);

	if (!server->encoder)
		return NULL;

	return server;
}

void shadow_server_free(rdpShadowServer* server)
{
	if (!server)
		return;

	shadow_server_stop(server);

	freerdp_listener_free(server->listener);

	shadow_encoder_free(server->encoder);

	server->subsystem->Free(server->subsystem);

	free(server);
}

int main(int argc, char* argv[])
{
	DWORD dwExitCode;
	rdpShadowServer* server;

	server = shadow_server_new(argc, argv);

	if (!server)
		return 0;

	shadow_server_start(server);

	WaitForSingleObject(server->thread, INFINITE);

	GetExitCodeThread(server->thread, &dwExitCode);

	shadow_server_free(server);

	return 0;
}
