/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/log.h>

#include "rdp.h"

#include "client.h"

#define TAG FREERDP_TAG("core.client")

static WINPR_TLS void* g_pInterface = NULL;
static WINPR_TLS rdpChannels* g_channels = NULL; /* use only for VirtualChannelInit hack */

static volatile LONG g_OpenHandleSeq =
    1; /* use global counter to ensure uniqueness across channel manager instances */
static WINPR_TLS rdpChannelHandles g_ChannelHandles = { NULL, NULL };

static CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(
    rdpChannels* channels, const char* name)
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
static rdpMcsChannel* freerdp_channels_find_channel_by_name(rdpRdp* rdp,
        const char* name)
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

static void channel_queue_free(void* obj)
{
	CHANNEL_OPEN_EVENT* ev;
	wMessage* msg = (wMessage*)obj;

	if (!msg || (msg->id != 0))
		return;

	ev = (CHANNEL_OPEN_EVENT*)msg->wParam;

	if (ev)
	{
		/* Added by FreeRDP_VirtualChannelWriteEx */
		if (ev->UserData)
		{
			wStream* s = (wStream*)ev->UserData;
			Stream_Free(s, TRUE);
		}
		/* Either has no data or added by FreeRDP_VirtualChannelWrite */
		else
			free(ev->Data);

		free(ev);
	}
}

rdpChannels* freerdp_channels_new(freerdp* instance)
{
	rdpChannels* channels;
	channels = (rdpChannels*) calloc(1, sizeof(rdpChannels));

	if (!channels)
		return NULL;

	if (!InitializeCriticalSectionAndSpinCount(&channels->channelsLock, 4000))
		goto error;

	channels->instance = instance;
	channels->queue = MessageQueue_New(NULL);

	if (!channels->queue)
		goto error;

	channels->queue->object.fnObjectFree = channel_queue_free;
	channels->openHandles = HashTable_New(TRUE);

	if (!channels->openHandles)
		goto error;

	return channels;
error:
	freerdp_channels_free(channels);
	return NULL;
}

void freerdp_channels_free(rdpChannels* channels)
{
	if (!channels)
		return;

	DeleteCriticalSection(&channels->channelsLock);

	if (channels->queue)
	{
		MessageQueue_Free(channels->queue);
		channels->queue = NULL;
	}

	if (channels->openHandles)
		HashTable_Free(channels->openHandles);

	free(channels);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT freerdp_drdynvc_on_channel_connected(DrdynvcClientContext* context,
        const char* name, void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelConnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelConnected(instance->context->pubSub, instance->context, &e);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT freerdp_drdynvc_on_channel_disconnected(DrdynvcClientContext*
        context,
        const char* name, void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelDisconnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelDisconnected(instance->context->pubSub, instance->context, &e);
	return status;
}

static UINT freerdp_drdynvc_on_channel_attached(DrdynvcClientContext*
        context,
        const char* name, void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelAttachedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelAttached(instance->context->pubSub, instance->context, &e);
	return status;
}

static UINT freerdp_drdynvc_on_channel_detached(DrdynvcClientContext*
        context,
        const char* name, void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelDetachedEventArgs e;
	rdpChannels* channels = (rdpChannels*) context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelDetached(instance->context->pubSub, instance->context, &e);
	return status;
}

/**
 * go through and inform all the libraries that we are initialized
 * called only from main thread
 */
UINT freerdp_channels_pre_connect(rdpChannels* channels, freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_INITIALIZED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_INITIALIZED, 0, 0);
		}

		if (CHANNEL_RC_OK != getChannelError(instance->context))
			break;
	}

	return error;
}

UINT freerdp_channels_attach(freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	char* name = NULL;
	char* hostname;
	int hostnameLength;
	rdpChannels* channels;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	channels = instance->context->channels;
	hostname = instance->settings->ServerHostname;
	hostnameLength = (int) strlen(hostname);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		ChannelAttachedEventArgs e;
		CHANNEL_OPEN_DATA* pChannelOpenData = NULL;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_ATTACHED, hostname, hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_ATTACHED, hostname, hostnameLength);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		name = (char*) malloc(9);

		if (!name)
		{
			error = CHANNEL_RC_NO_MEMORY;
			goto fail;
		}

		CopyMemory(name, pChannelOpenData->name, 8);
		name[8] = '\0';
		EventArgsInit(&e, "freerdp");
		e.name = name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelAttached(instance->context->pubSub, instance->context, &e);
		free(name);
		name = NULL;
	}

fail:
	free(name);
	return error;
}

UINT freerdp_channels_detach(freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	char* name = NULL;
	char* hostname;
	int hostnameLength;
	rdpChannels* channels;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	channels = instance->context->channels;
	hostname = instance->settings->ServerHostname;
	hostnameLength = (int) strlen(hostname);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		ChannelDetachedEventArgs e;
		CHANNEL_OPEN_DATA* pChannelOpenData;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_DETACHED, hostname, hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_DETACHED, hostname, hostnameLength);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		name = (char*) malloc(9);

		if (!name)
		{
			error = CHANNEL_RC_NO_MEMORY;
			goto fail;
		}

		CopyMemory(name, pChannelOpenData->name, 8);
		name[8] = '\0';
		EventArgsInit(&e, "freerdp");
		e.name = name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelDetached(instance->context->pubSub, instance->context, &e);
		free(name);
		name = NULL;
	}

fail:
	free(name);
	return error;
}

/**
 * go through and inform all the libraries that we are connected
 * this will tell the libraries that its ok to call MyVirtualChannelOpen
 * called only from main thread
 */
UINT freerdp_channels_post_connect(rdpChannels* channels, freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	char* name = NULL;
	char* hostname;
	int hostnameLength;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	channels->connected = TRUE;
	hostname = instance->settings->ServerHostname;
	hostnameLength = (int) strlen(hostname);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		ChannelConnectedEventArgs e;
		CHANNEL_OPEN_DATA* pChannelOpenData;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_CONNECTED, hostname, hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_CONNECTED, hostname, hostnameLength);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		name = (char*) malloc(9);

		if (!name)
		{
			error = CHANNEL_RC_NO_MEMORY;
			goto fail;
		}

		CopyMemory(name, pChannelOpenData->name, 8);
		name[8] = '\0';
		EventArgsInit(&e, "freerdp");
		e.name = name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelConnected(instance->context->pubSub, instance->context, &e);
		free(name);
		name = NULL;
	}

	channels->drdynvc = (DrdynvcClientContext*)
	                    freerdp_channels_get_static_channel_interface(channels, "drdynvc");

	if (channels->drdynvc)
	{
		channels->drdynvc->custom = (void*) channels;
		channels->drdynvc->OnChannelConnected = freerdp_drdynvc_on_channel_connected;
		channels->drdynvc->OnChannelDisconnected =
		    freerdp_drdynvc_on_channel_disconnected;
		channels->drdynvc->OnChannelAttached = freerdp_drdynvc_on_channel_attached;
		channels->drdynvc->OnChannelDetached = freerdp_drdynvc_on_channel_detached;
	}

fail:
	free(name);
	return error;
}

int freerdp_channels_data(freerdp* instance, UINT16 channelId, BYTE* data,
                          int dataSize, int flags, int totalSize)
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
		pChannelOpenData->pChannelOpenEventProc(
		    pChannelOpenData->OpenHandle, CHANNEL_EVENT_DATA_RECEIVED, data, dataSize, totalSize, flags);
	}
	else if (pChannelOpenData->pChannelOpenEventProcEx)
	{
		pChannelOpenData->pChannelOpenEventProcEx(pChannelOpenData->lpUserParam,
		        pChannelOpenData->OpenHandle, CHANNEL_EVENT_DATA_RECEIVED, data, dataSize, totalSize, flags);
	}

	return 0;
}

/**
 * called only from main thread
 */
static int freerdp_channels_process_sync(rdpChannels* channels,
        freerdp* instance)
{
	int status = TRUE;
	wMessage message;
	rdpMcsChannel* channel;
	CHANNEL_OPEN_EVENT* item;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	while (MessageQueue_Peek(channels->queue, &message, TRUE))
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
				pChannelOpenData->pChannelOpenEventProc(
				    pChannelOpenData->OpenHandle, CHANNEL_EVENT_WRITE_COMPLETE, item->UserData, item->DataLength,
				    item->DataLength, 0);
			}
			else if (pChannelOpenData->pChannelOpenEventProcEx)
			{
				pChannelOpenData->pChannelOpenEventProcEx(pChannelOpenData->lpUserParam,
				        pChannelOpenData->OpenHandle, CHANNEL_EVENT_WRITE_COMPLETE, item->UserData, item->DataLength,
				        item->DataLength, 0);
			}

			free(item);
		}
	}

	return status;
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance,
                              void** read_fds,
                              int* read_count, void** write_fds, int* write_count)
{
	void* pfd;
	pfd = GetEventWaitObject(MessageQueue_Event(channels->queue));

	if (pfd)
	{
		read_fds[*read_count] = pfd;
		(*read_count)++;
	}

	return TRUE;
}

void* freerdp_channels_get_static_channel_interface(rdpChannels* channels,
        const char* name)
{
	void* pInterface = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels,
	                   name);

	if (pChannelOpenData)
		pInterface = pChannelOpenData->pInterface;

	return pInterface;
}

HANDLE freerdp_channels_get_event_handle(freerdp* instance)
{
	HANDLE event = NULL;
	rdpChannels* channels;
	channels = instance->context->channels;
	event = MessageQueue_Event(channels->queue);
	return event;
}

int freerdp_channels_process_pending_messages(freerdp* instance)
{
	rdpChannels* channels;
	channels = instance->context->channels;

	if (WaitForSingleObject(MessageQueue_Event(channels->queue),
	                        0) == WAIT_OBJECT_0)
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
	if (WaitForSingleObject(MessageQueue_Event(channels->queue),
	                        0) == WAIT_OBJECT_0)
	{
		freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

UINT freerdp_channels_disconnect(rdpChannels* channels, freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (!channels->connected)
		return 0;

	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->clientDataCount; index++)
	{
		char name[9];
		ChannelDisconnectedEventArgs e;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_DISCONNECTED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_DISCONNECTED, 0, 0);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			continue;

		pChannelOpenData = &channels->openDataList[index];
		CopyMemory(name, pChannelOpenData->name, 8);
		name[8] = '\0';
		EventArgsInit(&e, "freerdp");
		e.name = name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelDisconnected(instance->context->pubSub, instance->context, &e);
	}

	channels->connected = FALSE;
	return error;
}

void freerdp_channels_close(rdpChannels* channels, freerdp* instance)
{
	int index;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(
			    pChannelClientData->pInitHandle, CHANNEL_EVENT_TERMINATED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			        pChannelClientData->pInitHandle, CHANNEL_EVENT_TERMINATED, 0, 0);
		}
	}

	channels->clientDataCount = 0;
	MessageQueue_PostQuit(channels->queue, 0);

	for (index = 0; index < channels->openDataCount; index++)
	{
		pChannelOpenData = &channels->openDataList[index];
		freerdp_channel_remove_open_handle_data(&g_ChannelHandles, pChannelOpenData->OpenHandle);

		if (channels->openHandles)
			HashTable_Remove(channels->openHandles,
			                 (void*)(UINT_PTR)pChannelOpenData->OpenHandle);
	}

	channels->openDataCount = 0;
	channels->initDataCount = 0;
	instance->settings->ChannelCount = 0;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelInitEx(LPVOID lpUserParam, LPVOID clientContext,
        LPVOID pInitHandle,
        PCHANNEL_DEF pChannel, INT channelCount, ULONG versionRequested,
        PCHANNEL_INIT_EVENT_EX_FN pChannelInitEventProcEx)
{
	INT index;
	CHANNEL_DEF* channel;
	rdpSettings* settings;
	PCHANNEL_DEF pChannelDef;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	rdpChannels* channels = (rdpChannels*) pInitHandle;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!pChannel || (channelCount <= 0) || !pChannelInitEventProcEx)
		return CHANNEL_RC_INITIALIZATION_ERROR;

	pChannelInitData = (CHANNEL_INIT_DATA*) pInitHandle;
	channels = pChannelInitData->channels;
	pChannelInitData->pInterface = clientContext;

	if (!channels->can_call_init)
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;

	if ((channels->openDataCount + channelCount) > CHANNEL_MAX_COUNT)
		return CHANNEL_RC_TOO_MANY_CHANNELS;

	if (!pChannel)
		return CHANNEL_RC_BAD_CHANNEL;

	if (channels->connected)
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
	pChannelClientData->pChannelInitEventProcEx = pChannelInitEventProcEx;
	pChannelClientData->pInitHandle = pInitHandle;
	pChannelClientData->lpUserParam = lpUserParam;
	channels->clientDataCount++;
	settings = channels->instance->context->settings;

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];
		pChannelOpenData = &channels->openDataList[channels->openDataCount];
		pChannelOpenData->OpenHandle = InterlockedIncrement(&g_OpenHandleSeq);
		pChannelOpenData->channels = channels;
		pChannelOpenData->lpUserParam = lpUserParam;
		HashTable_Add(channels->openHandles, (void*)(UINT_PTR) pChannelOpenData->OpenHandle,
		              (void*) pChannelOpenData);
		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (settings->ChannelCount < CHANNEL_MAX_COUNT)
		{
			channel = &settings->ChannelDefArray[settings->ChannelCount];
			strncpy(channel->name, pChannelDef->name, 7);
			channel->options = pChannelDef->options;
			settings->ChannelCount++;
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelInit(LPVOID* ppInitHandle,
        PCHANNEL_DEF pChannel,
        INT channelCount, ULONG versionRequested,
        PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	INT index;
	void* pInterface;
	CHANNEL_DEF* channel;
	rdpSettings* settings;
	PCHANNEL_DEF pChannelDef;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	rdpChannels* channels = g_channels;

	if (!ppInitHandle || !channels)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!pChannel || (channelCount <= 0) || !pChannelInitEventProc)
		return CHANNEL_RC_INITIALIZATION_ERROR;

	pInterface = g_pInterface;
	pChannelInitData = &(channels->initDataList[channels->initDataCount]);
	*ppInitHandle = pChannelInitData;
	channels->initDataCount++;
	pChannelInitData->channels = channels;
	pChannelInitData->pInterface = pInterface;

	if (!channels->can_call_init)
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;

	if (channels->openDataCount + channelCount > CHANNEL_MAX_COUNT)
		return CHANNEL_RC_TOO_MANY_CHANNELS;

	if (!pChannel)
		return CHANNEL_RC_BAD_CHANNEL;

	if (channels->connected)
		return CHANNEL_RC_ALREADY_CONNECTED;

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
	}

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];

		if (freerdp_channels_find_channel_open_data_by_name(channels,
		        pChannelDef->name) != 0)
		{
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->pChannelInitEventProc = pChannelInitEventProc;
	pChannelClientData->pInitHandle = *ppInitHandle;
	channels->clientDataCount++;
	settings = channels->instance->context->settings;

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];
		pChannelOpenData = &channels->openDataList[channels->openDataCount];
		pChannelOpenData->OpenHandle = InterlockedIncrement(&g_OpenHandleSeq);
		pChannelOpenData->channels = channels;
		freerdp_channel_add_open_handle_data(&g_ChannelHandles, pChannelOpenData->OpenHandle,
		                                     (void*) channels);
		HashTable_Add(channels->openHandles, (void*)(UINT_PTR) pChannelOpenData->OpenHandle,
		              (void*) pChannelOpenData);
		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (settings->ChannelCount < CHANNEL_MAX_COUNT)
		{
			channel = &settings->ChannelDefArray[settings->ChannelCount];
			strncpy(channel->name, pChannelDef->name, 7);
			channel->options = pChannelDef->options;
			settings->ChannelCount++;
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelOpenEx(LPVOID pInitHandle,
        LPDWORD pOpenHandle, PCHAR pChannelName, PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx)
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

	if (!pChannelOpenEventProcEx)
		return CHANNEL_RC_BAD_PROC;

	if (!channels->connected)
		return CHANNEL_RC_NOT_CONNECTED;

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, pChannelName);

	if (!pChannelOpenData)
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;

	if (pChannelOpenData->flags == 2)
		return CHANNEL_RC_ALREADY_OPEN;

	pChannelOpenData->flags = 2; /* open */
	pChannelOpenData->pInterface = pInterface;
	pChannelOpenData->pChannelOpenEventProcEx = pChannelOpenEventProcEx;
	*pOpenHandle = pChannelOpenData->OpenHandle;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelOpen(LPVOID pInitHandle,
        LPDWORD pOpenHandle, PCHAR pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
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

	if (!channels->connected)
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

static UINT VCAPITYPE FreeRDP_VirtualChannelCloseEx(LPVOID pInitHandle, DWORD openHandle)
{
	rdpChannels* channels = NULL;
	CHANNEL_INIT_DATA* pChannelInitData = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData = NULL;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	pChannelInitData = (CHANNEL_INIT_DATA*) pInitHandle;
	channels = pChannelInitData->channels;

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(channels->openHandles, (void*)(UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenData->flags = 0;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelClose(DWORD openHandle)
{
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	channels = (rdpChannels*) freerdp_channel_get_open_handle_data(&g_ChannelHandles, openHandle);

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(channels->openHandles, (void*)(UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenData->flags = 0;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelWriteEx(LPVOID pInitHandle, DWORD openHandle,
        LPVOID pData, ULONG dataLength, LPVOID pUserData)
{
	rdpChannels* channels = NULL;
	CHANNEL_INIT_DATA* pChannelInitData = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData = NULL;
	CHANNEL_OPEN_EVENT* pChannelOpenEvent = NULL;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	pChannelInitData = (CHANNEL_INIT_DATA*) pInitHandle;
	channels = pChannelInitData->channels;

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(channels->openHandles, (void*)(UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!channels->connected)
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

	if (!MessageQueue_Post(channels->queue, (void*) channels, 0, (void*) pChannelOpenEvent, NULL))
	{
		free(pChannelOpenEvent);
		return CHANNEL_RC_NO_MEMORY;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelWrite(DWORD openHandle,
        LPVOID pData, ULONG dataLength, LPVOID pUserData)
{
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_OPEN_EVENT* pChannelOpenEvent;
	rdpChannels* channels = (rdpChannels*) freerdp_channel_get_open_handle_data(&g_ChannelHandles,
	                        openHandle);

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(channels->openHandles, (void*)(UINT_PTR) openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!channels->connected)
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

	if (!MessageQueue_Post(channels->queue, (void*) channels, 0, (void*) pChannelOpenEvent, NULL))
	{
		free(pChannelOpenEvent);
		return CHANNEL_RC_NO_MEMORY;
	}

	return CHANNEL_RC_OK;
}

static BOOL freerdp_channels_is_loaded(rdpChannels* channels, PVIRTUALCHANNELENTRY entry)
{
	int i;

	for (i = 0; i < channels->clientDataCount; i++)
	{
		CHANNEL_CLIENT_DATA* pChannelClientData = &channels->clientDataList[i];

		if (pChannelClientData->entry == entry)
			return TRUE;
	}

	return FALSE;
}

static BOOL freerdp_channels_is_loaded_ex(rdpChannels* channels, PVIRTUALCHANNELENTRYEX entryEx)
{
	int i;

	for (i = 0; i < channels->clientDataCount; i++)
	{
		CHANNEL_CLIENT_DATA* pChannelClientData = &channels->clientDataList[i];

		if (pChannelClientData->entryEx == entryEx)
			return TRUE;
	}

	return FALSE;
}

int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings,
                                 PVIRTUALCHANNELENTRY entry, void* data)
{
	int status;
	CHANNEL_ENTRY_POINTS_FREERDP EntryPoints;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (channels->clientDataCount + 1 > CHANNEL_MAX_COUNT)
	{
		WLog_ERR(TAG,  "error: too many channels");
		return 1;
	}

	if (freerdp_channels_is_loaded(channels, entry))
	{
		WLog_WARN(TAG, "Skipping, channel already loaded");
		return 0;
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->entry = entry;
	ZeroMemory(&EntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));
	EntryPoints.cbSize = sizeof(EntryPoints);
	EntryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	EntryPoints.pVirtualChannelInit = FreeRDP_VirtualChannelInit;
	EntryPoints.pVirtualChannelOpen = FreeRDP_VirtualChannelOpen;
	EntryPoints.pVirtualChannelClose = FreeRDP_VirtualChannelClose;
	EntryPoints.pVirtualChannelWrite = FreeRDP_VirtualChannelWrite;
	EntryPoints.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	EntryPoints.ppInterface = &g_pInterface;
	EntryPoints.pExtendedData = data;
	EntryPoints.context = ((freerdp*)settings->instance)->context;
	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	EnterCriticalSection(&channels->channelsLock);
	g_pInterface = NULL;
	g_channels = channels;
	status = pChannelClientData->entry((PCHANNEL_ENTRY_POINTS) &EntryPoints);
	LeaveCriticalSection(&channels->channelsLock);
	/* disable MyVirtualChannelInit */
	channels->can_call_init = FALSE;

	if (!status)
	{
		WLog_ERR(TAG,  "error: channel export function call failed");
		return 1;
	}

	return 0;
}

int freerdp_channels_client_load_ex(rdpChannels* channels, rdpSettings* settings,
                                    PVIRTUALCHANNELENTRYEX entryEx, void* data)
{
	int status;
	void* pInitHandle = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX EntryPointsEx;
	CHANNEL_INIT_DATA* pChannelInitData = NULL;
	CHANNEL_CLIENT_DATA* pChannelClientData = NULL;

	if (channels->clientDataCount + 1 > CHANNEL_MAX_COUNT)
	{
		WLog_ERR(TAG,  "error: too many channels");
		return 1;
	}

	if (freerdp_channels_is_loaded_ex(channels, entryEx))
	{
		WLog_WARN(TAG, "Skipping, channel already loaded");
		return 0;
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->entryEx = entryEx;
	pChannelInitData = &(channels->initDataList[channels->initDataCount++]);
	pInitHandle = pChannelInitData;
	pChannelInitData->channels = channels;
	ZeroMemory(&EntryPointsEx, sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	EntryPointsEx.cbSize = sizeof(EntryPointsEx);
	EntryPointsEx.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	EntryPointsEx.pVirtualChannelInitEx = FreeRDP_VirtualChannelInitEx;
	EntryPointsEx.pVirtualChannelOpenEx = FreeRDP_VirtualChannelOpenEx;
	EntryPointsEx.pVirtualChannelCloseEx = FreeRDP_VirtualChannelCloseEx;
	EntryPointsEx.pVirtualChannelWriteEx = FreeRDP_VirtualChannelWriteEx;
	EntryPointsEx.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	EntryPointsEx.pExtendedData = data;
	EntryPointsEx.context = ((freerdp*) settings->instance)->context;
	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	EnterCriticalSection(&channels->channelsLock);
	status = pChannelClientData->entryEx((PCHANNEL_ENTRY_POINTS_EX) &EntryPointsEx, pInitHandle);
	LeaveCriticalSection(&channels->channelsLock);
	/* disable MyVirtualChannelInit */
	channels->can_call_init = FALSE;

	if (!status)
	{
		WLog_ERR(TAG, "error: channel export function call failed");
		return 1;
	}

	return 0;
}

/**
 * this is called when processing the command line parameters
 * called only from main thread
 */
int freerdp_channels_load_plugin(rdpChannels* channels, rdpSettings* settings, const char* name,
                                 void* data)
{
	PVIRTUALCHANNELENTRY entry;
	entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (!entry)
		return 1;

	return freerdp_channels_client_load(channels, settings, entry, data);
}
