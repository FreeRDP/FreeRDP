/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __FREERDP_SVC_H
#define __FREERDP_SVC_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define CHANNEL_EXPORT_FUNC_NAME "VirtualChannelEntry"

#define CHANNEL_NAME_LEN 7

struct _CHANNEL_DEF
{
	char name[CHANNEL_NAME_LEN + 1];
	uint32 options;
};
typedef struct _CHANNEL_DEF CHANNEL_DEF;
typedef CHANNEL_DEF* PCHANNEL_DEF;
typedef CHANNEL_DEF** PPCHANNEL_DEF;

typedef void (FREERDP_CC * PCHANNEL_INIT_EVENT_FN)(void* pInitHandle,
	uint32 event, void* pData, uint32 dataLength);


typedef void (FREERDP_CC * PCHANNEL_OPEN_EVENT_FN)(uint32 openHandle,
	uint32 event, void* pData, uint32 dataLength,
	uint32 totalLength, uint32 dataFlags);

#define CHANNEL_RC_OK                             0
#define CHANNEL_RC_ALREADY_INITIALIZED            1
#define CHANNEL_RC_NOT_INITIALIZED                2
#define CHANNEL_RC_ALREADY_CONNECTED              3
#define CHANNEL_RC_NOT_CONNECTED                  4
#define CHANNEL_RC_TOO_MANY_CHANNELS              5
#define CHANNEL_RC_BAD_CHANNEL                    6
#define CHANNEL_RC_BAD_CHANNEL_HANDLE             7
#define CHANNEL_RC_NO_BUFFER                      8
#define CHANNEL_RC_BAD_INIT_HANDLE                9
#define CHANNEL_RC_NOT_OPEN                      10
#define CHANNEL_RC_BAD_PROC                      11
#define CHANNEL_RC_NO_MEMORY                     12
#define CHANNEL_RC_UNKNOWN_CHANNEL_NAME          13
#define CHANNEL_RC_ALREADY_OPEN                  14
#define CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY    15
#define CHANNEL_RC_NULL_DATA                     16
#define CHANNEL_RC_ZERO_LENGTH                   17

#define VIRTUAL_CHANNEL_VERSION_WIN2000         1

typedef uint32 (FREERDP_CC * PVIRTUALCHANNELINIT)(void** ppInitHandle,
	PCHANNEL_DEF pChannel, int channelCount, uint32 versionRequested,
	PCHANNEL_INIT_EVENT_FN pChannelInitEventProc);
typedef uint32 (FREERDP_CC * PVIRTUALCHANNELOPEN)(void* pInitHandle,
	uint32* pOpenHandle, char* pChannelName,
	PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc);
typedef uint32 (FREERDP_CC * PVIRTUALCHANNELCLOSE)(uint32 openHandle);

typedef uint32 (FREERDP_CC * PVIRTUALCHANNELWRITE)(uint32 openHandle,
	void* pData, uint32 dataLength, void* pUserData);

typedef uint32 (FREERDP_CC * PVIRTUALCHANNELEVENTPUSH)(uint32 openHandle,
	RDP_EVENT* event);

struct _CHANNEL_ENTRY_POINTS
{
	uint32 cbSize;
	uint32 protocolVersion;
	PVIRTUALCHANNELINIT  pVirtualChannelInit;
	PVIRTUALCHANNELOPEN  pVirtualChannelOpen;
	PVIRTUALCHANNELCLOSE pVirtualChannelClose;
	PVIRTUALCHANNELWRITE pVirtualChannelWrite;
};
typedef struct _CHANNEL_ENTRY_POINTS CHANNEL_ENTRY_POINTS;
typedef CHANNEL_ENTRY_POINTS* PCHANNEL_ENTRY_POINTS;

typedef int (FREERDP_CC * PVIRTUALCHANNELENTRY)(PCHANNEL_ENTRY_POINTS pEntryPoints);

struct _CHANNEL_ENTRY_POINTS_EX
{
	uint32 cbSize;
	uint32 protocolVersion;
	PVIRTUALCHANNELINIT  pVirtualChannelInit;
	PVIRTUALCHANNELOPEN  pVirtualChannelOpen;
	PVIRTUALCHANNELCLOSE pVirtualChannelClose;
	PVIRTUALCHANNELWRITE pVirtualChannelWrite;
	void* pExtendedData; /* extended data field to pass initial parameters */
	PVIRTUALCHANNELEVENTPUSH pVirtualChannelEventPush;
};
typedef struct _CHANNEL_ENTRY_POINTS_EX CHANNEL_ENTRY_POINTS_EX;
typedef CHANNEL_ENTRY_POINTS_EX* PCHANNEL_ENTRY_POINTS_EX;

#endif
