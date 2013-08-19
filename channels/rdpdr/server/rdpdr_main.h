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

#ifndef FREERDP_CHANNEL_SERVER_RDPDR_MAIN_H
#define FREERDP_CHANNEL_SERVER_RDPDR_MAIN_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/settings.h>
#include <freerdp/server/rdpdr.h>

struct _rdpdr_server_private
{
	HANDLE Thread;
	HANDLE StopEvent;
	void* ChannelHandle;

	UINT32 ClientId;
	UINT16 VersionMajor;
	UINT16 VersionMinor;
	char* ClientComputerName;

	BOOL UserLoggedOnPdu;
};

#define RDPDR_HEADER_LENGTH		4

struct _RDPDR_HEADER
{
	UINT16 Component;
	UINT16 PacketId;
};
typedef struct _RDPDR_HEADER RDPDR_HEADER;

#define RDPDR_VERSION_MAJOR		0x0001

#define RDPDR_VERSION_MINOR_RDP50	0x0002
#define RDPDR_VERSION_MINOR_RDP51	0x0005
#define RDPDR_VERSION_MINOR_RDP52	0x000A
#define RDPDR_VERSION_MINOR_RDP6X	0x000C

#define RDPDR_CAPABILITY_HEADER_LENGTH	8

struct _RDPDR_CAPABILITY_HEADER
{
	UINT16 CapabilityType;
	UINT16 CapabilityLength;
	UINT32 Version;
};
typedef struct _RDPDR_CAPABILITY_HEADER RDPDR_CAPABILITY_HEADER;

#endif /* FREERDP_CHANNEL_SERVER_RDPDR_MAIN_H */
