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
#include <freerdp/utils/drdynvc.h>
#include <freerdp/server/proxy/proxy_log.h>

#include "pf_channel_drdynvc.h"
#include "../pf_channel.h"
#include "../proxy_modules.h"
#include "../pf_utils.h"

#define DTAG PROXY_TAG("drdynvc")

#define Stream_CheckAndLogRequiredLengthWLogWithBackend(log, s, nmemb, backdata)                 \
	Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, nmemb, 1, "%s(%s:%" PRIuz ")[%s]", \
	                                       __func__, __FILE__, (size_t)__LINE__,                 \
	                                       getDirection(backdata))

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
	wLog* log;
} DynChannelContext;

/** @brief result of dynamic channel packet treatment */
typedef enum
{
	DYNCVC_READ_OK,        /*!< read was OK */
	DYNCVC_READ_ERROR,     /*!< an error happened during read */
	DYNCVC_READ_INCOMPLETE /*!< missing bytes to read the complete packet */
} DynvcReadResult;

static const char* openstatus2str(PfDynChannelOpenStatus status)
{
	switch (status)
	{
		case CHANNEL_OPENSTATE_WAITING_OPEN_STATUS:
			return "CHANNEL_OPENSTATE_WAITING_OPEN_STATUS";
		case CHANNEL_OPENSTATE_CLOSED:
			return "CHANNEL_OPENSTATE_CLOSED";
		case CHANNEL_OPENSTATE_OPENED:
			return "CHANNEL_OPENSTATE_OPENED";
		default:
			return "CHANNEL_OPENSTATE_UNKNOWN";
	}
}

#define DynvcTrackerLog(log, level, dynChannel, cmd, isBackData, ...)                         \
	dyn_log_((log), (level), (dynChannel), (cmd), (isBackData), __func__, __FILE__, __LINE__, \
	         __VA_ARGS__)

static const char* getDirection(BOOL isBackData)
{
	return isBackData ? "B->F" : "F->B";
}

static void dyn_log_(wLog* log, DWORD level, const pServerDynamicChannelContext* dynChannel,
                     BYTE cmd, BOOL isBackData, const char* fkt, const char* file, size_t line,
                     const char* fmt, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	char* prefix = NULL;
	char* msg = NULL;
	size_t prefixlen = 0;
	size_t msglen = 0;

	uint32_t channelId = dynChannel ? dynChannel->channelId : UINT32_MAX;
	const char* channelName = dynChannel ? dynChannel->channelName : "<NULL>";
	(void)winpr_asprintf(&prefix, &prefixlen, "DynvcTracker[%s](%s [%s:%" PRIu32 "])",
	                     getDirection(isBackData), channelName, drdynvc_get_packet_type(cmd),
	                     channelId);

	va_list ap;
	va_start(ap, fmt);
	(void)winpr_vasprintf(&msg, &msglen, fmt, ap);
	va_end(ap);

	WLog_PrintTextMessage(log, level, line, file, fkt, "%s: %s", prefix, msg);
	free(prefix);
	free(msg);
}

static PfChannelResult data_cb(pServerContext* ps, pServerDynamicChannelContext* channel,
                               BOOL isBackData, ChannelStateTracker* tracker, BOOL firstPacket,
                               BOOL lastPacket)
{
	WINPR_ASSERT(ps);
	WINPR_ASSERT(channel);
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(ps->pdata);

	wStream* currentPacket = channelTracker_getCurrentPacket(tracker);
	proxyDynChannelInterceptData dyn = { .name = channel->channelName,
		                                 .channelId = channel->channelId,
		                                 .data = currentPacket,
		                                 .isBackData = isBackData,
		                                 .first = firstPacket,
		                                 .last = lastPacket,
		                                 .rewritten = FALSE,
		                                 .packetSize = channelTracker_getCurrentPacketSize(tracker),
		                                 .result = PF_CHANNEL_RESULT_ERROR };
	Stream_SealLength(dyn.data);
	if (!pf_modules_run_filter(ps->pdata->module, FILTER_TYPE_INTERCEPT_CHANNEL, ps->pdata, &dyn))
		return PF_CHANNEL_RESULT_ERROR;

	channelTracker_setCurrentPacketSize(tracker, dyn.packetSize);
	if (dyn.rewritten)
		return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);
	return dyn.result;
}

static pServerDynamicChannelContext* DynamicChannelContext_new(wLog* log, pServerContext* ps,
                                                               const char* name, UINT32 id)
{
	WINPR_ASSERT(log);

	pServerDynamicChannelContext* ret = calloc(1, sizeof(*ret));
	if (!ret)
	{
		WLog_Print(log, WLOG_ERROR, "error allocating dynamic channel context '%s'", name);
		return NULL;
	}

	ret->channelId = id;
	ret->channelName = _strdup(name);
	if (!ret->channelName)
	{
		WLog_Print(log, WLOG_ERROR, "error allocating name in dynamic channel context '%s'", name);
		free(ret);
		return NULL;
	}

	ret->frontTracker.dataCallback = data_cb;
	ret->backTracker.dataCallback = data_cb;

	proxyChannelToInterceptData dyn = { .name = name, .channelId = id, .intercept = FALSE };
	if (pf_modules_run_filter(ps->pdata->module, FILTER_TYPE_DYN_INTERCEPT_LIST, ps->pdata, &dyn) &&
	    dyn.intercept)
		ret->channelMode = PF_UTILS_CHANNEL_INTERCEPT;
	else
		ret->channelMode = pf_utils_get_channel_mode(ps->pdata->config, name);
	ret->openStatus = CHANNEL_OPENSTATE_OPENED;
	ret->packetReassembly = (ret->channelMode == PF_UTILS_CHANNEL_INTERCEPT);

	return ret;
}

static void DynamicChannelContext_free(void* ptr)
{
	pServerDynamicChannelContext* c = (pServerDynamicChannelContext*)ptr;
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

static BOOL ChannelId_Compare(const void* objA, const void* objB)
{
	const UINT32* v1 = objA;
	const UINT32* v2 = objB;
	return (*v1 == *v2);
}

static DynvcReadResult dynvc_read_varInt(wLog* log, wStream* s, size_t len, UINT64* varInt,
                                         BOOL last)
{
	WINPR_ASSERT(varInt);
	switch (len)
	{
		case 0x00:
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 1))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT8(s, *varInt);
			break;
		case 0x01:
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT16(s, *varInt);
			break;
		case 0x02:
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
				return last ? DYNCVC_READ_ERROR : DYNCVC_READ_INCOMPLETE;
			Stream_Read_UINT32(s, *varInt);
			break;
		case 0x03:
		default:
			WLog_Print(log, WLOG_ERROR, "Unknown int len %" PRIuz, len);
			return DYNCVC_READ_ERROR;
	}
	return DYNCVC_READ_OK;
}

static PfChannelResult DynvcTrackerPeekHandleByMode(ChannelStateTracker* tracker,
                                                    DynChannelTrackerState* trackerState,
                                                    pServerDynamicChannelContext* dynChannel,
                                                    BYTE cmd, BOOL firstPacket, BOOL lastPacket)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(trackerState);
	WINPR_ASSERT(dynChannel);
	PfChannelResult result = PF_CHANNEL_RESULT_ERROR;

	DynChannelContext* dynChannelContext =
	    (DynChannelContext*)channelTracker_getCustomData(tracker);
	WINPR_ASSERT(dynChannelContext);

	proxyData* pdata = channelTracker_getPData(tracker);
	WINPR_ASSERT(pdata);

	const BOOL isBackData = (tracker == dynChannelContext->backTracker);
	switch (dynChannel->channelMode)
	{
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			result = channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);
			break;
		case PF_UTILS_CHANNEL_BLOCK:
			channelTracker_setMode(tracker, CHANNEL_TRACKER_DROP);
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
				DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
				                "no intercept callback for channel, dropping packet");
				result = PF_CHANNEL_RESULT_DROP;
			}
			break;
		default:
			DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
			                "unknown channel mode %u", dynChannel->channelMode);
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

static PfChannelResult DynvcTrackerHandleClose(ChannelStateTracker* tracker,
                                               pServerDynamicChannelContext* dynChannel,
                                               DynChannelContext* dynChannelContext,
                                               BOOL firstPacket, BOOL lastPacket)
{
	WINPR_ASSERT(dynChannelContext);

	const BOOL isBackData = (tracker == dynChannelContext->backTracker);

	if (!lastPacket || !dynChannel)
		return PF_CHANNEL_RESULT_DROP;

	DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, CLOSE_REQUEST_PDU, isBackData,
	                "Close request");
	channelTracker_setMode(tracker, CHANNEL_TRACKER_PASS);
	if (dynChannel->openStatus != CHANNEL_OPENSTATE_OPENED)
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, CLOSE_REQUEST_PDU,
		                isBackData, "is in state %s, expected %s",
		                openstatus2str(dynChannel->openStatus),
		                openstatus2str(CHANNEL_OPENSTATE_OPENED));
	}
	dynChannel->openStatus = CHANNEL_OPENSTATE_CLOSED;
	return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);
}

static PfChannelResult DynvcTrackerHandleCreateBack(ChannelStateTracker* tracker, wStream* s,
                                                    DWORD flags, proxyData* pdata,
                                                    pServerDynamicChannelContext* dynChannel,
                                                    DynChannelContext* dynChannelContext,
                                                    UINT64 dynChannelId)
{
	proxyChannelDataEventInfo dev = { 0 };
	const char* name = Stream_ConstPointer(s);
	const size_t nameLen = Stream_GetRemainingLength(s);
	const size_t len = strnlen(name, nameLen);
	const BOOL isBackData = (tracker == dynChannelContext->backTracker);
	const BYTE cmd = CREATE_REQUEST_PDU;

	if ((len == 0) || (len == nameLen) || (dynChannelId > UINT16_MAX))
	{
		char namebuffer[64] = { 0 };
		(void)_snprintf(namebuffer, sizeof(namebuffer) - 1, "%s", name);

		DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
		                "channel id %" PRIu64 ", name=%s [%" PRIuz "|%" PRIuz "], status=%s",
		                dynChannelId, namebuffer, len, nameLen,
		                dynChannel ? openstatus2str(dynChannel->openStatus) : "NULL");
		return PF_CHANNEL_RESULT_ERROR;
	}

	wStream* currentPacket = channelTracker_getCurrentPacket(tracker);
	dev.channel_id = (UINT16)dynChannelId;
	dev.channel_name = name;
	dev.data = Stream_Buffer(s);
	dev.data_len = Stream_GetPosition(currentPacket);
	dev.flags = flags;
	dev.total_size = Stream_GetPosition(currentPacket);

	if (dynChannel)
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_WARN, dynChannel, cmd, isBackData,
		                "Reusing channel id, now %s", name);

		HashTable_Remove(dynChannelContext->channels, &dynChannel->channelId);
	}

	if (!pf_modules_run_filter(pdata->module, FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE,
	                           pdata, &dev))
		return PF_CHANNEL_RESULT_DROP; /* Silently drop */

	dynChannel =
	    DynamicChannelContext_new(dynChannelContext->log, pdata->ps, name, (UINT32)dynChannelId);
	if (!dynChannel)
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
		                "unable to create dynamic channel context data");
		return PF_CHANNEL_RESULT_ERROR;
	}

	DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
	                "Adding channel");
	if (!HashTable_Insert(dynChannelContext->channels, &dynChannel->channelId, dynChannel))
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
		                "unable register dynamic channel context data");
		DynamicChannelContext_free(dynChannel);
		return PF_CHANNEL_RESULT_ERROR;
	}

	dynChannel->openStatus = CHANNEL_OPENSTATE_WAITING_OPEN_STATUS;

	const BOOL firstPacket = (flags & CHANNEL_FLAG_FIRST) ? TRUE : FALSE;
	const BOOL lastPacket = (flags & CHANNEL_FLAG_LAST) ? TRUE : FALSE;

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): HashTable_Insert owns dynChannel
	return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, FALSE);
}

static PfChannelResult DynvcTrackerHandleCreateFront(ChannelStateTracker* tracker, wStream* s,
                                                     DWORD flags,
                                                     WINPR_ATTR_UNUSED proxyData* pdata,
                                                     pServerDynamicChannelContext* dynChannel,
                                                     DynChannelContext* dynChannelContext,
                                                     WINPR_ATTR_UNUSED UINT64 dynChannelId)
{
	const BOOL isBackData = (tracker == dynChannelContext->backTracker);
	const BYTE cmd = CREATE_REQUEST_PDU;

	/* CREATE_REQUEST_PDU response */
	if (!Stream_CheckAndLogRequiredLengthWLogWithBackend(dynChannelContext->log, s, 4, FALSE))
		return PF_CHANNEL_RESULT_ERROR;

	const UINT32 creationStatus = Stream_Get_UINT32(s);
	DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
	                "CREATE_RESPONSE openStatus=%" PRIu32, creationStatus);

	if (dynChannel && (creationStatus == 0))
		dynChannel->openStatus = CHANNEL_OPENSTATE_OPENED;

	const BOOL firstPacket = (flags & CHANNEL_FLAG_FIRST) ? TRUE : FALSE;
	const BOOL lastPacket = (flags & CHANNEL_FLAG_LAST) ? TRUE : FALSE;

	return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, TRUE);
}

static PfChannelResult DynvcTrackerHandleCreate(ChannelStateTracker* tracker, wStream* s,
                                                DWORD flags,
                                                pServerDynamicChannelContext* dynChannel,
                                                UINT64 dynChannelId)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(s);

	DynChannelContext* dynChannelContext =
	    (DynChannelContext*)channelTracker_getCustomData(tracker);
	WINPR_ASSERT(dynChannelContext);

	const BOOL lastPacket = (flags & CHANNEL_FLAG_LAST) ? TRUE : FALSE;
	const BOOL isBackData = (tracker == dynChannelContext->backTracker);

	proxyData* pdata = channelTracker_getPData(tracker);
	WINPR_ASSERT(pdata);

	/* we only want the full packet */
	if (!lastPacket)
		return PF_CHANNEL_RESULT_DROP;

	if (isBackData)
		return DynvcTrackerHandleCreateBack(tracker, s, flags, pdata, dynChannel, dynChannelContext,
		                                    dynChannelId);

	return DynvcTrackerHandleCreateFront(tracker, s, flags, pdata, dynChannel, dynChannelContext,
	                                     dynChannelId);
}

static PfChannelResult DynvcTrackerHandleCmdDATA(ChannelStateTracker* tracker,
                                                 pServerDynamicChannelContext* dynChannel,
                                                 wStream* s, BYTE cmd, UINT64 Length,
                                                 BOOL firstPacket, BOOL lastPacket)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(s);

	DynChannelContext* dynChannelContext =
	    (DynChannelContext*)channelTracker_getCustomData(tracker);
	WINPR_ASSERT(dynChannelContext);

	const BOOL isBackData = (tracker == dynChannelContext->backTracker);

	if (!dynChannel)
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_WARN, dynChannel, cmd, isBackData,
		                "channel is NULL, dropping packet");
		return PF_CHANNEL_RESULT_DROP;
	}

	DynChannelTrackerState* trackerState =
	    isBackData ? &dynChannel->backTracker : &dynChannel->frontTracker;
	if (dynChannel->openStatus != CHANNEL_OPENSTATE_OPENED)
	{
		DynvcTrackerLog(dynChannelContext->log, WLOG_WARN, dynChannel, cmd, isBackData,
		                "channel is not opened, dropping packet");
		return PF_CHANNEL_RESULT_DROP;
	}

	switch (cmd)
	{
		case DATA_FIRST_PDU:
		case DATA_FIRST_COMPRESSED_PDU:
		{
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "DATA_FIRST currentPacketLength=%" PRIu64 "", Length);
			if (Length > UINT32_MAX)
			{
				DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
				                "Length out of bounds: %" PRIu64, Length);
				return PF_CHANNEL_RESULT_ERROR;
			}
			trackerState->currentDataLength = (UINT32)Length;
			trackerState->CurrentDataReceived = 0;
			trackerState->CurrentDataFragments = 0;

			if (dynChannel->packetReassembly)
			{
				if (trackerState->currentPacket)
					Stream_SetPosition(trackerState->currentPacket, 0);
			}
		}
		break;
		default:
			break;
	}

	switch (cmd)
	{
		case DATA_PDU:
		case DATA_FIRST_PDU:
		{
			size_t extraSize = Stream_GetRemainingLength(s);

			trackerState->CurrentDataFragments++;
			trackerState->CurrentDataReceived += WINPR_ASSERTING_INT_CAST(uint32_t, extraSize);

			if (dynChannel->packetReassembly)
			{
				if (!trackerState->currentPacket)
				{
					trackerState->currentPacket = Stream_New(NULL, 1024);
					if (!trackerState->currentPacket)
					{
						DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd,
						                isBackData, "unable to create current packet",
						                getDirection(isBackData), dynChannel->channelName,
						                drdynvc_get_packet_type(cmd));
						return PF_CHANNEL_RESULT_ERROR;
					}
				}

				if (!Stream_EnsureRemainingCapacity(trackerState->currentPacket, extraSize))
				{
					DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
					                "unable to grow current packet", getDirection(isBackData),
					                dynChannel->channelName, drdynvc_get_packet_type(cmd));
					return PF_CHANNEL_RESULT_ERROR;
				}

				Stream_Write(trackerState->currentPacket, Stream_ConstPointer(s), extraSize);
			}
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "frags=%" PRIu32 " received=%" PRIu32 "(%" PRIu32 ")",
			                trackerState->CurrentDataFragments, trackerState->CurrentDataReceived,
			                trackerState->currentDataLength);
		}
		break;
		default:
			break;
	}

	switch (cmd)
	{
		case DATA_PDU:
		{
			if (trackerState->currentDataLength)
			{
				if (trackerState->CurrentDataReceived > trackerState->currentDataLength)
				{
					DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
					                "reassembled packet (%" PRIu32
					                ") is bigger than announced length (%" PRIu32 ")",
					                trackerState->CurrentDataReceived,
					                trackerState->currentDataLength);
					return PF_CHANNEL_RESULT_ERROR;
				}
			}
			else
			{
				trackerState->CurrentDataFragments = 0;
				trackerState->CurrentDataReceived = 0;
			}
		}
		break;
		default:
			break;
	}

	return DynvcTrackerPeekHandleByMode(tracker, trackerState, dynChannel, cmd, firstPacket,
	                                    lastPacket);
}

static PfChannelResult DynvcTrackerHandleCmd(ChannelStateTracker* tracker,
                                             pServerDynamicChannelContext* dynChannel, wStream* s,
                                             BYTE cmd, UINT32 flags, UINT64 Length,
                                             UINT64 dynChannelId, BOOL firstPacket, BOOL lastPacket)
{
	WINPR_ASSERT(tracker);
	WINPR_ASSERT(s);

	DynChannelContext* dynChannelContext =
	    (DynChannelContext*)channelTracker_getCustomData(tracker);
	WINPR_ASSERT(dynChannelContext);

	const BOOL isBackData = (tracker == dynChannelContext->backTracker);
	switch (cmd)
	{
		case CAPABILITY_REQUEST_PDU:
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "CAPABILITY_%s", isBackData ? "REQUEST" : "RESPONSE");
			channelTracker_setMode(tracker, CHANNEL_TRACKER_PASS);
			return PF_CHANNEL_RESULT_PASS;

		case CREATE_REQUEST_PDU:
			return DynvcTrackerHandleCreate(tracker, s, flags, dynChannel, dynChannelId);

		case CLOSE_REQUEST_PDU:
			return DynvcTrackerHandleClose(tracker, dynChannel, dynChannelContext, firstPacket,
			                               lastPacket);

		case SOFT_SYNC_REQUEST_PDU:
			/* just pass then as is for now */
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "SOFT_SYNC_REQUEST_PDU");
			channelTracker_setMode(tracker, CHANNEL_TRACKER_PASS);
			/*TODO: return pf_treat_softsync_req(pdata, s);*/
			return PF_CHANNEL_RESULT_PASS;

		case SOFT_SYNC_RESPONSE_PDU:
			/* just pass then as is for now */
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "SOFT_SYNC_RESPONSE_PDU");
			channelTracker_setMode(tracker, CHANNEL_TRACKER_PASS);
			return PF_CHANNEL_RESULT_PASS;

		case DATA_FIRST_PDU:
		case DATA_PDU:
			return DynvcTrackerHandleCmdDATA(tracker, dynChannel, s, cmd, Length, firstPacket,
			                                 lastPacket);

		case DATA_FIRST_COMPRESSED_PDU:
		case DATA_COMPRESSED_PDU:
			DynvcTrackerLog(dynChannelContext->log, WLOG_DEBUG, dynChannel, cmd, isBackData,
			                "TODO: compressed data packets, pass them as is for now");
			channelTracker_setMode(tracker, CHANNEL_TRACKER_PASS);
			return channelTracker_flushCurrent(tracker, firstPacket, lastPacket, !isBackData);

		default:
			DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
			                "Invalid command ID");
			return PF_CHANNEL_RESULT_ERROR;
	}
}

static PfChannelResult DynvcTrackerPeekFn(ChannelStateTracker* tracker, BOOL firstPacket,
                                          BOOL lastPacket)
{
	wStream* s = NULL;
	wStream sbuffer;
	BOOL haveChannelId = 0;
	BOOL haveLength = 0;
	UINT64 dynChannelId = 0;
	UINT64 Length = 0;
	pServerDynamicChannelContext* dynChannel = NULL;

	WINPR_ASSERT(tracker);

	DynChannelContext* dynChannelContext =
	    (DynChannelContext*)channelTracker_getCustomData(tracker);
	WINPR_ASSERT(dynChannelContext);

	const BOOL isBackData = (tracker == dynChannelContext->backTracker);

	UINT32 flags = lastPacket ? CHANNEL_FLAG_LAST : 0;
	if (firstPacket)
		flags |= CHANNEL_FLAG_FIRST;

	{
		wStream* currentPacket = channelTracker_getCurrentPacket(tracker);
		s = Stream_StaticConstInit(&sbuffer, Stream_Buffer(currentPacket),
		                           Stream_GetPosition(currentPacket));
	}

	if (!Stream_CheckAndLogRequiredLengthWLogWithBackend(dynChannelContext->log, s, 1, isBackData))
		return PF_CHANNEL_RESULT_ERROR;

	const BYTE byte0 = Stream_Get_UINT8(s);
	const BYTE cmd = byte0 >> 4;

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

		switch (dynvc_read_varInt(dynChannelContext->log, s, cbId, &dynChannelId, lastPacket))
		{
			case DYNCVC_READ_OK:
				break;
			case DYNCVC_READ_INCOMPLETE:
				return PF_CHANNEL_RESULT_DROP;
			case DYNCVC_READ_ERROR:
			default:
				DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
				                "invalid channelId field");
				return PF_CHANNEL_RESULT_ERROR;
		}

		/* we always try to retrieve the dynamic channel in case it would have been opened
		 * and closed
		 */
		dynChannel = (pServerDynamicChannelContext*)HashTable_GetItemValue(
		    dynChannelContext->channels, &dynChannelId);
		if ((cmd != CREATE_REQUEST_PDU) || !isBackData)
		{
			if (!dynChannel || (dynChannel->openStatus == CHANNEL_OPENSTATE_CLOSED))
			{
				/* we've not found the target channel, so we drop this chunk, plus all the rest of
				 * the packet */
				channelTracker_setMode(tracker, CHANNEL_TRACKER_DROP);
				return PF_CHANNEL_RESULT_DROP;
			}
		}
	}

	if (haveLength)
	{
		BYTE lenLen = (byte0 >> 2) & 0x03;
		switch (dynvc_read_varInt(dynChannelContext->log, s, lenLen, &Length, lastPacket))
		{
			case DYNCVC_READ_OK:
				break;
			case DYNCVC_READ_INCOMPLETE:
				return PF_CHANNEL_RESULT_DROP;
			case DYNCVC_READ_ERROR:
			default:
				DynvcTrackerLog(dynChannelContext->log, WLOG_ERROR, dynChannel, cmd, isBackData,
				                "invalid length field");
				return PF_CHANNEL_RESULT_ERROR;
		}
	}

	return DynvcTrackerHandleCmd(tracker, dynChannel, s, cmd, flags, Length, dynChannelId,
	                             firstPacket, lastPacket);
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

static const char* dynamic_context(void* arg)
{
	proxyData* pdata = arg;
	if (!pdata)
		return "pdata=null";
	return pdata->session_id;
}

static DynChannelContext* DynChannelContext_new(proxyData* pdata,
                                                pServerStaticChannelContext* channel)
{
	DynChannelContext* dyn = calloc(1, sizeof(DynChannelContext));
	if (!dyn)
		return NULL;

	dyn->log = WLog_Get(DTAG);
	WINPR_ASSERT(dyn->log);
	WLog_SetContext(dyn->log, dynamic_context, pdata);

	dyn->backTracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->backTracker)
		goto fail;
	if (!channelTracker_setPData(dyn->backTracker, pdata))
		goto fail;

	dyn->frontTracker = channelTracker_new(channel, DynvcTrackerPeekFn, dyn);
	if (!dyn->frontTracker)
		goto fail;
	if (!channelTracker_setPData(dyn->frontTracker, pdata))
		goto fail;

	dyn->channels = HashTable_New(FALSE);
	if (!dyn->channels)
		goto fail;

	if (!HashTable_SetHashFunction(dyn->channels, ChannelId_Hash))
		goto fail;

	{
		wObject* kobj = HashTable_KeyObject(dyn->channels);
		WINPR_ASSERT(kobj);
		kobj->fnObjectEquals = ChannelId_Compare;
	}

	{
		wObject* vobj = HashTable_ValueObject(dyn->channels);
		WINPR_ASSERT(vobj);
		vobj->fnObjectFree = DynamicChannelContext_free;
	}

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
