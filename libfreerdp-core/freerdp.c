/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Core
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

#include "rdp.h"
#include "input.h"
#include "update.h"
#include "transport.h"
#include "connection.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/memory.h>

boolean freerdp_connect(freerdp* instance)
{
	rdpRdp* rdp;
	boolean status;

	rdp = (rdpRdp*) instance->rdp;

	IFCALL(instance->PreConnect, instance);
	status = rdp_client_connect((rdpRdp*) instance->rdp);
	IFCALL(instance->PostConnect, instance);

	return status;
}

boolean freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) instance->rdp;
	rfds[*rcount] = (void*)(long)(rdp->transport->tcp->sockfd);
	(*rcount)++;

	return True;
}

boolean freerdp_check_fds(freerdp* instance)
{
	rdpRdp* rdp;
	int status;

	rdp = (rdpRdp*) instance->rdp;

	status = rdp_check_fds(rdp);
	if (status < 0)
		return False;

	return True;
}

static int freerdp_send_channel_data(freerdp* instance, int channel_id, uint8* data, int size)
{
	return rdp_send_channel_data(instance->rdp, channel_id, data, size);
}

freerdp* freerdp_new()
{
	freerdp* instance;

	instance = xzalloc(sizeof(freerdp));

	if (instance != NULL)
	{
		rdpRdp* rdp = rdp_new(instance);
		instance->rdp = (void*) rdp;
		instance->input = rdp->input;
		instance->update = rdp->update;
		instance->settings = rdp->settings;

		instance->Connect = freerdp_connect;
		instance->GetFileDescriptor = freerdp_get_fds;
		instance->CheckFileDescriptor = freerdp_check_fds;
		instance->SendChannelData = freerdp_send_channel_data;
	}

	return instance;
}

void freerdp_free(freerdp* freerdp)
{
	xfree(freerdp);
}
