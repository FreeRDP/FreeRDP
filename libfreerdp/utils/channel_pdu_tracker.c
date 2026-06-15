/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * static channel utility functions
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <freerdp/utils/channel_pdu_tracker.h>

#include <winpr/wtsapi.h>

struct ChannelPduTracker
{
	HANDLE vc;
	wStream* currentPacket;
	BYTE buffer[CHANNEL_PDU_LENGTH];
	BYTE* writePtr;
};

#define TAG "ChannelPduTracker"

wStream* ChannelPduTracker_poll(ChannelPduTracker* tracker, BOOL* ok)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(ok);

	ULONG sz = 0;
	*ok = FALSE;
	const ULONG readSz =
	    WINPR_ASSERTING_INT_CAST(ULONG, CHANNEL_PDU_LENGTH - (tracker->writePtr - tracker->buffer));
	if (!WTSVirtualChannelRead(tracker->vc, INFINITE, (char*)tracker->writePtr, readSz, &sz))
		return nullptr;

	tracker->writePtr += sz;
	const size_t recvSz = (tracker->writePtr - tracker->buffer);
	if (recvSz < sizeof(CHANNEL_PDU_HEADER))
	{
		*ok = TRUE;
		return nullptr;
	}

	CHANNEL_PDU_HEADER* header = (CHANNEL_PDU_HEADER*)tracker->buffer;
	if (header->length > CHANNEL_CHUNK_LENGTH)
	{
		WLog_ERR(TAG, "chunk size %" PRIu32 " is too big", header->length);
		return nullptr;
	}

	if (recvSz - sizeof(CHANNEL_PDU_HEADER) < header->length)
	{
		WLog_ERR(TAG, "chunk size %" PRIu32 " is too big", header->length);
		return nullptr;
	}

	tracker->writePtr = tracker->buffer;
	if (header->flags & CHANNEL_FLAG_FIRST)
		Stream_ResetPosition(tracker->currentPacket);

	if (!Stream_EnsureRemainingCapacity(tracker->currentPacket, header->length))
		return nullptr;

	Stream_Write(tracker->currentPacket, header + 1, header->length);

	if (header->flags & CHANNEL_FLAG_LAST)
	{
		Stream_SealLength(tracker->currentPacket);
		Stream_ResetPosition(tracker->currentPacket);
		wStream* ret = tracker->currentPacket;

		tracker->currentPacket = Stream_New(nullptr, 4096);
		if (!tracker->currentPacket)
		{
			Stream_Release(ret);
			WLog_ERR(TAG, "error allocating new currentPacket");
			return nullptr;
		}
	}

	*ok = TRUE;
	return nullptr;
}

void ChannelPduTracker_free(ChannelPduTracker* tracker)
{
	if (!tracker)
		return;

	Stream_Release(tracker->currentPacket);
	free(tracker);
}

ChannelPduTracker* ChannelPduTracker_new(HANDLE vc)
{
	ChannelPduTracker* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return nullptr;

	ret->currentPacket = Stream_New(nullptr, 4096);
	if (!ret->currentPacket)
	{
		free(ret);
		return nullptr;
	}
	ret->vc = vc;
	ret->writePtr = ret->buffer;
	return ret;
}
