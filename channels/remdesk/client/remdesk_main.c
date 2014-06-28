/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Assistance Virtual Channel
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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/client/remdesk.h>

#include "remdesk_main.h"

RemdeskClientContext* remdesk_get_client_interface(remdeskPlugin* remdesk)
{
	RemdeskClientContext* pInterface;
	pInterface = (RemdeskClientContext*) remdesk->channelEntryPoints.pInterface;
	return pInterface;
}

static int remdesk_process_receive(remdeskPlugin* remdesk, wStream* s)
{
	int status = 1;

	printf("RemdeskReceive: %d\n", Stream_GetRemainingLength(s));
	winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));

	return status;
}

static void remdesk_process_connect(remdeskPlugin* remdesk)
{
	printf("RemdeskProcessConnect\n");
}

/****************************************************************************************/

static wListDictionary* g_InitHandles;
static wListDictionary* g_OpenHandles;

void remdesk_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* remdesk_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void remdesk_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
}

void remdesk_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* remdesk_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void remdesk_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
}

int remdesk_send(remdeskPlugin* remdesk, wStream* s)
{
	UINT32 status = 0;
	remdeskPlugin* plugin = (remdeskPlugin*) remdesk;

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
		fprintf(stderr, "remdesk_send: VirtualChannelWrite failed %d\n", status);
	}

	return status;
}

static void remdesk_virtual_channel_event_data_received(remdeskPlugin* remdesk,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (remdesk->data_in)
			Stream_Free(remdesk->data_in, TRUE);

		remdesk->data_in = Stream_New(NULL, totalLength);
	}

	data_in = remdesk->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			fprintf(stderr, "remdesk_plugin_process_received: read error\n");
		}

		remdesk->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(remdesk->MsgPipe->In, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE remdesk_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	remdeskPlugin* remdesk;

	remdesk = (remdeskPlugin*) remdesk_get_open_handle_data(openHandle);

	if (!remdesk)
	{
		fprintf(stderr, "remdesk_virtual_channel_open_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			remdesk_virtual_channel_event_data_received(remdesk, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* remdesk_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	remdeskPlugin* remdesk = (remdeskPlugin*) arg;

	remdesk_process_connect(remdesk);

	while (1)
	{
		if (!MessageQueue_Wait(remdesk->MsgPipe->In))
			break;

		if (MessageQueue_Peek(remdesk->MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				remdesk_process_receive(remdesk, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void remdesk_virtual_channel_event_connected(remdeskPlugin* remdesk, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = remdesk->channelEntryPoints.pVirtualChannelOpen(remdesk->InitHandle,
		&remdesk->OpenHandle, remdesk->channelDef.name, remdesk_virtual_channel_open_event);

	remdesk_add_open_handle_data(remdesk->OpenHandle, remdesk);

	if (status != CHANNEL_RC_OK)
	{
		fprintf(stderr, "remdesk_virtual_channel_event_connected: open failed: status: %d\n", status);
		return;
	}

	remdesk->MsgPipe = MessagePipe_New();

	remdesk->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) remdesk_virtual_channel_client_thread, (void*) remdesk, 0, NULL);
}

static void remdesk_virtual_channel_event_terminated(remdeskPlugin* remdesk)
{
	MessagePipe_PostQuit(remdesk->MsgPipe, 0);
	WaitForSingleObject(remdesk->thread, INFINITE);

	MessagePipe_Free(remdesk->MsgPipe);
	CloseHandle(remdesk->thread);

	remdesk->channelEntryPoints.pVirtualChannelClose(remdesk->OpenHandle);

	if (remdesk->data_in)
	{
		Stream_Free(remdesk->data_in, TRUE);
		remdesk->data_in = NULL;
	}

	remdesk_remove_open_handle_data(remdesk->OpenHandle);
	remdesk_remove_init_handle_data(remdesk->InitHandle);
}

static VOID VCAPITYPE remdesk_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	remdeskPlugin* remdesk;

	remdesk = (remdeskPlugin*) remdesk_get_init_handle_data(pInitHandle);

	if (!remdesk)
	{
		fprintf(stderr, "remdesk_virtual_channel_init_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			remdesk_virtual_channel_event_connected(remdesk, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			break;

		case CHANNEL_EVENT_TERMINATED:
			remdesk_virtual_channel_event_terminated(remdesk);
			break;
	}
}

/* remdesk is always built-in */
#define VirtualChannelEntry	remdesk_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	remdeskPlugin* remdesk;
	RemdeskClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	remdesk = (remdeskPlugin*) calloc(1, sizeof(remdeskPlugin));

	remdesk->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(remdesk->channelDef.name, "remdesk");

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (RemdeskClientContext*) calloc(1, sizeof(RemdeskClientContext));

		context->handle = (void*) remdesk;

		*(pEntryPointsEx->ppInterface) = (void*) context;
	}

	CopyMemory(&(remdesk->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	remdesk->channelEntryPoints.pVirtualChannelInit(&remdesk->InitHandle,
		&remdesk->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, remdesk_virtual_channel_init_event);

	remdesk_add_init_handle_data(remdesk->InitHandle, (void*) remdesk);

	return 1;
}
