/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/collections.h>

#include <freerdp/constants.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/svc_plugin.h>

static wArrayList* g_AddinList = NULL;

static rdpSvcPlugin* svc_plugin_find_by_init_handle(void* init_handle)
{
	int index;
	BOOL found = FALSE;
	rdpSvcPlugin* plugin;

	ArrayList_Lock(g_AddinList);

	index = 0;
	plugin = (rdpSvcPlugin*) ArrayList_GetItem(g_AddinList, index++);

	while (plugin)
	{
		if (plugin->init_handle == init_handle)
		{
			found = TRUE;
			break;
		}

		plugin = (rdpSvcPlugin*) ArrayList_GetItem(g_AddinList, index++);
	}

	ArrayList_Unlock(g_AddinList);

	return (found) ? plugin : NULL;
}

static rdpSvcPlugin* svc_plugin_find_by_open_handle(UINT32 open_handle)
{
	int index;
	BOOL found = FALSE;
	rdpSvcPlugin* plugin;

	ArrayList_Lock(g_AddinList);

	index = 0;
	plugin = (rdpSvcPlugin*) ArrayList_GetItem(g_AddinList, index++);

	while (plugin)
	{
		if (plugin->open_handle == open_handle)
		{
			found = TRUE;
			break;
		}

		plugin = (rdpSvcPlugin*) ArrayList_GetItem(g_AddinList, index++);
	}

	ArrayList_Unlock(g_AddinList);

	return (found) ? plugin : NULL;
}

static void svc_plugin_remove(rdpSvcPlugin* plugin)
{
	ArrayList_Remove(g_AddinList, (void*) plugin);
}

static void svc_plugin_process_received(rdpSvcPlugin* plugin, void* pData, UINT32 dataLength,
	UINT32 totalLength, UINT32 dataFlags)
{
	STREAM* data_in;
	
	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		/*
		 * According to MS-RDPBCGR 2.2.6.1, "All virtual channel traffic MUST be suspended.
		 * This flag is only valid in server-to-client virtual channel traffic. It MUST be
		 * ignored in client-to-server data." Thus it would be best practice to cease data
		 * transmission. However, simply returning here avoids a crash.
		 */
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (plugin->data_in != NULL)
			stream_free(plugin->data_in);

		plugin->data_in = stream_new(totalLength);
	}

	data_in = plugin->data_in;
	stream_check_size(data_in, (int) dataLength);
	stream_write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (stream_get_size(data_in) != stream_get_length(data_in))
		{
			printf("svc_plugin_process_received: read error\n");
		}

		plugin->data_in = NULL;
		stream_set_pos(data_in, 0);

		MessageQueue_Post(plugin->MsgPipe->In, NULL, 0, (void*) data_in, NULL);

		freerdp_thread_signal(plugin->thread);
	}
}

static void svc_plugin_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event_in)
{
	MessageQueue_Post(plugin->MsgPipe->In, NULL, 1, (void*) event_in, NULL);

	freerdp_thread_signal(plugin->thread);
}

static void svc_plugin_open_event(UINT32 openHandle, UINT32 event, void* pData, UINT32 dataLength,
	UINT32 totalLength, UINT32 dataFlags)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("openHandle %d event %d dataLength %d totalLength %d dataFlags %d",
		openHandle, event, dataLength, totalLength, dataFlags);

	plugin = (rdpSvcPlugin*) svc_plugin_find_by_open_handle(openHandle);

	if (!plugin)
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
			stream_free((STREAM*) pData);
			break;

		case CHANNEL_EVENT_USER:
			svc_plugin_process_event(plugin, (RDP_EVENT*) pData);
			break;
	}
}

static void svc_plugin_process_data_in(rdpSvcPlugin* plugin)
{
	STREAM* data;
	RDP_EVENT* event;
	wMessage message;

	while (1)
	{
		if (freerdp_thread_is_stopped(plugin->thread))
			break;

		if (!MessageQueue_Wait(plugin->MsgPipe->In))
			break;

		if (MessageQueue_Peek(plugin->MsgPipe->In, &message, TRUE))
		{
			if (message.id == 0)
			{
				data = (STREAM*) message.wParam;
				IFCALL(plugin->receive_callback, plugin, data);
			}
			else if (message.id == 1)
			{
				event = (RDP_EVENT*) message.wParam;
				IFCALL(plugin->event_callback, plugin, event);
			}
		}
	}
}

static void* svc_plugin_thread_func(void* arg)
{
	rdpSvcPlugin* plugin = (rdpSvcPlugin*) arg;

	DEBUG_SVC("in");

	IFCALL(plugin->connect_callback, plugin);

	while (1)
	{
		if (plugin->interval_ms > 0)
			freerdp_thread_wait_timeout(plugin->thread, plugin->interval_ms);
		else
			freerdp_thread_wait(plugin->thread);

		if (freerdp_thread_is_stopped(plugin->thread))
			break;

		freerdp_thread_reset(plugin->thread);
		svc_plugin_process_data_in(plugin);

		if (plugin->interval_ms > 0)
			IFCALL(plugin->interval_callback, plugin);
	}

	freerdp_thread_quit(plugin->thread);

	DEBUG_SVC("out");

	return 0;
}

static void svc_plugin_process_connected(rdpSvcPlugin* plugin, void* pData, UINT32 dataLength)
{
	UINT32 error;

	error = plugin->channel_entry_points.pVirtualChannelOpen(plugin->init_handle,
		&plugin->open_handle, plugin->channel_def.name, svc_plugin_open_event);

	if (error != CHANNEL_RC_OK)
	{
		printf("svc_plugin_process_connected: open failed\n");
		return;
	}

	plugin->MsgPipe = MessagePipe_New();

	plugin->thread = freerdp_thread_new();

	freerdp_thread_start(plugin->thread, svc_plugin_thread_func, plugin);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	if (plugin->thread)
	{
		freerdp_thread_stop(plugin->thread);
		freerdp_thread_free(plugin->thread);
	}

	plugin->channel_entry_points.pVirtualChannelClose(plugin->open_handle);

	svc_plugin_remove(plugin);

	MessagePipe_Free(plugin->MsgPipe);

	if (plugin->data_in != NULL)
	{
		stream_free(plugin->data_in);
		plugin->data_in = NULL;
	}

	IFCALL(plugin->terminate_callback, plugin);
}

static void svc_plugin_init_event(void* pInitHandle, UINT32 event, void* pData, UINT32 dataLength)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("event %d", event);

	plugin = (rdpSvcPlugin*) svc_plugin_find_by_init_handle(pInitHandle);

	if (!plugin)
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
	/**
	 * The channel manager will guarantee only one thread can call
	 * VirtualChannelInit at a time. So this should be safe.
	 */

	CopyMemory(&plugin->channel_entry_points, pEntryPoints, pEntryPoints->cbSize);

	if (!g_AddinList)
		g_AddinList = ArrayList_New(TRUE);

	ArrayList_Add(g_AddinList, (void*) plugin);

	plugin->channel_entry_points.pVirtualChannelInit(&plugin->init_handle,
		&plugin->channel_def, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, svc_plugin_init_event);
}

int svc_plugin_send(rdpSvcPlugin* plugin, STREAM* data_out)
{
	UINT32 error = 0;

	DEBUG_SVC("length %d", (int) stream_get_length(data_out));

	if (!plugin)
		error = CHANNEL_RC_BAD_INIT_HANDLE;
	else
		error = plugin->channel_entry_points.pVirtualChannelWrite(plugin->open_handle,
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
	UINT32 error = 0;

	DEBUG_SVC("event_type %d", event->event_type);

	error = plugin->channel_entry_points.pVirtualChannelEventPush(plugin->open_handle, event);

	if (error != CHANNEL_RC_OK)
		printf("svc_plugin_send_event: VirtualChannelEventPush failed %d\n", error);

	return error;
}
