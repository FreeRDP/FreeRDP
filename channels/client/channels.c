/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/collections.h>

#include "addin.h"
#include "init.h"
#include "open.h"

#include "channels.h"

/**
 * MS compatible plugin interface
 * reference:
 * http://msdn.microsoft.com/en-us/library/aa383580.aspx
 *
 * Notes on threads:
 * Many virtual channel plugins are built using threads.
 * Non main threads may call MyVirtualChannelOpen,
 * MyVirtualChannelClose, or MyVirtualChannelWrite.
 * Since the plugin's VirtualChannelEntry function is called
 * from the main thread, MyVirtualChannelInit has to be called
 * from the main thread.
 */

/**
 * The current channel manager reference passes from VirtualChannelEntry to
 * VirtualChannelInit for the pInitHandle.
 */

void* g_pInterface;
CHANNEL_INIT_DATA g_ChannelInitData;

static wArrayList* g_ChannelsList = NULL;

/* To generate unique sequence for all open handles */
int g_open_handle_sequence;

/* For locking the global resources */
static HANDLE g_mutex_init;

rdpChannels* freerdp_channels_find_by_open_handle(int open_handle, int* pindex)
{
	int i, j;
	BOOL found = FALSE;
	rdpChannels* channels = NULL;

	ArrayList_Lock(g_ChannelsList);

	i = j = 0;
	channels = (rdpChannels*) ArrayList_GetItem(g_ChannelsList, i++);

	while (channels)
	{
		for (j = 0; j < channels->openDataCount; j++)
		{
			if (channels->openDataList[j].OpenHandle == open_handle)
			{
				*pindex = j;
				found = TRUE;
				break;
			}
		}

		if (found)
			break;

		channels = (rdpChannels*) ArrayList_GetItem(g_ChannelsList, i++);
	}

	ArrayList_Unlock(g_ChannelsList);

	return (found) ? channels : NULL;
}

rdpChannels* freerdp_channels_find_by_instance(freerdp* instance)
{
	int index;
	BOOL found = FALSE;
	rdpChannels* channels = NULL;

	ArrayList_Lock(g_ChannelsList);

	index = 0;
	channels = (rdpChannels*) ArrayList_GetItem(g_ChannelsList, index++);

	while (channels)
	{
		if (channels->instance == instance)
		{
			found = TRUE;
			break;
		}

		channels = (rdpChannels*) ArrayList_GetItem(g_ChannelsList, index++);
	}

	ArrayList_Unlock(g_ChannelsList);

	return (found) ? channels : NULL;
}

CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(rdpChannels* channels, const char* channel_name)
{
	int index;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	for (index = 0; index < channels->openDataCount; index++)
	{
		pChannelOpenData = &channels->openDataList[index];

		if (strcmp(channel_name, pChannelOpenData->name) == 0)
			return pChannelOpenData;
	}

	return NULL;
}

/* returns rdpChannel for the channel id passed in */
rdpChannel* freerdp_channels_find_channel_by_id(rdpChannels* channels, rdpSettings* settings, int channel_id, int* pindex)
{
	int index;
	int count;
	rdpChannel* channel;

	count = settings->ChannelCount;

	for (index = 0; index < count; index++)
	{
		channel = &settings->ChannelDefArray[index];

		if (channel->ChannelId == channel_id)
		{
			if (pindex != 0)
				*pindex = index;

			return channel;
		}
	}

	return NULL;
}

/* returns rdpChannel for the channel name passed in */
rdpChannel* freerdp_channels_find_channel_by_name(rdpChannels* channels,
		rdpSettings* settings, const char* channel_name, int* pindex)
{
	int index;
	int count;
	rdpChannel* channel;

	count = settings->ChannelCount;

	for (index = 0; index < count; index++)
	{
		channel = &settings->ChannelDefArray[index];

		if (strcmp(channel_name, channel->Name) == 0)
		{
			if (pindex != 0)
				*pindex = index;

			return channel;
		}
	}

	return NULL;
}

UINT32 FreeRDP_VirtualChannelWrite(UINT32 openHandle, void* pData, UINT32 dataLength, void* pUserData)
{
	int index;
	rdpChannels* channels;
	CHANNEL_OPEN_EVENT* item;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((!channels) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (!pData)
	{
		DEBUG_CHANNELS("error bad pData");
		return CHANNEL_RC_NULL_DATA;
	}

	if (!dataLength)
	{
		DEBUG_CHANNELS("error bad dataLength");
		return CHANNEL_RC_ZERO_LENGTH;
	}

	pChannelOpenData = &channels->openDataList[index];

	if (pChannelOpenData->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	item = (CHANNEL_OPEN_EVENT*) malloc(sizeof(CHANNEL_OPEN_EVENT));
	item->Data = pData;
	item->DataLength = dataLength;
	item->UserData = pUserData;
	item->Index = index;

	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 0, (void*) item, NULL);

	return CHANNEL_RC_OK;
}

UINT32 FreeRDP_VirtualChannelEventPush(UINT32 openHandle, wMessage* event)
{
	int index;
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((!channels) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (!event)
	{
		DEBUG_CHANNELS("error bad event");
		return CHANNEL_RC_NULL_DATA;
	}

	pChannelOpenData = &channels->openDataList[index];

	if (pChannelOpenData->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	/**
	 * We really intend to use the In queue for events, but we're pushing on both
	 * to wake up threads waiting on the out queue. Doing this cleanly would require
	 * breaking freerdp_pop_event() a bit too early in this refactoring.
	 */

	MessageQueue_Post(channels->MsgPipe->In, (void*) channels, 1, (void*) event, NULL);
	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 1, (void*) event, NULL);

	return CHANNEL_RC_OK;
}

/**
 * this is called shortly after the application starts and
 * before any other function in the file
 * called only from main thread
 */
int freerdp_channels_global_init(void)
{
	g_open_handle_sequence = 1;
	g_mutex_init = CreateMutex(NULL, FALSE, NULL);

	if (!g_ChannelsList)
		g_ChannelsList = ArrayList_New(TRUE);

	return 0;
}

int freerdp_channels_global_uninit(void)
{
	/* TODO: free channels list */

	CloseHandle(g_mutex_init);

	return 0;
}

rdpChannels* freerdp_channels_new(void)
{
	rdpChannels* channels;

	channels = (rdpChannels*) malloc(sizeof(rdpChannels));
	ZeroMemory(channels, sizeof(rdpChannels));

	channels->MsgPipe = MessagePipe_New();

	ArrayList_Add(g_ChannelsList, (void*) channels);

	return channels;
}

void freerdp_channels_free(rdpChannels* channels)
{
	MessagePipe_Free(channels->MsgPipe);

	/* TODO: remove from channels list */

	free(channels);
}

int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings, void* entry, void* data)
{
	int status;
	CHANNEL_ENTRY_POINTS_EX ep;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (channels->clientDataCount + 1 >= CHANNEL_MAX_COUNT)
	{
		fprintf(stderr, "error: too many channels\n");
		return 1;
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->entry = (PVIRTUALCHANNELENTRY) entry;

	ep.cbSize = sizeof(ep);
	ep.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	ep.pVirtualChannelInit = FreeRDP_VirtualChannelInit;
	ep.pVirtualChannelOpen = FreeRDP_VirtualChannelOpen;
	ep.pVirtualChannelClose = FreeRDP_VirtualChannelClose;
	ep.pVirtualChannelWrite = FreeRDP_VirtualChannelWrite;

	g_pInterface = NULL;
	ep.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	ep.ppInterface = &g_pInterface;
	ep.pExtendedData = data;
	ep.pVirtualChannelEventPush = FreeRDP_VirtualChannelEventPush;

	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	channels->settings = settings;

	WaitForSingleObject(g_mutex_init, INFINITE);

	g_ChannelInitData.channels = channels;
	status = pChannelClientData->entry((PCHANNEL_ENTRY_POINTS) &ep);

	ReleaseMutex(g_mutex_init);

	/* disable MyVirtualChannelInit */
	channels->settings = NULL;
	channels->can_call_init = FALSE;

	if (!status)
	{
		fprintf(stderr, "error: channel export function call failed\n");
		return 1;
	}

	return 0;
}

/**
 * this is called when processing the command line parameters
 * called only from main thread
 */
int freerdp_channels_load_plugin(rdpChannels* channels, rdpSettings* settings, const char* name, void* data)
{
	void* entry;

	DEBUG_CHANNELS("%s", name);

	entry = (PVIRTUALCHANNELENTRY) freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (!entry)
	{
		DEBUG_CHANNELS("failed to find export function");
		return 1;
	}

	return freerdp_channels_client_load(channels, settings, entry, data);
}

int freerdp_drdynvc_on_channel_connected(DrdynvcClientContext* context, const char* name, void* pInterface)
{
	int status = 0;
	ChannelConnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;

	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelConnected(instance->context->pubSub, instance->context, &e);

	return status;
}

int freerdp_drdynvc_on_channel_disconnected(DrdynvcClientContext* context, const char* name, void* pInterface)
{
	int status = 0;
	ChannelDisconnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;

	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelDisconnected(instance->context->pubSub, instance->context, &e);

	return status;
}

/**
 * go through and inform all the libraries that we are initialized
 * called only from main thread
 */
int freerdp_channels_pre_connect(rdpChannels* channels, freerdp* instance)
{
	int index;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	DEBUG_CHANNELS("enter");
	channels->instance = instance;

	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle, CHANNEL_EVENT_INITIALIZED, 0, 0);
	}

	return 0;
}

/**
 * go through and inform all the libraries that we are connected
 * this will tell the libraries that its ok to call MyVirtualChannelOpen
 * called only from main thread
 */
int freerdp_channels_post_connect(rdpChannels* channels, freerdp* instance)
{
	int index;
	char* hostname;
	int hostnameLength;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	channels->is_connected = 1;
	hostname = instance->settings->ServerHostname;
	hostnameLength = strlen(hostname);

	DEBUG_CHANNELS("hostname [%s] channels->num_libs [%d]", hostname, channels->clientDataCount);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle, CHANNEL_EVENT_CONNECTED, hostname, hostnameLength);
	}

	channels->drdynvc = (DrdynvcClientContext*) freerdp_channels_get_static_channel_interface(channels, "drdynvc");

	if (channels->drdynvc)
	{
		channels->drdynvc->custom = (void*) channels;
		channels->drdynvc->OnChannelConnected = freerdp_drdynvc_on_channel_connected;
		channels->drdynvc->OnChannelDisconnected = freerdp_drdynvc_on_channel_disconnected;
	}

	return 0;
}

/**
 * data coming from the server to the client
 * called only from main thread
 */
int freerdp_channels_data(freerdp* instance, int channel_id, void* data, int data_size, int flags, int total_size)
{
	int index;
	rdpChannel* channel;
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	channels = freerdp_channels_find_by_instance(instance);

	if (!channels)
	{
		DEBUG_CHANNELS("could not find channel manager");
		return 1;
	}

	channel = freerdp_channels_find_channel_by_id(channels, instance->settings, channel_id, &index);

	if (!channel)
	{
		DEBUG_CHANNELS("could not find channel id");
		return 1;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, channel->Name);

	if (!pChannelOpenData)
	{
		DEBUG_CHANNELS("could not find channel name");
		return 1;
	}

	if (pChannelOpenData->pChannelOpenEventProc)
	{
		pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle,
			CHANNEL_EVENT_DATA_RECEIVED, data, data_size, total_size, flags);
	}

	return 0;
}

/**
 * Send a plugin-defined event to the plugin.
 * called only from main thread
 * @param channels the channel manager instance
 * @param event an event object created by freerdp_event_new()
 */
FREERDP_API int freerdp_channels_send_event(rdpChannels* channels, wMessage* event)
{
	const char* name = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	switch (GetMessageClass(event->id))
	{
		case DebugChannel_Class:
			name = "rdpdbg";
			break;

		case CliprdrChannel_Class:
			name = "cliprdr";
			break;

		case TsmfChannel_Class:
			name = "tsmf";
			break;

		case RailChannel_Class:
			name = "rail";
			break;
	}

	if (!name)
	{
		DEBUG_CHANNELS("unknown event_class %d", GetMessageClass(event->id));
		freerdp_event_free(event);
		return 1;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, name);

	if (!pChannelOpenData)
	{
		DEBUG_CHANNELS("could not find channel name %s", name);
		freerdp_event_free(event);
		return 1;
	}

	if (pChannelOpenData->pChannelOpenEventProc)
	{
		pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle, CHANNEL_EVENT_USER,
			event, sizeof(wMessage), sizeof(wMessage), 0);
	}

	return 0;
}

/**
 * called only from main thread
 */
static int freerdp_channels_process_sync(rdpChannels* channels, freerdp* instance)
{
	int rc = TRUE;
	wMessage message;
	wMessage* event;
	rdpChannel* channel;
	CHANNEL_OPEN_EVENT* item;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	while (MessageQueue_Peek(channels->MsgPipe->Out, &message, TRUE))
	{
		if (message.id == WMQ_QUIT)
		{
			rc = FALSE;
			break;
		}

		if (message.id == 0)
		{
			item = (CHANNEL_OPEN_EVENT*) message.wParam;

			if (!item)
				break;

			pChannelOpenData = &channels->openDataList[item->Index];

			channel = freerdp_channels_find_channel_by_name(channels, instance->settings,
				pChannelOpenData->name, &item->Index);

			if (channel)
				instance->SendChannelData(instance, channel->ChannelId, item->Data, item->DataLength);

			if (pChannelOpenData->pChannelOpenEventProc)
			{
				pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle,
					CHANNEL_EVENT_WRITE_COMPLETE, item->UserData, item->DataLength, item->DataLength, 0);
			}

			free(item);
		}
		else if (message.id == 1)
		{
			event = (wMessage*) message.wParam;

			/**
			 * Ignore for now, the same event is being pushed on the In queue,
			 * and we're pushing it on the Out queue just to wake other threads
			 */
		}
	}

	return rc;
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance, void** read_fds,
	int* read_count, void** write_fds, int* write_count)
{
	void* pfd;

	pfd = GetEventWaitObject(MessageQueue_Event(channels->MsgPipe->Out));

	if (pfd)
	{
		read_fds[*read_count] = pfd;
		(*read_count)++;
	}

	return TRUE;
}

void* freerdp_channels_get_static_channel_interface(rdpChannels* channels, const char* name)
{
	void* pInterface = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, name);

	if (pChannelOpenData)
		pInterface = pChannelOpenData->pInterface;

	return pInterface;
}

HANDLE freerdp_channels_get_event_handle(freerdp* instance)
{
	HANDLE event = NULL;
	rdpChannels* channels;

	channels = instance->context->channels;
	event = MessageQueue_Event(channels->MsgPipe->Out);

	return event;
}

int freerdp_channels_process_pending_messages(freerdp* instance)
{
	rdpChannels* channels;

	channels = instance->context->channels;

	if (WaitForSingleObject(MessageQueue_Event(channels->MsgPipe->Out), 0) == WAIT_OBJECT_0)
	{
		 return freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_check_fds(rdpChannels* channels, freerdp* instance)
{
	if (WaitForSingleObject(MessageQueue_Event(channels->MsgPipe->Out), 0) == WAIT_OBJECT_0)
	{
		freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

wMessage* freerdp_channels_pop_event(rdpChannels* channels)
{
	wMessage message;
	wMessage* event = NULL;

	if (MessageQueue_Peek(channels->MsgPipe->In, &message, TRUE))
	{
		if (message.id == 1)
		{
			event = (wMessage*) message.wParam;
		}
	}

	return event;
}

void freerdp_channels_close(rdpChannels* channels, freerdp* instance)
{
	int index;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	DEBUG_CHANNELS("closing");
	channels->is_connected = 0;
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle, CHANNEL_EVENT_TERMINATED, 0, 0);
	}

	/* Emit a quit signal to the internal message pipe. */
	MessagePipe_PostQuit(channels->MsgPipe, 0);
}
