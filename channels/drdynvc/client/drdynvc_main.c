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
#include <winpr/stream.h>

#include <freerdp/constants.h>

#include "dvcman.h"
#include "drdynvc_types.h"
#include "drdynvc_main.h"

static int drdynvc_write_variable_uint(wStream* s, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(s, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(s, val);
	}
	else
	{
		cb = 2;
		Stream_Write_UINT32(s, val);
	}

	return cb;
}

int drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId, BYTE* data, UINT32 dataSize)
{
	wStream* data_out;
	UINT32 pos = 0;
	UINT32 cbChId;
	UINT32 cbLen;
	UINT32 chunkLength;
	int status;

	DEBUG_DVC("ChannelId=%d size=%d", ChannelId, dataSize);

	if (drdynvc->channel_error != CHANNEL_RC_OK)
		return 1;

	data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);
	Stream_SetPosition(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

	if (dataSize == 0)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x40 | cbChId);
		Stream_SetPosition(data_out, pos);
		status = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else if (dataSize <= CHANNEL_CHUNK_LENGTH - pos)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x30 | cbChId);
		Stream_SetPosition(data_out, pos);
		Stream_Write(data_out, data, dataSize);
		status = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else
	{
		/* Fragment the data */
		cbLen = drdynvc_write_variable_uint(data_out, dataSize);
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x20 | cbChId | (cbLen << 2));
		Stream_SetPosition(data_out, pos);
		chunkLength = CHANNEL_CHUNK_LENGTH - pos;
		Stream_Write(data_out, data, chunkLength);
		data += chunkLength;
		dataSize -= chunkLength;
		status = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

		while (status == CHANNEL_RC_OK && dataSize > 0)
		{
			data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);
			Stream_SetPosition(data_out, 1);
			cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

			pos = Stream_GetPosition(data_out);
			Stream_SetPosition(data_out, 0);
			Stream_Write_UINT8(data_out, 0x30 | cbChId);
			Stream_SetPosition(data_out, pos);

			chunkLength = dataSize;
			if (chunkLength > CHANNEL_CHUNK_LENGTH - pos)
				chunkLength = CHANNEL_CHUNK_LENGTH - pos;
			Stream_Write(data_out, data, chunkLength);
			data += chunkLength;
			dataSize -= chunkLength;
			status = svc_plugin_send((rdpSvcPlugin*)drdynvc, data_out);
		}
	}

	if (status != CHANNEL_RC_OK)
	{
		drdynvc->channel_error = status;
		DEBUG_WARN("VirtualChannelWrite failed %d", status);
		return 1;
	}

	return 0;
}

int drdynvc_push_event(drdynvcPlugin* drdynvc, wMessage* event)
{
	int status;

	status = svc_plugin_send_event((rdpSvcPlugin*) drdynvc, event);

	if (status != CHANNEL_RC_OK)
	{
		DEBUG_WARN("pVirtualChannelEventPush failed %d", status);
		return 1;
	}

	return 0;
}

static int drdynvc_send_capability_response(drdynvcPlugin* drdynvc)
{
	int status;
	wStream* s;

	s = Stream_New(NULL, 4);
	Stream_Write_UINT16(s, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	Stream_Write_UINT16(s, drdynvc->version);

	status = svc_plugin_send((rdpSvcPlugin*) drdynvc, s);

	if (status != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", status);
		return 1;
	}

	return status;
}

static int drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int status;

	DEBUG_DVC("Sp=%d cbChId=%d", Sp, cbChId);

	Stream_Seek(s, 1); /* pad */
	Stream_Read_UINT16(s, drdynvc->version);

	/* RDP8 servers offer version 3, though Microsoft forgot to document it
	 * in their early documents.  It behaves the same as version 2.
	 */
	if ((drdynvc->version == 2) || (drdynvc->version == 3))
	{
		Stream_Read_UINT16(s, drdynvc->PriorityCharge0);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge1);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge2);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge3);
	}

	status = drdynvc_send_capability_response(drdynvc);

	drdynvc->channel_error = status;

	drdynvc->state = DRDYNVC_STATE_READY;

	return 0;
}

static UINT32 drdynvc_read_variable_uint(wStream* s, int cbLen)
{
	UINT32 val;

	switch (cbLen)
	{
		case 0:
			Stream_Read_UINT8(s, val);
			break;

		case 1:
			Stream_Read_UINT16(s, val);
			break;

		default:
			Stream_Read_UINT32(s, val);
			break;
	}

	return val;
}

static int drdynvc_process_create_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int pos;
	int status;
	UINT32 ChannelId;
	wStream* data_out;
	int channel_status;

	if (drdynvc->state == DRDYNVC_STATE_CAPABILITIES)
	{
		/**
		 * For some reason the server does not always send the
		 * capabilities pdu as it should. When this happens,
		 * send a capabilities response.
		 */

		drdynvc->version = 3;
		drdynvc_send_capability_response(drdynvc);
		drdynvc->state = DRDYNVC_STATE_READY;
	}

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = Stream_GetPosition(s);
	DEBUG_DVC("ChannelId=%d ChannelName=%s", ChannelId, Stream_Pointer(s));

	channel_status = dvcman_create_channel(drdynvc->channel_mgr, ChannelId, (char*) Stream_Pointer(s));

	data_out = Stream_New(NULL, pos + 4);
	Stream_Write_UINT8(data_out, 0x10 | cbChId);
	Stream_SetPosition(s, 1);
	Stream_Copy(data_out, s, pos - 1);
	
	if (channel_status == 0)
	{
		DEBUG_DVC("channel created");
		Stream_Write_UINT32(data_out, 0);
	}
	else
	{
		DEBUG_DVC("no listener");
		Stream_Write_UINT32(data_out, (UINT32)(-1));
	}

	status = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

	if (status != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", status);
		return 1;
	}

	if (channel_status == 0)
	{
		dvcman_open_channel(drdynvc->channel_mgr, ChannelId);
	}

	return 0;
}

static int drdynvc_process_data_first(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int status;
	UINT32 Length;
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	DEBUG_DVC("ChannelId=%d Length=%d", ChannelId, Length);

	status = dvcman_receive_channel_data_first(drdynvc->channel_mgr, ChannelId, Length);

	if (status)
		return status;

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

static int drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

static int drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;
	wStream* data_out;
	int value;
	int error;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);
	dvcman_close_channel(drdynvc->channel_mgr, ChannelId);
	
	data_out = Stream_New(NULL, 4);
	value = (CLOSE_REQUEST_PDU << 4) | (cbChId & 0x03);
	Stream_Write_UINT8(data_out, value);
	drdynvc_write_variable_uint(data_out, ChannelId);
	error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	
	if (error != CHANNEL_RC_OK)
	{
		DEBUG_WARN("VirtualChannelWrite failed %d", error);
		return 1;
	}
	
	drdynvc->channel_error = error;

	return 0;
}

static void drdynvc_process_receive(rdpSvcPlugin* plugin, wStream* s)
{
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) plugin;

	Stream_Read_UINT8(s, value);
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
}

static void drdynvc_process_connect(rdpSvcPlugin* plugin)
{
	UINT32 index;
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
		dvcman_load_addin(drdynvc->channel_mgr, args, settings);
	}

	dvcman_init(drdynvc->channel_mgr);

	drdynvc->state = DRDYNVC_STATE_CAPABILITIES;
}

static void drdynvc_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	freerdp_event_free(event);
}

static void drdynvc_process_terminate(rdpSvcPlugin* plugin)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) plugin;

	DEBUG_DVC("terminating");

	if (!drdynvc)
		return;

	if (drdynvc->channel_mgr)
	{
		dvcman_free(drdynvc->channel_mgr);
		drdynvc->channel_mgr = NULL;
	}

	svc_plugin_terminate(plugin);

	free(drdynvc);
}

/**
 * Channel Client Interface
 */

int drdynvc_get_version(DrdynvcClientContext* context)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) context->handle;
	return drdynvc->version;
}

/* drdynvc is always built-in */
#define VirtualChannelEntry	drdynvc_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	drdynvcPlugin* _p;
	DrdynvcClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	_p = (drdynvcPlugin*) calloc(1, sizeof(drdynvcPlugin));

	if (!_p)
		return FALSE;

	_p->plugin.channel_def.options =
		CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP |
		CHANNEL_OPTION_COMPRESS_RDP;

	strcpy(_p->plugin.channel_def.name, "drdynvc");

	_p->state = DRDYNVC_STATE_INITIAL;

	_p->plugin.connect_callback = drdynvc_process_connect;
	_p->plugin.receive_callback = drdynvc_process_receive;
	_p->plugin.event_callback = drdynvc_process_event;
	_p->plugin.terminate_callback = drdynvc_process_terminate;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (DrdynvcClientContext*) calloc(1, sizeof(DrdynvcClientContext));

		if (!context)
			return -1;

		context->handle = (void*) _p;
		_p->context = context;

		context->GetVersion = drdynvc_get_version;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}

