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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/channels/drdynvc.h>

#include "drdynvc_main.h"

#define TAG CHANNELS_TAG("drdynvc.client")

static UINT dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId,
                                 BOOL bSendClosePDU);
static void dvcman_free(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr);
static void dvcman_channel_free(void* channel);
static UINT drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId, const BYTE* data,
                               UINT32 dataSize, BOOL* close);
static UINT drdynvc_send(drdynvcPlugin* drdynvc, wStream* s);

static void dvcman_wtslistener_free(DVCMAN_LISTENER* listener)
{
	if (listener)
		free(listener->channel_name);
	free(listener);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_get_configuration(IWTSListener* pListener, void** ppPropertyBag)
{
	WINPR_UNUSED(pListener);
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
                                   IWTSListenerCallback* pListenerCallback,
                                   IWTSListener** ppListener)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	DVCMAN_LISTENER* listener;

	WLog_DBG(TAG, "create_listener: %d.%s.", ArrayList_Count(dvcman->listeners) + 1,
	         pszChannelName);
	listener = (DVCMAN_LISTENER*)calloc(1, sizeof(DVCMAN_LISTENER));

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
		dvcman_wtslistener_free(listener);
		return CHANNEL_RC_NO_MEMORY;
	}

	listener->flags = ulFlags;
	listener->listener_callback = pListenerCallback;

	if (ppListener)
		*ppListener = (IWTSListener*)listener;

	if (!ArrayList_Append(dvcman->listeners, listener))
		return ERROR_INTERNAL_ERROR;
	return CHANNEL_RC_OK;
}

static UINT dvcman_destroy_listener(IWTSVirtualChannelManager* pChannelMgr, IWTSListener* pListener)
{
	DVCMAN_LISTENER* listener = (DVCMAN_LISTENER*)pListener;

	WINPR_UNUSED(pChannelMgr);

	if (listener)
	{
		DVCMAN* dvcman = listener->dvcman;
		if (dvcman)
			ArrayList_Remove(dvcman->listeners, listener);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name,
                                   IWTSPlugin* pPlugin)
{
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*)pEntryPoints)->dvcman;

	if (!ArrayList_Append(dvcman->plugin_names, _strdup(name)))
		return ERROR_INTERNAL_ERROR;
	if (!ArrayList_Append(dvcman->plugins, pPlugin))
		return ERROR_INTERNAL_ERROR;

	WLog_DBG(TAG, "register_plugin: num_plugins %d", ArrayList_Count(dvcman->plugins));
	return CHANNEL_RC_OK;
}

static IWTSPlugin* dvcman_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name)
{
	IWTSPlugin* plugin = NULL;
	size_t i, nc, pc;
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*)pEntryPoints)->dvcman;
	if (!dvcman || !pEntryPoints || !name)
		return NULL;

	nc = ArrayList_Count(dvcman->plugin_names);
	pc = ArrayList_Count(dvcman->plugins);
	if (nc != pc)
		return NULL;

	ArrayList_Lock(dvcman->plugin_names);
	ArrayList_Lock(dvcman->plugins);
	for (i = 0; i < pc; i++)
	{
		const char* cur = ArrayList_GetItem(dvcman->plugin_names, i);
		if (strcmp(cur, name) == 0)
		{
			plugin = ArrayList_GetItem(dvcman->plugins, i);
			break;
		}
	}
	ArrayList_Unlock(dvcman->plugin_names);
	ArrayList_Unlock(dvcman->plugins);
	return plugin;
}

static const ADDIN_ARGV* dvcman_get_plugin_data(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((DVCMAN_ENTRY_POINTS*)pEntryPoints)->args;
}

static rdpContext* dvcman_get_rdp_context(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	DVCMAN_ENTRY_POINTS* entry = (DVCMAN_ENTRY_POINTS*)pEntryPoints;
	WINPR_ASSERT(entry);
	return entry->context;
}

static rdpSettings* dvcman_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	rdpContext* context = dvcman_get_rdp_context(pEntryPoints);
	WINPR_ASSERT(context);

	return context->settings;
}

static UINT32 dvcman_get_channel_id(IWTSVirtualChannel* channel)
{
	DVCMAN_CHANNEL* dvc = (DVCMAN_CHANNEL*)channel;
	return dvc->channel_id;
}

static const char* dvcman_get_channel_name(IWTSVirtualChannel* channel)
{
	DVCMAN_CHANNEL* dvc = (DVCMAN_CHANNEL*)channel;
	return dvc->channel_name;
}

static IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager* pChannelMgr,
                                                     UINT32 ChannelId)
{
	size_t index;
	IWTSVirtualChannel* channel = NULL;
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	ArrayList_Lock(dvcman->channels);
	for (index = 0; index < ArrayList_Count(dvcman->channels); index++)
	{
		DVCMAN_CHANNEL* cur = (DVCMAN_CHANNEL*)ArrayList_GetItem(dvcman->channels, index);
		if (cur->channel_id == ChannelId)
		{
			channel = &cur->iface;
			break;
		}
	}

	ArrayList_Unlock(dvcman->channels);
	return channel;
}

static void dvcman_plugin_terminate(void* plugin)
{
	IWTSPlugin* pPlugin = plugin;

	UINT error = IFCALLRESULT(CHANNEL_RC_OK, pPlugin->Terminated, pPlugin);
	if (error != CHANNEL_RC_OK)
		WLog_ERR(TAG, "Terminated failed with error %" PRIu32 "!", error);
}

static void wts_listener_free(void* arg)
{
	DVCMAN_LISTENER* listener = (DVCMAN_LISTENER*)arg;
	dvcman_wtslistener_free(listener);
}
static IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	wObject* obj;
	DVCMAN* dvcman;
	dvcman = (DVCMAN*)calloc(1, sizeof(DVCMAN));

	if (!dvcman)
		return NULL;

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.DestroyListener = dvcman_destroy_listener;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->iface.GetChannelName = dvcman_get_channel_name;
	dvcman->drdynvc = plugin;
	dvcman->channels = ArrayList_New(TRUE);

	if (!dvcman->channels)
		goto fail;

	obj = ArrayList_Object(dvcman->channels);
	obj->fnObjectFree = dvcman_channel_free;

	dvcman->pool = StreamPool_New(TRUE, 10);
	if (!dvcman->pool)
		goto fail;

	dvcman->listeners = ArrayList_New(TRUE);
	if (!dvcman->listeners)
		goto fail;
	obj = ArrayList_Object(dvcman->listeners);
	obj->fnObjectFree = wts_listener_free;

	dvcman->plugin_names = ArrayList_New(TRUE);
	if (!dvcman->plugin_names)
		goto fail;
	obj = ArrayList_Object(dvcman->plugin_names);
	obj->fnObjectFree = free;

	dvcman->plugins = ArrayList_New(TRUE);
	if (!dvcman->plugins)
		goto fail;
	obj = ArrayList_Object(dvcman->plugins);
	obj->fnObjectFree = dvcman_plugin_terminate;
	return &dvcman->iface;
fail:
	dvcman_free(plugin, &dvcman->iface);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_load_addin(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr,
                              const ADDIN_ARGV* args, rdpContext* context)
{
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;

	WINPR_ASSERT(drdynvc);
	WINPR_ASSERT(pChannelMgr);
	WINPR_ASSERT(args);
	WINPR_ASSERT(context);

	WLog_Print(drdynvc->log, WLOG_INFO, "Loading Dynamic Virtual Channel %s", args->argv[0]);
	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY)freerdp_load_channel_addin_entry(
	    args->argv[0], NULL, NULL, FREERDP_ADDIN_CHANNEL_DYNAMIC);

	if (pDVCPluginEntry)
	{
		DVCMAN_ENTRY_POINTS entryPoints = { 0 };

		entryPoints.iface.RegisterPlugin = dvcman_register_plugin;
		entryPoints.iface.GetPlugin = dvcman_get_plugin;
		entryPoints.iface.GetPluginData = dvcman_get_plugin_data;
		entryPoints.iface.GetRdpSettings = dvcman_get_rdp_settings;
		entryPoints.iface.GetRdpContext = dvcman_get_rdp_context;
		entryPoints.dvcman = (DVCMAN*)pChannelMgr;
		entryPoints.args = args;
		entryPoints.context = context;
		return pDVCPluginEntry(&entryPoints.iface);
	}

	return ERROR_INVALID_FUNCTION;
}

static DVCMAN_CHANNEL* dvcman_channel_new(drdynvcPlugin* drdynvc,
                                          IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId,
                                          const char* ChannelName)
{
	DVCMAN_CHANNEL* channel;

	if (dvcman_find_channel_by_id(pChannelMgr, ChannelId))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR,
		           "Protocol error: Duplicated ChannelId %" PRIu32 " (%s)!", ChannelId,
		           ChannelName);
		return NULL;
	}

	channel = (DVCMAN_CHANNEL*)calloc(1, sizeof(DVCMAN_CHANNEL));

	if (!channel)
		goto fail;

	channel->dvcman = (DVCMAN*)pChannelMgr;
	channel->channel_id = ChannelId;
	channel->channel_name = _strdup(ChannelName);

	if (!channel->channel_name)
		goto fail;

	if (!InitializeCriticalSectionEx(&(channel->lock), 0, 0))
		goto fail;

	return channel;
fail:
	dvcman_channel_free(channel);
	return NULL;
}

static void dvcman_channel_free(void* arg)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*)arg;
	UINT error = CHANNEL_RC_OK;

	if (channel)
	{
		if (channel->channel_callback)
		{
			IFCALL(channel->channel_callback->OnClose, channel->channel_callback);
			channel->channel_callback = NULL;
		}

		if (channel->status == CHANNEL_RC_OK)
		{
			IWTSVirtualChannel* ichannel = (IWTSVirtualChannel*)channel;

			if (channel->dvcman && channel->dvcman->drdynvc)
			{
				DrdynvcClientContext* context = channel->dvcman->drdynvc->context;

				if (context)
				{
					IFCALLRET(context->OnChannelDisconnected, error, context, channel->channel_name,
					          channel->pInterface);
				}
			}

			error = IFCALLRESULT(CHANNEL_RC_OK, ichannel->Close, ichannel);

			if (error != CHANNEL_RC_OK)
				WLog_ERR(TAG, "Close failed with error %" PRIu32 "!", error);
		}

		if (channel->dvc_data)
			Stream_Release(channel->dvc_data);

		DeleteCriticalSection(&(channel->lock));
		free(channel->channel_name);
	}

	free(channel);
}

static void dvcman_clear(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;

	WINPR_UNUSED(drdynvc);

	ArrayList_Clear(dvcman->channels);
	ArrayList_Clear(dvcman->plugins);
	ArrayList_Clear(dvcman->plugin_names);
	ArrayList_Clear(dvcman->listeners);
}
static void dvcman_free(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;

	WINPR_UNUSED(drdynvc);

	ArrayList_Free(dvcman->plugins);
	ArrayList_Free(dvcman->channels);
	ArrayList_Free(dvcman->plugin_names);
	ArrayList_Free(dvcman->listeners);

	StreamPool_Free(dvcman->pool);
	free(dvcman);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_init(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr)
{
	size_t i;
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	UINT error = CHANNEL_RC_OK;

	ArrayList_Lock(dvcman->plugins);
	for (i = 0; i < ArrayList_Count(dvcman->plugins); i++)
	{
		IWTSPlugin* pPlugin = ArrayList_GetItem(dvcman->plugins, i);

		error = IFCALLRESULT(CHANNEL_RC_OK, pPlugin->Initialize, pPlugin, pChannelMgr);
		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "Initialize failed with error %" PRIu32 "!",
			           error);
			goto fail;
		}
	}

fail:
	ArrayList_Unlock(dvcman->plugins);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_write_channel(IWTSVirtualChannel* pChannel, ULONG cbSize, const BYTE* pBuffer,
                                 void* pReserved)
{
	BOOL close = FALSE;
	UINT status;
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*)pChannel;

	WINPR_UNUSED(pReserved);
	if (!channel || !channel->dvcman)
		return CHANNEL_RC_BAD_CHANNEL;

	EnterCriticalSection(&(channel->lock));
	status =
	    drdynvc_write_data(channel->dvcman->drdynvc, channel->channel_id, pBuffer, cbSize, &close);
	LeaveCriticalSection(&(channel->lock));
	/* Close delayed, it removes the channel struct */
	if (close)
		dvcman_close_channel(channel->dvcman->drdynvc->channel_mgr, channel->channel_id, TRUE);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_close_channel_iface(IWTSVirtualChannel* pChannel)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*)pChannel;

	if (!channel)
		return CHANNEL_RC_BAD_CHANNEL;

	WLog_DBG(TAG, "close_channel_iface: id=%" PRIu32 "", channel->channel_id);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_create_channel(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr,
                                  UINT32 ChannelId, const char* ChannelName)
{
	size_t i;
	BOOL bAccept;
	DVCMAN_CHANNEL* channel;
	DrdynvcClientContext* context;
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	UINT error;

	if (!(channel = dvcman_channel_new(drdynvc, pChannelMgr, ChannelId, ChannelName)))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_channel_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	channel->status = ERROR_NOT_CONNECTED;
	if (!ArrayList_Append(dvcman->channels, channel))
		return ERROR_INTERNAL_ERROR;

	ArrayList_Lock(dvcman->listeners);
	for (i = 0; i < ArrayList_Count(dvcman->listeners); i++)
	{
		DVCMAN_LISTENER* listener = (DVCMAN_LISTENER*)ArrayList_GetItem(dvcman->listeners, i);

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			IWTSVirtualChannelCallback* pCallback = NULL;
			channel->iface.Write = dvcman_write_channel;
			channel->iface.Close = dvcman_close_channel_iface;
			bAccept = TRUE;

			if ((error = listener->listener_callback->OnNewChannelConnection(
			         listener->listener_callback, &channel->iface, NULL, &bAccept, &pCallback)) ==
			        CHANNEL_RC_OK &&
			    bAccept)
			{
				WLog_Print(drdynvc->log, WLOG_DEBUG, "listener %s created new channel %" PRIu32 "",
				           listener->channel_name, channel->channel_id);
				channel->status = CHANNEL_RC_OK;
				channel->channel_callback = pCallback;
				channel->pInterface = listener->iface.pInterface;
				context = dvcman->drdynvc->context;
				IFCALLRET(context->OnChannelConnected, error, context, ChannelName,
				          listener->iface.pInterface);

				if (error)
					WLog_Print(drdynvc->log, WLOG_ERROR,
					           "context.OnChannelConnected failed with error %" PRIu32 "", error);

				goto fail;
			}
			else
			{
				if (error)
				{
					WLog_Print(drdynvc->log, WLOG_ERROR,
					           "OnNewChannelConnection failed with error %" PRIu32 "!", error);
					goto fail;
				}
				else
				{
					WLog_Print(drdynvc->log, WLOG_ERROR,
					           "OnNewChannelConnection returned with bAccept FALSE!");
					error = ERROR_INTERNAL_ERROR;
					goto fail;
				}
			}
		}
	}
	error = ERROR_INTERNAL_ERROR;
fail:
	ArrayList_Unlock(dvcman->listeners);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_open_channel(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr,
                                UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannelCallback* pCallback;
	UINT error;
	channel = (DVCMAN_CHANNEL*)dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "ChannelId %" PRIu32 " not found!", ChannelId);
		return ERROR_INTERNAL_ERROR;
	}

	if (channel->status == CHANNEL_RC_OK)
	{
		pCallback = channel->channel_callback;

		if (pCallback->OnOpen)
		{
			error = pCallback->OnOpen(pCallback);
			if (error)
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "OnOpen failed with error %" PRIu32 "!",
				           error);
				return error;
			}
		}

		WLog_Print(drdynvc->log, WLOG_DEBUG, "open_channel: ChannelId %" PRIu32 "", ChannelId);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId,
                          BOOL bSendClosePDU)
{
	DVCMAN_CHANNEL* channel;
	UINT error = CHANNEL_RC_OK;
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	drdynvcPlugin* drdynvc = dvcman->drdynvc;
	channel = (DVCMAN_CHANNEL*)dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		// WLog_Print(drdynvc->log, WLOG_ERROR, "ChannelId %"PRIu32" not found!", ChannelId);
		/**
		 * Windows 8 / Windows Server 2012 send close requests for channels that failed to be
		 * created. Do not warn, simply return success here.
		 */
		return CHANNEL_RC_OK;
	}

	if (drdynvc && bSendClosePDU)
	{
		wStream* s = StreamPool_Take(dvcman->pool, 5);
		if (!s)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
			error = CHANNEL_RC_NO_MEMORY;
		}
		else
		{
			Stream_Write_UINT8(s, (CLOSE_REQUEST_PDU << 4) | 0x02);
			Stream_Write_UINT32(s, ChannelId);
			error = drdynvc_send(drdynvc, s);
		}
	}

	ArrayList_Remove(dvcman->channels, channel);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_receive_channel_data_first(drdynvcPlugin* drdynvc,
                                              IWTSVirtualChannelManager* pChannelMgr,
                                              UINT32 ChannelId, UINT32 length)
{
	DVCMAN_CHANNEL* channel;
	channel = (DVCMAN_CHANNEL*)dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		/**
		 * Windows Server 2012 R2 can send some messages over
		 * Microsoft::Windows::RDS::Geometry::v08.01 even if the dynamic virtual channel wasn't
		 * registered on our side. Ignoring it works.
		 */
		WLog_Print(drdynvc->log, WLOG_ERROR, "ChannelId %" PRIu32 " not found!", ChannelId);
		return CHANNEL_RC_OK;
	}

	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	channel->dvc_data = StreamPool_Take(channel->dvcman->pool, length);

	if (!channel->dvc_data)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
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
static UINT dvcman_receive_channel_data(drdynvcPlugin* drdynvc,
                                        IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId,
                                        wStream* data, UINT32 ThreadingFlags)
{
	UINT status = CHANNEL_RC_OK;
	DVCMAN_CHANNEL* channel;
	size_t dataSize = Stream_GetRemainingLength(data);
	channel = (DVCMAN_CHANNEL*)dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (!channel)
	{
		/* Windows 8.1 tries to open channels not created.
		 * Ignore cases like this. */
		WLog_Print(drdynvc->log, WLOG_ERROR, "ChannelId %" PRIu32 " not found!", ChannelId);
		return CHANNEL_RC_OK;
	}

	if (channel->dvc_data)
	{
		/* Fragmented data */
		if (Stream_GetPosition(channel->dvc_data) + dataSize > channel->dvc_data_length)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "data exceeding declared length!");
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
			return ERROR_INVALID_DATA;
		}

		Stream_Copy(data, channel->dvc_data, dataSize);

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
		status = channel->channel_callback->OnDataReceived(channel->channel_callback, data);
	}

	return status;
}

static UINT8 drdynvc_write_variable_uint(wStream* s, UINT32 val)
{
	UINT8 cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(s, (UINT8)val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(s, (UINT16)val);
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
		WINPR_ASSERT(drdynvc->channelEntryPoints.pVirtualChannelWriteEx);
		status = drdynvc->channelEntryPoints.pVirtualChannelWriteEx(
		    drdynvc->InitHandle, drdynvc->OpenHandle, Stream_Buffer(s),
		    (UINT32)Stream_GetPosition(s), s);
	}

	switch (status)
	{
		case CHANNEL_RC_OK:
			return CHANNEL_RC_OK;

		case CHANNEL_RC_NOT_CONNECTED:
			Stream_Release(s);
			return CHANNEL_RC_OK;

		case CHANNEL_RC_BAD_CHANNEL_HANDLE:
			Stream_Release(s);
			WLog_ERR(TAG, "VirtualChannelWriteEx failed with CHANNEL_RC_BAD_CHANNEL_HANDLE");
			return status;

		default:
			Stream_Release(s);
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "VirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
			           WTSErrorToString(status), status);
			return status;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_write_data(drdynvcPlugin* drdynvc, UINT32 ChannelId, const BYTE* data,
                               UINT32 dataSize, BOOL* close)
{
	wStream* data_out;
	size_t pos;
	UINT8 cbChId;
	UINT8 cbLen;
	unsigned long chunkLength;
	UINT status = CHANNEL_RC_BAD_INIT_HANDLE;
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;

	WLog_Print(drdynvc->log, WLOG_TRACE, "write_data: ChannelId=%" PRIu32 " size=%" PRIu32 "",
	           ChannelId, dataSize);
	data_out = StreamPool_Take(dvcman->pool, CHANNEL_CHUNK_LENGTH);

	if (!data_out)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_SetPosition(data_out, 1);
	cbChId = drdynvc_write_variable_uint(data_out, ChannelId);
	pos = Stream_GetPosition(data_out);

	if (dataSize == 0)
	{
		*close = TRUE;
		Stream_Release(data_out);
	}
	else if (dataSize <= CHANNEL_CHUNK_LENGTH - pos)
	{
		Stream_SetPosition(data_out, 0);
		Stream_Write_UINT8(data_out, (DATA_PDU << 4) | cbChId);
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
		Stream_Write_UINT8(data_out, (DATA_FIRST_PDU << 4) | cbChId | (cbLen << 2));
		Stream_SetPosition(data_out, pos);
		chunkLength = CHANNEL_CHUNK_LENGTH - pos;
		Stream_Write(data_out, data, chunkLength);
		data += chunkLength;
		dataSize -= chunkLength;
		status = drdynvc_send(drdynvc, data_out);

		while (status == CHANNEL_RC_OK && dataSize > 0)
		{
			data_out = StreamPool_Take(dvcman->pool, CHANNEL_CHUNK_LENGTH);

			if (!data_out)
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
				return CHANNEL_RC_NO_MEMORY;
			}

			Stream_SetPosition(data_out, 1);
			cbChId = drdynvc_write_variable_uint(data_out, ChannelId);
			pos = Stream_GetPosition(data_out);
			Stream_SetPosition(data_out, 0);
			Stream_Write_UINT8(data_out, (DATA_PDU << 4) | cbChId);
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
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
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
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;
	WLog_Print(drdynvc->log, WLOG_TRACE, "capability_response");
	s = StreamPool_Take(dvcman->pool, 4);

	if (!s)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_Ndrdynvc_write_variable_uintew failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(s, 0x0050); /* Cmd+Sp+cbChId+Pad. Note: MSTSC sends 0x005c */
	Stream_Write_UINT16(s, drdynvc->version);
	status = drdynvc_send(drdynvc, s);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_capability_request(drdynvcPlugin* drdynvc, int Sp, int cbChId,
                                               wStream* s)
{
	UINT status;

	if (!drdynvc)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 3))
		return ERROR_INVALID_DATA;

	WLog_Print(drdynvc->log, WLOG_TRACE, "capability_request Sp=%d cbChId=%d", Sp, cbChId);
	Stream_Seek(s, 1); /* pad */
	Stream_Read_UINT16(s, drdynvc->version);

	/* RDP8 servers offer version 3, though Microsoft forgot to document it
	 * in their early documents.  It behaves the same as version 2.
	 */
	if ((drdynvc->version == 2) || (drdynvc->version == 3))
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT16(s, drdynvc->PriorityCharge0);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge1);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge2);
		Stream_Read_UINT16(s, drdynvc->PriorityCharge3);
	}

	status = drdynvc_send_capability_response(drdynvc);
	drdynvc->state = DRDYNVC_STATE_READY;
	return status;
}

static UINT32 drdynvc_cblen_to_bytes(int cbLen)
{
	switch (cbLen)
	{
		case 0:
			return 1;

		case 1:
			return 2;

		default:
			return 4;
	}
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
static UINT drdynvc_process_create_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	size_t pos;
	UINT status;
	UINT32 ChannelId;
	wStream* data_out;
	UINT channel_status;
	char* name;
	size_t length;
	DVCMAN* dvcman;

	WINPR_UNUSED(Sp);
	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;
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

	if (!Stream_CheckAndLogRequiredLength(TAG, s, drdynvc_cblen_to_bytes(cbChId)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	pos = Stream_GetPosition(s);
	name = (char*)Stream_Pointer(s);
	length = Stream_GetRemainingLength(s);

	if (strnlen(name, length) >= length)
		return ERROR_INVALID_DATA;

	WLog_Print(drdynvc->log, WLOG_DEBUG,
	           "process_create_request: ChannelId=%" PRIu32 " ChannelName=%s", ChannelId, name);
	channel_status = dvcman_create_channel(drdynvc, drdynvc->channel_mgr, ChannelId, name);
	data_out = StreamPool_Take(dvcman->pool, pos + 4);

	if (!data_out)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(data_out, (CREATE_REQUEST_PDU << 4) | cbChId);
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
		Stream_Write_UINT32(data_out, (UINT32)0xC0000001); /* same code used by mstsc */
	}

	status = drdynvc_send(drdynvc, data_out);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
		return status;
	}

	if (channel_status == CHANNEL_RC_OK)
	{
		if ((status = dvcman_open_channel(drdynvc, drdynvc->channel_mgr, ChannelId)))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "dvcman_open_channel failed with error %" PRIu32 "!", status);
			return status;
		}
	}
	else
	{
		if ((status = dvcman_close_channel(drdynvc->channel_mgr, ChannelId, FALSE)))
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "dvcman_close_channel failed with error %" PRIu32 "!", status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_data_first(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s,
                                       UINT32 ThreadingFlags)
{
	UINT status;
	UINT32 Length;
	UINT32 ChannelId;

	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, drdynvc_cblen_to_bytes(cbChId) + drdynvc_cblen_to_bytes(Sp)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	WLog_Print(drdynvc->log, WLOG_TRACE,
	           "process_data_first: Sp=%d cbChId=%d, ChannelId=%" PRIu32 " Length=%" PRIu32 "", Sp,
	           cbChId, ChannelId, Length);
	status = dvcman_receive_channel_data_first(drdynvc, drdynvc->channel_mgr, ChannelId, Length);

	if (status == CHANNEL_RC_OK)
		status = dvcman_receive_channel_data(drdynvc, drdynvc->channel_mgr, ChannelId, s,
		                                     ThreadingFlags);

	if (status != CHANNEL_RC_OK)
		status = dvcman_close_channel(drdynvc->channel_mgr, ChannelId, TRUE);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_data(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s,
                                 UINT32 ThreadingFlags)
{
	UINT32 ChannelId;
	UINT status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, drdynvc_cblen_to_bytes(cbChId)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_Print(drdynvc->log, WLOG_TRACE, "process_data: Sp=%d cbChId=%d, ChannelId=%" PRIu32 "", Sp,
	           cbChId, ChannelId);
	status =
	    dvcman_receive_channel_data(drdynvc, drdynvc->channel_mgr, ChannelId, s, ThreadingFlags);

	if (status != CHANNEL_RC_OK)
		status = dvcman_close_channel(drdynvc->channel_mgr, ChannelId, TRUE);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT error;
	UINT32 ChannelId;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, drdynvc_cblen_to_bytes(cbChId)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_Print(drdynvc->log, WLOG_DEBUG,
	           "process_close_request: Sp=%d cbChId=%d, ChannelId=%" PRIu32 "", Sp, cbChId,
	           ChannelId);

	if ((error = dvcman_close_channel(drdynvc->channel_mgr, ChannelId, TRUE)))
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_close_channel failed with error %" PRIu32 "!",
		           error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_order_recv(drdynvcPlugin* drdynvc, wStream* s, UINT32 ThreadingFlags)
{
	int value;
	int Cmd;
	int Sp;
	int cbChId;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, value);
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;
	WLog_Print(drdynvc->log, WLOG_TRACE, "order_recv: Cmd=0x%x, Sp=%d cbChId=%d", Cmd, Sp, cbChId);

	switch (Cmd)
	{
		case CAPABILITY_REQUEST_PDU:
			return drdynvc_process_capability_request(drdynvc, Sp, cbChId, s);

		case CREATE_REQUEST_PDU:
			return drdynvc_process_create_request(drdynvc, Sp, cbChId, s);

		case DATA_FIRST_PDU:
			return drdynvc_process_data_first(drdynvc, Sp, cbChId, s, ThreadingFlags);

		case DATA_PDU:
			return drdynvc_process_data(drdynvc, Sp, cbChId, s, ThreadingFlags);

		case CLOSE_REQUEST_PDU:
			return drdynvc_process_close_request(drdynvc, Sp, cbChId, s);

		default:
			WLog_Print(drdynvc->log, WLOG_ERROR, "unknown drdynvc cmd 0x%x", Cmd);
			return ERROR_INTERNAL_ERROR;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_virtual_channel_event_data_received(drdynvcPlugin* drdynvc, void* pData,
                                                        UINT32 dataLength, UINT32 totalLength,
                                                        UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		DVCMAN* mgr = (DVCMAN*)drdynvc->channel_mgr;
		if (drdynvc->data_in)
			Stream_Release(drdynvc->data_in);

		drdynvc->data_in = StreamPool_Take(mgr->pool, totalLength);
	}

	if (!(data_in = drdynvc->data_in))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_EnsureRemainingCapacity(data_in, dataLength))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
		Stream_Release(drdynvc->data_in);
		drdynvc->data_in = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		const size_t cap = Stream_Capacity(data_in);
		const size_t pos = Stream_GetPosition(data_in);
		if (cap < pos)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "drdynvc_plugin_process_received: read error");
			return ERROR_INVALID_DATA;
		}

		drdynvc->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (drdynvc->async)
		{
			if (!MessageQueue_Post(drdynvc->queue, NULL, 0, (void*)data_in, NULL))
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_Post failed!");
				return ERROR_INTERNAL_ERROR;
			}
		}
		else
		{
			UINT error = drdynvc_order_recv(drdynvc, data_in, TRUE);
			Stream_Release(data_in);

			if (error)
			{
				WLog_Print(drdynvc->log, WLOG_WARN,
				           "drdynvc_order_recv failed with error %" PRIu32 "!", error);
			}
		}
	}

	return CHANNEL_RC_OK;
}

static void VCAPITYPE drdynvc_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT32 dataLength, UINT32 totalLength,
                                                            UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)lpUserParam;

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if (!drdynvc || (drdynvc->OpenHandle != openHandle))
			{
				WLog_ERR(TAG, "drdynvc_virtual_channel_open_event: error no match");
				return;
			}
			if ((error = drdynvc_virtual_channel_event_data_received(drdynvc, pData, dataLength,
			                                                         totalLength, dataFlags)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_data_received failed with error %" PRIu32
				           "",
				           error);

			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
		case CHANNEL_EVENT_WRITE_COMPLETE:
		{
			wStream* s = (wStream*)pData;
			Stream_Release(s);
		}
		break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && drdynvc && drdynvc->rdpcontext)
		setChannelError(drdynvc->rdpcontext, error,
		                "drdynvc_virtual_channel_open_event reported an error");
}

static DWORD WINAPI drdynvc_virtual_channel_client_thread(LPVOID arg)
{
	/* TODO: rewrite this */
	wStream* data;
	wMessage message;
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)arg;

	if (!drdynvc)
	{
		ExitThread((DWORD)CHANNEL_RC_BAD_CHANNEL_HANDLE);
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
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
			UINT32 ThreadingFlags = TRUE;
			data = (wStream*)message.wParam;

			if ((error = drdynvc_order_recv(drdynvc, data, ThreadingFlags)))
			{
				WLog_Print(drdynvc->log, WLOG_WARN,
				           "drdynvc_order_recv failed with error %" PRIu32 "!", error);
			}

			Stream_Release(data);
		}
	}

	{
		/* Disconnect remaining dynamic channels that the server did not.
		 * This is required to properly shut down channels by calling the appropriate
		 * event handlers. */
		size_t count = 0;
		DVCMAN* drdynvcMgr = (DVCMAN*)drdynvc->channel_mgr;

		do
		{
			ArrayList_Lock(drdynvcMgr->channels);
			count = ArrayList_Count(drdynvcMgr->channels);
			if (count > 0)
			{
				IWTSVirtualChannel* channel =
				    (IWTSVirtualChannel*)ArrayList_GetItem(drdynvcMgr->channels, 0);
				const UINT32 ChannelId = drdynvc->channel_mgr->GetChannelId(channel);
				dvcman_close_channel(drdynvc->channel_mgr, ChannelId, FALSE);
				count--;
			}
			ArrayList_Unlock(drdynvcMgr->channels);
		} while (count > 0);
	}

	if (error && drdynvc->rdpcontext)
		setChannelError(drdynvc->rdpcontext, error,
		                "drdynvc_virtual_channel_client_thread reported an error");

	ExitThread((DWORD)error);
	return error;
}

static void drdynvc_queue_object_free(void* obj)
{
	wStream* s;
	wMessage* msg = (wMessage*)obj;

	if (!msg || (msg->id != 0))
		return;

	s = (wStream*)msg->wParam;

	if (s)
		Stream_Release(s);
}

static UINT drdynvc_virtual_channel_event_initialized(drdynvcPlugin* drdynvc, LPVOID pData,
                                                      UINT32 dataLength)
{
	wObject* obj;
	WINPR_UNUSED(pData);
	WINPR_UNUSED(dataLength);

	if (!drdynvc)
		goto error;

	drdynvc->queue = MessageQueue_New(NULL);

	if (!drdynvc->queue)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "MessageQueue_New failed!");
		goto error;
	}

	obj = MessageQueue_Object(drdynvc->queue);
	obj->fnObjectFree = drdynvc_queue_object_free;
	drdynvc->channel_mgr = dvcman_new(drdynvc);

	if (!drdynvc->channel_mgr)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_new failed!");
		goto error;
	}

	return CHANNEL_RC_OK;
error:
	return ERROR_INTERNAL_ERROR;
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
	rdpSettings* settings;

	WINPR_ASSERT(drdynvc);
	WINPR_UNUSED(pData);
	WINPR_UNUSED(dataLength);

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	WINPR_ASSERT(drdynvc->channelEntryPoints.pVirtualChannelOpenEx);
	status = drdynvc->channelEntryPoints.pVirtualChannelOpenEx(
	    drdynvc->InitHandle, &drdynvc->OpenHandle, drdynvc->channelDef.name,
	    drdynvc_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelOpen failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
		return status;
	}

	WINPR_ASSERT(drdynvc->rdpcontext);
	settings = drdynvc->rdpcontext->settings;
	WINPR_ASSERT(settings);

	for (index = 0; index < freerdp_settings_get_uint32(settings, FreeRDP_DynamicChannelCount);
	     index++)
	{
		const ADDIN_ARGV* args = settings->DynamicChannelArray[index];
		error = dvcman_load_addin(drdynvc, drdynvc->channel_mgr, args, drdynvc->rdpcontext);

		if (CHANNEL_RC_OK != error)
			goto error;
	}

	if ((error = dvcman_init(drdynvc, drdynvc->channel_mgr)))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_init failed with error %" PRIu32 "!", error);
		goto error;
	}

	drdynvc->state = DRDYNVC_STATE_CAPABILITIES;

	if (drdynvc->async)
	{
		if (!(drdynvc->thread = CreateThread(NULL, 0, drdynvc_virtual_channel_client_thread,
		                                     (void*)drdynvc, 0, NULL)))
		{
			error = ERROR_INTERNAL_ERROR;
			WLog_Print(drdynvc->log, WLOG_ERROR, "CreateThread failed!");
			goto error;
		}
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

	if (drdynvc->OpenHandle == 0)
		return CHANNEL_RC_OK;

	if (drdynvc->queue)
	{
		if (!MessageQueue_PostQuit(drdynvc->queue, 0))
		{
			status = GetLastError();
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "MessageQueue_PostQuit failed with error %" PRIu32 "", status);
			return status;
		}
	}

	if (drdynvc->thread)
	{
		if (WaitForSingleObject(drdynvc->thread, INFINITE) != WAIT_OBJECT_0)
		{
			status = GetLastError();
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "", status);
			return status;
		}

		CloseHandle(drdynvc->thread);
		drdynvc->thread = NULL;
	}

	WINPR_ASSERT(drdynvc->channelEntryPoints.pVirtualChannelCloseEx);
	status = drdynvc->channelEntryPoints.pVirtualChannelCloseEx(drdynvc->InitHandle,
	                                                            drdynvc->OpenHandle);

	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelClose failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
	}

	dvcman_clear(drdynvc, drdynvc->channel_mgr);
	if (drdynvc->queue)
		MessageQueue_Clear(drdynvc->queue);
	drdynvc->OpenHandle = 0;

	if (drdynvc->data_in)
	{
		Stream_Release(drdynvc->data_in);
		drdynvc->data_in = NULL;
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

	MessageQueue_Free(drdynvc->queue);
	drdynvc->queue = NULL;

	if (drdynvc->channel_mgr)
	{
		dvcman_free(drdynvc, drdynvc->channel_mgr);
		drdynvc->channel_mgr = NULL;
	}
	drdynvc->InitHandle = 0;
	free(drdynvc->context);
	free(drdynvc);
	return CHANNEL_RC_OK;
}

static UINT drdynvc_virtual_channel_event_attached(drdynvcPlugin* drdynvc)
{
	UINT error = CHANNEL_RC_OK;
	size_t i;
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;

	if (!dvcman)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	ArrayList_Lock(dvcman->plugins);
	for (i = 0; i < ArrayList_Count(dvcman->plugins); i++)
	{
		IWTSPlugin* pPlugin = ArrayList_GetItem(dvcman->plugins, i);

		error = IFCALLRESULT(CHANNEL_RC_OK, pPlugin->Attached, pPlugin);
		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "Attach failed with error %" PRIu32 "!", error);
			goto fail;
		}
	}

fail:
	ArrayList_Unlock(dvcman->plugins);
	return error;
}

static UINT drdynvc_virtual_channel_event_detached(drdynvcPlugin* drdynvc)
{
	UINT error = CHANNEL_RC_OK;
	size_t i;
	DVCMAN* dvcman;

	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;

	if (!dvcman)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	ArrayList_Lock(dvcman->plugins);
	for (i = 0; i < ArrayList_Count(dvcman->plugins); i++)
	{
		IWTSPlugin* pPlugin = ArrayList_GetItem(dvcman->plugins, i);

		error = IFCALLRESULT(CHANNEL_RC_OK, pPlugin->Detached, pPlugin);
		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "Detach failed with error %" PRIu32 "!", error);
			goto fail;
		}
	}

fail:
	ArrayList_Unlock(dvcman->plugins);

	return error;
}

static VOID VCAPITYPE drdynvc_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)lpUserParam;

	if (!drdynvc || (drdynvc->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "drdynvc_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_INITIALIZED:
			error = drdynvc_virtual_channel_event_initialized(drdynvc, pData, dataLength);
			break;
		case CHANNEL_EVENT_CONNECTED:
			if ((error = drdynvc_virtual_channel_event_connected(drdynvc, pData, dataLength)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_connected failed with error %" PRIu32 "",
				           error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = drdynvc_virtual_channel_event_disconnected(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_disconnected failed with error %" PRIu32
				           "",
				           error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			if ((error = drdynvc_virtual_channel_event_terminated(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_terminated failed with error %" PRIu32 "",
				           error);

			break;

		case CHANNEL_EVENT_ATTACHED:
			if ((error = drdynvc_virtual_channel_event_attached(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_attached failed with error %" PRIu32 "",
				           error);

			break;

		case CHANNEL_EVENT_DETACHED:
			if ((error = drdynvc_virtual_channel_event_detached(drdynvc)))
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "drdynvc_virtual_channel_event_detached failed with error %" PRIu32 "",
				           error);

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
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)context->handle;
	return drdynvc->version;
}

/* drdynvc is always built-in */
#define VirtualChannelEntryEx drdynvc_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS_EX pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	drdynvcPlugin* drdynvc;
	DrdynvcClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	drdynvc = (drdynvcPlugin*)calloc(1, sizeof(drdynvcPlugin));

	if (!drdynvc)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	drdynvc->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP;
	sprintf_s(drdynvc->channelDef.name, ARRAYSIZE(drdynvc->channelDef.name),
	          DRDYNVC_SVC_CHANNEL_NAME);
	drdynvc->state = DRDYNVC_STATE_INITIAL;
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (DrdynvcClientContext*)calloc(1, sizeof(DrdynvcClientContext));

		if (!context)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "calloc failed!");
			free(drdynvc);
			return FALSE;
		}

		context->handle = (void*)drdynvc;
		context->custom = NULL;
		drdynvc->context = context;
		context->GetVersion = drdynvc_get_version;
		drdynvc->rdpcontext = pEntryPointsEx->context;
		if (!freerdp_settings_get_bool(drdynvc->rdpcontext->settings, FreeRDP_TransportDumpReplay))
			drdynvc->async = TRUE;
	}

	drdynvc->log = WLog_Get(TAG);
	WLog_Print(drdynvc->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(drdynvc->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	drdynvc->InitHandle = pInitHandle;

	WINPR_ASSERT(drdynvc->channelEntryPoints.pVirtualChannelInitEx);
	rc = drdynvc->channelEntryPoints.pVirtualChannelInitEx(
	    drdynvc, context, pInitHandle, &drdynvc->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	    drdynvc_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "pVirtualChannelInit failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(rc), rc);
		free(drdynvc->context);
		free(drdynvc);
		return FALSE;
	}

	drdynvc->channelEntryPoints.pInterface = context;
	return TRUE;
}
