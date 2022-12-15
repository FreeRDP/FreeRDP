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
#include <winpr/interlocked.h>

#include <freerdp/channels/drdynvc.h>

#include "drdynvc_main.h"

#define TAG CHANNELS_TAG("drdynvc.client")

static void dvcman_channel_free(DVCMAN_CHANNEL* channel);
static UINT dvcman_channel_close(DVCMAN_CHANNEL* channel, BOOL perRequest, BOOL fromHashTableFn);
static void dvcman_free(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr);
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
	WINPR_ASSERT(ppPropertyBag);
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

	WINPR_ASSERT(dvcman);
	WLog_DBG(TAG, "create_listener: %" PRIuz ".%s.", HashTable_Count(dvcman->listeners) + 1,
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

	if (!HashTable_Insert(dvcman->listeners, listener->channel_name, listener))
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
			HashTable_Remove(dvcman->listeners, listener->channel_name);
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
	WINPR_ASSERT(pEntryPoints);
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*)pEntryPoints)->dvcman;

	WINPR_ASSERT(dvcman);
	if (!ArrayList_Append(dvcman->plugin_names, _strdup(name)))
		return ERROR_INTERNAL_ERROR;
	if (!ArrayList_Append(dvcman->plugins, pPlugin))
		return ERROR_INTERNAL_ERROR;

	WLog_DBG(TAG, "register_plugin: num_plugins %" PRIuz, ArrayList_Count(dvcman->plugins));
	return CHANNEL_RC_OK;
}

static IWTSPlugin* dvcman_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name)
{
	IWTSPlugin* plugin = NULL;
	size_t i, nc, pc;
	WINPR_ASSERT(pEntryPoints);
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
	WINPR_ASSERT(pEntryPoints);
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
	WINPR_ASSERT(dvc);
	return dvc->channel_id;
}

static const char* dvcman_get_channel_name(IWTSVirtualChannel* channel)
{
	DVCMAN_CHANNEL* dvc = (DVCMAN_CHANNEL*)channel;
	WINPR_ASSERT(dvc);
	return dvc->channel_name;
}

static DVCMAN_CHANNEL* dvcman_get_channel_by_id(IWTSVirtualChannelManager* pChannelMgr,
                                                UINT32 ChannelId, BOOL doRef)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	DVCMAN_CHANNEL* dvcChannel;

	WINPR_ASSERT(dvcman);
	HashTable_Lock(dvcman->channelsById);
	dvcChannel = HashTable_GetItemValue(dvcman->channelsById, &ChannelId);
	if (dvcChannel)
	{
		if (doRef)
			InterlockedIncrement(&dvcChannel->refCounter);
	}

	HashTable_Unlock(dvcman->channelsById);
	return dvcChannel;
}

static IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager* pChannelMgr,
                                                     UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel = dvcman_get_channel_by_id(pChannelMgr, ChannelId, FALSE);
	if (!channel)
		return NULL;

	return &channel->iface;
}

static void dvcman_plugin_terminate(void* plugin)
{
	IWTSPlugin* pPlugin = plugin;

	WINPR_ASSERT(pPlugin);
	UINT error = IFCALLRESULT(CHANNEL_RC_OK, pPlugin->Terminated, pPlugin);
	if (error != CHANNEL_RC_OK)
		WLog_ERR(TAG, "Terminated failed with error %" PRIu32 "!", error);
}

static void wts_listener_free(void* arg)
{
	DVCMAN_LISTENER* listener = (DVCMAN_LISTENER*)arg;
	dvcman_wtslistener_free(listener);
}

static BOOL channelIdMatch(const void* k1, const void* k2)
{
	WINPR_ASSERT(k1);
	WINPR_ASSERT(k2);
	return *((const UINT32*)k1) == *((const UINT32*)k2);
}

static UINT32 channelIdHash(const void* id)
{
	WINPR_ASSERT(id);
	return *((const UINT32*)id);
}

static void channelByIdCleanerFn(void* value)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*)value;
	if (channel)
	{
		dvcman_channel_close(channel, FALSE, TRUE);
		dvcman_channel_free(channel);
	}
}

static IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	wObject* obj;
	DVCMAN* dvcman = (DVCMAN*)calloc(1, sizeof(DVCMAN));

	if (!dvcman)
		return NULL;

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.DestroyListener = dvcman_destroy_listener;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->iface.GetChannelName = dvcman_get_channel_name;
	dvcman->drdynvc = plugin;
	dvcman->channelsById = HashTable_New(TRUE);

	if (!dvcman->channelsById)
		goto fail;

	HashTable_SetHashFunction(dvcman->channelsById, channelIdHash);
	obj = HashTable_KeyObject(dvcman->channelsById);
	WINPR_ASSERT(obj);
	obj->fnObjectEquals = channelIdMatch;

	obj = HashTable_ValueObject(dvcman->channelsById);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = channelByIdCleanerFn;

	dvcman->pool = StreamPool_New(TRUE, 10);
	if (!dvcman->pool)
		goto fail;

	dvcman->listeners = HashTable_New(TRUE);
	if (!dvcman->listeners)
		goto fail;
	HashTable_SetHashFunction(dvcman->listeners, HashTable_StringHash);

	obj = HashTable_KeyObject(dvcman->listeners);
	obj->fnObjectEquals = HashTable_StringCompare;

	obj = HashTable_ValueObject(dvcman->listeners);
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

static void dvcman_channel_free(DVCMAN_CHANNEL* channel)
{
	if (!channel)
		return;

	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	DeleteCriticalSection(&(channel->lock));
	free(channel->channel_name);
	free(channel);
}

static void dvcman_channel_unref(DVCMAN_CHANNEL* channel)
{
	DVCMAN* dvcman;

	WINPR_ASSERT(channel);
	if (InterlockedDecrement(&channel->refCounter))
		return;

	dvcman = channel->dvcman;
	HashTable_Remove(dvcman->channelsById, &channel->channel_id);
}

static UINT dvcchannel_send_close(DVCMAN_CHANNEL* channel)
{
	WINPR_ASSERT(channel);
	DVCMAN* dvcman = channel->dvcman;
	drdynvcPlugin* drdynvc = dvcman->drdynvc;
	wStream* s = StreamPool_Take(dvcman->pool, 5);

	if (!s)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(s, (CLOSE_REQUEST_PDU << 4) | 0x02);
	Stream_Write_UINT32(s, channel->channel_id);
	return drdynvc_send(drdynvc, s);
}

static void check_open_close_receive(DVCMAN_CHANNEL* channel)
{
	WINPR_ASSERT(channel);

	IWTSVirtualChannelCallback* cb = channel->channel_callback;
	const char* name = channel->channel_name;
	const UINT32 id = channel->channel_id;

	WINPR_ASSERT(cb);
	if (cb->OnOpen || cb->OnClose)
	{
		if (!cb->OnOpen || !cb->OnClose)
			WLog_WARN(TAG, "[%s] {%s:%" PRIu32 "} OnOpen=%p, OnClose=%p", __FUNCTION__, name, id,
			          cb->OnOpen, cb->OnClose);
	}
}

static UINT dvcman_call_on_receive(DVCMAN_CHANNEL* channel, wStream* data)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(data);

	IWTSVirtualChannelCallback* cb = channel->channel_callback;
	WINPR_ASSERT(cb);

	check_open_close_receive(channel);
	WINPR_ASSERT(cb->OnDataReceived);
	return cb->OnDataReceived(cb, data);
}

static UINT dvcman_channel_close(DVCMAN_CHANNEL* channel, BOOL perRequest, BOOL fromHashTableFn)
{
	UINT error = CHANNEL_RC_OK;
	drdynvcPlugin* drdynvc;
	DrdynvcClientContext* context;

	WINPR_ASSERT(channel);
	switch (channel->state)
	{
		case DVC_CHANNEL_INIT:
			break;
		case DVC_CHANNEL_RUNNING:
			drdynvc = channel->dvcman->drdynvc;
			context = drdynvc->context;
			if (perRequest)
				WLog_Print(drdynvc->log, WLOG_DEBUG, "sending close confirm for '%s'",
				           channel->channel_name);

			error = dvcchannel_send_close(channel);
			if (error != CHANNEL_RC_OK)
			{
				const char* msg = "error when sending close confirm for '%s'";
				if (perRequest)
					msg = "error when sending closeRequest for '%s'";

				WLog_Print(drdynvc->log, WLOG_DEBUG, msg, channel->channel_name);
			}

			channel->state = DVC_CHANNEL_CLOSED;

			IWTSVirtualChannelCallback* cb = channel->channel_callback;
			if (cb)
			{
				check_open_close_receive(channel);
				IFCALL(cb->OnClose, cb);
			}

			channel->channel_callback = NULL;

			if (channel->dvcman && channel->dvcman->drdynvc)
			{
				if (context)
				{
					IFCALLRET(context->OnChannelDisconnected, error, context, channel->channel_name,
					          channel->pInterface);
				}
			}

			if (!fromHashTableFn)
				dvcman_channel_unref(channel);
			break;
		case DVC_CHANNEL_CLOSED:
			break;
	}

	return error;
}

static DVCMAN_CHANNEL* dvcman_channel_new(drdynvcPlugin* drdynvc,
                                          IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId,
                                          const char* ChannelName)
{
	DVCMAN_CHANNEL* channel;

	WINPR_ASSERT(drdynvc);
	WINPR_ASSERT(pChannelMgr);
	channel = (DVCMAN_CHANNEL*)calloc(1, sizeof(DVCMAN_CHANNEL));

	if (!channel)
		return NULL;

	channel->dvcman = (DVCMAN*)pChannelMgr;
	channel->channel_id = ChannelId;
	channel->refCounter = 1;
	channel->state = DVC_CHANNEL_INIT;
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

static void dvcman_clear(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;

	WINPR_ASSERT(dvcman);
	WINPR_UNUSED(drdynvc);

	HashTable_Clear(dvcman->channelsById);
	ArrayList_Clear(dvcman->plugins);
	ArrayList_Clear(dvcman->plugin_names);
	HashTable_Clear(dvcman->listeners);
}
static void dvcman_free(drdynvcPlugin* drdynvc, IWTSVirtualChannelManager* pChannelMgr)
{
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;

	WINPR_ASSERT(dvcman);
	WINPR_UNUSED(drdynvc);

	HashTable_Free(dvcman->channelsById);
	ArrayList_Free(dvcman->plugins);
	ArrayList_Free(dvcman->plugin_names);
	HashTable_Free(dvcman->listeners);

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

	WINPR_ASSERT(dvcman);
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
		dvcman_channel_close(channel, FALSE, FALSE);

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
	return dvcman_channel_close(channel, FALSE, FALSE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static DVCMAN_CHANNEL* dvcman_create_channel(drdynvcPlugin* drdynvc,
                                             IWTSVirtualChannelManager* pChannelMgr,
                                             UINT32 ChannelId, const char* ChannelName, UINT* res)
{
	BOOL bAccept;
	DVCMAN_CHANNEL* channel = NULL;
	DrdynvcClientContext* context;
	DVCMAN* dvcman = (DVCMAN*)pChannelMgr;
	DVCMAN_LISTENER* listener;
	IWTSVirtualChannelCallback* pCallback = NULL;

	WINPR_ASSERT(dvcman);
	WINPR_ASSERT(res);

	HashTable_Lock(dvcman->listeners);
	listener = (DVCMAN_LISTENER*)HashTable_GetItemValue(dvcman->listeners, ChannelName);
	if (!listener)
	{
		*res = ERROR_NOT_FOUND;
		goto out;
	}

	channel = dvcman_get_channel_by_id(pChannelMgr, ChannelId, FALSE);
	if (channel)
	{
		switch (channel->state)
		{
			case DVC_CHANNEL_RUNNING:
				WLog_Print(drdynvc->log, WLOG_ERROR,
				           "Protocol error: Duplicated ChannelId %" PRIu32 " (%s)!", ChannelId,
				           ChannelName);
				*res = CHANNEL_RC_ALREADY_OPEN;
				goto out;

			case DVC_CHANNEL_CLOSED:
			case DVC_CHANNEL_INIT:
			default:
				WLog_Print(drdynvc->log, WLOG_ERROR, "not expecting a createChannel from state %d",
				           channel->state);
				*res = CHANNEL_RC_INITIALIZATION_ERROR;
				goto out;
		}
	}
	else
	{
		if (!(channel = dvcman_channel_new(drdynvc, pChannelMgr, ChannelId, ChannelName)))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_channel_new failed!");
			*res = CHANNEL_RC_NO_MEMORY;
			goto out;
		}
	}

	if (!HashTable_Insert(dvcman->channelsById, &channel->channel_id, channel))
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "unable to register channel in our channel list");
		*res = ERROR_INTERNAL_ERROR;
		dvcman_channel_free(channel);
		channel = NULL;
		goto out;
	}

	channel->iface.Write = dvcman_write_channel;
	channel->iface.Close = dvcman_close_channel_iface;
	bAccept = TRUE;

	*res = listener->listener_callback->OnNewChannelConnection(
	    listener->listener_callback, &channel->iface, NULL, &bAccept, &pCallback);

	if (*res != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR,
		           "OnNewChannelConnection failed with error %" PRIu32 "!", *res);
		*res = ERROR_INTERNAL_ERROR;
		dvcman_channel_unref(channel);
		goto out;
	}

	if (!bAccept)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "OnNewChannelConnection returned with bAccept FALSE!");
		*res = ERROR_INTERNAL_ERROR;
		dvcman_channel_unref(channel);
		channel = NULL;
		goto out;
	}

	WLog_Print(drdynvc->log, WLOG_DEBUG, "listener %s created new channel %" PRIu32 "",
	           listener->channel_name, channel->channel_id);
	channel->state = DVC_CHANNEL_RUNNING;
	channel->channel_callback = pCallback;
	channel->pInterface = listener->iface.pInterface;
	context = dvcman->drdynvc->context;

	IFCALLRET(context->OnChannelConnected, *res, context, ChannelName, listener->iface.pInterface);
	if (*res != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR,
		           "context.OnChannelConnected failed with error %" PRIu32 "", *res);
	}

out:
	HashTable_Unlock(dvcman->listeners);

	return channel;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_open_channel(drdynvcPlugin* drdynvc, DVCMAN_CHANNEL* channel)
{
	IWTSVirtualChannelCallback* pCallback;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(drdynvc);
	WINPR_ASSERT(channel);
	if (channel->state == DVC_CHANNEL_RUNNING)
	{
		pCallback = channel->channel_callback;

		if (pCallback->OnOpen)
		{
			check_open_close_receive(channel);
			error = pCallback->OnOpen(pCallback);
			if (error)
			{
				WLog_Print(drdynvc->log, WLOG_ERROR, "OnOpen failed with error %" PRIu32 "!",
				           error);
				goto out;
			}
		}

		WLog_Print(drdynvc->log, WLOG_DEBUG, "open_channel: ChannelId %" PRIu32 "",
		           channel->channel_id);
	}

out:
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT dvcman_receive_channel_data_first(DVCMAN_CHANNEL* channel, UINT32 length)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->dvcman);
	if (channel->dvc_data)
		Stream_Release(channel->dvc_data);

	channel->dvc_data = StreamPool_Take(channel->dvcman->pool, length);

	if (!channel->dvc_data)
	{
		drdynvcPlugin* drdynvc = channel->dvcman->drdynvc;
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
static UINT dvcman_receive_channel_data(DVCMAN_CHANNEL* channel, wStream* data,
                                        UINT32 ThreadingFlags)
{
	UINT status = CHANNEL_RC_OK;
	size_t dataSize = Stream_GetRemainingLength(data);

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->dvcman);
	if (channel->dvc_data)
	{
		drdynvcPlugin* drdynvc = channel->dvcman->drdynvc;

		/* Fragmented data */
		if (Stream_GetPosition(channel->dvc_data) + dataSize > channel->dvc_data_length)
		{
			WLog_Print(drdynvc->log, WLOG_ERROR, "data exceeding declared length!");
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
			status = ERROR_INVALID_DATA;
			goto out;
		}

		Stream_Copy(data, channel->dvc_data, dataSize);

		if (Stream_GetPosition(channel->dvc_data) >= channel->dvc_data_length)
		{
			Stream_SealLength(channel->dvc_data);
			Stream_SetPosition(channel->dvc_data, 0);

			status = dvcman_call_on_receive(channel, channel->dvc_data);
			Stream_Release(channel->dvc_data);
			channel->dvc_data = NULL;
		}
	}
	else
		status = dvcman_call_on_receive(channel, data);

out:
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
	WINPR_ASSERT(dvcman);

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
		/* TODO: shall treat that case with write(0) that do a close */
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
	WINPR_ASSERT(dvcman);

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
	DVCMAN_CHANNEL* channel;
	UINT32 retStatus;

	WINPR_UNUSED(Sp);
	if (!drdynvc)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	dvcman = (DVCMAN*)drdynvc->channel_mgr;
	WINPR_ASSERT(dvcman);

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

	data_out = StreamPool_Take(dvcman->pool, pos + 4);
	if (!data_out)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "StreamPool_Take failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(data_out, (CREATE_REQUEST_PDU << 4) | cbChId);
	Stream_SetPosition(s, 1);
	Stream_Copy(s, data_out, pos - 1);

	channel =
	    dvcman_create_channel(drdynvc, drdynvc->channel_mgr, ChannelId, name, &channel_status);
	switch (channel_status)
	{
		case CHANNEL_RC_OK:
			WLog_Print(drdynvc->log, WLOG_DEBUG, "channel created");
			retStatus = 0;
			break;
		case CHANNEL_RC_NO_MEMORY:
			WLog_Print(drdynvc->log, WLOG_DEBUG, "not enough memory for channel creation");
			retStatus = STATUS_NO_MEMORY;
			break;
		case ERROR_NOT_FOUND:
			WLog_Print(drdynvc->log, WLOG_DEBUG, "no listener for '%s'", name);
			retStatus = (UINT32)0xC0000001; /* same code used by mstsc, STATUS_UNSUCCESSFUL */
			break;
		default:
			WLog_Print(drdynvc->log, WLOG_DEBUG, "channel creation error");
			retStatus = (UINT32)0xC0000001; /* same code used by mstsc, STATUS_UNSUCCESSFUL */
			break;
	}
	Stream_Write_UINT32(data_out, retStatus);

	status = drdynvc_send(drdynvc, data_out);
	if (status != CHANNEL_RC_OK)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "VirtualChannelWriteEx failed with %s [%08" PRIX32 "]",
		           WTSErrorToString(status), status);
		dvcman_channel_unref(channel);
		return status;
	}

	if (channel_status == CHANNEL_RC_OK)
	{
		if ((status = dvcman_open_channel(drdynvc, channel)))
		{
			WLog_Print(drdynvc->log, WLOG_ERROR,
			           "dvcman_open_channel failed with error %" PRIu32 "!", status);
			return status;
		}
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
	UINT status = CHANNEL_RC_OK;
	UINT32 Length;
	UINT32 ChannelId;
	DVCMAN_CHANNEL* channel;

	WINPR_ASSERT(drdynvc);
	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, drdynvc_cblen_to_bytes(cbChId) + drdynvc_cblen_to_bytes(Sp)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	Length = drdynvc_read_variable_uint(s, Sp);
	WLog_Print(drdynvc->log, WLOG_TRACE,
	           "process_data_first: Sp=%d cbChId=%d, ChannelId=%" PRIu32 " Length=%" PRIu32 "", Sp,
	           cbChId, ChannelId, Length);

	channel = dvcman_get_channel_by_id(drdynvc->channel_mgr, ChannelId, TRUE);
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

	if (channel->state != DVC_CHANNEL_RUNNING)
		goto out;

	status = dvcman_receive_channel_data_first(channel, Length);

	if (status == CHANNEL_RC_OK)
		status = dvcman_receive_channel_data(channel, s, ThreadingFlags);

	if (status != CHANNEL_RC_OK)
		status = dvcman_channel_close(channel, FALSE, FALSE);

out:
	dvcman_channel_unref(channel);
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
	DVCMAN_CHANNEL* channel;
	UINT status = CHANNEL_RC_OK;

	WINPR_ASSERT(drdynvc);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, drdynvc_cblen_to_bytes(cbChId)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_Print(drdynvc->log, WLOG_TRACE, "process_data: Sp=%d cbChId=%d, ChannelId=%" PRIu32 "", Sp,
	           cbChId, ChannelId);

	channel = dvcman_get_channel_by_id(drdynvc->channel_mgr, ChannelId, TRUE);
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

	if (channel->state != DVC_CHANNEL_RUNNING)
		goto out;

	status = dvcman_receive_channel_data(channel, s, ThreadingFlags);
	if (status != CHANNEL_RC_OK)
		status = dvcman_channel_close(channel, FALSE, FALSE);

out:
	dvcman_channel_unref(channel);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_process_close_request(drdynvcPlugin* drdynvc, int Sp, int cbChId, wStream* s)
{
	UINT32 ChannelId;
	DVCMAN_CHANNEL* channel;

	WINPR_ASSERT(drdynvc);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, drdynvc_cblen_to_bytes(cbChId)))
		return ERROR_INVALID_DATA;

	ChannelId = drdynvc_read_variable_uint(s, cbChId);
	WLog_Print(drdynvc->log, WLOG_DEBUG,
	           "process_close_request: Sp=%d cbChId=%d, ChannelId=%" PRIu32 "", Sp, cbChId,
	           ChannelId);

	channel = (DVCMAN_CHANNEL*)dvcman_get_channel_by_id(drdynvc->channel_mgr, ChannelId, TRUE);
	if (!channel)
	{
		WLog_Print(drdynvc->log, WLOG_ERROR, "dvcman_close_request channel %" PRIu32 " not present",
		           ChannelId);
		return CHANNEL_RC_OK;
	}

	dvcman_channel_close(channel, TRUE, FALSE);
	dvcman_channel_unref(channel);
	return CHANNEL_RC_OK;
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

	WINPR_ASSERT(drdynvc);
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

	WINPR_ASSERT(drdynvc);
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

	WINPR_ASSERT(drdynvc);
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
		DVCMAN* drdynvcMgr = (DVCMAN*)drdynvc->channel_mgr;

		HashTable_Clear(drdynvcMgr->channelsById);
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
	else
	{
		{
			/* Disconnect remaining dynamic channels that the server did not.
			 * This is required to properly shut down channels by calling the appropriate
			 * event handlers. */
			DVCMAN* drdynvcMgr = (DVCMAN*)drdynvc->channel_mgr;

			HashTable_Clear(drdynvcMgr->channelsById);
		}
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
	WINPR_ASSERT(context);
	drdynvcPlugin* drdynvc = (drdynvcPlugin*)context->handle;
	WINPR_ASSERT(drdynvc);
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

	WINPR_ASSERT(pEntryPoints);
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
