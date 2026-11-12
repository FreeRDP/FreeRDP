/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Soft-Sync triggers of the server's dynamic channels over the UDP tunnel
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

#include <stdio.h>
#include <string.h>

#include <winpr/collections.h>
#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

/* Include the source file directly because Soft-Sync triggers are defined as static. */
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include "../server_tunnel.c"

#define TEST(cond)                                                         \
	do                                                                     \
	{                                                                      \
		if (!(cond))                                                       \
		{                                                                  \
			(void)printf("%s:%d: %s failed\n", __func__, __LINE__, #cond); \
			return FALSE;                                                  \
		}                                                                  \
	} while (0)

typedef struct
{
	WTSVirtualChannelManager vcm;
	struct s_wts_tunnel tunnel;
	rdpPeerChannel drdynvc;
	rdpPeerChannel channel;
} tunnelFixture;

static BOOL fixture_read_pdu(WINPR_ATTR_UNUSED rdpPeerChannel* channel,
                             WINPR_ATTR_UNUSED wStream* s, WINPR_ATTR_UNUSED UINT8 Cmd,
                             WINPR_ATTR_UNUSED UINT8 Sp, WINPR_ATTR_UNUSED UINT8 cbChId,
                             WINPR_ATTR_UNUSED BOOL fromTunnel)
{
	return TRUE;
}

/* A channel manager with drdynvc ready and one open dynamic channel on TCP. */
static BOOL fixture_init(tunnelFixture* f)
{
	memset(f, 0, sizeof(*f));
	f->vcm.tunnel = &f->tunnel;
	f->vcm.drdynvc_channel = &f->drdynvc;
	f->vcm.drdynvc_state = DRDYNVC_STATE_READY;
	f->drdynvc.vcm = &f->vcm;
	f->tunnel.readPdu = fixture_read_pdu;
	f->tunnel.tunnelBacklog = Queue_New(FALSE, -1, -1);
	f->vcm.queue = MessageQueue_New(nullptr);
	f->vcm.dynamicVirtualChannels = HashTable_New(TRUE);
	f->channel.vcm = &f->vcm;
	f->channel.channelId = 5;
	f->channel.dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	if (!f->tunnel.tunnelBacklog || !f->vcm.queue || !f->vcm.dynamicVirtualChannels ||
	    !InitializeCriticalSectionAndSpinCount(&f->channel.writeLock, 4000))
		return FALSE;
	return HashTable_Insert(f->vcm.dynamicVirtualChannels, &f->channel.channelId, &f->channel);
}

static void fixture_free(tunnelFixture* f)
{
	wMessage message = WINPR_C_ARRAY_INIT;
	while (f->vcm.queue && (MessageQueue_Peek(f->vcm.queue, &message, TRUE) > 0))
		free(message.wParam);
	MessageQueue_Free(f->vcm.queue);
	HashTable_Free(f->vcm.dynamicVirtualChannels);
	Queue_Free(f->tunnel.tunnelBacklog);
	DeleteCriticalSection(&f->channel.writeLock);
}

static BOOL queue_event_set(tunnelFixture* f)
{
	return WaitForSingleObject(MessageQueue_Event(f->vcm.queue), 0) == WAIT_OBJECT_0;
}

/* WTSVirtualChannelManagerCheckFileDescriptor's drain, without a client to send to. */
static BOOL drain(tunnelFixture* f)
{
	wMessage message = WINPR_C_ARRAY_INIT;
	while (MessageQueue_Peek(f->vcm.queue, &message, TRUE) > 0)
	{
		BOOL status = TRUE;
		const BOOL handled = wts_tunnel_send_message(&f->vcm, message.id, message.wParam,
		                                             (UINT32)(UINT_PTR)message.lParam, &status);
		free(message.wParam);
		if (!handled || !status)
			return FALSE;
	}
	return wts_tunnel_after_drain(&f->vcm, TRUE);
}

/* A Create Response wakes the drain, also when it arrives while the drain runs: the drain resets
 * the queue's event when it takes the wake message, so it must wake itself again. Nothing is left
 * set once Soft-Sync has run, so the server's loop does not spin. */
static BOOL a_create_response_wakes_soft_sync(void)
{
	tunnelFixture f;
	TEST(fixture_init(&f));

	TEST(!queue_event_set(&f));
	wts_tunnel_channel_opened(&f.vcm);
	TEST(queue_event_set(&f));

	/* the drain starts Soft-Sync, then a second channel answers before it drains */
	TEST(wts_tunnel_start_soft_sync_if_due(&f.vcm));
	wts_tunnel_channel_opened(&f.vcm);
	TEST(drain(&f));
	TEST(queue_event_set(&f));

	TEST(wts_tunnel_start_soft_sync_if_due(&f.vcm));
	TEST(drain(&f));
	TEST(!queue_event_set(&f));

	fixture_free(&f);
	return TRUE;
}

static DWORD WINAPI soft_sync_thread(LPVOID arg)
{
	tunnelFixture* f = arg;
	if (!wts_soft_sync(&f->vcm))
		(void)printf("wts_soft_sync failed\n");
	return 0;
}

/* The tunnel is lost while Soft-Sync waits for the table lock, which wts_tunnel_state_changed
 * takes to decide that the session stays on TCP: Soft-Sync must not move the channels. */
static BOOL soft_sync_leaves_a_lost_tunnel_alone(void)
{
	tunnelFixture f;
	TEST(fixture_init(&f));
	(void)InterlockedIncrement(&f.tunnel.tunnelReady);

	HashTable_Lock(f.vcm.dynamicVirtualChannels);
	HANDLE thread = CreateThread(nullptr, 0, soft_sync_thread, &f, 0, nullptr);
	TEST(thread);
	/* let it read tunnelReady and block on the lock */
	Sleep(200);
	(void)InterlockedDecrement(&f.tunnel.tunnelReady);
	const BOOL moved = (f.tunnel.softSyncState != SOFT_SYNC_STATE_NONE);
	HashTable_Unlock(f.vcm.dynamicVirtualChannels);
	(void)WaitForSingleObject(thread, INFINITE);
	(void)CloseHandle(thread);

	TEST(!moved);
	TEST(f.tunnel.softSyncState == SOFT_SYNC_STATE_NONE);
	TEST(!f.channel.tunnel);
	TEST(MessageQueue_Size(f.vcm.queue) == 0);

	fixture_free(&f);
	return TRUE;
}

/* Without a request outstanding a Soft-Sync Response is logged and ignored whatever it holds,
 * as a server without Soft-Sync does. One that answers a request has to be well-formed. */
static BOOL a_soft_sync_response_nobody_asked_for_is_ignored(void)
{
	tunnelFixture f;
	TEST(fixture_init(&f));

	const BYTE truncated[2] = { 0, 1 };
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, truncated, sizeof(truncated));
	TEST(s);
	TEST(wts_tunnel_read_soft_sync_response(&f.vcm, s));
	TEST(f.tunnel.softSyncState == SOFT_SYNC_STATE_NONE);

	f.tunnel.softSyncState = SOFT_SYNC_STATE_SENT;
	s = Stream_StaticConstInit(&sbuffer, truncated, sizeof(truncated));
	TEST(s);
	TEST(!wts_tunnel_read_soft_sync_response(&f.vcm, s));

	fixture_free(&f);
	return TRUE;
}

/* A client may write on the tunnel as soon as it has sent its Soft-Sync Response on TCP, and
 * the tunnel can be the faster of the two. What arrives before the Response waits; past
 * SOFT_SYNC_HOLD_BACKLOG the tunnel is asked to take no more, and what was already on its way
 * is still kept rather than ending the connection. */
static BOOL tunnel_data_ahead_of_the_response_waits(void)
{
	tunnelFixture f;
	TEST(fixture_init(&f));
	f.tunnel.softSyncState = SOFT_SYNC_STATE_SENT;

	const size_t count = SOFT_SYNC_HOLD_BACKLOG + 100;
	for (size_t x = 0; x < count; x++)
	{
		wStream* s = Stream_New(nullptr, 16);
		TEST(s);
		Stream_Zero(s, 16);
		Stream_SealLength(s);
		Stream_ResetPosition(s);
		TEST(wts_tunnel_data_received(&f.vcm, TUNNELTYPE_UDPFECR, s));
	}
	TEST((size_t)Queue_Count(f.tunnel.tunnelBacklog) == count);
	TEST(f.tunnel.backlogBytes == 16 * count);

	wStream* held = nullptr;
	while ((held = Queue_Dequeue(f.tunnel.tunnelBacklog)))
		Stream_Release(held);
	fixture_free(&f);
	return TRUE;
}

int TestServerTunnel(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!a_create_response_wakes_soft_sync())
		return -1;
	if (!soft_sync_leaves_a_lost_tunnel_alone())
		return -1;
	if (!a_soft_sync_response_nobody_asked_for_is_ignored())
		return -1;
	if (!tunnel_data_ahead_of_the_response_waits())
		return -1;
	return 0;
}
