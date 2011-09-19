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
#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/svc_plugin.h>

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

/* Queue for receiving packets */
struct _svc_data_in_item
{
	STREAM* data_in;
	RDP_EVENT* event_in;
};
typedef struct _svc_data_in_item svc_data_in_item;

static void svc_data_in_item_free(svc_data_in_item* item)
{
	if (item->data_in)
	{
		stream_free(item->data_in);
		item->data_in = NULL;
	}
	if (item->event_in)
	{
		freerdp_event_free(item->event_in);
		item->event_in = NULL;
	}
	xfree(item);
}

struct rdp_svc_plugin_private
{
	void* init_handle;
	uint32 open_handle;
	STREAM* data_in;

	LIST* data_in_list;
	freerdp_thread* thread;
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
	svc_data_in_item* item;

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (plugin->priv->data_in != NULL)
			stream_free(plugin->priv->data_in);
		plugin->priv->data_in = stream_new(totalLength);
	}

	data_in = plugin->priv->data_in;
	stream_check_size(data_in, (int) dataLength);
	stream_write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (stream_get_size(data_in) != stream_get_length(data_in))
		{
			printf("svc_plugin_process_received: read error\n");
		}

		plugin->priv->data_in = NULL;
		stream_set_pos(data_in, 0);

		item = xnew(svc_data_in_item);
		item->data_in = data_in;

		freerdp_thread_lock(plugin->priv->thread);
		list_enqueue(plugin->priv->data_in_list, item);
		freerdp_thread_unlock(plugin->priv->thread);

		freerdp_thread_signal(plugin->priv->thread);
	}
}

static void svc_plugin_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event_in)
{
	svc_data_in_item* item;

	item = xnew(svc_data_in_item);
	item->event_in = event_in;

	freerdp_thread_lock(plugin->priv->thread);
	list_enqueue(plugin->priv->data_in_list, item);
	freerdp_thread_unlock(plugin->priv->thread);

	freerdp_thread_signal(plugin->priv->thread);
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
			svc_plugin_process_event(plugin, (RDP_EVENT*)pData);
			break;
	}
}

static void svc_plugin_process_data_in(rdpSvcPlugin* plugin)
{
	svc_data_in_item* item;

	while (1)
	{
		/* terminate signal */
		if (freerdp_thread_is_stopped(plugin->priv->thread))
			break;

		freerdp_thread_lock(plugin->priv->thread);
		item = list_dequeue(plugin->priv->data_in_list);
		freerdp_thread_unlock(plugin->priv->thread);

		if (item != NULL)
		{
			/* the ownership of the data is passed to the callback */
			if (item->data_in)
				IFCALL(plugin->receive_callback, plugin, item->data_in);
			if (item->event_in)
				IFCALL(plugin->event_callback, plugin, item->event_in);
			xfree(item);
		}
		else
			break;
	}
}

static void* svc_plugin_thread_func(void* arg)
{
	rdpSvcPlugin* plugin = (rdpSvcPlugin*)arg;

	DEBUG_SVC("in");

	IFCALL(plugin->connect_callback, plugin);

	while (1)
	{
		if (plugin->interval_ms > 0)
			freerdp_thread_wait_timeout(plugin->priv->thread, plugin->interval_ms);
		else
			freerdp_thread_wait(plugin->priv->thread);

		if (freerdp_thread_is_stopped(plugin->priv->thread))
			break;

		freerdp_thread_reset(plugin->priv->thread);
		svc_plugin_process_data_in(plugin);

		if (plugin->interval_ms > 0)
			IFCALL(plugin->interval_callback, plugin);
	}

	freerdp_thread_quit(plugin->priv->thread);

	DEBUG_SVC("out");

	return 0;
}

static void svc_plugin_process_connected(rdpSvcPlugin* plugin, void* pData, uint32 dataLength)
{
	uint32 error;

	error = plugin->channel_entry_points.pVirtualChannelOpen(plugin->priv->init_handle,
		&plugin->priv->open_handle, plugin->channel_def.name, svc_plugin_open_event);

	if (error != CHANNEL_RC_OK)
	{
		printf("svc_plugin_process_connected: open failed\n");
		return;
	}

	plugin->priv->data_in_list = list_new();
	plugin->priv->thread = freerdp_thread_new();

	freerdp_thread_start(plugin->priv->thread, svc_plugin_thread_func, plugin);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	svc_data_in_item* item;

	freerdp_thread_stop(plugin->priv->thread);
	freerdp_thread_free(plugin->priv->thread);

	plugin->channel_entry_points.pVirtualChannelClose(plugin->priv->open_handle);
	xfree(plugin->channel_entry_points.pExtendedData);

	svc_plugin_remove(plugin);

	while ((item = list_dequeue(plugin->priv->data_in_list)) != NULL)
		svc_data_in_item_free(item);
	list_free(plugin->priv->data_in_list);

	if (plugin->priv->data_in != NULL)
	{
		stream_free(plugin->priv->data_in);
		plugin->priv->data_in = NULL;
	}
	xfree(plugin->priv);
	plugin->priv = NULL;

	IFCALL(plugin->terminate_callback, plugin);
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

	plugin->priv = xnew(rdpSvcPluginPrivate);

	/* Add it to the global list */
	list = xnew(rdpSvcPluginList);
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
	{
		stream_free(data_out);
		printf("svc_plugin_send: VirtualChannelWrite failed %d\n", error);
	}

	return error;
}

int svc_plugin_send_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	uint32 error = 0;

	DEBUG_SVC("event_type %d", event->event_type);

	error = plugin->channel_entry_points.pVirtualChannelEventPush(plugin->priv->open_handle, event);

	if (error != CHANNEL_RC_OK)
		printf("svc_plugin_send_event: VirtualChannelEventPush failed %d\n", error);

	return error;
}
