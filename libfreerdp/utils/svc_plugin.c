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
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/constants.h>
#include <freerdp/utils/debug.h>
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
	wStream* data_in;
	
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
			Stream_Free(plugin->data_in, TRUE);

		plugin->data_in = Stream_New(NULL, totalLength);
	}

	data_in = plugin->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			fprintf(stderr, "svc_plugin_process_received: read error\n");
		}

		plugin->data_in = NULL;
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(plugin->MsgPipe->In, NULL, 0, (void*) data_in, NULL);
	}
}

static void svc_plugin_process_event(rdpSvcPlugin* plugin, wMessage* event_in)
{
	MessageQueue_Post(plugin->MsgPipe->In, NULL, 1, (void*) event_in, NULL);
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
		fprintf(stderr, "svc_plugin_open_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			svc_plugin_process_received(plugin, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			svc_plugin_process_event(plugin, (wMessage*) pData);
			break;
	}
}

static void* svc_plugin_thread_func(void* arg)
{
	wStream* data;
	wMessage* event;
	wMessage message;
	rdpSvcPlugin* plugin = (rdpSvcPlugin*) arg;

	DEBUG_SVC("in");

	IFCALL(plugin->connect_callback, plugin);

	while (1)
	{
		if (!MessageQueue_Wait(plugin->MsgPipe->In))
			break;

		if (MessageQueue_Peek(plugin->MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				IFCALL(plugin->receive_callback, plugin, data);
			}
			else if (message.id == 1)
			{
				event = (wMessage*) message.wParam;
				IFCALL(plugin->event_callback, plugin, event);
			}
		}
	}

	DEBUG_SVC("out");

	return 0;
}

static void svc_plugin_process_connected(rdpSvcPlugin* plugin, void* pData, UINT32 dataLength)
{
	UINT32 status;

	status = plugin->channel_entry_points.pVirtualChannelOpen(plugin->init_handle,
		&plugin->open_handle, plugin->channel_def.name, svc_plugin_open_event);

	if (status != CHANNEL_RC_OK)
	{
		fprintf(stderr, "svc_plugin_process_connected: open failed: status: %d\n", status);
		return;
	}

	plugin->MsgPipe = MessagePipe_New();

	plugin->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) svc_plugin_thread_func, (void*) plugin, 0, NULL);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	MessagePipe_PostQuit(plugin->MsgPipe, 0);
	WaitForSingleObject(plugin->thread, INFINITE);

	MessagePipe_Free(plugin->MsgPipe);
	CloseHandle(plugin->thread);

	plugin->channel_entry_points.pVirtualChannelClose(plugin->open_handle);

	svc_plugin_remove(plugin);

	if (plugin->data_in)
	{
		Stream_Free(plugin->data_in, TRUE);
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
		fprintf(stderr, "svc_plugin_init_event: error no match\n");
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

int svc_plugin_send(rdpSvcPlugin* plugin, wStream* data_out)
{
	UINT32 status = 0;

	DEBUG_SVC("length %d", (int) Stream_GetPosition(data_out));

	if (!plugin)
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	else
		status = plugin->channel_entry_points.pVirtualChannelWrite(plugin->open_handle,
			Stream_Buffer(data_out), Stream_GetPosition(data_out), data_out);

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(data_out, TRUE);
		fprintf(stderr, "svc_plugin_send: VirtualChannelWrite failed %d\n", status);
	}

	return status;
}

int svc_plugin_send_event(rdpSvcPlugin* plugin, wMessage* event)
{
	UINT32 status = 0;

	DEBUG_SVC("event class: %d type: %d",
			GetMessageClass(event->id), GetMessageType(event->id));

	status = plugin->channel_entry_points.pVirtualChannelEventPush(plugin->open_handle, event);

	if (status != CHANNEL_RC_OK)
		fprintf(stderr, "svc_plugin_send_event: VirtualChannelEventPush failed %d\n", status);

	return status;
}
