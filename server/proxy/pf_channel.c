/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#include <winpr/assert.h>

#include <freerdp/freerdp.h>
#include <freerdp/server/proxy/proxy_log.h>

#include "proxy_modules.h"
#include "pf_channel.h"

#define TAG PROXY_TAG("channel")

/** @brief a tracker for channel packets */
struct _ChannelStateTracker
{
	pServerStaticChannelContext* channel;
	ChannelTrackerMode mode;
	wStream* currentPacket;
	size_t currentPacketReceived;
	size_t currentPacketSize;
	size_t currentPacketFragments;

	ChannelTrackerPeekFn peekFn;
	void* trackerData;
	proxyData* pdata;
};

static BOOL channelTracker_resetCurrentPacket(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);

	BOOL create = TRUE;
	if (tracker->currentPacket)
	{
		const size_t cap = Stream_Capacity(tracker->currentPacket);
		if (cap < 1 * 1000 * 1000)
			create = FALSE;
		else
			Stream_Free(tracker->currentPacket, TRUE);
	}

	if (create)
		tracker->currentPacket = Stream_New(NULL, 10 * 1024);
	if (!tracker->currentPacket)
		return FALSE;
	Stream_SetPosition(tracker->currentPacket, 0);
	return TRUE;
}

ChannelStateTracker* channelTracker_new(pServerStaticChannelContext* channel,
                                        ChannelTrackerPeekFn fn, void* data)
{
	ChannelStateTracker* ret = calloc(1, sizeof(ChannelStateTracker));
	if (!ret)
		return ret;

	WINPR_ASSERT(fn);

	ret->channel = channel;
	ret->peekFn = fn;

	if (!channelTracker_setCustomData(ret, data))
		goto fail;

	if (!channelTracker_resetCurrentPacket(ret))
		goto fail;

	return ret;

fail:
	channelTracker_free(ret);
	return NULL;
}

PfChannelResult channelTracker_update(ChannelStateTracker* tracker, const BYTE* xdata, size_t xsize,
                                      UINT32 flags, size_t totalSize)
{
	PfChannelResult result;
	BOOL firstPacket = (flags & CHANNEL_FLAG_FIRST);
	BOOL lastPacket = (flags & CHANNEL_FLAG_LAST);

	WINPR_ASSERT(tracker);

	WLog_VRB(TAG, "channelTracker_update(%s): sz=%" PRIuz " first=%d last=%d",
	         tracker->channel->channel_name, xsize, firstPacket, lastPacket);
	if (flags & CHANNEL_FLAG_FIRST)
	{
		if (!channelTracker_resetCurrentPacket(tracker))
			return FALSE;
		channelTracker_setCurrentPacketSize(tracker, totalSize);
		tracker->currentPacketReceived = 0;
		tracker->currentPacketFragments = 0;
	}

	{
		const size_t currentPacketSize = channelTracker_getCurrentPacketSize(tracker);
		if (tracker->currentPacketReceived + xsize > currentPacketSize)
			WLog_INFO(TAG, "cumulated size is bigger (%" PRIuz ") than total size (%" PRIuz ")",
			          tracker->currentPacketReceived + xsize, currentPacketSize);
	}

	tracker->currentPacketReceived += xsize;
	tracker->currentPacketFragments++;

	switch (channelTracker_getMode(tracker))
	{
		case CHANNEL_TRACKER_PEEK:
		{
			wStream* currentPacket = channelTracker_getCurrentPacket(tracker);
			if (!Stream_EnsureRemainingCapacity(currentPacket, xsize))
				return PF_CHANNEL_RESULT_ERROR;

			Stream_Write(currentPacket, xdata, xsize);

			WINPR_ASSERT(tracker->peekFn);
			result = tracker->peekFn(tracker, firstPacket, lastPacket);
		}
		break;
		case CHANNEL_TRACKER_PASS:
			result = PF_CHANNEL_RESULT_PASS;
			break;
		case CHANNEL_TRACKER_DROP:
			result = PF_CHANNEL_RESULT_DROP;
			break;
	}

	if (lastPacket)
	{
		const size_t currentPacketSize = channelTracker_getCurrentPacketSize(tracker);
		channelTracker_setMode(tracker, CHANNEL_TRACKER_PEEK);

		if (tracker->currentPacketReceived != currentPacketSize)
			WLog_INFO(TAG, "cumulated size(%" PRIuz ") does not match total size (%" PRIuz ")",
			          tracker->currentPacketReceived, currentPacketSize);
	}

	return result;
}

void channelTracker_free(ChannelStateTracker* t)
{
	if (!t)
		return;

	Stream_Free(t->currentPacket, TRUE);
	free(t);
}

/**
 * Flushes the current accumulated tracker content, if it's the first packet, then
 * when can just return that the packet shall be passed, otherwise to have to refragment
 * the accumulated current packet.
 */

PfChannelResult channelTracker_flushCurrent(ChannelStateTracker* t, BOOL first, BOOL last,
                                            BOOL toBack)
{
	proxyData* pdata;
	pServerContext* ps;
	pServerStaticChannelContext* channel;
	UINT32 flags = CHANNEL_FLAG_FIRST;
	BOOL r;
	const char* direction = toBack ? "F->B" : "B->F";
	const size_t currentPacketSize = channelTracker_getCurrentPacketSize(t);
	wStream* currentPacket = channelTracker_getCurrentPacket(t);

	WINPR_ASSERT(t);

	WLog_VRB(TAG, "channelTracker_flushCurrent(%s): %s sz=%" PRIuz " first=%d last=%d",
	         t->channel->channel_name, direction, Stream_GetPosition(currentPacket), first, last);

	if (first)
		return PF_CHANNEL_RESULT_PASS;

	pdata = t->pdata;
	channel = t->channel;
	if (last)
		flags |= CHANNEL_FLAG_LAST;

	if (toBack)
	{
		proxyChannelDataEventInfo ev;

		ev.channel_id = channel->front_channel_id;
		ev.channel_name = channel->channel_name;
		ev.data = Stream_Buffer(currentPacket);
		ev.data_len = Stream_GetPosition(currentPacket);
		ev.flags = flags;
		ev.total_size = currentPacketSize;

		if (!pdata->pc->sendChannelData)
			return PF_CHANNEL_RESULT_ERROR;

		return pdata->pc->sendChannelData(pdata->pc, &ev) ? PF_CHANNEL_RESULT_DROP
		                                                  : PF_CHANNEL_RESULT_ERROR;
	}

	ps = pdata->ps;
	r = ps->context.peer->SendChannelPacket(ps->context.peer, channel->front_channel_id,
	                                        currentPacketSize, flags, Stream_Buffer(currentPacket),
	                                        Stream_GetPosition(currentPacket));

	return r ? PF_CHANNEL_RESULT_DROP : PF_CHANNEL_RESULT_ERROR;
}

static PfChannelResult pf_channel_generic_back_data(proxyData* pdata,
                                                    const pServerStaticChannelContext* channel,
                                                    const BYTE* xdata, size_t xsize, UINT32 flags,
                                                    size_t totalSize)
{
	proxyChannelDataEventInfo ev = { 0 };

	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	switch (channel->channelMode)
	{
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			ev.channel_id = channel->back_channel_id;
			ev.channel_name = channel->channel_name;
			ev.data = xdata;
			ev.data_len = xsize;
			ev.flags = flags;
			ev.total_size = totalSize;

			if (!pf_modules_run_filter(pdata->module, FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA,
			                           pdata, &ev))
				return PF_CHANNEL_RESULT_DROP; /* Silently drop */

			return PF_CHANNEL_RESULT_PASS;

		case PF_UTILS_CHANNEL_INTERCEPT:
			/* TODO */
		case PF_UTILS_CHANNEL_BLOCK:
		default:
			return PF_CHANNEL_RESULT_DROP;
	}
}

static PfChannelResult pf_channel_generic_front_data(proxyData* pdata,
                                                     const pServerStaticChannelContext* channel,
                                                     const BYTE* xdata, size_t xsize, UINT32 flags,
                                                     size_t totalSize)
{
	proxyChannelDataEventInfo ev = { 0 };

	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	switch (channel->channelMode)
	{
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			ev.channel_id = channel->front_channel_id;
			ev.channel_name = channel->channel_name;
			ev.data = xdata;
			ev.data_len = xsize;
			ev.flags = flags;
			ev.total_size = totalSize;

			if (!pf_modules_run_filter(pdata->module, FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA,
			                           pdata, &ev))
				return PF_CHANNEL_RESULT_DROP; /* Silently drop */

			return PF_CHANNEL_RESULT_PASS;

		case PF_UTILS_CHANNEL_INTERCEPT:
			/* TODO */
		case PF_UTILS_CHANNEL_BLOCK:
		default:
			return PF_CHANNEL_RESULT_DROP;
	}
}

BOOL pf_channel_setup_generic(pServerStaticChannelContext* channel)
{
	channel->onBackData = pf_channel_generic_back_data;
	channel->onFrontData = pf_channel_generic_front_data;
	return TRUE;
}

BOOL channelTracker_setMode(ChannelStateTracker* tracker, ChannelTrackerMode mode)
{
	WINPR_ASSERT(tracker);
	tracker->mode = mode;
	return TRUE;
}

ChannelTrackerMode channelTracker_getMode(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);
	return tracker->mode;
}

BOOL channelTracker_setPData(ChannelStateTracker* tracker, proxyData* pdata)
{
	WINPR_ASSERT(tracker);
	tracker->pdata = pdata;
	return TRUE;
}

proxyData* channelTracker_getPData(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);
	return tracker->pdata;
}

wStream* channelTracker_getCurrentPacket(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);
	return tracker->currentPacket;
}

BOOL channelTracker_setCustomData(ChannelStateTracker* tracker, void* data)
{
	WINPR_ASSERT(tracker);
	tracker->trackerData = data;
	return TRUE;
}

void* channelTracker_getCustomData(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);
	return tracker->trackerData;
}

size_t channelTracker_getCurrentPacketSize(ChannelStateTracker* tracker)
{
	WINPR_ASSERT(tracker);
	return tracker->currentPacketSize;
}

BOOL channelTracker_setCurrentPacketSize(ChannelStateTracker* tracker, size_t size)
{
	WINPR_ASSERT(tracker);
	tracker->currentPacketSize = size;
	return TRUE;
}
