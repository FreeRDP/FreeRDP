/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/constants.h>
#include <winpr/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "dvcman.h"
#include "drdynvc_types.h"
#include "drdynvc_main.h"

#define CREATE_REQUEST_PDU		0x01
#define DATA_FIRST_PDU			0x02
#define DATA_PDU			0x03
#define CLOSE_REQUEST_PDU		0x04
#define CAPABILITY_REQUEST_PDU		0x05

struct drdynvc_plugin
{
	rdpSvcPlugin plugin;

	int version;
	int PriorityCharge0;
	int PriorityCharge1;
	int PriorityCharge2;
	int PriorityCharge3;
	int channel_error;

	IWTSVirtualChannelManager* channel_mgr;
};

static int drdynvc_write_variable_uint(wStream* stream, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		stream_write_BYTE(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		stream_write_UINT16(stream, val);
	}
	else
	{
		cb = 2;
		stream_write_UINT32(stream, val);
	}

	return cb;
}

int drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId, BYTE* data, UINT32 data_size)
{
	wStream* data_out;
	UINT32 pos = 0;
	UINT32 cbChId;
	UINT32 cbLen;
	UINT32 chunk_len;
	int error;

	DEBUG_DVC("ChannelId=%d size=%d", ChannelId, data_size);

	if (drdynvc->channel_error != CHANNEL_RC_OK)
		return 1;

	data_out = stream_new(CHANNEL_CHUNK_LENGTH);
	stream_set_pos(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

	if (data_size == 0)
	{
		pos = stream_get_pos(data_out);
		stream_set_pos(data_out, 0);
		stream_write_BYTE(data_out, 0x40 | cbChId);
		stream_set_pos(data_out, pos);
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else if (data_size <= CHANNEL_CHUNK_LENGTH - pos)
	{
		pos = stream_get_pos(data_out);
		stream_set_pos(data_out, 0);
		stream_write_BYTE(data_out, 0x30 | cbChId);
		stream_set_pos(data_out, pos);
		stream_write(data_out, data, data_size);
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else
	{
		/* Fragment the data */
		cbLen = drdynvc_write_variable_uint(data_out, data_size);
		pos = stream_get_pos(data_out);
		stream_set_pos(data_out, 0);
		stream_write_BYTE(data_out, 0x20 | cbChId | (cbLen << 2));
		stream_set_pos(data_out, pos);
		chunk_len = CHANNEL_CHUNK_LENGTH - pos;
		stream_write(data_out, data, chunk_len);
		data += chunk_len;
		data_size -= chunk_len;
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

		while (error == CHANNEL_RC_OK && data_size > 0)
		{
			data_out = stream_new(CHANNEL_CHUNK_LENGTH);
			stream_set_pos(data_out, 1);
			cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

			pos = stream_get_pos(data_out);
			stream_set_pos(data_out, 0);
			stream_write_BYTE(data_out, 0x30 | cbChId);
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
		drdynvc->channel_error = error;
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}

	return 0;
}

int drdynvc_push_event(drdynvcPlugin* drdynvc, RDP_EVENT* event)
{
	int error;

	error = svc_plugin_send_event((rdpSvcPlugin*) drdynvc, event);

	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("pVirtualChannelEventPush failed %d", error);
		return 1;
	}

	return 0;
}

static int drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	wStream* data_out;
	int error;

	DEBUG_DVC("Sp=%d cbChId=%d", Sp, cbChId);
	stream_seek(s, 1); /* pad */
	stream_read_UINT16(s, drdynvc->version);

	if (drdynvc->version == 2)
	{
		stream_read_UINT16(s, drdynvc->PriorityCharge0);
		stream_read_UINT16(s, drdynvc->PriorityCharge1);
		stream_read_UINT16(s, drdynvc->PriorityCharge2);
		stream_read_UINT16(s, drdynvc->PriorityCharge3);
	}

	data_out = stream_new(4);
	stream_write_UINT16(data_out, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	stream_write_UINT16(data_out, drdynvc->version);
	error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}

	drdynvc->channel_error = error;

	return 0;
}

static UINT32 drdynvc_read_variable_uint(wStream* stream, int cbLen)
{
	UINT32 val;

	switch (cbLen)
	{
		case 0:
			stream_read_BYTE(stream, val);
			break;

		case 1:
			stream_read_UINT16(stream, val);
			break;

		default:
			stream_read_UINT32(stream, val);
			break;
	}

	return val;
}

static int drdynvc_process_create_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int pos;
	int error;
	UINT32 ChannelId;
	wStream* data_out;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = stream_get_pos(s);
	DEBUG_DVC("ChannelId=%d ChannelName=%s", ChannelId, stream_get_tail(s));

	error = dvcman_create_channel(drdynvc->channel_mgr, ChannelId, (char*) stream_get_tail(s));

	data_out = stream_new(pos + 4);
	stream_write_BYTE(data_out, 0x10 | cbChId);
	stream_set_pos(s, 1);
	stream_copy(data_out, s, pos - 1);
	
	if (error == 0)
	{
		DEBUG_DVC("channel created");
		stream_write_UINT32(data_out, 0);
	}
	else
	{
		DEBUG_DVC("no listener");
		stream_write_UINT32(data_out, (UINT32)(-1));
	}

	error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}

	return 0;
}

static int drdynvc_process_data_first(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int error;
	UINT32 Length;
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	DEBUG_DVC("ChannelId=%d Length=%d", ChannelId, Length);

	error = dvcman_receive_channel_data_first(drdynvc->channel_mgr, ChannelId, Length);

	if (error)
		return error;

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		stream_get_tail(s), stream_get_left(s));
}

static int drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		stream_get_tail(s), stream_get_left(s));
}

static int drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);
	dvcman_close_channel(drdynvc->channel_mgr, ChannelId);

	return 0;
}

static void drdynvc_process_receive(rdpSvcPlugin* plugin, wStream* s)
{
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) plugin;

	stream_read_BYTE(s, value);
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
	int index;
	ADDIN_ARGV* args;
	rdpSettings* settings;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)plugin;

	DEBUG_DVC("connecting");

	drdynvc->channel_mgr = dvcman_new(drdynvc);
	drdynvc->channel_error = 0;

	settings = (rdpSettings*) ((rdpSvcPlugin*) plugin)->channel_entry_points.pExtendedData;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		args = settings->DynamicChannelArray[index];
		dvcman_load_addin(drdynvc->channel_mgr, args);
	}

	dvcman_init(drdynvc->channel_mgr);
}

static void drdynvc_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	freerdp_event_free(event);
}

static void drdynvc_process_terminate(rdpSvcPlugin* plugin)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) plugin;

	DEBUG_DVC("terminating");

	if (drdynvc->channel_mgr != NULL)
		dvcman_free(drdynvc->channel_mgr);

	free(drdynvc);
}

/* drdynvc is always built-in */
#define VirtualChannelEntry	drdynvc_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	drdynvcPlugin* _p;

	_p = (drdynvcPlugin*) malloc(sizeof(drdynvcPlugin));
	ZeroMemory(_p, sizeof(drdynvcPlugin));

	_p->plugin.channel_def.options =
		CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP |
		CHANNEL_OPTION_COMPRESS_RDP;

	strcpy(_p->plugin.channel_def.name, "drdynvc");

	_p->plugin.connect_callback = drdynvc_process_connect;
	_p->plugin.receive_callback = drdynvc_process_receive;
	_p->plugin.event_callback = drdynvc_process_event;
	_p->plugin.terminate_callback = drdynvc_process_terminate;

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
