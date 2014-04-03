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

#include <assert.h>
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

static wListDictionary* g_InitHandles;
static wListDictionary* g_OpenHandles;

void svc_plugin_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* svc_plugin_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void svc_plugin_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
}

void svc_plugin_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* svc_plugin_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void svc_plugin_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
}

static void svc_plugin_process_received(rdpSvcPlugin* plugin, void* pData, UINT32 dataLength,
	UINT32 totalLength, UINT32 dataFlags)
{
	wStream* s;
	
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

	s = plugin->data_in;
	Stream_EnsureRemainingCapacity(s, (int) dataLength);
	Stream_Write(s, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(s) != Stream_GetPosition(s))
		{
			fprintf(stderr, "svc_plugin_process_received: read error\n");
		}

		plugin->data_in = NULL;
		Stream_SealLength(s);
		Stream_SetPosition(s, 0);

		MessageQueue_Post(plugin->MsgPipe->In, NULL, 0, (void*) s, NULL);
	}
}

static void svc_plugin_process_event(rdpSvcPlugin* plugin, wMessage* event_in)
{
	MessageQueue_Post(plugin->MsgPipe->In, NULL, 1, (void*) event_in, NULL);
}

static VOID VCAPITYPE svc_plugin_open_event(DWORD openHandle, UINT event, LPVOID pData, UINT32 dataLength,
	UINT32 totalLength, UINT32 dataFlags)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("openHandle %d event %d dataLength %d totalLength %d dataFlags %d",
		openHandle, event, dataLength, totalLength, dataFlags);

	plugin = (rdpSvcPlugin*) svc_plugin_get_open_handle_data(openHandle);

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

	assert(NULL != plugin);

	IFCALL(plugin->connect_callback, plugin);

	SetEvent(plugin->started);
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

	ExitThread(0);

	return 0;
}

static void svc_plugin_process_connected(rdpSvcPlugin* plugin, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = plugin->channel_entry_points.pVirtualChannelOpen(plugin->InitHandle,
		&(plugin->OpenHandle), plugin->channel_def.name, svc_plugin_open_event);

	if (status != CHANNEL_RC_OK)
	{
		fprintf(stderr, "svc_plugin_process_connected: open failed: status: %d\n", status);
		return;
	}

	svc_plugin_add_open_handle_data(plugin->OpenHandle, plugin);

	plugin->MsgPipe = MessagePipe_New();

	plugin->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) svc_plugin_thread_func, (void*) plugin, 0, NULL);
	WaitForSingleObject(plugin->started,INFINITE);
}

static void svc_plugin_process_terminated(rdpSvcPlugin* plugin)
{
	MessagePipe_PostQuit(plugin->MsgPipe, 0);
	WaitForSingleObject(plugin->thread, INFINITE);

	MessagePipe_Free(plugin->MsgPipe);
	CloseHandle(plugin->thread);
	CloseHandle(plugin->started);

	plugin->channel_entry_points.pVirtualChannelClose(plugin->OpenHandle);

	if (plugin->data_in)
	{
		Stream_Free(plugin->data_in, TRUE);
		plugin->data_in = NULL;
	}

	IFCALL(plugin->terminate_callback, plugin);

	svc_plugin_remove_open_handle_data(plugin->OpenHandle);
	svc_plugin_remove_init_handle_data(plugin->InitHandle);
}

static VOID VCAPITYPE svc_plugin_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	rdpSvcPlugin* plugin;

	DEBUG_SVC("event %d", event);

	plugin = (rdpSvcPlugin*) svc_plugin_get_init_handle_data(pInitHandle);

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

	CopyMemory(&(plugin->channel_entry_points), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	plugin->channel_entry_points.pVirtualChannelInit(&(plugin->InitHandle),
		&(plugin->channel_def), 1, VIRTUAL_CHANNEL_VERSION_WIN2000, svc_plugin_init_event);

	plugin->channel_entry_points.pInterface = *(plugin->channel_entry_points.ppInterface);
	plugin->channel_entry_points.ppInterface = &(plugin->channel_entry_points.pInterface);
	plugin->started = CreateEvent(NULL,TRUE,FALSE,NULL);

	svc_plugin_add_init_handle_data(plugin->InitHandle, plugin);
}

int svc_plugin_send(rdpSvcPlugin* plugin, wStream* data_out)
{
	UINT32 status = 0;

	DEBUG_SVC("length %d", (int) Stream_GetPosition(data_out));

	if (!plugin)
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	else
		status = plugin->channel_entry_points.pVirtualChannelWrite(plugin->OpenHandle,
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

	status = plugin->channel_entry_points.pVirtualChannelEventPush(plugin->OpenHandle, event);

	if (status != CHANNEL_RC_OK)
		fprintf(stderr, "svc_plugin_send_event: VirtualChannelEventPush failed %d\n", status);

	return status;
}
