/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
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

#ifndef FREERDP_LIB_CORE_CLIENT_H
#define FREERDP_LIB_CORE_CLIENT_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/svc.h>
#include <freerdp/peer.h>
#include <freerdp/addin.h>
#include <freerdp/api.h>

#include <freerdp/client/channels.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/channels/channels.h>

#define CHANNEL_MAX_COUNT 31

struct rdp_channel_client_data
{
	PVIRTUALCHANNELENTRY entry;
	PVIRTUALCHANNELENTRYEX entryEx;
	PCHANNEL_INIT_EVENT_FN pChannelInitEventProc;
	PCHANNEL_INIT_EVENT_EX_FN pChannelInitEventProcEx;
	void* pInitHandle;
	void* lpUserParam;
};
typedef struct rdp_channel_client_data CHANNEL_CLIENT_DATA;

struct rdp_channel_open_data
{
	char name[8];
	int OpenHandle;
	int options;
	int flags;
	void* pInterface;
	rdpChannels* channels;
	void* lpUserParam;
	PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc;
	PCHANNEL_OPEN_EVENT_EX_FN pChannelOpenEventProcEx;
};
typedef struct rdp_channel_open_data CHANNEL_OPEN_DATA;

struct _CHANNEL_OPEN_EVENT
{
	void* Data;
	UINT32 DataLength;
	void* UserData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
};
typedef struct _CHANNEL_OPEN_EVENT CHANNEL_OPEN_EVENT;

/**
 * pInitHandle: handle that identifies the client connection
 * Obtained by the client with VirtualChannelInit
 * Used by the client with VirtualChannelOpen
 */

struct rdp_channel_init_data
{
	rdpChannels* channels;
	void* pInterface;
};
typedef struct rdp_channel_init_data CHANNEL_INIT_DATA;

struct rdp_channels
{
	/* internal */

	int clientDataCount;
	CHANNEL_CLIENT_DATA clientDataList[CHANNEL_MAX_COUNT];

	int openDataCount;
	CHANNEL_OPEN_DATA openDataList[CHANNEL_MAX_COUNT];

	int initDataCount;
	CHANNEL_INIT_DATA initDataList[CHANNEL_MAX_COUNT];

	/* control for entry into MyVirtualChannelInit */
	int can_call_init;

	/* true once freerdp_channels_post_connect is called */
	BOOL connected;

	/* used for locating the channels for a given instance */
	freerdp* instance;

	wMessageQueue* queue;

	DrdynvcClientContext* drdynvc;
	CRITICAL_SECTION channelsLock;

	wHashTable* openHandles;
};

FREERDP_LOCAL rdpChannels* freerdp_channels_new(freerdp* instance);
FREERDP_LOCAL UINT freerdp_channels_disconnect(rdpChannels* channels,
        freerdp* instance);
FREERDP_LOCAL void freerdp_channels_close(rdpChannels* channels,
        freerdp* instance);
FREERDP_LOCAL void freerdp_channels_free(rdpChannels* channels);
FREERDP_LOCAL UINT freerdp_channels_pre_connect(rdpChannels* channels,
        freerdp* instance);
FREERDP_LOCAL UINT freerdp_channels_post_connect(rdpChannels* channels,
        freerdp* instance);
#endif /* FREERDP_LIB_CORE_CLIENT_H */
