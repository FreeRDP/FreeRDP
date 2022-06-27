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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <freerdp/log.h>
#include <freerdp/channels/drdynvc.h>

#include "rdp.h"

#include "client.h"

#define TAG FREERDP_TAG("core.client")

/* Use this instance to get access to channels in VirtualChannelInit. It is set during
 * freerdp_connect so channels that use VirtualChannelInit must be initialized from the same thread
 * as freerdp_connect was called */
static WINPR_TLS freerdp* g_Instance = NULL;

/* use global counter to ensure uniqueness across channel manager instances */
static volatile LONG g_OpenHandleSeq = 1;

/* HashTable mapping channel handles to CHANNEL_OPEN_DATA */
static INIT_ONCE g_ChannelHandlesOnce = INIT_ONCE_STATIC_INIT;
static wHashTable* g_ChannelHandles = NULL;

static BOOL freerdp_channels_process_message_free(wMessage* message, DWORD type);

static CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(rdpChannels* channels,
                                                                          const char* name)
{
	int index;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	for (index = 0; index < channels->openDataCount; index++)
	{
		pChannelOpenData = &channels->openDataList[index];

		if (strncmp(name, pChannelOpenData->name, CHANNEL_NAME_LEN + 1) == 0)
			return pChannelOpenData;
	}

	return NULL;
}

/* returns rdpChannel for the channel name passed in */
static rdpMcsChannel* freerdp_channels_find_channel_by_name(rdpRdp* rdp, const char* name)
{
	UINT32 index;
	rdpMcs* mcs = NULL;

	if (!rdp)
		return NULL;

	mcs = rdp->mcs;

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* channel = &mcs->channels[index];

		if (strncmp(name, channel->Name, CHANNEL_NAME_LEN + 1) == 0)
		{
			return channel;
		}
	}

	return NULL;
}

static rdpMcsChannel* freerdp_channels_find_channel_by_id(rdpRdp* rdp, UINT16 channel_id)
{
	UINT32 index;
	rdpMcsChannel* channel = NULL;
	rdpMcs* mcs = NULL;

	if (!rdp)
		return NULL;

	mcs = rdp->mcs;

	for (index = 0; index < mcs->channelCount; index++)
	{
		channel = &mcs->channels[index];

		if (channel->ChannelId == channel_id)
		{
			return channel;
		}
	}

	return NULL;
}

static void channel_queue_message_free(wMessage* msg)
{
	CHANNEL_OPEN_EVENT* ev;

	if (!msg || (msg->id != 0))
		return;

	ev = (CHANNEL_OPEN_EVENT*)msg->wParam;
	free(ev);
}

static void channel_queue_free(void* obj)
{
	wMessage* msg = (wMessage*)obj;
	freerdp_channels_process_message_free(msg, CHANNEL_EVENT_WRITE_CANCELLED);
	channel_queue_message_free(msg);
}

static BOOL CALLBACK init_channel_handles_table(PINIT_ONCE once, PVOID param, PVOID* context)
{
	g_ChannelHandles = HashTable_New(TRUE);
	return TRUE;
}

rdpChannels* freerdp_channels_new(freerdp* instance)
{
	wObject* obj;
	rdpChannels* channels;
	channels = (rdpChannels*)calloc(1, sizeof(rdpChannels));

	if (!channels)
		return NULL;

	InitOnceExecuteOnce(&g_ChannelHandlesOnce, init_channel_handles_table, NULL, NULL);

	if (!g_ChannelHandles)
		goto error;
	if (!InitializeCriticalSectionAndSpinCount(&channels->channelsLock, 4000))
		goto error;

	channels->instance = instance;
	channels->queue = MessageQueue_New(NULL);

	if (!channels->queue)
		goto error;

	obj = MessageQueue_Object(channels->queue);
	obj->fnObjectFree = channel_queue_free;

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

	free(channels);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT freerdp_drdynvc_on_channel_connected(DrdynvcClientContext* context, const char* name,
                                                 void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelConnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*)context->custom;
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
static UINT freerdp_drdynvc_on_channel_disconnected(DrdynvcClientContext* context, const char* name,
                                                    void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelDisconnectedEventArgs e;
	rdpChannels* channels = (rdpChannels*)context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelDisconnected(instance->context->pubSub, instance->context, &e);
	return status;
}

static UINT freerdp_drdynvc_on_channel_attached(DrdynvcClientContext* context, const char* name,
                                                void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelAttachedEventArgs e;
	rdpChannels* channels = (rdpChannels*)context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelAttached(instance->context->pubSub, instance->context, &e);
	return status;
}

static UINT freerdp_drdynvc_on_channel_detached(DrdynvcClientContext* context, const char* name,
                                                void* pInterface)
{
	UINT status = CHANNEL_RC_OK;
	ChannelDetachedEventArgs e;
	rdpChannels* channels = (rdpChannels*)context->custom;
	freerdp* instance = channels->instance;
	EventArgsInit(&e, "freerdp");
	e.name = name;
	e.pInterface = pInterface;
	PubSub_OnChannelDetached(instance->context->pubSub, instance->context, &e);
	return status;
}

void freerdp_channels_register_instance(rdpChannels* channels, freerdp* instance)
{
	/* store instance in TLS so future VirtualChannelInit calls can use it */
	g_Instance = instance;
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

	MessageQueue_Clear(channels->queue);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_INITIALIZED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			                                            pChannelClientData->pInitHandle,
			                                            CHANNEL_EVENT_INITIALIZED, 0, 0);
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
	const char* hostname;
	size_t hostnameLength;
	rdpChannels* channels;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	channels = instance->context->channels;
	hostname = freerdp_settings_get_string(instance->context->settings, FreeRDP_ServerHostname);
	WINPR_ASSERT(hostname);
	hostnameLength = strnlen(hostname, MAX_PATH);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;
		ChannelAttachedEventArgs e = { 0 };
		CHANNEL_OPEN_DATA* pChannelOpenData = NULL;

		cnv.cpv = hostname;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{

			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_ATTACHED, cnv.pv,
			                                          (UINT)hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(
			    pChannelClientData->lpUserParam, pChannelClientData->pInitHandle,
			    CHANNEL_EVENT_ATTACHED, cnv.pv, (UINT)hostnameLength);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		EventArgsInit(&e, "freerdp");
		e.name = pChannelOpenData->name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelAttached(instance->context->pubSub, instance->context, &e);
	}

fail:
	return error;
}

UINT freerdp_channels_detach(freerdp* instance)
{
	UINT error = CHANNEL_RC_OK;
	int index;
	const char* hostname;
	size_t hostnameLength;
	rdpChannels* channels;
	rdpContext* context;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	WINPR_ASSERT(instance);

	context = instance->context;
	WINPR_ASSERT(context);

	channels = context->channels;
	WINPR_ASSERT(channels);

	WINPR_ASSERT(context->settings);
	hostname = freerdp_settings_get_string(context->settings, FreeRDP_ServerHostname);
	WINPR_ASSERT(hostname);
	hostnameLength = strnlen(hostname, MAX_PATH);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		union
		{
			const void* cpv;
			void* pv;
		} cnv;

		ChannelDetachedEventArgs e = { 0 };
		CHANNEL_OPEN_DATA* pChannelOpenData;

		cnv.cpv = hostname;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_DETACHED, cnv.pv,
			                                          (UINT)hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(
			    pChannelClientData->lpUserParam, pChannelClientData->pInitHandle,
			    CHANNEL_EVENT_DETACHED, cnv.pv, (UINT)hostnameLength);
		}

		if (getChannelError(context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		EventArgsInit(&e, "freerdp");
		e.name = pChannelOpenData->name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelDetached(context->pubSub, context, &e);
	}

fail:
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
	const char* hostname;
	size_t hostnameLength;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	WINPR_ASSERT(channels);
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);

	channels->connected = TRUE;
	hostname = freerdp_settings_get_string(instance->context->settings, FreeRDP_ServerHostname);
	WINPR_ASSERT(hostname);
	hostnameLength = strnlen(hostname, MAX_PATH);

	for (index = 0; index < channels->clientDataCount; index++)
	{
		union
		{
			const void* pcb;
			void* pb;
		} cnv;
		ChannelConnectedEventArgs e = { 0 };
		CHANNEL_OPEN_DATA* pChannelOpenData;
		pChannelClientData = &channels->clientDataList[index];

		cnv.pcb = hostname;
		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_CONNECTED, cnv.pb,
			                                          (UINT)hostnameLength);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(
			    pChannelClientData->lpUserParam, pChannelClientData->pInitHandle,
			    CHANNEL_EVENT_CONNECTED, cnv.pb, (UINT)hostnameLength);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			goto fail;

		pChannelOpenData = &channels->openDataList[index];
		EventArgsInit(&e, "freerdp");
		e.name = pChannelOpenData->name;
		e.pInterface = pChannelOpenData->pInterface;
		PubSub_OnChannelConnected(instance->context->pubSub, instance->context, &e);
	}

	channels->drdynvc = (DrdynvcClientContext*)freerdp_channels_get_static_channel_interface(
	    channels, DRDYNVC_SVC_CHANNEL_NAME);

	if (channels->drdynvc)
	{
		channels->drdynvc->custom = (void*)channels;
		channels->drdynvc->OnChannelConnected = freerdp_drdynvc_on_channel_connected;
		channels->drdynvc->OnChannelDisconnected = freerdp_drdynvc_on_channel_disconnected;
		channels->drdynvc->OnChannelAttached = freerdp_drdynvc_on_channel_attached;
		channels->drdynvc->OnChannelDetached = freerdp_drdynvc_on_channel_detached;
	}

fail:
	return error;
}

BOOL freerdp_channels_data(freerdp* instance, UINT16 channelId, const BYTE* cdata, size_t dataSize,
                           UINT32 flags, size_t totalSize)
{
	UINT32 index;
	rdpMcs* mcs;
	rdpChannels* channels;
	rdpMcsChannel* channel = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	union
	{
		const BYTE* pcb;
		BYTE* pb;
	} data;

	data.pcb = cdata;
	if (!instance || !data.pcb)
	{
		WLog_ERR(TAG, "%s(%p, %" PRIu16 ", %p, 0x%08x): Invalid arguments", __FUNCTION__, instance,
		         channelId, data, flags);
		return FALSE;
	}

	mcs = instance->context->rdp->mcs;
	channels = instance->context->channels;

	if (!channels || !mcs)
	{
		return FALSE;
	}

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* cur = &mcs->channels[index];

		if (cur->ChannelId == channelId)
		{
			channel = cur;
			break;
		}
	}

	if (!channel)
	{
		return FALSE;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, channel->Name);

	if (!pChannelOpenData)
	{
		return FALSE;
	}

	if (pChannelOpenData->pChannelOpenEventProc)
	{
		pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle,
		                                        CHANNEL_EVENT_DATA_RECEIVED, data.pb,
		                                        (UINT32)dataSize, (UINT32)totalSize, flags);
	}
	else if (pChannelOpenData->pChannelOpenEventProcEx)
	{
		pChannelOpenData->pChannelOpenEventProcEx(
		    pChannelOpenData->lpUserParam, pChannelOpenData->OpenHandle,
		    CHANNEL_EVENT_DATA_RECEIVED, data.pb, (UINT32)dataSize, (UINT32)totalSize, flags);
	}

	return TRUE;
}

UINT16 freerdp_channels_get_id_by_name(freerdp* instance, const char* channel_name)
{
	rdpMcsChannel* mcsChannel;
	if (!instance || !channel_name)
		return -1;

	mcsChannel = freerdp_channels_find_channel_by_name(instance->context->rdp, channel_name);
	if (!mcsChannel)
		return -1;

	return mcsChannel->ChannelId;
}

const char* freerdp_channels_get_name_by_id(freerdp* instance, UINT16 channelId)
{
	rdpMcsChannel* mcsChannel;
	if (!instance)
		return NULL;

	mcsChannel = freerdp_channels_find_channel_by_id(instance->context->rdp, channelId);
	if (!mcsChannel)
		return NULL;

	return mcsChannel->Name;
}

BOOL freerdp_channels_process_message_free(wMessage* message, DWORD type)
{
	if (message->id == WMQ_QUIT)
	{
		return FALSE;
	}

	if (message->id == 0)
	{
		CHANNEL_OPEN_DATA* pChannelOpenData;
		CHANNEL_OPEN_EVENT* item = (CHANNEL_OPEN_EVENT*)message->wParam;

		if (!item)
			return FALSE;

		pChannelOpenData = item->pChannelOpenData;

		if (pChannelOpenData->pChannelOpenEventProc)
		{
			pChannelOpenData->pChannelOpenEventProc(pChannelOpenData->OpenHandle, type,
			                                        item->UserData, item->DataLength,
			                                        item->DataLength, 0);
		}
		else if (pChannelOpenData->pChannelOpenEventProcEx)
		{
			pChannelOpenData->pChannelOpenEventProcEx(
			    pChannelOpenData->lpUserParam, pChannelOpenData->OpenHandle, type, item->UserData,
			    item->DataLength, item->DataLength, 0);
		}
	}

	return TRUE;
}

static BOOL freerdp_channels_process_message(freerdp* instance, wMessage* message)
{
	BOOL ret = TRUE;
	BOOL rc = FALSE;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(message);

	if (message->id == WMQ_QUIT)
		goto fail;
	else if (message->id == 0)
	{
		rdpMcsChannel* channel;
		CHANNEL_OPEN_DATA* pChannelOpenData;
		CHANNEL_OPEN_EVENT* item = (CHANNEL_OPEN_EVENT*)message->wParam;

		if (!item)
			goto fail;

		pChannelOpenData = item->pChannelOpenData;
		if (pChannelOpenData->flags != 2)
		{
			freerdp_channels_process_message_free(message, CHANNEL_EVENT_WRITE_CANCELLED);
			goto fail;
		}
		channel =
		    freerdp_channels_find_channel_by_name(instance->context->rdp, pChannelOpenData->name);

		if (channel)
			ret = instance->SendChannelData(instance, channel->ChannelId, item->Data,
			                                item->DataLength);
	}

	if (!freerdp_channels_process_message_free(message, CHANNEL_EVENT_WRITE_COMPLETE))
		goto fail;

	rc = ret;

fail:
	IFCALL(message->Free, message);
	return rc;
}

/**
 * called only from main thread
 */
static int freerdp_channels_process_sync(rdpChannels* channels, freerdp* instance)
{
	int status = TRUE;
	wMessage message;

	while (MessageQueue_Peek(channels->queue, &message, TRUE))
	{
		freerdp_channels_process_message(instance, &message);
	}

	return status;
}

/**
 * called only from main thread
 */
#if defined(WITH_FREERDP_DEPRECATED)
BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance, void** read_fds,
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
#endif

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
	event = MessageQueue_Event(channels->queue);
	return event;
}

int freerdp_channels_process_pending_messages(freerdp* instance)
{
	rdpChannels* channels;
	channels = instance->context->channels;

	if (WaitForSingleObject(MessageQueue_Event(channels->queue), 0) == WAIT_OBJECT_0)
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
	if (WaitForSingleObject(MessageQueue_Event(channels->queue), 0) == WAIT_OBJECT_0)
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
		ChannelDisconnectedEventArgs e;
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_DISCONNECTED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			                                            pChannelClientData->pInitHandle,
			                                            CHANNEL_EVENT_DISCONNECTED, 0, 0);
		}

		if (getChannelError(instance->context) != CHANNEL_RC_OK)
			continue;

		pChannelOpenData = &channels->openDataList[index];
		EventArgsInit(&e, "freerdp");
		e.name = pChannelOpenData->name;
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

	WINPR_ASSERT(channels);
	WINPR_ASSERT(instance);

	MessageQueue_PostQuit(channels->queue, 0);
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->clientDataCount; index++)
	{
		pChannelClientData = &channels->clientDataList[index];

		if (pChannelClientData->pChannelInitEventProc)
		{
			pChannelClientData->pChannelInitEventProc(pChannelClientData->pInitHandle,
			                                          CHANNEL_EVENT_TERMINATED, 0, 0);
		}
		else if (pChannelClientData->pChannelInitEventProcEx)
		{
			pChannelClientData->pChannelInitEventProcEx(pChannelClientData->lpUserParam,
			                                            pChannelClientData->pInitHandle,
			                                            CHANNEL_EVENT_TERMINATED, 0, 0);
		}
	}

	channels->clientDataCount = 0;

	for (index = 0; index < channels->openDataCount; index++)
	{
		pChannelOpenData = &channels->openDataList[index];
		HashTable_Remove(g_ChannelHandles, (void*)(UINT_PTR)pChannelOpenData->OpenHandle);
	}

	channels->openDataCount = 0;
	channels->initDataCount = 0;

	WINPR_ASSERT(instance->context);
	WINPR_ASSERT(instance->context->settings);
	instance->context->settings->ChannelCount = 0;
	g_Instance = NULL;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelInitEx(
    LPVOID lpUserParam, LPVOID clientContext, LPVOID pInitHandle, PCHANNEL_DEF pChannel,
    INT channelCount, ULONG versionRequested, PCHANNEL_INIT_EVENT_EX_FN pChannelInitEventProcEx)
{
	INT index;
	rdpSettings* settings;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	rdpChannels* channels;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!pChannel)
		return CHANNEL_RC_BAD_CHANNEL;

	if ((channelCount <= 0) || !pChannelInitEventProcEx)
		return CHANNEL_RC_INITIALIZATION_ERROR;

	pChannelInitData = (CHANNEL_INIT_DATA*)pInitHandle;
	WINPR_ASSERT(pChannelInitData);

	channels = pChannelInitData->channels;
	WINPR_ASSERT(channels);

	if (!channels->can_call_init)
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;

	if ((channels->openDataCount + channelCount) > CHANNEL_MAX_COUNT)
		return CHANNEL_RC_TOO_MANY_CHANNELS;

	if (channels->connected)
		return CHANNEL_RC_ALREADY_CONNECTED;

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
	}

	for (index = 0; index < channelCount; index++)
	{
		const PCHANNEL_DEF pChannelDef = &pChannel[index];

		if (freerdp_channels_find_channel_open_data_by_name(channels, pChannelDef->name) != 0)
		{
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	pChannelInitData->pInterface = clientContext;
	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->pChannelInitEventProcEx = pChannelInitEventProcEx;
	pChannelClientData->pInitHandle = pInitHandle;
	pChannelClientData->lpUserParam = lpUserParam;
	channels->clientDataCount++;

	WINPR_ASSERT(channels->instance);
	WINPR_ASSERT(channels->instance->context);
	settings = channels->instance->context->settings;
	WINPR_ASSERT(settings);

	for (index = 0; index < channelCount; index++)
	{
		const PCHANNEL_DEF pChannelDef = &pChannel[index];
		CHANNEL_OPEN_DATA* pChannelOpenData = &channels->openDataList[channels->openDataCount];

		WINPR_ASSERT(pChannelOpenData);

		pChannelOpenData->OpenHandle = InterlockedIncrement(&g_OpenHandleSeq);
		pChannelOpenData->channels = channels;
		pChannelOpenData->lpUserParam = lpUserParam;
		if (!HashTable_Insert(g_ChannelHandles, (void*)(UINT_PTR)pChannelOpenData->OpenHandle,
		                      (void*)pChannelOpenData))
		{
			pChannelInitData->pInterface = NULL;
			return CHANNEL_RC_INITIALIZATION_ERROR;
		}
		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (settings->ChannelCount < CHANNEL_MAX_COUNT)
		{
			CHANNEL_DEF* channel = freerdp_settings_get_pointer_array_writable(
			    settings, FreeRDP_ChannelDefArray, settings->ChannelCount);
			if (!channel)
				continue;
			strncpy(channel->name, pChannelDef->name, CHANNEL_NAME_LEN);
			channel->options = pChannelDef->options;
			settings->ChannelCount++;
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelInit(LPVOID* ppInitHandle, PCHANNEL_DEF pChannel,
                                                 INT channelCount, ULONG versionRequested,
                                                 PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	INT index;
	CHANNEL_DEF* channel;
	rdpSettings* settings;
	PCHANNEL_DEF pChannelDef;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;
	rdpChannels* channels;

	/* g_Instance should have been set during freerdp_connect - otherwise VirtualChannelInit was
	 * called from a different thread */
	if (!g_Instance || !g_Instance->context)
		return CHANNEL_RC_NOT_INITIALIZED;

	channels = g_Instance->context->channels;

	if (!ppInitHandle || !channels)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	if (!pChannel)
		return CHANNEL_RC_BAD_CHANNEL;

	if ((channelCount <= 0) || !pChannelInitEventProc)
		return CHANNEL_RC_INITIALIZATION_ERROR;

	pChannelInitData = &(channels->initDataList[channels->initDataCount]);
	*ppInitHandle = pChannelInitData;
	channels->initDataCount++;
	pChannelInitData->channels = channels;
	pChannelInitData->pInterface = NULL;

	if (!channels->can_call_init)
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;

	if (channels->openDataCount + channelCount > CHANNEL_MAX_COUNT)
		return CHANNEL_RC_TOO_MANY_CHANNELS;

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
	pChannelClientData->pChannelInitEventProc = pChannelInitEventProc;
	pChannelClientData->pInitHandle = *ppInitHandle;
	channels->clientDataCount++;
	settings = channels->instance->context->settings;

	for (index = 0; index < channelCount; index++)
	{
		UINT32 ChannelCount = freerdp_settings_get_uint32(settings, FreeRDP_ChannelCount);

		pChannelDef = &pChannel[index];
		pChannelOpenData = &channels->openDataList[channels->openDataCount];
		pChannelOpenData->OpenHandle = InterlockedIncrement(&g_OpenHandleSeq);
		pChannelOpenData->channels = channels;
		if (!HashTable_Insert(g_ChannelHandles, (void*)(UINT_PTR)pChannelOpenData->OpenHandle,
		                      (void*)pChannelOpenData))
			return CHANNEL_RC_INITIALIZATION_ERROR;
		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (ChannelCount < CHANNEL_MAX_COUNT)
		{
			channel = freerdp_settings_get_pointer_array_writable(settings, FreeRDP_ChannelDefArray,
			                                                      ChannelCount++);
			strncpy(channel->name, pChannelDef->name, CHANNEL_NAME_LEN);
			channel->options = pChannelDef->options;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, ChannelCount))
				return ERROR_INTERNAL_ERROR;
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE
FreeRDP_VirtualChannelOpenEx(LPVOID pInitHandle, LPDWORD pOpenHandle, PCHAR pChannelName,
                             PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx)
{
	void* pInterface;
	rdpChannels* channels;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	pChannelInitData = (CHANNEL_INIT_DATA*)pInitHandle;
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

static UINT VCAPITYPE FreeRDP_VirtualChannelOpen(LPVOID pInitHandle, LPDWORD pOpenHandle,
                                                 PCHAR pChannelName,
                                                 PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	void* pInterface;
	rdpChannels* channels;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	pChannelInitData = (CHANNEL_INIT_DATA*)pInitHandle;
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
	CHANNEL_OPEN_DATA* pChannelOpenData = NULL;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(g_ChannelHandles, (void*)(UINT_PTR)openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenData->flags = 0;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelClose(DWORD openHandle)
{
	CHANNEL_OPEN_DATA* pChannelOpenData;

	pChannelOpenData = HashTable_GetItemValue(g_ChannelHandles, (void*)(UINT_PTR)openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenData->flags = 0;
	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelWriteEx(LPVOID pInitHandle, DWORD openHandle,
                                                    LPVOID pData, ULONG dataLength,
                                                    LPVOID pUserData)
{
	rdpChannels* channels = NULL;
	CHANNEL_INIT_DATA* pChannelInitData = NULL;
	CHANNEL_OPEN_DATA* pChannelOpenData = NULL;
	CHANNEL_OPEN_EVENT* pChannelOpenEvent = NULL;
	wMessage message;

	if (!pInitHandle)
		return CHANNEL_RC_BAD_INIT_HANDLE;

	pChannelInitData = (CHANNEL_INIT_DATA*)pInitHandle;
	channels = pChannelInitData->channels;

	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pChannelOpenData = HashTable_GetItemValue(g_ChannelHandles, (void*)(UINT_PTR)openHandle);

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

	pChannelOpenEvent = (CHANNEL_OPEN_EVENT*)malloc(sizeof(CHANNEL_OPEN_EVENT));

	if (!pChannelOpenEvent)
		return CHANNEL_RC_NO_MEMORY;

	pChannelOpenEvent->Data = pData;
	pChannelOpenEvent->DataLength = dataLength;
	pChannelOpenEvent->UserData = pUserData;
	pChannelOpenEvent->pChannelOpenData = pChannelOpenData;
	message.context = channels;
	message.id = 0;
	message.wParam = pChannelOpenEvent;
	message.lParam = NULL;
	message.Free = channel_queue_message_free;

	if (!MessageQueue_Dispatch(channels->queue, &message))
	{
		free(pChannelOpenEvent);
		return CHANNEL_RC_NO_MEMORY;
	}

	return CHANNEL_RC_OK;
}

static UINT VCAPITYPE FreeRDP_VirtualChannelWrite(DWORD openHandle, LPVOID pData, ULONG dataLength,
                                                  LPVOID pUserData)
{
	wMessage message;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_OPEN_EVENT* pChannelOpenEvent;
	rdpChannels* channels;

	pChannelOpenData = HashTable_GetItemValue(g_ChannelHandles, (void*)(UINT_PTR)openHandle);

	if (!pChannelOpenData)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	channels = pChannelOpenData->channels;
	if (!channels)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!channels->connected)
		return CHANNEL_RC_NOT_CONNECTED;

	if (!pData)
		return CHANNEL_RC_NULL_DATA;

	if (!dataLength)
		return CHANNEL_RC_ZERO_LENGTH;

	if (pChannelOpenData->flags != 2)
		return CHANNEL_RC_NOT_OPEN;

	pChannelOpenEvent = (CHANNEL_OPEN_EVENT*)malloc(sizeof(CHANNEL_OPEN_EVENT));

	if (!pChannelOpenEvent)
		return CHANNEL_RC_NO_MEMORY;

	pChannelOpenEvent->Data = pData;
	pChannelOpenEvent->DataLength = dataLength;
	pChannelOpenEvent->UserData = pUserData;
	pChannelOpenEvent->pChannelOpenData = pChannelOpenData;
	message.context = channels;
	message.id = 0;
	message.wParam = pChannelOpenEvent;
	message.lParam = NULL;
	message.Free = channel_queue_message_free;

	if (!MessageQueue_Dispatch(channels->queue, &message))
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
	CHANNEL_ENTRY_POINTS_FREERDP EntryPoints = { 0 };
	CHANNEL_CLIENT_DATA* pChannelClientData;

	WINPR_ASSERT(channels);
	WINPR_ASSERT(channels->instance);
	WINPR_ASSERT(channels->instance->context);
	WINPR_ASSERT(entry);

	if (channels->clientDataCount + 1 > CHANNEL_MAX_COUNT)
	{
		WLog_ERR(TAG, "error: too many channels");
		return 1;
	}

	if (freerdp_channels_is_loaded(channels, entry))
	{
		WLog_WARN(TAG, "Skipping, channel already loaded");
		return 0;
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->entry = entry;

	EntryPoints.cbSize = sizeof(EntryPoints);
	EntryPoints.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	EntryPoints.pVirtualChannelInit = FreeRDP_VirtualChannelInit;
	EntryPoints.pVirtualChannelOpen = FreeRDP_VirtualChannelOpen;
	EntryPoints.pVirtualChannelClose = FreeRDP_VirtualChannelClose;
	EntryPoints.pVirtualChannelWrite = FreeRDP_VirtualChannelWrite;
	EntryPoints.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	EntryPoints.pExtendedData = data;
	EntryPoints.context = channels->instance->context;
	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	EnterCriticalSection(&channels->channelsLock);
	status = pChannelClientData->entry((PCHANNEL_ENTRY_POINTS)&EntryPoints);
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

int freerdp_channels_client_load_ex(rdpChannels* channels, rdpSettings* settings,
                                    PVIRTUALCHANNELENTRYEX entryEx, void* data)
{
	int status;
	void* pInitHandle = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX EntryPointsEx = { 0 };
	CHANNEL_INIT_DATA* pChannelInitData = NULL;
	CHANNEL_CLIENT_DATA* pChannelClientData = NULL;

	WINPR_ASSERT(channels);
	WINPR_ASSERT(channels->instance);
	WINPR_ASSERT(channels->instance->context);
	WINPR_ASSERT(entryEx);

	if (channels->clientDataCount + 1 > CHANNEL_MAX_COUNT)
	{
		WLog_ERR(TAG, "error: too many channels");
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
	EntryPointsEx.cbSize = sizeof(EntryPointsEx);
	EntryPointsEx.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	EntryPointsEx.pVirtualChannelInitEx = FreeRDP_VirtualChannelInitEx;
	EntryPointsEx.pVirtualChannelOpenEx = FreeRDP_VirtualChannelOpenEx;
	EntryPointsEx.pVirtualChannelCloseEx = FreeRDP_VirtualChannelCloseEx;
	EntryPointsEx.pVirtualChannelWriteEx = FreeRDP_VirtualChannelWriteEx;
	EntryPointsEx.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	EntryPointsEx.pExtendedData = data;
	EntryPointsEx.context = channels->instance->context;
	/* enable VirtualChannelInit */
	channels->can_call_init = TRUE;
	EnterCriticalSection(&channels->channelsLock);
	status = pChannelClientData->entryEx((PCHANNEL_ENTRY_POINTS_EX)&EntryPointsEx, pInitHandle);
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
