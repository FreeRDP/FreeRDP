/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/constants.h>
#include <freerdp/server/channels.h>

#include "rdp.h"

#include "server.h"

#define TAG FREERDP_TAG("core.server")
#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_DVC(...) \
	do                 \
	{                  \
	} while (0)
#endif

struct _wtsChannelMessage
{
	UINT16 channelId;
	UINT16 reserved;
	UINT32 length;
	UINT32 offset;
};
typedef struct _wtsChannelMessage wtsChannelMessage;

static DWORD g_SessionId = 1;
static wHashTable* g_ServerHandles = NULL;

static rdpPeerChannel* wts_get_dvc_channel_by_id(WTSVirtualChannelManager* vcm, UINT32 ChannelId)
{
	int index;
	int count;
	BOOL found = FALSE;
	rdpPeerChannel* channel = NULL;

	WINPR_ASSERT(vcm);

	ArrayList_Lock(vcm->dynamicVirtualChannels);
	count = ArrayList_Count(vcm->dynamicVirtualChannels);

	for (index = 0; index < count; index++)
	{
		channel = (rdpPeerChannel*)ArrayList_GetItem(vcm->dynamicVirtualChannels, index);
		WINPR_ASSERT(channel);
		if (channel->channelId == ChannelId)
		{
			found = TRUE;
			break;
		}
	}

	ArrayList_Unlock(vcm->dynamicVirtualChannels);
	return found ? channel : NULL;
}

static BOOL wts_queue_receive_data(rdpPeerChannel* channel, const BYTE* Buffer, UINT32 Length)
{
	BYTE* buffer;
	wtsChannelMessage* messageCtx;

	WINPR_ASSERT(channel);
	messageCtx = (wtsChannelMessage*)malloc(sizeof(wtsChannelMessage) + Length);

	if (!messageCtx)
		return FALSE;

	messageCtx->channelId = channel->channelId;
	messageCtx->length = Length;
	messageCtx->offset = 0;
	buffer = (BYTE*)(messageCtx + 1);
	CopyMemory(buffer, Buffer, Length);
	return MessageQueue_Post(channel->queue, messageCtx, 0, NULL, NULL);
}

static BOOL wts_queue_send_item(rdpPeerChannel* channel, BYTE* Buffer, UINT32 Length)
{
	BYTE* buffer;
	UINT32 length;
	UINT16 channelId;
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	buffer = Buffer;
	length = Length;
	channelId = channel->channelId;
	return MessageQueue_Post(channel->vcm->queue, (void*)(UINT_PTR)channelId, 0, (void*)buffer,
	                         (void*)(UINT_PTR)length);
}

static int wts_read_variable_uint(wStream* s, int cbLen, UINT32* val)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(val);
	switch (cbLen)
	{
		case 0:
			if (Stream_GetRemainingLength(s) < 1)
				return 0;

			Stream_Read_UINT8(s, *val);
			return 1;

		case 1:
			if (Stream_GetRemainingLength(s) < 2)
				return 0;

			Stream_Read_UINT16(s, *val);
			return 2;

		default:
			if (Stream_GetRemainingLength(s) < 4)
				return 0;

			Stream_Read_UINT32(s, *val);
			return 4;
	}
}

static BOOL wts_read_drdynvc_capabilities_response(rdpPeerChannel* channel, UINT32 length)
{
	UINT16 Version;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	if (length < 3)
		return FALSE;

	Stream_Seek_UINT8(channel->receiveData); /* Pad (1 byte) */
	Stream_Read_UINT16(channel->receiveData, Version);
	DEBUG_DVC("Version: %" PRIu16 "", Version);
	channel->vcm->drdynvc_state = DRDYNVC_STATE_READY;
	return TRUE;
}

static BOOL wts_read_drdynvc_create_response(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	UINT32 CreationStatus;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);
	if (length < 4)
		return FALSE;

	Stream_Read_UINT32(s, CreationStatus);

	if ((INT32)CreationStatus < 0)
	{
		DEBUG_DVC("ChannelId %" PRIu32 " creation failed (%" PRId32 ")", channel->channelId,
		          (INT32)CreationStatus);
		channel->dvc_open_state = DVC_OPEN_STATE_FAILED;
	}
	else
	{
		DEBUG_DVC("ChannelId %" PRIu32 " creation succeeded", channel->channelId);
		channel->dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	}

	return TRUE;
}

static BOOL wts_read_drdynvc_data_first(rdpPeerChannel* channel, wStream* s, int cbLen,
                                        UINT32 length)
{
	int value;
	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);
	value = wts_read_variable_uint(s, cbLen, &channel->dvc_total_length);

	if (value == 0)
		return FALSE;

	length -= value;

	if (length > channel->dvc_total_length)
		return FALSE;

	Stream_SetPosition(channel->receiveData, 0);

	if (!Stream_EnsureRemainingCapacity(channel->receiveData, channel->dvc_total_length))
		return FALSE;

	Stream_Write(channel->receiveData, Stream_Pointer(s), length);
	return TRUE;
}

static BOOL wts_read_drdynvc_data(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	BOOL ret = FALSE;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);
	if (channel->dvc_total_length > 0)
	{
		if (Stream_GetPosition(channel->receiveData) + length > channel->dvc_total_length)
		{
			channel->dvc_total_length = 0;
			WLog_ERR(TAG, "incorrect fragment data, discarded.");
			return FALSE;
		}

		Stream_Write(channel->receiveData, Stream_Pointer(s), length);

		if (Stream_GetPosition(channel->receiveData) >= channel->dvc_total_length)
		{
			ret = wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData),
			                             channel->dvc_total_length);
			channel->dvc_total_length = 0;
		}
		else
			ret = TRUE;
	}
	else
	{
		ret = wts_queue_receive_data(channel, Stream_Pointer(s), length);
	}

	return ret;
}

static void wts_read_drdynvc_close_response(rdpPeerChannel* channel)
{
	WINPR_ASSERT(channel);
	DEBUG_DVC("ChannelId %" PRIu32 " close response", channel->channelId);
	channel->dvc_open_state = DVC_OPEN_STATE_CLOSED;
	MessageQueue_PostQuit(channel->queue, 0);
}

static BOOL wts_read_drdynvc_pdu(rdpPeerChannel* channel)
{
	UINT32 length;
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	UINT32 ChannelId;
	rdpPeerChannel* dvc;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);

	length = Stream_GetPosition(channel->receiveData);

	if (length < 1)
		return FALSE;

	Stream_SetPosition(channel->receiveData, 0);
	Stream_Read_UINT8(channel->receiveData, value);
	length--;
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;

	if (Cmd == CAPABILITY_REQUEST_PDU)
	{
		return wts_read_drdynvc_capabilities_response(channel, length);
	}
	else if (channel->vcm->drdynvc_state == DRDYNVC_STATE_READY)
	{
		value = wts_read_variable_uint(channel->receiveData, cbChId, &ChannelId);

		if (value == 0)
			return FALSE;

		length -= value;
		DEBUG_DVC("Cmd %d ChannelId %" PRIu32 " length %" PRIu32 "", Cmd, ChannelId, length);
		dvc = wts_get_dvc_channel_by_id(channel->vcm, ChannelId);

		if (dvc)
		{
			switch (Cmd)
			{
				case CREATE_REQUEST_PDU:
					return wts_read_drdynvc_create_response(dvc, channel->receiveData, length);

				case DATA_FIRST_PDU:
					return wts_read_drdynvc_data_first(dvc, channel->receiveData, Sp, length);

				case DATA_PDU:
					return wts_read_drdynvc_data(dvc, channel->receiveData, length);

				case CLOSE_REQUEST_PDU:
					wts_read_drdynvc_close_response(dvc);
					break;

				default:
					WLog_ERR(TAG, "Cmd %d not recognized.", Cmd);
					break;
			}
		}
		else
		{
			DEBUG_DVC("ChannelId %" PRIu32 " not exists.", ChannelId);
		}
	}
	else
	{
		WLog_ERR(TAG, "received Cmd %d but channel is not ready.", Cmd);
	}

	return TRUE;
}

static int wts_write_variable_uint(wStream* s, UINT32 val)
{
	int cb;

	WINPR_ASSERT(s);
	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(s, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(s, val);
	}
	else
	{
		cb = 2;
		Stream_Write_UINT32(s, val);
	}

	return cb;
}

static void wts_write_drdynvc_header(wStream* s, BYTE Cmd, UINT32 ChannelId)
{
	BYTE* bm;
	int cbChId;

	WINPR_ASSERT(s);

	Stream_GetPointer(s, bm);
	Stream_Seek_UINT8(s);
	cbChId = wts_write_variable_uint(s, ChannelId);
	*bm = ((Cmd & 0x0F) << 4) | cbChId;
}

static BOOL wts_write_drdynvc_create_request(wStream* s, UINT32 ChannelId, const char* ChannelName)
{
	size_t len;

	WINPR_ASSERT(s);
	WINPR_ASSERT(ChannelName);

	wts_write_drdynvc_header(s, CREATE_REQUEST_PDU, ChannelId);
	len = strlen(ChannelName) + 1;

	if (!Stream_EnsureRemainingCapacity(s, len))
		return FALSE;

	Stream_Write(s, ChannelName, len);
	return TRUE;
}

static BOOL WTSProcessChannelData(rdpPeerChannel* channel, UINT16 channelId, const BYTE* data,
                                  size_t s, UINT32 flags, size_t t)
{
	BOOL ret = TRUE;
	const size_t size = (size_t)s;
	const size_t totalSize = (size_t)t;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_UNUSED(channelId);

	if (flags & CHANNEL_FLAG_FIRST)
	{
		Stream_SetPosition(channel->receiveData, 0);
	}

	if (!Stream_EnsureRemainingCapacity(channel->receiveData, size))
		return FALSE;

	Stream_Write(channel->receiveData, data, size);

	if (flags & CHANNEL_FLAG_LAST)
	{
		if (Stream_GetPosition(channel->receiveData) != totalSize)
		{
			WLog_ERR(TAG, "read error");
		}

		if (channel == channel->vcm->drdynvc_channel)
		{
			ret = wts_read_drdynvc_pdu(channel);
		}
		else
		{
			ret = wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData),
			                             Stream_GetPosition(channel->receiveData));
		}

		Stream_SetPosition(channel->receiveData, 0);
	}

	return ret;
}

static BOOL WTSReceiveChannelData(freerdp_peer* client, UINT16 channelId, const BYTE* data,
                                  size_t size, UINT32 flags, size_t totalSize)
{
	UINT32 i;
	rdpMcs* mcs;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);

	for (i = 0; i < mcs->channelCount; i++)
	{
		rdpMcsChannel* cur = &mcs->channels[i];
		if (cur->ChannelId == channelId)
		{
			rdpPeerChannel* channel = (rdpPeerChannel*)cur->handle;

			if (channel)
				return WTSProcessChannelData(channel, channelId, data, size, flags, totalSize);
		}
	}

	WLog_WARN(TAG, "[%s] unknown channelId %" PRIu16 " ignored", __FUNCTION__, channelId);

	return TRUE;
}

void WTSVirtualChannelManagerGetFileDescriptor(HANDLE hServer, void** fds, int* fds_count)
{
	void* fd;
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(fds);
	WINPR_ASSERT(fds_count);

	fd = GetEventWaitObject(MessageQueue_Event(vcm->queue));

	if (fd)
	{
		fds[*fds_count] = fd;
		(*fds_count)++;
	}

#if 0

	if (vcm->drdynvc_channel)
	{
		fd = GetEventWaitObject(vcm->drdynvc_channel->receiveEvent);

		if (fd)
		{
			fds[*fds_count] = fd;
			(*fds_count)++;
		}
	}

#endif
}

static BOOL WTSVirtualChannelManagerOpen(WTSVirtualChannelManager* vcm)
{
	if (!vcm)
		return FALSE;

	WINPR_ASSERT(vcm->client);

	if ((vcm->drdynvc_state == DRDYNVC_STATE_NONE) && vcm->client->activated)
	{
		rdpPeerChannel* channel;
		UINT32 dynvc_caps;

		/* Initialize drdynvc channel once and only once. */
		vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;
		channel =
		    (rdpPeerChannel*)WTSVirtualChannelOpen((HANDLE)vcm, WTS_CURRENT_SESSION, "drdynvc");

		if (channel)
		{
			ULONG written;
			vcm->drdynvc_channel = channel;
			dynvc_caps = 0x00010050; /* DYNVC_CAPS_VERSION1 (4 bytes) */

			if (!WTSVirtualChannelWrite(channel, (PCHAR)&dynvc_caps, sizeof(dynvc_caps), &written))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL WTSVirtualChannelManagerCheckFileDescriptorEx(HANDLE hServer, BOOL autoOpen)
{
	wMessage message;
	BOOL status = TRUE;
	WTSVirtualChannelManager* vcm;

	if (!hServer || hServer == INVALID_HANDLE_VALUE)
		return FALSE;

	vcm = (WTSVirtualChannelManager*)hServer;

	if (autoOpen)
	{
		if (!WTSVirtualChannelManagerOpen(vcm))
			return FALSE;
	}

	while (MessageQueue_Peek(vcm->queue, &message, TRUE))
	{
		BYTE* buffer;
		UINT32 length;
		UINT16 channelId;
		channelId = (UINT16)(UINT_PTR)message.context;
		buffer = (BYTE*)message.wParam;
		length = (UINT32)(UINT_PTR)message.lParam;

		if (!vcm->client->SendChannelData(vcm->client, channelId, buffer, length))
		{
			status = FALSE;
		}

		free(buffer);

		if (!status)
			break;
	}

	return status;
}

BOOL WTSVirtualChannelManagerCheckFileDescriptor(HANDLE hServer)
{
	return WTSVirtualChannelManagerCheckFileDescriptorEx(hServer, TRUE);
}

HANDLE WTSVirtualChannelManagerGetEventHandle(HANDLE hServer)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	WINPR_ASSERT(vcm);
	return MessageQueue_Event(vcm->queue);
}

static rdpMcsChannel* wts_get_joined_channel_by_name(rdpMcs* mcs, const char* channel_name)
{
	UINT32 index;

	if (!mcs || !channel_name || !strnlen(channel_name, CHANNEL_NAME_LEN + 1))
		return NULL;

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined)
		{
			if (_strnicmp(mchannel->Name, channel_name, CHANNEL_NAME_LEN + 1) == 0)
				return mchannel;
		}
	}

	return NULL;
}

static rdpMcsChannel* wts_get_joined_channel_by_id(rdpMcs* mcs, const UINT16 channel_id)
{
	UINT32 index;

	if (!mcs || !channel_id)
		return NULL;

	WINPR_ASSERT(mcs->channels);
	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined)
		{
			if (mchannel->ChannelId == channel_id)
				return &mcs->channels[index];
		}
	}

	return NULL;
}

BOOL WTSIsChannelJoinedByName(freerdp_peer* client, const char* channel_name)
{
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	return wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name) == NULL ? FALSE
	                                                                                       : TRUE;
}

BOOL WTSIsChannelJoinedById(freerdp_peer* client, const UINT16 channel_id)
{
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	return wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id) == NULL ? FALSE
	                                                                                   : TRUE;
}

BOOL WTSVirtualChannelManagerIsChannelJoined(HANDLE hServer, const char* name)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm || !vcm->rdp)
		return FALSE;

	return wts_get_joined_channel_by_name(vcm->rdp->mcs, name) == NULL ? FALSE : TRUE;
}

BYTE WTSVirtualChannelManagerGetDrdynvcState(HANDLE hServer)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	WINPR_ASSERT(vcm);
	return vcm->drdynvc_state;
}

UINT16 WTSChannelGetId(freerdp_peer* client, const char* channel_name)
{
	rdpMcsChannel* channel;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return 0;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return 0;

	return channel->ChannelId;
}

BOOL WTSChannelSetHandleByName(freerdp_peer* client, const char* channel_name, void* handle)
{
	rdpMcsChannel* channel;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return FALSE;

	channel->handle = handle;
	return TRUE;
}

BOOL WTSChannelSetHandleById(freerdp_peer* client, const UINT16 channel_id, void* handle)
{
	rdpMcsChannel* channel;

	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return FALSE;

	channel->handle = handle;
	return TRUE;
}

void* WTSChannelGetHandleByName(freerdp_peer* client, const char* channel_name)
{
	rdpMcsChannel* channel;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return NULL;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return NULL;

	return channel->handle;
}

void* WTSChannelGetHandleById(freerdp_peer* client, const UINT16 channel_id)
{
	rdpMcsChannel* channel;

	if (!client || !client->context || !client->context->rdp)
		return NULL;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return NULL;

	return channel->handle;
}

const char* WTSChannelGetName(freerdp_peer* client, UINT16 channel_id)
{
	rdpMcsChannel* channel;

	if (!client || !client->context || !client->context->rdp)
		return NULL;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return NULL;

	return (const char*)channel->Name;
}

char** WTSGetAcceptedChannelNames(freerdp_peer* client, size_t* count)
{
	rdpMcs* mcs;
	char** names;
	UINT32 index;

	if (!client || !client->context || !count)
		return NULL;

	WINPR_ASSERT(client->context->rdp);
	mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);
	*count = mcs->channelCount;

	names = (char**)calloc(mcs->channelCount, sizeof(char*));
	if (!names)
		return NULL;

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		names[index] = mchannel->Name;
	}

	return names;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId,
                                                  BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId,
                                                  BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExW(LPWSTR pTargetServerName, ULONG TargetLogonId,
                                                    BYTE HotkeyVk, USHORT HotkeyModifiers,
                                                    DWORD flags)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExA(LPSTR pTargetServerName, ULONG TargetLogonId,
                                                    BYTE HotkeyVk, USHORT HotkeyModifiers,
                                                    DWORD flags)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStopRemoteControlSession(ULONG LogonId)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword,
                                       BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword,
                                       BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version,
                                         PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version,
                                         PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	return FALSE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerW(LPWSTR pServerName)
{
	return INVALID_HANDLE_VALUE;
}

static void wts_virtual_channel_manager_free_message(void* obj)
{
	wMessage* msg = (wMessage*)obj;

	if (msg)
	{
		BYTE* buffer = (BYTE*)msg->wParam;

		if (buffer)
			free(buffer);
	}
}

HANDLE WINAPI FreeRDP_WTSOpenServerA(LPSTR pServerName)
{
	rdpContext* context;
	freerdp_peer* client;
	WTSVirtualChannelManager* vcm;
	HANDLE hServer = INVALID_HANDLE_VALUE;
	wObject queueCallbacks = { 0 };

	context = (rdpContext*)pServerName;

	if (!context)
		return INVALID_HANDLE_VALUE;

	client = context->peer;

	if (!client)
	{
		SetLastError(ERROR_INVALID_DATA);
		return INVALID_HANDLE_VALUE;
	}

	vcm = (WTSVirtualChannelManager*)calloc(1, sizeof(WTSVirtualChannelManager));

	if (!vcm)
		goto error_vcm_alloc;

	vcm->client = client;
	vcm->rdp = context->rdp;
	vcm->SessionId = g_SessionId++;

	if (!g_ServerHandles)
	{
		g_ServerHandles = HashTable_New(TRUE);

		if (!g_ServerHandles)
			goto error_free;
	}

	if (!HashTable_Insert(g_ServerHandles, (void*)(UINT_PTR)vcm->SessionId, (void*)vcm))
		goto error_free;

	queueCallbacks.fnObjectFree = wts_virtual_channel_manager_free_message;
	vcm->queue = MessageQueue_New(&queueCallbacks);

	if (!vcm->queue)
		goto error_queue;

	vcm->dvc_channel_id_seq = 0;
	vcm->dynamicVirtualChannels = ArrayList_New(TRUE);

	if (!vcm->dynamicVirtualChannels)
		goto error_dynamicVirtualChannels;

	client->ReceiveChannelData = WTSReceiveChannelData;
	hServer = (HANDLE)vcm;
	return hServer;
error_dynamicVirtualChannels:
	MessageQueue_Free(vcm->queue);
error_queue:
	HashTable_Remove(g_ServerHandles, (void*)(UINT_PTR)vcm->SessionId);
error_free:
	free(vcm);
error_vcm_alloc:
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerExW(LPWSTR pServerName)
{
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerExA(LPSTR pServerName)
{
	return FreeRDP_WTSOpenServerA(pServerName);
}

VOID WINAPI FreeRDP_WTSCloseServer(HANDLE hServer)
{
	int index;
	int count;
	rdpPeerChannel* channel;
	WTSVirtualChannelManager* vcm;
	vcm = (WTSVirtualChannelManager*)hServer;

	if (vcm)
	{
		HashTable_Remove(g_ServerHandles, (void*)(UINT_PTR)vcm->SessionId);
		ArrayList_Lock(vcm->dynamicVirtualChannels);
		count = ArrayList_Count(vcm->dynamicVirtualChannels);

		for (index = 0; index < count; index++)
		{
			channel = (rdpPeerChannel*)ArrayList_GetItem(vcm->dynamicVirtualChannels, index);
			WTSVirtualChannelClose(channel);
		}

		ArrayList_Unlock(vcm->dynamicVirtualChannels);
		ArrayList_Free(vcm->dynamicVirtualChannels);

		if (vcm->drdynvc_channel)
		{
			WTSVirtualChannelClose(vcm->drdynvc_channel);
			vcm->drdynvc_channel = NULL;
		}

		MessageQueue_Free(vcm->queue);
		free(vcm);
	}
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version,
                                          PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version,
                                          PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter,
                                            PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter,
                                            PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version,
                                           PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version,
                                           PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId,
                                                WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer,
                                                DWORD* pBytesReturned)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId,
                                                WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer,
                                                DWORD* pBytesReturned)
{
	DWORD BytesReturned;
	WTSVirtualChannelManager* vcm;
	vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm)
		return FALSE;

	if (WTSInfoClass == WTSSessionId)
	{
		ULONG* pBuffer;
		BytesReturned = sizeof(ULONG);
		pBuffer = (ULONG*)malloc(sizeof(BytesReturned));

		if (!pBuffer)
		{
			SetLastError(E_OUTOFMEMORY);
			return FALSE;
		}

		*pBuffer = vcm->SessionId;
		*ppBuffer = (LPSTR)pBuffer;
		*pBytesReturned = BytesReturned;
		return TRUE;
	}

	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
                                        WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer,
                                        DWORD* pBytesReturned)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName,
                                        WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer,
                                        DWORD* pBytesReturned)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
                                      WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer,
                                      DWORD DataLength)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName,
                                      WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer,
                                      DWORD DataLength)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle,
                                    DWORD TitleLength, LPWSTR pMessage, DWORD MessageLength,
                                    DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle,
                                    DWORD TitleLength, LPSTR pMessage, DWORD MessageLength,
                                    DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	return FALSE;
}

static void peer_channel_queue_free_message(void* obj)
{
	wMessage* msg = (wMessage*)obj;
	if (!msg)
		return;

	free(msg->context);
}

static void channel_free(rdpPeerChannel* channel)
{
	if (!channel)
		return;

	MessageQueue_Free(channel->queue);
	Stream_Free(channel->receiveData, TRUE);
	free(channel);
}

static rdpPeerChannel* channel_new(WTSVirtualChannelManager* vcm, freerdp_peer* client,
                                   UINT32 ChannelId, UINT16 index, UINT16 type, size_t chunkSize)
{
	wObject queueCallbacks = { 0 };
	rdpPeerChannel* channel = (rdpPeerChannel*)calloc(1, sizeof(rdpPeerChannel));

	WINPR_ASSERT(vcm);
	WINPR_ASSERT(client);

	if (!channel)
		goto fail;

	channel->vcm = vcm;
	channel->client = client;
	channel->channelId = ChannelId;
	channel->index = index;
	channel->channelType = type;
	channel->receiveData = Stream_New(NULL, chunkSize);

	if (!channel->receiveData)
		goto fail;

	queueCallbacks.fnObjectFree = peer_channel_queue_free_message;
	channel->queue = MessageQueue_New(&queueCallbacks);

	if (!channel->queue)
		goto fail;

	return channel;
fail:
	channel_free(channel);
	return NULL;
}

HANDLE WINAPI FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	size_t length;
	UINT32 index;
	rdpMcs* mcs;
	rdpMcsChannel* joined_channel = NULL;
	freerdp_peer* client;
	rdpPeerChannel* channel;
	WTSVirtualChannelManager* vcm;
	HANDLE hChannelHandle = NULL;
	vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm)
	{
		SetLastError(ERROR_INVALID_DATA);
		return NULL;
	}

	client = vcm->client;
	mcs = client->context->rdp->mcs;
	length = strlen(pVirtualName);

	if (length > 8)
	{
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined && (strncmp(mchannel->Name, pVirtualName, length) == 0))
		{
			joined_channel = mchannel;
			break;
		}
	}

	if (!joined_channel)
	{
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	channel = (rdpPeerChannel*)joined_channel->handle;

	if (!channel)
	{
		channel = channel_new(vcm, client, joined_channel->ChannelId, index,
		                      RDP_PEER_CHANNEL_TYPE_SVC, client->settings->VirtualChannelChunkSize);

		if (!channel)
			goto fail;

		joined_channel->handle = channel;
	}

	hChannelHandle = (HANDLE)channel;
	return hChannelHandle;
fail:
	channel_free(channel);
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return NULL;
}

HANDLE WINAPI FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	UINT32 index;
	wStream* s = NULL;
	rdpMcs* mcs;
	BOOL joined = FALSE;
	freerdp_peer* client;
	rdpPeerChannel* channel = NULL;
	ULONG written;
	WTSVirtualChannelManager* vcm = NULL;

	if (SessionId == WTS_CURRENT_SESSION)
		return NULL;

	vcm = (WTSVirtualChannelManager*)HashTable_GetItemValue(g_ServerHandles,
	                                                        (void*)(UINT_PTR)SessionId);

	if (!vcm)
		return NULL;

	if (!(flags & WTS_CHANNEL_OPTION_DYNAMIC))
	{
		return FreeRDP_WTSVirtualChannelOpen((HANDLE)vcm, SessionId, pVirtualName);
	}

	client = vcm->client;
	mcs = client->context->rdp->mcs;

	for (index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined && (strncmp(mchannel->Name, "drdynvc", CHANNEL_NAME_LEN + 1) == 0))
		{
			joined = TRUE;
			break;
		}
	}

	if (!joined)
	{
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	if (!vcm->drdynvc_channel || (vcm->drdynvc_state != DRDYNVC_STATE_READY))
	{
		SetLastError(ERROR_NOT_READY);
		return NULL;
	}

	channel = channel_new(vcm, client, 0, 0, RDP_PEER_CHANNEL_TYPE_DVC,
	                      client->settings->VirtualChannelChunkSize);

	if (!channel)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	channel->channelId = InterlockedIncrement(&vcm->dvc_channel_id_seq);

	if (!ArrayList_Append(vcm->dynamicVirtualChannels, channel))
		goto fail;

	s = Stream_New(NULL, 64);

	if (!s)
		goto fail;

	if (!wts_write_drdynvc_create_request(s, channel->channelId, pVirtualName))
		goto fail;

	if (!WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR)Stream_Buffer(s),
	                            Stream_GetPosition(s), &written))
		goto fail;

	Stream_Free(s, TRUE);
	return channel;
fail:
	Stream_Free(s, TRUE);
	if (vcm)
		ArrayList_Remove(vcm->dynamicVirtualChannels, channel);
	channel_free(channel);
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return NULL;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	wStream* s;
	rdpMcs* mcs;

	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;
	BOOL ret = TRUE;

	if (channel)
	{
		WTSVirtualChannelManager* vcm = channel->vcm;

		WINPR_ASSERT(vcm);
		WINPR_ASSERT(vcm->client);
		WINPR_ASSERT(vcm->client->context);
		WINPR_ASSERT(vcm->client->context->rdp);
		mcs = vcm->client->context->rdp->mcs;

		if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
		{
			if (channel->index < mcs->channelCount)
			{
				rdpMcsChannel* cur = &mcs->channels[channel->index];
				cur->handle = NULL;
			}
		}
		else
		{
			ArrayList_Remove(vcm->dynamicVirtualChannels, channel);

			if (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED)
			{
				ULONG written;
				s = Stream_New(NULL, 8);

				if (!s)
				{
					WLog_ERR(TAG, "Stream_New failed!");
					ret = FALSE;
				}
				else
				{
					wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, channel->channelId);
					ret = WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR)Stream_Buffer(s),
					                             Stream_GetPosition(s), &written);
					Stream_Free(s, TRUE);
				}
			}
		}

		if (channel->receiveData)
			Stream_Free(channel->receiveData, TRUE);

		if (channel->queue)
		{
			MessageQueue_Free(channel->queue);
			channel->queue = NULL;
		}

		free(channel);
	}

	return ret;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer,
                                          ULONG BufferSize, PULONG pBytesRead)
{
	BYTE* buffer;
	wMessage message;
	wtsChannelMessage* messageCtx;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;

	WINPR_ASSERT(channel);

	if (!MessageQueue_Peek(channel->queue, &message, FALSE))
	{
		SetLastError(ERROR_NO_DATA);
		*pBytesRead = 0;
		return FALSE;
	}

	messageCtx = (wtsChannelMessage*)(UINT_PTR)message.context;

	if (messageCtx == NULL)
		return FALSE;

	buffer = (BYTE*)(messageCtx + 1);
	*pBytesRead = messageCtx->length - messageCtx->offset;

	if (Buffer == NULL || BufferSize == 0)
	{
		return TRUE;
	}

	if (*pBytesRead > BufferSize)
		*pBytesRead = BufferSize;

	CopyMemory(Buffer, buffer + messageCtx->offset, *pBytesRead);
	messageCtx->offset += *pBytesRead;

	if (messageCtx->offset >= messageCtx->length)
	{
		MessageQueue_Peek(channel->queue, &message, TRUE);
		peer_channel_queue_free_message(&message);
	}

	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length,
                                           PULONG pBytesWritten)
{
	wStream* s;
	int cbLen;
	int cbChId;
	int first;
	BYTE* buffer;
	UINT32 length;
	UINT32 written;
	UINT32 totalWritten = 0;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;
	BOOL ret = TRUE;

	if (!channel)
		return FALSE;

	WINPR_ASSERT(channel->vcm);
	if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		length = Length;
		buffer = (BYTE*)malloc(length);

		if (!buffer)
		{
			SetLastError(E_OUTOFMEMORY);
			return FALSE;
		}

		CopyMemory(buffer, Buffer, length);
		totalWritten = Length;
		ret = wts_queue_send_item(channel, buffer, length);
	}
	else if (!channel->vcm->drdynvc_channel || (channel->vcm->drdynvc_state != DRDYNVC_STATE_READY))
	{
		DEBUG_DVC("drdynvc not ready");
		return FALSE;
	}
	else
	{
		first = TRUE;
		WINPR_ASSERT(channel->client);
		WINPR_ASSERT(channel->client->settings);
		while (Length > 0)
		{
			s = Stream_New(NULL, channel->client->settings->VirtualChannelChunkSize);

			if (!s)
			{
				WLog_ERR(TAG, "Stream_New failed!");
				SetLastError(E_OUTOFMEMORY);
				return FALSE;
			}

			buffer = Stream_Buffer(s);
			Stream_Seek_UINT8(s);
			cbChId = wts_write_variable_uint(s, channel->channelId);

			if (first && (Length > (UINT32)Stream_GetRemainingLength(s)))
			{
				cbLen = wts_write_variable_uint(s, Length);
				buffer[0] = (DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId;
			}
			else
			{
				buffer[0] = (DATA_PDU << 4) | cbChId;
			}

			first = FALSE;
			written = Stream_GetRemainingLength(s);

			if (written > Length)
				written = Length;

			Stream_Write(s, Buffer, written);
			length = Stream_GetPosition(s);
			Stream_Free(s, FALSE);
			Length -= written;
			Buffer += written;
			totalWritten += written;
			ret = wts_queue_send_item(channel->vcm->drdynvc_channel, buffer, length);
		}
	}

	if (pBytesWritten)
		*pBytesWritten = totalWritten;

	return ret;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                                           PVOID* ppBuffer, DWORD* pBytesReturned)
{
	void* pfd;
	BOOL bval;
	void* fds[10] = { 0 };
	HANDLE hEvent;
	int fds_count = 0;
	BOOL status = FALSE;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;

	WINPR_ASSERT(channel);

	hEvent = MessageQueue_Event(channel->queue);

	switch ((UINT32)WtsVirtualClass)
	{
		case WTSVirtualFileHandle:
			pfd = GetEventWaitObject(hEvent);

			if (pfd)
			{
				fds[fds_count] = pfd;
				(fds_count)++;
			}

			*ppBuffer = malloc(sizeof(void*));

			if (!*ppBuffer)
			{
				SetLastError(E_OUTOFMEMORY);
			}
			else
			{
				CopyMemory(*ppBuffer, &fds[0], sizeof(void*));
				*pBytesReturned = sizeof(void*);
				status = TRUE;
			}

			break;

		case WTSVirtualEventHandle:
			*ppBuffer = malloc(sizeof(HANDLE));

			if (!*ppBuffer)
			{
				SetLastError(E_OUTOFMEMORY);
			}
			else
			{
				CopyMemory(*ppBuffer, &(hEvent), sizeof(HANDLE));
				*pBytesReturned = sizeof(void*);
				status = TRUE;
			}

			break;

		case WTSVirtualChannelReady:
			if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
			{
				bval = TRUE;
				status = TRUE;
			}
			else
			{
				switch (channel->dvc_open_state)
				{
					case DVC_OPEN_STATE_NONE:
						bval = FALSE;
						status = TRUE;
						break;

					case DVC_OPEN_STATE_SUCCEEDED:
						bval = TRUE;
						status = TRUE;
						break;

					default:
						bval = FALSE;
						status = FALSE;
						break;
				}
			}

			*ppBuffer = malloc(sizeof(BOOL));

			if (!*ppBuffer)
			{
				SetLastError(E_OUTOFMEMORY);
				status = FALSE;
			}
			else
			{
				CopyMemory(*ppBuffer, &bval, sizeof(BOOL));
				*pBytesReturned = sizeof(BOOL);
			}

			break;

		default:
			break;
	}

	return status;
}

VOID WINAPI FreeRDP_WTSFreeMemory(PVOID pMemory)
{
	free(pMemory);
}

BOOL WINAPI FreeRDP_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
                                     ULONG NumberOfEntries)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
                                     ULONG NumberOfEntries)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotification(HWND hWnd)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId,
                                             LPWSTR* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId,
                                             LPSTR* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                           PWTSLISTENERNAMEW pListeners, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                           PWTSLISTENERNAMEA pListeners, DWORD* pCount)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                       LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer,
                                       DWORD flag)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                       LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPWSTR pListenerName,
                                            SECURITY_INFORMATION SecurityInformation,
                                            PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPSTR pListenerName,
                                            SECURITY_INFORMATION SecurityInformation,
                                            PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPWSTR pListenerName,
                                            SECURITY_INFORMATION SecurityInformation,
                                            PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength,
                                            LPDWORD lpnLengthNeeded)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
                                            LPSTR pListenerName,
                                            SECURITY_INFORMATION SecurityInformation,
                                            PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength,
                                            LPDWORD lpnLengthNeeded)
{
	return FALSE;
}

BOOL CDECL FreeRDP_WTSEnableChildSessions(BOOL bEnable)
{
	return FALSE;
}

BOOL CDECL FreeRDP_WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	return FALSE;
}

BOOL CDECL FreeRDP_WTSGetChildSessionId(PULONG pSessionId)
{
	return FALSE;
}

DWORD WINAPI FreeRDP_WTSGetActiveConsoleSessionId(void)
{
	return 0xFFFFFFFF;
}
BOOL WINAPI FreeRDP_WTSLogoffUser(HANDLE hServer)
{
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSLogonUser(HANDLE hServer, LPCSTR username, LPCSTR password, LPCSTR domain)
{
	return FALSE;
}
