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

#include <freerdp/config.h>

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
#include <winpr/assert.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include "mfreerdp.h"
#include "mf_peer.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

static void mf_server_main_loop(freerdp_listener* instance)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->GetEventHandles);
	WINPR_ASSERT(instance->CheckFileDescriptor);

	while (1)
	{
		DWORD status;
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		DWORD count = instance->GetEventHandles(instance, handles, ARRAYSIZE(handles));

		if (count == 0)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed");
			break;
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
		return 1;

	instance->PeerAccepted = mf_peer_accepted;

	if (instance->Open(instance, NULL, 3389))
	{
		mf_server_main_loop(instance);
	}

	freerdp_listener_free(instance);

	return 0;
}
