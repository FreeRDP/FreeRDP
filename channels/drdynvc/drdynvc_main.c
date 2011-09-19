/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Dynamic Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/wait_obj.h>

#include "dvcman.h"
#include "drdynvc_types.h"
#include "drdynvc_main.h"

#define CREATE_REQUEST_PDU     0x01
#define DATA_FIRST_PDU         0x02
#define DATA_PDU               0x03
#define CLOSE_REQUEST_PDU      0x04
#define CAPABILITY_REQUEST_PDU 0x05

struct drdynvc_plugin
{
	rdpSvcPlugin plugin;

	int version;
	int PriorityCharge0;
	int PriorityCharge1;
	int PriorityCharge2;
	int PriorityCharge3;

	IWTSVirtualChannelManager* channel_mgr;
};

static int drdynvc_write_variable_uint(STREAM* stream, uint32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		stream_write_uint8(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		stream_write_uint16(stream, val);
	}
	else
	{
		cb = 3;
		stream_write_uint32(stream, val);
	}
	return cb;
}

int drdynvc_write_data(drdynvcPlugin* drdynvc, uint32 ChannelId, uint8* data, uint32 data_size)
{
	STREAM* data_out;
	uint32 pos = 0;
	uint32 cbChId;
	uint32 cbLen;
	uint32 chunk_len;
	int error;

	DEBUG_DVC("ChannelId=%d size=%d", ChannelId, data_size);

	data_out = stream_new(CHANNEL_CHUNK_LENGTH);
	stream_set_pos(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

	if (data_size <= CHANNEL_CHUNK_LENGTH - pos)
	{
		pos = stream_get_pos(data_out);
		stream_set_pos(data_out, 0);
		stream_write_uint8(data_out, 0x30 | cbChId);
		stream_set_pos(data_out, pos);
		stream_write(data_out, data, data_size);
		error = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);
	}
	else
	{
		/* Fragment the data */
		cbLen = drdynvc_write_variable_uint(data_out, data_size);
		pos = stream_get_pos(data_out);
		stream_set_pos(data_out, 0);
		stream_write_uint8(data_out, 0x20 | cbChId | (cbLen << 2));
		stream_set_pos(data_out, pos);
		chunk_len = CHANNEL_CHUNK_LENGTH - pos;
		stream_write(data_out, data, chunk_len);
		data += chunk_len;
		data_size -= chunk_len;
		error = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);

		while (error == CHANNEL_RC_OK && data_size > 0)
		{
			data_out = stream_new(CHANNEL_CHUNK_LENGTH);
			stream_set_pos(data_out, 1);
			cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

			pos = stream_get_pos(data_out);
			stream_set_pos(data_out, 0);
			stream_write_uint8(data_out, 0x30 | cbChId);
			stream_set_pos(data_out, pos);

			chunk_len = data_size;
			if (chunk_len > CHANNEL_CHUNK_LENGTH - pos)
				chunk_len = CHANNEL_CHUNK_LENGTH - pos;
			stream_write(data_out, data, chunk_len);
			data += chunk_len;
			data_size -= chunk_len;
			error = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);
		}
	}
	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}
	return 0;
}

int drdynvc_push_event(drdynvcPlugin* drdynvc, RDP_EVENT* event)
{
	int error;

	error = svc_plugin_send_event((rdpSvcPlugin*)drdynvc, event);
	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("pVirtualChannelEventPush failed %d", error);
		return 1;
	}
	return 0;
}

static int drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, STREAM* s)
{
	STREAM* data_out;
	int error;

	DEBUG_DVC("Sp=%d cbChId=%d", Sp, cbChId);
	stream_seek(s, 1); /* pad */
	stream_read_uint16(s, drdynvc->version);
	if (drdynvc->version == 2)
	{
		stream_read_uint16(s, drdynvc->PriorityCharge0);
		stream_read_uint16(s, drdynvc->PriorityCharge1);
		stream_read_uint16(s, drdynvc->PriorityCharge2);
		stream_read_uint16(s, drdynvc->PriorityCharge3);
	}
	data_out = stream_new(4);
	stream_write_uint16(data_out, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	stream_write_uint16(data_out, drdynvc->version);
	error = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);
	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}
	return 0;
}

static uint32 drdynvc_read_variable_uint(STREAM* stream, int cbLen)
{
	uint32 val;

	switch (cbLen)
	{
		case 0:
			stream_read_uint8(stream, val);
			break;
		case 1:
			stream_read_uint16(stream, val);
			break;
		default:
			stream_read_uint32(stream, val);
			break;
	}
	return val;
}

static int drdynvc_process_create_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, STREAM* s)
{
	STREAM* data_out;
	int pos;
	int error;
	uint32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = stream_get_pos(s);
	DEBUG_DVC("ChannelId=%d ChannelName=%s", ChannelId, stream_get_tail(s));

	error = dvcman_create_channel(drdynvc->channel_mgr, ChannelId, (char*)stream_get_tail(s));

	data_out = stream_new(pos + 4);
	stream_write_uint8(data_out, 0x10 | cbChId);
	stream_set_pos(s, 1);
	stream_copy(data_out, s, pos - 1);
	
	if (error == 0)
	{
		DEBUG_DVC("channel created");
		stream_write_uint32(data_out, 0);
	}
	else
	{
		DEBUG_DVC("no listener");
		stream_write_uint32(data_out, (uint32)(-1));
	}

	error = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);
	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}
	return 0;
}

static int drdynvc_process_data_first(drdynvcPlugin* drdynvc, int Sp, int cbChId, STREAM* s)
{
	uint32 ChannelId;
	uint32 Length;
	int error;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	DEBUG_DVC("ChannelId=%d Length=%d", ChannelId, Length);

	error = dvcman_receive_channel_data_first(drdynvc->channel_mgr, ChannelId, Length);
	if (error)
		return error;

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		stream_get_tail(s), stream_get_left(s));
}

static int drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, STREAM* s)
{
	uint32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		stream_get_tail(s), stream_get_left(s));
}

static int drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, STREAM* s)
{
	uint32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);
	dvcman_close_channel(drdynvc->channel_mgr, ChannelId);

	return 0;
}

static void drdynvc_process_receive(rdpSvcPlugin* plugin, STREAM* s)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)plugin;
	int value;
	int Cmd;
	int Sp;
	int cbChId;

	stream_read_uint8(s, value);
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;

	DEBUG_DVC("Cmd=0x%x", Cmd);

	switch (Cmd)
	{
		case CAPABILITY_REQUEST_PDU:
			drdynvc_process_capability_request(drdynvc, Sp, cbChId, s);
			break;
		case CREATE_REQUEST_PDU:
			drdynvc_process_create_request(drdynvc, Sp, cbChId, s);
			break;
		case DATA_FIRST_PDU:
			drdynvc_process_data_first(drdynvc, Sp, cbChId, s);
			break;
		case DATA_PDU:
			drdynvc_process_data(drdynvc, Sp, cbChId, s);
			break;
		case CLOSE_REQUEST_PDU:
			drdynvc_process_close_request(drdynvc, Sp, cbChId, s);
			break;
		default:
			DEBUG_WARN("unknown drdynvc cmd 0x%x", Cmd);
			break;
	}

	stream_free(s);
}

static void drdynvc_process_connect(rdpSvcPlugin* plugin)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)plugin;

	DEBUG_DVC("connecting");

	drdynvc->channel_mgr = dvcman_new(drdynvc);
	dvcman_load_plugin(drdynvc->channel_mgr, svc_plugin_get_data(plugin));
	dvcman_init(drdynvc->channel_mgr);
}

static void drdynvc_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	freerdp_event_free(event);
}

static void drdynvc_process_terminate(rdpSvcPlugin* plugin)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)plugin;

	DEBUG_DVC("terminating");

	if (drdynvc->channel_mgr != NULL)
		dvcman_free(drdynvc->channel_mgr);
	xfree(drdynvc);
}

DEFINE_SVC_PLUGIN(drdynvc, "drdynvc",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP)
