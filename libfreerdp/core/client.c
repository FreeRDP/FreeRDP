/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "rdp.h"

#include "client.h"

static void* g_pInterface;
static CHANNEL_INIT_DATA g_ChannelInitData;

static wHashTable* g_OpenHandles = NULL;

/* To generate unique sequence for all open handles */
int g_open_handle_sequence = 1;

/* For locking the global resources */
static CRITICAL_SECTION g_channels_lock;

CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(rdpChannels* channels, const char* name)
{
	int index;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	for (index = 0; index < channels->openDataCount; index++)
	{
		pChannelOpenData = &channels->openDataList[index];

		if (strcmp(name, pChannelOpenData->name) == 0)
			return pChannelOpenData;
	}

	return NULL;
}

/* returns rdpChannel for the channel name passed in */
rdpMcsChannel* freerdp_channels_find_channel_by_name(rdpRdp* rdp, const char* name)
{
	UINT32 index;
	rdpMcsChannel* channel;
	rdpMcs* mcs = rdp->mcs;

	for (index = 0; index < mcs->channelCount; index++)
	{
		channel = &mcs->channels[index];

		if (strcmp(name, channel->Name) == 0)
		{
			return channel;
		}
	}

	return NULL;
}

rdpChannels* freerdp_channels_new(void)
{
	rdpChannels* channels;

	channels = (rdpChannels*) calloc(1, sizeof(rdpChannels));

	if (!channels)
		return NULL;

	channels->MsgPipe = MessagePipe_New();

	if (!g_OpenHandles)
	{
		g_OpenHandles = HashTable_New(TRUE);
		InitializeCriticalSectionAndSpinCount(&g_channels_lock, 4000);
	}

	return channels;
}

void freerdp_channels_free(rdpChannels* channels)
{
	MessagePipe_Free(channels->MsgPipe);

	free(channels);
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
	char* name;
	char* hostname;
	int hostnameLength;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	channels->is_connected = 1;
	hostname = instance->settings->ServerHostname;
	hostnameLength = (int) strlen(hostname);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			ChannelConnectedEventArgs e;
			CHANNEL_OPEN_DATA* pChannelOpenData;

			pChannelOpenData = &channels->openDataList[index];

			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle, CHANNEL_EVENT_CONNECTED, hostname, hostnameLength);

			name = (char*) malloc(9);
			CopyMemory(name, pChannelOpenData->name, 8);
			name[8] = '\0';

			EventArgsInit(&e, "freerdp");
			e.name = name;
			e.pInterface = pChannelOpenData->pInterface;
			PubSub_OnChannelConnected(instance->context->pubSub, instance->context, &e);

			free(name);
		}
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

int freerdp_channels_data(freerdp* instance, UINT16 channelId, BYTE* data, int dataSize, int flags, int totalSize)
{
	UINT32 index;
	rdpMcs* mcs;
	rdpChannels* channels;
	rdpMcsChannel* channel = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	mcs = instance->context->rdp->mcs;
	channels = instance->context->channels;

	if (!channels || !mcs)
	{
		return 1;
	}

	for (index = 0; index < mcs->channelCount; index++)
	{
		channel = &mcs->channels[index];

		if (mcs->channels[index].ChannelId == channelId)
		{
			channel = &mcs->channels[index];
			break;
		}
	}

	if (!channel)
	{
		return 1;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, channel->Name);

	if (!pChannelOpenData)
	{
		return 1;
	}

	if (pChannelOpenData->pChannelOpenEventProc)
	{
		pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle,
			CHANNEL_EVENT_DATA_RECEIVED, data, dataSize, totalSize, flags);
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
		freerdp_event_free(event);
		return 1;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, name);

	if (!pChannelOpenData)
	{
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
	int status = TRUE;
	wMessage message;
	wMessage* event;
	rdpMcsChannel* channel;
	CHANNEL_OPEN_EVENT* item;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	while (MessageQueue_Peek(channels->MsgPipe->Out, &message, TRUE))
	{
		if (message.id == WMQ_QUIT)
		{
			status = FALSE;
			break;
		}

		if (message.id == 0)
		{
			item = (CHANNEL_OPEN_EVENT*) message.wParam;

			if (!item)
				break;

			pChannelOpenData = item->pChannelOpenData;

			channel = freerdp_channels_find_channel_by_name(instance->context->rdp, pChannelOpenData->name);

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

	return status;
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
	char* name;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	channels->is_connected = 0;
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->clientDataCount; index++)
	{
		ChannelDisconnectedEventArgs e;

		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle, CHANNEL_EVENT_TERMINATED, 0, 0);

		pChannelOpenData = &channels->openDataList[index];

		name = (char*) malloc(9);
		CopyMemory(name, pChannelOpenData->name, 8);
		name[8] = '\0';

		EventArgsInit(&e, "freerdp");
		e.name = name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelDisconnected(instance->context->pubSub, instance->context, &e);

		free(name);
	}

	/* Emit a quit signal to the internal message pipe. */
	MessagePipe_PostQuit(channels->MsgPipe, 0);
}

UINT VCAPITYPE FreeRDP_VirtualChannelInit(LPVOID* ppInitHandle, PCHANNEL_DEF pChannel,
	INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	int index;
	void* pInterface;
	DWORD OpenHandle;
	CHANNEL_DEF* channel;
	rdpChannels* channels;
	rdpSettings* settings;
	PCHANNEL_DEF pChannelDef;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (!ppInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	channels = g_ChannelInitData.channels;
	pInterface = g_pInterface;

	pChannelInitData = &(channels->initDataList[channels->initDataCount]);
	*ppInitHandle = pChannelInitData;
	channels->initDataCount++;

	pChannelInitData->channels = channels;
	pChannelInitData->pInterface = pInterface;

	if (!channels->can_call_init)
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;

	if (channels->openDataCount + channelCount >= CHANNEL_MAX_COUNT)
		return CHANNEL_RC_TOO_MANY_CHANNELS;

	if (!pChannel)
		return CHANNEL_RC_BAD_CHANNEL;

	if (channels->is_connected)
		return CHANNEL_RC_ALREADY_CONNECTED;

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{

	}

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];

		if (freerdp_channels_find_channel_open_data_by_name(channels, pChannelDef->name) != 0)
		{
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->pChannelInitEventProc = pChannelInitEventProc;
	pChannelClientData->pInitHandle = *ppInitHandle;
	channels->clientDataCount++;

	settings = channels->settings;

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];
		pChannelOpenData = &channels->openDataList[channels->openDataCount];

		OpenHandle = g_open_handle_sequence++;

		pChannelOpenData->OpenHandle = OpenHandle;
		pChannelOpenData->channels = channels;

		HashTable_Add(g_OpenHandles, (void*) (UINT_PTR) OpenHandle, (void*) pChannelOpenData);

		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (settings->ChannelCount < CHANNEL_MAX_COUNT)
		{
			channel = &settings->ChannelDefArray[settings->ChannelCount];
			strncpy(channel->name, pChannelDef->name, 7);
			channel->options = pChannelDef->options;
			channels->settings->ChannelCount++;
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}

UINT VCAPITYPE FreeRDP_VirtualChannelOpen(LPVOID pInitHandle, LPDWORD pOpenHandle,
	PCHAR pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	void* pInterface;
	rdpChannels* channels;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	pChannelInitData = (CHANNEL_INIT_DATA*) pInitHandle;
	channels = pChannelInitData->channels;
	pInterface = pChannelInitData->pInterface;

	if (!pOpenHandle)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!pChannelOpenEventProc)
		return CHANNEL_RC_BAD_PROC;

	if (!channels->is_connected)
		return CHANNEL_RC_NOT_CONNECTED;

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, pChannelName);

	if (!pChannelOpenData)
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;

	if (pChannelOpenData->flags == 2)
		return CHANNEL_RC_ALREADY_OPEN;

	pChannelOpenData->flags = 2; /* open */
	pChannelOpenData->pInterface = pInterface;
	pChannelOpenData->pChannelOpenEventProc = pChannelOpenEventProc;
	*pOpenHandle = pChannelOpenData->OpenHandle;

	return CHANNEL_RC_OK;
}

UINT VCAPITYPE FreeRDP_VirtualChannelClose(DWORD openHandle)
{
	CHANNEL_OPEN_DATA* pChannelOpenData;

	pChannelOpenData = HashTable_GetItemValue(g_OpenHandles, (void*) (UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenData->flags = 0;

	return CHANNEL_RC_OK;
}

UINT VCAPITYPE FreeRDP_VirtualChannelWrite(DWORD openHandle, LPVOID pData, ULONG dataLength, LPVOID pUserData)
{
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_OPEN_EVENT* pChannelOpenEvent;

	pChannelOpenData = HashTable_GetItemValue(g_OpenHandles, (void*) (UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	channels = pChannelOpenData->channels;

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!channels->is_connected)
		return CHANNEL_RC_NOT_CONNECTED;

	if (!pData)
		return CHANNEL_RC_NULL_DATA;

	if (!dataLength)
		return CHANNEL_RC_ZERO_LENGTH;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenEvent = (CHANNEL_OPEN_EVENT*) malloc(sizeof(CHANNEL_OPEN_EVENT));

	if (!pChannelOpenEvent)
		return CHANNEL_RC_NO_MEMORY;

	pChannelOpenEvent->Data = pData;
	pChannelOpenEvent->DataLength = dataLength;
	pChannelOpenEvent->UserData = pUserData;
	pChannelOpenEvent->pChannelOpenData = pChannelOpenData;

	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 0, (void*) pChannelOpenEvent, NULL);

	return CHANNEL_RC_OK;
}

UINT FreeRDP_VirtualChannelEventPush(DWORD openHandle, wMessage* event)
{
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	pChannelOpenData = HashTable_GetItemValue(g_OpenHandles, (void*) (UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	channels = pChannelOpenData->channels;

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!channels->is_connected)
		return CHANNEL_RC_NOT_CONNECTED;

	if (!event)
		return CHANNEL_RC_NULL_DATA;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	/**
	 * We really intend to use the In queue for events, but we're pushing on both
	 * to wake up threads waiting on the out queue. Doing this cleanly would require
	 * breaking freerdp_pop_event() a bit too early in this refactoring.
	 */

	MessageQueue_Post(channels->MsgPipe->In, (void*) channels, 1, (void*) event, NULL);
	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 1, (void*) event, NULL);

	return CHANNEL_RC_OK;
}

int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings, void* entry, void* data)
{
	int status;
	CHANNEL_ENTRY_POINTS_FREERDP EntryPoints;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (channels->clientDataCount + 1 >= CHANNEL_MAX_COUNT)
	{
		DEBUG_WARN( "error: too many channels\n");
		return 1;
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->entry = (PVIRTUALCHANNELENTRY) entry;

	ZeroMemory(&EntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	EntryPoints.cbSize = sizeof(EntryPoints);
	EntryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	EntryPoints.pVirtualChannelInit = FreeRDP_VirtualChannelInit;
	EntryPoints.pVirtualChannelOpen = FreeRDP_VirtualChannelOpen;
	EntryPoints.pVirtualChannelClose = FreeRDP_VirtualChannelClose;
	EntryPoints.pVirtualChannelWrite = FreeRDP_VirtualChannelWrite;

	g_pInterface = NULL;
	EntryPoints.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	EntryPoints.ppInterface = &g_pInterface;
	EntryPoints.pExtendedData = data;
	EntryPoints.pVirtualChannelEventPush = FreeRDP_VirtualChannelEventPush;

	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	channels->settings = settings;

	EnterCriticalSection(&g_channels_lock);

	g_ChannelInitData.channels = channels;
	status = pChannelClientData->entry((PCHANNEL_ENTRY_POINTS) &EntryPoints);

	LeaveCriticalSection(&g_channels_lock);

	/* disable MyVirtualChannelInit */
	channels->settings = NULL;
	channels->can_call_init = FALSE;

	if (!status)
	{
		DEBUG_WARN( "error: channel export function call failed\n");
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

	entry = (PVIRTUALCHANNELENTRY) freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (!entry)
	{
		return 1;
	}

	return freerdp_channels_client_load(channels, settings, entry, data);
}
