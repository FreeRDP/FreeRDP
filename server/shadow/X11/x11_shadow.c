/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>
#include <sys/signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "x11_shadow.h"

void* x11_shadow_server_thread(void* param)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	x11ShadowServer* server;
	freerdp_listener* listener;

	server = (x11ShadowServer*) param;
	listener = server->listener;

	while (1)
	{
		rcount = 0;

		ZeroMemory(rfds, sizeof(rfds));
		if (listener->GetFileDescriptor(listener, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
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

		if (select(max_fds + 1, &rfds_set, NULL, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				fprintf(stderr, "select failed\n");
				break;
			}
		}

		if (listener->CheckFileDescriptor(listener) != TRUE)
		{
			fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	ExitThread(0);

	return NULL;
}

int x11_shadow_server_start(x11ShadowServer* server)
{
	server->thread = NULL;

	if (server->listener->Open(server->listener, NULL, 3389))
	{
		server->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
				x11_shadow_server_thread, (void*) server, 0, NULL);
	}

	return 0;
}

int x11_shadow_server_stop(x11ShadowServer* server)
{
	if (server->thread)
	{
		TerminateThread(server->thread, 0);
		WaitForSingleObject(server->thread, INFINITE);
		CloseHandle(server->thread);
	
		server->listener->Close(server->listener);
	}
	return 0;
}

HANDLE x11_shadow_server_get_thread(x11ShadowServer* server)
{
	return server->thread;
}

x11ShadowServer* x11_shadow_server_new(int argc, char** argv)
{
	x11ShadowServer* server;

	server = (x11ShadowServer*) malloc(sizeof(x11ShadowServer));

	if (server)
	{
		server->listener = freerdp_listener_new();
		server->listener->PeerAccepted = x11_shadow_peer_accepted;
	}

	signal(SIGPIPE, SIG_IGN);

	return server;
}

void x11_shadow_server_free(x11ShadowServer* server)
{
	if (server)
	{
		freerdp_listener_free(server->listener);
		free(server);
	}
}
