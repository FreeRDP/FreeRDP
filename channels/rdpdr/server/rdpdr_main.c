/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>

#include "rdpdr_main.h"

static UINT32 g_ClientId = 0;

static int rdpdr_server_send_announce_request(RdpdrServerContext* context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;

	printf("RdpdrServerSendAnnounceRequest\n");

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_ANNOUNCE;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

	Stream_Write_UINT16(s, context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId); /* ClientId (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int rdpdr_server_receive_announce_response(RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;

	Stream_Read_UINT16(s, VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Read_UINT32(s, ClientId); /* ClientId (4 bytes) */

	printf("Client Announce Response: VersionMajor: 0x%04X VersionMinor: 0x%04X ClientId: 0x%04X\n",
			VersionMajor, VersionMinor, ClientId);

	context->priv->ClientId = ClientId;

	return 0;
}

static int rdpdr_server_receive_client_name_request(RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	UINT32 UnicodeFlag;
	UINT32 ComputerNameLen;

	Stream_Read_UINT32(s, UnicodeFlag); /* UnicodeFlag (4 bytes) */
	Stream_Seek_UINT32(s); /* CodePage (4 bytes), MUST be set to zero */
	Stream_Read_UINT32(s, ComputerNameLen); /* ComputerNameLen (4 bytes) */

	/**
	 * Caution: ComputerNameLen is given *bytes*,
	 * not in characters, including the NULL terminator!
	 */

	if (context->priv->ClientComputerName)
	{
		free(context->priv->ClientComputerName);
		context->priv->ClientComputerName = NULL;
	}

	if (UnicodeFlag)
	{
		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s),
			-1, &(context->priv->ClientComputerName), 0, NULL, NULL);
	}
	else
	{
		context->priv->ClientComputerName = _strdup((char*) Stream_Pointer(s));
	}

	Stream_Seek(s, ComputerNameLen);

	printf("ClientComputerName: %s\n", context->priv->ClientComputerName);

	return 0;
}

static int rdpdr_server_read_capability_set_header(wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	Stream_Read_UINT16(s, header->CapabilityType); /* CapabilityType (2 bytes) */
	Stream_Read_UINT16(s, header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Read_UINT32(s, header->Version); /* Version (4 bytes) */
	return 0;
}

static int rdpdr_server_write_capability_set_header(wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	Stream_Write_UINT16(s, header->CapabilityType); /* CapabilityType (2 bytes) */
	Stream_Write_UINT16(s, header->CapabilityLength); /* CapabilityLength (2 bytes) */
	Stream_Write_UINT32(s, header->Version); /* Version (4 bytes) */
	return 0;
}

static int rdpdr_server_read_general_capability_set(RdpdrServerContext* context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	UINT32 ioCode1;
	UINT32 extraFlags1;
	UINT32 extendedPdu;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	UINT32 SpecialTypeDeviceCap;

	Stream_Seek_UINT32(s); /* osType (4 bytes), ignored on receipt */
	Stream_Seek_UINT32(s); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Read_UINT16(s, VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Read_UINT16(s, VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Read_UINT32(s, ioCode1); /* ioCode1 (4 bytes) */
	Stream_Seek_UINT32(s); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Read_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Read_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Seek_UINT32(s); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Read_UINT32(s, SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */

	context->priv->UserLoggedOnPdu = (extendedPdu & RDPDR_USER_LOGGEDON_PDU) ? TRUE : FALSE;

	return 0;
}

static int rdpdr_server_write_general_capability_set(RdpdrServerContext* context, wStream* s)
{
	UINT32 ioCode1;
	UINT32 extendedPdu;
	UINT32 extraFlags1;
	UINT32 SpecialTypeDeviceCap;
	RDPDR_CAPABILITY_HEADER header;

	header.CapabilityType = CAP_GENERAL_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH + 36;
	header.Version = GENERAL_CAPABILITY_VERSION_02;

	ioCode1 = 0;
	ioCode1 |= RDPDR_IRP_MJ_CREATE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLEANUP; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_CLOSE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_READ; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_WRITE; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_FLUSH_BUFFERS; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SHUTDOWN; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DEVICE_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_VOLUME_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_VOLUME_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_SET_INFORMATION; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_DIRECTORY_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_LOCK_CONTROL; /* always set */
	ioCode1 |= RDPDR_IRP_MJ_QUERY_SECURITY; /* optional */
	ioCode1 |= RDPDR_IRP_MJ_SET_SECURITY; /* optional */

	extendedPdu = 0;
	extendedPdu |= RDPDR_CLIENT_DISPLAY_NAME_PDU; /* always set */
	extendedPdu |= RDPDR_DEVICE_REMOVE_PDUS; /* optional */

	if (context->priv->UserLoggedOnPdu)
		extendedPdu |= RDPDR_USER_LOGGEDON_PDU; /* optional */

	extraFlags1 = 0;
	extraFlags1 |= ENABLE_ASYNCIO; /* optional */

	SpecialTypeDeviceCap = 0;

	Stream_EnsureRemainingCapacity(s, header.CapabilityLength);
	rdpdr_server_write_capability_set_header(s, &header);

	Stream_Write_UINT32(s, 0); /* osType (4 bytes), ignored on receipt */
	Stream_Write_UINT32(s, 0); /* osVersion (4 bytes), unused and must be set to zero */
	Stream_Write_UINT16(s, context->priv->VersionMajor); /* protocolMajorVersion (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* protocolMinorVersion (2 bytes) */
	Stream_Write_UINT32(s, ioCode1); /* ioCode1 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* ioCode2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, extendedPdu); /* extendedPdu (4 bytes) */
	Stream_Write_UINT32(s, extraFlags1); /* extraFlags1 (4 bytes) */
	Stream_Write_UINT32(s, 0); /* extraFlags2 (4 bytes), must be set to zero, reserved for future use */
	Stream_Write_UINT32(s, SpecialTypeDeviceCap); /* SpecialTypeDeviceCap (4 bytes) */

	return 0;
}

static int rdpdr_server_read_printer_capability_set(RdpdrServerContext* context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return 0;
}

static int rdpdr_server_write_printer_capability_set(RdpdrServerContext* context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;

	header.CapabilityType = CAP_PRINTER_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = PRINT_CAPABILITY_VERSION_01;

	Stream_EnsureRemainingCapacity(s, header.CapabilityLength);
	rdpdr_server_write_capability_set_header(s, &header);

	return 0;
}

static int rdpdr_server_read_port_capability_set(RdpdrServerContext* context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return 0;
}

static int rdpdr_server_write_port_capability_set(RdpdrServerContext* context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;

	header.CapabilityType = CAP_PORT_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = PORT_CAPABILITY_VERSION_01;

	Stream_EnsureRemainingCapacity(s, header.CapabilityLength);
	rdpdr_server_write_capability_set_header(s, &header);

	return 0;
}

static int rdpdr_server_read_drive_capability_set(RdpdrServerContext* context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return 0;
}

static int rdpdr_server_write_drive_capability_set(RdpdrServerContext* context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;

	header.CapabilityType = CAP_DRIVE_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = DRIVE_CAPABILITY_VERSION_02;

	Stream_EnsureRemainingCapacity(s, header.CapabilityLength);
	rdpdr_server_write_capability_set_header(s, &header);

	return 0;
}

static int rdpdr_server_read_smartcard_capability_set(RdpdrServerContext* context, wStream* s, RDPDR_CAPABILITY_HEADER* header)
{
	return 0;
}

static int rdpdr_server_write_smartcard_capability_set(RdpdrServerContext* context, wStream* s)
{
	RDPDR_CAPABILITY_HEADER header;

	header.CapabilityType = CAP_SMARTCARD_TYPE;
	header.CapabilityLength = RDPDR_CAPABILITY_HEADER_LENGTH;
	header.Version = SMARTCARD_CAPABILITY_VERSION_01;

	Stream_EnsureRemainingCapacity(s, header.CapabilityLength);
	rdpdr_server_write_capability_set_header(s, &header);

	return 0;
}

static int rdpdr_server_send_core_capability_request(RdpdrServerContext* context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;
	UINT16 numCapabilities;

	printf("RdpdrServerSendCoreCapabilityRequest\n");

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_SERVER_CAPABILITY;

	numCapabilities = 5;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 512);

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

	Stream_Write_UINT16(s, numCapabilities); /* numCapabilities (2 bytes) */
	Stream_Write_UINT16(s, 0); /* Padding (2 bytes) */

	rdpdr_server_write_general_capability_set(context, s);
	rdpdr_server_write_printer_capability_set(context, s);
	rdpdr_server_write_port_capability_set(context, s);
	rdpdr_server_write_drive_capability_set(context, s);
	rdpdr_server_write_smartcard_capability_set(context, s);

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int rdpdr_server_receive_core_capability_response(RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	int i;
	UINT16 numCapabilities;
	RDPDR_CAPABILITY_HEADER capabilityHeader;

	Stream_Read_UINT16(s, numCapabilities); /* numCapabilities (2 bytes) */
	Stream_Seek_UINT16(s); /* Padding (2 bytes) */

	for (i = 0; i < numCapabilities; i++)
	{
		rdpdr_server_read_capability_set_header(s, &capabilityHeader);

		switch (capabilityHeader.CapabilityType)
		{
			case CAP_GENERAL_TYPE:
				rdpdr_server_read_general_capability_set(context, s, &capabilityHeader);
				break;

			case CAP_PRINTER_TYPE:
				rdpdr_server_read_printer_capability_set(context, s, &capabilityHeader);
				break;

			case CAP_PORT_TYPE:
				rdpdr_server_read_port_capability_set(context, s, &capabilityHeader);
				break;

			case CAP_DRIVE_TYPE:
				rdpdr_server_read_drive_capability_set(context, s, &capabilityHeader);
				break;

			case CAP_SMARTCARD_TYPE:
				rdpdr_server_read_smartcard_capability_set(context, s, &capabilityHeader);
				break;

			default:
				printf("Unknown capabilityType %d\n", capabilityHeader.CapabilityType);
				Stream_Seek(s, capabilityHeader.CapabilityLength - RDPDR_CAPABILITY_HEADER_LENGTH);
				break;
		}
	}

	return 0;
}

static int rdpdr_server_send_client_id_confirm(RdpdrServerContext* context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;

	printf("RdpdrServerSendClientIdConfirm\n");

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_CLIENTID_CONFIRM;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH + 8);

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

	Stream_Write_UINT16(s, context->priv->VersionMajor); /* VersionMajor (2 bytes) */
	Stream_Write_UINT16(s, context->priv->VersionMinor); /* VersionMinor (2 bytes) */
	Stream_Write_UINT32(s, context->priv->ClientId); /* ClientId (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int rdpdr_server_receive_device_list_announce_request(RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	int i;
	UINT32 DeviceCount;
	UINT32 DeviceType;
	UINT32 DeviceId;
	char PreferredDosName[9];
	UINT32 DeviceDataLength;

	PreferredDosName[8] = 0;

	Stream_Read_UINT32(s, DeviceCount); /* DeviceCount (4 bytes) */

	printf("%s: DeviceCount: %d\n", __FUNCTION__, DeviceCount);

	for (i = 0; i < DeviceCount; i++)
	{
		Stream_Read_UINT32(s, DeviceType); /* DeviceType (4 bytes) */
		Stream_Read_UINT32(s, DeviceId); /* DeviceId (4 bytes) */
		Stream_Read(s, PreferredDosName, 8); /* PreferredDosName (8 bytes) */
		Stream_Read_UINT32(s, DeviceDataLength); /* DeviceDataLength (4 bytes) */

		printf("Device %d Name: %s Id: 0x%04X DataLength: %d\n",
				i, PreferredDosName, DeviceId, DeviceDataLength);

		switch (DeviceId)
		{
			case RDPDR_DTYP_FILESYSTEM:
				break;

			case RDPDR_DTYP_PRINT:
				break;

			case RDPDR_DTYP_SERIAL:
				break;

			case RDPDR_DTYP_PARALLEL:
				break;

			case RDPDR_DTYP_SMARTCARD:
				break;

			default:
				break;
		}

		Stream_Seek(s, DeviceDataLength);
	}

	return 0;
}

static int rdpdr_server_send_user_logged_on(RdpdrServerContext* context)
{
	wStream* s;
	BOOL status;
	RDPDR_HEADER header;

	printf("%s\n", __FUNCTION__);

	header.Component = RDPDR_CTYP_CORE;
	header.PacketId = PAKID_CORE_USER_LOGGEDON;

	s = Stream_New(NULL, RDPDR_HEADER_LENGTH);

	Stream_Write_UINT16(s, header.Component); /* Component (2 bytes) */
	Stream_Write_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int rdpdr_server_receive_pdu(RdpdrServerContext* context, wStream* s, RDPDR_HEADER* header)
{
	printf("RdpdrServerReceivePdu: Component: 0x%04X PacketId: 0x%04X\n",
			header->Component, header->PacketId);

	winpr_HexDump(Stream_Buffer(s), Stream_Length(s));

	if (header->Component == RDPDR_CTYP_CORE)
	{
		switch (header->PacketId)
		{
			case PAKID_CORE_CLIENTID_CONFIRM:
				rdpdr_server_receive_announce_response(context, s, header);
				break;

			case PAKID_CORE_CLIENT_NAME:
				rdpdr_server_receive_client_name_request(context, s, header);
				rdpdr_server_send_core_capability_request(context);
				break;

			case PAKID_CORE_CLIENT_CAPABILITY:
				rdpdr_server_receive_core_capability_response(context, s, header);
				rdpdr_server_send_client_id_confirm(context);

				if (context->priv->UserLoggedOnPdu)
					rdpdr_server_send_user_logged_on(context);
				break;

			case PAKID_CORE_DEVICELIST_ANNOUNCE:
				rdpdr_server_receive_device_list_announce_request(context, s, header);
				break;

			case PAKID_CORE_DEVICE_REPLY:
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				break;

			case PAKID_CORE_DEVICE_IOCOMPLETION:
				break;

			case PAKID_CORE_DEVICELIST_REMOVE:
				break;

			default:
				break;
		}
	}
	else if (header->Component == RDPDR_CTYP_PRN)
	{
		switch (header->PacketId)
		{
			case PAKID_PRN_CACHE_DATA:
				break;

			case PAKID_PRN_USING_XPS:
				break;

			default:
				break;
		}
	}
	else
	{
		printf("Unknown RDPDR_HEADER.Component: 0x%04X\n", header->Component);
		return -1;
	}

	return 0;
}

static void* rdpdr_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	int position;
	HANDLE events[8];
	RDPDR_HEADER header;
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	RdpdrServerContext* context;

	context = (RdpdrServerContext*) arg;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	s = Stream_New(NULL, 4096);

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle, &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	rdpdr_server_send_announce_request(context);

	while (1)
	{
		BytesReturned = 0;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(context->priv->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0, (PCHAR) Stream_Pointer(s),
				Stream_Capacity(s) - Stream_GetPosition(s), &BytesReturned))
		{
			if (BytesReturned)
				Stream_Seek(s, BytesReturned);
		}
		else
		{
			Stream_EnsureRemainingCapacity(s, BytesReturned);
		}

		if (Stream_GetPosition(s) >= RDPDR_HEADER_LENGTH)
		{
			position = Stream_GetPosition(s);
			Stream_SetPosition(s, 0);

			Stream_Read_UINT16(s, header.Component); /* Component (2 bytes) */
			Stream_Read_UINT16(s, header.PacketId); /* PacketId (2 bytes) */

			Stream_SetPosition(s, position);

			Stream_SealLength(s);
			Stream_SetPosition(s, RDPDR_HEADER_LENGTH);

			rdpdr_server_receive_pdu(context, s, &header);
			Stream_SetPosition(s, 0);
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int rdpdr_server_start(RdpdrServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelManagerOpenEx(context->vcm, "rdpdr", 0);

	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rdpdr_server_thread, (void*) context, 0, NULL);

	return 0;
}

static int rdpdr_server_stop(RdpdrServerContext* context)
{
	SetEvent(context->priv->StopEvent);

	WaitForSingleObject(context->priv->Thread, INFINITE);
	CloseHandle(context->priv->Thread);

	return 0;
}

RdpdrServerContext* rdpdr_server_context_new(WTSVirtualChannelManager* vcm)
{
	RdpdrServerContext* context;

	context = (RdpdrServerContext*) malloc(sizeof(RdpdrServerContext));

	if (context)
	{
		ZeroMemory(context, sizeof(RdpdrServerContext));

		context->vcm = vcm;

		context->Start = rdpdr_server_start;
		context->Stop = rdpdr_server_stop;

		context->priv = (RdpdrServerPrivate*) malloc(sizeof(RdpdrServerPrivate));

		if (context->priv)
		{
			ZeroMemory(context->priv, sizeof(RdpdrServerPrivate));

			context->priv->VersionMajor = RDPDR_VERSION_MAJOR;
			context->priv->VersionMinor = RDPDR_VERSION_MINOR_RDP6X;
			context->priv->ClientId = g_ClientId++;

			context->priv->UserLoggedOnPdu = TRUE;
		}
	}

	return context;
}

void rdpdr_server_context_free(RdpdrServerContext* context)
{
	if (context)
	{
		if (context->priv)
		{
			free(context->priv);
		}

		free(context);
	}
}
