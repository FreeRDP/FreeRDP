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

ChannelStateTracker* channelTracker_new(pServerStaticChannelContext* channel,
                                        ChannelTrackerPeekFn fn, void* data)
{
	ChannelStateTracker* ret = calloc(1, sizeof(ChannelStateTracker));
	if (!ret)
		return ret;

	WINPR_ASSERT(fn);

	ret->channel = channel;
	ret->peekFn = fn;
	ret->trackerData = data;
	ret->currentPacket = Stream_New(NULL, 10 * 1024);
	if (!ret->currentPacket)
	{
		channelTracker_free(ret);
		return NULL;
	}
	return ret;
}

PfChannelResult channelTracker_update(ChannelStateTracker* tracker, const BYTE* xdata, size_t xsize,
                                      UINT32 flags, size_t totalSize)
{
	PfChannelResult result;
	BOOL firstPacket = (flags & CHANNEL_FLAG_FIRST);
	BOOL lastPacket = (flags & CHANNEL_FLAG_LAST);

	WINPR_ASSERT(tracker);

	WLog_VRB(TAG, "channelTracker_update(%s): sz=%d first=%d last=%d",
	         tracker->channel->channel_name, xsize, firstPacket, lastPacket);
	if (flags & CHANNEL_FLAG_FIRST)
	{
		/* don't keep a too big currentPacket */
		if (Stream_Capacity(tracker->currentPacket) > 1 * 1000 * 1000)
		{
			Stream_Free(tracker->currentPacket, TRUE);

			tracker->currentPacket = Stream_New(NULL, 10 * 1024);
			if (!tracker->currentPacket)
			{
				return PF_CHANNEL_RESULT_ERROR;
			}
		}
		else
		{
			Stream_SetPosition(tracker->currentPacket, 0);
		}

		tracker->currentPacketSize = totalSize;
		tracker->currentPacketReceived = 0;
		tracker->currentPacketFragments = 0;
	}

	if (tracker->currentPacketReceived + xsize > tracker->currentPacketSize)
		WLog_INFO(TAG, "cumulated size is bigger (%d) than total size (%d)",
		          tracker->currentPacketReceived + xsize, tracker->currentPacketSize);

	tracker->currentPacketReceived += xsize;
	tracker->currentPacketFragments++;

	switch (tracker->mode)
	{
		case CHANNEL_TRACKER_PEEK:
			if (!Stream_EnsureRemainingCapacity(tracker->currentPacket, xsize))
				return PF_CHANNEL_RESULT_ERROR;

			Stream_Write(tracker->currentPacket, xdata, xsize);

			WINPR_ASSERT(tracker->peekFn);
			result = tracker->peekFn(tracker, firstPacket, lastPacket);
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
		tracker->mode = CHANNEL_TRACKER_PEEK;
		if (tracker->currentPacketReceived != tracker->currentPacketSize)
			WLog_INFO(TAG, "cumulated size(%d) does not match total size (%d)",
			          tracker->currentPacketReceived, tracker->currentPacketSize);
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

	WINPR_ASSERT(t);

	WLog_VRB(TAG, "channelTracker_flushCurrent(%s): %s sz=%d first=%d last=%d",
	         t->channel->channel_name, direction, Stream_GetPosition(t->currentPacket), first,
	         last);

	if (first)
		return PF_CHANNEL_RESULT_PASS;

	pdata = t->pdata;
	channel = t->channel;
	if (last)
		flags |= CHANNEL_FLAG_LAST;

	if (toBack)
	{
		proxyChannelDataEventInfo ev;

		ev.channel_id = channel->channel_id;
		ev.channel_name = channel->channel_name;
		ev.data = Stream_Buffer(t->currentPacket);
		ev.data_len = Stream_GetPosition(t->currentPacket);
		ev.flags = flags;
		ev.total_size = t->currentPacketSize;

		if (!pdata->pc->sendChannelData)
			return PF_CHANNEL_RESULT_ERROR;

		return pdata->pc->sendChannelData(pdata->pc, &ev) ? PF_CHANNEL_RESULT_DROP
		                                                  : PF_CHANNEL_RESULT_ERROR;
	}

	ps = pdata->ps;
	r = ps->context.peer->SendChannelPacket(
	    ps->context.peer, channel->channel_id, t->currentPacketSize, flags,
	    Stream_Buffer(t->currentPacket), Stream_GetPosition(t->currentPacket));

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
			ev.channel_id = channel->channel_id;
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
			ev.channel_id = channel->channel_id;
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
