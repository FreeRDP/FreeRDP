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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "drdynvc_main.h"

#define TAG CHANNELS_TAG("drdynvc.client")

static void dvcman_channel_free(void* channel);

static int dvcman_get_configuration(IWTSListener* pListener, void** ppPropertyBag)
{
	*ppPropertyBag = NULL;
	return 1;
}

static int dvcman_create_listener(IWTSVirtualChannelManager* pChannelMgr,
	const char* pszChannelName, UINT32 ulFlags,
	IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;
	DVCMAN_LISTENER* listener;

	if (dvcman->num_listeners < MAX_PLUGINS)
	{
		WLog_DBG(TAG, "create_listener: %d.%s.", dvcman->num_listeners, pszChannelName);

		listener = (DVCMAN_LISTENER*) calloc(1, sizeof(DVCMAN_LISTENER));
		if (!listener)
		{
			WLog_WARN(TAG, "create_listener: unable to allocate listener.");
			return 1;
		}

		listener->iface.GetConfiguration = dvcman_get_configuration;
		listener->iface.pInterface = NULL;

		listener->dvcman = dvcman;
		listener->channel_name = _strdup(pszChannelName);
		if (!listener->channel_name)
			goto error_strdup;
		listener->flags = ulFlags;
		listener->listener_callback = pListenerCallback;

		if (ppListener)
			*ppListener = (IWTSListener*) listener;

		dvcman->listeners[dvcman->num_listeners++] = (IWTSListener*) listener;

		return 0;
	}
	else
	{
		WLog_WARN(TAG, "create_listener: Maximum DVC listener number reached.");
		return 1;
	}

error_strdup:
	free(listener);
	return 1;
}

static int dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name, IWTSPlugin* pPlugin)
{
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	if (dvcman->num_plugins < MAX_PLUGINS)
	{
		dvcman->plugin_names[dvcman->num_plugins] = name;
		dvcman->plugins[dvcman->num_plugins++] = pPlugin;
		WLog_DBG(TAG, "register_plugin: num_plugins %d", dvcman->num_plugins);
		return 0;
	}
	else
	{
		WLog_WARN(TAG, "register_plugin: Maximum DVC plugin number reached.");
		return 1;
	}
}

IWTSPlugin* dvcman_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name)
{
	int i;
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		if (dvcman->plugin_names[i] == name ||
			strcmp(dvcman->plugin_names[i], name) == 0)
		{
			return dvcman->plugins[i];
		}
	}

	return NULL;
}

ADDIN_ARGV* dvcman_get_plugin_data(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->args;
}

void* dvcman_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return (void*) ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->settings;
}

UINT32 dvcman_get_channel_id(IWTSVirtualChannel * channel)
{
	return ((DVCMAN_CHANNEL*) channel)->channel_id;
}

IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	int index;
	BOOL found = FALSE;
	DVCMAN_CHANNEL* channel;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	ArrayList_Lock(dvcman->channels);

	index = 0;
	channel = (DVCMAN_CHANNEL*) ArrayList_GetItem(dvcman->channels, index++);

	while (channel)
	{
		if (channel->channel_id == ChannelId)
		{
			found = TRUE;
			break;
		}

		channel = (DVCMAN_CHANNEL*) ArrayList_GetItem(dvcman->channels, index++);
	}

	ArrayList_Unlock(dvcman->channels);

	return (found) ? ((IWTSVirtualChannel*) channel) : NULL;
}

void* dvcman_get_channel_interface_by_name(IWTSVirtualChannelManager* pChannelMgr, const char* ChannelName)
{
	int i;
	BOOL found = FALSE;
	void* pInterface = NULL;
	DVCMAN_LISTENER* listener;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			pInterface = listener->iface.pInterface;
			found = TRUE;
			break;
		}
	}

	return (found) ? pInterface : NULL;
}

IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	DVCMAN* dvcman;

	dvcman = (DVCMAN*) calloc(1, sizeof(DVCMAN));
	if (!dvcman)
		return NULL;

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->drdynvc = plugin;
	dvcman->channels = ArrayList_New(TRUE);
	if (!dvcman->channels)
		goto error_channels;
	dvcman->channels->object.fnObjectFree = dvcman_channel_free;
	dvcman->pool = StreamPool_New(TRUE, 10);
	if (!dvcman->pool)
		goto error_pool;

	return (IWTSVirtualChannelManager*) dvcman;

error_pool:
	ArrayList_Free(dvcman->channels);
error_channels:
	free(dvcman);
	return NULL;
}

int dvcman_load_addin(IWTSVirtualChannelManager* pChannelMgr, ADDIN_ARGV* args, rdpSettings* settings)
{
	DVCMAN_ENTRY_POINTS entryPoints;
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;

	WLog_INFO(TAG, "Loading Dynamic Virtual Channel %s", args->argv[0]);

	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY) freerdp_load_channel_addin_entry(args->argv[0],
			NULL, NULL, FREERDP_ADDIN_CHANNEL_DYNAMIC);

	if (pDVCPluginEntry)
	{
		entryPoints.iface.RegisterPlugin = dvcman_register_plugin;
		entryPoints.iface.GetPlugin = dvcman_get_plugin;
		entryPoints.iface.GetPluginData = dvcman_get_plugin_data;
		entryPoints.iface.GetRdpSettings = dvcman_get_rdp_settings;
		entryPoints.dvcman = (DVCMAN*) pChannelMgr;
		entryPoints.args = args;
		entryPoints.settings = settings;

		pDVCPluginEntry((IDRDYNVC_ENTRY_POINTS*) &entryPoints);
	}

	return 0;
}

static DVCMAN_CHANNEL* dvcman_channel_new(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, const char* ChannelName)
{
	DVCMAN_CHANNEL* channel;

	if (dvcman_find_channel_by_id(pChannelMgr, ChannelId))
	{
		WLog_ERR(TAG, "Protocol error: Duplicated ChannelId %d (%s)!", ChannelId, ChannelName);
		return NULL;
	}

	channel = (DVCMAN_CHANNEL*) calloc(1, sizeof(DVCMAN_CHANNEL));
	if (!channel)
		return NULL;

	channel->dvcman = (DVCMAN*) pChannelMgr;
	channel->channel_id = ChannelId;
	channel->channel_name = _strdup(ChannelName);

	InitializeCriticalSection(&(channel->lock));

	return channel;
}

void dvcman_channel_free(void* arg)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) arg;

	if (channel->channel_callback)
	{
		channel->channel_callback->OnClose(channel->channel_callback);
		channel->channel_callback = NULL;
	}

	if (channel->dvc_data)
	{
		Stream_Release(channel->dvc_data);
		channel->dvc_data = NULL;
	}

	DeleteCriticalSection(&(channel->lock));

	if (channel->channel_name)
	{
		free(channel->channel_name);
		channel->channel_name = NULL;
	}

	free(channel);
}

void dvcman_free(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN_LISTENER* listener;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	ArrayList_Free(dvcman->channels);

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];
		free(listener->channel_name);
		free(listener);
	}

	dvcman->num_listeners = 0;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Terminated)
			pPlugin->Terminated(pPlugin);
	}

	dvcman->num_plugins = 0;

	StreamPool_Free(dvcman->pool);

	free(dvcman);
}

int dvcman_init(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Initialize)
			pPlugin->Initialize(pPlugin, pChannelMgr);
	}

	return 0;
}

static int dvcman_write_channel(IWTSVirtualChannel* pChannel, UINT32 cbSize, BYTE* pBuffer, void* pReserved)
{
	int status;
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;

	EnterCriticalSection(&(channel->lock));

	status = drdynvc_write_data(channel->dvcman->drdynvc, channel->channel_id, pBuffer, cbSize);

	LeaveCriticalSection(&(channel->lock));

	return status;
}

static int dvcman_close_channel_iface(IWTSVirtualChannel* pChannel)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;

	WLog_DBG(TAG, "close_channel_iface: id=%d", channel->channel_id);

	return 1;
}

int dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, const char* ChannelName)
{
	int i;
	int bAccept;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	DrdynvcClientContext* context;
	IWTSVirtualChannelCallback* pCallback;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	channel = dvcman_channel_new(pChannelMgr, ChannelId, ChannelName);
	if (!channel)
		return 1;

	channel->status = 1;
	if (ArrayList_Add(dvcman->channels, channel) < 0)
		return 1;

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			channel->iface.Write = dvcman_write_channel;
			channel->iface.Close = dvcman_close_channel_iface;

			bAccept = 1;
			pCallback = NULL;

			if (listener->listener_callback->OnNewChannelConnection(listener->listener_callback,
				(IWTSVirtualChannel*) channel, NULL, &bAccept, &pCallback) == 0 && bAccept == 1)
			{
				WLog_DBG(TAG, "listener %s created new channel %d",
					  listener->channel_name, channel->channel_id);

				channel->status = 0;
				channel->channel_callback = pCallback;
				channel->pInterface = listener->iface.pInterface;

				context = dvcman->drdynvc->context;
				IFCALL(context->OnChannelConnected, context, ChannelName, listener->iface.pInterface);

				return 0;
			}
		}
	}

	return 1;
}

int dvcman_open_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannelCallback* pCallback;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->status == 0)
	{
		pCallback = channel->channel_callback;
		if (pCallback->OnOpen)
			pCallback->OnOpen(pCallback);
		WLog_DBG(TAG, "open_channel: ChannelId %d", ChannelId);
	}

	return 0;
}

int dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannel* ichannel;
	DrdynvcClientContext* context;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->status == 0)
	{
		context = dvcman->drdynvc->context;

		IFCALL(context->OnChannelDisconnected, context, channel->channel_name, channel->pInterface);

		WLog_DBG(TAG, "dvcman_close_channel: channel %d closed", ChannelId);

		ichannel = (IWTSVirtualChannel*) channel;

		if (ichannel->Close)
			ichannel->Close(ichannel);
	}

	ArrayList_Remove(dvcman->channels, channel);

	return 0;
}

int dvcman_receive_channel_data_first(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, UINT32 length)
{
	DVCMAN_CHANNEL* channel;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	channel->dvc_data = StreamPool_Take(channel->dvcman->pool, length);
	if (!channel->dvc_data)
		return 1;
	channel->dvc_data_length = length;

	return 0;
}

int dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, wStream* data)
{
	int status = 0;
	DVCMAN_CHANNEL* channel;
	UINT32 dataSize = Stream_GetRemainingLength(data);

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
	{
		/* Fragmented data */
		if (Stream_GetPosition(channel->dvc_data) + dataSize > (UINT32) Stream_Capacity(channel->dvc_data))
		{
			WLog_ERR(TAG, "data exceeding declared length!");
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
			return 1;
		}

		Stream_Write(channel->dvc_data, Stream_Pointer(data), dataSize);

		if (((size_t) Stream_GetPosition(channel->dvc_data)) >= channel->dvc_data_length)
		{
			Stream_SealLength(channel->dvc_data);
			Stream_SetPosition(channel->dvc_data, 0);
			status = channel->channel_callback->OnDataReceived(channel->channel_callback, channel->dvc_data);
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
		}
	}
	else
	{
		status = channel->channel_callback->OnDataReceived(channel->channel_callback, data);
	}

	return status;
}

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

int drdynvc_send(drdynvcPlugin* drdynvc, wStream* s)
{
	UINT32 status = 0;

	if (!drdynvc)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = drdynvc->channelEntryPoints.pVirtualChannelWrite(drdynvc->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	return status;
}

int drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId, BYTE* data, UINT32 dataSize)
{
	wStream* data_out;
	UINT32 pos = 0;
	UINT32 cbChId;
	UINT32 cbLen;
	UINT32 chunkLength;
	int status;

	WLog_DBG(TAG, "write_data: ChannelId=%d size=%d", ChannelId, dataSize);

	if (drdynvc->channel_error != CHANNEL_RC_OK)
		return 1;

	data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);
	if (!data_out)
		return 1;
	Stream_SetPosition(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);

	if (dataSize == 0)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x40 | cbChId);
		Stream_SetPosition(data_out, pos);

		status = drdynvc_send(drdynvc, data_out);
	}
	else if (dataSize <= CHANNEL_CHUNK_LENGTH - pos)
	{
		pos = Stream_GetPosition(data_out);
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x30 | cbChId);
		Stream_SetPosition(data_out, pos);
		Stream_Write(data_out, data, dataSize);

		status = drdynvc_send(drdynvc, data_out);
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

		status = drdynvc_send(drdynvc, data_out);

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

			status = drdynvc_send(drdynvc, data_out);
		}
	}

	if (status != CHANNEL_RC_OK)
	{
		drdynvc->channel_error = status;
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return 1;
	}

	return 0;
}

static int drdynvc_send_capability_response(drdynvcPlugin* drdynvc)
{
	int status;
	wStream* s;

	WLog_DBG(TAG, "capability_response");
	s = Stream_New(NULL, 4);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT16(s, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	Stream_Write_UINT16(s, drdynvc->version);

	status = drdynvc_send(drdynvc, s);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return 1;
	}

	return status;
}

static int drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int status;

	WLog_DBG(TAG, "capability_request Sp=%d cbChId=%d", Sp, cbChId);

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
	WLog_DBG(TAG, "process_create_request: ChannelId=%d ChannelName=%s", ChannelId, Stream_Pointer(s));

	channel_status = dvcman_create_channel(drdynvc->channel_mgr, ChannelId, (char*) Stream_Pointer(s));

	data_out = Stream_New(NULL, pos + 4);
	Stream_Write_UINT8(data_out, 0x10 | cbChId);
	Stream_SetPosition(s, 1);
	Stream_Copy(data_out, s, pos - 1);
	
	if (channel_status == 0)
	{
		WLog_DBG(TAG, "channel created");
		Stream_Write_UINT32(data_out, 0);
	}
	else
	{
		WLog_DBG(TAG, "no listener");
		Stream_Write_UINT32(data_out, (UINT32)(-1));
		dvcman_close_channel(drdynvc->channel_mgr, ChannelId);
	}

	status = drdynvc_send(drdynvc, data_out);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
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
	WLog_DBG(TAG, "process_data_first: Sp=%d cbChId=%d, ChannelId=%d Length=%d", Sp, cbChId, ChannelId, Length);

	status = dvcman_receive_channel_data_first(drdynvc->channel_mgr, ChannelId, Length);

	if (status)
		return status;

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

static int drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_DBG(TAG, "process_data: Sp=%d cbChId=%d, ChannelId=%d", Sp, cbChId, ChannelId);

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

static int drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	int value;
	int error;
	UINT32 ChannelId;
	wStream* data_out;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);

	WLog_DBG(TAG, "process_close_request: Sp=%d cbChId=%d, ChannelId=%d", Sp, cbChId, ChannelId);

	dvcman_close_channel(drdynvc->channel_mgr, ChannelId);
	
	data_out = Stream_New(NULL, 4);
	value = (CLOSE_REQUEST_PDU << 4) | (cbChId & 0x03);

	Stream_Write_UINT8(data_out, value);
	drdynvc_write_variable_uint(data_out, ChannelId);

	error = drdynvc_send(drdynvc, data_out);
	
	if (error != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(error), error);
		return 1;
	}
	
	drdynvc->channel_error = error;

	return 0;
}

static void drdynvc_order_recv(drdynvcPlugin* drdynvc, wStream* s)
{
	int value;
	int Cmd;
	int Sp;
	int cbChId;

	Stream_Read_UINT8(s, value);

	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;

	WLog_DBG(TAG, "order_recv: Cmd=0x%x, Sp=%d cbChId=%d, ChannelId=%d", Cmd, Sp, cbChId);

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
			WLog_ERR(TAG, "unknown drdynvc cmd 0x%x", Cmd);
			break;
	}
}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

BOOL drdynvc_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
	{
		g_InitHandles = ListDictionary_New(TRUE);
		if (!g_InitHandles)
			return FALSE;
	}

	return ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* drdynvc_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void drdynvc_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

BOOL drdynvc_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
	{
		g_OpenHandles = ListDictionary_New(TRUE);
		if (!g_OpenHandles)
			return FALSE;
	}

	return ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* drdynvc_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void drdynvc_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

static void drdynvc_virtual_channel_event_data_received(drdynvcPlugin* drdynvc,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (drdynvc->data_in)
			Stream_Free(drdynvc->data_in, TRUE);

		drdynvc->data_in = Stream_New(NULL, totalLength);
	}

	if (!(data_in = drdynvc->data_in))
		return;

	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		Stream_Free(drdynvc->data_in, TRUE);
		drdynvc->data_in = NULL;
		return;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "drdynvc_plugin_process_received: read error");
		}

		drdynvc->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(drdynvc->queue, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE drdynvc_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	drdynvcPlugin* drdynvc;

	drdynvc = (drdynvcPlugin*) drdynvc_get_open_handle_data(openHandle);

	if (!drdynvc)
	{
		WLog_ERR(TAG, "drdynvc_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			drdynvc_virtual_channel_event_data_received(drdynvc, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* drdynvc_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(drdynvc->queue))
			break;

		if (MessageQueue_Peek(drdynvc->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				drdynvc_order_recv(drdynvc, data);
				Stream_Free(data, TRUE);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void drdynvc_virtual_channel_event_connected(drdynvcPlugin* drdynvc, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	UINT32 index;
	ADDIN_ARGV* args;
	rdpSettings* settings;

	status = drdynvc->channelEntryPoints.pVirtualChannelOpen(drdynvc->InitHandle,
		&drdynvc->OpenHandle, drdynvc->channelDef.name, drdynvc_virtual_channel_open_event);

	if (!drdynvc_add_open_handle_data(drdynvc->OpenHandle, drdynvc))
	{
		WLog_ERR(TAG, "%s: unable to register open handle", __FUNCTION__);
		return;
	}

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	drdynvc->queue = MessageQueue_New(NULL);
	if (!drdynvc->queue)
	{
		WLog_ERR(TAG, "%s: unable to create message queue", __FUNCTION__);
		return;
	}

	drdynvc->channel_mgr = dvcman_new(drdynvc);
	if (!drdynvc->channel_mgr)
	{
		WLog_ERR(TAG, "%s: unable to create dvcman", __FUNCTION__);
		return;
	}

	drdynvc->channel_error = 0;

	settings = (rdpSettings*) drdynvc->channelEntryPoints.pExtendedData;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		args = settings->DynamicChannelArray[index];
		dvcman_load_addin(drdynvc->channel_mgr, args, settings);
	}

	dvcman_init(drdynvc->channel_mgr);

	drdynvc->state = DRDYNVC_STATE_CAPABILITIES;

	drdynvc->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) drdynvc_virtual_channel_client_thread, (void*) drdynvc, 0, NULL);
	if (!drdynvc->thread)
	{
		WLog_ERR(TAG, "%s: unable to create thread", __FUNCTION__);
		return;
	}
}

static void drdynvc_virtual_channel_event_disconnected(drdynvcPlugin* drdynvc)
{
	UINT status;

	MessageQueue_PostQuit(drdynvc->queue, 0);
	WaitForSingleObject(drdynvc->thread, INFINITE);

	MessageQueue_Free(drdynvc->queue);
	CloseHandle(drdynvc->thread);

	drdynvc->queue = NULL;
	drdynvc->thread = NULL;

	status = drdynvc->channelEntryPoints.pVirtualChannelClose(drdynvc->OpenHandle);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	if (drdynvc->data_in)
	{
		Stream_Free(drdynvc->data_in, TRUE);
		drdynvc->data_in = NULL;
	}

	if (drdynvc->channel_mgr)
	{
		dvcman_free(drdynvc->channel_mgr);
		drdynvc->channel_mgr = NULL;
	}

	drdynvc_remove_open_handle_data(drdynvc->OpenHandle);
}

static void drdynvc_virtual_channel_event_terminated(drdynvcPlugin* drdynvc)
{
	drdynvc_remove_init_handle_data(drdynvc->InitHandle);
	free(drdynvc);
}

static VOID VCAPITYPE drdynvc_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	drdynvcPlugin* drdynvc;

	drdynvc = (drdynvcPlugin*) drdynvc_get_init_handle_data(pInitHandle);

	if (!drdynvc)
	{
		WLog_ERR(TAG, "drdynvc_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			drdynvc_virtual_channel_event_connected(drdynvc, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			drdynvc_virtual_channel_event_disconnected(drdynvc);
			break;

		case CHANNEL_EVENT_TERMINATED:
			drdynvc_virtual_channel_event_terminated(drdynvc);
			break;
	}
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
	UINT rc;
	drdynvcPlugin* drdynvc;
	DrdynvcClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	drdynvc = (drdynvcPlugin*) calloc(1, sizeof(drdynvcPlugin));

	if (!drdynvc)
		return FALSE;

	drdynvc->channelDef.options =
		CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP |
		CHANNEL_OPTION_COMPRESS_RDP;

	strcpy(drdynvc->channelDef.name, "drdynvc");

	drdynvc->state = DRDYNVC_STATE_INITIAL;

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (DrdynvcClientContext*) calloc(1, sizeof(DrdynvcClientContext));

		if (!context)
		{
			free(drdynvc);
			return -1;
		}

		context->handle = (void*) drdynvc;
		context->custom = NULL;

		drdynvc->context = context;

		context->GetVersion = drdynvc_get_version;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	drdynvc->log = WLog_Get("com.freerdp.channels.drdynvc.client");

	WLog_Print(drdynvc->log, WLOG_DEBUG, "VirtualChannelEntry");

	CopyMemory(&(drdynvc->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = drdynvc->channelEntryPoints.pVirtualChannelInit(&drdynvc->InitHandle,
		&drdynvc->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, drdynvc_virtual_channel_init_event);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		free(drdynvc);
		return -1;
	}

	drdynvc->channelEntryPoints.pInterface = *(drdynvc->channelEntryPoints.ppInterface);
	drdynvc->channelEntryPoints.ppInterface = &(drdynvc->channelEntryPoints.pInterface);

	return drdynvc_add_init_handle_data(drdynvc->InitHandle, (void*) drdynvc) ? 1 : -1;
}

