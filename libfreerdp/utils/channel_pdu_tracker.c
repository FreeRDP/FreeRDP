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

#include <freerdp/log.h>
#define TTAG FREERDP_TAG("utils.ChannelPduTracker")

struct ChannelPduTracker
{
	HANDLE vc;
	wStream* currentPacket;
	char buffer[CHANNEL_PDU_LENGTH];
	size_t offset;
	wLog* log;
};

wStream* ChannelPduTracker_poll(ChannelPduTracker* tracker, BOOL* ok)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(ok);

	ULONG sz = 0;
	*ok = FALSE;

	WINPR_ASSERT(tracker->offset <= CHANNEL_PDU_LENGTH);
	const ULONG readSz = WINPR_ASSERTING_INT_CAST(ULONG, CHANNEL_PDU_LENGTH - tracker->offset);
	if (!WTSVirtualChannelRead(tracker->vc, INFINITE, &tracker->buffer[tracker->offset], readSz,
	                           &sz))
		return nullptr;

	tracker->offset += sz;
	WINPR_ASSERT(tracker->offset <= CHANNEL_PDU_LENGTH);

	const size_t recvSz = tracker->offset;
	if (recvSz < sizeof(CHANNEL_PDU_HEADER))
	{
		*ok = TRUE;
		return nullptr;
	}

	const CHANNEL_PDU_HEADER* header = (const CHANNEL_PDU_HEADER*)tracker->buffer;
	if (header->length > CHANNEL_CHUNK_LENGTH)
	{
		WLog_Print(tracker->log, WLOG_ERROR, "chunk size %" PRIu32 " is too big", header->length);
		return nullptr;
	}

	const size_t actual = recvSz - sizeof(CHANNEL_PDU_HEADER);
	if (actual != header->length)
	{
		WLog_Print(tracker->log, WLOG_ERROR,
		           "Expected chunk size %" PRIu32 " does not match received data size %" PRIuz,
		           header->length, actual);
		return nullptr;
	}

	tracker->offset = 0;
	if ((header->flags & CHANNEL_FLAG_FIRST) != 0)
		Stream_ResetPosition(tracker->currentPacket);

	if (!Stream_EnsureRemainingCapacity(tracker->currentPacket, header->length))
		return nullptr;

	Stream_Write(tracker->currentPacket, &tracker->buffer[sizeof(CHANNEL_PDU_HEADER)],
	             header->length);

	if ((header->flags & CHANNEL_FLAG_LAST) == 0)
	{
		*ok = TRUE;
		return nullptr;
	}

	Stream_SealLength(tracker->currentPacket);
	Stream_ResetPosition(tracker->currentPacket);
	wStream* ret = tracker->currentPacket;

	tracker->currentPacket = Stream_New(nullptr, 4096);
	if (!tracker->currentPacket)
	{
		Stream_Release(ret);
		WLog_Print(tracker->log, WLOG_ERROR, "error allocating new currentPacket");
		return nullptr;
	}

	*ok = TRUE;
	return ret;
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
		goto fail;

	ret->vc = vc;
	ret->log = WLog_Get(TTAG);
	if (!ret->log)
		goto fail;
	return ret;
fail:
	ChannelPduTracker_free(ret);
	return nullptr;
}
