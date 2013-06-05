/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
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

#ifndef FREERDP_PRIVATE_CLIENT_CHANNELS
#define FREERDP_PRIVATE_CLIENT_CHANNELS

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/collections.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/channels/channels.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CHANNEL_MAX_COUNT 30

struct rdp_channel_client_data
{
	PVIRTUALCHANNELENTRY entry;
	PCHANNEL_INIT_EVENT_FN pChannelInitEventProc;
	void* pInitHandle;
};
typedef struct rdp_channel_client_data CHANNEL_CLIENT_DATA;

struct rdp_channel_open_data
{
	char name[8];
	int OpenHandle;
	int options;
	int flags;
	void* pInterface;
	PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc;
};
typedef struct rdp_channel_open_data CHANNEL_OPEN_DATA;

struct _CHANNEL_OPEN_EVENT
{
	void* Data;
	UINT32 DataLength;
	void* UserData;
	int Index;
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
	rdpSettings* settings;

	/* true once freerdp_channels_post_connect is called */
	int is_connected;

	/* used for locating the channels for a given instance */
	freerdp* instance;

	wMessagePipe* MsgPipe;

	DrdynvcClientContext* drdynvc;
};

#ifdef WITH_DEBUG_CHANNELS
#define DEBUG_CHANNELS(fmt, ...) DEBUG_CLASS(CHANNELS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CHANNELS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

rdpChannels* freerdp_channels_find_by_open_handle(int open_handle, int* pindex);
CHANNEL_OPEN_DATA* freerdp_channels_find_channel_open_data_by_name(rdpChannels* channels, const char* channel_name);

#endif /* FREERDP_PRIVATE_CLIENT_CHANNELS */
