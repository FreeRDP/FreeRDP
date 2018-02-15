/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <CoreGraphics/CGEvent.h>

#include <winpr/crt.h>
#include <winpr/wtsapi.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include "mfreerdp.h"
#include "mf_peer.h"

static void mf_server_main_loop(freerdp_listener* instance)
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

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != TRUE)
		{
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
				break;
			}
		}

		if (instance->CheckFileDescriptor(instance) != TRUE)
		{
			break;
		}
	}

	instance->Close(instance);
}

int main(int argc, char* argv[])
{
	freerdp_listener* instance;

	signal(SIGPIPE, SIG_IGN);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	
	if (!(instance = freerdp_listener_new()))
		return winpr_exit(1);

	instance->PeerAccepted = mf_peer_accepted;

	if (instance->Open(instance, NULL, 3389))
	{
		mf_server_main_loop(instance);
	}

	freerdp_listener_free(instance);

	return winpr_exit(0);
}
