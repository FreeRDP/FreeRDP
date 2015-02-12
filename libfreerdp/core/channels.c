/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Virtual Channels
 *
 * Copyright 2011 Vic Lee
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
#include <winpr/wtsapi.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/log.h>
#include <freerdp/svc.h>
#include <freerdp/peer.h>
#include <freerdp/addin.h>

#include <freerdp/client/channels.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/channels/channels.h>

#include "rdp.h"
#include "client.h"
#include "server.h"
#include "channels.h"

#define TAG FREERDP_TAG("core.channels")

BOOL freerdp_channel_send(rdpRdp* rdp, UINT16 channelId, BYTE* data, int size)
{
	DWORD i;
	int left;
	wStream* s;
	UINT32 flags;
	int chunkSize;
	rdpMcs* mcs = rdp->mcs;
	rdpMcsChannel* channel = NULL;

	for (i = 0; i < mcs->channelCount; i++)
	{
		if (mcs->channels[i].ChannelId == channelId)
		{
			channel = &mcs->channels[i];
			break;
		}
	}

	if (!channel)
	{
		WLog_ERR(TAG,  "freerdp_channel_send: unknown channelId %d", channelId);
		return FALSE;
	}

	flags = CHANNEL_FLAG_FIRST;
	left = size;

	while (left > 0)
	{
		s = rdp_send_stream_init(rdp);

		if (left > (int) rdp->settings->VirtualChannelChunkSize)
		{
			chunkSize = rdp->settings->VirtualChannelChunkSize;
		}
		else
		{
			chunkSize = left;
			flags |= CHANNEL_FLAG_LAST;
		}

		if ((channel->options & CHANNEL_OPTION_SHOW_PROTOCOL))
		{
			flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
		}

		Stream_Write_UINT32(s, size);
		Stream_Write_UINT32(s, flags);
		Stream_EnsureCapacity(s, chunkSize);
		Stream_Write(s, data, chunkSize);

		rdp_send(rdp, s, channelId);

		data += chunkSize;
		left -= chunkSize;
		flags = 0;
	}

	return TRUE;
}

BOOL freerdp_channel_process(freerdp* instance, wStream* s, UINT16 channelId)
{
	UINT32 length;
	UINT32 flags;
	int chunkLength;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, length);
	Stream_Read_UINT32(s, flags);
	chunkLength = Stream_GetRemainingLength(s);

	IFCALL(instance->ReceiveChannelData, instance,
			channelId, Stream_Pointer(s), chunkLength, flags, length);

	return TRUE;
}

BOOL freerdp_channel_peer_process(freerdp_peer* client, wStream* s, UINT16 channelId)
{
	UINT32 length;
	UINT32 flags;
	int chunkLength;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, length);
	Stream_Read_UINT32(s, flags);
	chunkLength = Stream_GetRemainingLength(s);

	if (client->VirtualChannelRead)
	{
		UINT32 index;
		BOOL found = FALSE;
		HANDLE hChannel = 0;
		rdpContext* context = client->context;
		rdpMcs* mcs = context->rdp->mcs;
		rdpMcsChannel* mcsChannel = NULL;

		for (index = 0; index < mcs->channelCount; index++)
		{
			mcsChannel = &(mcs->channels[index]);

			if (mcsChannel->ChannelId == channelId)
			{
				hChannel = (HANDLE) mcsChannel->handle;
				found = TRUE;
				break;
			}
		}

		if (!found)
			return FALSE;

		client->VirtualChannelRead(client, hChannel, Stream_Pointer(s), Stream_GetRemainingLength(s));
	}
	else if (client->ReceiveChannelData)
	{
		client->ReceiveChannelData(client, channelId, Stream_Pointer(s), chunkLength, flags, length);
	}

	return TRUE;
}

static WtsApiFunctionTable FreeRDP_WtsApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	FreeRDP_WTSStopRemoteControlSession, /* StopRemoteControlSession */
	FreeRDP_WTSStartRemoteControlSessionW, /* StartRemoteControlSessionW */
	FreeRDP_WTSStartRemoteControlSessionA, /* StartRemoteControlSessionA */
	FreeRDP_WTSConnectSessionW, /* ConnectSessionW */
	FreeRDP_WTSConnectSessionA, /* ConnectSessionA */
	FreeRDP_WTSEnumerateServersW, /* EnumerateServersW */
	FreeRDP_WTSEnumerateServersA, /* EnumerateServersA */
	FreeRDP_WTSOpenServerW, /* OpenServerW */
	FreeRDP_WTSOpenServerA, /* OpenServerA */
	FreeRDP_WTSOpenServerExW, /* OpenServerExW */
	FreeRDP_WTSOpenServerExA, /* OpenServerExA */
	FreeRDP_WTSCloseServer, /* CloseServer */
	FreeRDP_WTSEnumerateSessionsW, /* EnumerateSessionsW */
	FreeRDP_WTSEnumerateSessionsA, /* EnumerateSessionsA */
	FreeRDP_WTSEnumerateSessionsExW, /* EnumerateSessionsExW */
	FreeRDP_WTSEnumerateSessionsExA, /* EnumerateSessionsExA */
	FreeRDP_WTSEnumerateProcessesW, /* EnumerateProcessesW */
	FreeRDP_WTSEnumerateProcessesA, /* EnumerateProcessesA */
	FreeRDP_WTSTerminateProcess, /* TerminateProcess */
	FreeRDP_WTSQuerySessionInformationW, /* QuerySessionInformationW */
	FreeRDP_WTSQuerySessionInformationA, /* QuerySessionInformationA */
	FreeRDP_WTSQueryUserConfigW, /* QueryUserConfigW */
	FreeRDP_WTSQueryUserConfigA, /* QueryUserConfigA */
	FreeRDP_WTSSetUserConfigW, /* SetUserConfigW */
	FreeRDP_WTSSetUserConfigA, /* SetUserConfigA */
	FreeRDP_WTSSendMessageW, /* SendMessageW */
	FreeRDP_WTSSendMessageA, /* SendMessageA */
	FreeRDP_WTSDisconnectSession, /* DisconnectSession */
	FreeRDP_WTSLogoffSession, /* LogoffSession */
	FreeRDP_WTSShutdownSystem, /* ShutdownSystem */
	FreeRDP_WTSWaitSystemEvent, /* WaitSystemEvent */
	FreeRDP_WTSVirtualChannelOpen, /* VirtualChannelOpen */
	FreeRDP_WTSVirtualChannelOpenEx, /* VirtualChannelOpenEx */
	FreeRDP_WTSVirtualChannelClose, /* VirtualChannelClose */
	FreeRDP_WTSVirtualChannelRead, /* VirtualChannelRead */
	FreeRDP_WTSVirtualChannelWrite, /* VirtualChannelWrite */
	FreeRDP_WTSVirtualChannelPurgeInput, /* VirtualChannelPurgeInput */
	FreeRDP_WTSVirtualChannelPurgeOutput, /* VirtualChannelPurgeOutput */
	FreeRDP_WTSVirtualChannelQuery, /* VirtualChannelQuery */
	FreeRDP_WTSFreeMemory, /* FreeMemory */
	FreeRDP_WTSRegisterSessionNotification, /* RegisterSessionNotification */
	FreeRDP_WTSUnRegisterSessionNotification, /* UnRegisterSessionNotification */
	FreeRDP_WTSRegisterSessionNotificationEx, /* RegisterSessionNotificationEx */
	FreeRDP_WTSUnRegisterSessionNotificationEx, /* UnRegisterSessionNotificationEx */
	FreeRDP_WTSQueryUserToken, /* QueryUserToken */
	FreeRDP_WTSFreeMemoryExW, /* FreeMemoryExW */
	FreeRDP_WTSFreeMemoryExA, /* FreeMemoryExA */
	FreeRDP_WTSEnumerateProcessesExW, /* EnumerateProcessesExW */
	FreeRDP_WTSEnumerateProcessesExA, /* EnumerateProcessesExA */
	FreeRDP_WTSEnumerateListenersW, /* EnumerateListenersW */
	FreeRDP_WTSEnumerateListenersA, /* EnumerateListenersA */
	FreeRDP_WTSQueryListenerConfigW, /* QueryListenerConfigW */
	FreeRDP_WTSQueryListenerConfigA, /* QueryListenerConfigA */
	FreeRDP_WTSCreateListenerW, /* CreateListenerW */
	FreeRDP_WTSCreateListenerA, /* CreateListenerA */
	FreeRDP_WTSSetListenerSecurityW, /* SetListenerSecurityW */
	FreeRDP_WTSSetListenerSecurityA, /* SetListenerSecurityA */
	FreeRDP_WTSGetListenerSecurityW, /* GetListenerSecurityW */
	FreeRDP_WTSGetListenerSecurityA, /* GetListenerSecurityA */
	FreeRDP_WTSEnableChildSessions, /* EnableChildSessions */
	FreeRDP_WTSIsChildSessionsEnabled, /* IsChildSessionsEnabled */
	FreeRDP_WTSGetChildSessionId, /* GetChildSessionId */
	FreeRDP_WTSGetActiveConsoleSessionId, /* GetActiveConsoleSessionId */
	FreeRDP_WTSLogonUser,
	FreeRDP_WTSLogoffUser
};

PWtsApiFunctionTable FreeRDP_InitWtsApi(void)
{
	return &FreeRDP_WtsApiFunctionTable;
}
