/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP X11 Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <sys/signal.h>

#include <freerdp/utils/memory.h>

#include "xf_peer.h"
#include "xfreerdp.h"

char* xf_pcap_file = NULL;
boolean xf_pcap_dump_realtime = true;

void xf_server_main_loop(freerdp_listener* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
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

		if (select(max_fds + 1, &rfds_set, NULL, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("select failed\n");
				break;
			}
		}

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
	freerdp_listener* instance;

	/* ignore SIGPIPE, otherwise an SSL_write failure could crash the server */
	signal(SIGPIPE, SIG_IGN);

	instance = freerdp_listener_new();
	instance->PeerAccepted = xf_peer_accepted;

	if (argc > 1)
		xf_pcap_file = argv[1];

	if (argc > 2 && !strcmp(argv[2], "--fast"))
		xf_pcap_dump_realtime = false;

	/* Open the server socket and start listening. */
	if (instance->Open(instance, NULL, 3389))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		xf_server_main_loop(instance);
	}

	freerdp_listener_free(instance);

	return 0;
}

