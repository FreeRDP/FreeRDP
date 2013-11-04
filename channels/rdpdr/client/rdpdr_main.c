/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/channels/rdpdr.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rdpdr_capabilities.h"

#include "devman.h"
#include "irp.h"

#include "rdpdr_main.h"

static void rdpdr_process_connect(rdpdrPlugin* rdpdr)
{
	int index;
	RDPDR_DEVICE* device;
	rdpSettings* settings;

	rdpdr->devman = devman_new(rdpdr);
	settings = (rdpSettings*) rdpdr->channelEntryPoints.pExtendedData;

	strncpy(rdpdr->computerName, settings->ComputerName, sizeof(rdpdr->computerName) - 1);

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = settings->DeviceArray[index];
		devman_load_device_service(rdpdr->devman, device);
	}
}

static void rdpdr_process_server_announce_request(rdpdrPlugin* rdpdr, wStream* s)
{
	Stream_Read_UINT16(s, rdpdr->versionMajor);
	Stream_Read_UINT16(s, rdpdr->versionMinor);
	Stream_Read_UINT32(s, rdpdr->clientID);
}

static void rdpdr_send_client_announce_reply(rdpdrPlugin* rdpdr)
{
	wStream* s;

	s = Stream_New(NULL, 12);

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_CLIENTID_CONFIRM);

	Stream_Write_UINT16(s, rdpdr->versionMajor);
	Stream_Write_UINT16(s, rdpdr->versionMinor);
	Stream_Write_UINT32(s, (UINT32) rdpdr->clientID);

	rdpdr_send(rdpdr, s);
}

static void rdpdr_send_client_name_request(rdpdrPlugin* rdpdr)
{
	wStream* s;
	WCHAR* computerNameW = NULL;
	size_t computerNameLenW;

	if (!rdpdr->computerName[0])
		gethostname(rdpdr->computerName, sizeof(rdpdr->computerName) - 1);

	computerNameLenW = ConvertToUnicode(CP_UTF8, 0, rdpdr->computerName, -1, &computerNameW, 0) * 2;

	s = Stream_New(NULL, 16 + computerNameLenW + 2);

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_CLIENT_NAME);

	Stream_Write_UINT32(s, 1); /* unicodeFlag, 0 for ASCII and 1 for Unicode */
	Stream_Write_UINT32(s, 0); /* codePage, must be set to zero */
	Stream_Write_UINT32(s, computerNameLenW + 2); /* computerNameLen, including null terminator */
	Stream_Write(s, computerNameW, computerNameLenW);
	Stream_Write_UINT16(s, 0); /* null terminator */

	free(computerNameW);

	rdpdr_send(rdpdr, s);
}

static void rdpdr_process_server_clientid_confirm(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;

	Stream_Read_UINT16(s, versionMajor);
	Stream_Read_UINT16(s, versionMinor);
	Stream_Read_UINT32(s, clientID);

	if (versionMajor != rdpdr->versionMajor || versionMinor != rdpdr->versionMinor)
	{
		rdpdr->versionMajor = versionMajor;
		rdpdr->versionMinor = versionMinor;
	}

	if (clientID != rdpdr->clientID)
	{
		rdpdr->clientID = clientID;
	}
}

static void rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr, BOOL userLoggedOn)
{
	int i;
	BYTE c;
	int pos;
	int index;
	wStream* s;
	UINT32 count;
	int data_len;
	int count_pos;
	DEVICE* device;
	int keyCount;
	ULONG_PTR* pKeys;

	s = Stream_New(NULL, 256);

	Stream_Write_UINT16(s, RDPDR_CTYP_CORE);
	Stream_Write_UINT16(s, PAKID_CORE_DEVICELIST_ANNOUNCE);

	count_pos = Stream_GetPosition(s);
	count = 0;

	Stream_Seek_UINT32(s); /* deviceCount */

	pKeys = NULL;
	keyCount = ListDictionary_GetKeys(rdpdr->devman->devices, &pKeys);

	for (index = 0; index < keyCount; index++)
	{
		device = (DEVICE*) ListDictionary_GetItemValue(rdpdr->devman->devices, (void*) pKeys[index]);

		/**
		 * 1. versionMinor 0x0005 doesn't send PAKID_CORE_USER_LOGGEDON
		 *    so all devices should be sent regardless of user_loggedon
		 * 2. smartcard devices should be always sent
		 * 3. other devices are sent only after user_loggedon
		 */

		if ((rdpdr->versionMinor == 0x0005) ||
			(device->type == RDPDR_DTYP_SMARTCARD) || userLoggedOn)
		{
			data_len = (device->data == NULL ? 0 : Stream_GetPosition(device->data));
			Stream_EnsureRemainingCapacity(s, 20 + data_len);

			Stream_Write_UINT32(s, device->type); /* deviceType */
			Stream_Write_UINT32(s, device->id); /* deviceID */
			strncpy((char*) Stream_Pointer(s), device->name, 8);

			for (i = 0; i < 8; i++)
			{
				Stream_Peek_UINT8(s, c);

				if (c > 0x7F)
					Stream_Write_UINT8(s, '_');
				else
					Stream_Seek_UINT8(s);
			}

			Stream_Write_UINT32(s, data_len);

			if (data_len > 0)
				Stream_Write(s, Stream_Buffer(device->data), data_len);

			count++;

			fprintf(stderr, "registered device #%d: %s (type=%d id=%d)\n",
				count, device->name, device->type, device->id);
		}
	}

	if (pKeys)
		free(pKeys);

	pos = Stream_GetPosition(s);
	Stream_SetPosition(s, count_pos);
	Stream_Write_UINT32(s, count);
	Stream_SetPosition(s, pos);
	Stream_SealLength(s);

	rdpdr_send(rdpdr, s);
}

static BOOL rdpdr_process_irp(rdpdrPlugin* rdpdr, wStream* s)
{
	IRP* irp;

	irp = irp_new(rdpdr->devman, s);

	if (!irp)
		return FALSE;

	IFCALL(irp->device->IRPRequest, irp->device, irp);

	return TRUE;
}

static void rdpdr_process_receive(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT16 component;
	UINT16 packetID;
	UINT32 deviceID;
	UINT32 status;

	Stream_Read_UINT16(s, component);
	Stream_Read_UINT16(s, packetID);

	if (component == RDPDR_CTYP_CORE)
	{
		switch (packetID)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				rdpdr_process_server_announce_request(rdpdr, s);
				rdpdr_send_client_announce_reply(rdpdr);
				rdpdr_send_client_name_request(rdpdr);
				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				rdpdr_process_capability_request(rdpdr, s);
				rdpdr_send_capability_response(rdpdr);
				break;

			case PAKID_CORE_CLIENTID_CONFIRM:
				rdpdr_process_server_clientid_confirm(rdpdr, s);
				rdpdr_send_device_list_announce_request(rdpdr, FALSE);
				break;

			case PAKID_CORE_USER_LOGGEDON:
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);
				break;

			case PAKID_CORE_DEVICE_REPLY:
				/* connect to a specific resource */
				Stream_Read_UINT32(s, deviceID);
				Stream_Read_UINT32(s, status);
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				if (rdpdr_process_irp(rdpdr, s))
					s = NULL;
				break;

			default:
				break;

		}
	}
	else if (component == RDPDR_CTYP_PRN)
	{

	}
	else
	{

	}

	Stream_Free(s, TRUE);
}


/****************************************************************************************/


static wListDictionary* g_InitHandles;
static wListDictionary* g_OpenHandles;

void rdpdr_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* rdpdr_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void rdpdr_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
}

void rdpdr_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* rdpdr_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void rdpdr_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
}

int rdpdr_send(rdpdrPlugin* rdpdr, wStream* s)
{
	UINT32 status = 0;
	rdpdrPlugin* plugin = (rdpdrPlugin*) rdpdr;

	if (!plugin)
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	else
		status = plugin->channelEntryPoints.pVirtualChannelWrite(plugin->OpenHandle,
			Stream_Buffer(s), Stream_GetPosition(s), s);

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		fprintf(stderr, "rdpdr_send: VirtualChannelWrite failed %d\n", status);
	}

	return status;
}

static void rdpdr_virtual_channel_event_data_received(rdpdrPlugin* rdpdr,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
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
		if (rdpdr->data_in != NULL)
			Stream_Free(rdpdr->data_in, TRUE);

		rdpdr->data_in = Stream_New(NULL, totalLength);
	}

	data_in = rdpdr->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			fprintf(stderr, "svc_plugin_process_received: read error\n");
		}

		rdpdr->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(rdpdr->MsgPipe->In, NULL, 0, (void*) data_in, NULL);
	}
}

static void rdpdr_virtual_channel_open_event(UINT32 openHandle, UINT32 event,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	rdpdrPlugin* rdpdr;

	rdpdr = (rdpdrPlugin*) rdpdr_get_open_handle_data(openHandle);

	if (!rdpdr)
	{
		fprintf(stderr, "rdpdr_virtual_channel_open_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			rdpdr_virtual_channel_event_data_received(rdpdr, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* rdpdr_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) arg;

	rdpdr_process_connect(rdpdr);

	while (1)
	{
		if (!MessageQueue_Wait(rdpdr->MsgPipe->In))
			break;

		if (MessageQueue_Peek(rdpdr->MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				rdpdr_process_receive(rdpdr, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void rdpdr_virtual_channel_event_connected(rdpdrPlugin* rdpdr, void* pData, UINT32 dataLength)
{
	UINT32 status;

	status = rdpdr->channelEntryPoints.pVirtualChannelOpen(rdpdr->InitHandle,
		&rdpdr->OpenHandle, rdpdr->channelDef.name, rdpdr_virtual_channel_open_event);

	rdpdr_add_open_handle_data(rdpdr->OpenHandle, rdpdr);

	if (status != CHANNEL_RC_OK)
	{
		fprintf(stderr, "rdpdr_virtual_channel_event_connected: open failed: status: %d\n", status);
		return;
	}

	rdpdr->MsgPipe = MessagePipe_New();

	rdpdr->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rdpdr_virtual_channel_client_thread, (void*) rdpdr, 0, NULL);
}

static void rdpdr_virtual_channel_event_terminated(rdpdrPlugin* rdpdr)
{
	MessagePipe_PostQuit(rdpdr->MsgPipe, 0);
	WaitForSingleObject(rdpdr->thread, INFINITE);

	MessagePipe_Free(rdpdr->MsgPipe);
	CloseHandle(rdpdr->thread);

	rdpdr->channelEntryPoints.pVirtualChannelClose(rdpdr->OpenHandle);

	if (rdpdr->data_in)
	{
		Stream_Free(rdpdr->data_in, TRUE);
		rdpdr->data_in = NULL;
	}

	if (rdpdr->devman)
	{
		devman_free(rdpdr->devman);
		rdpdr->devman = NULL;
	}

	rdpdr_remove_open_handle_data(rdpdr->OpenHandle);
	rdpdr_remove_init_handle_data(rdpdr->InitHandle);
}

static void rdpdr_virtual_channel_init_event(void* pInitHandle, UINT32 event, void* pData, UINT32 dataLength)
{
	rdpdrPlugin* rdpdr;

	rdpdr = (rdpdrPlugin*) rdpdr_get_init_handle_data(pInitHandle);

	if (!rdpdr)
	{
		fprintf(stderr, "rdpdr_virtual_channel_init_event: error no match\n");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			rdpdr_virtual_channel_event_connected(rdpdr, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			break;

		case CHANNEL_EVENT_TERMINATED:
			rdpdr_virtual_channel_event_terminated(rdpdr);
			break;
	}
}

/* rdpdr is always built-in */
#define VirtualChannelEntry	rdpdr_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	rdpdrPlugin* rdpdr;

	rdpdr = (rdpdrPlugin*) malloc(sizeof(rdpdrPlugin));
	ZeroMemory(rdpdr, sizeof(rdpdrPlugin));

	rdpdr->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP;

	strcpy(rdpdr->channelDef.name, "rdpdr");

	CopyMemory(&(rdpdr->channelEntryPoints), pEntryPoints, pEntryPoints->cbSize);

	rdpdr->channelEntryPoints.pVirtualChannelInit(&rdpdr->InitHandle,
		&rdpdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, rdpdr_virtual_channel_init_event);

	rdpdr_add_init_handle_data(rdpdr->InitHandle, (void*) rdpdr);

	return 1;
}
