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
#include <winpr/assert.h>

#include <freerdp/channels/drdynvc.h>
#include <freerdp/server/proxy/proxy_log.h>

#include "pf_channel_drdynvc.h"
#include "../pf_channel.h"
#include "../proxy_modules.h"
#include "../pf_utils.h"

#define TAG PROXY_TAG("drdynvc")

/** @brief channel opened status */
typedef enum
{
	CHANNEL_OPENSTATE_WAITING_OPEN_STATUS, /*!< dynamic channel waiting for create response */
	CHANNEL_OPENSTATE_OPENED,              /*!< opened */
	CHANNEL_OPENSTATE_CLOSED               /*!< dynamic channel has been opened then closed */
} PfDynChannelOpenStatus;

typedef struct p_server_dynamic_channel_context pServerDynamicChannelContext;
typedef struct DynChannelTrackerState DynChannelTrackerState;

typedef PfChannelResult (*dynamic_channel_on_data_fn)(pServerContext* ps,
                                                      pServerDynamicChannelContext* channel,
                                                      BOOL isBackData, ChannelStateTracker* tracker,
                                                      BOOL firstPacket, BOOL lastPacket);

/** @brief tracker state for a drdynvc stream */
struct DynChannelTrackerState
{
	UINT32 currentDataLength;
	UINT32 CurrentDataReceived;
	UINT32 CurrentDataFragments;
	wStream* currentPacket;
	dynamic_channel_on_data_fn dataCallback;
};

typedef void (*channel_data_dtor_fn)(void** user_data);

struct p_server_dynamic_channel_context
{
	char* channelName;
	UINT32 channelId;
	PfDynChannelOpenStatus openStatus;
	pf_utils_channel_mode channelMode;
	BOOL packetReassembly;
	DynChannelTrackerState backTracker;
	DynChannelTrackerState frontTracker;

	void* channelData;
	channel_data_dtor_fn channelDataDtor;
};

/** @brief context for the dynamic channel */
typedef struct
{
	wHashTable* channels;
	ChannelStateTracker* backTracker;
	ChannelStateTracker* frontTracker;
} DynChannelContext;

/** @brief result of dynamic channel packet treatment */
typedef enum
{
	DYNCVC_READ_OK,        /*!< read was OK */
	DYNCVC_READ_ERROR,     /*!< an error happened during read */
	DYNCVC_READ_INCOMPLETE /*!< missing bytes to read the complete packet */
} DynvcReadResult;

static pServerDynamicChannelContext* DynamicChannelContext_new(pServerContext* ps, const char* name,
                                                               UINT32 id)
{
	pServerDynamicChannelContext* ret = calloc(1, sizeof(*ret));
	if (!ret)
	{
		PROXY_LOG_ERR(TAG, ps, "error allocating dynamic channel context '%s'", name);
		return NULL;
	}

	ret->channelId = id;
	ret->channelName = _strdup(name);
	if (!ret->channelName)
	{
		PROXY_LOG_ERR(TAG, ps, "error allocating name in dynamic channel context '%s'", name);
		free(ret);
		return NULL;
	}

	ret->channelMode = pf_utils_get_channel_mode(ps->pdata->config, name);
	ret->openStatus = CHANNEL_OPENSTATE_OPENED;
	ret->packetReassembly = (ret->channelMode == PF_UTILS_CHANNEL_INTERCEPT);

	return ret;
}

static void DynamicChannelContext_free(pServerDynamicChannelContext* c)
{
	if (!c)
		return;

	if (c->backTracker.currentPacket)
		Stream_Free(c->backTracker.currentPacket, TRUE);

	if (c->frontTracker.currentPacket)
		Stream_Free(c->frontTracker.currentPacket, TRUE);

	if (c->channelDataDtor)
		c->channelDataDtor(&c->channelData);

	free(c->channelName);
	free(c);
}

static UINT32 ChannelId_Hash(const void* key)
{
	const UINT32* v = (const UINT32*)key;
	return *v;
}

static BOOL ChannelId_Compare(const UINT32* v1, const UINT32* v2)
{
	return (*v1 == *v2);
}

static DynvcReadResult dynvc_read_varInt(wStream* s, size_t len, UINT64* varInt, BOOL last)
{
	WINPR_ASSERT(varInt);
	switch (len)
	{
		case 0x00:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT8(s, *varInt);
			break;
		case 0x01:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT16(s, *varInt);
			break;
		case 0x02:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT32(s, *varInt);
			break;
		case 0x03:
		default:
			WLog_ERR(TAG, "Unknown int len %" PRIuz, len);
			return DYNCVC_READ_ERROR;
	}
	return DYNCVC_READ_OK;
}

static const char* get_packet_type(BYTE cmd)
{
	switch (cmd)
	{
		case CREATE_REQUEST_PDU:
			return "CREATE_REQUEST_PDU";
		case DATA_FIRST_PDU:
			return "DATA_FIRST_PDU";
		case DATA_PDU:
			return "DATA_PDU";
		case CLOSE_REQUEST_PDU:
			return "CLOSE_REQUEST_PDU";
		case CAPABILITY_REQUEST_PDU:
			return "CAPABILITY_REQUEST_PDU";
		case DATA_FIRST_COMPRESSED_PDU:
			return "DATA_FIRST_COMPRESSED_PDU";
		case DATA_COMPRESSED_PDU:
			return "DATA_COMPRESSED_PDU";
		case SOFT_SYNC_REQUEST_PDU:
			return "SOFT_SYNC_REQUEST_PDU";
		case SOFT_SYNC_RESPONSE_PDU:
			return "SOFT_SYNC_RESPONSE_PDU";
		default:
			return "UNKNOWN";
	}
}

static PfChannelResult DynvcTrackerPeekFn(ChannelStateTracker* tracker, BOOL firstPacket,
                                          BOOL lastPacket)
{
	BYTE cmd, byte0;
	wStream *s, sbuffer;
	BOOL haveChannelId;
	BOOL haveLength;
	UINT64 dynChannelId = 0;
	UINT64 Length = 0;
	pServerDynamicChannelContext* dynChannel = NULL;

	WINPR_ASSERT(tracker);

	DynChannelContext* dynChannelContext = (DynChannelContext*)tracker->trackerData;
	WINPR_ASSERT(dynChannelContext);

	BOOL isBackData = (tracker == dynChannelContext->backTracker);
	DynChannelTrackerState* trackerState = NULL;

	UINT32 flags = lastPacket ? CHANNEL_FLAG_LAST : 0;
	proxyData* pdata = tracker->pdata;
	WINPR_ASSERT(pdata);

	const char* direction = isBackData ? "B->F" : "F->B";

	s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(tracker->currentPacket),
	                           Stream_GetPosition(tracker->currentPacket));
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return PF_CHANNEL_RESULT_ERROR;

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
		dynChannel = (pServerDynamicChannelContext*)HashTable_GetItemValue(
		    dynChannelContext->channels, &dynChannelId);
		if (cmd != CREATE_REQUEST_PDU || !isBackData)
		{
			if (!dynChannel)
			{
				/* we've not found the target channel, so we drop this chunk, plus all the rest of
				 * the packet */
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
			WLog_DBG(TAG, "DynvcTracker: %s CAPABILITY_%s", direction,
			         isBackData ? "REQUEST" : "RESPONSE");
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

				if (!pf_modules_run_filter(pdata->module,
				                           FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE, pdata,
				                           &dev))
					return PF_CHANNEL_RESULT_DROP; /* Silently drop */

				if (!dynChannel)
				{
					dynChannel = DynamicChannelContext_new(pdata->ps, name, dynChannelId);
					if (!dynChannel)
					{
						WLog_ERR(TAG, "unable to create dynamic channel context data");
						return PF_CHANNEL_RESULT_ERROR;
					}

					if (!HashTable_Insert(dynChannelContext->channels, &dynChannel->channelId,
					                      dynChannel))
					{
						WLog_ERR(TAG, "unable register dynamic channel context data");
						DynamicChannelContext_free(dynChannel);
						return PF_CHANNEL_RESULT_ERROR;
					}
				}
				dynChannel->openStatus = CHANNEL_OPENSTATE_WAITING_OPEN_STATUS;

				return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, FALSE);
			}

			/* CREATE_REQUEST_PDU response */
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return PF_CHANNEL_RESULT_ERROR;

			Stream_Read_UINT32(s, creationStatus);
			WLog_DBG(TAG, "DynvcTracker(%" PRIu64 ",%s): %s CREATE_RESPONSE openStatus=%" PRIu32,
			         dynChannelId, dynChannel->channelName, direction, creationStatus);

			if (creationStatus != 0)
			{
				/* we remove it from the channels map, as it happens that server reused channel ids
				 * when the channel can't be opened
				 */
				HashTable_Remove(dynChannelContext->channels, &dynChannel->channelId);
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

			WLog_DBG(TAG, "DynvcTracker(%s): %s Close request on channel", dynChannel->channelName,
			         direction);
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
			trackerState = isBackData ? &dynChannel->backTracker : &dynChannel->frontTracker;
			break;

		case DATA_FIRST_COMPRESSED_PDU:
		case DATA_COMPRESSED_PDU:
			WLog_DBG(TAG, "TODO: compressed data packets, pass them as is for now");
			tracker->mode = CHANNEL_TRACKER_PASS;
			return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);

		default:
			return PF_CHANNEL_RESULT_ERROR;
	}

	if (dynChannel->openStatus != CHANNEL_OPENSTATE_OPENED)
	{
		WLog_ERR(TAG, "DynvcTracker(%s [%s]): channel is not opened", dynChannel->channelName,
		         get_packet_type(cmd));
		return PF_CHANNEL_RESULT_ERROR;
	}

	if ((cmd == DATA_FIRST_PDU) || (cmd == DATA_FIRST_COMPRESSED_PDU))
	{
		WLog_DBG(TAG, "DynvcTracker(%s [%s]): %s DATA_FIRST currentPacketLength=%" PRIu64 "",
		         dynChannel->channelName, get_packet_type(cmd), direction, Length);
		trackerState->currentDataLength = Length;
		trackerState->CurrentDataReceived = 0;
		trackerState->CurrentDataFragments = 0;

		if (dynChannel->packetReassembly)
		{
			if (trackerState->currentPacket)
				Stream_SetPosition(trackerState->currentPacket, 0);
		}
	}

	if (cmd == DATA_PDU || cmd == DATA_FIRST_PDU)
	{
		size_t extraSize = Stream_GetRemainingLength(s);

		trackerState->CurrentDataFragments++;
		trackerState->CurrentDataReceived += extraSize;

		if (dynChannel->packetReassembly)
		{
			if (!trackerState->currentPacket)
			{
				trackerState->currentPacket = Stream_New(NULL, 1024);
				if (!trackerState->currentPacket)
				{
					WLog_ERR(TAG, "unable to create current packet");
					return PF_CHANNEL_RESULT_ERROR;
				}
			}

			if (!Stream_EnsureRemainingCapacity(trackerState->currentPacket, extraSize))
			{
				WLog_ERR(TAG, "unable to grow current packet");
				return PF_CHANNEL_RESULT_ERROR;
			}

			Stream_Write(trackerState->currentPacket, Stream_Pointer(s), extraSize);
		}
		WLog_DBG(TAG,
		         "DynvcTracker(%s [%s]): %s frags=%" PRIu32 " received=%" PRIu32 "(%" PRIu32 ")",
		         dynChannel->channelName, get_packet_type(cmd), direction,
		         trackerState->CurrentDataFragments, trackerState->CurrentDataReceived,
		         trackerState->currentDataLength);
	}

	if (cmd == DATA_PDU)
	{
		if (trackerState->currentDataLength)
		{
			if (trackerState->CurrentDataReceived > trackerState->currentDataLength)
			{
				WLog_ERR(TAG,
				         "DynvcTracker (%s  [%s]): reassembled packet (%" PRIu32
				         ") is bigger than announced length (%" PRIu32 ")",
				         dynChannel->channelName, get_packet_type(cmd),
				         trackerState->CurrentDataReceived, trackerState->currentDataLength);
				return PF_CHANNEL_RESULT_ERROR;
			}
		}
		else
		{
			trackerState->CurrentDataFragments = 0;
			trackerState->CurrentDataReceived = 0;
		}
	}

	PfChannelResult result;
	switch (dynChannel->channelMode)
	{
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			result = channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);
			break;
		case PF_UTILS_CHANNEL_BLOCK:
			tracker->mode = CHANNEL_TRACKER_DROP;
			result = PF_CHANNEL_RESULT_DROP;
			break;
		case PF_UTILS_CHANNEL_INTERCEPT:
			if (trackerState->dataCallback)
			{
				result = trackerState->dataCallback(pdata->ps, dynChannel, isBackData, tracker,
				                                    firstPacket, lastPacket);
			}
			else
			{
				WLog_ERR(TAG, "no intercept callback for channel %s(fromBack=%d), dropping packet",
				         dynChannel->channelName, isBackData);
				result = PF_CHANNEL_RESULT_DROP;
			}
			break;
		default:
			WLog_ERR(TAG, "unknown channel mode %d", dynChannel->channelMode);
			result = PF_CHANNEL_RESULT_ERROR;
			break;
	}

	if (!trackerState->currentDataLength ||
	    (trackerState->CurrentDataReceived == trackerState->currentDataLength))
	{
		trackerState->currentDataLength = 0;
		trackerState->CurrentDataFragments = 0;
		trackerState->CurrentDataReceived = 0;

		if (dynChannel->packetReassembly && trackerState->currentPacket)
			Stream_SetPosition(trackerState->currentPacket, 0);
	}

	return result;
}

static void DynChannelContext_free(void* context)
{
	DynChannelContext* c = context;
	if (!c)
		return;
	channelTracker_free(c->backTracker);
	channelTracker_free(c->frontTracker);
	HashTable_Free(c->channels);
	free(c);
}

static DynChannelContext* DynChannelContext_new(proxyData* pdata,
                                                pServerStaticChannelContext* channel)
{
	wObject* obj;
	DynChannelContext* dyn = calloc(1, sizeof(DynChannelContext));
	if (!dyn)
		return FALSE;

	dyn->backTracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->backTracker)
		goto fail;
	dyn->backTracker->pdata = pdata;

	dyn->frontTracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->frontTracker)
		goto fail;
	dyn->frontTracker->pdata = pdata;

	dyn->channels = HashTable_New(FALSE);
	if (!dyn->channels)
		goto fail;

	if (!HashTable_SetHashFunction(dyn->channels, ChannelId_Hash))
		goto fail;

	obj = HashTable_KeyObject(dyn->channels);
	obj->fnObjectEquals = (OBJECT_EQUALS_FN)ChannelId_Compare;

	obj = HashTable_ValueObject(dyn->channels);
	obj->fnObjectFree = (OBJECT_FREE_FN)DynamicChannelContext_free;

	return dyn;

fail:
	DynChannelContext_free(dyn);
	return NULL;
}

static PfChannelResult pf_dynvc_back_data(proxyData* pdata,
                                          const pServerStaticChannelContext* channel,
                                          const BYTE* xdata, size_t xsize, UINT32 flags,
                                          size_t totalSize)
{
	WINPR_ASSERT(channel);
	DynChannelContext* dyn = (DynChannelContext*)channel->context;
	WINPR_UNUSED(pdata);
	WINPR_ASSERT(dyn);
	return channelTracker_update(dyn->backTracker, xdata, xsize, flags, totalSize);
}

static PfChannelResult pf_dynvc_front_data(proxyData* pdata,
                                           const pServerStaticChannelContext* channel,
                                           const BYTE* xdata, size_t xsize, UINT32 flags,
                                           size_t totalSize)
{
	WINPR_ASSERT(channel);
	DynChannelContext* dyn = (DynChannelContext*)channel->context;
	WINPR_UNUSED(pdata);
	WINPR_ASSERT(dyn);
	return channelTracker_update(dyn->frontTracker, xdata, xsize, flags, totalSize);
}

BOOL pf_channel_setup_drdynvc(proxyData* pdata, pServerStaticChannelContext* channel)
{
	DynChannelContext* ret = DynChannelContext_new(pdata, channel);
	if (!ret)
		return FALSE;

	channel->onBackData = pf_dynvc_back_data;
	channel->onFrontData = pf_dynvc_front_data;
	channel->contextDtor = DynChannelContext_free;
	channel->context = ret;
	return TRUE;
}
