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
#include <freerdp/utils/svc_plugin.h>

#include "dvcman.h"
#include "drdynvc_types.h"
#include "drdynvc_main.h"

static int drdynvc_write_variable_uint(wStream* stream, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(stream, val);
	}
	else
	{
		cb = 2;
		Stream_Write_UINT32(stream, val);
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

	data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);
	Stream_SetPosition(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

	if (data_size == 0)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x40 | cbChId);
		Stream_SetPosition(data_out, pos);
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else if (data_size <= CHANNEL_CHUNK_LENGTH - pos)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x30 | cbChId);
		Stream_SetPosition(data_out, pos);
		Stream_Write(data_out, data, data_size);
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);
	}
	else
	{
		/* Fragment the data */
		cbLen = drdynvc_write_variable_uint(data_out, data_size);
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x20 | cbChId | (cbLen << 2));
		Stream_SetPosition(data_out, pos);
		chunk_len = CHANNEL_CHUNK_LENGTH - pos;
		Stream_Write(data_out, data, chunk_len);
		data += chunk_len;
		data_size -= chunk_len;
		error = svc_plugin_send((rdpSvcPlugin*) drdynvc, data_out);

		while (error == CHANNEL_RC_OK && data_size > 0)
		{
			data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);
			Stream_SetPosition(data_out, 1);
			cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

			pos = Stream_GetPosition(data_out);
			Stream_SetPosition(data_out, 0);
			Stream_Write_UINT8(data_out, 0x30 | cbChId);
			Stream_SetPosition(data_out, pos);

			chunk_len = data_size;
			if (chunk_len > CHANNEL_CHUNK_LENGTH - pos)
				chunk_len = CHANNEL_CHUNK_LENGTH - pos;
			Stream_Write(data_out, data, chunk_len);
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

int drdynvc_push_event(drdynvcPlugin* drdynvc, wMessage* event)
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

	data_out = Stream_New(NULL, 4);
	Stream_Write_UINT16(data_out, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	Stream_Write_UINT16(data_out, drdynvc->version);
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
			Stream_Read_UINT8(stream, val);
			break;

		case 1:
			Stream_Read_UINT16(stream, val);
			break;

		default:
			Stream_Read_UINT32(stream, val);
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

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = Stream_GetPosition(s);
	DEBUG_DVC("ChannelId=%d ChannelName=%s", ChannelId, Stream_Pointer(s));

	status = dvcman_create_channel(drdynvc->channel_mgr, ChannelId, (char*) Stream_Pointer(s));

	data_out = Stream_New(NULL, pos + 4);
	Stream_Write_UINT8(data_out, 0x10 | cbChId);
	Stream_SetPosition(s, 1);
	Stream_Copy(data_out, s, pos - 1);
	
	if (status == 0)
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

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		Stream_Pointer(s), Stream_GetRemainingLength(s));
}

static int drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	DEBUG_DVC("ChannelId=%d", ChannelId);

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId,
		Stream_Pointer(s), Stream_GetRemainingLength(s));
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

	Stream_Free(s, TRUE);
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

static void drdynvc_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	freerdp_event_free(event);
}

static void drdynvc_process_terminate(rdpSvcPlugin* plugin)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) plugin;

	DEBUG_DVC("terminating");

	if (drdynvc->channel_mgr)
		dvcman_free(drdynvc->channel_mgr);

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

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	drdynvcPlugin* _p;
	DrdynvcClientContext* context;
	CHANNEL_ENTRY_POINTS_EX* pEntryPointsEx;

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

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_EX)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (DrdynvcClientContext*) malloc(sizeof(DrdynvcClientContext));

		context->handle = (void*) _p;
		_p->context = context;

		context->GetVersion = drdynvc_get_version;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
