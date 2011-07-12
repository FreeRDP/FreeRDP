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
#include <time.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/wait_obj.h>
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
struct svc_data_in_item
{
	STREAM* data_in;
	FRDP_EVENT* event_in;
};

DEFINE_LIST_TYPE(svc_data_in_list, svc_data_in_item);

void svc_data_in_item_free(struct svc_data_in_item* item)
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
}

struct rdp_svc_plugin_private
{
	void* init_handle;
	uint32 open_handle;
	STREAM* data_in;

	struct svc_data_in_list* data_in_list;
	freerdp_mutex* data_in_mutex;

	struct wait_obj* signals[5];
	int num_signals;

	int thread_status;
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
	struct svc_data_in_item* item;

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

		plugin->priv->data_in = NULL;
		stream_set_pos(data_in, 0);

		item = svc_data_in_item_new();
		item->data_in = data_in;

		freerdp_mutex_lock(plugin->priv->data_in_mutex);
		svc_data_in_list_enqueue(plugin->priv->data_in_list, item);
		freerdp_mutex_unlock(plugin->priv->data_in_mutex);

		wait_obj_set(plugin->priv->signals[1]);
	}
}

static void svc_plugin_process_event(rdpSvcPlugin* plugin, FRDP_EVENT* event_in)
{
	struct svc_data_in_item* item;

	item = svc_data_in_item_new();
	item->event_in = event_in;

	freerdp_mutex_lock(plugin->priv->data_in_mutex);
	svc_data_in_list_enqueue(plugin->priv->data_in_list, item);
	freerdp_mutex_unlock(plugin->priv->data_in_mutex);

	wait_obj_set(plugin->priv->signals[1]);
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
			svc_plugin_process_event(plugin, (FRDP_EVENT*)pData);
			break;
	}
}

static void svc_plugin_process_data_in(rdpSvcPlugin* plugin)
{
	struct svc_data_in_item* item;

	while (1)
	{
		/* terminate signal */
		if (wait_obj_is_set(plugin->priv->signals[0]))
			break;

		freerdp_mutex_lock(plugin->priv->data_in_mutex);
		item = svc_data_in_list_dequeue(plugin->priv->data_in_list);
		freerdp_mutex_unlock(plugin->priv->data_in_mutex);

		if (item != NULL)
		{
			/* the ownership of the data is passed to the callback */
			if (item->data_in)
				plugin->receive_callback(plugin, item->data_in);
			if (item->event_in)
				plugin->event_callback(plugin, item->event_in);
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

	plugin->connect_callback(plugin);

	while (1)
	{
		wait_obj_select(plugin->priv->signals, plugin->priv->num_signals, -1);

		/* terminate signal */
		if (wait_obj_is_set(plugin->priv->signals[0]))
			break;

		/* data_in signal */
		if (wait_obj_is_set(plugin->priv->signals[1]))
		{
			wait_obj_clear(plugin->priv->signals[1]);
			/* process data in */
			svc_plugin_process_data_in(plugin);
		}
	}

	plugin->priv->thread_status = -1;

	DEBUG_SVC("out");

	return 0;
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

	plugin->priv->data_in_list = svc_data_in_list_new();
	plugin->priv->data_in_mutex = freerdp_mutex_new();

	/* terminate signal */
	plugin->priv->signals[plugin->priv->num_signals++] = wait_obj_new();
	/* data_in signal */
	plugin->priv->signals[plugin->priv->num_signals++] = wait_obj_new();

	plugin->priv->thread_status = 1;

	freerdp_thread_create(svc_plugin_thread_func, plugin);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	struct timespec ts;
	int i;

	wait_obj_set(plugin->priv->signals[0]);
	i = 0;
	ts.tv_sec = 0;
	ts.tv_nsec = 10000000;
	while (plugin->priv->thread_status > 0 && i < 1000)
	{
		i++;
		nanosleep(&ts, NULL);
	}

	plugin->channel_entry_points.pVirtualChannelClose(plugin->priv->open_handle);

	svc_plugin_remove(plugin);

	for (i = 0; i < plugin->priv->num_signals; i++)
		wait_obj_free(plugin->priv->signals[i]);
	plugin->priv->num_signals = 0;

	freerdp_mutex_free(plugin->priv->data_in_mutex);
	svc_data_in_list_free(plugin->priv->data_in_list);

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
	{
		stream_free(data_out);
		printf("svc_plugin_send: VirtualChannelWrite failed %d\n", error);
	}

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
