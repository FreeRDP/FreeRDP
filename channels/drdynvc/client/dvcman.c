/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Manager
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
#include <winpr/synch.h>

#include <freerdp/addin.h>

#include <winpr/stream.h>
#include <freerdp/utils/list.h>

#include "drdynvc_types.h"
#include "dvcman.h"

#define MAX_PLUGINS 10

typedef struct _DVCMAN DVCMAN;

struct _DVCMAN
{
	IWTSVirtualChannelManager iface;

	drdynvcPlugin* drdynvc;

	const char* plugin_names[MAX_PLUGINS];
	IWTSPlugin* plugins[MAX_PLUGINS];
	int num_plugins;

	IWTSListener* listeners[MAX_PLUGINS];
	int num_listeners;

	LIST* channels;
};

typedef struct _DVCMAN_LISTENER DVCMAN_LISTENER;
struct _DVCMAN_LISTENER
{
	IWTSListener iface;

	DVCMAN* dvcman;
	char* channel_name;
	UINT32 flags;
	IWTSListenerCallback* listener_callback;
};

typedef struct _DVCMAN_ENTRY_POINTS DVCMAN_ENTRY_POINTS;
struct _DVCMAN_ENTRY_POINTS
{
	IDRDYNVC_ENTRY_POINTS iface;

	DVCMAN* dvcman;
	ADDIN_ARGV* args;
};

typedef struct _DVCMAN_CHANNEL DVCMAN_CHANNEL;
struct _DVCMAN_CHANNEL
{
	IWTSVirtualChannel iface;

	DVCMAN* dvcman;
	DVCMAN_CHANNEL* next;
	UINT32 channel_id;
	IWTSVirtualChannelCallback* channel_callback;

	wStream* dvc_data;

	HANDLE dvc_chan_mutex;
};

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
		DEBUG_DVC("%d.%s.", dvcman->num_listeners, pszChannelName);

		listener = (DVCMAN_LISTENER*) malloc(sizeof(DVCMAN_LISTENER));
		ZeroMemory(listener, sizeof(DVCMAN_LISTENER));

		listener->iface.GetConfiguration = dvcman_get_configuration;
		listener->dvcman = dvcman;
		listener->channel_name = _strdup(pszChannelName);
		listener->flags = ulFlags;
		listener->listener_callback = pListenerCallback;

		if (ppListener)
			*ppListener = (IWTSListener*)listener;

		dvcman->listeners[dvcman->num_listeners++] = (IWTSListener*)listener;
		
		return 0;
	}
	else
	{
		DEBUG_WARN("Maximum DVC listener number reached.");
		return 1;
	}
}

static int dvcman_push_event(IWTSVirtualChannelManager* pChannelMgr, RDP_EVENT* pEvent)
{
	int status;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	status = drdynvc_push_event(dvcman->drdynvc, pEvent);

	if (status == 0)
	{
		DEBUG_DVC("event_type %d pushed.", pEvent->event_type);
	}
	else
	{
		DEBUG_WARN("event_type %d push failed.", pEvent->event_type);
	}

	return status;
}

static int dvcman_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* name, IWTSPlugin* pPlugin)
{
	DVCMAN* dvcman = ((DVCMAN_ENTRY_POINTS*) pEntryPoints)->dvcman;

	if (dvcman->num_plugins < MAX_PLUGINS)
	{
		DEBUG_DVC("num_plugins %d", dvcman->num_plugins);
		dvcman->plugin_names[dvcman->num_plugins] = name;
		dvcman->plugins[dvcman->num_plugins++] = pPlugin;
		return 0;
	}
	else
	{
		DEBUG_WARN("Maximum DVC plugin number reached.");
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

UINT32 dvcman_get_channel_id(IWTSVirtualChannel * channel)
{
	return ((DVCMAN_CHANNEL*) channel)->channel_id;
}

IWTSVirtualChannel* dvcman_find_channel_by_id(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	LIST_ITEM* curr;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (curr = dvcman->channels->head; curr; curr = curr->next)
	{
		if (((DVCMAN_CHANNEL*) curr->data)->channel_id == ChannelId)
		{
			return (IWTSVirtualChannel*) curr->data;
		}
	}

	return NULL;
}

IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin)
{
	DVCMAN* dvcman;

	dvcman = (DVCMAN*) malloc(sizeof(DVCMAN));
	ZeroMemory(dvcman, sizeof(DVCMAN));

	dvcman->iface.CreateListener = dvcman_create_listener;
	dvcman->iface.PushEvent = dvcman_push_event;
	dvcman->iface.FindChannelById = dvcman_find_channel_by_id;
	dvcman->iface.GetChannelId = dvcman_get_channel_id;
	dvcman->drdynvc = plugin;
	dvcman->channels = list_new();

	return (IWTSVirtualChannelManager*) dvcman;
}

int dvcman_load_addin(IWTSVirtualChannelManager* pChannelMgr, ADDIN_ARGV* args)
{
	DVCMAN_ENTRY_POINTS entryPoints;
	PDVC_PLUGIN_ENTRY pDVCPluginEntry = NULL;

	printf("Loading Dynamic Virtual Channel %s\n", args->argv[0]);

	pDVCPluginEntry = (PDVC_PLUGIN_ENTRY) freerdp_load_channel_addin_entry(args->argv[0],
			NULL, NULL, FREERDP_ADDIN_CHANNEL_DYNAMIC);

	if (pDVCPluginEntry != NULL)
	{
		entryPoints.iface.RegisterPlugin = dvcman_register_plugin;
		entryPoints.iface.GetPlugin = dvcman_get_plugin;
		entryPoints.iface.GetPluginData = dvcman_get_plugin_data;
		entryPoints.dvcman = (DVCMAN*) pChannelMgr;
		entryPoints.args = args;
		pDVCPluginEntry((IDRDYNVC_ENTRY_POINTS*) &entryPoints);
	}

	return 0;
}

static void dvcman_channel_free(DVCMAN_CHANNEL* channel)
{
	if (channel->channel_callback)
		channel->channel_callback->OnClose(channel->channel_callback);

	free(channel);
}

void dvcman_free(IWTSVirtualChannelManager* pChannelMgr)
{
	int i;
	IWTSPlugin* pPlugin;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	while ((channel = (DVCMAN_CHANNEL*) list_dequeue(dvcman->channels)) != NULL)
		dvcman_channel_free(channel);

	list_free(dvcman->channels);

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];
		free(listener->channel_name);
		free(listener);
	}

	for (i = 0; i < dvcman->num_plugins; i++)
	{
		pPlugin = dvcman->plugins[i];

		if (pPlugin->Terminated)
			pPlugin->Terminated(pPlugin);
	}

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

	WaitForSingleObject(channel->dvc_chan_mutex, INFINITE);
	status = drdynvc_write_data(channel->dvcman->drdynvc, channel->channel_id, pBuffer, cbSize);
	ReleaseMutex(channel->dvc_chan_mutex);

	return status;
}

static int dvcman_close_channel_iface(IWTSVirtualChannel* pChannel)
{
	DVCMAN_CHANNEL* channel = (DVCMAN_CHANNEL*) pChannel;
	DVCMAN* dvcman = channel->dvcman;

	DEBUG_DVC("id=%d", channel->channel_id);

	if (list_remove(dvcman->channels, channel) == NULL)
		DEBUG_WARN("channel not found");

	dvcman_channel_free(channel);

	return 1;
}

int dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, const char* ChannelName)
{
	int i;
	int bAccept;
	DVCMAN_LISTENER* listener;
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannelCallback* pCallback;
	DVCMAN* dvcman = (DVCMAN*) pChannelMgr;

	for (i = 0; i < dvcman->num_listeners; i++)
	{
		listener = (DVCMAN_LISTENER*) dvcman->listeners[i];

		if (strcmp(listener->channel_name, ChannelName) == 0)
		{
			channel = (DVCMAN_CHANNEL*) malloc(sizeof(DVCMAN_CHANNEL));
			ZeroMemory(channel, sizeof(DVCMAN_CHANNEL));

			channel->iface.Write = dvcman_write_channel;
			channel->iface.Close = dvcman_close_channel_iface;
			channel->dvcman = dvcman;
			channel->channel_id = ChannelId;
			channel->dvc_chan_mutex = CreateMutex(NULL, FALSE, NULL);

			bAccept = 1;
			pCallback = NULL;

			if (listener->listener_callback->OnNewChannelConnection(listener->listener_callback,
				(IWTSVirtualChannel*) channel, NULL, &bAccept, &pCallback) == 0 && bAccept == 1)
			{
				DEBUG_DVC("listener %s created new channel %d",
					  listener->channel_name, channel->channel_id);
				channel->channel_callback = pCallback;
				list_add(dvcman->channels, channel);

				return 0;
			}
			else
			{
				DEBUG_WARN("channel rejected by plugin");
				dvcman_channel_free(channel);
				return 1;
			}
		}
	}

	return 1;
}

int dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId)
{
	DVCMAN_CHANNEL* channel;
	IWTSVirtualChannel* ichannel;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (channel == NULL)
	{
		DEBUG_WARN("ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
	{
		stream_free(channel->dvc_data);
		channel->dvc_data = NULL;
	}

	DEBUG_DVC("dvcman_close_channel: channel %d closed", ChannelId);
	ichannel = (IWTSVirtualChannel*) channel;
	ichannel->Close(ichannel);

	return 0;
}

int dvcman_receive_channel_data_first(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, UINT32 length)
{
	DVCMAN_CHANNEL* channel;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (channel == NULL)
	{
		DEBUG_WARN("ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
		stream_free(channel->dvc_data);

	channel->dvc_data = stream_new(length);

	return 0;
}

int dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, BYTE* data, UINT32 data_size)
{
	int error = 0;
	DVCMAN_CHANNEL* channel;

	channel = (DVCMAN_CHANNEL*) dvcman_find_channel_by_id(pChannelMgr, ChannelId);

	if (channel == NULL)
	{
		DEBUG_WARN("ChannelId %d not found!", ChannelId);
		return 1;
	}

	if (channel->dvc_data)
	{
		/* Fragmented data */
		if (stream_get_length(channel->dvc_data) + data_size > (UINT32) stream_get_size(channel->dvc_data))
		{
			DEBUG_WARN("data exceeding declared length!");
			stream_free(channel->dvc_data);
			channel->dvc_data = NULL;
			return 1;
		}

		stream_write(channel->dvc_data, data, data_size);

		if (stream_get_length(channel->dvc_data) >= stream_get_size(channel->dvc_data))
		{
			error = channel->channel_callback->OnDataReceived(channel->channel_callback,
				stream_get_size(channel->dvc_data), stream_get_data(channel->dvc_data));
			stream_free(channel->dvc_data);
			channel->dvc_data = NULL;
		}
	}
	else
	{
		error = channel->channel_callback->OnDataReceived(channel->channel_callback, data_size, data);
	}

	return error;
}
