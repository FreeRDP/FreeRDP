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

#ifdef WITH_DEBUG_CHANNELS
#define DEBUG_CHANNELS(fmt, ...) DEBUG_CLASS(CHANNELS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CHANNELS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#define CHANNEL_MAX_COUNT 30

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

struct rdp_library_data
{
	PVIRTUALCHANNELENTRY entry; /* the one and only exported function */
	PCHANNEL_INIT_EVENT_FN init_event_proc;
	void* init_handle;
};
typedef struct rdp_library_data rdpLibraryData;

struct rdp_channel_data
{
	char name[CHANNEL_NAME_LEN + 1];
	int open_handle;
	int options;
	int flags; /* 0 nothing 1 init 2 open */
	PCHANNEL_OPEN_EVENT_FN open_event_proc;
};
typedef struct rdp_channel_data rdpChannelData;

struct _CHANNEL_OPEN_EVENT
{
	void* Data;
	UINT32 DataLength;
	void* UserData;
	int Index;
};
typedef struct _CHANNEL_OPEN_EVENT CHANNEL_OPEN_EVENT;

typedef struct rdp_init_handle rdpInitHandle;

struct rdp_init_handle
{
	rdpChannels* channels;
};

struct rdp_channels
{
	/**
	 * Only the main thread alters these arrays, before any
	 * library thread is allowed in(post_connect is called)
	 * so no need to use mutex locking
	 * After post_connect, each library thread can only access it's
	 * own array items
	 * ie, no two threads can access index 0, ...
	 */

	int libraryDataCount;
	rdpLibraryData libraryDataList[CHANNEL_MAX_COUNT];

	int channelDataCount;
	rdpChannelData channelDataList[CHANNEL_MAX_COUNT];

	int initHandleCount;
	rdpInitHandle initHandleList[CHANNEL_MAX_COUNT];

	/* control for entry into MyVirtualChannelInit */
	int can_call_init;
	rdpSettings* settings;

	/* true once freerdp_channels_post_connect is called */
	int is_connected;

	/* used for locating the channels for a given instance */
	freerdp* instance;

	wMessagePipe* MsgPipe;
};

/**
 * The current channel manager reference passes from VirtualChannelEntry to
 * VirtualChannelInit for the pInitHandle.
 */
static rdpChannels* g_init_channels;

static wArrayList* g_ChannelsList = NULL;

/* To generate unique sequence for all open handles */
static int g_open_handle_sequence;

/* For locking the global resources */
static HANDLE g_mutex_init;

static rdpChannels* freerdp_channels_find_by_open_handle(int open_handle, int* pindex)
{
	int i, j;
	BOOL found = FALSE;
	rdpChannels* channels = NULL;

	ArrayList_Lock(g_ChannelsList);

	i = j = 0;
	channels = (rdpChannels*) ArrayList_GetItem(g_ChannelsList, i++);

	while (channels)
	{
		for (j = 0; j < channels->channelDataCount; j++)
		{
			if (channels->channelDataList[j].open_handle == open_handle)
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

static rdpChannels* freerdp_channels_find_by_instance(freerdp* instance)
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

/* returns rdpChannelData for the channel name passed in */
static rdpChannelData* freerdp_channels_find_channel_data_by_name(rdpChannels* channels, const char* channel_name, int* pindex)
{
	int index;
	rdpChannelData* lchannel_data;

	for (index = 0; index < channels->channelDataCount; index++)
	{
		lchannel_data = &channels->channelDataList[index];

		if (strcmp(channel_name, lchannel_data->name) == 0)
		{
			if (pindex != 0)
				*pindex = index;

			return lchannel_data;
		}
	}

	return NULL;
}

/* returns rdpChannel for the channel id passed in */
static rdpChannel* freerdp_channels_find_channel_by_id(rdpChannels* channels, rdpSettings* settings, int channel_id, int* pindex)
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
static rdpChannel* freerdp_channels_find_channel_by_name(rdpChannels* channels,
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

/**
 * must be called by same thread that calls freerdp_chanman_load_plugin
 * according to MS docs
 * only called from main thread
 */
static UINT32 FREERDP_CC MyVirtualChannelInit(void** ppInitHandle, PCHANNEL_DEF pChannel,
	int channelCount, UINT32 versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	int index;
	rdpChannel* channel;
	rdpChannels* channels;
	rdpLibraryData* llib;
	PCHANNEL_DEF lchannel_def;
	rdpChannelData* lchannel_data;

	if (!ppInitHandle)
	{
		DEBUG_CHANNELS("error bad init handle");
		return CHANNEL_RC_BAD_INIT_HANDLE;
	}

	channels = g_init_channels;

	channels->initHandleList[channels->initHandleCount].channels = channels;
	*ppInitHandle = &channels->initHandleList[channels->initHandleCount];

	channels->initHandleCount++;

	DEBUG_CHANNELS("enter");

	if (!channels->can_call_init)
	{
		DEBUG_CHANNELS("error not in entry");
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;
	}

	if (channels->channelDataCount + channelCount >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANNELS("error too many channels");
		return CHANNEL_RC_TOO_MANY_CHANNELS;
	}

	if (!pChannel)
	{
		DEBUG_CHANNELS("error bad channel");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (channels->is_connected)
	{
		DEBUG_CHANNELS("error already connected");
		return CHANNEL_RC_ALREADY_CONNECTED;
	}

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
		DEBUG_CHANNELS("warning version");
	}

	for (index = 0; index < channelCount; index++)
	{
		lchannel_def = &pChannel[index];

		if (freerdp_channels_find_channel_data_by_name(channels, lchannel_def->name, 0) != 0)
		{
			DEBUG_CHANNELS("error channel already used");
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	llib = &channels->libraryDataList[channels->libraryDataCount];
	llib->init_event_proc = pChannelInitEventProc;
	llib->init_handle = *ppInitHandle;
	channels->libraryDataCount++;

	for (index = 0; index < channelCount; index++)
	{
		lchannel_def = &pChannel[index];
		lchannel_data = &channels->channelDataList[channels->channelDataCount];

		lchannel_data->open_handle = g_open_handle_sequence++;

		lchannel_data->flags = 1; /* init */
		strncpy(lchannel_data->name, lchannel_def->name, CHANNEL_NAME_LEN);
		lchannel_data->options = lchannel_def->options;

		if (channels->settings->ChannelCount < 16)
		{
			channel = channels->settings->ChannelDefArray + channels->settings->ChannelCount;
			strncpy(channel->Name, lchannel_def->name, 7);
			channel->options = lchannel_def->options;
			channels->settings->ChannelCount++;
		}
		else
		{
			DEBUG_CHANNELS("warning more than 16 channels");
		}

		channels->channelDataCount++;
	}

	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same channel name registered
 */
static UINT32 FREERDP_CC MyVirtualChannelOpen(void* pInitHandle, UINT32* pOpenHandle,
	char* pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	int index;
	rdpChannels* channels;
	rdpChannelData* lchannel_data;

	DEBUG_CHANNELS("enter");

	channels = ((rdpInitHandle*) pInitHandle)->channels;

	if (!pOpenHandle)
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!pChannelOpenEventProc)
	{
		DEBUG_CHANNELS("error bad proc");
		return CHANNEL_RC_BAD_PROC;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, pChannelName, &index);

	if (!lchannel_data)
	{
		DEBUG_CHANNELS("error channel name");
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;
	}

	if (lchannel_data->flags == 2)
	{
		DEBUG_CHANNELS("error channel already open");
		return CHANNEL_RC_ALREADY_OPEN;
	}

	lchannel_data->flags = 2; /* open */
	lchannel_data->open_event_proc = pChannelOpenEventProc;
	*pOpenHandle = lchannel_data->open_handle;

	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same openHandle
 */
static UINT32 FREERDP_CC MyVirtualChannelClose(UINT32 openHandle)
{
	int index;
	rdpChannels* channels;
	rdpChannelData* lchannel_data;

	DEBUG_CHANNELS("enter");

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((channels == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	lchannel_data = &channels->channelDataList[index];

	if (lchannel_data->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	lchannel_data->flags = 0;

	return CHANNEL_RC_OK;
}

/* can be called from any thread */
static UINT32 FREERDP_CC MyVirtualChannelWrite(UINT32 openHandle, void* pData, UINT32 dataLength, void* pUserData)
{
	int index;
	CHANNEL_OPEN_EVENT* item;
	rdpChannels* channels;
	rdpChannelData* lchannel_data;

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

	lchannel_data = &channels->channelDataList[index];

	if (lchannel_data->flags != 2)
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

static UINT32 FREERDP_CC MyVirtualChannelEventPush(UINT32 openHandle, wMessage* event)
{
	int index;
	rdpChannels* channels;
	rdpChannelData* lchannel_data;

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

	lchannel_data = &channels->channelDataList[index];

	if (lchannel_data->flags != 2)
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
	g_init_channels = NULL;
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
	rdpLibraryData* lib;
	CHANNEL_ENTRY_POINTS_EX ep;

	if (channels->libraryDataCount + 1 >= CHANNEL_MAX_COUNT)
	{
		fprintf(stderr, "error: too many channels\n");
		return 1;
	}

	lib = &channels->libraryDataList[channels->libraryDataCount];
	lib->entry = (PVIRTUALCHANNELENTRY) entry;

	ep.cbSize = sizeof(ep);
	ep.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	ep.pVirtualChannelInit = MyVirtualChannelInit;
	ep.pVirtualChannelOpen = MyVirtualChannelOpen;
	ep.pVirtualChannelClose = MyVirtualChannelClose;
	ep.pVirtualChannelWrite = MyVirtualChannelWrite;

	ep.pExtendedData = data;
	ep.pVirtualChannelEventPush = MyVirtualChannelEventPush;

	/* enable MyVirtualChannelInit */
	channels->can_call_init = 1;
	channels->settings = settings;

	WaitForSingleObject(g_mutex_init, INFINITE);

	g_init_channels = channels;
	status = lib->entry((PCHANNEL_ENTRY_POINTS) &ep);
	g_init_channels = NULL;

	ReleaseMutex(g_mutex_init);

	/* disable MyVirtualChannelInit */
	channels->settings = 0;
	channels->can_call_init = 0;

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

/**
 * go through and inform all the libraries that we are initialized
 * called only from main thread
 */
int freerdp_channels_pre_connect(rdpChannels* channels, freerdp* instance)
{
	int index;
	rdpLibraryData* llib;

	DEBUG_CHANNELS("enter");
	channels->instance = instance;

	for (index = 0; index < channels->libraryDataCount; index++)
	{
		llib = &channels->libraryDataList[index];

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_INITIALIZED, 0, 0);
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
	int hostname_len;
	rdpLibraryData* llib;

	channels->is_connected = 1;
	hostname = instance->settings->ServerHostname;
	hostname_len = strlen(hostname);

	DEBUG_CHANNELS("hostname [%s] channels->num_libs [%d]", hostname, channels->libraryDataCount);

	for (index = 0; index < channels->libraryDataCount; index++)
	{
		llib = &channels->libraryDataList[index];

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_CONNECTED, hostname, hostname_len);
	}

	return 0;
}

/**
 * data comming from the server to the client
 * called only from main thread
 */
int freerdp_channels_data(freerdp* instance, int channel_id, void* data, int data_size, int flags, int total_size)
{
	int index;
	rdpChannel* channel;
	rdpChannels* channels;
	rdpChannelData* lchannel_data;

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

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, channel->Name, &index);

	if (!lchannel_data)
	{
		DEBUG_CHANNELS("could not find channel name");
		return 1;
	}

	if (lchannel_data->open_event_proc != 0)
	{
		lchannel_data->open_event_proc(lchannel_data->open_handle,
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
	int index;
	const char* name = NULL;
	rdpChannelData* lchannel_data;

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
		DEBUG_CHANNELS("unknown event_class %d", event->event_class);
		freerdp_event_free(event);
		return 1;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, name, &index);

	if (!lchannel_data)
	{
		DEBUG_CHANNELS("could not find channel name %s", name);
		freerdp_event_free(event);
		return 1;
	}

	if (lchannel_data->open_event_proc)
	{
		lchannel_data->open_event_proc(lchannel_data->open_handle, CHANNEL_EVENT_USER,
			event, sizeof(wMessage), sizeof(wMessage), 0);
	}

	return 0;
}

/**
 * called only from main thread
 */
static void freerdp_channels_process_sync(rdpChannels* channels, freerdp* instance)
{
	wMessage message;
	wMessage* event;
	rdpChannel* channel;
	CHANNEL_OPEN_EVENT* item;
	rdpChannelData* lchannel_data;

	while (MessageQueue_Peek(channels->MsgPipe->Out, &message, TRUE))
	{
		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			item = (CHANNEL_OPEN_EVENT*) message.wParam;

			if (!item)
				break;

			lchannel_data = &channels->channelDataList[item->Index];

			channel = freerdp_channels_find_channel_by_name(channels, instance->settings,
				lchannel_data->name, &item->Index);

			if (channel)
				instance->SendChannelData(instance, channel->ChannelId, item->Data, item->DataLength);

			if (lchannel_data->open_event_proc)
			{
				lchannel_data->open_event_proc(lchannel_data->open_handle,
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
		freerdp_channels_process_sync(channels, instance);
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
	rdpLibraryData* llib;

	DEBUG_CHANNELS("closing");
	channels->is_connected = 0;
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->libraryDataCount; index++)
	{
		llib = &channels->libraryDataList[index];

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_TERMINATED, 0, 0);
	}
}

/* Local variables: */
/* c-basic-offset: 8 */
/* c-file-style: "linux" */
/* End: */
