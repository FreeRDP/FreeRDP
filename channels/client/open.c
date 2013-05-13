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

#include "open.h"

UINT32 FreeRDP_VirtualChannelOpen(void* pInitHandle, UINT32* pOpenHandle,
	char* pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	void* pInterface;
	rdpChannels* channels;
	CHANNEL_INIT_DATA* pChannelInitData;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	DEBUG_CHANNELS("enter");

	pChannelInitData = (CHANNEL_INIT_DATA*) pInitHandle;
	channels = pChannelInitData->channels;
	pInterface = pChannelInitData->pInterface;

	if (!pOpenHandle)
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!pChannelOpenEventProc)
	{
		DEBUG_CHANNELS("error bad proc");
		return CHANNEL_RC_BAD_PROC;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	pChannelOpenData = freerdp_channels_find_channel_open_data_by_name(channels, pChannelName);

	if (!pChannelOpenData)
	{
		DEBUG_CHANNELS("error channel name");
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;
	}

	if (pChannelOpenData->flags == 2)
	{
		DEBUG_CHANNELS("error channel already open");
		return CHANNEL_RC_ALREADY_OPEN;
	}

	pChannelOpenData->flags = 2; /* open */
	pChannelOpenData->pInterface = pInterface;
	pChannelOpenData->pChannelOpenEventProc = pChannelOpenEventProc;
	*pOpenHandle = pChannelOpenData->OpenHandle;

	return CHANNEL_RC_OK;
}

UINT32 FreeRDP_VirtualChannelClose(UINT32 openHandle)
{
	int index;
	rdpChannels* channels;
	CHANNEL_OPEN_DATA* pChannelOpenData;

	DEBUG_CHANNELS("enter");

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((channels == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	pChannelOpenData = &channels->openDataList[index];

	if (pChannelOpenData->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	pChannelOpenData->flags = 0;

	return CHANNEL_RC_OK;
}
