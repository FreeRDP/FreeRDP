/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Extension
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H
#define FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H

#include <winpr/collections.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/settings.h>
#include <freerdp/server/rdpdr.h>

struct s_rdpdr_server_private
{
	HANDLE Thread;
	HANDLE StopEvent;
	void* ChannelHandle;

	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	char* ClientComputerName;

	BOOL UserLoggedOnPdu;

	wListDictionary* IrpList;
	UINT32 NextCompletionId;
};

#define RDPDR_HEADER_LENGTH 4

typedef struct
{
	UINT16 Component;
	UINT16 PacketId;
} RDPDR_HEADER;

#define RDPDR_CAPABILITY_HEADER_LENGTH 8

typedef struct
{
	UINT16 CapabilityType;
	UINT16 CapabilityLength;
	UINT32 Version;
} RDPDR_CAPABILITY_HEADER;

typedef struct S_RDPDR_IRP
{
	UINT32 CompletionId;
	UINT32 DeviceId;
	UINT32 FileId;
	char PathName[256];
	char ExtraBuffer[256];
	void* CallbackData;
	UINT(*Callback)
	(RdpdrServerContext* context, wStream* s, struct S_RDPDR_IRP* irp, UINT32 deviceId,
	 UINT32 completionId, UINT32 ioStatus);
} RDPDR_IRP;

#endif /* FREERDP_CHANNEL_RDPDR_SERVER_MAIN_H */
