/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic virtual channels of a server on the reliable UDP tunnel
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

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/collections.h>
#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/multitransport.h>
#include <freerdp/channels/drdynvc.h>
#include <freerdp/utils/drdynvc.h>

#include "rdp.h"
#include "server.h"
#include "server_tunnel.h"

#define TAG FREERDP_TAG("core.server")

/* Client PDUs from the tunnel that the server keeps until the client sends its Soft-Sync
 * Response. */
#define SOFT_SYNC_MAX_BACKLOG 1024

/* vcm->queue message ids: where a PDU for the drdynvc channel goes */
typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
{
	WTS_QUEUE_TCP = 0,
	WTS_QUEUE_TUNNEL = 1
} WTS_QUEUE_ID;

/* Soft-Sync ([MS-RDPEDYC] 3.1.5.3) */
typedef enum WINPR_C23_ENUM_TYPE(uint8_t)
{
	SOFT_SYNC_STATE_NONE = 0,     /* every dynamic channel on TCP */
	SOFT_SYNC_STATE_SENT = 1,     /* the server sent the request and writes moved channels on UDP */
	SOFT_SYNC_STATE_ANSWERED = 2, /* the client answered, the server reads its tunnel data */
} WTS_SOFT_SYNC_STATE;

/* On the thread that runs the connection */
struct s_wts_tunnel
{
	wtsTunnelReadPduFn readPdu;
	BOOL tunnelReady;
	BOOL softSyncDue; /* a channel opened, Soft-Sync can move it */
	WTS_SOFT_SYNC_STATE softSyncState;
	BOOL clientOnTunnel;   /* the Soft-Sync Response named the tunnel the client writes on */
	wQueue* tunnelBacklog; /* client PDUs from the tunnel before its Soft-Sync Response */
	BOOL written;          /* the queue had PDUs for the tunnel, flush them */
};

WINPR_ATTR_NODISCARD
static BOOL wts_tunnel_post(rdpPeerChannel* channel, BYTE* buffer, UINT32 length, BOOL tunnel)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_ASSERT(channel->channelId <= UINT16_MAX);

	const UINT16 channelId = (UINT16)channel->channelId;
	const WTS_QUEUE_ID id = tunnel ? WTS_QUEUE_TUNNEL : WTS_QUEUE_TCP;
	return MessageQueue_Post(channel->vcm->queue, (void*)(UINT_PTR)channelId, id, (void*)buffer,
	                         (void*)(UINT_PTR)length);
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
	if (!wts_tunnel_post(vcm->drdynvc_channel, buffer, WINPR_ASSERTING_INT_CAST(UINT32, length),
	                     tunnel))
	{
		free(buffer);
		return FALSE;
	}
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): the queue takes ownership of buffer
	return TRUE;
}

/* One client PDU that arrived on the tunnel. Capabilities and Soft-Sync travel only on the drdynvc
 * static channel. */
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
	const UINT8 Cmd = (UINT8)((value & 0xf0u) >> 4u);
	const UINT8 Sp = (UINT8)((value & 0x0cu) >> 2u);
	const UINT8 cbChId = (UINT8)(value & 0x03u);
	switch (Cmd)
	{
		case CAPABILITY_REQUEST_PDU:
		case SOFT_SYNC_REQUEST_PDU:
		case SOFT_SYNC_RESPONSE_PDU:
			WLog_WARN(TAG, "a %s on the tunnel, ending the connection",
			          drdynvc_get_packet_type(Cmd));
			return FALSE;
		default:
			return vcm->tunnel->readPdu(vcm->drdynvc_channel, s, Cmd, Sp, cbChId);
	}
}

/* [MS-RDPEDYC] 2.2.5.2 DYNVC_SOFT_SYNC_RESPONSE: the client reads and writes the tunnels it
 * names, and the server reads them from now on (3.3.5.3.2). */
BOOL wts_tunnel_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s)
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
	{
		if (Stream_Get_UINT32(s) == TUNNELTYPE_UDPFECR)
			reliable = TRUE;
	}

	if (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_SENT)
	{
		WLog_WARN(TAG, "unexpected Soft-Sync Response");
		return TRUE;
	}
	vcm->tunnel->softSyncState = SOFT_SYNC_STATE_ANSWERED;
	vcm->tunnel->clientOnTunnel = reliable;
	WLog_INFO(TAG, "Soft-Sync Response: the client %s the reliable UDP tunnel",
	          reliable ? "writes on" : "does not use");

	/* [MS-RDPEDYC] 3.2.5.3.2: the response names every tunnel the client writes on. The server
	 * continues to write on the tunnel that its request named. The server reads only what the
	 * client named. */
	if (!reliable && (Queue_Count(vcm->tunnel->tunnelBacklog) > 0))
	{
		WLog_ERR(TAG, "the client wrote on a tunnel its Soft-Sync Response does not name");
		return FALSE;
	}

	wStream* held = nullptr;
	while ((held = Queue_Dequeue(vcm->tunnel->tunnelBacklog)))
	{
		const BOOL rc = wts_read_tunnel_pdu(vcm, Stream_Buffer(held), Stream_Length(held));
		Stream_Release(held);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

/* Moves every open dynamic channel onto the tunnel with a Soft-Sync Request ([MS-RDPEDYC]
 * 3.3.5.3.1). The server sends the request on TCP after everything that the channels queued there.
 * Then the server writes the channels on the tunnel. The server keeps the writeLock of each channel
 * during both steps, so no message of a channel crosses the switch. */
WINPR_ATTR_NODISCARD
static BOOL wts_soft_sync(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);

	if ((vcm->tunnel->softSyncState != SOFT_SYNC_STATE_NONE) || !vcm->tunnel->tunnelReady ||
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
		channels = (rdpPeerChannel**)calloc(total, sizeof(rdpPeerChannel*));
		if (!channels)
			goto out;
	}
	for (size_t x = 0; x < total; x++)
	{
		rdpPeerChannel* channel =
		    HashTable_GetItemValue(vcm->dynamicVirtualChannels, (void*)keys[x]);
		/* If Soft-Sync moves the others now, a channel that the client did not answer yet stays on
		 * TCP permanently. The client's answer tries again. */
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

	/* DYNVC_SOFT_SYNC_REQUEST: Length counts from the Length field */
	const size_t listLength = 4 + 2 + sizeof(UINT32) * count;
	s = Stream_New(nullptr, 2 + 4 + 2 + 2 + listLength);
	if (!s)
		goto out;
	/* the Stream_Write macros evaluate their value more than once */
	const UINT8 header = (UINT8)((UINT32)SOFT_SYNC_REQUEST_PDU << 4u);
	const UINT16 flags =
	    (UINT16)((UINT32)SOFT_SYNC_TCP_FLUSHED | (UINT32)SOFT_SYNC_CHANNEL_LIST_PRESENT);
	const UINT32 length = WINPR_ASSERTING_INT_CAST(UINT32, 4 + 2 + 2 + listLength);
	const UINT16 dvcs = WINPR_ASSERTING_INT_CAST(UINT16, count);
	Stream_Write_UINT8(s, header);
	Stream_Write_UINT8(s, 0);                   /* Pad */
	Stream_Write_UINT32(s, length);             /* Length */
	Stream_Write_UINT16(s, flags);              /* Flags */
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
		vcm->tunnel->softSyncState = SOFT_SYNC_STATE_SENT;
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
	free((void*)channels);
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

	/* [MS-RDPEDYC] 3.3.5.3.2: the server reads nothing from the tunnel before the Soft-Sync
	 * Response */
	if (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_ANSWERED)
	{
		if (Queue_Count(vcm->tunnel->tunnelBacklog) >= SOFT_SYNC_MAX_BACKLOG)
		{
			WLog_ERR(TAG, "tunnel data keeps coming without a Soft-Sync Response");
			Stream_Release(s);
			return FALSE;
		}
		/* the backlog owns s now */
		if (!Queue_Enqueue(vcm->tunnel->tunnelBacklog, s))
		{
			Stream_Release(s);
			return FALSE;
		}
		return TRUE;
	}
	BOOL rc = FALSE;
	if (!vcm->tunnel->clientOnTunnel)
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
	vcm->tunnel->tunnelReady = (state == FREERDP_MULTITRANSPORT_TUNNEL_READY);
	return wts_soft_sync(vcm);
}

BOOL wts_tunnel_new(WTSVirtualChannelManager* vcm, rdpContext* context, wtsTunnelReadPduFn readPdu)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(readPdu);

	vcm->tunnel = calloc(1, sizeof(struct s_wts_tunnel));
	if (!vcm->tunnel)
		return FALSE;
	vcm->tunnel->readPdu = readPdu;
	vcm->tunnel->tunnelBacklog = Queue_New(FALSE, -1, -1);
	if (!vcm->tunnel->tunnelBacklog)
		return FALSE;

	const FreeRDP_MultitransportDvcCallbacks callbacks = {
		.TunnelDataReceived = wts_tunnel_data_received,
		.TunnelStateChanged = wts_tunnel_state_changed,
	};
	return freerdp_multitransport_set_dvc_callbacks(context, &callbacks, vcm);
}

void wts_tunnel_free(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);

	if (vcm->client && vcm->client->context &&
	    !freerdp_multitransport_set_dvc_callbacks(vcm->client->context, nullptr, nullptr))
		WLog_WARN(TAG, "cannot remove the multitransport callbacks");
	if (!vcm->tunnel)
		return;
	if (vcm->tunnel->tunnelBacklog)
	{
		wStream* held = nullptr;
		while ((held = Queue_Dequeue(vcm->tunnel->tunnelBacklog)))
			Stream_Release(held);
		Queue_Free(vcm->tunnel->tunnelBacklog);
	}
	free(vcm->tunnel);
	vcm->tunnel = nullptr;
}

void wts_tunnel_channel_opened(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);
	vcm->tunnel->softSyncDue = TRUE;
}

BOOL wts_tunnel_is_soft_sync_response(UINT8 Cmd)
{
	return Cmd == SOFT_SYNC_RESPONSE_PDU;
}

BOOL wts_tunnel_before_drain(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);

	vcm->tunnel->written = FALSE;
	if (!vcm->tunnel->softSyncDue)
		return TRUE;
	vcm->tunnel->softSyncDue = FALSE;
	return wts_soft_sync(vcm);
}

BOOL wts_tunnel_send_message(WTSVirtualChannelManager* vcm, UINT32 id, const BYTE* buffer,
                             UINT32 length, BOOL* status)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);
	WINPR_ASSERT(status);

	if (id != WTS_QUEUE_TUNNEL)
		return FALSE;

	WINPR_ASSERT(vcm->client);
	if (freerdp_multitransport_send_dvc(vcm->client->context, TUNNELTYPE_UDPFECR, buffer, length))
		vcm->tunnel->written = TRUE;
	else
	{
		WLog_ERR(TAG, "cannot write a dynamic channel PDU on the UDP tunnel");
		*status = FALSE;
	}
	return TRUE;
}

BOOL wts_tunnel_after_drain(WTSVirtualChannelManager* vcm, BOOL status)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);

	if (!status || !vcm->tunnel->written)
		return status;

	WINPR_ASSERT(vcm->client->context->rdp);
	if (!multitransport_flush_dvc(vcm->client->context->rdp->multitransport))
	{
		WLog_ERR(TAG, "cannot write dynamic channel PDUs on the UDP tunnel");
		return FALSE;
	}
	return TRUE;
}

BOOL wts_tunnel_open_channel(WTSVirtualChannelManager* vcm, rdpPeerChannel* channel, wStream* s,
                             BOOL* onTunnel)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);
	WINPR_ASSERT(channel);
	WINPR_ASSERT(s);
	WINPR_ASSERT(onTunnel);

	/* the client answers on the same transport */
	channel->tunnel =
	    (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_NONE) && vcm->tunnel->tunnelReady;
	*onTunnel = channel->tunnel;
	if (!channel->tunnel)
		return TRUE;
	return wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), Stream_GetPosition(s), TRUE);
}

BOOL wts_tunnel_write(rdpPeerChannel* channel, const BYTE* data, size_t length, BOOL* status)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(status);

	if (!channel->tunnel)
		return FALSE;
	*status = wts_queue_drdynvc_pdu(channel->vcm, data, length, TRUE);
	return TRUE;
}

BOOL wts_tunnel_post_item(rdpPeerChannel* channel, BYTE* buffer, UINT32 length, BOOL* status)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_ASSERT(status);

	if (!channel->tunnel)
		return FALSE;
	*status = wts_tunnel_post(channel->vcm->drdynvc_channel, buffer, length, TRUE);
	return TRUE;
}
