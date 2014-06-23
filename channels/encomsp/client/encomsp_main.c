/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
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

#include <freerdp/client/encomsp.h>

#include "encomsp_main.h"

static void encomsp_process_connect(encomspPlugin* encomsp)
{

}

static void encomsp_process_receive(encomspPlugin* encomsp, wStream* s)
{

}


/****************************************************************************************/


static wListDictionary* g_InitHandles;
static wListDictionary* g_OpenHandles;

void encomsp_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* encomsp_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void encomsp_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
}

void encomsp_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* encomsp_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void encomsp_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
}

int encomsp_send(encomspPlugin* encomsp, wStream* s)
{
	UINT32 status = 0;
	encomspPlugin* plugin = (encomspPlugin*) encomsp;

	if (!plugin)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = plugin->channelEntryPoints.pVirtualChannelWrite(plugin->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		fprintf(stderr, "encomsp_send: VirtualChannelWrite failed %d\n", status);
	}

	return status;
}

static void encomsp_virtual_channel_event_data_received(encomspPlugin* encomsp,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (encomsp->data_in)
			Stream_Free(encomsp->data_in, TRUE);

		encomsp->data_in = Stream_New(NULL, totalLength);
	}

	data_in = encomsp->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			fprintf(stderr, "encomsp_plugin_process_received: read error\n");
		}

		encomsp->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(encomsp->MsgPipe->In, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE encomsp_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	encomspPlugin* encomsp;

	encomsp = (encomspPlugin*) encomsp_get_open_handle_data(openHandle);

	if (!encomsp)
	{
		fprintf(stderr, "encomsp_virtual_channel_open_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			encomsp_virtual_channel_event_data_received(encomsp, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* encomsp_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	encomspPlugin* encomsp = (encomspPlugin*) arg;

	encomsp_process_connect(encomsp);

	while (1)
	{
		if (!MessageQueue_Wait(encomsp->MsgPipe->In))
			break;

		if (MessageQueue_Peek(encomsp->MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				encomsp_process_receive(encomsp, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void encomsp_virtual_channel_event_connected(encomspPlugin* encomsp, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = encomsp->channelEntryPoints.pVirtualChannelOpen(encomsp->InitHandle,
		&encomsp->OpenHandle, encomsp->channelDef.name, encomsp_virtual_channel_open_event);

	encomsp_add_open_handle_data(encomsp->OpenHandle, encomsp);

	if (status != CHANNEL_RC_OK)
	{
		fprintf(stderr, "encomsp_virtual_channel_event_connected: open failed: status: %d\n", status);
		return;
	}

	encomsp->MsgPipe = MessagePipe_New();

	encomsp->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) encomsp_virtual_channel_client_thread, (void*) encomsp, 0, NULL);
}

static void encomsp_virtual_channel_event_terminated(encomspPlugin* encomsp)
{
	MessagePipe_PostQuit(encomsp->MsgPipe, 0);
	WaitForSingleObject(encomsp->thread, INFINITE);

	MessagePipe_Free(encomsp->MsgPipe);
	CloseHandle(encomsp->thread);

	encomsp->channelEntryPoints.pVirtualChannelClose(encomsp->OpenHandle);

	if (encomsp->data_in)
	{
		Stream_Free(encomsp->data_in, TRUE);
		encomsp->data_in = NULL;
	}

	encomsp_remove_open_handle_data(encomsp->OpenHandle);
	encomsp_remove_init_handle_data(encomsp->InitHandle);
}

static VOID VCAPITYPE encomsp_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	encomspPlugin* encomsp;

	encomsp = (encomspPlugin*) encomsp_get_init_handle_data(pInitHandle);

	if (!encomsp)
	{
		fprintf(stderr, "encomsp_virtual_channel_init_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			encomsp_virtual_channel_event_connected(encomsp, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			break;

		case CHANNEL_EVENT_TERMINATED:
			encomsp_virtual_channel_event_terminated(encomsp);
			break;
	}
}

/* encomsp is always built-in */
#define VirtualChannelEntry	encomsp_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	encomspPlugin* encomsp;
	EncomspClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	encomsp = (encomspPlugin*) calloc(1, sizeof(encomspPlugin));

	encomsp->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(encomsp->channelDef.name, "encomsp");

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (EncomspClientContext*) calloc(1, sizeof(EncomspClientContext));

		context->handle = (void*) encomsp;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	CopyMemory(&(encomsp->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	encomsp->channelEntryPoints.pVirtualChannelInit(&encomsp->InitHandle,
		&encomsp->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, encomsp_virtual_channel_init_event);

	encomsp_add_init_handle_data(encomsp->InitHandle, (void*) encomsp);

	return 1;
}
