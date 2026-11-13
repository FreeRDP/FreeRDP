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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/cast.h>

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
#include <freerdp/codec/h264.h>

#include "rdpgfx_common.h"
#include "rdpgfx_codec.h"

#include "rdpgfx_main.h"

#define GFXTAG CHANNELS_TAG("rdpgfx.client")

#define RDPGFX_NUMBER_CAPSETS 0x100

typedef struct
{
	uint64_t cntGfxCodecID[RDPGFX_CODECID_MAX];
	uint64_t cntGfxCommandID[RDPGFX_CMDID_MAX + 1];
	uint64_t bytesGfxCodecID[RDPGFX_CODECID_MAX];
	uint64_t framesDecoded;
	uint64_t framesUnacknowledged;
} RdpgfxClientContextStats;

typedef struct
{
	RdpgfxClientContext common;
	RdpgfxClientContextStats stats;
} RdpgfxClientContextInt;

/* The name of a byte counter is the name of its codec ID with _BYTES added */
static char bytesGfxCodecNames[RDPGFX_CODECID_MAX][64];
static INIT_ONCE bytesGfxCodecNamesOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK rdpgfx_stats_init_names(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                             WINPR_ATTR_UNUSED PVOID param,
                                             WINPR_ATTR_UNUSED PVOID* context)
{
	for (UINT16 x = 0; x < RDPGFX_CODECID_MAX; x++)
		(void)_snprintf(bytesGfxCodecNames[x], sizeof(bytesGfxCodecNames[x]), "%s_BYTES",
		                rdpgfx_get_codec_id_string(x));
	return TRUE;
}

static const UINT32 MAX_SURFACE_SIZE = 32766;
static const UINT32 MAX_MONITOR_COUNT = 16;

size_t rdpgfx_stats_max_index(void)
{
	return sizeof(RdpgfxClientContextStats) / sizeof(uint64_t);
}

const char* rdpgfx_stats_name_for_index(size_t index)
{
	if (index < RDPGFX_CODECID_MAX)
		return rdpgfx_get_codec_id_string(WINPR_ASSERTING_INT_CAST(UINT16, index));

	index -= RDPGFX_CODECID_MAX;
	if (index < RDPGFX_CMDID_MAX)
		return rdpgfx_get_cmd_id_string(WINPR_ASSERTING_INT_CAST(UINT16, index));
	if (index == RDPGFX_CMDID_MAX)
		return "RDPGFX_CMDID_INVALID";

	index -= RDPGFX_CMDID_MAX + 1;
	if (index < RDPGFX_CODECID_MAX)
	{
		if (!InitOnceExecuteOnce(&bytesGfxCodecNamesOnce, rdpgfx_stats_init_names, nullptr,
		                         nullptr))
			return "RDPGFX_STATS_UNUSED";
		return bytesGfxCodecNames[index];
	}

	index -= RDPGFX_CODECID_MAX;
	if (index == 0)
		return "RDPGFX_FRAMES_DECODED";
	if (index == 1)
		return "RDPGFX_FRAMES_UNACKNOWLEDGED";
	return "RDPGFX_STATS_UNUSED";
}

uint64_t rdpgfx_stats_value_for_index(RdpgfxClientContext* context, size_t index)
{
	WINPR_ASSERT(context);

	RdpgfxClientContextInt* intCtx = (RdpgfxClientContextInt*)context;

	if (index < RDPGFX_CODECID_MAX)
		return intCtx->stats.cntGfxCodecID[WINPR_ASSERTING_INT_CAST(UINT16, index)];

	index -= RDPGFX_CODECID_MAX;
	if (index <= RDPGFX_CMDID_MAX)
		return intCtx->stats.cntGfxCommandID[WINPR_ASSERTING_INT_CAST(UINT16, index)];

	index -= RDPGFX_CMDID_MAX + 1;
	if (index < RDPGFX_CODECID_MAX)
		return intCtx->stats.bytesGfxCodecID[WINPR_ASSERTING_INT_CAST(UINT16, index)];

	index -= RDPGFX_CODECID_MAX;
	if (index == 0)
		return intCtx->stats.framesDecoded;
	if (index == 1)
		return intCtx->stats.framesUnacknowledged;
	return 0;
}

static void rdpgfx_codecid_event(RdpgfxClientContext* context, uint32_t index, uint32_t length)
{
	if (!context)
		return;
	RdpgfxClientContextInt* intCtx = (RdpgfxClientContextInt*)context;
	WINPR_ASSERT(intCtx);
	if (index < RDPGFX_CODECID_MAX)
	{
		intCtx->stats.cntGfxCodecID[index]++;
		intCtx->stats.bytesGfxCodecID[index] += length;
	}
	else
	{
		RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
		WINPR_ASSERT(gfx);
		WLog_Print(gfx->base.log, WLOG_WARN, "Invalid event index %" PRIu32, index);
	}
}

static void rdpgfx_stats_cmdid_event(RdpgfxClientContext* context, uint32_t index)
{
	if (!context)
		return;
	RdpgfxClientContextInt* intCtx = (RdpgfxClientContextInt*)context;
	WINPR_ASSERT(intCtx);

	if (index >= RDPGFX_CMDID_MAX)
	{
		RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
		WINPR_ASSERT(gfx);
		WLog_Print(gfx->base.log, WLOG_WARN, "Invalid codecID %" PRIu32, index);
		index = RDPGFX_CMDID_MAX;
	}
	intCtx->stats.cntGfxCommandID[index]++;
}

static void rdpgfx_stats_frame_decoded(RdpgfxClientContext* context)
{
	if (!context)
		return;
	RdpgfxClientContextInt* intCtx = (RdpgfxClientContextInt*)context;
	intCtx->stats.framesDecoded++;
}

/* A copy: the plugin's own counter is reset before the stats are dumped on close */
static void rdpgfx_stats_frames_unacknowledged(RdpgfxClientContext* context, UINT32 frames)
{
	if (!context)
		return;
	RdpgfxClientContextInt* intCtx = (RdpgfxClientContextInt*)context;
	intCtx->stats.framesUnacknowledged = frames;
}

static BOOL delete_surface(const void* key, void* value, void* arg)
{
	const ULONG_PTR id = (ULONG_PTR)(key);
	if ((id < 1) || (id - 1 > UINT16_MAX))
		return FALSE;
	const UINT16 uid = WINPR_ASSERTING_INT_CAST(UINT16, id - 1);

	RdpgfxClientContext* context = arg;
	const RDPGFX_DELETE_SURFACE_PDU pdu = { .surfaceId = uid };

	WINPR_UNUSED(value);

	if (context)
	{
		UINT error = CHANNEL_RC_OK;
		IFCALLRET(context->DeleteSurface, error, context, &pdu);

		if (error)
		{
			RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
			WINPR_ASSERT(gfx);
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->DeleteSurface failed with error %" PRIu32 "", error);
		}
	}
	return TRUE;
}

static void free_surfaces(RdpgfxClientContext* context, wHashTable* SurfaceTable)
{
	WINPR_ASSERT(context);
	if (!HashTable_Foreach(SurfaceTable, delete_surface, context))
	{
		RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
		WINPR_ASSERT(gfx);
		WLog_Print(gfx->base.log, WLOG_WARN, "delete_surface failed");
	}
}

static UINT evict_cache_slots(RdpgfxClientContext* context, UINT16 MaxCacheSlots, void** CacheSlots)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(CacheSlots);
	for (UINT16 index = 0; index < MaxCacheSlots; index++)
	{
		if (CacheSlots[index])
		{
			const RDPGFX_EVICT_CACHE_ENTRY_PDU pdu = { .cacheSlot = index + 1 };

			if (context && context->EvictCacheEntry)
			{
				const UINT rc = context->EvictCacheEntry(context, &pdu);
				if (rc != CHANNEL_RC_OK)
					error = rc;
			}

			CacheSlots[index] = nullptr;
		}
	}
	return error;
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

	WINPR_ASSERT(pdu);
	WINPR_ASSERT(context);

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->base.listener_callback)
		return ERROR_BAD_ARGUMENTS;

	memset(gfx->allowedCodecs, FALSE, sizeof(gfx->allowedCodecs));

	GENERIC_CHANNEL_CALLBACK* callback = gfx->base.listener_callback->channel_callback;

	RDPGFX_HEADER header = { .flags = 0,
		                     .cmdId = RDPGFX_CMDID_CAPSADVERTISE,
		                     .pduLength = RDPGFX_HEADER_SIZE + 2 };

	for (UINT16 index = 0; index < pdu->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* capsSet = &(pdu->capsSets[index]);
		header.pduLength += RDPGFX_CAPSET_BASE_SIZE + capsSet->length;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG, "SendCapsAdvertisePdu %" PRIu16 "", pdu->capsSetCount);
	wStream* s = Stream_New(nullptr, header.pduLength);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	/* RDPGFX_CAPS_ADVERTISE_PDU */
	Stream_Write_UINT16(s, pdu->capsSetCount); /* capsSetCount (2 bytes) */

	for (UINT16 index = 0; index < pdu->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* capsSet = &(pdu->capsSets[index]);

		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_Print(gfx->base.log, WLOG_DEBUG, "Sending %s [0x%08" PRIx32 "] flags=%s",
		           rdpgfx_caps_version_str(capsSet->version), capsSet->version,
		           rdpgfx_caps_flags_str(capsSet->flags, buffer, sizeof(buffer)));

		Stream_Write_UINT32(s, capsSet->version); /* version (4 bytes) */
		Stream_Write_UINT32(s, capsSet->length);  /* capsDataLength (4 bytes) */
		Stream_Write_UINT32(s, capsSet->flags);   /* capsData (4 bytes) */
		if (capsSet->length < 4)
			goto fail;
		Stream_Zero(s, capsSet->length - 4);
	}

	Stream_SealLength(s);
	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 nullptr);
fail:
	Stream_Free(s, TRUE);
	return error;
}

static BOOL rdpgfx_is_capability_filtered(RDPGFX_PLUGIN* gfx, UINT32 caps)
{
	WINPR_ASSERT(gfx);
	const UINT32 filter =
	    freerdp_settings_get_uint32(gfx->rdpcontext->settings, FreeRDP_GfxCapsFilter);
	const UINT32 capList[] = {
#if defined(WITH_GFX_AV1)
		RDPGFX_CAPVERSION_FRDP_1,
#endif
#if defined(WITH_GFX_AV2)
		RDPGFX_CAPVERSION_FRDP_2,
#endif
		RDPGFX_CAPVERSION_8,
		RDPGFX_CAPVERSION_81,
		RDPGFX_CAPVERSION_10,
		RDPGFX_CAPVERSION_101,
		RDPGFX_CAPVERSION_102,
		RDPGFX_CAPVERSION_103,
		RDPGFX_CAPVERSION_104,
		RDPGFX_CAPVERSION_105,
		RDPGFX_CAPVERSION_106,
		RDPGFX_CAPVERSION_106_ERR,
		RDPGFX_CAPVERSION_107
#if defined(WITH_GFX_AZURE)
		,
		RDPGFX_CAPVERSION_111,
		RDPGFX_CAPVERSION_112,
		RDPGFX_CAPVERSION_113,
		RDPGFX_CAPVERSION_115
#endif
	};

	for (size_t x = 0; x < ARRAYSIZE(capList); x++)
	{
		if (caps == capList[x])
			return (filter & (1u << x)) != 0;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static RDPGFX_CAPSET* nextCapset(RDPGFX_CAPS_ADVERTISE_PDU* pdu, size_t count)
{
	WINPR_ASSERT(pdu);
	WINPR_ASSERT(pdu->capsSets);
	WINPR_ASSERT(count > 0);
	WINPR_ASSERT(pdu->capsSetCount < count);
	if (pdu->capsSetCount >= count)
		return nullptr;
	return &pdu->capsSets[pdu->capsSetCount++];
}

#if defined(WITH_GFX_AZURE)
/* Only offer HEVC if it can be decoded: a server confirming it sends HEVC420 frames. */
WINPR_ATTR_NODISCARD
static BOOL rdpgfx_hevc_enabled(RDPGFX_PLUGIN* gfx)
{
	WINPR_ASSERT(gfx);

	if (!freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxCodecHEVC))
		return FALSE;
	if (freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_HEVC_I420, FALSE))
		return TRUE;

	WLog_Print(gfx->base.log, WLOG_WARN,
	           "HEVC mode enabled, but decoder does not support it. Disabling.");
	return FALSE;
}
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_supported_caps(GENERIC_CHANNEL_CALLBACK* callback)
{
	RDPGFX_CAPSET capsSets[RDPGFX_NUMBER_CAPSETS] = WINPR_C_ARRAY_INIT;

	if (!callback)
		return ERROR_BAD_ARGUMENTS;

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	if (!gfx)
		return ERROR_BAD_CONFIGURATION;

	RdpgfxClientContext* context = gfx->context;

	if (!context)
		return ERROR_BAD_CONFIGURATION;

	RDPGFX_CAPS_ADVERTISE_PDU pdu = { .capsSetCount = 0, .capsSets = capsSets };

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_8))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_8;
		capsSet->length = 4;
		capsSet->flags = 0;

		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxThinClient))
			capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

		/* in CAPVERSION_8 the spec says that we should not have both
		 * thinclient and smallcache (and thinclient implies a small cache)
		 */
		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSmallCache) &&
		    !freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxThinClient))
			capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_81))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_81;
		capsSet->length = 4;
		capsSet->flags = 0;

		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxThinClient))
			capsSet->flags |= RDPGFX_CAPS_FLAG_THINCLIENT;

		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSmallCache))
			capsSet->flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxH264) &&
		    freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AVC420, FALSE))
			capsSet->flags |= RDPGFX_CAPS_FLAG_AVC420_ENABLED;
	}

	UINT32 caps10Flags = 0;
	if (!freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxH264) ||
	    freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxAVC444))
	{
		if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSmallCache))
			caps10Flags |= RDPGFX_CAPS_FLAG_SMALL_CACHE;

		if (!freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxAVC444) ||
		    !freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AVC444, FALSE))
			caps10Flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_10))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_10;
		capsSet->length = 4;
		capsSet->flags = caps10Flags;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_101))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_101;
		capsSet->length = 0x10;
		capsSet->flags = 0;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_102))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_102;
		capsSet->length = 0x4;
		capsSet->flags = caps10Flags;
	}

	if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxThinClient))
	{
		if ((caps10Flags & RDPGFX_CAPS_FLAG_AVC_DISABLED) == 0)
			caps10Flags |= RDPGFX_CAPS_FLAG_AVC_THINCLIENT;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_103))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_103;
		capsSet->length = 0x4;
		capsSet->flags = caps10Flags & ~RDPGFX_CAPS_FLAG_SMALL_CACHE;
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_104))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_104;
		capsSet->length = 0x4;
		capsSet->flags = caps10Flags;
	}

	/* The following capabilities expect support for image scaling.
	 * Disable these for builds that do not have support for that.
	 */
	if (freerdp_image_scaling_supported())
	{
		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_105))
		{
			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = RDPGFX_CAPVERSION_105;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_106))
		{
			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = RDPGFX_CAPVERSION_106;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}

		if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_106_ERR))
		{
			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = RDPGFX_CAPVERSION_106_ERR;
			capsSet->length = 0x4;
			capsSet->flags = caps10Flags;
		}
	}

	if (!rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_107))
	{
		RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
		if (!capsSet)
			return ERROR_BAD_CONFIGURATION;
		capsSet->version = RDPGFX_CAPVERSION_107;
		capsSet->length = 0x4;
		capsSet->flags = caps10Flags;
		if (!freerdp_image_scaling_supported())
			capsSet->flags |= RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE;
	}

#if defined(WITH_GFX_AZURE)
	if (freerdp_image_scaling_supported())
	{
		const UINT32 caps[] = { RDPGFX_CAPVERSION_111, RDPGFX_CAPVERSION_112, RDPGFX_CAPVERSION_113,
			                    RDPGFX_CAPVERSION_115 };
		const BOOL hevc = rdpgfx_hevc_enabled(gfx);
		const BOOL hevc444 =
		    hevc && freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxCodecHEVC444) &&
		    freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_HEVC_I444, FALSE);

		for (size_t x = 0; x < ARRAYSIZE(caps); x++)
		{
			const UINT32 cap = caps[x];

			if (rdpgfx_is_capability_filtered(gfx, cap))
				continue;

			/* the flags differ between the versions, keep them out of caps10Flags */
			UINT32 flags = caps10Flags;

			/* HEVC420 is valid from RDPGFX_CAPVERSION_112 on. AVC stays enabled: the server
			 * falls back to it if it has no HEVC encoder. */
			if (hevc && (cap >= RDPGFX_CAPVERSION_112))
				flags |= RDPGFX_CAPS_FLAG_HEVC_ENABLE;

			/* HEVC444 is only valid in RDPGFX_CAPVERSION_115 */
			if (hevc444 && (cap >= RDPGFX_CAPVERSION_115))
				flags |= RDPGFX_CAPS_FLAG_HEVC_I444;

			/* AVC mixed mode is valid in RDPGFX_CAPVERSION_113 and RDPGFX_CAPVERSION_115.
			 * The server then mixes AVC420 or AVC444 regions with the other codecs on a
			 * surface, all of which are decoded already, so offer it whenever AVC is. */
			if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxCodecAVCMM))
			{
				if ((cap >= RDPGFX_CAPVERSION_113) &&
				    ((flags & RDPGFX_CAPS_FLAG_AVC_DISABLED) == 0))
					flags |= RDPGFX_CAPS_FLAG_AVCMM_ENABLE;
			}

			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = cap;
			capsSet->length = 0x4;
			capsSet->flags = flags;
			if (!freerdp_image_scaling_supported())
				capsSet->flags |= RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE;
		}
	}
#endif

#if defined(WITH_GFX_AV1)
	if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxCodecAV1))
	{
		if (freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AV1_I420, FALSE) &&
		    !rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_FRDP_1))
		{
			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = RDPGFX_CAPVERSION_FRDP_1;
			capsSet->length = 0x4;

			if (freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AV1_I444, FALSE))
				capsSet->flags = caps10Flags | RDPGFX_CAPS_FLAG_AV1_I444_SUPPORTED;
			else
				capsSet->flags |= RDPGFX_CAPS_FLAG_AV1_I444_DISABLED;

			const UINT32 profile =
			    freerdp_settings_get_uint32(gfx->rdpcontext->settings, FreeRDP_GfxCodecAV1Profile);
			if (profile == 0)
				capsSet->flags |= RDPGFX_CAPS_FLAG_AV1_I444_DISABLED;

			if (!freerdp_image_scaling_supported())
				capsSet->flags |= RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE;
		}
	}
#endif

#if defined(WITH_GFX_AV2)
	/* A capability set of its own: a server that confirms it may send AV2, and one that does
	 * not know it never confirms it. */
	if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxCodecAV2))
	{
		if (freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AV2_I420, FALSE) &&
		    !rdpgfx_is_capability_filtered(gfx, RDPGFX_CAPVERSION_FRDP_2))
		{
			RDPGFX_CAPSET* capsSet = nextCapset(&pdu, ARRAYSIZE(capsSets));
			if (!capsSet)
				return ERROR_BAD_CONFIGURATION;
			capsSet->version = RDPGFX_CAPVERSION_FRDP_2;
			capsSet->length = 0x4;

			if (freerdp_codec_supported(gfx->rdpcontext, FREERDP_CODEC_AV2_I444, FALSE))
			{
				capsSet->flags = caps10Flags | RDPGFX_CAPS_FLAG_AV1_I444_SUPPORTED;

				const UINT32 profile = freerdp_settings_get_uint32(gfx->rdpcontext->settings,
				                                                   FreeRDP_GfxCodecAV2Profile);
				if (profile == 0)
					capsSet->flags |= RDPGFX_CAPS_FLAG_AV1_I444_DISABLED;
			}
			else
			{
				/* the decoder (dav2d) has no AV2 4:4:4: always ask for 4:2:0 */
				capsSet->flags = caps10Flags | RDPGFX_CAPS_FLAG_AV1_I444_DISABLED;
			}

			if (!freerdp_image_scaling_supported())
				capsSet->flags |= RDPGFX_CAPS_FLAG_SCALEDMAP_DISABLE;
		}
	}
#endif

	return IFCALLRESULT(ERROR_BAD_CONFIGURATION, context->CapsAdvertise, context, &pdu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_caps_confirm_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;

	RDPGFX_CAPSET capsSet = WINPR_C_ARRAY_INIT;
	RDPGFX_CAPS_CONFIRM_PDU pdu = { .capsSet = &capsSet };

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, capsSet.version); /* version (4 bytes) */
	Stream_Read_UINT32(s, capsSet.length);  /* capsDataLength (4 bytes) */
	Stream_Read_UINT32(s, capsSet.flags);   /* capsData (4 bytes) */
	gfx->TotalDecodedFrames = 0;
	gfx->ConnectionCaps = capsSet;
	if (context)
		context->CapsVersion = capsSet.version;
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;

		WLog_Print(gfx->base.log, WLOG_DEBUG,
		           "RecvCapsConfirmPdu: version: %s [0x%08" PRIX32 "] flags: %s",
		           rdpgfx_caps_version_str(capsSet.version), capsSet.version,
		           rdpgfx_caps_flags_str(capsSet.flags, buffer, sizeof(buffer)));
	}
	memset(gfx->allowedCodecs, TRUE, sizeof(gfx->allowedCodecs));

	gfx->allowedCodecs[RDPGFX_CODECID_AVC420] = FALSE;
	gfx->allowedCodecs[RDPGFX_CODECID_AVC444] = FALSE;
	gfx->allowedCodecs[RDPGFX_CODECID_AVC444v2] = FALSE;
#if defined(WITH_GFX_AV1) || defined(WITH_GFX_AV2)
	gfx->allowedCodecs[RDPGFX_CODECID_AV1] = FALSE;
#endif

	if (capsSet.version >= RDPGFX_CAPVERSION_10)
	{
		if ((capsSet.flags & RDPGFX_CAPS_FLAG_AVC_DISABLED) == 0)
		{
			WLog_Print(gfx->base.log, WLOG_INFO, "AVC not disabled by server");
			gfx->allowedCodecs[RDPGFX_CODECID_AVC420] = TRUE;
			gfx->allowedCodecs[RDPGFX_CODECID_AVC444] = TRUE;
		}
	}

	if (capsSet.version >= RDPGFX_CAPVERSION_103)
	{
		if ((capsSet.flags & RDPGFX_CAPS_FLAG_AVC_DISABLED) == 0)
			gfx->allowedCodecs[RDPGFX_CODECID_AVC444v2] = TRUE;
	}

	if (capsSet.version == RDPGFX_CAPVERSION_81)
	{
		if ((capsSet.flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED) != 0)
		{
			WLog_Print(gfx->base.log, WLOG_INFO, "AVC420 enabled by server");
			gfx->allowedCodecs[RDPGFX_CODECID_AVC420] = TRUE;
		}
	}

#if defined(WITH_GFX_AV1)
	if (capsSet.version == RDPGFX_CAPVERSION_FRDP_1)
		gfx->allowedCodecs[RDPGFX_CODECID_AV1] = TRUE;
#endif
#if defined(WITH_GFX_AV2)
	if (capsSet.version == RDPGFX_CAPVERSION_FRDP_2)
		gfx->allowedCodecs[RDPGFX_CODECID_AV1] = TRUE;
#endif

#if defined(WITH_GFX_AZURE)
	gfx->allowedCodecs[RDPGFX_CODECID_HEVC420] = FALSE;
	gfx->allowedCodecs[RDPGFX_CODECID_HEVC444] = FALSE;

	if (capsSet.version >= RDPGFX_CAPVERSION_112)
	{
		if ((capsSet.flags & RDPGFX_CAPS_FLAG_HEVC_ENABLE) != 0)
		{
			WLog_Print(gfx->base.log, WLOG_INFO, "HEVC confirmed by server");
			gfx->allowedCodecs[RDPGFX_CODECID_HEVC420] = TRUE;

			/* HEVC444 is only valid in RDPGFX_CAPVERSION_115 and needs HEVC enabled */
			if ((capsSet.version >= RDPGFX_CAPVERSION_115) &&
			    ((capsSet.flags & RDPGFX_CAPS_FLAG_HEVC_I444) != 0))
			{
				WLog_Print(gfx->base.log, WLOG_INFO, "HEVC I444 confirmed by server");
				gfx->allowedCodecs[RDPGFX_CODECID_HEVC444] = TRUE;
			}
		}
	}
#endif

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
	UINT error = 0;

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->base.listener_callback)
		return ERROR_BAD_CONFIGURATION;

	GENERIC_CHANNEL_CALLBACK* callback = gfx->base.listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	const RDPGFX_HEADER header = { .flags = 0,
		                           .cmdId = RDPGFX_CMDID_FRAMEACKNOWLEDGE,
		                           .pduLength = RDPGFX_HEADER_SIZE + 12 };

	WLog_Print(gfx->base.log, WLOG_TRACE, "SendFrameAcknowledgePdu: %" PRIu32 "", pdu->frameId);

	wStream* s = Stream_New(nullptr, header.pduLength);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	/* RDPGFX_FRAME_ACKNOWLEDGE_PDU */
	Stream_Write_UINT32(s, pdu->queueDepth);         /* queueDepth (4 bytes) */
	Stream_Write_UINT32(s, pdu->frameId);            /* frameId (4 bytes) */
	Stream_Write_UINT32(s, pdu->totalFramesDecoded); /* totalFramesDecoded (4 bytes) */
	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 nullptr);

	if (error == CHANNEL_RC_OK) /* frame successfully acked */
	{
		gfx->UnacknowledgedFrames--;
		rdpgfx_stats_frames_unacknowledged(context, gfx->UnacknowledgedFrames);
	}

fail:
	Stream_Free(s, TRUE);
	return error;
}

static UINT rdpgfx_send_qoe_frame_acknowledge_pdu(RdpgfxClientContext* context,
                                                  const RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* pdu)
{
	UINT error = CHANNEL_RC_OK;
	const RDPGFX_HEADER header = { .flags = 0,
		                           .cmdId = RDPGFX_CMDID_QOEFRAMEACKNOWLEDGE,
		                           .pduLength = RDPGFX_HEADER_SIZE + 12 };

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->base.listener_callback)
		return ERROR_BAD_CONFIGURATION;

	GENERIC_CHANNEL_CALLBACK* callback = gfx->base.listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	WLog_Print(gfx->base.log, WLOG_TRACE, "SendQoeFrameAcknowledgePdu: %" PRIu32 "", pdu->frameId);
	wStream* s = Stream_New(nullptr, header.pduLength);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
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
	                                 nullptr);
fail:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_reset_graphics_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_RESET_GRAPHICS_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);

	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;
	GraphicsResetEventArgs graphicsReset = WINPR_C_ARRAY_INIT;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.width);        /* width (4 bytes) */
	Stream_Read_UINT32(s, pdu.height);       /* height (4 bytes) */
	Stream_Read_UINT32(s, pdu.monitorCount); /* monitorCount (4 bytes) */

	if ((pdu.width == 0) || (pdu.height == 0) || (pdu.monitorCount == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->ResetGraphics Width[%" PRIu32 "], Height[%" PRIu32
		           "] or monitorCount[%" PRIu32 " is 0",
		           pdu.width, pdu.height, pdu.monitorCount);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.width > MAX_SURFACE_SIZE) || (pdu.height > MAX_SURFACE_SIZE) ||
	    (pdu.monitorCount > MAX_MONITOR_COUNT))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->ResetGraphics Width[%" PRIu32 "], Height[%" PRIu32 "] > %" PRIu32
		           " or monitorCount[%" PRIu32 " > %" PRIu32,
		           pdu.width, pdu.height, MAX_SURFACE_SIZE, pdu.monitorCount, MAX_MONITOR_COUNT);
		return ERROR_INVALID_DATA;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, pdu.monitorCount, 20ull))
		return ERROR_INVALID_DATA;

	pdu.monitorDefArray = (MONITOR_DEF*)calloc(pdu.monitorCount, sizeof(MONITOR_DEF));

	if (!pdu.monitorDefArray)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (UINT32 index = 0; index < pdu.monitorCount; index++)
	{
		MONITOR_DEF* monitor = &(pdu.monitorDefArray[index]);
		Stream_Read_INT32(s, monitor->left);   /* left (4 bytes) */
		Stream_Read_INT32(s, monitor->top);    /* top (4 bytes) */
		Stream_Read_INT32(s, monitor->right);  /* right (4 bytes) */
		Stream_Read_INT32(s, monitor->bottom); /* bottom (4 bytes) */
		Stream_Read_UINT32(s, monitor->flags); /* flags (4 bytes) */
	}

	const size_t size = (RDPGFX_HEADER_SIZE + 12ULL + (pdu.monitorCount * 20ULL));
	if (size > 340)
	{
		free(pdu.monitorDefArray);
		return CHANNEL_RC_NULL_DATA;
	}
	const size_t pad = 340ULL - size;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, (size_t)pad))
	{
		free(pdu.monitorDefArray);
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Seek(s, pad); /* pad (total size is 340 bytes) */
	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvResetGraphicsPdu: width: %" PRIu32 " height: %" PRIu32 " count: %" PRIu32 "",
	           pdu.width, pdu.height, pdu.monitorCount);

	for (UINT32 index = 0; index < pdu.monitorCount; index++)
	{
		MONITOR_DEF* monitor = &(pdu.monitorDefArray[index]);
		WLog_Print(gfx->base.log, WLOG_TRACE,
		           "RecvResetGraphicsPdu: monitor left:%" PRIi32 " top:%" PRIi32 " right:%" PRIi32
		           " bottom:%" PRIi32 " flags:0x%" PRIx32 "",
		           monitor->left, monitor->top, monitor->right, monitor->bottom, monitor->flags);
	}

	if (context)
	{
		IFCALLRET(context->ResetGraphics, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->ResetGraphics failed with error %" PRIu32 "", error);
	}

	free(pdu.monitorDefArray);

	/* some listeners may be interested (namely the display channel) */
	if (error == CHANNEL_RC_OK)
	{
		EventArgsInit(&graphicsReset, "libfreerdp");
		graphicsReset.width = pdu.width;
		graphicsReset.height = pdu.height;
		if (PubSub_OnGraphicsReset(gfx->rdpcontext->pubSub, gfx->rdpcontext, &graphicsReset) < 0)
			return ERROR_INTERNAL_ERROR;
	}
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_evict_cache_entry_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_EVICT_CACHE_ENTRY_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 2))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */
	WLog_Print(gfx->base.log, WLOG_DEBUG, "RecvEvictCacheEntryPdu: cacheSlot: %" PRIu16 "",
	           pdu.cacheSlot);

	if (context)
	{
		IFCALLRET(context->EvictCacheEntry, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->EvictCacheEntry failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_save_persistent_cache(RDPGFX_PLUGIN* gfx)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(gfx);
	WINPR_ASSERT(gfx->rdpcontext);
	rdpSettings* settings = gfx->rdpcontext->settings;
	RdpgfxClientContext* context = gfx->context;

	WINPR_ASSERT(context);
	WINPR_ASSERT(settings);

	if (!freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled))
		return CHANNEL_RC_OK;

	const char* BitmapCachePersistFile =
	    freerdp_settings_get_string(settings, FreeRDP_BitmapCachePersistFile);
	if (!BitmapCachePersistFile)
		return CHANNEL_RC_OK;

	if (!context->ExportCacheEntry)
		return CHANNEL_RC_INITIALIZATION_ERROR;

	rdpPersistentCache* persistent = persistent_cache_new();

	if (!persistent)
		return CHANNEL_RC_NO_MEMORY;

	if (persistent_cache_open(persistent, BitmapCachePersistFile, TRUE, 3) < 1)
	{
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto fail;
	}

	for (UINT16 idx = 0; idx < gfx->MaxCacheSlots; idx++)
	{
		if (gfx->CacheSlots[idx])
		{
			const UINT16 cacheSlot = idx;
			PERSISTENT_CACHE_ENTRY cacheEntry = WINPR_C_ARRAY_INIT;

			if (context->ExportCacheEntry(context, cacheSlot, &cacheEntry) != CHANNEL_RC_OK)
				continue;

			if (persistent_cache_write_entry(persistent, &cacheEntry) < 0)
				goto fail;
		}
	}

	persistent_cache_free(persistent);

	return error;
fail:
	persistent_cache_free(persistent);
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
	UINT error = CHANNEL_RC_OK;

	if (!context || !pdu)
		return ERROR_BAD_ARGUMENTS;

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	if (!gfx || !gfx->base.listener_callback)
		return ERROR_BAD_CONFIGURATION;

	GENERIC_CHANNEL_CALLBACK* callback = gfx->base.listener_callback->channel_callback;

	if (!callback)
		return ERROR_BAD_CONFIGURATION;

	const RDPGFX_HEADER header = { .flags = 0,
		                           .cmdId = RDPGFX_CMDID_CACHEIMPORTOFFER,
		                           .pduLength =
		                               RDPGFX_HEADER_SIZE + 2ul + pdu->cacheEntriesCount * 12ul };

	WLog_Print(gfx->base.log, WLOG_DEBUG, "SendCacheImportOfferPdu: cacheEntriesCount: %" PRIu16 "",
	           pdu->cacheEntriesCount);
	wStream* s = Stream_New(nullptr, header.pduLength);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_write_header(s, &header)))
		goto fail;

	if (pdu->cacheEntriesCount <= 0)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Invalid cacheEntriesCount: %" PRIu16 "",
		           pdu->cacheEntriesCount);
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	/* cacheEntriesCount (2 bytes) */
	Stream_Write_UINT16(s, pdu->cacheEntriesCount);

	for (UINT16 index = 0; index < pdu->cacheEntriesCount; index++)
	{
		const RDPGFX_CACHE_ENTRY_METADATA* cacheEntry = &(pdu->cacheEntries[index]);
		Stream_Write_UINT64(s, cacheEntry->cacheKey);     /* cacheKey (8 bytes) */
		Stream_Write_UINT32(s, cacheEntry->bitmapLength); /* bitmapLength (4 bytes) */
	}

	error = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                 nullptr);

fail:
	Stream_Free(s, TRUE);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_send_cache_offer(RDPGFX_PLUGIN* gfx)
{
	int count = 0;
	UINT error = CHANNEL_RC_OK;
	PERSISTENT_CACHE_ENTRY entry = WINPR_C_ARRAY_INIT;
	RDPGFX_CACHE_IMPORT_OFFER_PDU* offer = nullptr;

	WINPR_ASSERT(gfx);
	WINPR_ASSERT(gfx->rdpcontext);

	RdpgfxClientContext* context = gfx->context;
	rdpSettings* settings = gfx->rdpcontext->settings;

	if (!freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled))
		return CHANNEL_RC_OK;

	const char* BitmapCachePersistFile =
	    freerdp_settings_get_string(settings, FreeRDP_BitmapCachePersistFile);
	if (!BitmapCachePersistFile)
		return CHANNEL_RC_OK;

	rdpPersistentCache* persistent = persistent_cache_new();

	if (!persistent)
		return CHANNEL_RC_NO_MEMORY;

	if (persistent_cache_open(persistent, BitmapCachePersistFile, FALSE, 3) < 1)
	{
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto fail;
	}

	if (persistent_cache_get_version(persistent) != 3)
	{
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	count = persistent_cache_get_count(persistent);
	if (count < 0)
	{
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	if (count >= RDPGFX_CACHE_ENTRY_MAX_COUNT)
		count = RDPGFX_CACHE_ENTRY_MAX_COUNT - 1;

	if (count > gfx->MaxCacheSlots)
		count = gfx->MaxCacheSlots;

	offer = (RDPGFX_CACHE_IMPORT_OFFER_PDU*)calloc(1, sizeof(RDPGFX_CACHE_IMPORT_OFFER_PDU));
	if (!offer)
	{
		error = CHANNEL_RC_NO_MEMORY;
		goto fail;
	}

	WINPR_ASSERT(count <= UINT16_MAX);
	offer->cacheEntriesCount = (UINT16)count;

	WLog_Print(gfx->base.log, WLOG_DEBUG, "Sending Cache Import Offer: %d", count);

	for (int idx = 0; idx < count; idx++)
	{
		if (persistent_cache_read_entry(persistent, &entry) < 1)
		{
			error = ERROR_INVALID_DATA;
			goto fail;
		}

		offer->cacheEntries[idx].cacheKey = entry.key64;
		offer->cacheEntries[idx].bitmapLength = entry.size;
	}

	if (offer->cacheEntriesCount > 0)
	{
		error = rdpgfx_send_cache_import_offer_pdu(context, offer);
		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(gfx->base.log, WLOG_ERROR, "Failed to send cache import offer PDU");
			goto fail;
		}
	}

fail:
	persistent_cache_free(persistent);
	free(offer);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_load_cache_import_reply(RDPGFX_PLUGIN* gfx,
                                           const RDPGFX_CACHE_IMPORT_REPLY_PDU* reply)
{
	UINT error = CHANNEL_RC_OK;
	rdpPersistentCache* persistent = nullptr;
	WINPR_ASSERT(gfx);
	WINPR_ASSERT(gfx->rdpcontext);
	rdpSettings* settings = gfx->rdpcontext->settings;
	RdpgfxClientContext* context = gfx->context;

	WINPR_ASSERT(settings);
	WINPR_ASSERT(reply);
	if (!freerdp_settings_get_bool(settings, FreeRDP_BitmapCachePersistEnabled))
		return CHANNEL_RC_OK;

	const char* BitmapCachePersistFile =
	    freerdp_settings_get_string(settings, FreeRDP_BitmapCachePersistFile);
	if (!BitmapCachePersistFile)
		return CHANNEL_RC_OK;

	persistent = persistent_cache_new();

	if (!persistent)
		return CHANNEL_RC_NO_MEMORY;

	if (persistent_cache_open(persistent, BitmapCachePersistFile, FALSE, 3) < 1)
	{
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto fail;
	}

	if (persistent_cache_get_version(persistent) != 3)
	{
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	int count = persistent_cache_get_count(persistent);

	count = (count < reply->importedEntriesCount) ? count : reply->importedEntriesCount;

	WLog_Print(gfx->base.log, WLOG_DEBUG, "Receiving Cache Import Reply: %d", count);

	for (int idx = 0; idx < count; idx++)
	{
		PERSISTENT_CACHE_ENTRY entry = WINPR_C_ARRAY_INIT;
		if (persistent_cache_read_entry(persistent, &entry) < 1)
		{
			error = ERROR_INVALID_DATA;
			goto fail;
		}

		const UINT16 cacheSlot = reply->cacheSlots[idx];
		if (context && context->ImportCacheEntry)
		{
			error = context->ImportCacheEntry(context, cacheSlot, &entry);
			if (error != CHANNEL_RC_OK)
				break;
		}
	}

	persistent_cache_free(persistent);

	return error;
fail:
	persistent_cache_free(persistent);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_cache_import_reply_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CACHE_IMPORT_REPLY_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 2))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.importedEntriesCount); /* cacheSlot (2 bytes) */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, pdu.importedEntriesCount,
	                                                2ull))
		return ERROR_INVALID_DATA;

	if (pdu.importedEntriesCount > RDPGFX_CACHE_ENTRY_MAX_COUNT)
		return ERROR_INVALID_DATA;

	for (UINT16 idx = 0; idx < pdu.importedEntriesCount; idx++)
	{
		Stream_Read_UINT16(s, pdu.cacheSlots[idx]); /* cacheSlot (2 bytes) */
	}

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvCacheImportReplyPdu: importedEntriesCount: %" PRIu16 "",
	           pdu.importedEntriesCount);

	error = rdpgfx_load_cache_import_reply(gfx, &pdu);

	if (error)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "rdpgfx_load_cache_import_reply failed with error %" PRIu32 "", error);
		return error;
	}

	if (context)
	{
		IFCALLRET(context->CacheImportReply, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->CacheImportReply failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_create_surface_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CREATE_SURFACE_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 7))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);  /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.width);      /* width (2 bytes) */
	Stream_Read_UINT16(s, pdu.height);     /* height (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* RDPGFX_PIXELFORMAT (1 byte) */

	if ((pdu.width == 0) || (pdu.height == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvCreateSurfacePdu Width[%" PRIu32 "], Height[%" PRIu32 "] is 0",
		           pdu.width, pdu.height);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.width > MAX_SURFACE_SIZE) || (pdu.height > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvCreateSurfacePdu Width[%" PRIu32 "], Height[%" PRIu32
		           "] > %" PRIu32,
		           pdu.width, pdu.height, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvCreateSurfacePdu: surfaceId: %" PRIu16 " width: %" PRIu16 " height: %" PRIu16
	           " pixelFormat: 0x%02" PRIX8 "",
	           pdu.surfaceId, pdu.width, pdu.height, pdu.pixelFormat);

	if (context)
	{
		/* create surface PDU sometimes happens for surface ID that are already in use and have not
		 * been removed yet. Ensure that there is no surface with the new ID by trying to remove it
		 * manually.
		 */
		RDPGFX_DELETE_SURFACE_PDU deletePdu = { pdu.surfaceId };
		const UINT drc = IFCALLRESULT(CHANNEL_RC_OK, context->DeleteSurface, context, &deletePdu);
		if (drc != CHANNEL_RC_OK)
			WLog_Print(gfx->base.log, WLOG_WARN,
			           "context->DeleteSurface failed with error %" PRIu32 ", ignoring", drc);

		IFCALLRET(context->CreateSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->CreateSurface failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_delete_surface_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_SURFACE_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 2))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	WLog_Print(gfx->base.log, WLOG_DEBUG, "RecvDeleteSurfacePdu: surfaceId: %" PRIu16 "",
	           pdu.surfaceId);

	if (context)
	{
		IFCALLRET(context->DeleteSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->DeleteSurface failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_start_frame_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_START_FRAME_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, RDPGFX_START_FRAME_PDU_SIZE))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.timestamp); /* timestamp (4 bytes) */
	Stream_Read_UINT32(s, pdu.frameId);   /* frameId (4 bytes) */
	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvStartFramePdu: frameId: %" PRIu32 " timestamp: 0x%08" PRIX32 "", pdu.frameId,
	           pdu.timestamp);
	gfx->StartDecodingTime = GetTickCount64();

	if (context)
	{
		IFCALLRET(context->StartFrame, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->StartFrame failed with error %" PRIu32 "", error);
	}

	gfx->UnacknowledgedFrames++;
	rdpgfx_stats_frames_unacknowledged(context, gfx->UnacknowledgedFrames);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_end_frame_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_END_FRAME_PDU pdu = WINPR_C_ARRAY_INIT;
	RDPGFX_FRAME_ACKNOWLEDGE_PDU ack = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, RDPGFX_END_FRAME_PDU_SIZE))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.frameId); /* frameId (4 bytes) */
	WLog_Print(gfx->base.log, WLOG_TRACE, "RecvEndFramePdu: frameId: %" PRIu32 "", pdu.frameId);

	const UINT64 start = GetTickCount64();
	if (context)
	{
		IFCALLRET(context->EndFrame, error, context, &pdu);

		if (error)
		{
			WLog_Print(gfx->base.log, WLOG_ERROR, "context->EndFrame failed with error %" PRIu32 "",
			           error);
			return error;
		}
	}
	const UINT64 end = GetTickCount64();
	const UINT64 EndFrameTime = end - start;
	gfx->TotalDecodedFrames++;
	rdpgfx_stats_frame_decoded(context);

	if (!gfx->sendFrameAcks)
		return error;

	ack.frameId = pdu.frameId;
	ack.totalFramesDecoded = gfx->TotalDecodedFrames;

	if (gfx->suspendFrameAcks)
	{
		ack.queueDepth = SUSPEND_FRAME_ACKNOWLEDGEMENT;

		if (gfx->TotalDecodedFrames == 1)
			if ((error = rdpgfx_send_frame_acknowledge_pdu(context, &ack)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_send_frame_acknowledge_pdu failed with error %" PRIu32 "",
				           error);
	}
	else
	{
		ack.queueDepth = QUEUE_DEPTH_UNAVAILABLE;

		if ((error = rdpgfx_send_frame_acknowledge_pdu(context, &ack)))
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_send_frame_acknowledge_pdu failed with error %" PRIu32 "", error);
	}

	switch (gfx->ConnectionCaps.version)
	{
#if defined(WITH_GFX_AV1)
		case RDPGFX_CAPVERSION_FRDP_1:
#endif
#if defined(WITH_GFX_AV2)
		case RDPGFX_CAPVERSION_FRDP_2:
#endif
		case RDPGFX_CAPVERSION_10:
		case RDPGFX_CAPVERSION_102:
		case RDPGFX_CAPVERSION_103:
		case RDPGFX_CAPVERSION_104:
		case RDPGFX_CAPVERSION_105:
		case RDPGFX_CAPVERSION_106:
		case RDPGFX_CAPVERSION_106_ERR:
		case RDPGFX_CAPVERSION_107:
#if defined(WITH_GFX_AZURE)
		case RDPGFX_CAPVERSION_111:
		case RDPGFX_CAPVERSION_112:
		case RDPGFX_CAPVERSION_113:
		case RDPGFX_CAPVERSION_115:
#endif
			if (freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSendQoeAck))
			{
				RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU qoe = WINPR_C_ARRAY_INIT;
				UINT64 diff = (GetTickCount64() - gfx->StartDecodingTime);

				if (diff > 65000)
					diff = 0;

				qoe.frameId = pdu.frameId;
				qoe.timestamp = gfx->StartDecodingTime % UINT32_MAX;
				qoe.timeDiffSE = WINPR_ASSERTING_INT_CAST(UINT16, diff);
				qoe.timeDiffEDR = WINPR_ASSERTING_INT_CAST(UINT16, EndFrameTime);

				if ((error = rdpgfx_send_qoe_frame_acknowledge_pdu(context, &qoe)))
					WLog_Print(gfx->base.log, WLOG_ERROR,
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
 * The server restarts the graphics pipeline and asks for the capabilities again. It then confirms
 * one, resets the graphics and creates its surfaces anew, so the ones we still hold are dropped
 * here.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_caps_reset_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);

	/* the PDU has no body, tolerate one */
	if (!Stream_SafeSeek(s, Stream_GetRemainingLength(s)))
		return ERROR_INVALID_DATA;

	WLog_Print(gfx->base.log, WLOG_DEBUG, "RecvCapsResetPdu: sending the capabilities again");

	free_surfaces(gfx->context, gfx->SurfaceTable);
	const UINT error = evict_cache_slots(gfx->context, gfx->MaxCacheSlots, gfx->CacheSlots);
	if (error)
	{
		// print error, but don't consider this a hard failure
		WLog_Print(gfx->base.log, WLOG_ERROR, "evict_cache_slots failed with error %" PRIu32 "",
		           error);
	}
	gfx->UnacknowledgedFrames = 0;
	gfx->TotalDecodedFrames = 0;

	return rdpgfx_send_supported_caps(callback);
}

WINPR_ATTR_NODISCARD
static BOOL rdpgfx_codec_has_region_data_embedded(UINT16 codecId)
{
	switch (codecId)
	{
		case RDPGFX_CODECID_AVC420:
		case RDPGFX_CODECID_AVC444:
		case RDPGFX_CODECID_AVC444v2:
#if defined(WITH_GFX_AZURE)
		case RDPGFX_CODECID_HEVC420:
		case RDPGFX_CODECID_HEVC444:
#endif
#if defined(WITH_GFX_AV1) || defined(WITH_GFX_AV2)
		case RDPGFX_CODECID_AV1:
#endif
			return TRUE;
		default:
			return FALSE;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_1_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd = WINPR_C_ARRAY_INIT;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	UINT error = 0;

	WINPR_ASSERT(gfx);
	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);  /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId);    /* codecId (2 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat); /* pixelFormat (1 byte) */

	if ((error = rdpgfx_read_rect16(
	         gfx->base.log, s, &(pdu.destRect),
	         rdpgfx_codec_has_region_data_embedded(pdu.codecId)))) /* destRect (8 bytes) */
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "",
		           error);
		return error;
	}

	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, pdu.bitmapDataLength))
		return ERROR_INVALID_DATA;

	pdu.bitmapData = Stream_Pointer(s);
	Stream_Seek(s, pdu.bitmapDataLength);

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvWireToSurface1Pdu: surfaceId: %" PRIu16 " codecId: %s (0x%04" PRIX16
	           ") pixelFormat: 0x%02" PRIX8 " "
	           "destRect: left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16
	           " bitmapDataLength: %" PRIu32 "",
	           pdu.surfaceId, rdpgfx_get_codec_id_string(pdu.codecId), pdu.codecId, pdu.pixelFormat,
	           pdu.destRect.left, pdu.destRect.top, pdu.destRect.right, pdu.destRect.bottom,
	           pdu.bitmapDataLength);
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
	cmd.extra = nullptr;

	if (cmd.right < cmd.left)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "RecvWireToSurface1Pdu right=%" PRIu32 " < left=%" PRIu32, cmd.right, cmd.left);
		return ERROR_INVALID_DATA;
	}
	if (cmd.bottom < cmd.top)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "RecvWireToSurface1Pdu bottom=%" PRIu32 " < top=%" PRIu32, cmd.bottom, cmd.top);
		return ERROR_INVALID_DATA;
	}

	if ((error = rdpgfx_decode(gfx, &cmd)))
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_decode failed with error %" PRIu32 "!",
		           error);

	return error;
}

WINPR_ATTR_NODISCARD
static BOOL checkSurfaceCommand(const RDPGFX_PLUGIN* gfx, const RDPGFX_SURFACE_COMMAND* cmd)
{
	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
		case RDPGFX_CODECID_CAVIDEO:
		case RDPGFX_CODECID_CLEARCODEC:
		case RDPGFX_CODECID_PLANAR:
		case RDPGFX_CODECID_AVC420:
		case RDPGFX_CODECID_ALPHA:
		case RDPGFX_CODECID_AVC444:
		case RDPGFX_CODECID_AVC444v2:
		case RDPGFX_CODECID_CAPROGRESSIVE:
		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
#if defined(WITH_GFX_AV1) || defined(WITH_GFX_AV2)
		case RDPGFX_CODECID_AV1:
#endif
#if defined(WITH_GFX_AZURE)
		case RDPGFX_CODECID_HEVC420:
		case RDPGFX_CODECID_HEVC444:
#endif
			if (!gfx->allowedCodecs[cmd->codecId])
			{
				char buffer[128] = WINPR_C_ARRAY_INIT;
				WLog_Print(
				    gfx->base.log, WLOG_ERROR,
				    "RDPGFX_SURFACE_COMMAND::codecId %s not allowed with %s [%s]",
				    rdpgfx_get_codec_id_string(WINPR_ASSERTING_INT_CAST(UINT16, cmd->codecId)),
				    rdpgfx_caps_version_str(gfx->ConnectionCaps.version),
				    rdpgfx_caps_flags_str(gfx->ConnectionCaps.flags, buffer, sizeof(buffer)));
				return FALSE;
			}
			return TRUE;
		default:
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "Unknown RDPGFX_SURFACE_COMMAND::codecId %" PRIu32, cmd->codecId);
			return FALSE;
	}
}

UINT logSurfaceCommand(RDPGFX_PLUGIN* gfx, const RDPGFX_SURFACE_COMMAND* cmd)
{
	WINPR_ASSERT(gfx);

	WLog_Print(gfx->base.log, WLOG_DEBUG, "Got GFX %s",
	           rdpgfx_get_codec_id_string(WINPR_ASSERTING_INT_CAST(UINT16, cmd->codecId)));

	RdpgfxClientContext* context = gfx->context;
	if (!context)
		return CHANNEL_RC_OK;

	if (!checkSurfaceCommand(gfx, cmd))
		return CHANNEL_RC_NULL_DATA;

	const UINT error = IFCALLRESULT(CHANNEL_RC_OK, context->SurfaceCommand, context, cmd);

	if (error)
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->SurfaceCommand failed with error %" PRIu32 "", error);

	rdpgfx_codecid_event(context, cmd->codecId, cmd->length);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_wire_to_surface_2_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_COMMAND cmd = WINPR_C_ARRAY_INIT;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, RDPGFX_WIRE_TO_SURFACE_PDU_2_SIZE))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);        /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.codecId);          /* codecId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId);   /* codecContextId (4 bytes) */
	Stream_Read_UINT8(s, pdu.pixelFormat);       /* pixelFormat (1 byte) */
	Stream_Read_UINT32(s, pdu.bitmapDataLength); /* bitmapDataLength (4 bytes) */
	pdu.bitmapData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, pdu.bitmapDataLength))
		return ERROR_INVALID_DATA;

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvWireToSurface2Pdu: surfaceId: %" PRIu16 " codecId: %s (0x%04" PRIX16 ") "
	           "codecContextId: %" PRIu32 " pixelFormat: 0x%02" PRIX8 " bitmapDataLength: %" PRIu32
	           "",
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

	cmd.length = pdu.bitmapDataLength;
	cmd.data = pdu.bitmapData;
	cmd.extra = nullptr;

	if (cmd.codecId != RDPGFX_CODECID_CAPROGRESSIVE)
		return ERROR_INVALID_DATA;

	return logSurfaceCommand(gfx, &cmd);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_delete_encoding_context_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_DELETE_ENCODING_CONTEXT_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 6))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);      /* surfaceId (2 bytes) */
	Stream_Read_UINT32(s, pdu.codecContextId); /* codecContextId (4 bytes) */

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvDeleteEncodingContextPdu: surfaceId: %" PRIu16 " codecContextId: %" PRIu32 "",
	           pdu.surfaceId, pdu.codecContextId);

	if (context)
	{
		IFCALLRET(context->DeleteEncodingContext, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->DeleteEncodingContext failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_solid_fill_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SOLID_FILL_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */

	UINT error = rdpgfx_read_color32(gfx->base.log, s, &(pdu.fillPixel));
	if (error != CHANNEL_RC_OK) /* fillPixel (4 bytes) */
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_read_color32 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	Stream_Read_UINT16(s, pdu.fillRectCount); /* fillRectCount (2 bytes) */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, pdu.fillRectCount, 8ull))
		return ERROR_INVALID_DATA;

	pdu.fillRects = (RECTANGLE_16*)calloc(pdu.fillRectCount, sizeof(RECTANGLE_16));

	if (!pdu.fillRects)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (UINT16 index = 0; index < pdu.fillRectCount; index++)
	{
		RECTANGLE_16* fillRect = &(pdu.fillRects[index]);

		if ((error = rdpgfx_read_rect16(gfx->base.log, s, fillRect, FALSE)))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_read_rect16 failed with error %" PRIu32 "!", error);
			free(pdu.fillRects);
			return error;
		}
	}
	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvSolidFillPdu: surfaceId: %" PRIu16 " fillRectCount: %" PRIu16 "", pdu.surfaceId,
	           pdu.fillRectCount);

	if (context)
	{
		IFCALLRET(context->SolidFill, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->SolidFill failed with error %" PRIu32 "", error);
	}

	free(pdu.fillRects);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_surface_to_surface_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_SURFACE_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 14))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceIdSrc);  /* surfaceIdSrc (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceIdDest); /* surfaceIdDest (2 bytes) */

	if ((error =
	         rdpgfx_read_rect16(gfx->base.log, s, &(pdu.rectSrc), FALSE))) /* rectSrc (8 bytes ) */
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, pdu.destPtsCount, 4ull))
		return ERROR_INVALID_DATA;

	pdu.destPts = (RDPGFX_POINT16*)calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (UINT16 index = 0; index < pdu.destPtsCount; index++)
	{
		RDPGFX_POINT16* destPt = &(pdu.destPts[index]);

		if ((error = rdpgfx_read_point16(gfx->base.log, s, destPt)))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_read_point16 failed with error %" PRIu32 "!", error);
			free(pdu.destPts);
			return error;
		}
	}

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvSurfaceToSurfacePdu: surfaceIdSrc: %" PRIu16 " surfaceIdDest: %" PRIu16 " "
	           "left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16
	           " destPtsCount: %" PRIu16 "",
	           pdu.surfaceIdSrc, pdu.surfaceIdDest, pdu.rectSrc.left, pdu.rectSrc.top,
	           pdu.rectSrc.right, pdu.rectSrc.bottom, pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->SurfaceToSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
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
static UINT rdpgfx_recv_surface_to_cache_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_SURFACE_TO_CACHE_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 20))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId); /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.cacheKey);  /* cacheKey (8 bytes) */
	Stream_Read_UINT16(s, pdu.cacheSlot); /* cacheSlot (2 bytes) */

	UINT error = rdpgfx_read_rect16(gfx->base.log, s, &(pdu.rectSrc), FALSE);
	if (error != CHANNEL_RC_OK) /* rectSrc (8 bytes ) */
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_read_rect16 failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RecvSurfaceToCachePdu: surfaceId: %" PRIu16 " cacheKey: 0x%016" PRIX64
	           " cacheSlot: %" PRIu16 " "
	           "left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16 " bottom: %" PRIu16 "",
	           pdu.surfaceId, pdu.cacheKey, pdu.cacheSlot, pdu.rectSrc.left, pdu.rectSrc.top,
	           pdu.rectSrc.right, pdu.rectSrc.bottom);

	if (context)
	{
		IFCALLRET(context->SurfaceToCache, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->SurfaceToCache failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_cache_to_surface_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_CACHE_TO_SURFACE_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 6))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.cacheSlot);    /* cacheSlot (2 bytes) */
	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.destPtsCount); /* destPtsCount (2 bytes) */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, pdu.destPtsCount, 4ull))
		return ERROR_INVALID_DATA;

	pdu.destPts = (RDPGFX_POINT16*)calloc(pdu.destPtsCount, sizeof(RDPGFX_POINT16));

	if (!pdu.destPts)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (UINT16 index = 0; index < pdu.destPtsCount; index++)
	{
		RDPGFX_POINT16* destPt = &(pdu.destPts[index]);

		if ((error = rdpgfx_read_point16(gfx->base.log, s, destPt)))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_read_point16 failed with error %" PRIu32 "", error);
			free(pdu.destPts);
			return error;
		}
	}

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "RdpGfxRecvCacheToSurfacePdu: cacheSlot: %" PRIu16 " surfaceId: %" PRIu16
	           " destPtsCount: %" PRIu16 "",
	           pdu.cacheSlot, pdu.surfaceId, pdu.destPtsCount);

	if (context)
	{
		IFCALLRET(context->CacheToSurface, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
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
static UINT rdpgfx_recv_map_surface_to_output_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);     /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved);      /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */

	if ((pdu.outputOriginX > MAX_SURFACE_SIZE) || (pdu.outputOriginY > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->MapSurfaceToOutput outputOriginX[%" PRIu32 "], outputOriginY[%" PRIu32
		           "] > %" PRIu32,
		           pdu.outputOriginX, pdu.outputOriginY, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvMapSurfaceToOutputPdu: surfaceId: %" PRIu16 " outputOriginX: %" PRIu32
	           " outputOriginY: %" PRIu32 "",
	           pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY);

	if (context)
	{
		IFCALLRET(context->MapSurfaceToOutput, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->MapSurfaceToOutput failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT rdpgfx_recv_map_surface_to_scaled_output_pdu(GENERIC_CHANNEL_CALLBACK* callback,
                                                         wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 20))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);     /* surfaceId (2 bytes) */
	Stream_Read_UINT16(s, pdu.reserved);      /* reserved (2 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginX); /* outputOriginX (4 bytes) */
	Stream_Read_UINT32(s, pdu.outputOriginY); /* outputOriginY (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetWidth);   /* targetWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetHeight);  /* targetHeight (4 bytes) */

	if ((pdu.outputOriginX > MAX_SURFACE_SIZE) || (pdu.outputOriginY > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->MapSurfaceToScaledOutput outputOriginX[%" PRIu32
		           "], outputOriginY[%" PRIu32 "] > %" PRIu32,
		           pdu.outputOriginX, pdu.outputOriginY, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.targetWidth > MAX_SURFACE_SIZE) || (pdu.targetHeight > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->MapSurfaceToScaledOutput targetWidth[%" PRIu32
		           "], targetHeight[%" PRIu32 "] > %" PRIu32,
		           pdu.targetWidth, pdu.targetHeight, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvMapSurfaceToScaledOutputPdu: surfaceId: %" PRIu16 " outputOriginX: %" PRIu32
	           " outputOriginY: %" PRIu32 " targetWidth: %" PRIu32 " targetHeight: %" PRIu32,
	           pdu.surfaceId, pdu.outputOriginX, pdu.outputOriginY, pdu.targetWidth,
	           pdu.targetHeight);

	if (context)
	{
		IFCALLRET(context->MapSurfaceToScaledOutput, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->MapSurfaceToScaledOutput failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_map_surface_to_window_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_WINDOW_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 18))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId);     /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */

	if ((pdu.mappedWidth == 0) || (pdu.mappedHeight == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->MapSurfaceToWindow mappedWidth[%" PRIu32 "], mappedHeight[%" PRIu32
		           "] is 0",
		           pdu.mappedWidth, pdu.mappedHeight);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.mappedWidth > MAX_SURFACE_SIZE) || (pdu.mappedHeight > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->MapSurfaceToWindow mappedWidth[%" PRIu32 "], mappedHeight[%" PRIu32
		           "] > %" PRIu32,
		           pdu.mappedWidth, pdu.mappedHeight, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvMapSurfaceToWindowPdu: surfaceId: %" PRIu16 " windowId: 0x%016" PRIX64
	           " mappedWidth: %" PRIu32 " mappedHeight: %" PRIu32 "",
	           pdu.surfaceId, pdu.windowId, pdu.mappedWidth, pdu.mappedHeight);

	if (context && context->MapSurfaceToWindow)
	{
		IFCALLRET(context->MapSurfaceToWindow, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->MapSurfaceToWindow failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT rdpgfx_recv_map_surface_to_scaled_window_pdu(GENERIC_CHANNEL_CALLBACK* callback,
                                                         wStream* s)
{
	RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU pdu = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 26))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.surfaceId);    /* surfaceId (2 bytes) */
	Stream_Read_UINT64(s, pdu.windowId);     /* windowId (8 bytes) */
	Stream_Read_UINT32(s, pdu.mappedWidth);  /* mappedWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.mappedHeight); /* mappedHeight (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetWidth);  /* targetWidth (4 bytes) */
	Stream_Read_UINT32(s, pdu.targetHeight); /* targetHeight (4 bytes) */

	if ((pdu.mappedWidth == 0) || (pdu.mappedHeight == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvMapSurfaceToScaledWindowPdu mappedWidth[%" PRIu32
		           "], mappedHeight[%" PRIu32 "] is 0",
		           pdu.mappedWidth, pdu.mappedHeight);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.targetWidth == 0) || (pdu.targetHeight == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvMapSurfaceToScaledWindowPdu targetWidth[%" PRIu32
		           "], targetHeight[%" PRIu32 "] is 0",
		           pdu.targetWidth, pdu.targetHeight);
		return ERROR_INVALID_DATA;
	}

	if ((pdu.mappedWidth > MAX_SURFACE_SIZE) || (pdu.mappedHeight > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvMapSurfaceToScaledWindowPdu mappedWidth[%" PRIu32
		           "], mappedHeight[%" PRIu32 "] > %" PRIu32,
		           pdu.mappedWidth, pdu.mappedHeight, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}
	if ((pdu.targetWidth > MAX_SURFACE_SIZE) || (pdu.targetHeight > MAX_SURFACE_SIZE))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->RecvMapSurfaceToScaledWindowPdu targetWidth[%" PRIu32
		           "], targetHeight[%" PRIu32 "] > %" PRIu32,
		           pdu.targetWidth, pdu.targetHeight, MAX_SURFACE_SIZE);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(gfx->base.log, WLOG_DEBUG,
	           "RecvMapSurfaceToScaledWindowPdu: surfaceId: %" PRIu16 " windowId: 0x%016" PRIX64
	           " mappedWidth: %" PRIu32 " mappedHeight: %" PRIu32 " targetWidth: %" PRIu32
	           " targetHeight: %" PRIu32 "",
	           pdu.surfaceId, pdu.windowId, pdu.mappedWidth, pdu.mappedHeight, pdu.targetWidth,
	           pdu.targetHeight);

	if (context && context->MapSurfaceToScaledWindow)
	{
		IFCALLRET(context->MapSurfaceToScaledWindow, error, context, &pdu);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "context->MapSurfaceToScaledWindow failed with error %" PRIu32 "", error);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_recv_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* data)
{
	RDPGFX_HEADER header = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	WINPR_ASSERT(gfx);

	size_t packetlen = 0;
	UINT error = rdpgfx_read_header(gfx->base.log, data, &header, &packetlen);
	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_read_header failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	/* Limit the PDU parsers to the data of this PDU, the stream might contain more of them */
	wStream* s = Stream_StaticConstInit(&sbuffer, Stream_ConstPointer(data), packetlen);
	Stream_Seek(data, packetlen);

	WLog_Print(gfx->base.log, WLOG_TRACE,
	           "cmdId: %s (0x%04" PRIX16 ") flags: 0x%04" PRIX16 " pduLength: %" PRIu32 "",
	           rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId, header.flags,
	           header.pduLength);

	rdpgfx_stats_cmdid_event(gfx->context, header.cmdId);
	switch (header.cmdId)
	{
		case RDPGFX_CMDID_WIRETOSURFACE_1:
			if ((error = rdpgfx_recv_wire_to_surface_1_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_wire_to_surface_1_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_WIRETOSURFACE_2:
			if ((error = rdpgfx_recv_wire_to_surface_2_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_wire_to_surface_2_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_DELETEENCODINGCONTEXT:
			if ((error = rdpgfx_recv_delete_encoding_context_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_delete_encoding_context_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_SOLIDFILL:
			if ((error = rdpgfx_recv_solid_fill_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_solid_fill_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_SURFACETOSURFACE:
			if ((error = rdpgfx_recv_surface_to_surface_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_surface_to_surface_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_SURFACETOCACHE:
			if ((error = rdpgfx_recv_surface_to_cache_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_surface_to_cache_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CACHETOSURFACE:
			if ((error = rdpgfx_recv_cache_to_surface_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_cache_to_surface_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_EVICTCACHEENTRY:
			if ((error = rdpgfx_recv_evict_cache_entry_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_evict_cache_entry_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CREATESURFACE:
			if ((error = rdpgfx_recv_create_surface_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_create_surface_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_DELETESURFACE:
			if ((error = rdpgfx_recv_delete_surface_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_delete_surface_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_STARTFRAME:
			if ((error = rdpgfx_recv_start_frame_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_start_frame_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_ENDFRAME:
			if ((error = rdpgfx_recv_end_frame_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_end_frame_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_RESETGRAPHICS:
			if ((error = rdpgfx_recv_reset_graphics_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_reset_graphics_pdu failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOOUTPUT:
			if ((error = rdpgfx_recv_map_surface_to_output_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_output_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CACHEIMPORTREPLY:
			if ((error = rdpgfx_recv_cache_import_reply_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_cache_import_reply_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_CAPSCONFIRM:
			if ((error = rdpgfx_recv_caps_confirm_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_caps_confirm_pdu failed with error %" PRIu32 "!", error);

			if ((error = rdpgfx_send_cache_offer(gfx)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_send_cache_offer failed with error %" PRIu32 "!", error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOWINDOW:
			if ((error = rdpgfx_recv_map_surface_to_window_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_window_pdu failed with error %" PRIu32 "!",
				           error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOSCALEDWINDOW:
			if ((error = rdpgfx_recv_map_surface_to_scaled_window_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_scaled_window_pdu failed with error %" PRIu32
				           "!",
				           error);

			break;

		case RDPGFX_CMDID_MAPSURFACETOSCALEDOUTPUT:
			if ((error = rdpgfx_recv_map_surface_to_scaled_output_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_map_surface_to_scaled_output_pdu failed with error %" PRIu32
				           "!",
				           error);

			break;

		case RDPGFX_CMDID_PROTECT_SURFACE:
		case RDPGFX_CMDID_WATERMARK:
			WLog_Print(gfx->base.log, WLOG_WARN,
			           "Command %s not implemented, skipping %" PRIu32 " bytes",
			           rdpgfx_get_cmd_id_string(header.cmdId), header.pduLength);
			if (!Stream_SafeSeek(s, packetlen))
				error = ERROR_INVALID_DATA;
			break;

		case RDPGFX_CMDID_UNUSED_0014:
			/* servers send diagnostic information with this command, it carries no graphics */
			WLog_Print(gfx->base.log, WLOG_DEBUG,
			           "Diagnostic information from the server, skipping %" PRIu32 " bytes",
			           header.pduLength);
			if (!Stream_SafeSeek(s, packetlen))
				error = ERROR_INVALID_DATA;
			break;

		case RDPGFX_CMDID_CAPSRESET:
			if ((error = rdpgfx_recv_caps_reset_pdu(callback, s)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_recv_caps_reset_pdu failed with error %" PRIu32 "!", error);
			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			break;
	}

	if (error)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "Error while processing GFX cmdId: %s (0x%04" PRIX16 ")",
		           rdpgfx_get_cmd_id_string(header.cmdId), header.cmdId);
		return error;
	}

	const size_t end = Stream_GetPosition(s);

	if (end != packetlen)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "Unexpected gfx pdu end: Actual: %" PRIuz ", Expected: %" PRIuz, end, packetlen);
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
	UINT32 DstSize = 0;
	BYTE* pDstData = nullptr;
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(gfx);
	int status = zgfx_decompress(gfx->zgfx, Stream_ConstPointer(data),
	                             (UINT32)Stream_GetRemainingLength(data), &pDstData, &DstSize, 0);

	if ((status < 0) || (DstSize == 0))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "zgfx_decompress failure! status: %d, DstSize=%" PRIu32, status, DstSize);
		free(pDstData);
		return ERROR_INTERNAL_ERROR;
	}

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, pDstData, DstSize);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		free(pDstData);
		return CHANNEL_RC_NO_MEMORY;
	}

	while (Stream_GetPosition(s) < Stream_Length(s))
	{
		if ((error = rdpgfx_recv_pdu(callback, s)))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR, "rdpgfx_recv_pdu failed with error %" PRIu32 "!",
			           error);
			break;
		}
	}

	free(pDstData);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;
	UINT error = CHANNEL_RC_OK;
	BOOL do_caps_advertise = TRUE;

	gfx->sendFrameAcks = TRUE;

	if (context)
	{
		IFCALLRET(context->OnOpen, error, context, &do_caps_advertise, &gfx->sendFrameAcks);

		if (error)
			WLog_Print(gfx->base.log, WLOG_ERROR, "context->OnOpen failed with error %" PRIu32 "",
			           error);

		RdpgfxClientContextInt* ctx = (RdpgfxClientContextInt*)context;
		const RdpgfxClientContextStats empty = WINPR_C_ARRAY_INIT;
		ctx->stats = empty;
	}

	if (do_caps_advertise)
		error = rdpgfx_send_supported_caps(callback);

	return error;
}

static void rdpgfx_dump_stats(RdpgfxClientContext* context)
{
	WINPR_ASSERT(context);

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	WINPR_ASSERT(gfx);

	wLog* log = gfx->base.log;
	const DWORD level = WLOG_TRACE;
	if (!WLog_IsLevelActive(log, level))
		return;

	WLog_Print(log, level, "RdpgfxClientContextStats");
	for (size_t x = 0; x < rdpgfx_stats_max_index(); x++)
	{
		const char* name = rdpgfx_stats_name_for_index(x);
		const uint64_t val = rdpgfx_stats_value_for_index(context, x);
		WLog_Print(log, level, "%s: %" PRIu64, name, val);
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	UINT error = CHANNEL_RC_OK;
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(callback);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)callback->plugin;

	if (!gfx)
		goto fail;

	RdpgfxClientContext* context = gfx->context;

	WLog_Print(gfx->base.log, WLOG_DEBUG, "OnClose");
	error = rdpgfx_save_persistent_cache(gfx);

	if (error)
	{
		// print error, but don't consider this a hard failure
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "rdpgfx_save_persistent_cache failed with error %" PRIu32 "", error);
	}

	free_surfaces(context, gfx->SurfaceTable);
	error = evict_cache_slots(context, gfx->MaxCacheSlots, gfx->CacheSlots);
	if (error)
	{
		// print error, but don't consider this a hard failure
		WLog_Print(gfx->base.log, WLOG_ERROR, "evict_cache_slots failed with error %" PRIu32 "",
		           error);
	}

	free(callback);
	gfx->UnacknowledgedFrames = 0;
	gfx->TotalDecodedFrames = 0;

	if (context)
	{
		error = IFCALLRESULT(CHANNEL_RC_OK, context->OnClose, context);
		if (error)
		{
			// print error, but don't consider this a hard failure
			WLog_Print(gfx->base.log, WLOG_ERROR, "context->OnClose failed with error %" PRIu32 "",
			           error);
		}
		rdpgfx_dump_stats(context);
	}

fail:
	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)base;
	WINPR_ASSERT(gfx);
	RdpgfxClientContext* context = gfx->context;

	WLog_Print(gfx->base.log, WLOG_DEBUG, "Terminated");
	rdpgfx_client_context_free(context);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_surface_data(RdpgfxClientContext* context, UINT16 surfaceId, void* pData)
{
	WINPR_ASSERT(context);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	WINPR_ASSERT(gfx);
	ULONG_PTR key = ((ULONG_PTR)surfaceId) + 1;

	if (pData)
	{
		if (!HashTable_Insert(gfx->SurfaceTable, (void*)key, pData))
			return ERROR_BAD_ARGUMENTS;
	}
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
	ULONG_PTR* pKeys = nullptr;
	WINPR_ASSERT(context);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	WINPR_ASSERT(gfx);
	size_t count = HashTable_GetKeys(gfx->SurfaceTable, &pKeys);

	WINPR_ASSERT(ppSurfaceIds);
	WINPR_ASSERT(count_out);
	if (count < 1)
	{
		*count_out = 0;
		return CHANNEL_RC_OK;
	}

	UINT16* pSurfaceIds = (UINT16*)calloc(count, sizeof(UINT16));

	if (!pSurfaceIds)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "calloc failed!");
		free(pKeys);
		return CHANNEL_RC_NO_MEMORY;
	}

	for (size_t index = 0; index < count; index++)
	{
		pSurfaceIds[index] = (UINT16)(pKeys[index] - 1);
	}

	free(pKeys);
	*ppSurfaceIds = pSurfaceIds;
	*count_out = (UINT16)count;
	return CHANNEL_RC_OK;
}

static void* rdpgfx_get_surface_data(RdpgfxClientContext* context, UINT16 surfaceId)
{
	WINPR_ASSERT(context);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	WINPR_ASSERT(gfx);
	ULONG_PTR key = ((ULONG_PTR)surfaceId) + 1;
	return HashTable_GetItemValue(gfx->SurfaceTable, (void*)key);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_set_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot, void* pData)
{
	WINPR_ASSERT(context);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	WINPR_ASSERT(gfx);
	/* Microsoft uses 1-based indexing for the egfx bitmap cache ! */
	if (cacheSlot == 0 || cacheSlot > gfx->MaxCacheSlots)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "invalid cache slot %" PRIu16 ", must be between 1 and %" PRIu16 "", cacheSlot,
		           gfx->MaxCacheSlots);
		return ERROR_INVALID_INDEX;
	}

	gfx->CacheSlots[cacheSlot - 1] = pData;
	return CHANNEL_RC_OK;
}

static void* rdpgfx_get_cache_slot_data(RdpgfxClientContext* context, UINT16 cacheSlot)
{
	WINPR_ASSERT(context);
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;
	WINPR_ASSERT(gfx);
	/* Microsoft uses 1-based indexing for the egfx bitmap cache ! */
	if (cacheSlot == 0 || cacheSlot > gfx->MaxCacheSlots)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "invalid cache slot %" PRIu16 ", must be between 1 and %" PRIu16 "", cacheSlot,
		           gfx->MaxCacheSlots);
		return nullptr;
	}

	return gfx->CacheSlots[cacheSlot - 1];
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext,
                           WINPR_ATTR_UNUSED rdpSettings* settings)
{
	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)base;

	WINPR_ASSERT(base);
	gfx->rdpcontext = rcontext;

	gfx->SurfaceTable = HashTable_New(TRUE);
	if (!gfx->SurfaceTable)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "HashTable_New for surfaces failed !");
		return CHANNEL_RC_NO_MEMORY;
	}

	gfx->suspendFrameAcks =
	    freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSuspendFrameAck);
	gfx->MaxCacheSlots =
	    freerdp_settings_get_bool(gfx->rdpcontext->settings, FreeRDP_GfxSmallCache) ? 4096 : 25600;

	RdpgfxClientContextInt* context =
	    (RdpgfxClientContextInt*)calloc(1, sizeof(RdpgfxClientContextInt));
	if (!context)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "context calloc failed!");
		HashTable_Free(gfx->SurfaceTable);
		gfx->SurfaceTable = nullptr;
		return CHANNEL_RC_NO_MEMORY;
	}

	gfx->zgfx = zgfx_context_new(FALSE);
	if (!gfx->zgfx)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "zgfx_context_new failed!");
		HashTable_Free(gfx->SurfaceTable);
		gfx->SurfaceTable = nullptr;
		free(context);
		return CHANNEL_RC_NO_MEMORY;
	}

	context->common.handle = (void*)gfx;
	context->common.GetSurfaceIds = rdpgfx_get_surface_ids;
	context->common.SetSurfaceData = rdpgfx_set_surface_data;
	context->common.GetSurfaceData = rdpgfx_get_surface_data;
	context->common.SetCacheSlotData = rdpgfx_set_cache_slot_data;
	context->common.GetCacheSlotData = rdpgfx_get_cache_slot_data;
	context->common.CapsAdvertise = rdpgfx_send_caps_advertise_pdu;
	context->common.FrameAcknowledge = rdpgfx_send_frame_acknowledge_pdu;
	context->common.CacheImportOffer = rdpgfx_send_cache_import_offer_pdu;
	context->common.QoeFrameAcknowledge = rdpgfx_send_qoe_frame_acknowledge_pdu;

	gfx->base.iface.pInterface = (void*)context;
	gfx->context = &context->common;
	return CHANNEL_RC_OK;
}

void rdpgfx_client_context_free(RdpgfxClientContext* context)
{
	if (!context)
		return;

	RDPGFX_PLUGIN* gfx = (RDPGFX_PLUGIN*)context->handle;

	free_surfaces(context, gfx->SurfaceTable);
	evict_cache_slots(context, gfx->MaxCacheSlots, gfx->CacheSlots);

	if (gfx->zgfx)
	{
		zgfx_context_free(gfx->zgfx);
		gfx->zgfx = nullptr;
	}

	HashTable_Free(gfx->SurfaceTable);
	free(context);
}

static const IWTSVirtualChannelCallback rdpgfx_callbacks = { rdpgfx_on_data_received,
	                                                         rdpgfx_on_open, rdpgfx_on_close,
	                                                         nullptr };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE rdpgfx_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, GFXTAG, RDPGFX_DVC_CHANNEL_NAME,
	                                      sizeof(RDPGFX_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &rdpgfx_callbacks, init_plugin_cb, terminate_plugin_cb);
}
