/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library
 *
 * Copyright 2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 Norbert Federa <norbert.federa@thincast.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>

#include <freerdp/log.h>
#include <freerdp/settings.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/region.h>
#include <freerdp/build-config.h>

#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_decode.h"
#include "rfx_encode.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"
#include "rfx_rlgr.h"

#include "sse/rfx_sse2.h"
#include "neon/rfx_neon.h"

#define TAG FREERDP_TAG("codec")

#define RFX_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\RemoteFX"

/**
 * The quantization values control the compression rate and quality. The value
 * range is between 6 and 15. The higher value, the higher compression rate
 * and lower quality.
 *
 * This is the default values being use by the MS RDP server, and we will also
 * use it as our default values for the encoder. It can be overridden by setting
 * the context->num_quants and context->quants member.
 *
 * The order of the values are:
 * LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
 */
static const UINT32 rfx_default_quantization_values[] = { 6, 6, 6, 6, 7, 7, 8, 8, 8, 9 };

static INLINE BOOL rfx_write_progressive_tile_simple(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                     wStream* WINPR_RESTRICT s,
                                                     const RFX_TILE* WINPR_RESTRICT tile);

static INLINE void rfx_profiler_create(RFX_CONTEXT* WINPR_RESTRICT context)
{
	if (!context || !context->priv)
		return;
	PROFILER_CREATE(context->priv->prof_rfx_decode_rgb, "rfx_decode_rgb")
	PROFILER_CREATE(context->priv->prof_rfx_decode_component, "rfx_decode_component")
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_decode, "rfx_rlgr_decode")
	PROFILER_CREATE(context->priv->prof_rfx_differential_decode, "rfx_differential_decode")
	PROFILER_CREATE(context->priv->prof_rfx_quantization_decode, "rfx_quantization_decode")
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_decode, "rfx_dwt_2d_decode")
	PROFILER_CREATE(context->priv->prof_rfx_ycbcr_to_rgb, "prims->yCbCrToRGB")
	PROFILER_CREATE(context->priv->prof_rfx_encode_rgb, "rfx_encode_rgb")
	PROFILER_CREATE(context->priv->prof_rfx_encode_component, "rfx_encode_component")
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_encode, "rfx_rlgr_encode")
	PROFILER_CREATE(context->priv->prof_rfx_differential_encode, "rfx_differential_encode")
	PROFILER_CREATE(context->priv->prof_rfx_quantization_encode, "rfx_quantization_encode")
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_encode, "rfx_dwt_2d_encode")
	PROFILER_CREATE(context->priv->prof_rfx_rgb_to_ycbcr, "prims->RGBToYCbCr")
	PROFILER_CREATE(context->priv->prof_rfx_encode_format_rgb, "rfx_encode_format_rgb")
}

static INLINE void rfx_profiler_free(RFX_CONTEXT* WINPR_RESTRICT context)
{
	if (!context || !context->priv)
		return;
	PROFILER_FREE(context->priv->prof_rfx_decode_rgb)
	PROFILER_FREE(context->priv->prof_rfx_decode_component)
	PROFILER_FREE(context->priv->prof_rfx_rlgr_decode)
	PROFILER_FREE(context->priv->prof_rfx_differential_decode)
	PROFILER_FREE(context->priv->prof_rfx_quantization_decode)
	PROFILER_FREE(context->priv->prof_rfx_dwt_2d_decode)
	PROFILER_FREE(context->priv->prof_rfx_ycbcr_to_rgb)
	PROFILER_FREE(context->priv->prof_rfx_encode_rgb)
	PROFILER_FREE(context->priv->prof_rfx_encode_component)
	PROFILER_FREE(context->priv->prof_rfx_rlgr_encode)
	PROFILER_FREE(context->priv->prof_rfx_differential_encode)
	PROFILER_FREE(context->priv->prof_rfx_quantization_encode)
	PROFILER_FREE(context->priv->prof_rfx_dwt_2d_encode)
	PROFILER_FREE(context->priv->prof_rfx_rgb_to_ycbcr)
	PROFILER_FREE(context->priv->prof_rfx_encode_format_rgb)
}

static INLINE void rfx_profiler_print(RFX_CONTEXT* WINPR_RESTRICT context)
{
	if (!context || !context->priv)
		return;

	PROFILER_PRINT_HEADER
	PROFILER_PRINT(context->priv->prof_rfx_decode_rgb)
	PROFILER_PRINT(context->priv->prof_rfx_decode_component)
	PROFILER_PRINT(context->priv->prof_rfx_rlgr_decode)
	PROFILER_PRINT(context->priv->prof_rfx_differential_decode)
	PROFILER_PRINT(context->priv->prof_rfx_quantization_decode)
	PROFILER_PRINT(context->priv->prof_rfx_dwt_2d_decode)
	PROFILER_PRINT(context->priv->prof_rfx_ycbcr_to_rgb)
	PROFILER_PRINT(context->priv->prof_rfx_encode_rgb)
	PROFILER_PRINT(context->priv->prof_rfx_encode_component)
	PROFILER_PRINT(context->priv->prof_rfx_rlgr_encode)
	PROFILER_PRINT(context->priv->prof_rfx_differential_encode)
	PROFILER_PRINT(context->priv->prof_rfx_quantization_encode)
	PROFILER_PRINT(context->priv->prof_rfx_dwt_2d_encode)
	PROFILER_PRINT(context->priv->prof_rfx_rgb_to_ycbcr)
	PROFILER_PRINT(context->priv->prof_rfx_encode_format_rgb)
	PROFILER_PRINT_FOOTER
}

static INLINE void rfx_tile_init(void* obj)
{
	RFX_TILE* tile = (RFX_TILE*)obj;
	if (tile)
	{
		tile->x = 0;
		tile->y = 0;
		tile->YLen = 0;
		tile->YData = NULL;
		tile->CbLen = 0;
		tile->CbData = NULL;
		tile->CrLen = 0;
		tile->CrData = NULL;
	}
}

static INLINE void* rfx_decoder_tile_new(const void* val)
{
	const size_t size = 4ULL * 64ULL * 64ULL;
	RFX_TILE* tile = NULL;
	WINPR_UNUSED(val);

	if (!(tile = (RFX_TILE*)winpr_aligned_calloc(1, sizeof(RFX_TILE), 32)))
		return NULL;

	if (!(tile->data = (BYTE*)winpr_aligned_malloc(size, 16)))
	{
		winpr_aligned_free(tile);
		return NULL;
	}
	memset(tile->data, 0xff, size);
	tile->allocated = TRUE;
	return tile;
}

static INLINE void rfx_decoder_tile_free(void* obj)
{
	RFX_TILE* tile = (RFX_TILE*)obj;

	if (tile)
	{
		if (tile->allocated)
			winpr_aligned_free(tile->data);

		winpr_aligned_free(tile);
	}
}

static INLINE void* rfx_encoder_tile_new(const void* val)
{
	WINPR_UNUSED(val);
	return winpr_aligned_calloc(1, sizeof(RFX_TILE), 32);
}

static INLINE void rfx_encoder_tile_free(void* obj)
{
	winpr_aligned_free(obj);
}

RFX_CONTEXT* rfx_context_new(BOOL encoder)
{
	return rfx_context_new_ex(encoder, 0);
}

RFX_CONTEXT* rfx_context_new_ex(BOOL encoder, UINT32 ThreadingFlags)
{
	HKEY hKey = NULL;
	LONG status = 0;
	DWORD dwType = 0;
	DWORD dwSize = 0;
	DWORD dwValue = 0;
	SYSTEM_INFO sysinfo;
	RFX_CONTEXT* context = NULL;
	wObject* pool = NULL;
	RFX_CONTEXT_PRIV* priv = NULL;
	context = (RFX_CONTEXT*)winpr_aligned_calloc(1, sizeof(RFX_CONTEXT), 32);

	if (!context)
		return NULL;

	context->encoder = encoder;
	context->currentMessage.freeArray = TRUE;
	context->priv = priv = (RFX_CONTEXT_PRIV*)winpr_aligned_calloc(1, sizeof(RFX_CONTEXT_PRIV), 32);

	if (!priv)
		goto fail;

	priv->log = WLog_Get("com.freerdp.codec.rfx");
	WLog_OpenAppender(priv->log);
	priv->TilePool = ObjectPool_New(TRUE);

	if (!priv->TilePool)
		goto fail;

	pool = ObjectPool_Object(priv->TilePool);
	pool->fnObjectInit = rfx_tile_init;

	if (context->encoder)
	{
		pool->fnObjectNew = rfx_encoder_tile_new;
		pool->fnObjectFree = rfx_encoder_tile_free;
	}
	else
	{
		pool->fnObjectNew = rfx_decoder_tile_new;
		pool->fnObjectFree = rfx_decoder_tile_free;
	}

	/*
	 * align buffers to 16 byte boundary (needed for SSE/NEON instructions)
	 *
	 * y_r_buffer, cb_g_buffer, cr_b_buffer: 64 * 64 * sizeof(INT16) = 8192 (0x2000)
	 * dwt_buffer: 32 * 32 * 2 * 2 * sizeof(INT16) = 8192, maximum sub-band width is 32
	 *
	 * Additionally we add 32 bytes (16 in front and 16 at the back of the buffer)
	 * in order to allow optimized functions (SEE, NEON) to read from positions
	 * that are actually in front/beyond the buffer. Offset calculations are
	 * performed at the BufferPool_Take function calls in rfx_encode/decode.c.
	 *
	 * We then multiply by 3 to use a single, partionned buffer for all 3 channels.
	 */
	priv->BufferPool = BufferPool_New(TRUE, (8192ULL + 32ULL) * 3ULL, 16);

	if (!priv->BufferPool)
		goto fail;

	if (!(ThreadingFlags & THREADING_FLAGS_DISABLE_THREADS))
	{
		priv->UseThreads = TRUE;

		GetNativeSystemInfo(&sysinfo);
		priv->MinThreadCount = sysinfo.dwNumberOfProcessors;
		priv->MaxThreadCount = 0;
		status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, RFX_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

		if (status == ERROR_SUCCESS)
		{
			dwSize = sizeof(dwValue);

			if (RegQueryValueEx(hKey, _T("UseThreads"), NULL, &dwType, (BYTE*)&dwValue, &dwSize) ==
			    ERROR_SUCCESS)
				priv->UseThreads = dwValue ? 1 : 0;

			if (RegQueryValueEx(hKey, _T("MinThreadCount"), NULL, &dwType, (BYTE*)&dwValue,
			                    &dwSize) == ERROR_SUCCESS)
				priv->MinThreadCount = dwValue;

			if (RegQueryValueEx(hKey, _T("MaxThreadCount"), NULL, &dwType, (BYTE*)&dwValue,
			                    &dwSize) == ERROR_SUCCESS)
				priv->MaxThreadCount = dwValue;

			RegCloseKey(hKey);
		}
	}
	else
	{
		priv->UseThreads = FALSE;
	}

	if (priv->UseThreads)
	{
		/* Call primitives_get here in order to avoid race conditions when using primitives_get */
		/* from multiple threads. This call will initialize all function pointers correctly     */
		/* before any decoding threads are started */
		primitives_get();
		priv->ThreadPool = CreateThreadpool(NULL);

		if (!priv->ThreadPool)
			goto fail;

		InitializeThreadpoolEnvironment(&priv->ThreadPoolEnv);
		SetThreadpoolCallbackPool(&priv->ThreadPoolEnv, priv->ThreadPool);

		if (priv->MinThreadCount)
			if (!SetThreadpoolThreadMinimum(priv->ThreadPool, priv->MinThreadCount))
				goto fail;

		if (priv->MaxThreadCount)
			SetThreadpoolThreadMaximum(priv->ThreadPool, priv->MaxThreadCount);
	}

	/* initialize the default pixel format */
	rfx_context_set_pixel_format(context, PIXEL_FORMAT_BGRX32);
	/* create profilers for default decoding routines */
	rfx_profiler_create(context);
	/* set up default routines */
	context->quantization_decode = rfx_quantization_decode;
	context->quantization_encode = rfx_quantization_encode;
	context->dwt_2d_decode = rfx_dwt_2d_decode;
	context->dwt_2d_extrapolate_decode = rfx_dwt_2d_extrapolate_decode;
	context->dwt_2d_encode = rfx_dwt_2d_encode;
	context->rlgr_decode = rfx_rlgr_decode;
	context->rlgr_encode = rfx_rlgr_encode;
	rfx_init_sse2(context);
	rfx_init_neon(context);
	context->state = RFX_STATE_SEND_HEADERS;
	context->expectedDataBlockType = WBT_FRAME_BEGIN;
	return context;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	rfx_context_free(context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void rfx_context_free(RFX_CONTEXT* context)
{
	RFX_CONTEXT_PRIV* priv = NULL;

	if (!context)
		return;

	WINPR_ASSERT(NULL != context);

	priv = context->priv;
	WINPR_ASSERT(NULL != priv);
	WINPR_ASSERT(NULL != priv->TilePool);
	WINPR_ASSERT(NULL != priv->BufferPool);

	/* coverity[address_free] */
	rfx_message_free(context, &context->currentMessage);
	winpr_aligned_free(context->quants);
	rfx_profiler_print(context);
	rfx_profiler_free(context);

	if (priv)
	{
		ObjectPool_Free(priv->TilePool);
		if (priv->UseThreads)
		{
			if (priv->ThreadPool)
				CloseThreadpool(priv->ThreadPool);
			DestroyThreadpoolEnvironment(&priv->ThreadPoolEnv);
			winpr_aligned_free((void*)priv->workObjects);
			winpr_aligned_free(priv->tileWorkParams);
#ifdef WITH_PROFILER
			WLog_VRB(
			    TAG,
			    "WARNING: Profiling results probably unusable with multithreaded RemoteFX codec!");
#endif
		}

		BufferPool_Free(priv->BufferPool);
		winpr_aligned_free(priv);
	}
	winpr_aligned_free(context);
}

static INLINE RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* WINPR_RESTRICT message, UINT32 index)
{
	WINPR_ASSERT(message);
	WINPR_ASSERT(message->tiles);
	WINPR_ASSERT(index < message->numTiles);
	return message->tiles[index];
}

static INLINE const RFX_RECT* rfx_message_get_rect_const(const RFX_MESSAGE* WINPR_RESTRICT message,
                                                         UINT32 index)
{
	WINPR_ASSERT(message);
	WINPR_ASSERT(message->rects);
	WINPR_ASSERT(index < message->numRects);
	return &message->rects[index];
}

static INLINE RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* WINPR_RESTRICT message, UINT32 index)
{
	WINPR_ASSERT(message);
	WINPR_ASSERT(message->rects);
	WINPR_ASSERT(index < message->numRects);
	return &message->rects[index];
}

void rfx_context_set_pixel_format(RFX_CONTEXT* WINPR_RESTRICT context, UINT32 pixel_format)
{
	WINPR_ASSERT(context);
	context->pixel_format = pixel_format;
	context->bits_per_pixel = FreeRDPGetBitsPerPixel(pixel_format);
}

UINT32 rfx_context_get_pixel_format(RFX_CONTEXT* WINPR_RESTRICT context)
{
	WINPR_ASSERT(context);
	return context->pixel_format;
}

void rfx_context_set_palette(RFX_CONTEXT* WINPR_RESTRICT context,
                             const BYTE* WINPR_RESTRICT palette)
{
	WINPR_ASSERT(context);
	context->palette = palette;
}

const BYTE* rfx_context_get_palette(RFX_CONTEXT* WINPR_RESTRICT context)
{
	WINPR_ASSERT(context);
	return context->palette;
}

BOOL rfx_context_reset(RFX_CONTEXT* WINPR_RESTRICT context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	context->width = width;
	context->height = height;
	context->state = RFX_STATE_SEND_HEADERS;
	context->expectedDataBlockType = WBT_FRAME_BEGIN;
	context->frameIdx = 0;
	return TRUE;
}

static INLINE BOOL rfx_process_message_sync(RFX_CONTEXT* WINPR_RESTRICT context,
                                            wStream* WINPR_RESTRICT s)
{
	UINT32 magic = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	context->decodedHeaderBlocks &= ~RFX_DECODED_SYNC;

	/* RFX_SYNC */
	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 6))
		return FALSE;

	Stream_Read_UINT32(s, magic); /* magic (4 bytes), 0xCACCACCA */
	if (magic != WF_MAGIC)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid magic number 0x%08" PRIX32 "", magic);
		return FALSE;
	}

	Stream_Read_UINT16(s, context->version); /* version (2 bytes), WF_VERSION_1_0 (0x0100) */
	if (context->version != WF_VERSION_1_0)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid version number 0x%08" PRIX32 "",
		           context->version);
		return FALSE;
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "version 0x%08" PRIX32 "", context->version);
	context->decodedHeaderBlocks |= RFX_DECODED_SYNC;
	return TRUE;
}

static INLINE BOOL rfx_process_message_codec_versions(RFX_CONTEXT* WINPR_RESTRICT context,
                                                      wStream* WINPR_RESTRICT s)
{
	BYTE numCodecs = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	context->decodedHeaderBlocks &= ~RFX_DECODED_VERSIONS;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return FALSE;

	Stream_Read_UINT8(s, numCodecs);         /* numCodecs (1 byte), must be set to 0x01 */
	Stream_Read_UINT8(s, context->codec_id); /* codecId (1 byte), must be set to 0x01 */
	Stream_Read_UINT16(
	    s, context->codec_version); /* version (2 bytes), must be set to WF_VERSION_1_0 (0x0100)  */

	if (numCodecs != 1)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "numCodes is 0x%02" PRIX8 " (must be 0x01)",
		           numCodecs);
		return FALSE;
	}

	if (context->codec_id != 0x01)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid codec id (0x%02" PRIX32 ")",
		           context->codec_id);
		return FALSE;
	}

	if (context->codec_version != WF_VERSION_1_0)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid codec version (0x%08" PRIX32 ")",
		           context->codec_version);
		return FALSE;
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "id %" PRIu32 " version 0x%" PRIX32 ".",
	           context->codec_id, context->codec_version);
	context->decodedHeaderBlocks |= RFX_DECODED_VERSIONS;
	return TRUE;
}

static INLINE BOOL rfx_process_message_channels(RFX_CONTEXT* WINPR_RESTRICT context,
                                                wStream* WINPR_RESTRICT s)
{
	BYTE channelId = 0;
	BYTE numChannels = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	context->decodedHeaderBlocks &= ~RFX_DECODED_CHANNELS;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 1))
		return FALSE;

	Stream_Read_UINT8(s, numChannels); /* numChannels (1 byte), must bet set to 0x01 */

	/* In RDVH sessions, numChannels will represent the number of virtual monitors
	 * configured and does not always be set to 0x01 as [MS-RDPRFX] said.
	 */
	if (numChannels < 1)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "no channels announced");
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(context->priv->log, s, numChannels, 5ull))
		return FALSE;

	/* RFX_CHANNELT */
	Stream_Read_UINT8(s, channelId); /* channelId (1 byte), must be set to 0x00 */

	if (channelId != 0x00)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "channelId:0x%02" PRIX8 ", expected:0x00",
		           channelId);
		return FALSE;
	}

	Stream_Read_UINT16(s, context->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, context->height); /* height (2 bytes) */

	if (!context->width || !context->height)
	{
		WLog_Print(context->priv->log, WLOG_ERROR,
		           "invalid channel with/height: %" PRIu16 "x%" PRIu16 "", context->width,
		           context->height);
		return FALSE;
	}

	/* Now, only the first monitor can be used, therefore the other channels will be ignored. */
	Stream_Seek(s, 5ULL * (numChannels - 1));
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "numChannels %" PRIu8 " id %" PRIu8 ", %" PRIu16 "x%" PRIu16 ".", numChannels,
	           channelId, context->width, context->height);
	context->decodedHeaderBlocks |= RFX_DECODED_CHANNELS;
	return TRUE;
}

static INLINE BOOL rfx_process_message_context(RFX_CONTEXT* WINPR_RESTRICT context,
                                               wStream* WINPR_RESTRICT s)
{
	BYTE ctxId = 0;
	UINT16 tileSize = 0;
	UINT16 properties = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	context->decodedHeaderBlocks &= ~RFX_DECODED_CONTEXT;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 5))
		return FALSE;

	Stream_Read_UINT8(s, ctxId);     /* ctxId (1 byte), must be set to 0x00 */
	Stream_Read_UINT16(s, tileSize); /* tileSize (2 bytes), must be set to CT_TILE_64x64 (0x0040) */
	Stream_Read_UINT16(s, properties); /* properties (2 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "ctxId %" PRIu8 " tileSize %" PRIu16 " properties 0x%04" PRIX16 ".", ctxId, tileSize,
	           properties);
	context->properties = properties;
	context->flags = (properties & 0x0007);

	if (context->flags == CODEC_MODE)
	{
		WLog_Print(context->priv->log, WLOG_DEBUG, "codec is in image mode.");
	}
	else
	{
		WLog_Print(context->priv->log, WLOG_DEBUG, "codec is in video mode.");
	}

	switch ((properties & 0x1E00) >> 9)
	{
		case CLW_ENTROPY_RLGR1:
			context->mode = RLGR1;
			WLog_Print(context->priv->log, WLOG_DEBUG, "RLGR1.");
			break;

		case CLW_ENTROPY_RLGR3:
			context->mode = RLGR3;
			WLog_Print(context->priv->log, WLOG_DEBUG, "RLGR3.");
			break;

		default:
			WLog_Print(context->priv->log, WLOG_ERROR, "unknown RLGR algorithm.");
			return FALSE;
	}

	context->decodedHeaderBlocks |= RFX_DECODED_CONTEXT;
	return TRUE;
}

static INLINE BOOL rfx_process_message_frame_begin(RFX_CONTEXT* WINPR_RESTRICT context,
                                                   RFX_MESSAGE* WINPR_RESTRICT message,
                                                   wStream* WINPR_RESTRICT s,
                                                   UINT16* WINPR_RESTRICT pExpectedBlockType)
{
	UINT32 frameIdx = 0;
	UINT16 numRegions = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(message);
	WINPR_ASSERT(pExpectedBlockType);

	if (*pExpectedBlockType != WBT_FRAME_BEGIN)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "message unexpected wants WBT_FRAME_BEGIN");
		return FALSE;
	}

	*pExpectedBlockType = WBT_REGION;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 6))
		return FALSE;

	Stream_Read_UINT32(
	    s, frameIdx); /* frameIdx (4 bytes), if codec is in video mode, must be ignored */
	Stream_Read_UINT16(s, numRegions); /* numRegions (2 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RFX_FRAME_BEGIN: frameIdx: %" PRIu32 " numRegions: %" PRIu16 "", frameIdx,
	           numRegions);
	return TRUE;
}

static INLINE BOOL rfx_process_message_frame_end(RFX_CONTEXT* WINPR_RESTRICT context,
                                                 RFX_MESSAGE* WINPR_RESTRICT message,
                                                 wStream* WINPR_RESTRICT s,
                                                 UINT16* WINPR_RESTRICT pExpectedBlockType)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(message);
	WINPR_ASSERT(s);
	WINPR_ASSERT(pExpectedBlockType);

	if (*pExpectedBlockType != WBT_FRAME_END)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "message unexpected, wants WBT_FRAME_END");
		return FALSE;
	}

	*pExpectedBlockType = WBT_FRAME_BEGIN;
	WLog_Print(context->priv->log, WLOG_DEBUG, "RFX_FRAME_END");
	return TRUE;
}

static INLINE BOOL rfx_resize_rects(RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(message);

	RFX_RECT* tmpRects =
	    winpr_aligned_recalloc(message->rects, message->numRects, sizeof(RFX_RECT), 32);
	if (!tmpRects)
		return FALSE;
	message->rects = tmpRects;
	return TRUE;
}

static INLINE BOOL rfx_process_message_region(RFX_CONTEXT* WINPR_RESTRICT context,
                                              RFX_MESSAGE* WINPR_RESTRICT message,
                                              wStream* WINPR_RESTRICT s,
                                              UINT16* WINPR_RESTRICT pExpectedBlockType)
{
	UINT16 regionType = 0;
	UINT16 numTileSets = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(message);
	WINPR_ASSERT(pExpectedBlockType);

	if (*pExpectedBlockType != WBT_REGION)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "message unexpected wants WBT_REGION");
		return FALSE;
	}

	*pExpectedBlockType = WBT_EXTENSION;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 3))
		return FALSE;

	Stream_Seek_UINT8(s);                     /* regionFlags (1 byte) */
	Stream_Read_UINT16(s, message->numRects); /* numRects (2 bytes) */

	if (message->numRects < 1)
	{
		/*
		   If numRects is zero the decoder must generate a rectangle with
		   coordinates (0, 0, width, height).
		   See [MS-RDPRFX] (revision >= 17.0) 2.2.2.3.3 TS_RFX_REGION
		   https://msdn.microsoft.com/en-us/library/ff635233.aspx
		*/
		message->numRects = 1;
		if (!rfx_resize_rects(message))
			return FALSE;

		message->rects->x = 0;
		message->rects->y = 0;
		message->rects->width = context->width;
		message->rects->height = context->height;
		return TRUE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(context->priv->log, s, message->numRects, 8ull))
		return FALSE;

	if (!rfx_resize_rects(message))
		return FALSE;

	/* rects */
	for (UINT16 i = 0; i < message->numRects; i++)
	{
		RFX_RECT* rect = rfx_message_get_rect(message, i);
		/* RFX_RECT */
		Stream_Read_UINT16(s, rect->x);      /* x (2 bytes) */
		Stream_Read_UINT16(s, rect->y);      /* y (2 bytes) */
		Stream_Read_UINT16(s, rect->width);  /* width (2 bytes) */
		Stream_Read_UINT16(s, rect->height); /* height (2 bytes) */
		WLog_Print(context->priv->log, WLOG_DEBUG,
		           "rect %" PRIu16 " (x,y=%" PRIu16 ",%" PRIu16 " w,h=%" PRIu16 " %" PRIu16 ").", i,
		           rect->x, rect->y, rect->width, rect->height);
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, regionType);  /*regionType (2 bytes): MUST be set to CBT_REGION (0xCAC1)*/
	Stream_Read_UINT16(s, numTileSets); /*numTilesets (2 bytes): MUST be set to 0x0001.*/

	if (regionType != CBT_REGION)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid region type 0x%04" PRIX16 "",
		           regionType);
		return TRUE;
	}

	if (numTileSets != 0x0001)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid number of tilesets (%" PRIu16 ")",
		           numTileSets);
		return FALSE;
	}

	return TRUE;
}

typedef struct
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
} RFX_TILE_PROCESS_WORK_PARAM;

static INLINE void CALLBACK rfx_process_message_tile_work_callback(PTP_CALLBACK_INSTANCE instance,
                                                                   void* context, PTP_WORK work)
{
	RFX_TILE_PROCESS_WORK_PARAM* param = (RFX_TILE_PROCESS_WORK_PARAM*)context;
	WINPR_ASSERT(param);
	rfx_decode_rgb(param->context, param->tile, param->tile->data, 64 * 4);
}

static INLINE BOOL rfx_allocate_tiles(RFX_MESSAGE* WINPR_RESTRICT message, size_t count,
                                      BOOL allocOnly)
{
	WINPR_ASSERT(message);

	RFX_TILE** tmpTiles =
	    (RFX_TILE**)winpr_aligned_recalloc((void*)message->tiles, count, sizeof(RFX_TILE*), 32);
	if (!tmpTiles && (count != 0))
		return FALSE;

	message->tiles = tmpTiles;
	if (!allocOnly)
		message->numTiles = count;
	else
	{
		WINPR_ASSERT(message->numTiles <= count);
	}
	message->allocatedTiles = count;

	return TRUE;
}

static INLINE BOOL rfx_process_message_tileset(RFX_CONTEXT* WINPR_RESTRICT context,
                                               RFX_MESSAGE* WINPR_RESTRICT message,
                                               wStream* WINPR_RESTRICT s,
                                               UINT16* WINPR_RESTRICT pExpectedBlockType)
{
	BOOL rc = 0;
	size_t close_cnt = 0;
	BYTE quant = 0;
	RFX_TILE* tile = NULL;
	UINT32* quants = NULL;
	UINT16 subtype = 0;
	UINT16 numTiles = 0;
	UINT32 blockLen = 0;
	UINT32 blockType = 0;
	UINT32 tilesDataSize = 0;
	PTP_WORK* work_objects = NULL;
	RFX_TILE_PROCESS_WORK_PARAM* params = NULL;
	void* pmem = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->priv);
	WINPR_ASSERT(message);
	WINPR_ASSERT(pExpectedBlockType);

	if (*pExpectedBlockType != WBT_EXTENSION)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "message unexpected wants a tileset");
		return FALSE;
	}

	*pExpectedBlockType = WBT_FRAME_END;

	if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 14))
		return FALSE;

	Stream_Read_UINT16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */
	if (subtype != CBT_TILESET)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "invalid subtype, expected CBT_TILESET.");
		return FALSE;
	}

	Stream_Seek_UINT16(s);                   /* idx (2 bytes), must be set to 0x0000 */
	Stream_Seek_UINT16(s);                   /* properties (2 bytes) */
	Stream_Read_UINT8(s, context->numQuant); /* numQuant (1 byte) */
	Stream_Seek_UINT8(s);                    /* tileSize (1 byte), must be set to 0x40 */

	if (context->numQuant < 1)
	{
		WLog_Print(context->priv->log, WLOG_ERROR, "no quantization value.");
		return FALSE;
	}

	Stream_Read_UINT16(s, numTiles); /* numTiles (2 bytes) */
	if (numTiles < 1)
	{
		/* Windows Server 2012 (not R2) can send empty tile sets */
		return TRUE;
	}

	Stream_Read_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	if (!(pmem =
	          winpr_aligned_recalloc(context->quants, context->numQuant, 10 * sizeof(UINT32), 32)))
		return FALSE;

	quants = context->quants = (UINT32*)pmem;

	/* quantVals */
	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(context->priv->log, s, context->numQuant, 5ull))
		return FALSE;

	for (size_t i = 0; i < context->numQuant; i++)
	{
		/* RFX_CODEC_QUANT */
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		WLog_Print(context->priv->log, WLOG_DEBUG,
		           "quant %d (%" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32
		           " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 ").",
		           i, context->quants[i * 10], context->quants[i * 10 + 1],
		           context->quants[i * 10 + 2], context->quants[i * 10 + 3],
		           context->quants[i * 10 + 4], context->quants[i * 10 + 5],
		           context->quants[i * 10 + 6], context->quants[i * 10 + 7],
		           context->quants[i * 10 + 8], context->quants[i * 10 + 9]);
	}

	for (size_t i = 0; i < message->numTiles; i++)
	{
		ObjectPool_Return(context->priv->TilePool, message->tiles[i]);
		message->tiles[i] = NULL;
	}

	if (!rfx_allocate_tiles(message, numTiles, FALSE))
		return FALSE;

	if (context->priv->UseThreads)
	{
		work_objects = (PTP_WORK*)winpr_aligned_calloc(message->numTiles, sizeof(PTP_WORK), 32);
		params = (RFX_TILE_PROCESS_WORK_PARAM*)winpr_aligned_recalloc(
		    NULL, message->numTiles, sizeof(RFX_TILE_PROCESS_WORK_PARAM), 32);

		if (!work_objects)
		{
			winpr_aligned_free(params);
			return FALSE;
		}

		if (!params)
		{
			winpr_aligned_free((void*)work_objects);
			return FALSE;
		}
	}

	/* tiles */
	close_cnt = 0;
	rc = FALSE;

	if (Stream_GetRemainingLength(s) >= tilesDataSize)
	{
		rc = TRUE;
		for (size_t i = 0; i < message->numTiles; i++)
		{
			wStream subBuffer;
			wStream* sub = NULL;

			if (!(tile = (RFX_TILE*)ObjectPool_Take(context->priv->TilePool)))
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "RfxMessageTileSet failed to get tile from object pool");
				rc = FALSE;
				break;
			}

			message->tiles[i] = tile;

			/* RFX_TILE */
			if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 6))
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "RfxMessageTileSet packet too small to read tile %d/%" PRIu16 "", i,
				           message->numTiles);
				rc = FALSE;
				break;
			}

			sub = Stream_StaticInit(&subBuffer, Stream_Pointer(s), Stream_GetRemainingLength(s));
			Stream_Read_UINT16(
			    sub, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
			Stream_Read_UINT32(sub, blockLen); /* blockLen (4 bytes) */

			if (!Stream_SafeSeek(s, blockLen))
			{
				rc = FALSE;
				break;
			}
			if ((blockLen < 6 + 13) ||
			    (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, sub, blockLen - 6)))
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "RfxMessageTileSet not enough bytes to read tile %d/%" PRIu16
				           " with blocklen=%" PRIu32 "",
				           i, message->numTiles, blockLen);
				rc = FALSE;
				break;
			}

			if (blockType != CBT_TILE)
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "unknown block type 0x%" PRIX32 ", expected CBT_TILE (0xCAC3).",
				           blockType);
				rc = FALSE;
				break;
			}

			Stream_Read_UINT8(sub, tile->quantIdxY);  /* quantIdxY (1 byte) */
			Stream_Read_UINT8(sub, tile->quantIdxCb); /* quantIdxCb (1 byte) */
			Stream_Read_UINT8(sub, tile->quantIdxCr); /* quantIdxCr (1 byte) */
			if (tile->quantIdxY >= context->numQuant)
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "quantIdxY %" PRIu8 " >= numQuant %" PRIu8, tile->quantIdxY,
				           context->numQuant);
				rc = FALSE;
				break;
			}
			if (tile->quantIdxCb >= context->numQuant)
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "quantIdxCb %" PRIu8 " >= numQuant %" PRIu8, tile->quantIdxCb,
				           context->numQuant);
				rc = FALSE;
				break;
			}
			if (tile->quantIdxCr >= context->numQuant)
			{
				WLog_Print(context->priv->log, WLOG_ERROR,
				           "quantIdxCr %" PRIu8 " >= numQuant %" PRIu8, tile->quantIdxCr,
				           context->numQuant);
				rc = FALSE;
				break;
			}

			Stream_Read_UINT16(sub, tile->xIdx);      /* xIdx (2 bytes) */
			Stream_Read_UINT16(sub, tile->yIdx);      /* yIdx (2 bytes) */
			Stream_Read_UINT16(sub, tile->YLen);      /* YLen (2 bytes) */
			Stream_Read_UINT16(sub, tile->CbLen);     /* CbLen (2 bytes) */
			Stream_Read_UINT16(sub, tile->CrLen);     /* CrLen (2 bytes) */
			Stream_GetPointer(sub, tile->YData);
			if (!Stream_SafeSeek(sub, tile->YLen))
			{
				rc = FALSE;
				break;
			}
			Stream_GetPointer(sub, tile->CbData);
			if (!Stream_SafeSeek(sub, tile->CbLen))
			{
				rc = FALSE;
				break;
			}
			Stream_GetPointer(sub, tile->CrData);
			if (!Stream_SafeSeek(sub, tile->CrLen))
			{
				rc = FALSE;
				break;
			}
			tile->x = tile->xIdx * 64;
			tile->y = tile->yIdx * 64;

			if (context->priv->UseThreads)
			{
				if (!params)
				{
					rc = FALSE;
					break;
				}

				params[i].context = context;
				params[i].tile = message->tiles[i];

				if (!(work_objects[i] =
				          CreateThreadpoolWork(rfx_process_message_tile_work_callback,
				                               (void*)&params[i], &context->priv->ThreadPoolEnv)))
				{
					WLog_Print(context->priv->log, WLOG_ERROR, "CreateThreadpoolWork failed.");
					rc = FALSE;
					break;
				}

				SubmitThreadpoolWork(work_objects[i]);
				close_cnt = i + 1;
			}
			else
			{
				rfx_decode_rgb(context, tile, tile->data, 64 * 4);
			}
		}
	}

	if (context->priv->UseThreads)
	{
		for (size_t i = 0; i < close_cnt; i++)
		{
			WaitForThreadpoolWorkCallbacks(work_objects[i], FALSE);
			CloseThreadpoolWork(work_objects[i]);
		}
	}

	winpr_aligned_free((void*)work_objects);
	winpr_aligned_free(params);

	for (size_t i = 0; i < message->numTiles; i++)
	{
		if (!(tile = message->tiles[i]))
			continue;

		tile->YLen = tile->CbLen = tile->CrLen = 0;
		tile->YData = tile->CbData = tile->CrData = NULL;
	}

	return rc;
}

BOOL rfx_process_message(RFX_CONTEXT* WINPR_RESTRICT context, const BYTE* WINPR_RESTRICT data,
                         UINT32 length, UINT32 left, UINT32 top, BYTE* WINPR_RESTRICT dst,
                         UINT32 dstFormat, UINT32 dstStride, UINT32 dstHeight,
                         REGION16* WINPR_RESTRICT invalidRegion)
{
	REGION16 updateRegion = { 0 };
	wStream inStream = { 0 };
	BOOL ok = TRUE;

	if (!context || !data || !length)
		return FALSE;

	WINPR_ASSERT(context->priv);
	RFX_MESSAGE* message = &context->currentMessage;

	wStream* s = Stream_StaticConstInit(&inStream, data, length);

	while (ok && Stream_GetRemainingLength(s) > 6)
	{
		wStream subStreamBuffer = { 0 };
		size_t extraBlockLen = 0;
		UINT32 blockLen = 0;
		UINT32 blockType = 0;

		/* RFX_BLOCKT */
		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
		Stream_Read_UINT32(s, blockLen);  /* blockLen (4 bytes) */
		WLog_Print(context->priv->log, WLOG_DEBUG, "blockType 0x%" PRIX32 " blockLen %" PRIu32 "",
		           blockType, blockLen);

		if (blockLen < 6)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "blockLen too small(%" PRIu32 ")", blockLen);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, blockLen - 6))
			return FALSE;

		if (blockType > WBT_CONTEXT && context->decodedHeaderBlocks != RFX_DECODED_HEADERS)
		{
			WLog_Print(context->priv->log, WLOG_ERROR, "incomplete header blocks processing");
			return FALSE;
		}

		if (blockType >= WBT_CONTEXT && blockType <= WBT_EXTENSION)
		{
			/* RFX_CODEC_CHANNELT */
			UINT8 codecId = 0;
			UINT8 channelId = 0;

			if (!Stream_CheckAndLogRequiredLengthWLog(context->priv->log, s, 2))
				return FALSE;

			extraBlockLen = 2;
			Stream_Read_UINT8(s, codecId);   /* codecId (1 byte) must be set to 0x01 */
			Stream_Read_UINT8(s, channelId); /* channelId (1 byte) 0xFF or 0x00, see below */

			if (codecId != 0x01)
			{
				WLog_Print(context->priv->log, WLOG_ERROR, "invalid codecId 0x%02" PRIX8 "",
				           codecId);
				return FALSE;
			}

			if (blockType == WBT_CONTEXT)
			{
				/* If the blockType is set to WBT_CONTEXT, then channelId MUST be set to 0xFF.*/
				if (channelId != 0xFF)
				{
					WLog_Print(context->priv->log, WLOG_ERROR,
					           "invalid channelId 0x%02" PRIX8 " for blockType 0x%08" PRIX32 "",
					           channelId, blockType);
					return FALSE;
				}
			}
			else
			{
				/* For all other values of blockType, channelId MUST be set to 0x00. */
				if (channelId != 0x00)
				{
					WLog_Print(context->priv->log, WLOG_ERROR,
					           "invalid channelId 0x%02" PRIX8 " for blockType WBT_CONTEXT",
					           channelId);
					return FALSE;
				}
			}
		}

		const size_t blockLenNoHeader = blockLen - 6;
		if (blockLenNoHeader < extraBlockLen)
		{
			WLog_Print(context->priv->log, WLOG_ERROR,
			           "blockLen too small(%" PRIu32 "), must be >= 6 + %" PRIu16, blockLen,
			           extraBlockLen);
			return FALSE;
		}

		const size_t subStreamLen = blockLenNoHeader - extraBlockLen;
		wStream* subStream = Stream_StaticInit(&subStreamBuffer, Stream_Pointer(s), subStreamLen);
		Stream_Seek(s, subStreamLen);

		switch (blockType)
		{
			/* Header messages:
			 * The stream MUST start with the header messages and any of these headers can appear
			 * in the stream at a later stage. The header messages can be repeated.
			 */
			case WBT_SYNC:
				ok = rfx_process_message_sync(context, subStream);
				break;

			case WBT_CONTEXT:
				ok = rfx_process_message_context(context, subStream);
				break;

			case WBT_CODEC_VERSIONS:
				ok = rfx_process_message_codec_versions(context, subStream);
				break;

			case WBT_CHANNELS:
				ok = rfx_process_message_channels(context, subStream);
				break;

				/* Data messages:
				 * The data associated with each encoded frame or image is always bracketed by the
				 * TS_RFX_FRAME_BEGIN (section 2.2.2.3.1) and TS_RFX_FRAME_END (section 2.2.2.3.2)
				 * messages. There MUST only be one TS_RFX_REGION (section 2.2.2.3.3) message per
				 * frame and one TS_RFX_TILESET (section 2.2.2.3.4) message per TS_RFX_REGION.
				 */

			case WBT_FRAME_BEGIN:
				ok = rfx_process_message_frame_begin(context, message, subStream,
				                                     &context->expectedDataBlockType);
				break;

			case WBT_REGION:
				ok = rfx_process_message_region(context, message, subStream,
				                                &context->expectedDataBlockType);
				break;

			case WBT_EXTENSION:
				ok = rfx_process_message_tileset(context, message, subStream,
				                                 &context->expectedDataBlockType);
				break;

			case WBT_FRAME_END:
				ok = rfx_process_message_frame_end(context, message, subStream,
				                                   &context->expectedDataBlockType);
				break;

			default:
				WLog_Print(context->priv->log, WLOG_ERROR, "unknown blockType 0x%" PRIX32 "",
				           blockType);
				return FALSE;
		}
	}

	if (ok)
	{
		UINT32 nbUpdateRects = 0;
		REGION16 clippingRects = { 0 };
		const RECTANGLE_16* updateRects = NULL;
		const DWORD formatSize = FreeRDPGetBytesPerPixel(context->pixel_format);
		const UINT32 dstWidth = dstStride / FreeRDPGetBytesPerPixel(dstFormat);
		region16_init(&clippingRects);

		WINPR_ASSERT(dstWidth <= UINT16_MAX);
		WINPR_ASSERT(dstHeight <= UINT16_MAX);
		for (UINT32 i = 0; i < message->numRects; i++)
		{
			RECTANGLE_16 clippingRect = { 0 };
			const RFX_RECT* rect = &(message->rects[i]);

			WINPR_ASSERT(left + rect->x <= UINT16_MAX);
			WINPR_ASSERT(top + rect->y <= UINT16_MAX);
			WINPR_ASSERT(clippingRect.left + rect->width <= UINT16_MAX);
			WINPR_ASSERT(clippingRect.top + rect->height <= UINT16_MAX);

			clippingRect.left = (UINT16)MIN(left + rect->x, dstWidth);
			clippingRect.top = (UINT16)MIN(top + rect->y, dstHeight);
			clippingRect.right = (UINT16)MIN(clippingRect.left + rect->width, dstWidth);
			clippingRect.bottom = (UINT16)MIN(clippingRect.top + rect->height, dstHeight);
			region16_union_rect(&clippingRects, &clippingRects, &clippingRect);
		}

		for (UINT32 i = 0; i < message->numTiles; i++)
		{
			RECTANGLE_16 updateRect = { 0 };
			const RFX_TILE* tile = rfx_message_get_tile(message, i);

			WINPR_ASSERT(left + tile->x <= UINT16_MAX);
			WINPR_ASSERT(top + tile->y <= UINT16_MAX);

			updateRect.left = (UINT16)left + tile->x;
			updateRect.top = (UINT16)top + tile->y;
			updateRect.right = updateRect.left + 64;
			updateRect.bottom = updateRect.top + 64;
			region16_init(&updateRegion);
			region16_intersect_rect(&updateRegion, &clippingRects, &updateRect);
			updateRects = region16_rects(&updateRegion, &nbUpdateRects);

			for (UINT32 j = 0; j < nbUpdateRects; j++)
			{
				const UINT32 stride = 64 * formatSize;
				const UINT32 nXDst = updateRects[j].left;
				const UINT32 nYDst = updateRects[j].top;
				const UINT32 nXSrc = nXDst - updateRect.left;
				const UINT32 nYSrc = nYDst - updateRect.top;
				const UINT32 nWidth = updateRects[j].right - updateRects[j].left;
				const UINT32 nHeight = updateRects[j].bottom - updateRects[j].top;

				if (!freerdp_image_copy_no_overlap(dst, dstFormat, dstStride, nXDst, nYDst, nWidth,
				                                   nHeight, tile->data, context->pixel_format,
				                                   stride, nXSrc, nYSrc, NULL, FREERDP_FLIP_NONE))
				{
					region16_uninit(&updateRegion);
					WLog_Print(context->priv->log, WLOG_ERROR,
					           "nbUpdateRectx[%" PRIu32 " (%" PRIu32 ")] freerdp_image_copy failed",
					           j, nbUpdateRects);
					return FALSE;
				}

				if (invalidRegion)
					region16_union_rect(invalidRegion, invalidRegion, &updateRects[j]);
			}

			region16_uninit(&updateRegion);
		}

		region16_uninit(&clippingRects);
		return TRUE;
	}
	else
	{
		rfx_message_free(context, message);
		context->currentMessage.freeArray = TRUE;
	}

	WLog_Print(context->priv->log, WLOG_ERROR, "failed");
	return FALSE;
}

const UINT32* rfx_message_get_quants(const RFX_MESSAGE* WINPR_RESTRICT message,
                                     UINT16* WINPR_RESTRICT numQuantVals)
{
	WINPR_ASSERT(message);
	if (numQuantVals)
		*numQuantVals = message->numQuant;
	return message->quantVals;
}

const RFX_TILE** rfx_message_get_tiles(const RFX_MESSAGE* WINPR_RESTRICT message,
                                       UINT16* WINPR_RESTRICT numTiles)
{
	WINPR_ASSERT(message);
	if (numTiles)
		*numTiles = message->numTiles;

	union
	{
		RFX_TILE** pp;
		const RFX_TILE** ppc;
	} cnv;
	cnv.pp = message->tiles;
	return cnv.ppc;
}

UINT16 rfx_message_get_tile_count(const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(message);
	return message->numTiles;
}

const RFX_RECT* rfx_message_get_rects(const RFX_MESSAGE* WINPR_RESTRICT message,
                                      UINT16* WINPR_RESTRICT numRects)
{
	WINPR_ASSERT(message);
	if (numRects)
		*numRects = message->numRects;
	return message->rects;
}

UINT16 rfx_message_get_rect_count(const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(message);
	return message->numRects;
}

void rfx_message_free(RFX_CONTEXT* WINPR_RESTRICT context, RFX_MESSAGE* WINPR_RESTRICT message)
{
	if (!message)
		return;

	winpr_aligned_free(message->rects);

	if (message->tiles)
	{
		for (size_t i = 0; i < message->numTiles; i++)
		{
			RFX_TILE* tile = message->tiles[i];
			if (!tile)
				continue;

			if (tile->YCbCrData)
			{
				BufferPool_Return(context->priv->BufferPool, tile->YCbCrData);
				tile->YCbCrData = NULL;
			}

			ObjectPool_Return(context->priv->TilePool, (void*)tile);
		}

		rfx_allocate_tiles(message, 0, FALSE);
	}

	const BOOL freeArray = message->freeArray;
	const RFX_MESSAGE empty = { 0 };
	*message = empty;

	if (!freeArray)
		winpr_aligned_free(message);
}

static INLINE void rfx_update_context_properties(RFX_CONTEXT* WINPR_RESTRICT context)
{
	UINT16 properties = 0;

	WINPR_ASSERT(context);
	/* properties in tilesets: note that this has different format from the one in TS_RFX_CONTEXT */
	properties = 1;                          /* lt */
	properties |= (context->flags << 1);     /* flags */
	properties |= (COL_CONV_ICT << 4);       /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 10); /* et */
	properties |= (SCALAR_QUANTIZATION << 14);                                              /* qt */
	context->properties = properties;
}

static INLINE void rfx_write_message_sync(const RFX_CONTEXT* WINPR_RESTRICT context,
                                          wStream* WINPR_RESTRICT s)
{
	WINPR_ASSERT(context);

	Stream_Write_UINT16(s, WBT_SYNC);       /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 12);             /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT32(s, WF_MAGIC);       /* magic (4 bytes) */
	Stream_Write_UINT16(s, WF_VERSION_1_0); /* version (2 bytes) */
}

static INLINE void rfx_write_message_codec_versions(const RFX_CONTEXT* WINPR_RESTRICT context,
                                                    wStream* WINPR_RESTRICT s)
{
	WINPR_ASSERT(context);

	Stream_Write_UINT16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 10);                 /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1);                   /* numCodecs (1 byte) */
	Stream_Write_UINT8(s, 1);                   /* codecs.codecId (1 byte) */
	Stream_Write_UINT16(s, WF_VERSION_1_0);     /* codecs.version (2 bytes) */
}

static INLINE void rfx_write_message_channels(const RFX_CONTEXT* WINPR_RESTRICT context,
                                              wStream* WINPR_RESTRICT s)
{
	WINPR_ASSERT(context);

	Stream_Write_UINT16(s, WBT_CHANNELS);    /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 12);              /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1);                /* numChannels (1 byte) */
	Stream_Write_UINT8(s, 0);                /* Channel.channelId (1 byte) */
	Stream_Write_UINT16(s, context->width);  /* Channel.width (2 bytes) */
	Stream_Write_UINT16(s, context->height); /* Channel.height (2 bytes) */
}

static INLINE void rfx_write_message_context(RFX_CONTEXT* WINPR_RESTRICT context,
                                             wStream* WINPR_RESTRICT s)
{
	UINT16 properties = 0;
	WINPR_ASSERT(context);

	Stream_Write_UINT16(s, WBT_CONTEXT);   /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 13);            /* CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1);              /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0xFF);           /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 0);              /* ctxId (1 byte) */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize (2 bytes) */
	/* properties */
	properties = context->flags;             /* flags */
	properties |= (COL_CONV_ICT << 3);       /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 9); /* et */
	properties |= (SCALAR_QUANTIZATION << 13);                                             /* qt */
	Stream_Write_UINT16(s, properties); /* properties (2 bytes) */
	rfx_update_context_properties(context);
}

static INLINE BOOL rfx_compose_message_header(RFX_CONTEXT* WINPR_RESTRICT context,
                                              wStream* WINPR_RESTRICT s)
{
	WINPR_ASSERT(context);
	if (!Stream_EnsureRemainingCapacity(s, 12 + 10 + 12 + 13))
		return FALSE;

	rfx_write_message_sync(context, s);
	rfx_write_message_context(context, s);
	rfx_write_message_codec_versions(context, s);
	rfx_write_message_channels(context, s);
	return TRUE;
}

static INLINE size_t rfx_tile_length(const RFX_TILE* WINPR_RESTRICT tile)
{
	WINPR_ASSERT(tile);
	return 19ull + tile->YLen + tile->CbLen + tile->CrLen;
}

static INLINE BOOL rfx_write_tile(wStream* WINPR_RESTRICT s, const RFX_TILE* WINPR_RESTRICT tile)
{
	const size_t blockLen = rfx_tile_length(tile);
	if (blockLen > UINT32_MAX)
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, CBT_TILE);           /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, (UINT32)blockLen);   /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, tile->quantIdxY);     /* quantIdxY (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCb);    /* quantIdxCb (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCr);    /* quantIdxCr (1 byte) */
	Stream_Write_UINT16(s, tile->xIdx);         /* xIdx (2 bytes) */
	Stream_Write_UINT16(s, tile->yIdx);         /* yIdx (2 bytes) */
	Stream_Write_UINT16(s, tile->YLen);         /* YLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CbLen);        /* CbLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CrLen);        /* CrLen (2 bytes) */
	Stream_Write(s, tile->YData, tile->YLen);   /* YData */
	Stream_Write(s, tile->CbData, tile->CbLen); /* CbData */
	Stream_Write(s, tile->CrData, tile->CrLen); /* CrData */
	return TRUE;
}

struct S_RFX_TILE_COMPOSE_WORK_PARAM
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};

static INLINE void CALLBACK rfx_compose_message_tile_work_callback(PTP_CALLBACK_INSTANCE instance,
                                                                   void* context, PTP_WORK work)
{
	RFX_TILE_COMPOSE_WORK_PARAM* param = (RFX_TILE_COMPOSE_WORK_PARAM*)context;
	WINPR_ASSERT(param);
	rfx_encode_rgb(param->context, param->tile);
}

static INLINE BOOL computeRegion(const RFX_RECT* WINPR_RESTRICT rects, size_t numRects,
                                 REGION16* WINPR_RESTRICT region, size_t width, size_t height)
{
	const RECTANGLE_16 mainRect = { 0, 0, width, height };

	WINPR_ASSERT(rects);
	for (size_t i = 0; i < numRects; i++)
	{
		const RFX_RECT* rect = &rects[i];
		RECTANGLE_16 rect16 = { 0 };
		rect16.left = rect->x;
		rect16.top = rect->y;
		rect16.right = rect->x + rect->width;
		rect16.bottom = rect->y + rect->height;

		if (!region16_union_rect(region, region, &rect16))
			return FALSE;
	}

	return region16_intersect_rect(region, region, &mainRect);
}

#define TILE_NO(v) ((v) / 64)

static INLINE BOOL setupWorkers(RFX_CONTEXT* WINPR_RESTRICT context, size_t nbTiles)
{
	WINPR_ASSERT(context);

	RFX_CONTEXT_PRIV* priv = context->priv;
	WINPR_ASSERT(priv);

	void* pmem = NULL;

	if (!context->priv->UseThreads)
		return TRUE;

	if (!(pmem = winpr_aligned_recalloc((void*)priv->workObjects, nbTiles, sizeof(PTP_WORK), 32)))
		return FALSE;

	priv->workObjects = (PTP_WORK*)pmem;

	if (!(pmem = winpr_aligned_recalloc(priv->tileWorkParams, nbTiles,
	                                    sizeof(RFX_TILE_COMPOSE_WORK_PARAM), 32)))
		return FALSE;

	priv->tileWorkParams = (RFX_TILE_COMPOSE_WORK_PARAM*)pmem;
	return TRUE;
}

static INLINE BOOL rfx_ensure_tiles(RFX_MESSAGE* WINPR_RESTRICT message, size_t count)
{
	WINPR_ASSERT(message);

	if (message->numTiles + count <= message->allocatedTiles)
		return TRUE;

	const size_t alloc = MAX(message->allocatedTiles + 1024, message->numTiles + count);
	return rfx_allocate_tiles(message, alloc, TRUE);
}

RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* WINPR_RESTRICT context,
                                const RFX_RECT* WINPR_RESTRICT rects, size_t numRects,
                                const BYTE* WINPR_RESTRICT data, UINT32 w, UINT32 h, size_t s)
{
	const UINT32 width = w;
	const UINT32 height = h;
	const UINT32 scanline = (UINT32)s;
	RFX_MESSAGE* message = NULL;
	PTP_WORK* workObject = NULL;
	RFX_TILE_COMPOSE_WORK_PARAM* workParam = NULL;
	BOOL success = FALSE;
	REGION16 rectsRegion = { 0 };
	REGION16 tilesRegion = { 0 };
	RECTANGLE_16 currentTileRect = { 0 };
	const RECTANGLE_16* regionRect = NULL;

	WINPR_ASSERT(data);
	WINPR_ASSERT(rects);
	WINPR_ASSERT(numRects > 0);
	WINPR_ASSERT(w > 0);
	WINPR_ASSERT(h > 0);
	WINPR_ASSERT(s > 0);

	if (!(message = (RFX_MESSAGE*)winpr_aligned_calloc(1, sizeof(RFX_MESSAGE), 32)))
		return NULL;

	region16_init(&tilesRegion);
	region16_init(&rectsRegion);

	if (context->state == RFX_STATE_SEND_HEADERS)
		rfx_update_context_properties(context);

	message->frameIdx = context->frameIdx++;

	if (!context->numQuant)
	{
		WINPR_ASSERT(context->quants == NULL);
		if (!(context->quants =
		          (UINT32*)winpr_aligned_malloc(sizeof(rfx_default_quantization_values), 32)))
			goto skip_encoding_loop;

		CopyMemory(context->quants, &rfx_default_quantization_values,
		           sizeof(rfx_default_quantization_values));
		context->numQuant = 1;
		context->quantIdxY = 0;
		context->quantIdxCb = 0;
		context->quantIdxCr = 0;
	}

	message->numQuant = context->numQuant;
	message->quantVals = context->quants;
	const UINT32 bytesPerPixel = (context->bits_per_pixel / 8);

	if (!computeRegion(rects, numRects, &rectsRegion, width, height))
		goto skip_encoding_loop;

	const RECTANGLE_16* extents = region16_extents(&rectsRegion);
	WINPR_ASSERT((INT32)extents->right - extents->left > 0);
	WINPR_ASSERT((INT32)extents->bottom - extents->top > 0);
	const UINT32 maxTilesX = 1 + TILE_NO(extents->right - 1) - TILE_NO(extents->left);
	const UINT32 maxTilesY = 1 + TILE_NO(extents->bottom - 1) - TILE_NO(extents->top);
	const UINT32 maxNbTiles = maxTilesX * maxTilesY;

	if (!rfx_ensure_tiles(message, maxNbTiles))
		goto skip_encoding_loop;

	if (!setupWorkers(context, maxNbTiles))
		goto skip_encoding_loop;

	if (context->priv->UseThreads)
	{
		workObject = context->priv->workObjects;
		workParam = context->priv->tileWorkParams;
	}

	UINT32 regionNbRects = 0;
	regionRect = region16_rects(&rectsRegion, &regionNbRects);

	if (!(message->rects = winpr_aligned_calloc(regionNbRects, sizeof(RFX_RECT), 32)))
		goto skip_encoding_loop;

	message->numRects = regionNbRects;

	for (UINT32 i = 0; i < regionNbRects; i++, regionRect++)
	{
		RFX_RECT* rfxRect = &message->rects[i];
		UINT32 startTileX = regionRect->left / 64;
		UINT32 endTileX = (regionRect->right - 1) / 64;
		UINT32 startTileY = regionRect->top / 64;
		UINT32 endTileY = (regionRect->bottom - 1) / 64;
		rfxRect->x = regionRect->left;
		rfxRect->y = regionRect->top;
		rfxRect->width = (regionRect->right - regionRect->left);
		rfxRect->height = (regionRect->bottom - regionRect->top);

		for (UINT32 yIdx = startTileY, gridRelY = startTileY * 64; yIdx <= endTileY;
		     yIdx++, gridRelY += 64)
		{
			UINT32 tileHeight = 64;

			if ((yIdx == endTileY) && (gridRelY + 64 > height))
				tileHeight = height - gridRelY;

			currentTileRect.top = gridRelY;
			currentTileRect.bottom = gridRelY + tileHeight;

			for (UINT32 xIdx = startTileX, gridRelX = startTileX * 64; xIdx <= endTileX;
			     xIdx++, gridRelX += 64)
			{
				union
				{
					const BYTE* cpv;
					BYTE* pv;
				} cnv;
				int tileWidth = 64;

				if ((xIdx == endTileX) && (gridRelX + 64 > width))
					tileWidth = width - gridRelX;

				currentTileRect.left = gridRelX;
				currentTileRect.right = gridRelX + tileWidth;

				/* checks if this tile is already treated */
				if (region16_intersects_rect(&tilesRegion, &currentTileRect))
					continue;

				RFX_TILE* tile = (RFX_TILE*)ObjectPool_Take(context->priv->TilePool);
				if (!tile)
					goto skip_encoding_loop;

				tile->xIdx = xIdx;
				tile->yIdx = yIdx;
				tile->x = gridRelX;
				tile->y = gridRelY;
				tile->scanline = scanline;
				tile->width = tileWidth;
				tile->height = tileHeight;
				const UINT32 ax = gridRelX;
				const UINT32 ay = gridRelY;

				if (tile->data && tile->allocated)
				{
					winpr_aligned_free(tile->data);
					tile->allocated = FALSE;
				}

				/* Cast away const */
				cnv.cpv = &data[(ay * scanline) + (ax * bytesPerPixel)];
				tile->data = cnv.pv;
				tile->quantIdxY = context->quantIdxY;
				tile->quantIdxCb = context->quantIdxCb;
				tile->quantIdxCr = context->quantIdxCr;
				tile->YLen = tile->CbLen = tile->CrLen = 0;

				if (!(tile->YCbCrData = (BYTE*)BufferPool_Take(context->priv->BufferPool, -1)))
					goto skip_encoding_loop;

				tile->YData = &(tile->YCbCrData[((8192 + 32) * 0) + 16]);
				tile->CbData = &(tile->YCbCrData[((8192 + 32) * 1) + 16]);
				tile->CrData = &(tile->YCbCrData[((8192 + 32) * 2) + 16]);

				if (!rfx_ensure_tiles(message, 1))
					goto skip_encoding_loop;
				message->tiles[message->numTiles++] = tile;

				if (context->priv->UseThreads)
				{
					workParam->context = context;
					workParam->tile = tile;

					if (!(*workObject = CreateThreadpoolWork(rfx_compose_message_tile_work_callback,
					                                         (void*)workParam,
					                                         &context->priv->ThreadPoolEnv)))
					{
						goto skip_encoding_loop;
					}

					SubmitThreadpoolWork(*workObject);
					workObject++;
					workParam++;
				}
				else
				{
					rfx_encode_rgb(context, tile);
				}

				if (!region16_union_rect(&tilesRegion, &tilesRegion, &currentTileRect))
					goto skip_encoding_loop;
			} /* xIdx */
		}     /* yIdx */
	}         /* rects */

	success = TRUE;
skip_encoding_loop:

	/* when using threads ensure all computations are done */
	if (success)
	{
		message->tilesDataSize = 0;
		workObject = context->priv->workObjects;

		for (UINT32 i = 0; i < message->numTiles; i++)
		{
			if (context->priv->UseThreads)
			{
				if (*workObject)
				{
					WaitForThreadpoolWorkCallbacks(*workObject, FALSE);
					CloseThreadpoolWork(*workObject);
				}

				workObject++;
			}

			const RFX_TILE* tile = message->tiles[i];
			message->tilesDataSize += rfx_tile_length(tile);
		}

		region16_uninit(&tilesRegion);
		region16_uninit(&rectsRegion);

		return message;
	}

	WLog_Print(context->priv->log, WLOG_ERROR, "failed");

	rfx_message_free(context, message);
	return NULL;
}

static INLINE BOOL rfx_clone_rects(RFX_MESSAGE* WINPR_RESTRICT dst,
                                   const RFX_MESSAGE* WINPR_RESTRICT src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	WINPR_ASSERT(dst->rects == NULL);
	WINPR_ASSERT(dst->numRects == 0);

	if (src->numRects == 0)
		return TRUE;

	dst->rects = winpr_aligned_calloc(src->numRects, sizeof(RECTANGLE_16), 32);
	if (!dst->rects)
		return FALSE;
	dst->numRects = src->numRects;
	for (size_t x = 0; x < src->numRects; x++)
	{
		dst->rects[x] = src->rects[x];
	}
	return TRUE;
}

static INLINE BOOL rfx_clone_quants(RFX_MESSAGE* WINPR_RESTRICT dst,
                                    const RFX_MESSAGE* WINPR_RESTRICT src)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	WINPR_ASSERT(dst->quantVals == NULL);
	WINPR_ASSERT(dst->numQuant == 0);

	if (src->numQuant == 0)
		return TRUE;

	/* quantVals are part of context */
	dst->quantVals = src->quantVals;
	dst->numQuant = src->numQuant;

	return TRUE;
}

static INLINE RFX_MESSAGE* rfx_split_message(RFX_CONTEXT* WINPR_RESTRICT context,
                                             RFX_MESSAGE* WINPR_RESTRICT message,
                                             size_t* WINPR_RESTRICT numMessages, size_t maxDataSize)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);
	WINPR_ASSERT(numMessages);

	maxDataSize -= 1024; /* reserve enough space for headers */
	*numMessages = ((message->tilesDataSize + maxDataSize) / maxDataSize) * 4ull;

	RFX_MESSAGE* messages =
	    (RFX_MESSAGE*)winpr_aligned_calloc((*numMessages), sizeof(RFX_MESSAGE), 32);
	if (!messages)
		return NULL;

	UINT32 j = 0;
	for (UINT16 i = 0; i < message->numTiles; i++)
	{
		RFX_TILE* tile = message->tiles[i];
		RFX_MESSAGE* msg = &messages[j];

		WINPR_ASSERT(tile);
		WINPR_ASSERT(msg);

		const size_t tileDataSize = rfx_tile_length(tile);

		if ((msg->tilesDataSize + tileDataSize) > ((UINT32)maxDataSize))
			j++;

		if (msg->numTiles == 0)
		{
			msg->frameIdx = message->frameIdx + j;
			if (!rfx_clone_quants(msg, message))
				goto free_messages;
			if (!rfx_clone_rects(msg, message))
				goto free_messages;
			msg->freeArray = TRUE;
			if (!rfx_allocate_tiles(msg, message->numTiles, TRUE))
				goto free_messages;
		}

		msg->tilesDataSize += tileDataSize;

		WINPR_ASSERT(msg->numTiles < msg->allocatedTiles);
		msg->tiles[msg->numTiles++] = message->tiles[i];
		message->tiles[i] = NULL;
	}

	*numMessages = j + 1ULL;
	context->frameIdx += j;
	message->numTiles = 0;
	return messages;
free_messages:

	for (size_t i = 0; i < j; i++)
		rfx_allocate_tiles(&messages[i], 0, FALSE);

	winpr_aligned_free(messages);
	return NULL;
}

const RFX_MESSAGE* rfx_message_list_get(const RFX_MESSAGE_LIST* WINPR_RESTRICT messages, size_t idx)
{
	WINPR_ASSERT(messages);
	if (idx >= messages->count)
		return NULL;
	WINPR_ASSERT(messages->list);
	return &messages->list[idx];
}

void rfx_message_list_free(RFX_MESSAGE_LIST* messages)
{
	if (!messages)
		return;
	for (size_t x = 0; x < messages->count; x++)
		rfx_message_free(messages->context, &messages->list[x]);
	free(messages);
}

static INLINE RFX_MESSAGE_LIST* rfx_message_list_new(RFX_CONTEXT* WINPR_RESTRICT context,
                                                     RFX_MESSAGE* WINPR_RESTRICT messages,
                                                     size_t count)
{
	WINPR_ASSERT(context);
	RFX_MESSAGE_LIST* msg = calloc(1, sizeof(RFX_MESSAGE_LIST));
	WINPR_ASSERT(msg);

	msg->context = context;
	msg->count = count;
	msg->list = messages;
	return msg;
}

RFX_MESSAGE_LIST* rfx_encode_messages(RFX_CONTEXT* WINPR_RESTRICT context,
                                      const RFX_RECT* WINPR_RESTRICT rects, size_t numRects,
                                      const BYTE* WINPR_RESTRICT data, UINT32 width, UINT32 height,
                                      UINT32 scanline, size_t* WINPR_RESTRICT numMessages,
                                      size_t maxDataSize)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(numMessages);

	RFX_MESSAGE* message =
	    rfx_encode_message(context, rects, numRects, data, width, height, scanline);
	if (!message)
		return NULL;

	RFX_MESSAGE* list = rfx_split_message(context, message, numMessages, maxDataSize);
	rfx_message_free(context, message);
	if (!list)
		return NULL;

	return rfx_message_list_new(context, list, *numMessages);
}

static INLINE BOOL rfx_write_message_tileset(RFX_CONTEXT* WINPR_RESTRICT context,
                                             wStream* WINPR_RESTRICT s,
                                             const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);

	const UINT32 blockLen = 22 + (message->numQuant * 5) + message->tilesDataSize;

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, WBT_EXTENSION);          /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);               /* set CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1);                       /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0);                       /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT16(s, CBT_TILESET);            /* subtype (2 bytes) */
	Stream_Write_UINT16(s, 0);                      /* idx (2 bytes) */
	Stream_Write_UINT16(s, context->properties);    /* properties (2 bytes) */
	Stream_Write_UINT8(s, message->numQuant);       /* numQuant (1 byte) */
	Stream_Write_UINT8(s, 0x40);                    /* tileSize (1 byte) */
	Stream_Write_UINT16(s, message->numTiles);      /* numTiles (2 bytes) */
	Stream_Write_UINT32(s, message->tilesDataSize); /* tilesDataSize (4 bytes) */

	UINT32* quantVals = message->quantVals;
	for (size_t i = 0; i < message->numQuant * 5ul; i++)
	{
		WINPR_ASSERT(quantVals);
		Stream_Write_UINT8(s, quantVals[0] + (quantVals[1] << 4));
		quantVals += 2;
	}

	for (size_t i = 0; i < message->numTiles; i++)
	{
		RFX_TILE* tile = message->tiles[i];
		if (!tile)
			return FALSE;

		if (!rfx_write_tile(s, tile))
			return FALSE;
	}

#ifdef WITH_DEBUG_RFX
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "numQuant: %" PRIu16 " numTiles: %" PRIu16 " tilesDataSize: %" PRIu32 "",
	           message->numQuant, message->numTiles, message->tilesDataSize);
#endif
	return TRUE;
}

static INLINE BOOL rfx_write_message_frame_begin(RFX_CONTEXT* WINPR_RESTRICT context,
                                                 wStream* WINPR_RESTRICT s,
                                                 const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);

	if (!Stream_EnsureRemainingCapacity(s, 14))
		return FALSE;

	Stream_Write_UINT16(s, WBT_FRAME_BEGIN);   /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 14);                /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1);                  /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0);                  /* CodecChannelT.channelId */
	Stream_Write_UINT32(s, message->frameIdx); /* frameIdx */
	Stream_Write_UINT16(s, 1);                 /* numRegions */
	return TRUE;
}

static INLINE BOOL rfx_write_message_region(RFX_CONTEXT* WINPR_RESTRICT context,
                                            wStream* WINPR_RESTRICT s,
                                            const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);

	const size_t blockLen = 15 + (message->numRects * 8);
	if (blockLen > UINT32_MAX)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, WBT_REGION);        /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, (UINT32)blockLen);  /* set CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1);                  /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0);                  /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 1);                  /* regionFlags (1 byte) */
	Stream_Write_UINT16(s, message->numRects); /* numRects (2 bytes) */

	for (UINT16 i = 0; i < message->numRects; i++)
	{
		const RFX_RECT* rect = rfx_message_get_rect_const(message, i);
		WINPR_ASSERT(rect);

		/* Clipping rectangles are relative to destLeft, destTop */
		Stream_Write_UINT16(s, rect->x);      /* x (2 bytes) */
		Stream_Write_UINT16(s, rect->y);      /* y (2 bytes) */
		Stream_Write_UINT16(s, rect->width);  /* width (2 bytes) */
		Stream_Write_UINT16(s, rect->height); /* height (2 bytes) */
	}

	Stream_Write_UINT16(s, CBT_REGION); /* regionType (2 bytes) */
	Stream_Write_UINT16(s, 1);          /* numTilesets (2 bytes) */
	return TRUE;
}

static INLINE BOOL rfx_write_message_frame_end(RFX_CONTEXT* WINPR_RESTRICT context,
                                               wStream* WINPR_RESTRICT s,
                                               const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);

	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 8);             /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1);              /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0);              /* CodecChannelT.channelId */
	return TRUE;
}

BOOL rfx_write_message(RFX_CONTEXT* WINPR_RESTRICT context, wStream* WINPR_RESTRICT s,
                       const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(message);

	if (context->state == RFX_STATE_SEND_HEADERS)
	{
		if (!rfx_compose_message_header(context, s))
			return FALSE;

		context->state = RFX_STATE_SEND_FRAME_DATA;
	}

	if (!rfx_write_message_frame_begin(context, s, message) ||
	    !rfx_write_message_region(context, s, message) ||
	    !rfx_write_message_tileset(context, s, message) ||
	    !rfx_write_message_frame_end(context, s, message))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL rfx_compose_message(RFX_CONTEXT* WINPR_RESTRICT context, wStream* WINPR_RESTRICT s,
                         const RFX_RECT* WINPR_RESTRICT rects, size_t numRects,
                         const BYTE* WINPR_RESTRICT data, UINT32 width, UINT32 height,
                         UINT32 scanline)
{
	WINPR_ASSERT(context);
	RFX_MESSAGE* message =
	    rfx_encode_message(context, rects, numRects, data, width, height, scanline);
	if (!message)
		return FALSE;

	const BOOL ret = rfx_write_message(context, s, message);
	rfx_message_free(context, message);
	return ret;
}

BOOL rfx_context_set_mode(RFX_CONTEXT* WINPR_RESTRICT context, RLGR_MODE mode)
{
	WINPR_ASSERT(context);
	context->mode = mode;
	return TRUE;
}

RLGR_MODE rfx_context_get_mode(RFX_CONTEXT* WINPR_RESTRICT context)
{
	WINPR_ASSERT(context);
	return context->mode;
}

UINT32 rfx_context_get_frame_idx(const RFX_CONTEXT* WINPR_RESTRICT context)
{
	WINPR_ASSERT(context);
	return context->frameIdx;
}

UINT32 rfx_message_get_frame_idx(const RFX_MESSAGE* WINPR_RESTRICT message)
{
	WINPR_ASSERT(message);
	return message->frameIdx;
}

static INLINE BOOL rfx_write_progressive_wb_sync(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                 wStream* WINPR_RESTRICT s)
{
	const UINT32 blockLen = 12;
	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_SYNC); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);             /* blockLen (4 bytes) */
	Stream_Write_UINT32(s, 0xCACCACCA);           /* magic (4 bytes) */
	Stream_Write_UINT16(s, 0x0100);               /* version (2 bytes) */
	return TRUE;
}

static INLINE BOOL rfx_write_progressive_wb_context(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                    wStream* WINPR_RESTRICT s)
{
	const UINT32 blockLen = 10;
	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_CONTEXT); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);                /* blockLen (4 bytes) */
	Stream_Write_UINT8(s, 0);                        /* ctxId (1 byte) */
	Stream_Write_UINT16(s, 64);                      /* tileSize (2 bytes) */
	Stream_Write_UINT8(s, 0);                        /* flags (1 byte) */
	return TRUE;
}

static INLINE BOOL rfx_write_progressive_region(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                wStream* WINPR_RESTRICT s,
                                                const RFX_MESSAGE* WINPR_RESTRICT msg)
{
	/* RFX_REGION */
	UINT32 blockLen = 18;
	UINT32 tilesDataSize = 0;
	const size_t start = Stream_GetPosition(s);

	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);
	WINPR_ASSERT(msg);

	blockLen += msg->numRects * 8;
	blockLen += msg->numQuant * 5;
	tilesDataSize = msg->numTiles * 22UL;
	for (UINT16 i = 0; i < msg->numTiles; i++)
	{
		const RFX_TILE* tile = msg->tiles[i];
		WINPR_ASSERT(tile);
		tilesDataSize += tile->YLen + tile->CbLen + tile->CrLen;
	}
	blockLen += tilesDataSize;

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_REGION); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);               /* blockLen (4 bytes) */
	Stream_Write_UINT8(s, 64);                      /* tileSize (1 byte) */
	Stream_Write_UINT16(s, msg->numRects);          /* numRects (2 bytes) */
	WINPR_ASSERT(msg->numQuant <= UINT8_MAX);
	Stream_Write_UINT8(s, (UINT8)msg->numQuant); /* numQuant (1 byte) */
	Stream_Write_UINT8(s, 0);                    /* numProgQuant (1 byte) */
	Stream_Write_UINT8(s, 0);                    /* flags (1 byte) */
	Stream_Write_UINT16(s, msg->numTiles);       /* numTiles (2 bytes) */
	Stream_Write_UINT32(s, tilesDataSize);       /* tilesDataSize (4 bytes) */

	for (UINT16 i = 0; i < msg->numRects; i++)
	{
		/* TS_RFX_RECT */
		const RFX_RECT* r = &msg->rects[i];
		Stream_Write_UINT16(s, r->x);      /* x (2 bytes) */
		Stream_Write_UINT16(s, r->y);      /* y (2 bytes) */
		Stream_Write_UINT16(s, r->width);  /* width (2 bytes) */
		Stream_Write_UINT16(s, r->height); /* height (2 bytes) */
	}

	/**
	 * Note: The RFX_COMPONENT_CODEC_QUANT structure differs from the
	 * TS_RFX_CODEC_QUANT ([MS-RDPRFX] section 2.2.2.1.5) structure with respect
	 * to the order of the bands.
	 *             0    1    2   3     4    5    6    7    8    9
	 * RDPRFX:   LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
	 * RDPEGFX:  LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
	 */
	for (UINT16 i = 0; i < msg->numQuant; i++)
	{
		const UINT32* qv = &msg->quantVals[10ULL * i];
		/* RFX_COMPONENT_CODEC_QUANT */
		Stream_Write_UINT8(s, (UINT8)(qv[0] + (qv[2] << 4))); /* LL3 (4-bit), HL3 (4-bit) */
		Stream_Write_UINT8(s, (UINT8)(qv[1] + (qv[3] << 4))); /* LH3 (4-bit), HH3 (4-bit) */
		Stream_Write_UINT8(s, (UINT8)(qv[5] + (qv[4] << 4))); /* HL2 (4-bit), LH2 (4-bit) */
		Stream_Write_UINT8(s, (UINT8)(qv[6] + (qv[8] << 4))); /* HH2 (4-bit), HL1 (4-bit) */
		Stream_Write_UINT8(s, (UINT8)(qv[7] + (qv[9] << 4))); /* LH1 (4-bit), HH1 (4-bit) */
	}

	for (UINT16 i = 0; i < msg->numTiles; i++)
	{
		const RFX_TILE* tile = msg->tiles[i];
		if (!rfx_write_progressive_tile_simple(rfx, s, tile))
			return FALSE;
	}

	const size_t end = Stream_GetPosition(s);
	const size_t used = end - start;
	return (used == blockLen);
}

static INLINE BOOL rfx_write_progressive_frame_begin(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                     wStream* WINPR_RESTRICT s,
                                                     const RFX_MESSAGE* WINPR_RESTRICT msg)
{
	const UINT32 blockLen = 12;
	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);
	WINPR_ASSERT(msg);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_FRAME_BEGIN); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);                    /* blockLen (4 bytes) */
	Stream_Write_UINT32(s, msg->frameIdx);               /* frameIndex (4 bytes) */
	Stream_Write_UINT16(s, 1);                           /* regionCount (2 bytes) */

	return TRUE;
}

static INLINE BOOL rfx_write_progressive_frame_end(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                   wStream* WINPR_RESTRICT s)
{
	const UINT32 blockLen = 6;
	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_FRAME_END); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);                  /* blockLen (4 bytes) */

	return TRUE;
}

static INLINE BOOL rfx_write_progressive_tile_simple(RFX_CONTEXT* WINPR_RESTRICT rfx,
                                                     wStream* WINPR_RESTRICT s,
                                                     const RFX_TILE* WINPR_RESTRICT tile)
{
	UINT32 blockLen = 0;
	WINPR_ASSERT(rfx);
	WINPR_ASSERT(s);
	WINPR_ASSERT(tile);

	blockLen = 22 + tile->YLen + tile->CbLen + tile->CrLen;
	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, PROGRESSIVE_WBT_TILE_SIMPLE); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen);                    /* blockLen (4 bytes) */
	Stream_Write_UINT8(s, tile->quantIdxY);              /* quantIdxY (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCb);             /* quantIdxCb (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCr);             /* quantIdxCr (1 byte) */
	Stream_Write_UINT16(s, tile->xIdx);                  /* xIdx (2 bytes) */
	Stream_Write_UINT16(s, tile->yIdx);                  /* yIdx (2 bytes) */
	Stream_Write_UINT8(s, 0);                            /* flags (1 byte) */
	Stream_Write_UINT16(s, tile->YLen);                  /* YLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CbLen);                 /* CbLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CrLen);                 /* CrLen (2 bytes) */
	Stream_Write_UINT16(s, 0);                           /* tailLen (2 bytes) */
	Stream_Write(s, tile->YData, tile->YLen);            /* YData */
	Stream_Write(s, tile->CbData, tile->CbLen);          /* CbData */
	Stream_Write(s, tile->CrData, tile->CrLen);          /* CrData */

	return TRUE;
}

const char* rfx_get_progressive_block_type_string(UINT16 blockType)
{
	switch (blockType)
	{
		case PROGRESSIVE_WBT_SYNC:
			return "PROGRESSIVE_WBT_SYNC";

		case PROGRESSIVE_WBT_FRAME_BEGIN:
			return "PROGRESSIVE_WBT_FRAME_BEGIN";

		case PROGRESSIVE_WBT_FRAME_END:
			return "PROGRESSIVE_WBT_FRAME_END";

		case PROGRESSIVE_WBT_CONTEXT:
			return "PROGRESSIVE_WBT_CONTEXT";

		case PROGRESSIVE_WBT_REGION:
			return "PROGRESSIVE_WBT_REGION";

		case PROGRESSIVE_WBT_TILE_SIMPLE:
			return "PROGRESSIVE_WBT_TILE_SIMPLE";

		case PROGRESSIVE_WBT_TILE_FIRST:
			return "PROGRESSIVE_WBT_TILE_FIRST";

		case PROGRESSIVE_WBT_TILE_UPGRADE:
			return "PROGRESSIVE_WBT_TILE_UPGRADE";

		default:
			return "PROGRESSIVE_WBT_UNKNOWN";
	}
}

BOOL rfx_write_message_progressive_simple(RFX_CONTEXT* WINPR_RESTRICT context,
                                          wStream* WINPR_RESTRICT s,
                                          const RFX_MESSAGE* WINPR_RESTRICT msg)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(msg);
	WINPR_ASSERT(context);

	if (context->mode != RLGR1)
	{
		WLog_ERR(TAG, "error, RLGR1 mode is required!");
		return FALSE;
	}

	if (!rfx_write_progressive_wb_sync(context, s))
		return FALSE;

	if (!rfx_write_progressive_wb_context(context, s))
		return FALSE;

	if (!rfx_write_progressive_frame_begin(context, s, msg))
		return FALSE;

	if (!rfx_write_progressive_region(context, s, msg))
		return FALSE;

	if (!rfx_write_progressive_frame_end(context, s))
		return FALSE;

	return TRUE;
}
