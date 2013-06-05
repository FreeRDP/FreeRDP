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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "channels.h"

#include "init.h"

extern int g_open_handle_sequence;

extern void* g_pInterface;
extern CHANNEL_INIT_DATA g_ChannelInitData;

UINT32 FreeRDP_VirtualChannelInit(void** ppInitHandle, PCHANNEL_DEF pChannel,
	int channelCount, UINT32 versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	int index;
	void* pInterface;
	rdpChannel* channel;
	rdpChannels* channels;
	PCHANNEL_DEF pChannelDef;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;
	CHANNEL_CLIENT_DATA* pChannelClientData;

	if (!ppInitHandle)
	{
		DEBUG_CHANNELS("error bad init handle");
		return CHANNEL_RC_BAD_INIT_HANDLE;
	}

	channels = g_ChannelInitData.channels;
	pInterface = g_pInterface;

	pChannelInitData = &(channels->initDataList[channels->initDataCount]);
	*ppInitHandle = pChannelInitData;
	channels->initDataCount++;

	pChannelInitData->channels = channels;
	pChannelInitData->pInterface = pInterface;

	DEBUG_CHANNELS("enter");

	if (!channels->can_call_init)
	{
		DEBUG_CHANNELS("error not in entry");
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;
	}

	if (channels->openDataCount + channelCount >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANNELS("error too many channels");
		return CHANNEL_RC_TOO_MANY_CHANNELS;
	}

	if (!pChannel)
	{
		DEBUG_CHANNELS("error bad channel");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (channels->is_connected)
	{
		DEBUG_CHANNELS("error already connected");
		return CHANNEL_RC_ALREADY_CONNECTED;
	}

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
		DEBUG_CHANNELS("warning version");
	}

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];

		if (freerdp_channels_find_channel_open_data_by_name(channels, pChannelDef->name) != 0)
		{
			DEBUG_CHANNELS("error channel already used");
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	pChannelClientData = &channels->clientDataList[channels->clientDataCount];
	pChannelClientData->pChannelInitEventProc = pChannelInitEventProc;
	pChannelClientData->pInitHandle = *ppInitHandle;
	channels->clientDataCount++;

	for (index = 0; index < channelCount; index++)
	{
		pChannelDef = &pChannel[index];
		pChannelOpenData = &channels->openDataList[channels->openDataCount];

		pChannelOpenData->OpenHandle = g_open_handle_sequence++;

		pChannelOpenData->flags = 1; /* init */
		strncpy(pChannelOpenData->name, pChannelDef->name, CHANNEL_NAME_LEN);
		pChannelOpenData->options = pChannelDef->options;

		if (channels->settings->ChannelCount < CHANNEL_MAX_COUNT)
		{
			channel = channels->settings->ChannelDefArray + channels->settings->ChannelCount;
			strncpy(channel->Name, pChannelDef->name, 7);
			channel->options = pChannelDef->options;
			channels->settings->ChannelCount++;
		}
		else
		{
			DEBUG_CHANNELS("warning more than %d channels", CHANNEL_MAX_COUNT);
		}

		channels->openDataCount++;
	}

	return CHANNEL_RC_OK;
}
