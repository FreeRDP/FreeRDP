/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * pf_channel_drdynvc
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
#include <freerdp/channels/drdynvc.h>
#include <freerdp/server/proxy/proxy_log.h>

#include "pf_channel_drdynvc.h"
#include "../pf_channel.h"
#include "../proxy_modules.h"


#define TAG PROXY_TAG("drdynvc")

/** @brief tracker state for a drdynvc stream */
typedef struct
{
	ChannelStateTracker* tracker;
	UINT32 currentDataLength;
	UINT32 CurrentDataReceived;
	UINT32 CurrentDataFragments;
} DynChannelTrackerState;

/** @brief context for the dynamic channel */
typedef struct
{
	DynChannelTrackerState backTracker;
	DynChannelTrackerState frontTracker;
} DynChannelContext;


/** @brief result of dynamic channel packet treatment */
typedef enum {
	DYNCVC_READ_OK, 		/*!< read was OK */
	DYNCVC_READ_ERROR,		/*!< an error happened during read */
	DYNCVC_READ_INCOMPLETE  /*!< missing bytes to read the complete packet */
} DynvcReadResult;

static DynvcReadResult dynvc_read_varInt(wStream* s, size_t len, UINT64* varInt, BOOL last)
{
	switch (len)
	{
		case 0x00:
			if (Stream_GetRemainingLength(s) < 1)
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT8(s, *varInt);
			break;
		case 0x01:
			if (Stream_GetRemainingLength(s) < 2)
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT16(s, *varInt);
			break;
		case 0x02:
			if (Stream_GetRemainingLength(s) < 4)
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT32(s, *varInt);
			break;
		case 0x03:
		default:
			WLog_ERR(TAG, "Unknown int len %d", len);
			return DYNCVC_READ_ERROR;
	}
	return DYNCVC_READ_OK;
}


PfChannelResult DynvcTrackerPeekFn(ChannelStateTracker* tracker, BOOL firstPacket, BOOL lastPacket)
{
	BYTE cmd, byte0;
	wStream *s, sbuffer;
	BOOL haveChannelId;
	BOOL haveLength;
	UINT64 dynChannelId = 0;
	UINT64 Length = 0;
	pServerChannelContext* dynChannel;
	DynChannelContext* dynChannelContext = (DynChannelContext*)tracker->trackerData;
	BOOL isBackData = (tracker == dynChannelContext->backTracker.tracker);
	DynChannelTrackerState* trackerState = isBackData ? &dynChannelContext->backTracker : &dynChannelContext->frontTracker;
	UINT32 flags = lastPacket ? CHANNEL_FLAG_LAST : 0;
	proxyData* pdata = tracker->pdata;
	const char* direction = isBackData ? "B->F" : "F->B";

	s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(tracker->currentPacket), Stream_GetPosition(tracker->currentPacket));
	if (Stream_Length(s) < 1)
		return DYNCVC_READ_INCOMPLETE;

	Stream_Read_UINT8(s, byte0);
	cmd = byte0 >> 4;

	switch (cmd)
	{
	case CREATE_REQUEST_PDU:
	case CLOSE_REQUEST_PDU:
	case DATA_PDU:
	case DATA_COMPRESSED_PDU:
		haveChannelId = TRUE;
		haveLength = FALSE;
		break;
	case DATA_FIRST_PDU:
	case DATA_FIRST_COMPRESSED_PDU:
		haveLength = TRUE;
		haveChannelId = TRUE;
		break;
	default:
		haveChannelId = FALSE;
		haveLength = FALSE;
		break;
	}

	if (haveChannelId)
	{
		UINT64 maskedDynChannelId;
		BYTE cbId = byte0 & 0x03;

		switch (dynvc_read_varInt(s, cbId, &dynChannelId, lastPacket))
		{
		case DYNCVC_READ_OK:
			break;
		case DYNCVC_READ_INCOMPLETE:
			return PF_CHANNEL_RESULT_DROP;
		case DYNCVC_READ_ERROR:
		default:
			WLog_ERR(TAG, "DynvcTrackerPeekFn: invalid channelId field");
			return PF_CHANNEL_RESULT_ERROR;
		}

		/* we always try to retrieve the dynamic channel in case it would have been opened
		 * and closed
		 */
		maskedDynChannelId = dynChannelId | PF_DYNAMIC_CHANNEL_MASK;
		dynChannel = (pServerChannelContext*)HashTable_GetItemValue(pdata->ps->channelsById, &maskedDynChannelId);

		if (cmd != CREATE_REQUEST_PDU || !isBackData)
		{
			if (!dynChannel)
			{
				/* we've not found the target channel, so we drop this chunk, plus all the rest of the packet */
				tracker->mode = CHANNEL_TRACKER_DROP;
				return PF_CHANNEL_RESULT_DROP;
			}
		}
	}

	if (haveLength)
	{
		BYTE lenLen = (byte0 >> 2) & 0x03;
		switch (dynvc_read_varInt(s, lenLen, &Length, lastPacket))
		{
		case DYNCVC_READ_OK:
			break;
		case DYNCVC_READ_INCOMPLETE:
			return PF_CHANNEL_RESULT_DROP;
		case DYNCVC_READ_ERROR:
		default:
			WLog_ERR(TAG, "DynvcTrackerPeekFn: invalid length field");
			return PF_CHANNEL_RESULT_ERROR;
		}
	}

	switch (cmd)
	{
		case CAPABILITY_REQUEST_PDU:
			WLog_DBG(TAG, "DynvcTracker: %s CAPABILITY_%s", direction, isBackData ? "REQUEST" : "RESPONSE");
			tracker->mode = CHANNEL_TRACKER_PASS;
			return PF_CHANNEL_RESULT_PASS;

		case CREATE_REQUEST_PDU:
		{
			UINT32 creationStatus;

			/* we only want the full packet */
			if (!lastPacket)
				return PF_CHANNEL_RESULT_DROP;

			if (isBackData)
			{
				proxyChannelDataEventInfo dev;
				size_t len;
				const char* name = (const char*)Stream_Pointer(s);
				size_t nameLen = Stream_GetRemainingLength(s);

				len = strnlen(name, nameLen);
				if ((len == 0) || (len == nameLen))
					return PF_CHANNEL_RESULT_ERROR;

				dev.channel_id = dynChannelId;
				dev.channel_name = name;
				dev.data = Stream_Buffer(s);
				dev.data_len = Stream_GetPosition(tracker->currentPacket);
				dev.flags = flags;
				dev.total_size = Stream_GetPosition(tracker->currentPacket);

				if (!pf_modules_run_filter(
						pdata->module, FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE, pdata, &dev))
					return PF_CHANNEL_RESULT_DROP; /* Silently drop */

				if (!dynChannel)
				{
					dynChannel = ChannelContext_new(pdata->ps, name, dynChannelId | PF_DYNAMIC_CHANNEL_MASK);
					if (!dynChannel)
					{
						WLog_ERR(TAG, "unable to create dynamic channel context data");
						return PF_CHANNEL_RESULT_ERROR;
					}
					dynChannel->isDynamic = TRUE;

					if (!HashTable_Insert(pdata->ps->channelsById, &dynChannel->channel_id, dynChannel))
					{
						WLog_ERR(TAG, "unable register dynamic channel context data");
						ChannelContext_free(dynChannel);
						return PF_CHANNEL_RESULT_ERROR;
					}
				}
				dynChannel->openStatus = CHANNEL_OPENSTATE_WAITING_OPEN_STATUS;

				return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, FALSE);
			}

			/* CREATE_REQUEST_PDU response */
			if (Stream_GetRemainingLength(s) < 4)
				return PF_CHANNEL_RESULT_ERROR;

			Stream_Read_INT32(s, creationStatus);
			WLog_DBG(TAG, "DynvcTracker(%"PRIu16",%s): %s CREATE_RESPONSE openStatus=%"PRIu32, dynChannelId,
					dynChannel->channel_name, direction, creationStatus);

			if (creationStatus != 0)
			{
				/* we remove it from the channels map, as it happens that server reused channel ids when
				 * the channel can't be opened
				 */
				HashTable_Remove(pdata->ps->channelsById, &dynChannel->channel_id);
			}
			else
			{
				dynChannel->openStatus = CHANNEL_OPENSTATE_OPENED;
			}

			return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, TRUE);
		}

		case CLOSE_REQUEST_PDU:
			if (!lastPacket)
				return PF_CHANNEL_RESULT_DROP;

			WLog_DBG(TAG, "DynvcTracker(%s): %s Close request on channel", dynChannel->channel_name, direction);
			tracker->mode = CHANNEL_TRACKER_PASS;
			dynChannel->openStatus = CHANNEL_OPENSTATE_CLOSED;
			return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);

		case SOFT_SYNC_REQUEST_PDU:
			/* just pass then as is for now */
			WLog_DBG(TAG, "SOFT_SYNC_REQUEST_PDU");
			tracker->mode = CHANNEL_TRACKER_PASS;
			/*TODO: return pf_treat_softsync_req(pdata, s);*/
			return PF_CHANNEL_RESULT_PASS;

		case SOFT_SYNC_RESPONSE_PDU:
			/* just pass then as is for now */
			WLog_DBG(TAG, "SOFT_SYNC_RESPONSE_PDU");
			tracker->mode = CHANNEL_TRACKER_PASS;
			return PF_CHANNEL_RESULT_PASS;

		case DATA_FIRST_PDU:
		case DATA_PDU:
			/* treat these below */
			break;

		case DATA_FIRST_COMPRESSED_PDU:
		case DATA_COMPRESSED_PDU:
			WLog_DBG(TAG, "TODO: compressed data packets, pass them as is for now");
			tracker->mode = CHANNEL_TRACKER_PASS;
			return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);

		default:
			return PF_CHANNEL_RESULT_ERROR;
	}

	if ((cmd == DATA_FIRST_PDU) || (cmd == DATA_FIRST_COMPRESSED_PDU))
	{
		WLog_DBG(TAG, "DynvcTracker(%s): %s DATA_FIRST currentPacketLength=%d", dynChannel->channel_name, direction, Length);
		trackerState->currentDataLength = Length;
		trackerState->CurrentDataReceived = 0;
		trackerState->CurrentDataFragments = 0;
	}

	if (cmd == DATA_PDU || cmd == DATA_FIRST_PDU)
	{
		trackerState->CurrentDataFragments++;
		trackerState->CurrentDataReceived += Stream_GetRemainingLength(s);
		WLog_DBG(TAG, "DynvcTracker(%s): %s %s frags=%d received=%d(%d)", dynChannel->channel_name, direction,
				cmd == DATA_PDU ? "DATA" : "DATA_FIRST",
				trackerState->CurrentDataFragments, trackerState->CurrentDataReceived,
				trackerState->currentDataLength);
	}

	if (cmd == DATA_PDU)
	{
		if (trackerState->currentDataLength)
		{
			if (trackerState->CurrentDataReceived > trackerState->currentDataLength)
			{
				WLog_ERR(TAG, "DynvcTracker: reassembled packet (%d) is bigger than announced length (%d)", trackerState->CurrentDataReceived, trackerState->currentDataLength);
				return PF_CHANNEL_RESULT_ERROR;
			}

			if (trackerState->CurrentDataReceived == trackerState->currentDataLength)
			{
				trackerState->currentDataLength = 0;
				trackerState->CurrentDataFragments = 0;
				trackerState->CurrentDataReceived = 0;
			}
		}
		else
		{
			trackerState->CurrentDataFragments = 0;
			trackerState->CurrentDataReceived = 0;
		}
	}

	if (dynChannel->openStatus != CHANNEL_OPENSTATE_OPENED)
	{
		WLog_ERR(TAG, "DynvcTracker(%s): channel is not opened", dynChannel->channel_name);
		return PF_CHANNEL_RESULT_ERROR;
	}

	switch(dynChannel->channelMode)
	{
	case PF_UTILS_CHANNEL_PASSTHROUGH:
		return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);
	case PF_UTILS_CHANNEL_BLOCK:
		tracker->mode = CHANNEL_TRACKER_DROP;
		return PF_CHANNEL_RESULT_DROP;
	case PF_UTILS_CHANNEL_INTERCEPT:
		WLog_DBG(TAG, "TODO: implement intercepted dynamic channel");
		return PF_CHANNEL_RESULT_DROP;
	default:
		WLog_ERR(TAG, "unknown channel mode");
		return PF_CHANNEL_RESULT_ERROR;
	}
}


static DynChannelContext* DynChannelContext_new(proxyData* pdata, pServerChannelContext* channel)
{
	DynChannelContext* dyn = calloc(1, sizeof(DynChannelContext));
	if (!dyn)
		return FALSE;

	dyn->backTracker.tracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->backTracker.tracker)
		goto out_fromClientTracker;
	dyn->backTracker.tracker->pdata = pdata;

	dyn->frontTracker.tracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->frontTracker.tracker)
		goto out_fromServerTracker;
	dyn->frontTracker.tracker->pdata = pdata;

	return dyn;

out_fromServerTracker:
	channelTracker_free(dyn->backTracker.tracker);
out_fromClientTracker:
	free(dyn);
	return NULL;
}

static void DynChannelContext_free(DynChannelContext* c)
{
	channelTracker_free(c->backTracker.tracker);
	channelTracker_free(c->frontTracker.tracker);
	free(c);
}

static PfChannelResult pf_dynvc_back_data(proxyData* pdata, const pServerChannelContext* channel,
            const BYTE* xdata, size_t xsize, UINT32 flags,
            size_t totalSize)
{
	DynChannelContext* dyn = (DynChannelContext*)channel->context;
	return channelTracker_update(dyn->backTracker.tracker, xdata, xsize, flags, totalSize);
}

static PfChannelResult pf_dynvc_front_data(proxyData* pdata, const pServerChannelContext* channel,
            const BYTE* xdata, size_t xsize, UINT32 flags,
            size_t totalSize)
{
	DynChannelContext* dyn = (DynChannelContext*)channel->context;
	return channelTracker_update(dyn->frontTracker.tracker, xdata, xsize, flags, totalSize);
}


BOOL pf_channel_setup_drdynvc(proxyData* pdata, pServerChannelContext* channel)
{
	DynChannelContext* ret = DynChannelContext_new(pdata, channel);
	if (!ret)
		return FALSE;

	channel->onBackData = pf_dynvc_back_data;
	channel->onFrontData = pf_dynvc_front_data;
	channel->contextDtor = (proxyChannelContextDtor)DynChannelContext_free;
	channel->context = ret;
	return TRUE;
}
