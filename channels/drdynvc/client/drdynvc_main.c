/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
static UINT drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId,
                               const BYTE* data, UINT32 dataSize);

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_get_configuration(IWTSListener* pListener,
                                     void** ppPropertyBag)
{
	*ppPropertyBag = NULL;
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_create_listener(IWTSVirtualChannelManager* pChannelMgr,
                                   const char* pszChannelName, ULONG ulFlags,
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
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		listener->iface.GetConfiguration = dvcman_get_configuration;
		listener->iface.pInterface = NULL;
		listener->dvcman = dvcman;
		listener->channel_name = _strdup(pszChannelName);

		if (!listener->channel_name)
		{
			WLog_ERR(TAG, "_strdup failed!");
			free(listener);
			return CHANNEL_RC_NO_MEMORY;
		}

		listener->flags = ulFlags;
		listener->listener_callback = pListenerCallback;

		if (ppListener)
			*ppListener = (IWTSListener*) listener;

		dvcman->listeners[dvcman->num_listeners++] = (IWTSListener*) listener;
		return CHANNEL_RC_OK;
	}
	else
	{
		WLog_ERR(TAG, "create_listener: Maximum DVC listener number reached.");
		return ERROR_INTERNAL_ERROR;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                   const char* name, IWTSPlugin* pPlugin)
{
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	if (dvcman->num_plugins < MAX_PLUGINS)
	{
		dvcman->plugin_names[dvcman->num_plugins] = name;
		dvcman->plugins[dvcman->num_plugins++] = pPlugin;
		WLog_DBG(TAG, "register_plugin: num_plugins %d", dvcman->num_plugins);
		return CHANNEL_RC_OK;
	}
	else
	{
		WLog_ERR(TAG, "register_plugin: Maximum DVC plugin number %u reached.",
		         MAX_PLUGINS);
		return ERROR_INTERNAL_ERROR;
	}
}

static IWTSPlugin* dvcman_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                     const char* name)
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

static ADDIN_ARGV* dvcman_get_plugin_data(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->args;
}

static void* dvcman_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return (void*)((DVCMAN_ENTRY_POINTS*) pEntryPoints)->settings;
}

static UINT32 dvcman_get_channel_id(IWTSVirtualChannel* channel)
{
	return ((DVCMAN_CHANNEL*) channel)->channel_id;
}

static IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager*
        pChannelMgr,
        UINT32 ChannelId)
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

static IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	DVCMAN* dvcman;
	dvcman = (DVCMAN*) calloc(1, sizeof(DVCMAN));

	if (!dvcman)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->drdynvc = plugin;
	dvcman->channels = ArrayList_New(TRUE);

	if (!dvcman->channels)
	{
		WLog_ERR(TAG, "ArrayList_New failed!");
		free(dvcman);
		return NULL;
	}

	dvcman->channels->object.fnObjectFree = dvcman_channel_free;
	dvcman->pool = StreamPool_New(TRUE, 10);

	if (!dvcman->pool)
	{
		WLog_ERR(TAG, "StreamPool_New failed!");
		ArrayList_Free(dvcman->channels);
		free(dvcman);
		return NULL;
	}

	return (IWTSVirtualChannelManager*) dvcman;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_load_addin(IWTSVirtualChannelManager* pChannelMgr,
                              ADDIN_ARGV* args,
                              rdpSettings* settings)
{
	DVCMAN_ENTRY_POINTS entryPoints;
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;
	WLog_INFO(TAG, "Loading Dynamic Virtual Channel %s", args->argv[0]);
	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY) freerdp_load_channel_addin_entry(
	                      args->argv[0],
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
		return pDVCPluginEntry((IDRDYNVC_ENTRY_POINTS*) &entryPoints);
	}

	return ERROR_INVALID_FUNCTION;
}

static DVCMAN_CHANNEL* dvcman_channel_new(IWTSVirtualChannelManager*
        pChannelMgr,
        UINT32 ChannelId, const char* ChannelName)
{
	DVCMAN_CHANNEL* channel;

	if (dvcman_find_channel_by_id(pChannelMgr, ChannelId))
	{
		WLog_ERR(TAG, "Protocol error: Duplicated ChannelId %"PRIu32" (%s)!", ChannelId,
		         ChannelName);
		return NULL;
	}

	channel = (DVCMAN_CHANNEL*) calloc(1, sizeof(DVCMAN_CHANNEL));

	if (!channel)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	channel->dvcman = (DVCMAN*) pChannelMgr;
	channel->channel_id = ChannelId;
	channel->channel_name = _strdup(ChannelName);

	if (!channel->channel_name)
	{
		WLog_ERR(TAG, "_strdup failed!");
		free(channel);
		return NULL;
	}

	if (!InitializeCriticalSectionEx(&(channel->lock), 0 , 0))
	{
		WLog_ERR(TAG, "InitializeCriticalSectionEx failed!");
		free(channel->channel_name);
		free(channel);
		return NULL;
	}

	return channel;
}

static void dvcman_channel_free(void* arg)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) arg;
	UINT error = CHANNEL_RC_OK;

	if (channel)
	{
		if (channel->channel_callback)
		{
			IFCALL(channel->channel_callback->OnClose,
			       channel->channel_callback);
		}

		if (channel->status == CHANNEL_RC_OK)
		{
			IWTSVirtualChannel* ichannel = (IWTSVirtualChannel*) channel;

			if (channel->dvcman && channel->dvcman->drdynvc)
			{			
				DrdynvcClientContext* context = channel->dvcman->drdynvc->context;
				if (context)
				{
					IFCALLRET(context->OnChannelDisconnected, error,
							  context, channel->channel_name,
							  channel->pInterface);
				}
			}

			error = IFCALLRESULT(CHANNEL_RC_OK, ichannel->Close, ichannel);
			if (error != CHANNEL_RC_OK)
				WLog_ERR(TAG, "Close failed with error %"PRIu32"!", error);
		}

		if (channel->dvc_data)
			Stream_Release(channel->dvc_data);

		DeleteCriticalSection(&(channel->lock));

		free(channel->channel_name);
	}
	free(channel);
}

static void dvcman_free(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN_LISTENER* listener;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;
	UINT error;
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
			if ((error = pPlugin->Terminated(pPlugin)))
				WLog_ERR(TAG, "Terminated failed with error %"PRIu32"!", error);
	}

	dvcman->num_plugins = 0;
	StreamPool_Free(dvcman->pool);
	free(dvcman);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_init(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;
	UINT error;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Initialize)
			if ((error = pPlugin->Initialize(pPlugin, pChannelMgr)))
			{
				WLog_ERR(TAG, "Initialize failed with error %"PRIu32"!", error);
				return error;
			}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_write_channel(IWTSVirtualChannel* pChannel, ULONG cbSize,
                                 const BYTE* pBuffer, void* pReserved)
{
	UINT status;
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;

	if (!channel || !channel->dvcman)
		return CHANNEL_RC_BAD_CHANNEL;

	EnterCriticalSection(&(channel->lock));
	status = drdynvc_write_data(channel->dvcman->drdynvc,
	                            channel->channel_id, pBuffer, cbSize);
	LeaveCriticalSection(&(channel->lock));
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_close_channel_iface(IWTSVirtualChannel* pChannel)
{

	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;

	if (!channel)
		return CHANNEL_RC_BAD_CHANNEL;

	WLog_DBG(TAG, "close_channel_iface: id=%"PRIu32"", channel->channel_id);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr,
                                  UINT32 ChannelId, const char* ChannelName)
{
	int i;
	BOOL bAccept;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	DrdynvcClientContext* context;
	IWTSVirtualChannelCallback* pCallback;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;
	UINT error;

	if (!(channel = dvcman_channel_new(pChannelMgr, ChannelId, ChannelName)))
	{
		WLog_ERR(TAG, "dvcman_channel_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	channel->status = ERROR_NOT_CONNECTED;
	ArrayList_Add(dvcman->channels, channel);

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			channel->iface.Write = dvcman_write_channel;
			channel->iface.Close = dvcman_close_channel_iface;
			bAccept = TRUE;
			pCallback = NULL;

			if ((error = listener->listener_callback->OnNewChannelConnection(
			                 listener->listener_callback,
			                 (IWTSVirtualChannel*) channel, NULL, &bAccept, &pCallback)) == CHANNEL_RC_OK
			    && bAccept)
			{
				WLog_DBG(TAG, "listener %s created new channel %"PRIu32"",
				         listener->channel_name, channel->channel_id);
				channel->status = CHANNEL_RC_OK;
				channel->channel_callback = pCallback;
				channel->pInterface = listener->iface.pInterface;
				context = dvcman->drdynvc->context;
				IFCALLRET(context->OnChannelConnected, error, context, ChannelName,
				          listener->iface.pInterface);

				if (error)
					WLog_ERR(TAG, "context.ReceiveSamples failed with error %"PRIu32"", error);

				return error;
			}
			else
			{
				if (error)
				{
					WLog_ERR(TAG, "OnNewChannelConnection failed with error %"PRIu32"!", error);
					return error;
				}
				else
				{
					WLog_ERR(TAG, "OnNewChannelConnection returned with bAccept FALSE!");
					return ERROR_INTERNAL_ERROR;
				}
			}
		}
	}

	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_open_channel(IWTSVirtualChannelManager* pChannelMgr,
                                UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannelCallback* pCallback;
	UINT error;
	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_ERR(TAG, "ChannelId %"PRIu32" not found!", ChannelId);
		return ERROR_INTERNAL_ERROR;
	}

	if (channel->status == CHANNEL_RC_OK)
	{
		pCallback = channel->channel_callback;

		if ((pCallback->OnOpen) && (error = pCallback->OnOpen(pCallback)))
		{
			WLog_ERR(TAG, "OnOpen failed with error %"PRIu32"!", error);
			return error;
		}

		WLog_DBG(TAG, "open_channel: ChannelId %"PRIu32"", ChannelId);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr,
                                 UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	UINT error = CHANNEL_RC_OK;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		//WLog_ERR(TAG, "ChannelId %"PRIu32" not found!", ChannelId);
		/**
		 * Windows 8 / Windows Server 2012 send close requests for channels that failed to be created.
		 * Do not warn, simply return success here.
		 */
		return CHANNEL_RC_OK;
	}

	ArrayList_Remove(dvcman->channels, channel);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_receive_channel_data_first(IWTSVirtualChannelManager*
        pChannelMgr,
        UINT32 ChannelId, UINT32 length)
{
	DVCMAN_CHANNEL* channel;
	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		/**
		 * Windows Server 2012 R2 can send some messages over Microsoft::Windows::RDS::Geometry::v08.01
		 * even if the dynamic virtual channel wasn't registered on our side. Ignoring it works.
		 */
		WLog_ERR(TAG, "ChannelId %"PRIu32" not found!", ChannelId);
		return CHANNEL_RC_OK;
	}

	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	channel->dvc_data = StreamPool_Take(channel->dvcman->pool, length);

	if (!channel->dvc_data)
	{
		WLog_ERR(TAG, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	channel->dvc_data_length = length;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr,
                                        UINT32 ChannelId, wStream* data)
{
	UINT status = CHANNEL_RC_OK;
	DVCMAN_CHANNEL* channel;
	size_t dataSize = Stream_GetRemainingLength(data);
	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		/* Windows 8.1 tries to open channels not created.
				 * Ignore cases like this. */
		WLog_ERR(TAG, "ChannelId %"PRIu32" not found!", ChannelId);
		return CHANNEL_RC_OK;
	}

	if (channel->dvc_data)
	{
		/* Fragmented data */
		if (Stream_GetPosition(channel->dvc_data) + dataSize > (UINT32) Stream_Capacity(
		        channel->dvc_data))
		{
			WLog_ERR(TAG, "data exceeding declared length!");
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
			return ERROR_INVALID_DATA;
		}

		Stream_Write(channel->dvc_data, Stream_Pointer(data), dataSize);

		if (Stream_GetPosition(channel->dvc_data) >= channel->dvc_data_length)
		{
			Stream_SealLength(channel->dvc_data);
			Stream_SetPosition(channel->dvc_data, 0);
			status = channel->channel_callback->OnDataReceived(channel->channel_callback,
			         channel->dvc_data);
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
		}
	}
	else
	{
		status = channel->channel_callback->OnDataReceived(channel->channel_callback,
		         data);
	}

	return status;
}

static UINT drdynvc_write_variable_uint(wStream* s, UINT32 val)
{
	UINT cb;

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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_send(drdynvcPlugin* drdynvc, wStream* s)
{
	UINT status;

	if (!drdynvc)
		status = CHANNEL_RC_BAD_CHANNEL_HANDLE;
	else
	{
		status = drdynvc->channelEntryPoints.pVirtualChannelWriteEx(drdynvc->InitHandle,
		         drdynvc->OpenHandle, Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	switch (status)
	{
		case CHANNEL_RC_OK:
		case CHANNEL_RC_NOT_CONNECTED:
			return CHANNEL_RC_OK;

		default:
			Stream_Free(s, TRUE);
			WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
			           WTSErrorToString(status),
			           status);
			return status;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId,
                               const BYTE* data, UINT32 dataSize)
{
	wStream* data_out;
	unsigned long pos;
	UINT32 cbChId;
	UINT32 cbLen;
	unsigned long chunkLength;
	UINT status;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	WLog_Print(drdynvc->log, WLOG_DEBUG, "write_data: ChannelId=%"PRIu32" size=%"PRIu32"", ChannelId,
	           dataSize);
	data_out = Stream_New(NULL, CHANNEL_CHUNK_LENGTH);

	if (!data_out)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_SetPosition(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);
	pos = Stream_GetPosition(data_out);

	if (dataSize == 0)
	{
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, 0x40 | cbChId);
		Stream_SetPosition(data_out, pos);
		status = drdynvc_send(drdynvc, data_out);
	}
	else if (dataSize <= CHANNEL_CHUNK_LENGTH - pos)
	{
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

			if (!data_out)
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_New failed!");
				return CHANNEL_RC_NO_MEMORY;
			}

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
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		           WTSErrorToString(status), status);
		return status;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_send_capability_response(drdynvcPlugin* drdynvc)
{
	UINT status;
	wStream* s;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	WLog_Print(drdynvc->log, WLOG_TRACE, "capability_response");
	s = Stream_New(NULL, 4);

	if (!s)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_Ndrdynvc_write_variable_uintew failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s,
	                    0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	Stream_Write_UINT16(s, drdynvc->version);
	status = drdynvc_send(drdynvc, s);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		           WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp,
        int cbChId, wStream* s)
{
	UINT status;

	if (!drdynvc)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	WLog_Print(drdynvc->log, WLOG_TRACE, "capability_request Sp=%d cbChId=%d", Sp, cbChId);
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
	drdynvc->state = DRDYNVC_STATE_READY;
	return status;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_create_request(drdynvcPlugin* drdynvc, int Sp,
        int cbChId, wStream* s)
{
	unsigned long pos;
	UINT status;
	UINT32 ChannelId;
	wStream* data_out;
	UINT channel_status;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (drdynvc->state == DRDYNVC_STATE_CAPABILITIES)
	{
		/**
		 * For some reason the server does not always send the
		 * capabilities pdu as it should. When this happens,
		 * send a capabilities response.
		 */
		drdynvc->version = 3;

		if ((status = drdynvc_send_capability_response(drdynvc)))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "drdynvc_send_capability_response failed!");
			return status;
		}

		drdynvc->state = DRDYNVC_STATE_READY;
	}

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = Stream_GetPosition(s);
	WLog_Print(drdynvc->log, WLOG_DEBUG, "process_create_request: ChannelId=%"PRIu32" ChannelName=%s",
	           ChannelId,
	           Stream_Pointer(s));
	channel_status = dvcman_create_channel(drdynvc->channel_mgr, ChannelId,
	                                       (char*) Stream_Pointer(s));
	data_out = Stream_New(NULL, pos + 4);

	if (!s)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(data_out, 0x10 | cbChId);
	Stream_SetPosition(s, 1);
	Stream_Copy(s, data_out, pos - 1);

	if (channel_status == CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_DEBUG, "channel created");
		Stream_Write_UINT32(data_out, 0);
	}
	else
	{
		WLog_Print(drdynvc->log, WLOG_DEBUG, "no listener");
		Stream_Write_UINT32(data_out,
		                    (UINT32) 0xC0000001); /* same code used by mstsc */
	}

	status = drdynvc_send(drdynvc, data_out);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	if (channel_status == CHANNEL_RC_OK)
	{
		if ((status = dvcman_open_channel(drdynvc->channel_mgr, ChannelId)))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_open_channel failed with error %"PRIu32"!", status);
			return status;
		}
	}
	else
	{
		if ((status = dvcman_close_channel(drdynvc->channel_mgr, ChannelId)))
			WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_close_channel failed with error %"PRIu32"!", status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_data_first(drdynvcPlugin* drdynvc, int Sp,
                                       int cbChId, wStream* s)
{
	UINT status;
	UINT32 Length;
	UINT32 ChannelId;
	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	WLog_DBG(TAG, "process_data_first: Sp=%d cbChId=%d, ChannelId=%"PRIu32" Length=%"PRIu32"", Sp,
	         cbChId, ChannelId, Length);
	status = dvcman_receive_channel_data_first(drdynvc->channel_mgr, ChannelId,
	         Length);

	if (status)
		return status;

	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId,
                                 wStream* s)
{
	UINT32 ChannelId;
	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_Print(drdynvc->log, WLOG_TRACE, "process_data: Sp=%d cbChId=%d, ChannelId=%"PRIu32"", Sp,
	           cbChId,
	           ChannelId);
	return dvcman_receive_channel_data(drdynvc->channel_mgr, ChannelId, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp,
        int cbChId, wStream* s)
{
	int value;
	UINT error;
	UINT32 ChannelId;
	wStream* data_out;
	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_DBG(TAG, "process_close_request: Sp=%d cbChId=%d, ChannelId=%"PRIu32"", Sp,
	         cbChId, ChannelId);

	if ((error = dvcman_close_channel(drdynvc->channel_mgr, ChannelId)))
	{
		WLog_ERR(TAG, "dvcman_close_channel failed with error %"PRIu32"!", error);
		return error;
	}

	data_out = Stream_New(NULL, 4);

	if (!data_out)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	value = (CLOSE_REQUEST_PDU << 4) | (cbChId & 0x03);
	Stream_Write_UINT8(data_out, value);
	drdynvc_write_variable_uint(data_out, ChannelId);
	error = drdynvc_send(drdynvc, data_out);

	if (error)
		WLog_ERR(TAG, "VirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(error), error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_order_recv(drdynvcPlugin* drdynvc, wStream* s)
{
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	Stream_Read_UINT8(s, value);
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;
	WLog_Print(drdynvc->log, WLOG_DEBUG, "order_recv: Cmd=0x%x, Sp=%d cbChId=%d", Cmd, Sp, cbChId);

	switch (Cmd)
	{
		case CAPABILITY_REQUEST_PDU:
			return drdynvc_process_capability_request(drdynvc, Sp, cbChId, s);

		case CREATE_REQUEST_PDU:
			return drdynvc_process_create_request(drdynvc, Sp, cbChId, s);

		case DATA_FIRST_PDU:
			return drdynvc_process_data_first(drdynvc, Sp, cbChId, s);

		case DATA_PDU:
			return drdynvc_process_data(drdynvc, Sp, cbChId, s);

		case CLOSE_REQUEST_PDU:
			return drdynvc_process_close_request(drdynvc, Sp, cbChId, s);

		default:
			WLog_ERR(TAG, "unknown drdynvc cmd 0x%x", Cmd);
			return ERROR_INTERNAL_ERROR;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_virtual_channel_event_data_received(drdynvcPlugin* drdynvc,
        void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (drdynvc->data_in)
			Stream_Free(drdynvc->data_in, TRUE);

		drdynvc->data_in = Stream_New(NULL, totalLength);
	}

	if (!(data_in = drdynvc->data_in))
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		Stream_Free(drdynvc->data_in, TRUE);
		drdynvc->data_in = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "drdynvc_plugin_process_received: read error");
			return ERROR_INVALID_DATA;
		}

		drdynvc->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(drdynvc->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static void VCAPITYPE drdynvc_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event, LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) lpUserParam;

	if (!drdynvc || (drdynvc->OpenHandle != openHandle))
	{
		WLog_ERR(TAG, "drdynvc_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = drdynvc_virtual_channel_event_data_received(drdynvc, pData, dataLength, totalLength,
			             dataFlags)))
				WLog_ERR(TAG, "drdynvc_virtual_channel_event_data_received failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && drdynvc->rdpcontext)
		setChannelError(drdynvc->rdpcontext, error, "drdynvc_virtual_channel_open_event reported an error");
}

static void* drdynvc_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) arg;

	if (!drdynvc)
	{
		ExitThread((DWORD) CHANNEL_RC_BAD_CHANNEL_HANDLE);
		return NULL;
	}

	while (1)
	{
		if (!MessageQueue_Wait(drdynvc->queue))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(drdynvc->queue, &message, TRUE))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;

			if ((error = drdynvc_order_recv(drdynvc, data)))
			{
				Stream_Free(data, TRUE);
				WLog_Print(drdynvc->log, WLOG_ERROR, "drdynvc_order_recv failed with error %"PRIu32"!", error);
				break;
			}

			Stream_Free(data, TRUE);
		}
	}

	{
		/* Disconnect remaining dynamic channels that the server did not.
		* This is required to properly shut down channels by calling the appropriate
		* event handlers. */
		DVCMAN* drdynvcMgr = (DVCMAN*)drdynvc->channel_mgr;

		while (ArrayList_Count(drdynvcMgr->channels) > 0)
		{
			IWTSVirtualChannel* channel = (IWTSVirtualChannel*)
			                              ArrayList_GetItem(drdynvcMgr->channels, 0);
			const UINT32 ChannelId = drdynvc->channel_mgr->GetChannelId(channel);
			dvcman_close_channel(drdynvc->channel_mgr, ChannelId);
		}
	}

	if (error && drdynvc->rdpcontext)
		setChannelError(drdynvc->rdpcontext, error,
		                "drdynvc_virtual_channel_client_thread reported an error");

	ExitThread((DWORD) error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_virtual_channel_event_connected(drdynvcPlugin* drdynvc, LPVOID pData,
        UINT32 dataLength)
{
	UINT error;
	UINT32 status;
	UINT32 index;
	ADDIN_ARGV* args;
	rdpSettings* settings;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	status = drdynvc->channelEntryPoints.pVirtualChannelOpenEx(drdynvc->InitHandle,
	         &drdynvc->OpenHandle, drdynvc->channelDef.name, drdynvc_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelOpen failed with %s [%08"PRIX32"]",
		           WTSErrorToString(status), status);
		return status;
	}

	drdynvc->queue = MessageQueue_New(NULL);

	if (!drdynvc->queue)
	{
		error = CHANNEL_RC_NO_MEMORY;
		WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_New failed!");
		goto error;
	}

	drdynvc->channel_mgr = dvcman_new(drdynvc);

	if (!drdynvc->channel_mgr)
	{
		error = CHANNEL_RC_NO_MEMORY;
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_new failed!");
		goto error;
	}

	settings = (rdpSettings*) drdynvc->channelEntryPoints.pExtendedData;

	for (index = 0; index < settings->DynamicChannelCount; index++)
	{
		args = settings->DynamicChannelArray[index];
		error = dvcman_load_addin(drdynvc->channel_mgr, args, settings);

		if (CHANNEL_RC_OK != error)
			goto error;
	}

	if ((error = dvcman_init(drdynvc->channel_mgr)))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_init failed with error %"PRIu32"!", error);
		goto error;
	}

	drdynvc->state = DRDYNVC_STATE_CAPABILITIES;

	if (!(drdynvc->thread = CreateThread(NULL, 0,
	                                     (LPTHREAD_START_ROUTINE) drdynvc_virtual_channel_client_thread, (void*) drdynvc,
	                                     0, NULL)))
	{
		error = ERROR_INTERNAL_ERROR;
		WLog_Print(drdynvc->log, WLOG_ERROR, "CreateThread failed!");
		goto error;
	}

error:
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_virtual_channel_event_disconnected(drdynvcPlugin* drdynvc)
{
	UINT status;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!MessageQueue_PostQuit(drdynvc->queue, 0))
	{
		status = GetLastError();
		WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_PostQuit failed with error %"PRIu32"", status);
		return status;
	}

	if (WaitForSingleObject(drdynvc->thread, INFINITE) != WAIT_OBJECT_0)
	{
		status = GetLastError();
		WLog_Print(drdynvc->log, WLOG_ERROR, "WaitForSingleObject failed with error %"PRIu32"", status);
		return status;
	}

	MessageQueue_Free(drdynvc->queue);
	CloseHandle(drdynvc->thread);
	drdynvc->queue = NULL;
	drdynvc->thread = NULL;
	status = drdynvc->channelEntryPoints.pVirtualChannelCloseEx(drdynvc->InitHandle,
	         drdynvc->OpenHandle);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelClose failed with %s [%08"PRIX32"]",
		           WTSErrorToString(status), status);
	}

	drdynvc->OpenHandle = 0;

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

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_virtual_channel_event_terminated(drdynvcPlugin* drdynvc)
{
	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	drdynvc->InitHandle = 0;
	free(drdynvc->context);
	free(drdynvc);
	return CHANNEL_RC_OK;
}

static UINT drdynvc_virtual_channel_event_attached(drdynvcPlugin* drdynvc)
{
	int i;
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*) drdynvc->channel_mgr;

	if (!dvcman)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		UINT error;
		IWTSPlugin* pPlugin = dvcman->plugins[i];

		if (pPlugin->Attached)
			if ((error = pPlugin->Attached(pPlugin)))
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "Attach failed with error %"PRIu32"!", error);
				return error;
			}
	}

	return CHANNEL_RC_OK;
}

static UINT drdynvc_virtual_channel_event_detached(drdynvcPlugin* drdynvc)
{
	int i;
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*) drdynvc->channel_mgr;

	if (!dvcman)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		UINT error;
		IWTSPlugin* pPlugin = dvcman->plugins[i];

		if (pPlugin->Detached)
			if ((error = pPlugin->Detached(pPlugin)))
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "Detach failed with error %"PRIu32"!", error);
				return error;
			}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE drdynvc_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) lpUserParam;

	if (!drdynvc || (drdynvc->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "drdynvc_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = drdynvc_virtual_channel_event_connected(drdynvc, pData, dataLength)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_connected failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error =  drdynvc_virtual_channel_event_disconnected(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_disconnected failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			if ((error =  drdynvc_virtual_channel_event_terminated(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_terminated failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_ATTACHED:
			if ((error =  drdynvc_virtual_channel_event_attached(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_attached failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_DETACHED:
			if ((error =  drdynvc_virtual_channel_event_detached(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_detached failed with error %"PRIu32"", error);

			break;

		default:
			break;
	}

	if (error && drdynvc->rdpcontext)
		setChannelError(drdynvc->rdpcontext, error,
		                "drdynvc_virtual_channel_init_event_ex reported an error");
}

/**
 * Channel Client Interface
 */

static int drdynvc_get_version(DrdynvcClientContext* context)
{
	drdynvcPlugin* drdynvc = (drdynvcPlugin*) context->handle;
	return drdynvc->version;
}

/* drdynvc is always built-in */
#define VirtualChannelEntryEx	drdynvc_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS_EX pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	drdynvcPlugin* drdynvc;
	DrdynvcClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	drdynvc = (drdynvcPlugin*) calloc(1, sizeof(drdynvcPlugin));

	if (!drdynvc)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	drdynvc->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED |
	    CHANNEL_OPTION_ENCRYPT_RDP |
	    CHANNEL_OPTION_COMPRESS_RDP;
	strcpy(drdynvc->channelDef.name, "drdynvc");
	drdynvc->state = DRDYNVC_STATE_INITIAL;
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (DrdynvcClientContext*) calloc(1, sizeof(DrdynvcClientContext));

		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(drdynvc);
			return FALSE;
		}

		context->handle = (void*) drdynvc;
		context->custom = NULL;
		drdynvc->context = context;
		context->GetVersion = drdynvc_get_version;
		drdynvc->rdpcontext = pEntryPointsEx->context;
	}

	drdynvc->log = WLog_Get("com.freerdp.channels.drdynvc.client");
	WLog_Print(drdynvc->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(drdynvc->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	drdynvc->InitHandle = pInitHandle;
	rc = drdynvc->channelEntryPoints.pVirtualChannelInitEx(drdynvc, context, pInitHandle,
	        &drdynvc->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, drdynvc_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelInit failed with %s [%08"PRIX32"]",
		           WTSErrorToString(rc), rc);
		free(drdynvc->context);
		free(drdynvc);
		return FALSE;
	}

	drdynvc->channelEntryPoints.pInterface = context;
	return TRUE;
}

