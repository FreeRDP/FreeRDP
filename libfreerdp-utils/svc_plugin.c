/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Static Virtual Channel Interface
 *
 * Copyright 2009-2011 Jay Sorg
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

#ifdef WITH_DEBUG_SVC
#define DEBUG_SVC(fmt, ...) DEBUG_CLASS(SVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

/* The list of all plugin instances. */
typedef struct rdp_svc_plugin_list rdpSvcPluginList;
struct rdp_svc_plugin_list
{
	rdpSvcPlugin* plugin;
	rdpSvcPluginList* next;
};

static rdpSvcPluginList* g_svc_plugin_list = NULL;

/* For locking the global resources */
static freerdp_mutex g_mutex = NULL;

struct rdp_svc_plugin_private
{
	void* init_handle;
	uint32 open_handle;
	STREAM* data_in;
};

static rdpSvcPlugin* svc_plugin_find_by_init_handle(void* init_handle)
{
	rdpSvcPluginList * list;
	rdpSvcPlugin * plugin;

	freerdp_mutex_lock(g_mutex);
	for (list = g_svc_plugin_list; list; list = list->next)
	{
		plugin = list->plugin;
		if (plugin->priv->init_handle == init_handle)
		{
			freerdp_mutex_unlock(g_mutex);
			return plugin;
		}
	}
	freerdp_mutex_unlock(g_mutex);
	return NULL;
}

static rdpSvcPlugin* svc_plugin_find_by_open_handle(uint32 open_handle)
{
	rdpSvcPluginList * list;
	rdpSvcPlugin * plugin;

	freerdp_mutex_lock(g_mutex);
	for (list = g_svc_plugin_list; list; list = list->next)
	{
		plugin = list->plugin;
		if (plugin->priv->open_handle == open_handle)
		{
			freerdp_mutex_unlock(g_mutex);
			return plugin;
		}
	}
	freerdp_mutex_unlock(g_mutex);
	return NULL;
}

static void svc_plugin_remove(rdpSvcPlugin* plugin)
{
	rdpSvcPluginList* list;
	rdpSvcPluginList* prev;

	/* Remove from global list */
	freerdp_mutex_lock(g_mutex);
	for (prev = NULL, list = g_svc_plugin_list; list; prev = list, list = list->next)
	{
		if (list->plugin == plugin)
			break;
	}
	if (list)
	{
		if (prev)
			prev->next = list->next;
		else
			g_svc_plugin_list = list->next;
		xfree(list);
	}
	freerdp_mutex_unlock(g_mutex);
}

static void svc_plugin_process_received(rdpSvcPlugin* plugin, void* pData, uint32 dataLength,
	uint32 totalLength, uint32 dataFlags)
{
	STREAM* data_in;

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (plugin->priv->data_in != NULL)
			stream_free(plugin->priv->data_in);
		plugin->priv->data_in = stream_new(totalLength);
	}

	data_in = plugin->priv->data_in;
	stream_check_size(data_in, dataLength);
	stream_write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (stream_get_size(data_in) != stream_get_length(data_in))
		{
			printf("svc_plugin_process_received: read error\n");
		}
		/* the stream ownership is passed to the callback who is responsible for freeing it. */
		plugin->priv->data_in = NULL;
		stream_set_pos(data_in, 0);
		plugin->receive_callback(plugin, data_in);
	}
}

static void svc_plugin_open_event(uint32 openHandle, uint32 event, void* pData, uint32 dataLength,
	uint32 totalLength, uint32 dataFlags)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("openHandle %d event %d dataLength %d totalLength %d dataFlags %d",
		openHandle, event, dataLength, totalLength, dataFlags);

	plugin = (rdpSvcPlugin*)svc_plugin_find_by_open_handle(openHandle);
	if (plugin == NULL)
	{
		printf("svc_plugin_open_event: error no match\n");
		return;
	}
	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			svc_plugin_process_received(plugin, pData, dataLength, totalLength, dataFlags);
			break;
		case CHANNEL_EVENT_WRITE_COMPLETE:
			stream_free((STREAM*)pData);
			break;
		case CHANNEL_EVENT_USER:
			plugin->event_callback(plugin, (FRDP_EVENT*)pData);
			break;
	}
}

static void svc_plugin_process_connected(rdpSvcPlugin* plugin, void* pData, uint32 dataLength)
{
	uint32 error;

	error = plugin->channel_entry_points.pVirtualChannelOpen(plugin->priv->init_handle, &plugin->priv->open_handle,
		plugin->channel_def.name, svc_plugin_open_event);
	if (error != CHANNEL_RC_OK)
	{
		printf("svc_plugin_process_connected: open failed\n");
		return;
	}
	plugin->connect_callback(plugin);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	plugin->channel_entry_points.pVirtualChannelClose(plugin->priv->open_handle);

	svc_plugin_remove(plugin);

	if (plugin->priv->data_in != NULL)
	{
		stream_free(plugin->priv->data_in);
		plugin->priv->data_in = NULL;
	}
	xfree(plugin->priv);
	plugin->priv = NULL;

	plugin->terminate_callback(plugin);
}

static void svc_plugin_init_event(void* pInitHandle, uint32 event, void* pData, uint32 dataLength)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("event %d", event);

	plugin = (rdpSvcPlugin*)svc_plugin_find_by_init_handle(pInitHandle);
	if (plugin == NULL)
	{
		printf("svc_plugin_init_event: error no match\n");
		return;
	}
	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			svc_plugin_process_connected(plugin, pData, dataLength);
			break;
		case CHANNEL_EVENT_DISCONNECTED:
			break;
		case CHANNEL_EVENT_TERMINATED:
			svc_plugin_process_terminated(plugin);
			break;
	}
}

void svc_plugin_init(rdpSvcPlugin* plugin, CHANNEL_ENTRY_POINTS* pEntryPoints)
{
	rdpSvcPluginList* list;

	/**
	 * The channel manager will guarantee only one thread can call
	 * VirtualChannelInit at a time. So this should be safe.
	 */
	if (g_mutex == NULL)
		g_mutex = freerdp_mutex_new();

	memcpy(&plugin->channel_entry_points, pEntryPoints, pEntryPoints->cbSize);

	plugin->priv = (rdpSvcPluginPrivate*)xmalloc(sizeof(rdpSvcPluginPrivate));
	memset(plugin->priv, 0, sizeof(rdpSvcPluginPrivate));

	/* Add it to the global list */
	list = (rdpSvcPluginList*)xmalloc(sizeof(rdpSvcPluginList));
	list->plugin = plugin;

	freerdp_mutex_lock(g_mutex);
	list->next = g_svc_plugin_list;
	g_svc_plugin_list = list;
	freerdp_mutex_unlock(g_mutex);

	plugin->channel_entry_points.pVirtualChannelInit(&plugin->priv->init_handle,
		&plugin->channel_def, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, svc_plugin_init_event);
}

int svc_plugin_send(rdpSvcPlugin* plugin, STREAM* data_out)
{
	uint32 error = 0;

	DEBUG_SVC("length %d", stream_get_length(data_out));

	error = plugin->channel_entry_points.pVirtualChannelWrite(plugin->priv->open_handle,
		stream_get_data(data_out), stream_get_length(data_out), data_out);
	if (error != CHANNEL_RC_OK)
		printf("svc_plugin_send: VirtualChannelWrite failed %d\n", error);

	return error;
}

int svc_plugin_send_event(rdpSvcPlugin* plugin, FRDP_EVENT* event)
{
	uint32 error = 0;

	DEBUG_SVC("event_type %d", event->event_type);

	error = plugin->channel_entry_points.pVirtualChannelEventPush(plugin->priv->open_handle,
		event);
	if (error != CHANNEL_RC_OK)
		printf("svc_plugin_send_event: VirtualChannelEventPush failed %d\n", error);

	return error;
}
