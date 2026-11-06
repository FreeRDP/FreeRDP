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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <winpr/atexit.h>
#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/log.h>
#include <freerdp/constants.h>
#if defined(WITH_RDPEUDP)
#include <freerdp/multitransport.h>
#endif
#include <freerdp/server/channels.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/utils/drdynvc.h>

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

#define DVC_MAX_DATA_PDU_SIZE 1600

#if defined(WITH_RDPEUDP)
/* Client PDUs from the tunnel held while waiting for its Soft-Sync Response. */
#define SOFT_SYNC_MAX_BACKLOG 1024

/* vcm->queue message ids: where a PDU for the drdynvc channel goes */
typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
{
	WTS_QUEUE_TCP = 0,
	WTS_QUEUE_TUNNEL = 1
} WTS_QUEUE_ID;

#endif
typedef struct
{
	UINT16 channelId;
	UINT16 reserved;
	UINT32 length;
	UINT32 offset;
} wtsChannelMessage;

static const DWORD g_err_oom = WINPR_CXX_COMPAT_CAST(DWORD, E_OUTOFMEMORY);

static DWORD g_SessionId = 1;
static wHashTable* g_ServerHandles = nullptr;
static INIT_ONCE g_HandleInitializer = INIT_ONCE_STATIC_INIT;

static rdpPeerChannel* wts_get_dvc_channel_by_id(WTSVirtualChannelManager* vcm, UINT32 ChannelId)
{
	WINPR_ASSERT(vcm);
	return HashTable_GetItemValue(vcm->dynamicVirtualChannels, &ChannelId);
}

static BOOL wts_queue_receive_data(rdpPeerChannel* channel, const BYTE* Buffer1, size_t Length1,
                                   const BYTE* Buffer2, size_t Length2)
{
	WINPR_ASSERT(channel);

	if (Length1 > UINT32_MAX - sizeof(wtsChannelMessage))
		return FALSE;
	if (Length2 > UINT32_MAX - sizeof(wtsChannelMessage) - Length1)
		return FALSE;
	const size_t len = Length1 + Length2;
	const size_t tlen = sizeof(wtsChannelMessage) + len;
	wtsChannelMessage* messageCtx = (wtsChannelMessage*)malloc(tlen);
	if (!messageCtx)
		return FALSE;

	WINPR_ASSERT(channel->channelId <= UINT16_MAX);
	messageCtx->channelId = WINPR_ASSERTING_INT_CAST(UINT16, channel->channelId);
	messageCtx->length = WINPR_ASSERTING_INT_CAST(UINT32, len);
	messageCtx->offset = 0;
	BYTE* buffer = (BYTE*)(&messageCtx[1]);
	CopyMemory(buffer, Buffer1, Length1);

	WINPR_ASSERT(Buffer2 || (Length2 == 0));
	buffer += Length1;
	if (Buffer2)
		CopyMemory(buffer, Buffer2, Length2);

	return MessageQueue_Post(channel->queue, messageCtx, 0, nullptr, nullptr);
}

#if defined(WITH_RDPEUDP)
WINPR_ATTR_NODISCARD
static BOOL wts_queue_send_item_on(rdpPeerChannel* channel, BYTE* Buffer, UINT32 Length,
                                   BOOL tunnel)
#else
static BOOL wts_queue_send_item(rdpPeerChannel* channel, BYTE* Buffer, UINT32 Length)
#endif
{
	BYTE* buffer = nullptr;
	UINT32 length = 0;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	buffer = Buffer;
	length = Length;

	WINPR_ASSERT(channel->channelId <= UINT16_MAX);
	const UINT16 channelId = (UINT16)channel->channelId;
#if defined(WITH_RDPEUDP)
	const WTS_QUEUE_ID id = tunnel ? WTS_QUEUE_TUNNEL : WTS_QUEUE_TCP;
	return MessageQueue_Post(channel->vcm->queue, (void*)(UINT_PTR)channelId, id, (void*)buffer,
	                         (void*)(UINT_PTR)length);
#else
	return MessageQueue_Post(channel->vcm->queue, (void*)(UINT_PTR)channelId, 0, (void*)buffer,
	                         (void*)(UINT_PTR)length);
#endif
}

#if defined(WITH_RDPEUDP)
WINPR_ATTR_NODISCARD
static BOOL wts_queue_send_item(rdpPeerChannel* channel, BYTE* Buffer, UINT32 Length)
{
	return wts_queue_send_item_on(channel, Buffer, Length, FALSE);
}

/* Queues a copy of one drdynvc PDU, on TCP or on the tunnel. */
WINPR_ATTR_NODISCARD
static BOOL wts_queue_drdynvc_pdu(WTSVirtualChannelManager* vcm, const BYTE* data, size_t length,
                                  BOOL tunnel)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->drdynvc_channel);
	WINPR_ASSERT(data || (length == 0));
	if ((length == 0) || (length > UINT32_MAX))
		return FALSE;
	BYTE* buffer = malloc(length);
	if (!buffer)
		return FALSE;
	memcpy(buffer, data, length);
	if (!wts_queue_send_item_on(vcm->drdynvc_channel, buffer,
	                            WINPR_ASSERTING_INT_CAST(UINT32, length), tunnel))
	{
		free(buffer);
		return FALSE;
	}
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): the queue takes ownership of buffer
	return TRUE;
}

#endif
static unsigned wts_read_variable_uint(wStream* s, int cbLen, UINT32* val)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(val);
	switch (cbLen)
	{
		case 0:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
				return 0;

			Stream_Read_UINT8(s, *val);
			return 1;

		case 1:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
				return 0;

			Stream_Read_UINT16(s, *val);
			return 2;

		case 2:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return 0;

			Stream_Read_UINT32(s, *val);
			return 4;

		default:
			WLog_ERR(TAG, "invalid wts variable uint len %d", cbLen);
			return 0;
	}
}

static BOOL wts_read_drdynvc_capabilities_response(rdpPeerChannel* channel, wStream* s)
{
	UINT16 Version = 0;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	const size_t length = Stream_GetRemainingLength(s);
	if (length < 3)
		return FALSE;

	Stream_Seek_UINT8(s); /* Pad (1 byte) */
	Stream_Read_UINT16(s, Version);
	DEBUG_DVC("Version: %" PRIu16 "", Version);

	if (Version < 1)
	{
		WLog_ERR(TAG, "invalid version %" PRIu16 " for DRDYNVC", Version);
		return FALSE;
	}

	WTSVirtualChannelManager* vcm = channel->vcm;
	vcm->drdynvc_state = DRDYNVC_STATE_READY;

	/* we only support version 1 for now (no compression yet) */
	vcm->dvc_spoken_version = MAX(Version, 1);

	return SetEvent(MessageQueue_Event(vcm->queue));
}

static BOOL wts_read_drdynvc_create_response(rdpPeerChannel* channel, wStream* s)
{
	UINT32 CreationStatus = 0;
	BOOL status = TRUE;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);
	const size_t length = Stream_GetRemainingLength(s);
	if ((length < 4) || (length > UINT32_MAX))
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

	channel->creationStatus = (INT32)CreationStatus;
	IFCALLRET(channel->vcm->dvc_creation_status, status, channel->vcm->dvc_creation_status_userdata,
	          channel->channelId, (INT32)CreationStatus);
	if (!status)
		WLog_ERR(TAG, "vcm->dvc_creation_status failed!");

#if defined(WITH_RDPEUDP)
	channel->vcm->softSyncDue = TRUE;
#endif
	return status;
}

static BOOL wts_read_drdynvc_data_first(rdpPeerChannel* channel, wStream* s, int cbLen)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);

	const UINT32 value = wts_read_variable_uint(s, cbLen, &channel->dvc_total_length);

	if ((value == 0) || (channel->dvc_total_length == 0))
		return FALSE;

	const size_t length = Stream_GetRemainingLength(s);
	if (length > channel->dvc_total_length)
		return FALSE;

	Stream_ResetPosition(channel->receiveData);

	if (!Stream_EnsureRemainingCapacity(channel->receiveData, length))
		return FALSE;

	Stream_Write(channel->receiveData, Stream_ConstPointer(s), length);
	return TRUE;
}

static BOOL wts_read_drdynvc_data(rdpPeerChannel* channel, wStream* s)
{
	BOOL ret = FALSE;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);

	const size_t length = Stream_GetRemainingLength(s);
	if (channel->dvc_total_length > 0)
	{
		const size_t pos = Stream_GetPosition(channel->receiveData);
		if (pos > SIZE_MAX - length)
			return FALSE;

		if (pos + length > channel->dvc_total_length)
		{
			channel->dvc_total_length = 0;
			WLog_ERR(TAG, "incorrect fragment data, discarded.");
			return FALSE;
		}

		if (!Stream_EnsureRemainingCapacity(channel->receiveData, length))
			return FALSE;

		Stream_Write(channel->receiveData, Stream_ConstPointer(s), length);

		if (Stream_GetPosition(channel->receiveData) >= channel->dvc_total_length)
		{
			ret = wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData),
			                             channel->dvc_total_length, nullptr, 0);
			channel->dvc_total_length = 0;
		}
		else
			ret = TRUE;
	}
	else
	{
		ret = wts_queue_receive_data(channel, Stream_ConstPointer(s), length, nullptr, 0);
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

WINPR_ATTR_NODISCARD
static BOOL wts_read_drdynvc_pdu_ready_guarded(wStream* s, UINT8 Cmd, UINT8 Sp, UINT32 ChannelId,
                                               rdpPeerChannel* dvc)
{
	switch (Cmd)
	{
		case CREATE_REQUEST_PDU:
			return wts_read_drdynvc_create_response(dvc, s);

		case DATA_FIRST_PDU:
			if (dvc->dvc_open_state != DVC_OPEN_STATE_SUCCEEDED)
			{
				WLog_ERR(TAG,
				         "ChannelId %" PRIu32 " did not open successfully. "
				         "Ignoring DYNVC_DATA_FIRST PDU",
				         ChannelId);
				return TRUE;
			}

			return wts_read_drdynvc_data_first(dvc, s, Sp);

		case DATA_PDU:
			if (dvc->dvc_open_state != DVC_OPEN_STATE_SUCCEEDED)
			{
				WLog_ERR(TAG,
				         "ChannelId %" PRIu32 " did not open successfully. "
				         "Ignoring DYNVC_DATA PDU",
				         ChannelId);
				return TRUE;
			}

			return wts_read_drdynvc_data(dvc, s);

		case CLOSE_REQUEST_PDU:
			wts_read_drdynvc_close_response(dvc);
			break;

		case DATA_FIRST_COMPRESSED_PDU:
		case DATA_COMPRESSED_PDU:
			WLog_ERR(TAG, "Compressed data not handled");
			break;

		case SOFT_SYNC_RESPONSE_PDU:
			WLog_ERR(TAG, "SoftSync response not handled yet(and rather strange to receive "
			              "that packet as our code doesn't send SoftSync requests");
			break;

		case SOFT_SYNC_REQUEST_PDU:
			WLog_ERR(TAG, "Not expecting a SoftSyncRequest on the server");
			return FALSE;

		default:
			WLog_ERR(TAG, "Cmd %d not recognized.", Cmd);
			break;
	}
	return TRUE;
}

#if defined(WITH_RDPEUDP)
WINPR_ATTR_NODISCARD
static BOOL wts_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s);

#endif
WINPR_ATTR_NODISCARD
static BOOL wts_read_drdynvc_pdu_ready(rdpPeerChannel* channel, wStream* s, UINT8 Cmd, UINT8 Sp,
                                       UINT8 cbChId)
{
	WINPR_ASSERT(channel);

	BOOL haveChannelId = 0;
	switch (Cmd)
	{
		case SOFT_SYNC_REQUEST_PDU:
		case SOFT_SYNC_RESPONSE_PDU:
			haveChannelId = FALSE;
			break;
		default:
			haveChannelId = TRUE;
			break;
	}

	UINT32 ChannelId = 0;
	rdpPeerChannel* dvc = nullptr;
	wHashTable* table = nullptr;
	if (haveChannelId)
	{
		const unsigned val = wts_read_variable_uint(s, cbChId, &ChannelId);
		if (val == 0)
			return FALSE;

		DEBUG_DVC("Cmd %s ChannelId %" PRIu32 " length %" PRIuz "", drdynvc_get_packet_type(Cmd),
		          ChannelId, Stream_GetRemainingLength(s));
		table = channel->vcm->dynamicVirtualChannels;
		if (!table)
			return FALSE;
		HashTable_Lock(table);
		dvc = wts_get_dvc_channel_by_id(channel->vcm, ChannelId);
		if (!dvc)
		{
			DEBUG_DVC("ChannelId %" PRIu32 " does not exist.", ChannelId);
			HashTable_Unlock(table);
			return TRUE;
		}
	}

#if defined(WITH_RDPEUDP)
	if (Cmd == SOFT_SYNC_RESPONSE_PDU)
		return wts_read_soft_sync_response(channel->vcm, s);

#endif
	const BOOL rc = wts_read_drdynvc_pdu_ready_guarded(s, Cmd, Sp, ChannelId, dvc);
	if (table)
		HashTable_Unlock(table);
	return rc;
}

#if defined(WITH_RDPEUDP)
/* One client PDU that arrived on the tunnel. Capabilities and Soft-Sync only travel on the
 * drdynvc static channel. */
WINPR_ATTR_NODISCARD
static BOOL wts_read_tunnel_pdu(WTSVirtualChannelManager* vcm, const BYTE* data, size_t length)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(data || (length == 0));
	if (!vcm->drdynvc_channel || (vcm->drdynvc_state != DRDYNVC_STATE_READY) || (length < 1))
		return FALSE;

	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, data, length);
	if (!s)
		return FALSE;
	const UINT8 value = Stream_Get_UINT8(s);
	const UINT8 Cmd = (value & 0xf0) >> 4;
	const UINT8 Sp = (value & 0x0c) >> 2;
	const UINT8 cbChId = (value & 0x03) >> 0;
	switch (Cmd)
	{
		case CAPABILITY_REQUEST_PDU:
		case SOFT_SYNC_REQUEST_PDU:
		case SOFT_SYNC_RESPONSE_PDU:
			WLog_WARN(TAG, "a %s on the tunnel, ending the connection",
			          drdynvc_get_packet_type(Cmd));
			return FALSE;
		default:
			return wts_read_drdynvc_pdu_ready(vcm->drdynvc_channel, s, Cmd, Sp, cbChId);
	}
}

/* [MS-RDPEDYC] 2.2.5.2 DYNVC_SOFT_SYNC_RESPONSE: the client reads and writes the tunnels it
 * names, and the server reads them from now on (3.3.5.3.2). */
WINPR_ATTR_NODISCARD
static BOOL wts_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return FALSE;
	Stream_Seek_UINT8(s); /* Pad */
	const UINT32 count = Stream_Get_UINT32(s);
	if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, count, sizeof(UINT32)))
		return FALSE;
	BOOL reliable = FALSE;
	for (UINT32 x = 0; x < count; x++)
		reliable |= (Stream_Get_UINT32(s) == TUNNELTYPE_UDPFECR);

	if (vcm->softSyncState != SOFT_SYNC_STATE_SENT)
	{
		WLog_WARN(TAG, "unexpected Soft-Sync Response");
		return TRUE;
	}
	vcm->softSyncState = SOFT_SYNC_STATE_ANSWERED;
	vcm->clientOnTunnel = reliable;
	WLog_INFO(TAG, "Soft-Sync Response: the client %s the reliable UDP tunnel",
	          reliable ? "writes on" : "does not use");

	/* [MS-RDPEDYC] 3.2.5.3.2: the response names every tunnel the client writes on. The server
	 * keeps writing on the tunnel its request named, but reads only what the client named. */
	if (!reliable && (Queue_Count(vcm->tunnelBacklog) > 0))
	{
		WLog_ERR(TAG, "the client wrote on a tunnel its Soft-Sync Response does not name");
		return FALSE;
	}

	wStream* held = nullptr;
	while ((held = Queue_Dequeue(vcm->tunnelBacklog)))
	{
		const BOOL rc = wts_read_tunnel_pdu(vcm, Stream_Buffer(held), Stream_Length(held));
		Stream_Release(held);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

/* Moves every open dynamic channel onto the tunnel with a Soft-Sync Request ([MS-RDPEDYC]
 * 3.3.5.3.1). The request goes out on TCP after everything the channels queued there, then they
 * are written on the tunnel: each channel's writeLock is held across both, so no message of
 * theirs straddles the switch. */
WINPR_ATTR_NODISCARD
static BOOL wts_soft_sync(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);

	if ((vcm->softSyncState != SOFT_SYNC_STATE_NONE) || !vcm->tunnelReady ||
	    !vcm->drdynvc_channel || (vcm->drdynvc_state != DRDYNVC_STATE_READY))
		return TRUE;

	BOOL rc = FALSE;
	ULONG_PTR* keys = nullptr;
	rdpPeerChannel** channels = nullptr;
	size_t count = 0;
	wStream* s = nullptr;

	HashTable_Lock(vcm->dynamicVirtualChannels);
	const size_t total = HashTable_GetKeys(vcm->dynamicVirtualChannels, &keys);
	if (total > 0)
	{
		channels = calloc(total, sizeof(rdpPeerChannel*));
		if (!channels)
			goto out;
	}
	for (size_t x = 0; x < total; x++)
	{
		rdpPeerChannel* channel = HashTable_GetItemValue(vcm->dynamicVirtualChannels, (void*)keys[x]);
		/* one the client has not answered yet would stay on TCP for good: its answer tries again */
		if (channel && (channel->dvc_open_state == DVC_OPEN_STATE_NONE))
		{
			rc = TRUE;
			goto out;
		}
		if (channel && (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED))
			channels[count++] = channel;
	}
	if (count == 0)
	{
		/* nothing to move yet, the first channel that opens tries again */
		rc = TRUE;
		goto out;
	}
	/* NumberOfDVCs is 16 bits */
	if (count > UINT16_MAX)
	{
		WLog_ERR(TAG, "%" PRIuz " dynamic channels are too many for a Soft-Sync Request", count);
		goto out;
	}

	/* DYNVC_SOFT_SYNC_REQUEST: Length counts from the Length field on */
	const size_t listLength = 4 + 2 + sizeof(UINT32) * count;
	s = Stream_New(nullptr, 2 + 4 + 2 + 2 + listLength);
	if (!s)
		goto out;
	/* the Stream_Write macros evaluate their value more than once */
	const UINT8 header = WINPR_ASSERTING_INT_CAST(UINT8, (SOFT_SYNC_REQUEST_PDU << 4) & 0xFF);
	const UINT32 length = WINPR_ASSERTING_INT_CAST(UINT32, 4 + 2 + 2 + listLength);
	const UINT16 dvcs = WINPR_ASSERTING_INT_CAST(UINT16, count);
	Stream_Write_UINT8(s, header);
	Stream_Write_UINT8(s, 0);                                                       /* Pad */
	Stream_Write_UINT32(s, length);                                                 /* Length */
	Stream_Write_UINT16(s, SOFT_SYNC_TCP_FLUSHED | SOFT_SYNC_CHANNEL_LIST_PRESENT); /* Flags */
	Stream_Write_UINT16(s, 1);                  /* NumberOfTunnels */
	Stream_Write_UINT32(s, TUNNELTYPE_UDPFECR); /* TunnelType */
	Stream_Write_UINT16(s, dvcs);               /* NumberOfDVCs */
	for (size_t x = 0; x < count; x++)
		Stream_Write_UINT32(s, channels[x]->channelId);

	for (size_t x = 0; x < count; x++)
		EnterCriticalSection(&channels[x]->writeLock);
	rc = wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), Stream_GetPosition(s), FALSE);
	if (rc)
	{
		for (size_t x = 0; x < count; x++)
			channels[x]->tunnel = TRUE;
		vcm->softSyncState = SOFT_SYNC_STATE_SENT;
	}
	for (size_t x = 0; x < count; x++)
		LeaveCriticalSection(&channels[x]->writeLock);

	if (rc)
	{
		WLog_INFO(TAG, "Soft-Sync Request: %" PRIuz " dynamic channel(s) move to the UDP tunnel",
		          count);
		for (size_t x = 0; x < count; x++)
			WLog_DBG(TAG, "  %" PRIu32 " %s", channels[x]->channelId, channels[x]->channelName);
	}

out:
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	Stream_Free(s, TRUE);
	free(channels);
	free(keys);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL wts_tunnel_data_received(void* custom, UINT32 tunnelType, wStream* s)
{
	WTSVirtualChannelManager* vcm = custom;
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(s);
	WINPR_UNUSED(tunnelType);

	/* [MS-RDPEDYC] 3.3.5.3.2: nothing from the tunnel is read before the Soft-Sync Response */
	if (vcm->softSyncState != SOFT_SYNC_STATE_ANSWERED)
	{
		if (Queue_Count(vcm->tunnelBacklog) >= SOFT_SYNC_MAX_BACKLOG)
		{
			WLog_ERR(TAG, "tunnel data keeps coming without a Soft-Sync Response");
			Stream_Release(s);
			return FALSE;
		}
		/* the backlog owns s now */
		if (!Queue_Enqueue(vcm->tunnelBacklog, s))
		{
			Stream_Release(s);
			return FALSE;
		}
		return TRUE;
	}
	BOOL rc = FALSE;
	if (!vcm->clientOnTunnel)
		WLog_ERR(TAG, "the client writes on a tunnel its Soft-Sync Response does not name");
	else
		rc = wts_read_tunnel_pdu(vcm, Stream_Buffer(s), Stream_Length(s));
	Stream_Release(s);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL wts_tunnel_state_changed(void* custom, UINT32 tunnelType,
                                     FreeRDP_MultitransportTunnelState state)
{
	WTSVirtualChannelManager* vcm = custom;
	WINPR_ASSERT(vcm);
	if (tunnelType != TUNNELTYPE_UDPFECR)
		return TRUE;
	vcm->tunnelReady = (state == FREERDP_MULTITRANSPORT_TUNNEL_READY);
	return wts_soft_sync(vcm);
}

#endif
WINPR_ATTR_NODISCARD
static BOOL wts_read_drdynvc_pdu(rdpPeerChannel* channel)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);

	const size_t slength = Stream_GetPosition(channel->receiveData);
	if ((slength < 1) || (slength > UINT32_MAX))
		return FALSE;

	Stream_ResetPosition(channel->receiveData);
	wStream buffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&buffer, Stream_Buffer(channel->receiveData), slength);
	const UINT8 value = Stream_Get_UINT8(s);
	const UINT8 Cmd = (value & 0xf0) >> 4;
	const UINT8 Sp = (value & 0x0c) >> 2;
	const UINT8 cbChId = (value & 0x03) >> 0;

	if (Cmd == CAPABILITY_REQUEST_PDU)
		return wts_read_drdynvc_capabilities_response(channel, s);

	if (channel->vcm->drdynvc_state == DRDYNVC_STATE_READY)
		return wts_read_drdynvc_pdu_ready(channel, s, Cmd, Sp, cbChId);
	else
		WLog_ERR(TAG, "received Cmd %d but channel is not ready.", Cmd);

	return TRUE;
}

static int wts_write_variable_uint(wStream* s, UINT32 val)
{
	int cb = 0;

	WINPR_ASSERT(s);
	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(s, WINPR_ASSERTING_INT_CAST(uint8_t, val));
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(uint16_t, val));
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
	WINPR_ASSERT(s);

	BYTE* bm = Stream_PointerAs(s, BYTE);
	Stream_Seek_UINT8(s);
	const int cbChId = wts_write_variable_uint(s, ChannelId);
	*bm = (((Cmd & 0x0F) << 4) | cbChId) & 0xFF;
}

static BOOL wts_write_drdynvc_create_request(wStream* s, UINT32 ChannelId, const char* ChannelName)
{
	size_t len = 0;

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
	size_t size = s;
	const size_t totalSize = t;

	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_UNUSED(channelId);

	if (channel->channelFlags & CHANNEL_OPTION_SHOW_PROTOCOL)
	{
		BOOL firstPass = TRUE;

		while (size)
		{
			const UINT32 payloadLen = (size > CHANNEL_CHUNK_LENGTH)
			                              ? CHANNEL_CHUNK_LENGTH
			                              : WINPR_ASSERTING_INT_CAST(UINT32, size);
			size -= payloadLen;

			/* here we skip other flags than CHANNEL_FLAG_FIRST and CHANNEL_FLAG_LAST
			 * as it's the only ones treated by ChannelPduTracker.
			 */
			UINT32 newFlags = 0;
			if (firstPass)
			{
				firstPass = FALSE;
				if (flags & CHANNEL_FLAG_FIRST)
					newFlags = CHANNEL_FLAG_FIRST;
			}

			if (!size && (flags & CHANNEL_FLAG_LAST))
				newFlags |= CHANNEL_FLAG_LAST;

			const CHANNEL_PDU_HEADER header = {
				.length = payloadLen,
				.flags = newFlags,
			};

			if (!wts_queue_receive_data(channel, (const BYTE*)&header, sizeof(header), data,
			                            payloadLen))
				return FALSE;

			data += payloadLen;
		}
		return TRUE;
	}

	if ((flags & CHANNEL_FLAG_FIRST) != 0)
	{
		Stream_ResetPosition(channel->receiveData);
	}

	if (!Stream_EnsureRemainingCapacity(channel->receiveData, size))
		return FALSE;

	Stream_Write(channel->receiveData, data, size);

	if ((flags & CHANNEL_FLAG_LAST) != 0)
	{
		if (Stream_GetPosition(channel->receiveData) != totalSize)
			return FALSE;

		if (channel == channel->vcm->drdynvc_channel)
		{
			ret = wts_read_drdynvc_pdu(channel);
		}
		else
		{
			const size_t pos = Stream_GetPosition(channel->receiveData);
			if (pos > UINT32_MAX)
				ret = FALSE;
			else
				ret = wts_queue_receive_data(channel, Stream_Buffer(channel->receiveData), pos,
				                             nullptr, 0);
		}

		Stream_ResetPosition(channel->receiveData);
	}
	else if (Stream_GetPosition(channel->receiveData) >= totalSize)
		return FALSE;

	return ret;
}

static BOOL WTSReceiveChannelData(freerdp_peer* client, UINT16 channelId, const BYTE* data,
                                  size_t size, UINT32 flags, size_t totalSize)
{
	rdpMcs* mcs = nullptr;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);

	mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);

	for (UINT32 i = 0; i < mcs->channelCount; i++)
	{
		rdpMcsChannel* cur = &mcs->channels[i];
		if (cur->ChannelId == channelId)
		{
			rdpPeerChannel* channel = (rdpPeerChannel*)cur->handle;

			if (channel)
				return WTSProcessChannelData(channel, channelId, data, size, flags, totalSize);
		}
	}

	WLog_WARN(TAG, "unknown channelId %" PRIu16 " ignored", channelId);

	return TRUE;
}

#if defined(WITH_FREERDP_DEPRECATED)
void WTSVirtualChannelManagerGetFileDescriptor(HANDLE hServer, void** fds, int* fds_count)
{
	void* fd = nullptr;
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
}
#endif

BOOL WTSVirtualChannelManagerOpen(HANDLE hServer)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm)
		return FALSE;

	if (vcm->drdynvc_state == DRDYNVC_STATE_NONE)
	{
		rdpPeerChannel* channel = nullptr;

		/* Initialize drdynvc channel once and only once. */
		vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;
		channel = (rdpPeerChannel*)WTSVirtualChannelOpen((HANDLE)vcm, WTS_CURRENT_SESSION,
		                                                 DRDYNVC_SVC_CHANNEL_NAME);

		if (channel)
		{
			BYTE capaBuffer[12] = WINPR_C_ARRAY_INIT;
			wStream staticS = WINPR_C_ARRAY_INIT;
			wStream* s = Stream_StaticInit(&staticS, capaBuffer, sizeof(capaBuffer));

			vcm->drdynvc_channel = channel;
			vcm->dvc_spoken_version = 1;
			Stream_Write_UINT8(s, 0x50);    /* Cmd=5 sp=0 cbId=0 */
			Stream_Write_UINT8(s, 0x00);    /* Pad */
			Stream_Write_UINT16(s, 0x0001); /* Version */

			/* TODO: shall implement version 2 and 3 */

			const size_t pos = Stream_GetPosition(s);
			WINPR_ASSERT(pos <= UINT32_MAX);
			ULONG written = 0;
			if (!WTSVirtualChannelWrite(channel, (PCHAR)capaBuffer, (UINT32)pos, &written))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL WTSVirtualChannelManagerCheckFileDescriptorEx(HANDLE hServer, BOOL autoOpen)
{
	wMessage message = WINPR_C_ARRAY_INIT;
	BOOL status = TRUE;
	WTSVirtualChannelManager* vcm = nullptr;

	if (!hServer || hServer == INVALID_HANDLE_VALUE)
		return FALSE;

	vcm = (WTSVirtualChannelManager*)hServer;

	if (autoOpen)
	{
		if (!WTSVirtualChannelManagerOpen(hServer))
			return FALSE;
	}

#if defined(WITH_RDPEUDP)
	if (vcm->softSyncDue)
	{
		vcm->softSyncDue = FALSE;
		if (!wts_soft_sync(vcm))
			return FALSE;
	}

	BOOL tunnelWritten = FALSE;
#endif
	while (MessageQueue_Peek(vcm->queue, &message, TRUE))
	{
		BYTE* buffer = nullptr;
		UINT32 length = 0;
		UINT16 channelId = 0;
		channelId = (UINT16)(UINT_PTR)message.context;
		buffer = (BYTE*)message.wParam;
		length = (UINT32)(UINT_PTR)message.lParam;

		WINPR_ASSERT(vcm->client);
		WINPR_ASSERT(vcm->client->SendChannelData);
#if defined(WITH_RDPEUDP)
		if (message.id == WTS_QUEUE_TUNNEL)
		{
			if (!freerdp_multitransport_send_dvc(vcm->client->context, TUNNELTYPE_UDPFECR,
			                                     buffer, length))
			{
				WLog_ERR(TAG, "cannot write a dynamic channel PDU on the UDP tunnel");
				status = FALSE;
			}
		}
		else
#endif
		if (!vcm->client->SendChannelData(vcm->client, channelId, buffer, length))
		{
			status = FALSE;
		}

		free(buffer);

		if (!status)
			break;
#if defined(WITH_RDPEUDP)
		if (message.id == WTS_QUEUE_TUNNEL)
			tunnelWritten = TRUE;
#endif
	}

#if defined(WITH_RDPEUDP)
	WINPR_ASSERT(!tunnelWritten || vcm->client->context->rdp);
	if (status && tunnelWritten &&
	    !multitransport_flush_dvc(vcm->client->context->rdp->multitransport))
	{
		WLog_ERR(TAG, "cannot write dynamic channel PDUs on the UDP tunnel");
		status = FALSE;
	}

#endif
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
	if (!mcs || !channel_name || !strnlen(channel_name, CHANNEL_NAME_LEN + 1))
		return nullptr;

	for (UINT32 index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined)
		{
			if (_strnicmp(mchannel->Name, channel_name, CHANNEL_NAME_LEN + 1) == 0)
				return mchannel;
		}
	}

	return nullptr;
}

static rdpMcsChannel* wts_get_joined_channel_by_id(rdpMcs* mcs, const UINT16 channel_id)
{
	if (!mcs || !channel_id)
		return nullptr;

	WINPR_ASSERT(mcs->channels);
	for (UINT32 index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined)
		{
			if (mchannel->ChannelId == channel_id)
				return &mcs->channels[index];
		}
	}

	return nullptr;
}

BOOL WTSIsChannelJoinedByName(freerdp_peer* client, const char* channel_name)
{
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	return (wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name) != nullptr);
}

BOOL WTSIsChannelJoinedById(freerdp_peer* client, UINT16 channel_id)
{
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	return (wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id) != nullptr);
}

BOOL WTSVirtualChannelManagerIsChannelJoined(HANDLE hServer, const char* name)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm || !vcm->rdp)
		return FALSE;

	return (wts_get_joined_channel_by_name(vcm->rdp->mcs, name) != nullptr);
}

BYTE WTSVirtualChannelManagerGetDrdynvcState(HANDLE hServer)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	WINPR_ASSERT(vcm);
	return vcm->drdynvc_state;
}

void WTSVirtualChannelManagerSetDVCCreationCallback(HANDLE hServer, psDVCCreationStatusCallback cb,
                                                    void* userdata)
{
	WTSVirtualChannelManager* vcm = hServer;

	WINPR_ASSERT(vcm);

	vcm->dvc_creation_status = cb;
	vcm->dvc_creation_status_userdata = userdata;
}

UINT16 WTSChannelGetId(freerdp_peer* client, const char* channel_name)
{
	rdpMcsChannel* channel = nullptr;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return 0;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return 0;

	return channel->ChannelId;
}

UINT32 WTSChannelGetIdByHandle(HANDLE hChannelHandle)
{
	rdpPeerChannel* channel = hChannelHandle;

	WINPR_ASSERT(channel);

	return channel->channelId;
}

BOOL WTSChannelSetHandleByName(freerdp_peer* client, const char* channel_name, void* handle)
{
	rdpMcsChannel* channel = nullptr;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return FALSE;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return FALSE;

	channel->handle = handle;
	return TRUE;
}

BOOL WTSChannelSetHandleById(freerdp_peer* client, UINT16 channel_id, void* handle)
{
	rdpMcsChannel* channel = nullptr;

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
	rdpMcsChannel* channel = nullptr;

	WINPR_ASSERT(channel_name);
	if (!client || !client->context || !client->context->rdp)
		return nullptr;

	channel = wts_get_joined_channel_by_name(client->context->rdp->mcs, channel_name);

	if (!channel)
		return nullptr;

	return channel->handle;
}

void* WTSChannelGetHandleById(freerdp_peer* client, UINT16 channel_id)
{
	rdpMcsChannel* channel = nullptr;

	if (!client || !client->context || !client->context->rdp)
		return nullptr;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return nullptr;

	return channel->handle;
}

const char* WTSChannelGetName(freerdp_peer* client, UINT16 channel_id)
{
	rdpMcsChannel* channel = nullptr;

	if (!client || !client->context || !client->context->rdp)
		return nullptr;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return nullptr;

	return (const char*)channel->Name;
}

char** WTSGetAcceptedChannelNames(freerdp_peer* client, size_t* count)
{
	rdpMcs* mcs = nullptr;
	char** names = nullptr;

	if (!client || !client->context || !count)
		return nullptr;

	WINPR_ASSERT(client->context->rdp);
	mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);
	*count = mcs->channelCount;

	names = (char**)calloc(mcs->channelCount, sizeof(char*));
	if (!names)
		return nullptr;

	for (UINT32 index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		names[index] = mchannel->Name;
	}

	return names;
}

INT64 WTSChannelGetOptions(freerdp_peer* client, UINT16 channel_id)
{
	rdpMcsChannel* channel = nullptr;

	if (!client || !client->context || !client->context->rdp)
		return -1;

	channel = wts_get_joined_channel_by_id(client->context->rdp->mcs, channel_id);

	if (!channel)
		return -1;

	return (INT64)channel->options;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionW(WINPR_ATTR_UNUSED LPWSTR pTargetServerName,
                                                  WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                                  WINPR_ATTR_UNUSED BYTE HotkeyVk,
                                                  WINPR_ATTR_UNUSED USHORT HotkeyModifiers)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionA(WINPR_ATTR_UNUSED LPSTR pTargetServerName,
                                                  WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                                  WINPR_ATTR_UNUSED BYTE HotkeyVk,
                                                  WINPR_ATTR_UNUSED USHORT HotkeyModifiers)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExW(WINPR_ATTR_UNUSED LPWSTR pTargetServerName,
                                                    WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                                    WINPR_ATTR_UNUSED BYTE HotkeyVk,
                                                    WINPR_ATTR_UNUSED USHORT HotkeyModifiers,
                                                    WINPR_ATTR_UNUSED DWORD flags)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExA(WINPR_ATTR_UNUSED LPSTR pTargetServerName,
                                                    WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                                    WINPR_ATTR_UNUSED BYTE HotkeyVk,
                                                    WINPR_ATTR_UNUSED USHORT HotkeyModifiers,
                                                    WINPR_ATTR_UNUSED DWORD flags)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSStopRemoteControlSession(WINPR_ATTR_UNUSED ULONG LogonId)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSConnectSessionW(WINPR_ATTR_UNUSED ULONG LogonId,
                                       WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                       WINPR_ATTR_UNUSED PWSTR pPassword,
                                       WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSConnectSessionA(WINPR_ATTR_UNUSED ULONG LogonId,
                                       WINPR_ATTR_UNUSED ULONG TargetLogonId,
                                       WINPR_ATTR_UNUSED PSTR pPassword,
                                       WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateServersW(WINPR_ATTR_UNUSED LPWSTR pDomainName,
                                         WINPR_ATTR_UNUSED DWORD Reserved,
                                         WINPR_ATTR_UNUSED DWORD Version,
                                         WINPR_ATTR_UNUSED PWTS_SERVER_INFOW* ppServerInfo,
                                         WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateServersA(WINPR_ATTR_UNUSED LPSTR pDomainName,
                                         WINPR_ATTR_UNUSED DWORD Reserved,
                                         WINPR_ATTR_UNUSED DWORD Version,
                                         WINPR_ATTR_UNUSED PWTS_SERVER_INFOA* ppServerInfo,
                                         WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerW(WINPR_ATTR_UNUSED LPWSTR pServerName)
{
	WLog_ERR("TODO", "TODO: implement");
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

static void channel_free(rdpPeerChannel* channel)
{
	server_channel_common_free(channel);
}

static void array_channel_free(void* ptr)
{
	rdpPeerChannel* channel = ptr;
	channel_free(channel);
}

static BOOL dynChannelMatch(const void* v1, const void* v2)
{
	const UINT32* p1 = (const UINT32*)v1;
	const UINT32* p2 = (const UINT32*)v2;
	return *p1 == *p2;
}

static UINT32 channelId_Hash(const void* key)
{
	const UINT32* v = (const UINT32*)key;
	return *v;
}

static void clearHandles(void)
{
	HashTable_Free(g_ServerHandles);
	g_ServerHandles = nullptr;
}

static BOOL CALLBACK initializeHandles(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                       WINPR_ATTR_UNUSED PVOID param,
                                       WINPR_ATTR_UNUSED PVOID* context)
{
	WINPR_ASSERT(g_ServerHandles == nullptr);
	g_ServerHandles = HashTable_New(TRUE);
	(void)winpr_atexit(clearHandles);
	return g_ServerHandles != nullptr;
}

static bool setup(void)
{
	return InitOnceExecuteOnce(&g_HandleInitializer, initializeHandles, nullptr, nullptr);
}

static void wtsCloseVCM(WTSVirtualChannelManager* vcm, bool closeDrdynvc)
{
	WINPR_ASSERT(vcm);

	HashTable_Lock(g_ServerHandles);

/* clang analyzer does not like the check for INVALID_HANDLE_VALUE and considers the path not taken,
 * leading to false positives on memory leaks. */
#ifdef __clang_analyzer__
	const BOOL valid = vcm != nullptr;
#else
	const BOOL valid = (vcm != nullptr) && (vcm != INVALID_HANDLE_VALUE);
#endif
	if (valid)
	{
		HashTable_Remove(g_ServerHandles, (void*)(UINT_PTR)vcm->SessionId);

#if defined(WITH_RDPEUDP)
		if (vcm->client && vcm->client->context &&
		    !freerdp_multitransport_set_dvc_callbacks(vcm->client->context, nullptr, nullptr))
			WLog_WARN(TAG, "cannot remove the multitransport callbacks");
		if (vcm->tunnelBacklog)
		{
			wStream* held = nullptr;
			while ((held = Queue_Dequeue(vcm->tunnelBacklog)))
				Stream_Release(held);
			Queue_Free(vcm->tunnelBacklog);
		}

#endif
		HashTable_Free(vcm->dynamicVirtualChannels);

		if (vcm->drdynvc_channel)
		{
			if (closeDrdynvc)
				(void)WTSVirtualChannelClose(vcm->drdynvc_channel);
			vcm->drdynvc_channel = nullptr;
		}

		MessageQueue_Free(vcm->queue);
		free(vcm);
	}
	HashTable_Unlock(g_ServerHandles);
}

HANDLE WINAPI FreeRDP_WTSOpenServerA(LPSTR pServerName)
{
	wObject queueCallbacks = WINPR_C_ARRAY_INIT;

	rdpContext* context = WINPR_PACKED_ALIGN_CAST(rdpContext*, pServerName);

	if (!setup() || !context)
		return INVALID_HANDLE_VALUE;

	freerdp_peer* client = context->peer;

	if (!client)
	{
		SetLastError(ERROR_INVALID_DATA);
		return INVALID_HANDLE_VALUE;
	}

	WTSVirtualChannelManager* vcm = calloc(1, sizeof(WTSVirtualChannelManager));

	if (!vcm)
		goto fail;

	vcm->client = client;
	vcm->rdp = context->rdp;

	queueCallbacks.fnObjectFree = wts_virtual_channel_manager_free_message;
	vcm->queue = MessageQueue_New(&queueCallbacks);

	if (!vcm->queue)
		goto fail;

	vcm->dvc_channel_id_seq = 0;
	vcm->dynamicVirtualChannels = HashTable_New(TRUE);

	if (!vcm->dynamicVirtualChannels)
		goto fail;

	if (!HashTable_SetHashFunction(vcm->dynamicVirtualChannels, channelId_Hash))
		goto fail;

	{
		wObject* obj = HashTable_ValueObject(vcm->dynamicVirtualChannels);
		WINPR_ASSERT(obj);
		obj->fnObjectFree = array_channel_free;

		obj = HashTable_KeyObject(vcm->dynamicVirtualChannels);
		obj->fnObjectEquals = dynChannelMatch;
	}
	client->ReceiveChannelData = WTSReceiveChannelData;
#if defined(WITH_RDPEUDP)

	vcm->tunnelBacklog = Queue_New(FALSE, -1, -1);
	if (!vcm->tunnelBacklog)
		goto fail;
	{
		const FreeRDP_MultitransportDvcCallbacks callbacks = {
			.TunnelDataReceived = wts_tunnel_data_received,
			.TunnelStateChanged = wts_tunnel_state_changed,
		};
		if (!freerdp_multitransport_set_dvc_callbacks(context, &callbacks, vcm))
			goto fail;
	}
#endif
	{
		HashTable_Lock(g_ServerHandles);
		vcm->SessionId = g_SessionId++;
		const BOOL rc =
		    HashTable_Insert(g_ServerHandles, (void*)(UINT_PTR)vcm->SessionId, (void*)vcm);
		HashTable_Unlock(g_ServerHandles);
		if (!rc)
			goto fail;
	}

	HANDLE hServer = (HANDLE)vcm;
	return hServer;

fail:
	wtsCloseVCM(vcm, false);
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerExW(WINPR_ATTR_UNUSED LPWSTR pServerName)
{
	WLog_ERR("TODO", "TODO: implement");
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI FreeRDP_WTSOpenServerExA(LPSTR pServerName)
{
	return FreeRDP_WTSOpenServerA(pServerName);
}

VOID WINAPI FreeRDP_WTSCloseServer(HANDLE hServer)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	wtsCloseVCM(vcm, true);
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsW(WINPR_ATTR_UNUSED HANDLE hServer,
                                          WINPR_ATTR_UNUSED DWORD Reserved,
                                          WINPR_ATTR_UNUSED DWORD Version,
                                          WINPR_ATTR_UNUSED PWTS_SESSION_INFOW* ppSessionInfo,
                                          WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsA(WINPR_ATTR_UNUSED HANDLE hServer,
                                          WINPR_ATTR_UNUSED DWORD Reserved,
                                          WINPR_ATTR_UNUSED DWORD Version,
                                          WINPR_ATTR_UNUSED PWTS_SESSION_INFOA* ppSessionInfo,
                                          WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsExW(WINPR_ATTR_UNUSED HANDLE hServer,
                                            WINPR_ATTR_UNUSED DWORD* pLevel,
                                            WINPR_ATTR_UNUSED DWORD Filter,
                                            WINPR_ATTR_UNUSED PWTS_SESSION_INFO_1W* ppSessionInfo,
                                            WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateSessionsExA(WINPR_ATTR_UNUSED HANDLE hServer,
                                            WINPR_ATTR_UNUSED DWORD* pLevel,
                                            WINPR_ATTR_UNUSED DWORD Filter,
                                            WINPR_ATTR_UNUSED PWTS_SESSION_INFO_1A* ppSessionInfo,
                                            WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesW(WINPR_ATTR_UNUSED HANDLE hServer,
                                           WINPR_ATTR_UNUSED DWORD Reserved,
                                           WINPR_ATTR_UNUSED DWORD Version,
                                           WINPR_ATTR_UNUSED PWTS_PROCESS_INFOW* ppProcessInfo,
                                           WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesA(WINPR_ATTR_UNUSED HANDLE hServer,
                                           WINPR_ATTR_UNUSED DWORD Reserved,
                                           WINPR_ATTR_UNUSED DWORD Version,
                                           WINPR_ATTR_UNUSED PWTS_PROCESS_INFOA* ppProcessInfo,
                                           WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSTerminateProcess(WINPR_ATTR_UNUSED HANDLE hServer,
                                        WINPR_ATTR_UNUSED DWORD ProcessId,
                                        WINPR_ATTR_UNUSED DWORD ExitCode)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQuerySessionInformationW(WINPR_ATTR_UNUSED HANDLE hServer,
                                                WINPR_ATTR_UNUSED DWORD SessionId,
                                                WINPR_ATTR_UNUSED WTS_INFO_CLASS WTSInfoClass,
                                                WINPR_ATTR_UNUSED LPWSTR* ppBuffer,
                                                WINPR_ATTR_UNUSED DWORD* pBytesReturned)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQuerySessionInformationA(HANDLE hServer, WINPR_ATTR_UNUSED DWORD SessionId,
                                                WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer,
                                                DWORD* pBytesReturned)
{
	DWORD BytesReturned = 0;
	WTSVirtualChannelManager* vcm = nullptr;
	vcm = (WTSVirtualChannelManager*)hServer;

	if (!vcm)
		return FALSE;

	if (WTSInfoClass == WTSSessionId)
	{
		ULONG* pBuffer = nullptr;
		BytesReturned = sizeof(ULONG);
		pBuffer = (ULONG*)malloc(sizeof(BytesReturned));

		if (!pBuffer)
		{
			SetLastError(g_err_oom);
			return FALSE;
		}

		*pBuffer = vcm->SessionId;
		*ppBuffer = (LPSTR)pBuffer;
		*pBytesReturned = BytesReturned;
		return TRUE;
	}

	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserConfigW(WINPR_ATTR_UNUSED LPWSTR pServerName,
                                        WINPR_ATTR_UNUSED LPWSTR pUserName,
                                        WINPR_ATTR_UNUSED WTS_CONFIG_CLASS WTSConfigClass,
                                        WINPR_ATTR_UNUSED LPWSTR* ppBuffer,
                                        WINPR_ATTR_UNUSED DWORD* pBytesReturned)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserConfigA(WINPR_ATTR_UNUSED LPSTR pServerName,
                                        WINPR_ATTR_UNUSED LPSTR pUserName,
                                        WINPR_ATTR_UNUSED WTS_CONFIG_CLASS WTSConfigClass,
                                        WINPR_ATTR_UNUSED LPSTR* ppBuffer,
                                        WINPR_ATTR_UNUSED DWORD* pBytesReturned)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetUserConfigW(WINPR_ATTR_UNUSED LPWSTR pServerName,
                                      WINPR_ATTR_UNUSED LPWSTR pUserName,
                                      WINPR_ATTR_UNUSED WTS_CONFIG_CLASS WTSConfigClass,
                                      WINPR_ATTR_UNUSED LPWSTR pBuffer,
                                      WINPR_ATTR_UNUSED DWORD DataLength)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetUserConfigA(WINPR_ATTR_UNUSED LPSTR pServerName,
                                      WINPR_ATTR_UNUSED LPSTR pUserName,
                                      WINPR_ATTR_UNUSED WTS_CONFIG_CLASS WTSConfigClass,
                                      WINPR_ATTR_UNUSED LPSTR pBuffer,
                                      WINPR_ATTR_UNUSED DWORD DataLength)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI
FreeRDP_WTSSendMessageW(WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED DWORD SessionId,
                        WINPR_ATTR_UNUSED LPWSTR pTitle, WINPR_ATTR_UNUSED DWORD TitleLength,
                        WINPR_ATTR_UNUSED LPWSTR pMessage, WINPR_ATTR_UNUSED DWORD MessageLength,
                        WINPR_ATTR_UNUSED DWORD Style, WINPR_ATTR_UNUSED DWORD Timeout,
                        WINPR_ATTR_UNUSED DWORD* pResponse, WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI
FreeRDP_WTSSendMessageA(WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED DWORD SessionId,
                        WINPR_ATTR_UNUSED LPSTR pTitle, WINPR_ATTR_UNUSED DWORD TitleLength,
                        WINPR_ATTR_UNUSED LPSTR pMessage, WINPR_ATTR_UNUSED DWORD MessageLength,
                        WINPR_ATTR_UNUSED DWORD Style, WINPR_ATTR_UNUSED DWORD Timeout,
                        WINPR_ATTR_UNUSED DWORD* pResponse, WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSDisconnectSession(WINPR_ATTR_UNUSED HANDLE hServer,
                                         WINPR_ATTR_UNUSED DWORD SessionId,
                                         WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSLogoffSession(WINPR_ATTR_UNUSED HANDLE hServer,
                                     WINPR_ATTR_UNUSED DWORD SessionId,
                                     WINPR_ATTR_UNUSED BOOL bWait)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSShutdownSystem(WINPR_ATTR_UNUSED HANDLE hServer,
                                      WINPR_ATTR_UNUSED DWORD ShutdownFlag)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSWaitSystemEvent(WINPR_ATTR_UNUSED HANDLE hServer,
                                       WINPR_ATTR_UNUSED DWORD EventMask,
                                       WINPR_ATTR_UNUSED DWORD* pEventFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

static void peer_channel_queue_free_message(void* obj)
{
	wMessage* msg = (wMessage*)obj;
	if (!msg)
		return;

	free(msg->context);
	msg->context = nullptr;
}

static rdpPeerChannel* channel_new(WTSVirtualChannelManager* vcm, freerdp_peer* client,
                                   UINT32 ChannelId, UINT16 index, UINT16 type, size_t chunkSize,
                                   const char* name, UINT32 flags)
{
	wObject queueCallbacks = WINPR_C_ARRAY_INIT;
	queueCallbacks.fnObjectFree = peer_channel_queue_free_message;

	rdpPeerChannel* channel =
	    server_channel_common_new(client, index, ChannelId, chunkSize, &queueCallbacks, name);

	WINPR_ASSERT(vcm);
	WINPR_ASSERT(client);

	if (!channel)
		goto fail;

	channel->vcm = vcm;
	channel->channelType = type;
	channel->creationStatus =
	    (type == RDP_PEER_CHANNEL_TYPE_SVC) ? ERROR_SUCCESS : ERROR_OPERATION_IN_PROGRESS;
	channel->channelFlags = flags;

	return channel;
fail:
	channel_free(channel);
	return nullptr;
}

static HANDLE WINAPI FreeRDP_WTSVirtualChannelOpenStatic(HANDLE hServer,
                                                         WINPR_ATTR_UNUSED DWORD SessionId,
                                                         LPSTR pVirtualName, UINT32 Flags)
{
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)hServer;
	if (!vcm)
	{
		SetLastError(ERROR_INVALID_DATA);
		return nullptr;
	}

	freerdp_peer* client = vcm->client;
	WINPR_ASSERT(client);

	rdpContext* context = client->context;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	WINPR_ASSERT(context->settings);

	rdpMcs* mcs = context->rdp->mcs;
	WINPR_ASSERT(mcs);

	size_t length = strnlen(pVirtualName, CHANNEL_NAME_LEN + 1);
	if (length > CHANNEL_NAME_LEN)
	{
		SetLastError(ERROR_NOT_FOUND);
		return nullptr;
	}

	rdpMcsChannel* joined_channel = nullptr;
	UINT32 index = 0;
	for (; index < mcs->channelCount; index++)
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
		return nullptr;
	}

	rdpPeerChannel* channel = (rdpPeerChannel*)joined_channel->handle;
	if (!channel)
	{
		const UINT32 VCChunkSize =
		    freerdp_settings_get_uint32(context->settings, FreeRDP_VCChunkSize);

		WINPR_ASSERT(index <= UINT16_MAX);
		channel = channel_new(vcm, client, joined_channel->ChannelId, (UINT16)index,
		                      RDP_PEER_CHANNEL_TYPE_SVC, VCChunkSize, pVirtualName, Flags);

		if (!channel)
			goto fail;

		joined_channel->handle = channel;
	}

	HANDLE hChannelHandle = (HANDLE)channel;
	return hChannelHandle;
fail:
	channel_free(channel);
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return nullptr;
}

HANDLE WINAPI FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	return FreeRDP_WTSVirtualChannelOpenStatic(hServer, SessionId, pVirtualName, 0);
}

HANDLE WINAPI FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	wStream* s = nullptr;
	rdpPeerChannel* channel = nullptr;
	BOOL joined = FALSE;
	ULONG written = 0;

	if (!setup())
		return nullptr;

	if (SessionId == WTS_CURRENT_SESSION)
		return nullptr;

	HashTable_Lock(g_ServerHandles);
	WTSVirtualChannelManager* vcm = (WTSVirtualChannelManager*)HashTable_GetItemValue(
	    g_ServerHandles, (void*)(UINT_PTR)SessionId);

	if (!vcm)
		goto end;

	if (!(flags & WTS_CHANNEL_OPTION_DYNAMIC))
	{
		HashTable_Unlock(g_ServerHandles);
		return FreeRDP_WTSVirtualChannelOpenStatic((HANDLE)vcm, SessionId, pVirtualName, flags);
	}

	freerdp_peer* client = vcm->client;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);

	rdpMcs* mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);

	for (UINT32 index = 0; index < mcs->channelCount; index++)
	{
		rdpMcsChannel* mchannel = &mcs->channels[index];
		if (mchannel->joined &&
		    (strncmp(mchannel->Name, DRDYNVC_SVC_CHANNEL_NAME, CHANNEL_NAME_LEN + 1) == 0))
		{
			joined = TRUE;
			break;
		}
	}

	if (!joined)
	{
		SetLastError(ERROR_NOT_FOUND);
		goto end;
	}

	if (!vcm->drdynvc_channel || (vcm->drdynvc_state != DRDYNVC_STATE_READY))
	{
		SetLastError(ERROR_NOT_READY);
		goto end;
	}

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->settings);

	const UINT32 VCChunkSize =
	    freerdp_settings_get_uint32(client->context->settings, FreeRDP_VCChunkSize);
	channel =
	    channel_new(vcm, client, 0, 0, RDP_PEER_CHANNEL_TYPE_DVC, VCChunkSize, pVirtualName, flags);

	if (!channel)
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		goto end;
	}

	const LONG hdl = InterlockedIncrement(&vcm->dvc_channel_id_seq);
	channel->channelId = WINPR_ASSERTING_INT_CAST(uint32_t, hdl);

	if (!HashTable_Insert(vcm->dynamicVirtualChannels, &channel->channelId, channel))
	{
		channel_free(channel);
		channel = nullptr;
		goto fail;
	}
	s = Stream_New(nullptr, 64);

	if (!s)
		goto fail;

	if (!wts_write_drdynvc_create_request(s, channel->channelId, pVirtualName))
		goto fail;

#if defined(WITH_RDPEUDP)
	/* After Soft-Sync a new channel lives on the tunnel: its Create Request goes there, and the
	 * client answers on the same transport. */
	channel->tunnel = (vcm->softSyncState != SOFT_SYNC_STATE_NONE) && vcm->tunnelReady;
	if (channel->tunnel)
	{
		if (!wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), Stream_GetPosition(s), TRUE))
			goto fail;
	}
	else
#endif
	{
		const size_t pos = Stream_GetPosition(s);
		WINPR_ASSERT(pos <= UINT32_MAX);
		if (!WTSVirtualChannelWrite(vcm->drdynvc_channel, Stream_BufferAs(s, char), (UINT32)pos,
		                            &written))
			goto fail;
	}

end:
	Stream_Free(s, TRUE);
	HashTable_Unlock(g_ServerHandles);
	return channel;

fail:
	Stream_Free(s, TRUE);
	if (channel)
		HashTable_Remove(vcm->dynamicVirtualChannels, &channel->channelId);
	HashTable_Unlock(g_ServerHandles);

	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return nullptr;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	wStream* s = nullptr;
	rdpMcs* mcs = nullptr;

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
				rdpPeerChannel* peerChannel = (rdpPeerChannel*)cur->handle;
				channel_free(peerChannel);
				cur->handle = nullptr;
			}
		}
		else
		{
			if (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED)
			{
				ULONG written = 0;
				s = Stream_New(nullptr, 8);

				if (!s)
				{
					WLog_ERR(TAG, "Stream_New failed!");
					ret = FALSE;
				}
				else
				{
					wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, channel->channelId);

					const size_t pos = Stream_GetPosition(s);
					WINPR_ASSERT(pos <= UINT32_MAX);
#if defined(WITH_RDPEUDP)
					if (channel->tunnel)
						ret = wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), pos, TRUE);
					else
						ret = WTSVirtualChannelWrite(vcm->drdynvc_channel, Stream_BufferAs(s, char),
						                             (UINT32)pos, &written);
#else
					ret = WTSVirtualChannelWrite(vcm->drdynvc_channel, Stream_BufferAs(s, char),
					                             (UINT32)pos, &written);
#endif
					Stream_Free(s, TRUE);
				}
			}
			HashTable_Remove(vcm->dynamicVirtualChannels, &channel->channelId);
		}
	}

	return ret;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, WINPR_ATTR_UNUSED ULONG TimeOut,
                                          PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	BYTE* buffer = nullptr;
	wMessage message = WINPR_C_ARRAY_INIT;
	wtsChannelMessage* messageCtx = nullptr;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;

	WINPR_ASSERT(channel);

	if (!MessageQueue_Peek(channel->queue, &message, FALSE))
	{
		SetLastError(ERROR_NO_DATA);
		*pBytesRead = 0;
		return FALSE;
	}

	messageCtx = message.context;

	if (messageCtx == nullptr)
		return FALSE;

	buffer = (BYTE*)(messageCtx + 1);
	*pBytesRead = messageCtx->length - messageCtx->offset;

	if (Buffer == nullptr || BufferSize == 0)
	{
		return TRUE;
	}

	if (*pBytesRead > BufferSize)
		*pBytesRead = BufferSize;

	CopyMemory(Buffer, buffer + messageCtx->offset, *pBytesRead);
	messageCtx->offset += *pBytesRead;

	if (messageCtx->offset >= messageCtx->length)
	{
		const int rc = MessageQueue_Peek(channel->queue, &message, TRUE);
		peer_channel_queue_free_message(&message);
		if (rc < 0)
			return FALSE;
	}

	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG uLength,
                                           PULONG pBytesWritten)
{
	wStream* s = nullptr;
	int cbLen = 0;
	int cbChId = 0;
	int first = 0;
	BYTE* buffer = nullptr;
	size_t totalWritten = 0;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;
	BOOL ret = FALSE;

	if (!channel)
		return FALSE;

	EnterCriticalSection(&channel->writeLock);
	WINPR_ASSERT(channel->vcm);
	if (channel->channelType == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		buffer = (BYTE*)malloc(uLength);

		if (!buffer)
		{
			SetLastError(g_err_oom);
			goto fail;
		}

		CopyMemory(buffer, Buffer, uLength);
		totalWritten = uLength;
		if (!wts_queue_send_item(channel, buffer, uLength))
			goto fail;
	}
	else if (!channel->vcm->drdynvc_channel || (channel->vcm->drdynvc_state != DRDYNVC_STATE_READY))
	{
		DEBUG_DVC("drdynvc not ready");
		goto fail;
	}
	else
	{
		first = TRUE;

		size_t Length = uLength;
		while (Length > 0)
		{
			s = Stream_New(nullptr, DVC_MAX_DATA_PDU_SIZE);

			if (!s)
			{
				WLog_ERR(TAG, "Stream_New failed!");
				SetLastError(g_err_oom);
				goto fail;
			}

			buffer = Stream_Buffer(s);
			Stream_Seek_UINT8(s);
			cbChId = wts_write_variable_uint(s, channel->channelId);

			if (first && (Length > Stream_GetRemainingLength(s)))
			{
				cbLen = wts_write_variable_uint(s, WINPR_ASSERTING_INT_CAST(uint32_t, Length));
				buffer[0] = ((DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId) & 0xFF;
			}
			else
			{
				buffer[0] = ((DATA_PDU << 4) | cbChId) & 0xFF;
			}

			first = FALSE;
			size_t written = Stream_GetRemainingLength(s);

			if (written > Length)
				written = Length;

			Stream_Write(s, Buffer, written);
			const size_t length = Stream_GetPosition(s);
			Stream_Free(s, FALSE);
			if (length > UINT32_MAX)
				goto fail;
			Length -= written;
			Buffer += written;
			totalWritten += written;
#if defined(WITH_RDPEUDP)
			if (!wts_queue_send_item_on(channel->vcm->drdynvc_channel, buffer,
			                            WINPR_ASSERTING_INT_CAST(UINT32, length), channel->tunnel))
#else
			if (!wts_queue_send_item(channel->vcm->drdynvc_channel, buffer, (UINT32)length))
#endif
				goto fail;
		}
	}

	if (pBytesWritten)
		*pBytesWritten = WINPR_ASSERTING_INT_CAST(uint32_t, totalWritten);

	ret = TRUE;
fail:
	LeaveCriticalSection(&channel->writeLock);
	return ret;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeInput(WINPR_ATTR_UNUSED HANDLE hChannelHandle)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeOutput(WINPR_ATTR_UNUSED HANDLE hChannelHandle)
{
	WLog_ERR("TODO", "TODO: implement");
	return TRUE;
}

BOOL WINAPI FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                                           PVOID* ppBuffer, DWORD* pBytesReturned)
{
	void* pfd = nullptr;
	BOOL bval = 0;
	void* fds[10] = WINPR_C_ARRAY_INIT;
	HANDLE hEvent = nullptr;
	int fds_count = 0;
	BOOL status = FALSE;
	rdpPeerChannel* channel = (rdpPeerChannel*)hChannelHandle;

	WINPR_ASSERT(channel);

	switch ((UINT32)WtsVirtualClass)
	{
		case WTSVirtualFileHandle:
			hEvent = MessageQueue_Event(channel->queue);
			pfd = GetEventWaitObject(hEvent);

			if (pfd)
			{
				fds[fds_count] = pfd;
				(fds_count)++;
			}

			*ppBuffer = malloc(sizeof(void*));

			if (!*ppBuffer)
			{
				SetLastError(g_err_oom);
			}
			else
			{
				CopyMemory(*ppBuffer, (void*)&fds[0], sizeof(void*));
				*pBytesReturned = sizeof(void*);
				status = TRUE;
			}

			break;

		case WTSVirtualEventHandle:
			hEvent = MessageQueue_Event(channel->queue);

			*ppBuffer = malloc(sizeof(HANDLE));

			if (!*ppBuffer)
			{
				SetLastError(g_err_oom);
			}
			else
			{
				CopyMemory(*ppBuffer, (void*)&hEvent, sizeof(HANDLE));
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
						*ppBuffer = nullptr;
						*pBytesReturned = 0;
						return FALSE;
				}
			}

			*ppBuffer = malloc(sizeof(BOOL));

			if (!*ppBuffer)
			{
				SetLastError(g_err_oom);
				status = FALSE;
			}
			else
			{
				CopyMemory(*ppBuffer, &bval, sizeof(BOOL));
				*pBytesReturned = sizeof(BOOL);
			}

			break;
		case WTSVirtualChannelOpenStatus:
		{
			INT32 value = channel->creationStatus;
			status = TRUE;

			*ppBuffer = malloc(sizeof(value));
			if (!*ppBuffer)
			{
				SetLastError(g_err_oom);
				status = FALSE;
			}
			else
			{
				CopyMemory(*ppBuffer, &value, sizeof(value));
				*pBytesReturned = sizeof(value);
			}
			break;
		}
		default:
			break;
	}

	return status;
}

VOID WINAPI FreeRDP_WTSFreeMemory(PVOID pMemory)
{
	free(pMemory);
}

BOOL WINAPI FreeRDP_WTSFreeMemoryExW(WINPR_ATTR_UNUSED WTS_TYPE_CLASS WTSTypeClass,
                                     WINPR_ATTR_UNUSED PVOID pMemory,
                                     WINPR_ATTR_UNUSED ULONG NumberOfEntries)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSFreeMemoryExA(WINPR_ATTR_UNUSED WTS_TYPE_CLASS WTSTypeClass,
                                     WINPR_ATTR_UNUSED PVOID pMemory,
                                     WINPR_ATTR_UNUSED ULONG NumberOfEntries)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSRegisterSessionNotification(WINPR_ATTR_UNUSED HWND hWnd,
                                                   WINPR_ATTR_UNUSED DWORD dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotification(WINPR_ATTR_UNUSED HWND hWnd)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSRegisterSessionNotificationEx(WINPR_ATTR_UNUSED HANDLE hServer,
                                                     WINPR_ATTR_UNUSED HWND hWnd,
                                                     WINPR_ATTR_UNUSED DWORD dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotificationEx(WINPR_ATTR_UNUSED HANDLE hServer,
                                                       WINPR_ATTR_UNUSED HWND hWnd)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryUserToken(WINPR_ATTR_UNUSED ULONG SessionId,
                                      WINPR_ATTR_UNUSED PHANDLE phToken)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesExW(WINPR_ATTR_UNUSED HANDLE hServer,
                                             WINPR_ATTR_UNUSED DWORD* pLevel,
                                             WINPR_ATTR_UNUSED DWORD SessionId,
                                             WINPR_ATTR_UNUSED LPWSTR* ppProcessInfo,
                                             WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateProcessesExA(WINPR_ATTR_UNUSED HANDLE hServer,
                                             WINPR_ATTR_UNUSED DWORD* pLevel,
                                             WINPR_ATTR_UNUSED DWORD SessionId,
                                             WINPR_ATTR_UNUSED LPSTR* ppProcessInfo,
                                             WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateListenersW(WINPR_ATTR_UNUSED HANDLE hServer,
                                           WINPR_ATTR_UNUSED PVOID pReserved,
                                           WINPR_ATTR_UNUSED DWORD Reserved,
                                           WINPR_ATTR_UNUSED PWTSLISTENERNAMEW pListeners,
                                           WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSEnumerateListenersA(WINPR_ATTR_UNUSED HANDLE hServer,
                                           WINPR_ATTR_UNUSED PVOID pReserved,
                                           WINPR_ATTR_UNUSED DWORD Reserved,
                                           WINPR_ATTR_UNUSED PWTSLISTENERNAMEA pListeners,
                                           WINPR_ATTR_UNUSED DWORD* pCount)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryListenerConfigW(WINPR_ATTR_UNUSED HANDLE hServer,
                                            WINPR_ATTR_UNUSED PVOID pReserved,
                                            WINPR_ATTR_UNUSED DWORD Reserved,
                                            WINPR_ATTR_UNUSED LPWSTR pListenerName,
                                            WINPR_ATTR_UNUSED PWTSLISTENERCONFIGW pBuffer)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSQueryListenerConfigA(WINPR_ATTR_UNUSED HANDLE hServer,
                                            WINPR_ATTR_UNUSED PVOID pReserved,
                                            WINPR_ATTR_UNUSED DWORD Reserved,
                                            WINPR_ATTR_UNUSED LPSTR pListenerName,
                                            WINPR_ATTR_UNUSED PWTSLISTENERCONFIGA pBuffer)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSCreateListenerW(WINPR_ATTR_UNUSED HANDLE hServer,
                                       WINPR_ATTR_UNUSED PVOID pReserved,
                                       WINPR_ATTR_UNUSED DWORD Reserved,
                                       WINPR_ATTR_UNUSED LPWSTR pListenerName,
                                       WINPR_ATTR_UNUSED PWTSLISTENERCONFIGW pBuffer,
                                       WINPR_ATTR_UNUSED DWORD flag)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSCreateListenerA(WINPR_ATTR_UNUSED HANDLE hServer,
                                       WINPR_ATTR_UNUSED PVOID pReserved,
                                       WINPR_ATTR_UNUSED DWORD Reserved,
                                       WINPR_ATTR_UNUSED LPSTR pListenerName,
                                       WINPR_ATTR_UNUSED PWTSLISTENERCONFIGA pBuffer,
                                       WINPR_ATTR_UNUSED DWORD flag)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetListenerSecurityW(
    WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED PVOID pReserved,
    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPWSTR pListenerName,
    WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
    WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSSetListenerSecurityA(
    WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED PVOID pReserved,
    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPSTR pListenerName,
    WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
    WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSGetListenerSecurityW(
    WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED PVOID pReserved,
    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPWSTR pListenerName,
    WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
    WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor, WINPR_ATTR_UNUSED DWORD nLength,
    WINPR_ATTR_UNUSED LPDWORD lpnLengthNeeded)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSGetListenerSecurityA(
    WINPR_ATTR_UNUSED HANDLE hServer, WINPR_ATTR_UNUSED PVOID pReserved,
    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPSTR pListenerName,
    WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
    WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor, WINPR_ATTR_UNUSED DWORD nLength,
    WINPR_ATTR_UNUSED LPDWORD lpnLengthNeeded)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL CDECL FreeRDP_WTSEnableChildSessions(WINPR_ATTR_UNUSED BOOL bEnable)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL CDECL FreeRDP_WTSIsChildSessionsEnabled(WINPR_ATTR_UNUSED PBOOL pbEnabled)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL CDECL FreeRDP_WTSGetChildSessionId(WINPR_ATTR_UNUSED PULONG pSessionId)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

DWORD WINAPI FreeRDP_WTSGetActiveConsoleSessionId(void)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0xFFFFFFFF;
}
BOOL WINAPI FreeRDP_WTSLogoffUser(WINPR_ATTR_UNUSED HANDLE hServer)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

BOOL WINAPI FreeRDP_WTSLogonUser(WINPR_ATTR_UNUSED HANDLE hServer,
                                 WINPR_ATTR_UNUSED LPCSTR username,
                                 WINPR_ATTR_UNUSED LPCSTR password, WINPR_ATTR_UNUSED LPCSTR domain)
{
	WLog_ERR("TODO", "TODO: implement");
	return FALSE;
}

void server_channel_common_free(rdpPeerChannel* channel)
{
	if (!channel)
		return;
	MessageQueue_Free(channel->queue);
	Stream_Free(channel->receiveData, TRUE);
	DeleteCriticalSection(&channel->writeLock);
	free(channel);
}

rdpPeerChannel* server_channel_common_new(freerdp_peer* client, UINT16 index, UINT32 channelId,
                                          size_t chunkSize, const wObject* callback,
                                          const char* name)
{
	rdpPeerChannel* channel = (rdpPeerChannel*)calloc(1, sizeof(rdpPeerChannel));
	if (!channel)
		return nullptr;

	InitializeCriticalSection(&channel->writeLock);

	channel->receiveData = Stream_New(nullptr, chunkSize);
	if (!channel->receiveData)
		goto fail;

	channel->queue = MessageQueue_New(callback);
	if (!channel->queue)
		goto fail;

	channel->index = index;
	channel->client = client;
	channel->channelId = channelId;
	strncpy(channel->channelName, name, ARRAYSIZE(channel->channelName) - 1);
	return channel;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	server_channel_common_free(channel);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}
