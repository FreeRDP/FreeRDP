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
#include "surface.h"
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

	if (status)
	{
		if (instance->settings->dump_rfx)
		{
			instance->update->dump_rfx = instance->settings->dump_rfx;
			instance->update->pcap_rfx = pcap_open(instance->settings->dump_rfx_file, True);
		}

		IFCALL(instance->PostConnect, instance);

		if (instance->settings->play_rfx)
		{
			STREAM* s;
			rdpUpdate* update;
			pcap_record record;

			s = stream_new(1024);
			instance->update->play_rfx = instance->settings->play_rfx;
			instance->update->pcap_rfx = pcap_open(instance->settings->play_rfx_file, False);
			update = instance->update;

			while (pcap_has_next_record(update->pcap_rfx))
			{
				pcap_get_next_record_header(update->pcap_rfx, &record);

				s->data = xrealloc(s->data, record.length);
				record.data = s->data;
				s->size = record.length;

				pcap_get_next_record_content(update->pcap_rfx, &record);
				stream_set_pos(s, 0);

				update->BeginPaint(update);
				update_recv_surfcmds(update, s->size, s);
				update->EndPaint(update);
			}

			xfree(s->data);
			return True;
		}
	}

	return status;
}

boolean freerdp_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) instance->rdp;
	transport_get_fds(rdp->transport, rfds, rcount);

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

void freerdp_disconnect(freerdp* instance)
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) instance->rdp;
	transport_disconnect(rdp->transport);
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
		instance->Disconnect = freerdp_disconnect;

		input_register_client_callbacks(rdp->input);
	}

	return instance;
}

void freerdp_free(freerdp* freerdp)
{
	if (freerdp)
	{
		rdp_free(freerdp->rdp);
		xfree(freerdp);
	}
}
