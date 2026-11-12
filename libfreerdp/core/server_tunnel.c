/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server dynamic virtual channels over the reliable UDP tunnel
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

/* Client tunnel PDUs held until the Soft-Sync Response. From this many on the tunnel accepts
 * no more data from the client (multitransport_hold_tunnel_data), which sends it again once the
 * Response is in: a client may write on the tunnel as soon as it has sent the Response on TCP,
 * and TCP can be the slower of the two. */
#define SOFT_SYNC_HOLD_BACKLOG 1024
#define SOFT_SYNC_HOLD_BACKLOG_BYTES (1024ull * 1024ull)
/* What was already on its way to the main loop when the hold began still arrives, at most what
 * the main loop's event queue takes. More than that means the hold does not work, and the
 * connection ends. A client that never responds can pin this memory for the session. */
#define SOFT_SYNC_MAX_BACKLOG (256 * 1024)
#define SOFT_SYNC_MAX_BACKLOG_BYTES (48ull * 1024ull * 1024ull)

/* Timeout for channel Create Response. After this, open channels are moved and unanswered
 * ones remain on TCP. */
#define SOFT_SYNC_MAX_WAIT_MS 10000

/* vcm->queue message IDs specifying the transport for drdynvc PDUs */
typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
{
	WTS_QUEUE_TCP = 0,
	WTS_QUEUE_TUNNEL = 1,
	WTS_QUEUE_WAKE = 2 /* No PDU: wakes the drain to initiate Soft-Sync */
} WTS_QUEUE_ID;

/* Post a message instead of just signaling the event. The drain only resets the event after
 * processing the last message, so signaling an empty queue would leave the event set. */
static void wts_tunnel_wake_drain(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);
	if (!MessageQueue_Post(vcm->queue, nullptr, WTS_QUEUE_WAKE, nullptr, nullptr))
		WLog_WARN(TAG, "unable to wake the channel manager for Soft-Sync");
}

/* Soft-Sync as described in [MS-RDPEDYC] 3.1.5.3 */
typedef enum WINPR_C23_ENUM_TYPE(uint8_t)
{
	SOFT_SYNC_STATE_NONE = 0, /* All dynamic channels are on TCP */
	SOFT_SYNC_STATE_SENT =
	    1, /* Request sent; channels moved to the tunnel are transmitted via UDP */
	SOFT_SYNC_STATE_ANSWERED = 2, /* Client responded; tunnel data has been processed */
} WTS_SOFT_SYNC_STATE;

/* Associated with the connection thread. Since Soft-Sync may run on the channel manager drain
 * thread, softSyncState, softSyncWaitStart, clientOnTunnel, and the backlog are protected by
 * the vcm->dynamicVirtualChannels lock. */
struct s_wts_tunnel
{
	wtsTunnelReadPduFn readPdu;
	LONG tunnelReady;         /* Accessed by other threads; use Interlocked functions */
	LONG softSyncDue;         /* Trigger for Soft-Sync; Interlocked only */
	UINT64 softSyncWaitStart; /* GetTickCount64() timestamp when Soft-Sync first waited for a
	                             channel, or 0. */
	WTS_SOFT_SYNC_STATE softSyncState;
	BOOL clientOnTunnel; /* True if the client's Soft-Sync Response lists this tunnel. */
	wQueue*
	    tunnelBacklog;   /* Tunnel PDUs received from the client prior to the Soft-Sync Response. */
	size_t backlogBytes; /* Total number of bytes in tunnelBacklog. */
	BOOL written;        /* Indicates that tunnel PDUs have been queued and require flushing. */
};

WINPR_ATTR_NODISCARD
static BOOL wts_queue_item(rdpPeerChannel* channel, BYTE* buffer, UINT32 length, BOOL tunnel)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_ASSERT(channel->channelId <= UINT16_MAX);

	const UINT16 channelId = (UINT16)channel->channelId;
	const WTS_QUEUE_ID id = tunnel ? WTS_QUEUE_TUNNEL : WTS_QUEUE_TCP;
	return MessageQueue_Post(channel->vcm->queue, (void*)(UINT_PTR)channelId, id, (void*)buffer,
	                         (void*)(UINT_PTR)length);
}

/* Queues a copy of a drdynvc PDU for TCP or the tunnel. */
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
	if (!wts_queue_item(vcm->drdynvc_channel, buffer, WINPR_ASSERTING_INT_CAST(UINT32, length),
	                    tunnel))
	{
		free(buffer);
		return FALSE;
	}
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): the queue takes ownership of buffer
	return TRUE;
}

/* Processes a client PDU received on the tunnel. Capabilities and Soft-Sync PDUs are only
 * valid on the drdynvc static channel. */
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
			WLog_WARN(TAG, "encountered %s on the tunnel, terminating the connection",
			          drdynvc_get_packet_type(Cmd));
			return FALSE;
		default:
			return vcm->tunnel->readPdu(vcm->drdynvc_channel, s, Cmd, Sp, cbChId, TRUE);
	}
}

/* The connection's multitransport, nullptr without one (unit tests). */
WINPR_ATTR_NODISCARD
static rdpMultitransport* wts_tunnel_multitransport(const WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);
	if (!vcm->client || !vcm->client->context || !vcm->client->context->rdp)
		return nullptr;
	return vcm->client->context->rdp->multitransport;
}

/* [MS-RDPEDYC] 2.2.5.2 DYNVC_SOFT_SYNC_RESPONSE: Specifies the tunnels the client writes on;
 * the server will also read from these tunnels henceforth (3.3.5.3.2). */
BOOL wts_tunnel_read_soft_sync_response(WTSVirtualChannelManager* vcm, wStream* s)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(s);

	/* No request outstanding, as always without a tunnel: logged and ignored whatever it
	 * holds, as before there was Soft-Sync. Only this thread takes the state out of SENT. */
	HashTable_Lock(vcm->dynamicVirtualChannels);
	const BOOL expected = (vcm->tunnel->softSyncState == SOFT_SYNC_STATE_SENT);
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	if (!expected)
	{
		WLog_WARN(TAG, "received an unexpected Soft-Sync response");
		return TRUE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return FALSE;
	Stream_Seek_UINT8(s); /* Skip padding byte */
	const UINT32 count = Stream_Get_UINT32(s);
	if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, count, sizeof(UINT32)))
		return FALSE;
	BOOL reliable = FALSE;
	for (UINT32 x = 0; x < count; x++)
	{
		if (Stream_Get_UINT32(s) == TUNNELTYPE_UDPFECR)
			reliable = TRUE;
	}

	BOOL rc = TRUE;
	HashTable_Lock(vcm->dynamicVirtualChannels);
	/* No request outstanding: assume no tunnel, log and continue */
	if (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_SENT)
	{
		WLog_WARN(TAG, "received an unexpected Soft-Sync response");
		goto out;
	}

	vcm->tunnel->softSyncState = SOFT_SYNC_STATE_ANSWERED;
	vcm->tunnel->clientOnTunnel = reliable;
	WLog_INFO(TAG, "Soft-Sync Response: the client %s the reliable UDP tunnel",
	          reliable ? "writes on" : "does not use");

	/* [MS-RDPEDYC] 3.2.5.3.2: The response lists all tunnels the client writes on. The server
	 * continues writing to the tunnel specified in its request, but only reads those
	 * listed by the client. */
	if (!reliable && (Queue_Count(vcm->tunnel->tunnelBacklog) > 0))
	{
		WLog_ERR(TAG, "client wrote to a tunnel not named in its Soft-Sync Response");
		rc = FALSE;
		goto out;
	}

	wStream* held = nullptr;
	vcm->tunnel->backlogBytes = 0;
	while (rc && (held = Queue_Dequeue(vcm->tunnel->tunnelBacklog)))
	{
		rc = wts_read_tunnel_pdu(vcm, Stream_Buffer(held), Stream_Length(held));
		Stream_Release(held);
	}
out:
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	/* the backlog is processed, the client's data is taken as it comes from here on */
	multitransport_hold_tunnel_data(wts_tunnel_multitransport(vcm), FALSE);
	return rc;
}

/* Moves all open dynamic channels to the tunnel via a Soft-Sync Request ([MS-RDPEDYC]
 * 3.3.5.3.1). The request is sent over TCP after any previously queued channel data; subsequent
 * data is sent via the tunnel. Each channel's writeLock is held across both steps to prevent
 * messages from crossing the switch. */
WINPR_ATTR_NODISCARD
static BOOL wts_soft_sync(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);

	if (!InterlockedCompareExchange(&vcm->tunnel->tunnelReady, 0, 0) || !vcm->drdynvc_channel ||
	    (vcm->drdynvc_state != DRDYNVC_STATE_READY))
		return TRUE;
	/* tunnelReady is cleared when the main loop handles the tunnel's failure. A request sent
	 * between the failure and that would count as channels moved, and end a connection that
	 * could have stayed on TCP. */
	if (multitransport_tunnel_has_failed(wts_tunnel_multitransport(vcm)))
		return TRUE;

	BOOL rc = FALSE;
	ULONG_PTR* keys = nullptr;
	rdpPeerChannel** channels = nullptr;
	rdpPeerChannel** moving = nullptr;
	size_t count = 0;
	wStream* s = nullptr;

	/* Both the peer thread and the channel manager drain thread may call this; only one
	 * should send the request. */
	HashTable_Lock(vcm->dynamicVirtualChannels);
	/* Re-verify tunnelReady under the lock: wts_tunnel_state_changed may clear it before
	 * acquiring the lock to decide if a lost tunnel terminates the connection or stays on TCP. */
	if ((vcm->tunnel->softSyncState != SOFT_SYNC_STATE_NONE) ||
	    !InterlockedCompareExchange(&vcm->tunnel->tunnelReady, 0, 0))
	{
		rc = TRUE;
		goto out;
	}
	const UINT64 waitStart = vcm->tunnel->softSyncWaitStart;
	const BOOL waitedLong =
	    (waitStart != 0) && (GetTickCount64() - waitStart > SOFT_SYNC_MAX_WAIT_MS);
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
		/* Moving other channels now would permanently leave unanswered channels on TCP. Retry
		 * upon client response or after SOFT_SYNC_MAX_WAIT_MS. */
		if (channel && (channel->dvc_open_state == DVC_OPEN_STATE_NONE))
		{
			if (!waitedLong)
			{
				if (waitStart == 0)
					vcm->tunnel->softSyncWaitStart = GetTickCount64();
				rc = TRUE;
				goto out;
			}
			WLog_WARN(TAG, "missing Create Response for channel %" PRIu32 ", it stays on TCP",
			          channel->channelId);
		}
		if (channel && (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED))
			channels[count++] = channel;
	}
	if (count == 0)
	{
		/* Nothing to move; the next channel open will retry. Once the timeout expires, prevent
		 * unanswered channels from triggering a full rescan on every drain. */
		if (waitedLong)
			vcm->tunnel->softSyncWaitStart = 0;
		rc = TRUE;
		goto out;
	}
	/* NumberOfDVCs is a 16-bit field */
	if (count > UINT16_MAX)
	{
		WLog_ERR(TAG, "%" PRIuz " dynamic channels are too many for a Soft-Sync Request", count);
		goto out;
	}

	/* The list is processed while holding writeLocks; any channel closed in the meantime has
	 * already posted its Close Request on TCP and is excluded (see wts_tunnel_close_request). */
	for (size_t x = 0; x < count; x++)
		EnterCriticalSection(&channels[x]->writeLock);
	moving = (rdpPeerChannel**)calloc(count, sizeof(rdpPeerChannel*));
	size_t moves = 0;
	if (moving)
	{
		for (size_t x = 0; x < count; x++)
		{
			if (!channels[x]->closing)
				moving[moves++] = channels[x];
		}
	}
	if (!moving || (moves == 0))
	{
		rc = (moving != nullptr);
		goto unlock;
	}

	/* DYNVC_SOFT_SYNC_REQUEST: The length is calculated starting from the Length field. */
	const size_t listLength = 4 + 2 + sizeof(UINT32) * moves;
	s = Stream_New(nullptr, 2 + 4 + 2 + 2 + listLength);
	if (!s)
		goto unlock;
	/* Stream_Write macros may evaluate their arguments multiple times. */
	const UINT8 header = (UINT8)((UINT32)SOFT_SYNC_REQUEST_PDU << 4u);
	const UINT16 flags =
	    (UINT16)((UINT32)SOFT_SYNC_TCP_FLUSHED | (UINT32)SOFT_SYNC_CHANNEL_LIST_PRESENT);
	const UINT32 length = WINPR_ASSERTING_INT_CAST(UINT32, 4 + 2 + 2 + listLength);
	const UINT16 dvcs = WINPR_ASSERTING_INT_CAST(UINT16, moves);
	Stream_Write_UINT8(s, header);
	Stream_Write_UINT8(s, 0);                   /* Padding byte */
	Stream_Write_UINT32(s, length);             /* The length of the PDU */
	Stream_Write_UINT16(s, flags);              /* Soft-Sync Request flags */
	Stream_Write_UINT16(s, 1);                  /* Number of tunnels */
	Stream_Write_UINT32(s, TUNNELTYPE_UDPFECR); /* Specify the tunnel type */
	Stream_Write_UINT16(s, dvcs);               /* Number of Dynamic Virtual Channels */
	for (size_t x = 0; x < moves; x++)
		Stream_Write_UINT32(s, moving[x]->channelId);

	rc = wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), Stream_GetPosition(s), FALSE);
	if (rc)
	{
		for (size_t x = 0; x < moves; x++)
			moving[x]->tunnel = TRUE;
		vcm->tunnel->softSyncState = SOFT_SYNC_STATE_SENT;
		vcm->tunnel->softSyncWaitStart = 0;
		WLog_INFO(TAG,
		          "Soft-Sync Request: %" PRIuz " dynamic channel(s) are moving to the UDP tunnel",
		          moves);
		for (size_t x = 0; x < moves; x++)
			WLog_DBG(TAG, "  %" PRIu32 " %s", moving[x]->channelId, moving[x]->channelName);
	}

unlock:
	for (size_t x = 0; x < count; x++)
		LeaveCriticalSection(&channels[x]->writeLock);

out:
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	Stream_Free(s, TRUE);
	free((void*)moving);
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

	BOOL rc = FALSE;
	BOOL hold = FALSE;
	HashTable_Lock(vcm->dynamicVirtualChannels);
	/* [MS-RDPEDYC] 3.3.5.3.2: The server does not process tunnel data before the Soft-Sync Response
	 */
	if (vcm->tunnel->softSyncState == SOFT_SYNC_STATE_NONE)
	{
		/* No channels should be on the tunnel before a Soft-Sync Request. */
		WLog_ERR(TAG, "client wrote to the tunnel before a Soft-Sync Request");
	}
	else if (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_ANSWERED)
	{
		const size_t length = Stream_Length(s);
		const size_t held =
		    WINPR_ASSERTING_INT_CAST(size_t, Queue_Count(vcm->tunnel->tunnelBacklog));
		hold = (held + 1 >= SOFT_SYNC_HOLD_BACKLOG) ||
		       (length >= SOFT_SYNC_HOLD_BACKLOG_BYTES - vcm->tunnel->backlogBytes) ||
		       (vcm->tunnel->backlogBytes >= SOFT_SYNC_HOLD_BACKLOG_BYTES);
		if ((held >= SOFT_SYNC_MAX_BACKLOG) ||
		    (length > SOFT_SYNC_MAX_BACKLOG_BYTES - vcm->tunnel->backlogBytes))
			WLog_ERR(TAG, "tunnel data continues to arrive without a Soft-Sync Response");
		else if (Queue_Enqueue(vcm->tunnel->tunnelBacklog, s))
		{
			/* Backlog now owns s. */
			vcm->tunnel->backlogBytes += length;
			s = nullptr;
			rc = TRUE;
		}
	}
	else if (!vcm->tunnel->clientOnTunnel)
		WLog_ERR(TAG, "the client wrote data on a tunnel that its Soft-Sync Response did not list");
	else
		rc = wts_read_tunnel_pdu(vcm, Stream_Buffer(s), Stream_Length(s));
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	if (hold)
		multitransport_hold_tunnel_data(wts_tunnel_multitransport(vcm), TRUE);
	if (s)
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
	const BOOL ready = (state == FREERDP_MULTITRANSPORT_TUNNEL_READY);
	/* Use compare-exchange as WinPR's InterlockedExchange reads the old value non-atomically.
	 * Only this thread writes the value. */
	const LONG old = InterlockedCompareExchange(&vcm->tunnel->tunnelReady, 0, 0);
	if (InterlockedCompareExchange(&vcm->tunnel->tunnelReady, ready ? 1 : 0, old) != old)
		WLog_DBG(TAG, "tunnel state was updated by another thread");
	/* Soft-Sync only moves channels from TCP to UDP ([MS-RDPEDYC] 3.1.5.3). Once moved,
	tunnel loss results in data loss and session termination; otherwise, the session remains
	on TCP. */
	HashTable_Lock(vcm->dynamicVirtualChannels);
	const BOOL moved = (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_NONE);
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	if (!ready && moved)
	{
		WLog_WARN(TAG, "UDP tunnel carrying channels failed, terminating the connection");
		return FALSE;
	}
	return wts_soft_sync(vcm);
}

BOOL wts_tunnel_new(WTSVirtualChannelManager* vcm, rdpContext* context, wtsTunnelReadPduFn readPdu)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->dynamicVirtualChannels);
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
		WLog_WARN(TAG, "failed to remove the multitransport callbacks");
	multitransport_hold_tunnel_data(wts_tunnel_multitransport(vcm), FALSE);
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
	/* May be called from an application thread via WTSVirtualChannelClose. Waking the drain
	 * triggers Soft-Sync (wts_tunnel_start_soft_sync_if_due). Only the first call in a burst
	 * posts a message; others are either already queued or re-armed by wts_tunnel_after_drain. */
	if (InterlockedIncrement(&vcm->tunnel->softSyncDue) == 1)
		wts_tunnel_wake_drain(vcm);
}

BOOL wts_tunnel_is_soft_sync_response(UINT8 Cmd)
{
	return Cmd == SOFT_SYNC_RESPONSE_PDU;
}

BOOL wts_tunnel_start_soft_sync_if_due(WTSVirtualChannelManager* vcm)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);

	HashTable_Lock(vcm->dynamicVirtualChannels);
	const UINT64 waitStart = vcm->tunnel->softSyncWaitStart;
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	const BOOL waitedLong =
	    (waitStart != 0) && (GetTickCount64() - waitStart > SOFT_SYNC_MAX_WAIT_MS);
	/* Consume the flag; use compare-exchange as WinPR's InterlockedExchange is not atomic. */
	LONG due = InterlockedCompareExchange(&vcm->tunnel->softSyncDue, 0, 0);
	while (due != 0)
	{
		const LONG seen = InterlockedCompareExchange(&vcm->tunnel->softSyncDue, 0, due);
		if (seen == due)
			break;
		due = seen;
	}
	if ((due == 0) && !waitedLong)
		return TRUE;
	return wts_soft_sync(vcm);
}

BOOL wts_tunnel_send_message(WTSVirtualChannelManager* vcm, UINT32 id, const BYTE* buffer,
                             UINT32 length, BOOL* status)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);
	WINPR_ASSERT(status);

	if (id == WTS_QUEUE_WAKE)
		return TRUE;
	if (id != WTS_QUEUE_TUNNEL)
		return FALSE;

	WINPR_ASSERT(vcm->client);
	WINPR_ASSERT(vcm->client->context);
	if (multitransport_queue_dvc(vcm->client->context->rdp->multitransport, buffer, length))
		vcm->tunnel->written = TRUE;
	else
	{
		WLog_ERR(TAG, "unable to write a dynamic channel PDU to the UDP tunnel");
		*status = FALSE;
	}
	return TRUE;
}

BOOL wts_tunnel_after_drain(WTSVirtualChannelManager* vcm, BOOL status)
{
	WINPR_ASSERT(vcm);
	WINPR_ASSERT(vcm->tunnel);

	/* If a channel responded during this drain, its wake message might have been processed
	 * before Soft-Sync saw the flag; trigger the drain again. */
	if (InterlockedCompareExchange(&vcm->tunnel->softSyncDue, 0, 0) != 0)
		wts_tunnel_wake_drain(vcm);

	/* the next drain starts clean */
	const BOOL written = vcm->tunnel->written;
	vcm->tunnel->written = FALSE;
	if (!status || !written)
		return status;

	WINPR_ASSERT(vcm->client->context->rdp);
	if (!multitransport_flush_dvc(vcm->client->context->rdp->multitransport))
	{
		WLog_ERR(TAG, "unable to write dynamic channel PDUs over the UDP tunnel");
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

	/* Client responds on the same transport. */
	BOOL rc = TRUE;
	HashTable_Lock(vcm->dynamicVirtualChannels);
	channel->tunnel = (vcm->tunnel->softSyncState != SOFT_SYNC_STATE_NONE) &&
	                  InterlockedCompareExchange(&vcm->tunnel->tunnelReady, 0, 0);
	*onTunnel = channel->tunnel;
	if (channel->tunnel)
		rc = wts_queue_drdynvc_pdu(vcm, Stream_Buffer(s), Stream_GetPosition(s), TRUE);
	HashTable_Unlock(vcm->dynamicVirtualChannels);
	return rc;
}

BOOL wts_tunnel_close_request(rdpPeerChannel* channel, const BYTE* data, size_t length,
                              BOOL* status)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(status);

	/* Send the Close Request now. A Soft-Sync waiting for this writeLock must not list the
	 * channel ([MS-RDPEDYC] 2.2.5.1 lists the channels the server will send data for). */
	channel->closing = TRUE;
	if (!channel->tunnel)
		return FALSE;
	*status = wts_queue_drdynvc_pdu(channel->vcm, data, length, TRUE);
	return TRUE;
}

BOOL wts_tunnel_queue_send_item(rdpPeerChannel* channel, BYTE* buffer, UINT32 length, BOOL* status)
{
	WINPR_ASSERT(channel);
	WINPR_ASSERT(channel->vcm);
	WINPR_ASSERT(status);

	if (!channel->tunnel)
		return FALSE;
	*status = wts_queue_item(channel->vcm->drdynvc_channel, buffer, length, TRUE);
	return TRUE;
}
