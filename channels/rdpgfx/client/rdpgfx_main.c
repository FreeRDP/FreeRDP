/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#include "rdpgfx_common.h"
#include "rdpgfx_codec.h"

#include "rdpgfx_main.h"

#define TAG CHANNELS_TAG("rdpgfx.client")

static void free_surfaces(RdpgfxClientContext* context, wHashTable* SurfaceTable)
{
	UINT error = 0;
	ULONG_PTR* pKeys = NULL;
	int count;
	int index;

	count = HashTable_GetKeys(SurfaceTable, &pKeys);

	for (index = 0; index < count; index++)
	{
		RDPGFX_DELETE_SURFACE_PDU pdu;
		pdu.surfaceId = ((UINT16)pKeys[index]) - 1;

		if (context)
		{
			IFCALLRET(context->DeleteSurface, error, context, &pdu);

			if (error)
			{
				WLog_ERR(TAG, "context->DeleteSurface failed with error %" PRIu32 "", error);
			}
		}
	}

	free(pKeys);
}

static void evict_cache_slots(RdpgfxClientContext* context, UINT16 MaxCacheSlots, void** CacheSlots)
{
	UINT16 index;

	for (index = 0; index < MaxCacheSlots; index++)
	{
		if (CacheSlots[index])
		{
			RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;
			pdu.cacheSlot = (UINT16)index + 1;

			if (context && context->EvictCacheEntry)
			{
				context->EvictCacheEntry(context, &pdu);
			}

			CacheSlots[index] = NULL;
		}
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_caps_advertise_pdu(RdpgfxClientContext* context,
                                           const RDPGFX_CAPS_ADVERTISE_PDU* pdu)
{
	UINT error = CHANNEL_RC_OK;
	UINT16 index;
	RDPGFX_HEADER header;
	RDPGFX_CAPSET* capsSet;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_CHANNEL_CALLBACK* callback;
	wStream* s;
	gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->listener_callback)
		return ERROR_BAD_ARGUMENTS;

	callback = gfx->listener_callback->channel_callback;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_CAPSADVERTISE;
	header.pduLength = RDPGFX_HEADER_SIZE + 2;

	for (index = 0; index < pdu->capsSetCount; index++)
	{
		capsSet = &(pdu->capsSets[index]);
		header.pduLength += RDPGFX_CAPSET_BASE_SIZE + capsSet->length;
	}

	DEBUG_RDPGFX(gfx->log, "SendCapsAdvertisePdu %" PRIu16 "", pdu->capsSetCount);
	s = Stream_New(NULL, header.pduLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	/* RDPGFX_CAPS_ADVERTISE_PDU */
	Stream_Write_UINT16(s, pdu->capsSetCount); /* capsSetCount (2 bytes) */

	for (index = 0; index < pdu->capsSetCount; index++)
	{
		capsSet = &(pdu->capsSets[index]);
		Stream_Write_UINT32(s, capsSet->version); /* version (4 bytes) */
		Stream_Write_UINT32(s, capsSet->length);  /* capsDataLength (4 bytes) */
		Stream_Write_UINT32(s, capsSet->flags);   /* capsData (4 bytes) */
		Stream_Zero(s, capsSet->length - 4);
	}

	Stream_SealLength(s);
	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 NULL);
fail:
	Stream_Free(s, TRUE);
	return error;
}

static BOOL rdpgfx_is_capability_filtered(RDPGFX_PLUGIN* gfx, UINT32 caps)
{
	const UINT32 filter = gfx->capsFilter;
	const UINT32 capList[] = {
		RDPGFX_CAPVERSION_8,   RDPGFX_CAPVERSION_81,  RDPGFX_CAPVERSION_10,
		RDPGFX_CAPVERSION_101, RDPGFX_CAPVERSION_102, RDPGFX_CAPVERSION_103,
		RDPGFX_CAPVERSION_104, RDPGFX_CAPVERSION_105, RDPGFX_CAPVERSION_106
	};
	UINT32 x;

	for (x = 0; x < ARRAYSIZE(capList); x++)
	{
		if (caps == capList[x])
			return (filter & (1 << x)) != 0;
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_supported_caps(RDPGFX_CHANNEL_CALLBACK* callback)
{
	RDPGFX_PLUGIN* gfx;
	RdpgfxClientContext* context;
	RDPGFX_CAPSET* capsSet;
	RDPGFX_CAPSET capsSets[RDPGFX_NUMBER_CAPSETS] = { 0 };
	RDPGFX_CAPS_ADVERTISE_PDU pdu;

	if (!callback)
		return ERROR_BAD_ARGUMENTS;

	gfx = (RDPGFX_PLUGIN*)callback->plugin;

	if (!gfx)
		return ERROR_BAD_CONFIGURATION;

	context = (RdpgfxClientContext*)gfx->iface.pInterface;

	if (!context)
		return ERROR_BAD_CONFIGURATION;

	pdu.capsSetCount = 0;
	pdu.capsSets = (RDPGFX_CAPSET*)capsSets;

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_8))
	{
		capsSet = &capsSets[pdu.capsSetCount++];
		capsSet->version = RDPGFX_CAPVERSION_8;
		capsSet->length = 4;
		capsSet->flags = 0;

		if (gfx->ThinClient)
			capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

		/* in CAPVERSION_8 the spec says that we should not have both
		 * thinclient and smallcache (and thinclient implies a small cache)
		 */
		if (gfx->SmallCache && !gfx->ThinClient)
			capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_81))
	{
		capsSet = &capsSets[pdu.capsSetCount++];
		capsSet->version = RDPGFX_CAPVERSION_81;
		capsSet->length = 4;
		capsSet->flags = 0;

		if (gfx->ThinClient)
			capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

		if (gfx->SmallCache)
			capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

#ifdef WITH_GFX_H264

		if (gfx->H264)
			capsSet->flags |= RDPGFX_CAPS_FLAG_AVC420_ENABLED;

#endif
	}

	if (!gfx->H264 || gfx->AVC444)
	{
		UINT32 caps10Flags = 0;

		if (gfx->SmallCache)
			caps10Flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

#ifdef WITH_GFX_H264

		if (!gfx->AVC444)
			caps10Flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;

#else
		caps10Flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#endif

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_10))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_10;
			capsSet->length = 4;
			capsSet->flags = caps10Flags;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_101))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_101;
			capsSet->length = 0x10;
			capsSet->flags = 0;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_102))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_102;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}

		if (gfx->ThinClient)
		{
			if ((caps10Flags & RDPGFX_CAPS_FLAG_AVC_DISABLED) == 0)
				caps10Flags |= RDPGFX_CAPS_FLAG_AVC_THINCLIENT;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_103))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_103;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags & ~RDPGFX_CAPS_FLAG_SMALL_CACHE;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_104))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_104;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_105))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_105;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_106))
		{
			capsSet = &capsSets[pdu.capsSetCount++];
			capsSet->version = RDPGFX_CAPVERSION_106;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}
	}

	return IFCALLRESULT(ERROR_BAD_CONFIGURATION, context->CapsAdvertise, context, &pdu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_caps_confirm_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CAPSET capsSet;
	RDPGFX_CAPS_CONFIRM_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	pdu.capsSet = &capsSet;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, capsSet.version); /* version (4 bytes) */
	Stream_Read_UINT32(s, capsSet.length);  /* capsDataLength (4 bytes) */
	Stream_Read_UINT32(s, capsSet.flags);   /* capsData (4 bytes) */
	gfx->ConnectionCaps = capsSet;
	DEBUG_RDPGFX(gfx->log, "RecvCapsConfirmPdu: version: 0x%08" PRIX32 " flags: 0x%08" PRIX32 "",
	             capsSet.version, capsSet.flags);

	if (!context)
		return ERROR_BAD_CONFIGURATION;

	return IFCALLRESULT(CHANNEL_RC_OK, context->CapsConfirm, context, &pdu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_frame_acknowledge_pdu(RdpgfxClientContext* context,
                                              const RDPGFX_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	UINT error;
	wStream* s;
	RDPGFX_HEADER header;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_CHANNEL_CALLBACK* callback;

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->listener_callback)
		return ERROR_BAD_CONFIGURATION;

	callback = gfx->listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_FRAMEACKNOWLEDGE;
	header.pduLength = RDPGFX_HEADER_SIZE + 12;
	DEBUG_RDPGFX(gfx->log, "SendFrameAcknowledgePdu: %" PRIu32 "", pdu->frameId);
	s = Stream_New(NULL, header.pduLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */
	Stream_Write_UINT32(s, pdu->queueDepth);         /* queueDepth (4 bytes) */
	Stream_Write_UINT32(s, pdu->frameId);            /* frameId (4 bytes) */
	Stream_Write_UINT32(s, pdu->totalFramesDecoded); /* totalFramesDecoded (4 bytes) */
	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 NULL);

	if (error == CHANNEL_RC_OK) /* frame successfully acked */
		gfx->UnacknowledgedFrames--;

fail:
	Stream_Free(s, TRUE);
	return error;
}

static UINT rdpgfx_send_qoe_frame_acknowledge_pdu(RdpgfxClientContext* context,
                                                  const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	UINT error;
	wStream* s;
	RDPGFX_HEADER header;
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_PLUGIN* gfx;
	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE;
	header.pduLength = RDPGFX_HEADER_SIZE + 12;

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->listener_callback)
		return ERROR_BAD_CONFIGURATION;

	callback = gfx->listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	DEBUG_RDPGFX(gfx->log, "SendQoeFrameAcknowledgePdu: %" PRIu32 "", pdu->frameId);
	s = Stream_New(NULL, header.pduLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */
	Stream_Write_UINT32(s, pdu->frameId);
	Stream_Write_UINT32(s, pdu->timestamp);
	Stream_Write_UINT16(s, pdu->timeDiffSE);
	Stream_Write_UINT16(s, pdu->timeDiffEDR);
	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 NULL);
fail:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_cache_import_offer_pdu(RdpgfxClientContext* context,
                                               const RDPGFX_CACHE_IMPORT_OFFER_PDU* pdu)
{
	UINT16 index;
	UINT error = CHANNEL_RC_OK;
	wStream* s;
	RDPGFX_PLUGIN* gfx;
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_HEADER header;
	RDPGFX_CACHE_ENTRY_METADATA* cacheEntries;

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->listener_callback)
		return ERROR_BAD_CONFIGURATION;

	callback = gfx->listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	header.flags = 0;
	header.cmdId = RDPGFX_CMDID_CACHEIMPORTOFFER;
	header.pduLength = RDPGFX_HEADER_SIZE + 2 + pdu->cacheEntriesCount * 12;
	DEBUG_RDPGFX(gfx->log, "SendCacheImportOfferPdu: cacheEntriesCount: %" PRIu16 "",
	             pdu->cacheEntriesCount);
	s = Stream_New(NULL, header.pduLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	if (pdu->cacheEntriesCount <= 0)
	{
		WLog_ERR(TAG, "Invalid cacheEntriesCount: %" PRIu16 "", pdu->cacheEntriesCount);
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	/* cacheEntriesCount (2 bytes) */
	Stream_Write_UINT16(s, pdu->cacheEntriesCount);

	for (index = 0; index < pdu->cacheEntriesCount; index++)
	{
		cacheEntries = &(pdu->cacheEntries[index]);
		Stream_Write_UINT64(s, cacheEntries->cacheKey);     /* cacheKey (8 bytes) */
		Stream_Write_UINT32(s, cacheEntries->bitmapLength); /* bitmapLength (4 bytes) */
	}

	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 NULL);

fail:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_reset_graphics_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	int pad;
	UINT32 index;
	MONITOR_DEF* monitor;
	RDPGFX_RESET_GRAPHICS_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;
	GraphicsResetEventArgs graphicsReset;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.width);        /* width (4 bytes) */
	Stream_Read_UINT32(s, pdu.height);       /* height (4 bytes) */
	Stream_Read_UINT32(s, pdu.monitorCount); /* monitorCount (4 bytes) */

	if (Stream_GetRemainingLength(s) < (pdu.monitorCount * 20))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.monitorDefArray = (MONITOR_DEF*)calloc(pdu.monitorCount, sizeof(MONITOR_DEF));

	if (!pdu.monitorDefArray)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.monitorCount; index++)
	{
		monitor = &(pdu.monitorDefArray[index]);
		Stream_Read_UINT32(s, monitor->left);   /* left (4 bytes) */
		Stream_Read_UINT32(s, monitor->top);    /* top (4 bytes) */
		Stream_Read_UINT32(s, monitor->right);  /* right (4 bytes) */
		Stream_Read_UINT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Read_UINT32(s, monitor->flags);  /* flags (4 bytes) */
	}

	pad = 340 - (RDPGFX_HEADER_SIZE + 12 + (pdu.monitorCount * 20));

	if (Stream_GetRemainingLength(s) < (size_t)pad)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "Stream_GetRemainingLength failed!");
		free(pdu.monitorDefArray);
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Seek(s, pad); /* pad (total size is 340 bytes) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvResetGraphicsPdu: width: %" PRIu32 " height: %" PRIu32 " count: %" PRIu32 "",
	             pdu.width, pdu.height, pdu.monitorCount);

	for (index = 0; index < pdu.monitorCount; index++)
	{
		monitor = &(pdu.monitorDefArray[index]);
		DEBUG_RDPGFX(gfx->log,
		             "RecvResetGraphicsPdu: monitor left:%" PRIi32 " top:%" PRIi32 " right:%" PRIi32
		             " left:%" PRIi32 " flags:0x%" PRIx32 "",
		             monitor->left, monitor->top, monitor->right, monitor->bottom, monitor->flags);
	}

	if (context)
	{
		IFCALLRET(context->ResetGraphics, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->ResetGraphics failed with error %" PRIu32 "",
			           error);
	}

	/* some listeners may be interested (namely the display channel) */
	EventArgsInit(&graphicsReset, "libfreerdp");
	graphicsReset.width = pdu.width;
	graphicsReset.height = pdu.height;
	PubSub_OnGraphicsReset(gfx->rdpcontext->pubSub, gfx->rdpcontext, &graphicsReset);
	free(pdu.monitorDefArray);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_evict_cache_entry_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_EVICT_CACHE_ENTRY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	WLog_Print(gfx->log, WLOG_DEBUG, "RecvEvictCacheEntryPdu: cacheSlot: %" PRIu16 "",
	           pdu.cacheSlot);

	if (context)
	{
		IFCALLRET(context->EvictCacheEntry, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->EvictCacheEntry failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_cache_import_reply_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_CACHE_IMPORT_REPLY_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.importedEntriesCount); /* cacheSlot (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t)(pdu.importedEntriesCount * 2))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.cacheSlots = (UINT16*)calloc(pdu.importedEntriesCount, sizeof(UINT16));

	if (!pdu.cacheSlots)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.importedEntriesCount; index++)
	{
		Stream_Read_UINT16(s, pdu.cacheSlots[index]); /* cacheSlot (2 bytes) */
	}

	DEBUG_RDPGFX(gfx->log, "RecvCacheImportReplyPdu: importedEntriesCount: %" PRIu16 "",
	             pdu.importedEntriesCount);

	if (context)
	{
		IFCALLRET(context->CacheImportReply, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->CacheImportReply failed with error %" PRIu32 "", error);
	}

	free(pdu.cacheSlots);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_create_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CREATE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 7)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);  /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.width);      /* width (2 bytes) */
	Stream_Read_UINT16(s, pdu.height);     /* height (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvCreateSurfacePdu: surfaceId: %" PRIu16 " width: %" PRIu16 " height: %" PRIu16
	             " pixelFormat: 0x%02" PRIX8 "",
	             pdu.surfaceId, pdu.width, pdu.height, pdu.pixelFormat);

	if (context)
	{
		IFCALLRET(context->CreateSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->CreateSurface failed with error %" PRIu32 "",
			           error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_delete_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 2)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	DEBUG_RDPGFX(gfx->log, "RecvDeleteSurfacePdu: surfaceId: %" PRIu16 "", pdu.surfaceId);

	if (context)
	{
		IFCALLRET(context->DeleteSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->DeleteSurface failed with error %" PRIu32 "",
			           error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_start_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_START_FRAME_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < RDPGFX_START_FRAME_PDU_SIZE)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.timestamp); /* timestamp (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId);   /* frameId (4 bytes) */
	DEBUG_RDPGFX(gfx->log, "RecvStartFramePdu: frameId: %" PRIu32 " timestamp: 0x%08" PRIX32 "",
	             pdu.frameId, pdu.timestamp);
	gfx->StartDecodingTime = GetTickCount64();

	if (context)
	{
		IFCALLRET(context->StartFrame, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->StartFrame failed with error %" PRIu32 "",
			           error);
	}

	gfx->UnacknowledgedFrames++;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_end_frame_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_END_FRAME_PDU pdu;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU ack;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < RDPGFX_END_FRAME_PDU_SIZE)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */
	DEBUG_RDPGFX(gfx->log, "RecvEndFramePdu: frameId: %" PRIu32 "", pdu.frameId);

	if (context)
	{
		IFCALLRET(context->EndFrame, error, context, &pdu);

		if (error)
		{
			WLog_Print(gfx->log, WLOG_ERROR, "context->EndFrame failed with error %" PRIu32 "",
			           error);
			return error;
		}
	}

	gfx->TotalDecodedFrames++;

	if (!gfx->sendFrameAcks)
		return error;

	ack.frameId = pdu.frameId;
	ack.totalFramesDecoded = gfx->TotalDecodedFrames;

	if (gfx->suspendFrameAcks)
	{
		ack.queueDepth = SUSPEND_FRAME_ACKNOWLEDGEMENT;

		if (gfx->TotalDecodedFrames == 1)
			if ((error = rdpgfx_send_frame_acknowledge_pdu(context, &ack)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_send_frame_acknowledge_pdu failed with error %" PRIu32 "",
				           error);
	}
	else
	{
		ack.queueDepth = QUEUE_DEPTH_UNAVAILABLE;

		if ((error = rdpgfx_send_frame_acknowledge_pdu(context, &ack)))
			WLog_Print(gfx->log, WLOG_ERROR,
			           "rdpgfx_send_frame_acknowledge_pdu failed with error %" PRIu32 "", error);
	}

	switch (gfx->ConnectionCaps.version)
	{
		case RDPGFX_CAPVERSION_10:
		case RDPGFX_CAPVERSION_102:
		case RDPGFX_CAPVERSION_103:
		case RDPGFX_CAPVERSION_104:
		case RDPGFX_CAPVERSION_105:
		case RDPGFX_CAPVERSION_106:
			if (gfx->SendQoeAck)
			{
				RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU qoe;
				UINT64 diff = (GetTickCount64() - gfx->StartDecodingTime);

				if (diff > 65000)
					diff = 0;

				qoe.frameId = pdu.frameId;
				qoe.timestamp = gfx->StartDecodingTime;
				qoe.timeDiffSE = diff;
				qoe.timeDiffEDR = 1;

				if ((error = rdpgfx_send_qoe_frame_acknowledge_pdu(context, &qoe)))
					WLog_Print(gfx->log, WLOG_ERROR,
					           "rdpgfx_send_qoe_frame_acknowledge_pdu failed with error %" PRIu32
					           "",
					           error);
			}

			break;

		default:
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_1_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	UINT error;

	if (Stream_GetRemainingLength(s) < RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);  /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId);    /* codecId (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	if ((error = rdpgfx_read_rect16(s, &(pdu.destRect)))) /* destRect (8 bytes) */
	{
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "", error);
		return error;
	}

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	if (pdu.bitmapDataLength > Stream_GetRemainingLength(s))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	DEBUG_RDPGFX(gfx->log,
	             "RecvWireToSurface1Pdu: surfaceId: %" PRIu16 " codecId: %s (0x%04" PRIX16
	             ") pixelFormat: 0x%02" PRIX8 " "
	             "destRect: left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16
	             " bitmapDataLength: %" PRIu32 "",
	             pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId,
	             pdu.pixelFormat, pdu.destRect.left, pdu.destRect.top, pdu.destRect.right,
	             pdu.destRect.bottom, pdu.bitmapDataLength);
	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.contextId = 0;

	switch (pdu.pixelFormat)
	{
		case GFX_PIXEL_FORMAT_XRGB_8888:
			cmd.format = PIXEL_FORMAT_BGRX32;
			break;

		case GFX_PIXEL_FORMAT_ARGB_8888:
			cmd.format = PIXEL_FORMAT_BGRA32;
			break;

		default:
			return ERROR_INVALID_DATA;
	}

	cmd.left = pdu.destRect.left;
	cmd.top = pdu.destRect.top;
	cmd.right = pdu.destRect.right;
	cmd.bottom = pdu.destRect.bottom;
	cmd.width = cmd.right - cmd.left;
	cmd.height = cmd.bottom - cmd.top;
	cmd.length = pdu.bitmapDataLength;
	cmd.data = pdu.bitmapData;
	cmd.extra = NULL;

	if ((error = rdpgfx_decode(gfx, &cmd)))
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_decode failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_2_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < RDPGFX_WIRE_TO_SURFACE_PDU_2_SIZE)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);        /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId);          /* codecId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId);   /* codecContextId (4 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat);       /* pixelFormat (1 byte) */
	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */
	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);
	DEBUG_RDPGFX(gfx->log,
	             "RecvWireToSurface2Pdu: surfaceId: %" PRIu16 " codecId: %s (0x%04" PRIX16 ") "
	             "codecContextId: %" PRIu32 " pixelFormat: 0x%02" PRIX8
	             " bitmapDataLength: %" PRIu32 "",
	             pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId,
	             pdu.codecContextId, pdu.pixelFormat, pdu.bitmapDataLength);

	cmd.surfaceId = pdu.surfaceId;
	cmd.codecId = pdu.codecId;
	cmd.contextId = pdu.codecContextId;

	switch (pdu.pixelFormat)
	{
		case GFX_PIXEL_FORMAT_XRGB_8888:
			cmd.format = PIXEL_FORMAT_BGRX32;
			break;

		case GFX_PIXEL_FORMAT_ARGB_8888:
			cmd.format = PIXEL_FORMAT_BGRA32;
			break;

		default:
			return ERROR_INVALID_DATA;
	}

	cmd.left = 0;
	cmd.top = 0;
	cmd.right = 0;
	cmd.bottom = 0;
	cmd.width = 0;
	cmd.height = 0;
	cmd.length = pdu.bitmapDataLength;
	cmd.data = pdu.bitmapData;
	cmd.extra = NULL;

	if (context)
	{
		IFCALLRET(context->SurfaceCommand, error, context, &cmd);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->SurfaceCommand failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_delete_encoding_context_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_ENCODING_CONTEXT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);      /* surfaceId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */

	DEBUG_RDPGFX(gfx->log,
	             "RecvDeleteEncodingContextPdu: surfaceId: %" PRIu16 " codecContextId: %" PRIu32 "",
	             pdu.surfaceId, pdu.codecContextId);

	if (context)
	{
		IFCALLRET(context->DeleteEncodingContext, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->DeleteEncodingContext failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_solid_fill_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RECTANGLE_16* fillRect;
	RDPGFX_SOLID_FILL_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	if ((error = rdpgfx_read_color32(s, &(pdu.fillPixel)))) /* fillPixel (4 bytes) */
	{
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_color32 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	Stream_Read_UINT16(s, pdu.fillRectCount); /* fillRectCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t)(pdu.fillRectCount * 8))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.fillRects = (RECTANGLE_16*)calloc(pdu.fillRectCount, sizeof(RECTANGLE_16));

	if (!pdu.fillRects)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.fillRectCount; index++)
	{
		fillRect = &(pdu.fillRects[index]);

		if ((error = rdpgfx_read_rect16(s, fillRect)))
		{
			WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "!",
			           error);
			free(pdu.fillRects);
			return error;
		}
	}
	DEBUG_RDPGFX(gfx->log, "RecvSolidFillPdu: surfaceId: %" PRIu16 " fillRectCount: %" PRIu16 "",
	             pdu.surfaceId, pdu.fillRectCount);

	if (context)
	{
		IFCALLRET(context->SolidFill, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->SolidFill failed with error %" PRIu32 "",
			           error);
	}

	free(pdu.fillRects);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_surface_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_SURFACE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 14)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceIdSrc);  /* surfaceIdSrc (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceIdDest); /* surfaceIdDest (2 bytes) */

	if ((error = rdpgfx_read_rect16(s, &(pdu.rectSrc)))) /* rectSrc (8 bytes ) */
	{
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t)(pdu.destPtsCount * 4))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.destPts = (RDPGFX_POINT16*)calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);

		if ((error = rdpgfx_read_point16(s, destPt)))
		{
			WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_point16 failed with error %" PRIu32 "!",
			           error);
			free(pdu.destPts);
			return error;
		}
	}

	DEBUG_RDPGFX(gfx->log,
	             "RecvSurfaceToSurfacePdu: surfaceIdSrc: %" PRIu16 " surfaceIdDest: %" PRIu16 " "
	             "left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16
	             " destPtsCount: %" PRIu16 "",
	             pdu.surfaceIdSrc, pdu.surfaceIdDest, pdu.rectSrc.left, pdu.rectSrc.top,
	             pdu.rectSrc.right, pdu.rectSrc.bottom, pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->SurfaceToSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->SurfaceToSurface failed with error %" PRIu32 "", error);
	}

	free(pdu.destPts);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_surface_to_cache_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_CACHE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error;

	if (Stream_GetRemainingLength(s) < 20)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.cacheKey);  /* cacheKey (8 bytes) */
	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */

	if ((error = rdpgfx_read_rect16(s, &(pdu.rectSrc)))) /* rectSrc (8 bytes ) */
	{
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	DEBUG_RDPGFX(gfx->log,
	             "RecvSurfaceToCachePdu: surfaceId: %" PRIu16 " cacheKey: 0x%016" PRIX64
	             " cacheSlot: %" PRIu16 " "
	             "left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16 "",
	             pdu.surfaceId, pdu.cacheKey, pdu.cacheSlot, pdu.rectSrc.left, pdu.rectSrc.top,
	             pdu.rectSrc.right, pdu.rectSrc.bottom);

	if (context)
	{
		IFCALLRET(context->SurfaceToCache, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->SurfaceToCache failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_cache_to_surface_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 index;
	RDPGFX_POINT16* destPt;
	RDPGFX_CACHE_TO_SURFACE_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.cacheSlot);    /* cacheSlot (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (Stream_GetRemainingLength(s) < (size_t)(pdu.destPtsCount * 4))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	pdu.destPts = (RDPGFX_POINT16*)calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < pdu.destPtsCount; index++)
	{
		destPt = &(pdu.destPts[index]);

		if ((error = rdpgfx_read_point16(s, destPt)))
		{
			WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_point16 failed with error %" PRIu32 "",
			           error);
			free(pdu.destPts);
			return error;
		}
	}

	DEBUG_RDPGFX(gfx->log,
	             "RdpGfxRecvCacheToSurfacePdu: cacheSlot: %" PRIu16 " surfaceId: %" PRIu16
	             " destPtsCount: %" PRIu16 "",
	             pdu.cacheSlot, pdu.surfaceId, pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->CacheToSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->CacheToSurface failed with error %" PRIu32 "", error);
	}

	free(pdu.destPts);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_map_surface_to_output_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);     /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved);      /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvMapSurfaceToOutputPdu: surfaceId: %" PRIu16 " outputOriginX: %" PRIu32
	             " outputOriginY: %" PRIu32 "",
	             pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY);

	if (context)
	{
		IFCALLRET(context->MapSurfaceToOutput, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->MapSurfaceToOutput failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT rdpgfx_recv_map_surface_to_scaled_output_pdu(RDPGFX_CHANNEL_CALLBACK* callback,
                                                         wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 20)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);     /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved);      /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetWidth);   /* targetWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetHeight);  /* targetHeight (4 bytes) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvMapSurfaceToScaledOutputPdu: surfaceId: %" PRIu16 " outputOriginX: %" PRIu32
	             " outputOriginY: %" PRIu32 " targetWidth: %" PRIu32 " targetHeight: %" PRIu32,
	             pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY, pdu.targetWidth,
	             pdu.targetHeight);

	if (context)
	{
		IFCALLRET(context->MapSurfaceToScaledOutput, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->MapSurfaceToScaledOutput failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_map_surface_to_window_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_WINDOW_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 18)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId);     /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvMapSurfaceToWindowPdu: surfaceId: %" PRIu16 " windowId: 0x%016" PRIX64
	             " mappedWidth: %" PRIu32 " mappedHeight: %" PRIu32 "",
	             pdu.surfaceId, pdu.windowId, pdu.mappedWidth, pdu.mappedHeight);

	if (context && context->MapSurfaceToWindow)
	{
		IFCALLRET(context->MapSurfaceToWindow, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->MapSurfaceToWindow failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT rdpgfx_recv_map_surface_to_scaled_window_pdu(RDPGFX_CHANNEL_CALLBACK* callback,
                                                         wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU pdu;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 26)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId);     /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetWidth);  /* targetWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetHeight); /* targetHeight (4 bytes) */
	DEBUG_RDPGFX(gfx->log,
	             "RecvMapSurfaceToScaledWindowPdu: surfaceId: %" PRIu16 " windowId: 0x%016" PRIX64
	             " mappedWidth: %" PRIu32 " mappedHeight: %" PRIu32 " targetWidth: %" PRIu32
	             " targetHeight: %" PRIu32 "",
	             pdu.surfaceId, pdu.windowId, pdu.mappedWidth, pdu.mappedHeight, pdu.targetWidth,
	             pdu.targetHeight);

	if (context && context->MapSurfaceToScaledWindow)
	{
		IFCALLRET(context->MapSurfaceToScaledWindow, error, context, &pdu);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR,
			           "context->MapSurfaceToScaledWindow failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_pdu(RDPGFX_CHANNEL_CALLBACK* callback, wStream* s)
{
	size_t beg, end;
	RDPGFX_HEADER header;
	UINT error;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	beg = Stream_GetPosition(s);

	if ((error = rdpgfx_read_header(s, &header)))
	{
		WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_read_header failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	DEBUG_RDPGFX(
	    gfx->log, "cmdId: %s (0x%04" PRIX16 ") flags: 0x%04" PRIX16 " pduLength: %" PRIu32 "",
	    rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId, header.flags, header.pduLength);

	switch (header.cmdId)
	{
		case RDPGFX_CMDID_WIRETOSURFACE_1:
			if ((error = rdpgfx_recv_wire_to_surface_1_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_wire_to_surface_1_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_WIRETOSURFACE_2:
			if ((error = rdpgfx_recv_wire_to_surface_2_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_wire_to_surface_2_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_DELETEENCODINGCONTEXT:
			if ((error = rdpgfx_recv_delete_encoding_context_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_delete_encoding_context_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_SOLIDFILL:
			if ((error = rdpgfx_recv_solid_fill_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_solid_fill_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_SURFACETOSURFACE:
			if ((error = rdpgfx_recv_surface_to_surface_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_surface_to_surface_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_SURFACETOCACHE:
			if ((error = rdpgfx_recv_surface_to_cache_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_surface_to_cache_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CACHETOSURFACE:
			if ((error = rdpgfx_recv_cache_to_surface_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_cache_to_surface_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_EVICTCACHEENTRY:
			if ((error = rdpgfx_recv_evict_cache_entry_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_evict_cache_entry_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CREATESURFACE:
			if ((error = rdpgfx_recv_create_surface_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_create_surface_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_DELETESURFACE:
			if ((error = rdpgfx_recv_delete_surface_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_delete_surface_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_STARTFRAME:
			if ((error = rdpgfx_recv_start_frame_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_start_frame_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_ENDFRAME:
			if ((error = rdpgfx_recv_end_frame_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_end_frame_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_RESETGRAPHICS:
			if ((error = rdpgfx_recv_reset_graphics_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_reset_graphics_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOOUTPUT:
			if ((error = rdpgfx_recv_map_surface_to_output_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_output_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CACHEIMPORTREPLY:
			if ((error = rdpgfx_recv_cache_import_reply_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_cache_import_reply_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CAPSCONFIRM:
			if ((error = rdpgfx_recv_caps_confirm_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_caps_confirm_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOWINDOW:
			if ((error = rdpgfx_recv_map_surface_to_window_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_window_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW:
			if ((error = rdpgfx_recv_map_surface_to_scaled_window_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_scaled_window_pdu failed with error %" PRIu32
				           "!",
				           error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT:
			if ((error = rdpgfx_recv_map_surface_to_scaled_output_pdu(callback, s)))
				WLog_Print(gfx->log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_scaled_output_pdu failed with error %" PRIu32
				           "!",
				           error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			break;
	}

	if (error)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "Error while processing GFX cmdId: %s (0x%04" PRIX16 ")",
		           rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId);
		return error;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.pduLength))
	{
		WLog_Print(gfx->log, WLOG_ERROR,
		           "Unexpected gfx pdu end: Actual: %d, Expected: %" PRIu32 "", end,
		           (beg + header.pduLength));
		Stream_SetPosition(s, (beg + header.pduLength));
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	wStream* s;
	int status = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*)pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	UINT error = CHANNEL_RC_OK;
	status = zgfx_decompress(gfx->zgfx, Stream_Pointer(data), Stream_GetRemainingLength(data),
	                         &pDstData, &DstSize, 0);

	if (status < 0)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "zgfx_decompress failure! status: %d", status);
		return ERROR_INTERNAL_ERROR;
	}

	s = Stream_New(pDstData, DstSize);

	if (!s)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	while (Stream_GetPosition(s) < Stream_Length(s))
	{
		if ((error = rdpgfx_recv_pdu(callback, s)))
		{
			WLog_Print(gfx->log, WLOG_ERROR, "rdpgfx_recv_pdu failed with error %" PRIu32 "!",
			           error);
			break;
		}
	}

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*)pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	UINT error = CHANNEL_RC_OK;
	BOOL do_caps_advertise = TRUE;
	gfx->sendFrameAcks = TRUE;

	if (context)
	{
		IFCALLRET(context->OnOpen, error, context, &do_caps_advertise, &gfx->sendFrameAcks);

		if (error)
			WLog_Print(gfx->log, WLOG_ERROR, "context->OnOpen failed with error %" PRIu32 "",
			           error);
	}

	if (do_caps_advertise)
		error = rdpgfx_send_supported_caps(callback);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback = (RDPGFX_CHANNEL_CALLBACK*)pChannelCallback;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;

	DEBUG_RDPGFX(gfx->log, "OnClose");
	free_surfaces(context, gfx->SurfaceTable);
	evict_cache_slots(context, gfx->MaxCacheSlots, gfx->CacheSlots);

	if (gfx->listener_callback)
	{
		free(gfx->listener_callback);
		gfx->listener_callback = NULL;
	}

	free(callback);
	gfx->UnacknowledgedFrames = 0;
	gfx->TotalDecodedFrames = 0;

	if (context)
	{
		IFCALL(context->OnClose, context);
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                             IWTSVirtualChannel* pChannel, BYTE* Data,
                                             BOOL* pbAccept,
                                             IWTSVirtualChannelCallback** ppCallback)
{
	RDPGFX_CHANNEL_CALLBACK* callback;
	RDPGFX_LISTENER_CALLBACK* listener_callback = (RDPGFX_LISTENER_CALLBACK*)pListenerCallback;
	callback = (RDPGFX_CHANNEL_CALLBACK*)calloc(1, sizeof(RDPGFX_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = rdpgfx_on_data_received;
	callback->iface.OnOpen = rdpgfx_on_open;
	callback->iface.OnClose = rdpgfx_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;
	*ppCallback = (IWTSVirtualChannelCallback*)callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT error;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)pPlugin;
	gfx->listener_callback = (RDPGFX_LISTENER_CALLBACK*)calloc(1, sizeof(RDPGFX_LISTENER_CALLBACK));

	if (!gfx->listener_callback)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	gfx->listener_callback->iface.OnNewChannelConnection = rdpgfx_on_new_channel_connection;
	gfx->listener_callback->plugin = pPlugin;
	gfx->listener_callback->channel_mgr = pChannelMgr;
	error = pChannelMgr->CreateListener(pChannelMgr, RDPGFX_DVC_CHANNEL_NAME, 0,
	                                    (IWTSListenerCallback*)gfx->listener_callback,
	                                    &(gfx->listener));
	gfx->listener->pInterface = gfx->iface.pInterface;
	DEBUG_RDPGFX(gfx->log, "Initialize");
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)pPlugin;
	RdpgfxClientContext* context = (RdpgfxClientContext*)gfx->iface.pInterface;
	DEBUG_RDPGFX(gfx->log, "Terminated");
	rdpgfx_client_context_free(context);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_surface_data(RdpgfxClientContext* context, UINT16 surfaceId, void* pData)
{
	ULONG_PTR key;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	key = ((ULONG_PTR)surfaceId) + 1;

	if (pData)
		HashTable_Add(gfx->SurfaceTable, (void*)key, pData);
	else
		HashTable_Remove(gfx->SurfaceTable, (void*)key);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_get_surface_ids(RdpgfxClientContext* context, UINT16** ppSurfaceIds,
                                   UINT16* count_out)
{
	int count;
	int index;
	UINT16* pSurfaceIds;
	ULONG_PTR* pKeys = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	if (count < 1)
	{
		*count_out = 0;
		return CHANNEL_RC_OK;
	}

	pSurfaceIds = (UINT16*)calloc(count, sizeof(UINT16));

	if (!pSurfaceIds)
	{
		WLog_Print(gfx->log, WLOG_ERROR, "calloc failed!");
		free(pKeys);
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < count; index++)
	{
		pSurfaceIds[index] = pKeys[index] - 1;
	}

	free(pKeys);
	*ppSurfaceIds = pSurfaceIds;
	*count_out = (UINT16)count;
	return CHANNEL_RC_OK;
}

static void* rdpgfx_get_surface_data(RdpgfxClientContext* context, UINT16 surfaceId)
{
	ULONG_PTR key;
	void* pData = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	key = ((ULONG_PTR)surfaceId) + 1;
	pData = HashTable_GetItemValue(gfx->SurfaceTable, (void*)key);
	return pData;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	/* Microsoft uses 1-based indexing for the egfx bitmap cache ! */
	if (cacheSlot == 0 || cacheSlot > gfx->MaxCacheSlots)
	{
		WLog_ERR(TAG, "%s: invalid cache slot %" PRIu16 ", must be between 1 and %" PRIu16 "",
		         __FUNCTION__, cacheSlot, gfx->MaxCacheSlots);
		return ERROR_INVALID_INDEX;
	}

	gfx->CacheSlots[cacheSlot - 1] = pData;
	return CHANNEL_RC_OK;
}

static void* rdpgfx_get_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot)
{
	void* pData = NULL;
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	/* Microsoft uses 1-based indexing for the egfx bitmap cache ! */
	if (cacheSlot == 0 || cacheSlot > gfx->MaxCacheSlots)
	{
		WLog_ERR(TAG, "%s: invalid cache slot %" PRIu16 ", must be between 1 and %" PRIu16 "",
		         __FUNCTION__, cacheSlot, gfx->MaxCacheSlots);
		return NULL;
	}

	pData = gfx->CacheSlots[cacheSlot - 1];
	return pData;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry rdpgfx_DVCPluginEntry
#else
#define DVCPluginEntry FREERDP_API DVCPluginEntry
#endif

RdpgfxClientContext* rdpgfx_client_context_new(rdpSettings* settings)
{
	RDPGFX_PLUGIN* gfx;
	RdpgfxClientContext* context;

	gfx = (RDPGFX_PLUGIN*)calloc(1, sizeof(RDPGFX_PLUGIN));

	if (!gfx)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	gfx->log = WLog_Get(TAG);

	if (!gfx->log)
	{
		free(gfx);
		WLog_ERR(TAG, "Failed to acquire reference to WLog %s", TAG);
		return NULL;
	}

	gfx->settings = settings;
	gfx->rdpcontext = ((freerdp*)gfx->settings->instance)->context;
	gfx->SurfaceTable = HashTable_New(TRUE);

	if (!gfx->SurfaceTable)
	{
		free(gfx);
		WLog_ERR(TAG, "HashTable_New failed!");
		return NULL;
	}

	gfx->ThinClient = gfx->settings->GfxThinClient;
	gfx->SmallCache = gfx->settings->GfxSmallCache;
	gfx->Progressive = gfx->settings->GfxProgressive;
	gfx->ProgressiveV2 = gfx->settings->GfxProgressiveV2;
	gfx->H264 = gfx->settings->GfxH264;
	gfx->AVC444 = gfx->settings->GfxAVC444;
	gfx->SendQoeAck = gfx->settings->GfxSendQoeAck;
	gfx->capsFilter = gfx->settings->GfxCapsFilter;

	if (gfx->H264)
		gfx->SmallCache = TRUE;

	gfx->MaxCacheSlots = gfx->SmallCache ? 4096 : 25600;
	context = (RdpgfxClientContext*)calloc(1, sizeof(RdpgfxClientContext));

	if (!context)
	{
		free(gfx);
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	context->handle = (void*)gfx;
	context->GetSurfaceIds = rdpgfx_get_surface_ids;
	context->SetSurfaceData = rdpgfx_set_surface_data;
	context->GetSurfaceData = rdpgfx_get_surface_data;
	context->SetCacheSlotData = rdpgfx_set_cache_slot_data;
	context->GetCacheSlotData = rdpgfx_get_cache_slot_data;
	context->CapsAdvertise = rdpgfx_send_caps_advertise_pdu;
	context->FrameAcknowledge = rdpgfx_send_frame_acknowledge_pdu;
	context->CacheImportOffer = rdpgfx_send_cache_import_offer_pdu;
	context->QoeFrameAcknowledge = rdpgfx_send_qoe_frame_acknowledge_pdu;

	gfx->iface.pInterface = (void*)context;
	gfx->zgfx = zgfx_context_new(FALSE);

	if (!gfx->zgfx)
	{
		free(gfx);
		free(context);
		WLog_ERR(TAG, "zgfx_context_new failed!");
		return NULL;
	}

	return context;
}

void rdpgfx_client_context_free(RdpgfxClientContext* context)
{

	RDPGFX_PLUGIN* gfx;

	if (!context)
		return;

	gfx = (RDPGFX_PLUGIN*)context->handle;

	free_surfaces(context, gfx->SurfaceTable);
	evict_cache_slots(context, gfx->MaxCacheSlots, gfx->CacheSlots);

	if (gfx->listener_callback)
	{
		free(gfx->listener_callback);
		gfx->listener_callback = NULL;
	}

	if (gfx->zgfx)
	{
		zgfx_context_free(gfx->zgfx);
		gfx->zgfx = NULL;
	}

	HashTable_Free(gfx->SurfaceTable);
	free(context);
	free(gfx);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_PLUGIN* gfx;
	RdpgfxClientContext* context;
	gfx = (RDPGFX_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, "rdpgfx");

	if (!gfx)
	{
		context =
		    rdpgfx_client_context_new((rdpSettings*)pEntryPoints->GetRdpSettings(pEntryPoints));

		if (!context)
		{
			WLog_ERR(TAG, "rdpgfx_client_context_new failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		gfx = (RDPGFX_PLUGIN*)context->handle;

		gfx->iface.Initialize = rdpgfx_plugin_initialize;
		gfx->iface.Connected = NULL;
		gfx->iface.Disconnected = NULL;
		gfx->iface.Terminated = rdpgfx_plugin_terminated;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpgfx", (IWTSPlugin*)gfx);
	}

	return error;
}
