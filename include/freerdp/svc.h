/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Virtual Channel Interface
 *
 * Copyright 2009-2011 Jay Sorg
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

/**
 * MS compatible SVC plugin interface
 * reference:
 * http://msdn.microsoft.com/en-us/library/aa383580.aspx
 */

#ifndef FREERDP_SVC_H
#define FREERDP_SVC_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define CHANNEL_EXPORT_FUNC_NAME "VirtualChannelEntry"

#define CHANNEL_NAME_LEN 7

struct _CHANNEL_DEF
{
	char name[8];
	UINT32 options;
};

typedef struct _CHANNEL_DEF CHANNEL_DEF;
typedef CHANNEL_DEF* PCHANNEL_DEF;
typedef CHANNEL_DEF** PPCHANNEL_DEF;

typedef void (FREERDP_CC * PCHANNEL_INIT_EVENT_FN)(void* pInitHandle, UINT32 event, void* pData, UINT32 dataLength);

typedef void (FREERDP_CC * PCHANNEL_OPEN_EVENT_FN)(UINT32 openHandle, UINT32 event,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags);

#define CHANNEL_RC_OK					0
#define CHANNEL_RC_ALREADY_INITIALIZED			1
#define CHANNEL_RC_NOT_INITIALIZED			2
#define CHANNEL_RC_ALREADY_CONNECTED			3
#define CHANNEL_RC_NOT_CONNECTED			4
#define CHANNEL_RC_TOO_MANY_CHANNELS			5
#define CHANNEL_RC_BAD_CHANNEL				6
#define CHANNEL_RC_BAD_CHANNEL_HANDLE			7
#define CHANNEL_RC_NO_BUFFER				8
#define CHANNEL_RC_BAD_INIT_HANDLE			9
#define CHANNEL_RC_NOT_OPEN				10
#define CHANNEL_RC_BAD_PROC				11
#define CHANNEL_RC_NO_MEMORY				12
#define CHANNEL_RC_UNKNOWN_CHANNEL_NAME			13
#define CHANNEL_RC_ALREADY_OPEN				14
#define CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY		15
#define CHANNEL_RC_NULL_DATA				16
#define CHANNEL_RC_ZERO_LENGTH				17

#define VIRTUAL_CHANNEL_VERSION_WIN2000			1

typedef UINT32 (FREERDP_CC * PVIRTUALCHANNELINIT)(void** ppInitHandle, PCHANNEL_DEF pChannel,
		int channelCount, UINT32 versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc);

typedef UINT32 (FREERDP_CC * PVIRTUALCHANNELOPEN)(void* pInitHandle, UINT32* pOpenHandle,
		char* pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc);

typedef UINT32 (FREERDP_CC * PVIRTUALCHANNELCLOSE)(UINT32 openHandle);

typedef UINT32 (FREERDP_CC * PVIRTUALCHANNELWRITE)(UINT32 openHandle, void* pData, UINT32 dataLength, void* pUserData);

typedef UINT32 (FREERDP_CC * PVIRTUALCHANNELEVENTPUSH)(UINT32 openHandle, wMessage* event);

struct _CHANNEL_ENTRY_POINTS
{
	UINT32 cbSize;
	UINT32 protocolVersion;
	PVIRTUALCHANNELINIT  pVirtualChannelInit;
	PVIRTUALCHANNELOPEN  pVirtualChannelOpen;
	PVIRTUALCHANNELCLOSE pVirtualChannelClose;
	PVIRTUALCHANNELWRITE pVirtualChannelWrite;
};
typedef struct _CHANNEL_ENTRY_POINTS CHANNEL_ENTRY_POINTS;
typedef CHANNEL_ENTRY_POINTS* PCHANNEL_ENTRY_POINTS;

typedef int (FREERDP_CC * PVIRTUALCHANNELENTRY)(PCHANNEL_ENTRY_POINTS pEntryPoints);

#define FREERDP_CHANNEL_MAGIC_NUMBER	0x46524450

struct _CHANNEL_ENTRY_POINTS_EX
{
	UINT32 cbSize;
	UINT32 protocolVersion;
	PVIRTUALCHANNELINIT  pVirtualChannelInit;
	PVIRTUALCHANNELOPEN  pVirtualChannelOpen;
	PVIRTUALCHANNELCLOSE pVirtualChannelClose;
	PVIRTUALCHANNELWRITE pVirtualChannelWrite;

	/* Extended Fields */
	UINT32 MagicNumber; /* identifies FreeRDP */
	void* pExtendedData; /* extended initial data */
	void** ppInterface; /* channel callback interface */
	PVIRTUALCHANNELEVENTPUSH pVirtualChannelEventPush;
};
typedef struct _CHANNEL_ENTRY_POINTS_EX CHANNEL_ENTRY_POINTS_EX;
typedef CHANNEL_ENTRY_POINTS_EX* PCHANNEL_ENTRY_POINTS_EX;

#endif /* FREERDP_SVC_H */
