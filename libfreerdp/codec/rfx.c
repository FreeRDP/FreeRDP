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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>
#include <winpr/tchar.h>

#include <freerdp/log.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/region.h>
#include <freerdp/build-config.h>
#include <freerdp/codec/region.h>

#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_decode.h"
#include "rfx_encode.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"
#include "rfx_rlgr.h"

#include "rfx_sse2.h"
#include "rfx_neon.h"

#define TAG FREERDP_TAG("codec")

#ifndef RFX_INIT_SIMD
#define RFX_INIT_SIMD(_rfx_context) do { } while (0)
#endif

#define RFX_KEY "Software\\"FREERDP_VENDOR_STRING"\\" \
	FREERDP_PRODUCT_STRING"\\RemoteFX"

/**
 * The quantization values control the compression rate and quality. The value
 * range is between 6 and 15. The higher value, the higher compression rate
 * and lower quality.
 *
 * This is the default values being use by the MS RDP server, and we will also
 * use it as our default values for the encoder. It can be overrided by setting
 * the context->num_quants and context->quants member.
 *
 * The order of the values are:
 * LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
 */
static const UINT32 rfx_default_quantization_values[] =
{
	6, 6, 6, 6, 7, 7, 8, 8, 8, 9
};

static void rfx_profiler_create(RFX_CONTEXT* context)
{
	PROFILER_CREATE(context->priv->prof_rfx_decode_rgb, "rfx_decode_rgb")
	PROFILER_CREATE(context->priv->prof_rfx_decode_component,
	                "rfx_decode_component")
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_decode, "rfx_rlgr_decode")
	PROFILER_CREATE(context->priv->prof_rfx_differential_decode,
	                "rfx_differential_decode")
	PROFILER_CREATE(context->priv->prof_rfx_quantization_decode,
	                "rfx_quantization_decode")
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_decode, "rfx_dwt_2d_decode")
	PROFILER_CREATE(context->priv->prof_rfx_ycbcr_to_rgb, "prims->yCbCrToRGB")
	PROFILER_CREATE(context->priv->prof_rfx_encode_rgb, "rfx_encode_rgb")
	PROFILER_CREATE(context->priv->prof_rfx_encode_component,
	                "rfx_encode_component")
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_encode, "rfx_rlgr_encode")
	PROFILER_CREATE(context->priv->prof_rfx_differential_encode,
	                "rfx_differential_encode")
	PROFILER_CREATE(context->priv->prof_rfx_quantization_encode,
	                "rfx_quantization_encode")
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_encode, "rfx_dwt_2d_encode")
	PROFILER_CREATE(context->priv->prof_rfx_rgb_to_ycbcr, "prims->RGBToYCbCr")
	PROFILER_CREATE(context->priv->prof_rfx_encode_format_rgb,
	                "rfx_encode_format_rgb")
}

static void rfx_profiler_free(RFX_CONTEXT* context)
{
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

static void rfx_profiler_print(RFX_CONTEXT* context)
{
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

static void rfx_tile_init(RFX_TILE* tile)
{
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

static RFX_TILE* rfx_decoder_tile_new(void)
{
	RFX_TILE* tile = NULL;

	if (!(tile = (RFX_TILE*) calloc(1, sizeof(RFX_TILE))))
		return NULL;

	if (!(tile->data = (BYTE*) _aligned_malloc(4 * 64 * 64, 16)))
	{
		free(tile);
		return NULL;
	}

	tile->allocated = TRUE;
	return tile;
}

static void rfx_decoder_tile_free(RFX_TILE* tile)
{
	if (tile)
	{
		if (tile->allocated)
			_aligned_free(tile->data);

		free(tile);
	}
}

static RFX_TILE* rfx_encoder_tile_new(void)
{
	return (RFX_TILE*)calloc(1, sizeof(RFX_TILE));
}

static void rfx_encoder_tile_free(RFX_TILE* tile)
{
	free(tile);
}

RFX_CONTEXT* rfx_context_new(BOOL encoder)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	SYSTEM_INFO sysinfo;
	RFX_CONTEXT* context;
	wObject* pool;
	RFX_CONTEXT_PRIV* priv;
	context = (RFX_CONTEXT*)calloc(1, sizeof(RFX_CONTEXT));

	if (!context)
		return NULL;

	context->encoder = encoder;
	context->priv = priv = (RFX_CONTEXT_PRIV*)calloc(1, sizeof(RFX_CONTEXT_PRIV));

	if (!priv)
		goto error_priv;

	WLog_Init();
	priv->log = WLog_Get("com.freerdp.codec.rfx");
	WLog_OpenAppender(priv->log);
#ifdef WITH_DEBUG_RFX
	WLog_SetLogLevel(priv->log, WLOG_DEBUG);
#endif
	priv->TilePool = ObjectPool_New(TRUE);

	if (!priv->TilePool)
		goto error_tilePool;

	pool = ObjectPool_Object(priv->TilePool);
	pool->fnObjectInit = (OBJECT_INIT_FN) rfx_tile_init;

	if (context->encoder)
	{
		pool->fnObjectNew = (OBJECT_NEW_FN) rfx_encoder_tile_new;
		pool->fnObjectFree = (OBJECT_FREE_FN) rfx_encoder_tile_free;
	}
	else
	{
		pool->fnObjectNew = (OBJECT_NEW_FN) rfx_decoder_tile_new;
		pool->fnObjectFree = (OBJECT_FREE_FN) rfx_decoder_tile_free;
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
	 * We then multiply by 3 to use a single, partioned buffer for all 3 channels.
	 */
	priv->BufferPool = BufferPool_New(TRUE, (8192 + 32) * 3, 16);

	if (!priv->BufferPool)
		goto error_BufferPool;

#ifdef _WIN32
	{
		BOOL isVistaOrLater;
		OSVERSIONINFOA verinfo;
		ZeroMemory(&verinfo, sizeof(OSVERSIONINFOA));
		verinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		GetVersionExA(&verinfo);
		isVistaOrLater = ((verinfo.dwMajorVersion >= 6)
		                  && (verinfo.dwMinorVersion >= 0)) ? TRUE : FALSE;
		priv->UseThreads = isVistaOrLater;
	}
#else
	priv->UseThreads = TRUE;
#endif
	GetNativeSystemInfo(&sysinfo);
	priv->MinThreadCount = sysinfo.dwNumberOfProcessors;
	priv->MaxThreadCount = 0;
	status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, RFX_KEY, 0,
	                       KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		dwSize = sizeof(dwValue);

		if (RegQueryValueEx(hKey, _T("UseThreads"), NULL, &dwType, (BYTE*) &dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			priv->UseThreads = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("MinThreadCount"), NULL, &dwType, (BYTE*) &dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			priv->MinThreadCount = dwValue;

		if (RegQueryValueEx(hKey, _T("MaxThreadCount"), NULL, &dwType, (BYTE*) &dwValue,
		                    &dwSize) == ERROR_SUCCESS)
			priv->MaxThreadCount = dwValue;

		RegCloseKey(hKey);
	}

	if (priv->UseThreads)
	{
		/* Call primitives_get here in order to avoid race conditions when using primitives_get */
		/* from multiple threads. This call will initialize all function pointers correctly     */
		/* before any decoding threads are started */
		primitives_get();
		priv->ThreadPool = CreateThreadpool(NULL);

		if (!priv->ThreadPool)
			goto error_threadPool;

		InitializeThreadpoolEnvironment(&priv->ThreadPoolEnv);
		SetThreadpoolCallbackPool(&priv->ThreadPoolEnv, priv->ThreadPool);

		if (priv->MinThreadCount)
			if (!SetThreadpoolThreadMinimum(priv->ThreadPool, priv->MinThreadCount))
				goto error_threadPool_minimum;

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
	context->dwt_2d_encode = rfx_dwt_2d_encode;
	context->rlgr_decode = rfx_rlgr_decode;
	context->rlgr_encode = rfx_rlgr_encode;
	RFX_INIT_SIMD(context);
	context->state = RFX_STATE_SEND_HEADERS;
	return context;
error_threadPool_minimum:
	CloseThreadpool(priv->ThreadPool);
error_threadPool:
	BufferPool_Free(priv->BufferPool);
error_BufferPool:
	ObjectPool_Free(priv->TilePool);
error_tilePool:
	free(priv);
error_priv:
	free(context);
	return NULL;
}

void rfx_context_free(RFX_CONTEXT* context)
{
	RFX_CONTEXT_PRIV* priv;

	if (!context)
		return;

	assert(NULL != context);
	assert(NULL != context->priv);
	assert(NULL != context->priv->TilePool);
	assert(NULL != context->priv->BufferPool);
	priv = context->priv;
	free(context->quants);
	ObjectPool_Free(priv->TilePool);
	rfx_profiler_print(context);
	rfx_profiler_free(context);

	if (priv->UseThreads)
	{
		CloseThreadpool(context->priv->ThreadPool);
		DestroyThreadpoolEnvironment(&context->priv->ThreadPoolEnv);
		free(priv->workObjects);
		free(priv->tileWorkParams);
#ifdef WITH_PROFILER
		WLog_VRB(TAG,
		         "WARNING: Profiling results probably unusable with multithreaded RemoteFX codec!");
#endif
	}

	BufferPool_Free(context->priv->BufferPool);
	free(context->priv);
	free(context);
}

static RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, UINT32 index)
{
	return message->tiles[index];
}

static RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, UINT32 index)
{
	return &message->rects[index];
}

void rfx_context_set_pixel_format(RFX_CONTEXT* context, UINT32 pixel_format)
{
	context->pixel_format = pixel_format;
	context->bits_per_pixel = GetBitsPerPixel(pixel_format);
}

BOOL rfx_context_reset(RFX_CONTEXT* context, UINT32 width, UINT32 height)
{
	if (!context)
		return FALSE;

	context->width = width;
	context->height = height;
	context->state = RFX_STATE_SEND_HEADERS;
	context->frameIdx = 0;
	return TRUE;
}

static BOOL rfx_process_message_sync(RFX_CONTEXT* context, wStream* s)
{
	UINT32 magic;
	context->decodedHeaderBlocks &= ~_RFX_DECODED_SYNC;

	/* RFX_SYNC */
	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "RfxSync packet too small");
		return FALSE;
	}

	Stream_Read_UINT32(s, magic); /* magic (4 bytes), 0xCACCACCA */

	if (magic != WF_MAGIC)
	{
		WLog_ERR(TAG, "invalid magic number 0x%08"PRIX32"", magic);
		return FALSE;
	}

	Stream_Read_UINT16(s,
	                   context->version); /* version (2 bytes), WF_VERSION_1_0 (0x0100) */

	if (context->version != WF_VERSION_1_0)
	{
		WLog_ERR(TAG, "invalid version number 0x%08"PRIX32"", context->version);
		return FALSE;
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "version 0x%08"PRIX32"", context->version);
	context->decodedHeaderBlocks |= _RFX_DECODED_SYNC;
	return TRUE;
}

static BOOL rfx_process_message_codec_versions(RFX_CONTEXT* context, wStream* s)
{
	BYTE numCodecs;
	context->decodedHeaderBlocks &= ~_RFX_DECODED_VERSIONS;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "%s: packet too small for reading codec versions", __FUNCTION__);
		return FALSE;
	}

	Stream_Read_UINT8(s, numCodecs); /* numCodecs (1 byte), must be set to 0x01 */
	Stream_Read_UINT8(s,
	                  context->codec_id); /* codecId (1 byte), must be set to 0x01 */
	Stream_Read_UINT16(s,
	                   context->codec_version); /* version (2 bytes), must be set to WF_VERSION_1_0 (0x0100)  */

	if (numCodecs != 1)
	{
		WLog_ERR(TAG, "%s: numCodes is 0x%02"PRIX8" (must be 0x01)", __FUNCTION__, numCodecs);
		return FALSE;
	}

	if (context->codec_id != 0x01)
	{
		WLog_ERR(TAG, "%s: invalid codec id (0x%02"PRIX32")", __FUNCTION__, context->codec_id);
		return FALSE;
	}

	if (context->codec_version != WF_VERSION_1_0)
	{
		WLog_ERR(TAG, "%s: invalid codec version (0x%08"PRIX32")", __FUNCTION__,
		         context->codec_version);
		return FALSE;
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "id %"PRIu32" version 0x%"PRIX32".",
	           context->codec_id, context->codec_version);
	context->decodedHeaderBlocks |= _RFX_DECODED_VERSIONS;
	return TRUE;
}

static BOOL rfx_process_message_channels(RFX_CONTEXT* context, wStream* s)
{
	BYTE channelId;
	BYTE numChannels;
	context->decodedHeaderBlocks &= ~_RFX_DECODED_CHANNELS;

	if (Stream_GetRemainingLength(s) < 1)
	{
		WLog_ERR(TAG, "RfxMessageChannels packet too small");
		return FALSE;
	}

	Stream_Read_UINT8(s,
	                  numChannels); /* numChannels (1 byte), must bet set to 0x01 */

	/* In RDVH sessions, numChannels will represent the number of virtual monitors
	 * configured and does not always be set to 0x01 as [MS-RDPRFX] said.
	 */
	if (numChannels < 1)
	{
		WLog_ERR(TAG, "no channels announced");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < (size_t)(numChannels * 5))
	{
		WLog_ERR(TAG, "RfxMessageChannels packet too small for numChannels=%"PRIu8"",
		         numChannels);
		return FALSE;
	}

	/* RFX_CHANNELT */
	Stream_Read_UINT8(s, channelId); /* channelId (1 byte), must be set to 0x00 */

	if (channelId != 0x00)
	{
		WLog_ERR(TAG, "channelId:0x%02"PRIX8", expected:0x00", channelId);
		return FALSE;
	}

	Stream_Read_UINT16(s, context->width); /* width (2 bytes) */
	Stream_Read_UINT16(s, context->height); /* height (2 bytes) */

	if (!context->width || !context->height)
	{
		WLog_ERR(TAG, "%s: invalid channel with/height: %"PRIu16"x%"PRIu16"", __FUNCTION__,
		         context->width, context->height);
		return FALSE;
	}

	/* Now, only the first monitor can be used, therefore the other channels will be ignored. */
	Stream_Seek(s, 5 * (numChannels - 1));
	WLog_Print(context->priv->log, WLOG_DEBUG, "numChannels %"PRIu8" id %"PRIu8", %"PRIu16"x%"PRIu16".",
	           numChannels, channelId, context->width, context->height);
	context->decodedHeaderBlocks |= _RFX_DECODED_CHANNELS;
	return TRUE;
}

static BOOL rfx_process_message_context(RFX_CONTEXT* context, wStream* s)
{
	BYTE ctxId;
	UINT16 tileSize;
	UINT16 properties;
	context->decodedHeaderBlocks &= ~_RFX_DECODED_CONTEXT;

	if (Stream_GetRemainingLength(s) < 5)
	{
		WLog_ERR(TAG, "RfxMessageContext packet too small");
		return FALSE;
	}

	Stream_Read_UINT8(s, ctxId); /* ctxId (1 byte), must be set to 0x00 */
	Stream_Read_UINT16(s,
	                   tileSize); /* tileSize (2 bytes), must be set to CT_TILE_64x64 (0x0040) */
	Stream_Read_UINT16(s, properties); /* properties (2 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "ctxId %"PRIu8" tileSize %"PRIu16" properties 0x%04"PRIX16".",
	           ctxId, tileSize, properties);
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
			WLog_ERR(TAG, "unknown RLGR algorithm.");
			return FALSE;
	}

	context->decodedHeaderBlocks |= _RFX_DECODED_CONTEXT;
	return TRUE;
}

static BOOL rfx_process_message_frame_begin(RFX_CONTEXT* context,
        RFX_MESSAGE* message, wStream* s, UINT16* pExpectedBlockType)
{
	UINT32 frameIdx;
	UINT16 numRegions;

	if (*pExpectedBlockType != WBT_FRAME_BEGIN)
	{
		WLog_ERR(TAG, "%s: message unexpeced", __FUNCTION__);
		return FALSE;
	}

	*pExpectedBlockType = WBT_REGION;

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "RfxMessageFrameBegin packet too small");
		return FALSE;
	}

	Stream_Read_UINT32(s,
	                   frameIdx); /* frameIdx (4 bytes), if codec is in video mode, must be ignored */
	Stream_Read_UINT16(s, numRegions); /* numRegions (2 bytes) */
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "RFX_FRAME_BEGIN: frameIdx: %"PRIu32" numRegions: %"PRIu16"", frameIdx, numRegions);
	return TRUE;
}

static BOOL rfx_process_message_frame_end(RFX_CONTEXT* context,
        RFX_MESSAGE* message, wStream* s, UINT16* pExpectedBlockType)
{
	if (*pExpectedBlockType != WBT_FRAME_END)
	{
		WLog_ERR(TAG, "%s: message unexpeced", __FUNCTION__);
		return FALSE;
	}

	*pExpectedBlockType = WBT_FRAME_BEGIN;
	WLog_Print(context->priv->log, WLOG_DEBUG, "RFX_FRAME_END");
	return TRUE;
}

static BOOL rfx_process_message_region(RFX_CONTEXT* context,
                                       RFX_MESSAGE* message, wStream* s, UINT16* pExpectedBlockType)
{
	int i;
	UINT16 regionType;
	UINT16 numTileSets;

	if (*pExpectedBlockType != WBT_REGION)
	{
		WLog_ERR(TAG, "%s: message unexpeced", __FUNCTION__);
		return FALSE;
	}

	*pExpectedBlockType = WBT_EXTENSION;

	if (Stream_GetRemainingLength(s) < 3)
	{
		WLog_ERR(TAG, "%s: packet too small (regionFlags/numRects)", __FUNCTION__);
		return FALSE;
	}

	Stream_Seek_UINT8(s); /* regionFlags (1 byte) */
	Stream_Read_UINT16(s, message->numRects); /* numRects (2 bytes) */

	if (message->numRects < 1)
	{
		/*
		   If numRects is zero the decoder must generate a rectangle with
		   coordinates (0, 0, width, height).
		   See [MS-RDPRFX] (revision >= 17.0) 2.2.2.3.3 TS_RFX_REGION
		   https://msdn.microsoft.com/en-us/library/ff635233.aspx
		*/
		if (!(message->rects = (RFX_RECT*) malloc(sizeof(RFX_RECT))))
			return FALSE;

		message->numRects = 1;
		message->rects->x = 0;
		message->rects->y = 0;
		message->rects->width = context->width;
		message->rects->height = context->height;
		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < (size_t)(8 * message->numRects))
	{
		WLog_ERR(TAG, "%s: packet too small for num_rects=%"PRIu16"", __FUNCTION__,
		         message->numRects);
		return FALSE;
	}

	if (!(message->rects = (RFX_RECT*) calloc(message->numRects, sizeof(RFX_RECT))))
		return FALSE;

	/* rects */
	for (i = 0; i < message->numRects; i++)
	{
		RFX_RECT* rect = rfx_message_get_rect(message, i);
		/* RFX_RECT */
		Stream_Read_UINT16(s, rect->x); /* x (2 bytes) */
		Stream_Read_UINT16(s, rect->y); /* y (2 bytes) */
		Stream_Read_UINT16(s, rect->width); /* width (2 bytes) */
		Stream_Read_UINT16(s, rect->height); /* height (2 bytes) */
		WLog_Print(context->priv->log, WLOG_DEBUG,
		           "rect %d (x,y=%"PRIu16",%"PRIu16" w,h=%"PRIu16" %"PRIu16").", i,
		           rect->x, rect->y,
		           rect->width, rect->height);
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "%s: packet too small (regionType/numTileSets)", __FUNCTION__);
		return FALSE;
	}

	Stream_Read_UINT16(s,
	                   regionType); /* regionType (2 bytes): MUST be set to CBT_REGION (0xCAC1) */
	Stream_Read_UINT16(s,
	                   numTileSets); /* numTilesets (2 bytes): MUST be set to 0x0001. */

	if (regionType != CBT_REGION)
	{
		WLog_ERR(TAG, "%s: invalid region type 0x%04"PRIX16"", __FUNCTION__, regionType);
		return TRUE;
	}

	if (numTileSets != 0x0001)
	{
		WLog_ERR(TAG, "%s: invalid number of tilesets (%"PRIu16")", __FUNCTION__, numTileSets);
		return FALSE;
	}

	return TRUE;
}

struct _RFX_TILE_PROCESS_WORK_PARAM
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};
typedef struct _RFX_TILE_PROCESS_WORK_PARAM RFX_TILE_PROCESS_WORK_PARAM;

static void CALLBACK rfx_process_message_tile_work_callback(
    PTP_CALLBACK_INSTANCE
    instance, void* context, PTP_WORK work)
{
	RFX_TILE_PROCESS_WORK_PARAM* param = (RFX_TILE_PROCESS_WORK_PARAM*) context;
	rfx_decode_rgb(param->context, param->tile, param->tile->data, 64 * 4);
}

static BOOL rfx_process_message_tileset(RFX_CONTEXT* context,
                                        RFX_MESSAGE* message, wStream* s, UINT16* pExpecedBlockType)
{
	BOOL rc;
	int i, close_cnt;
	size_t pos;
	BYTE quant;
	RFX_TILE* tile;
	UINT32* quants;
	UINT16 subtype;
	UINT32 blockLen;
	UINT32 blockType;
	UINT32 tilesDataSize;
	PTP_WORK* work_objects = NULL;
	RFX_TILE_PROCESS_WORK_PARAM* params = NULL;
	void* pmem;

	if (*pExpecedBlockType != WBT_EXTENSION)
	{
		WLog_ERR(TAG, "%s: message unexpeced", __FUNCTION__);
		return FALSE;
	}

	*pExpecedBlockType = WBT_FRAME_END;

	if (Stream_GetRemainingLength(s) < 14)
	{
		WLog_ERR(TAG, "RfxMessageTileSet packet too small");
		return FALSE;
	}

	Stream_Read_UINT16(s,
	                   subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */

	if (subtype != CBT_TILESET)
	{
		WLog_ERR(TAG, "invalid subtype, expected CBT_TILESET.");
		return FALSE;
	}

	Stream_Seek_UINT16(s); /* idx (2 bytes), must be set to 0x0000 */
	Stream_Seek_UINT16(s); /* properties (2 bytes) */
	Stream_Read_UINT8(s, context->numQuant); /* numQuant (1 byte) */
	Stream_Seek_UINT8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (context->numQuant < 1)
	{
		WLog_ERR(TAG, "no quantization value.");
		return FALSE;
	}

	Stream_Read_UINT16(s, message->numTiles); /* numTiles (2 bytes) */

	if (message->numTiles < 1)
	{
		/* Windows Server 2012 (not R2) can send empty tile sets */
		return TRUE;
	}

	Stream_Read_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	if (!(pmem = realloc((void*) context->quants,
	                     context->numQuant * 10 * sizeof(UINT32))))
		return FALSE;

	quants = context->quants = (UINT32*) pmem;

	/* quantVals */
	if (Stream_GetRemainingLength(s) < (size_t)(context->numQuant * 5))
	{
		WLog_ERR(TAG, "RfxMessageTileSet packet too small for num_quants=%"PRIu8"",
		         context->numQuant);
		return FALSE;
	}

	for (i = 0; i < context->numQuant; i++)
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
		           "quant %d (%"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32").",
		           i, context->quants[i * 10], context->quants[i * 10 + 1],
		           context->quants[i * 10 + 2], context->quants[i * 10 + 3],
		           context->quants[i * 10 + 4], context->quants[i * 10 + 5],
		           context->quants[i * 10 + 6], context->quants[i * 10 + 7],
		           context->quants[i * 10 + 8], context->quants[i * 10 + 9]);
	}

	if (!(message->tiles = (RFX_TILE**) calloc(message->numTiles,
	                       sizeof(RFX_TILE*))))
	{
		message->numTiles = 0;
		return FALSE;
	}

	if (context->priv->UseThreads)
	{
		work_objects = (PTP_WORK*) calloc(message->numTiles, sizeof(PTP_WORK));
		params = (RFX_TILE_PROCESS_WORK_PARAM*) calloc(message->numTiles,
		         sizeof(RFX_TILE_PROCESS_WORK_PARAM));

		if (!work_objects)
		{
			free(params);
			return FALSE;
		}

		if (!params)
		{
			free(work_objects);
			return FALSE;
		}
	}

	/* tiles */
	close_cnt = 0;
	rc = TRUE;

	for (i = 0; i < message->numTiles; i++)
	{
		if (!(tile = (RFX_TILE*) ObjectPool_Take(context->priv->TilePool)))
		{
			WLog_ERR(TAG, "RfxMessageTileSet failed to get tile from object pool");
			rc = FALSE;
			break;
		}

		message->tiles[i] = tile;

		/* RFX_TILE */
		if (Stream_GetRemainingLength(s) < 6)
		{
			WLog_ERR(TAG, "RfxMessageTileSet packet too small to read tile %d/%"PRIu16"", i,
			         message->numTiles);
			rc = FALSE;
			break;
		}

		Stream_Read_UINT16(s,
		                   blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */

		if (Stream_GetRemainingLength(s) < blockLen - 6)
		{
			WLog_ERR(TAG,
			         "RfxMessageTileSet not enough bytes to read tile %d/%"PRIu16" with blocklen=%"PRIu32"",
			         i, message->numTiles, blockLen);
			rc = FALSE;
			break;
		}

		pos = Stream_GetPosition(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			WLog_ERR(TAG, "unknown block type 0x%"PRIX32", expected CBT_TILE (0xCAC3).",
			         blockType);
			rc = FALSE;
			break;
		}

		Stream_Read_UINT8(s, tile->quantIdxY); /* quantIdxY (1 byte) */
		Stream_Read_UINT8(s, tile->quantIdxCb); /* quantIdxCb (1 byte) */
		Stream_Read_UINT8(s, tile->quantIdxCr); /* quantIdxCr (1 byte) */
		Stream_Read_UINT16(s, tile->xIdx); /* xIdx (2 bytes) */
		Stream_Read_UINT16(s, tile->yIdx); /* yIdx (2 bytes) */
		Stream_Read_UINT16(s, tile->YLen); /* YLen (2 bytes) */
		Stream_Read_UINT16(s, tile->CbLen); /* CbLen (2 bytes) */
		Stream_Read_UINT16(s, tile->CrLen); /* CrLen (2 bytes) */
		Stream_GetPointer(s, tile->YData);
		Stream_Seek(s, tile->YLen);
		Stream_GetPointer(s, tile->CbData);
		Stream_Seek(s, tile->CbLen);
		Stream_GetPointer(s, tile->CrData);
		Stream_Seek(s, tile->CrLen);
		tile->x = tile->xIdx * 64;
		tile->y = tile->yIdx * 64;

		if (context->priv->UseThreads)
		{
			assert(params);
			params[i].context = context;
			params[i].tile = message->tiles[i];

			if (!(work_objects[i] = CreateThreadpoolWork(
			                        rfx_process_message_tile_work_callback,
			                        (void*) &params[i], &context->priv->ThreadPoolEnv)))
			{
				WLog_ERR(TAG, "CreateThreadpoolWork failed.");
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

		Stream_SetPosition(s, pos);
	}

	if (context->priv->UseThreads)
	{
		for (i = 0; i < close_cnt; i++)
		{
			WaitForThreadpoolWorkCallbacks(work_objects[i], FALSE);
			CloseThreadpoolWork(work_objects[i]);
		}
	}

	free(work_objects);
	free(params);

	for (i = 0; i < message->numTiles; i++)
	{
		if (!(tile = message->tiles[i]))
			continue;

		tile->YLen = tile->CbLen = tile->CrLen = 0;
		tile->YData = tile->CbData = tile->CrData = NULL;
	}

	return rc;
}

BOOL rfx_process_message(RFX_CONTEXT* context, const BYTE* data, UINT32 length,
                         UINT32 left, UINT32 top,
                         BYTE* dst, UINT32 dstFormat,
                         UINT32 dstStride, UINT32 dstHeight,
                         REGION16* invalidRegion)
{
	size_t pos;
	REGION16 updateRegion;
	UINT32 blockLen;
	UINT32 blockType;
	RFX_MESSAGE* message = NULL;
	wStream* s = NULL;
	BOOL ok = TRUE;
	UINT16 expectedDataBlockType = WBT_FRAME_BEGIN;

	if (!context || !data || !length)
		goto fail;

	if (!(s = Stream_New((BYTE*)data, length)))
		goto fail;

	if (!(message = (RFX_MESSAGE*) calloc(1, sizeof(RFX_MESSAGE))))
		goto fail;

	message->freeRects = TRUE;

	while (ok && Stream_GetRemainingLength(s) > 6)
	{
		/* RFX_BLOCKT */
		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */
		WLog_Print(context->priv->log, WLOG_DEBUG, "blockType 0x%"PRIX32" blockLen %"PRIu32"",
		           blockType, blockLen);

		if (blockLen == 0)
		{
			WLog_ERR(TAG, "zero blockLen");
			goto fail;
		}

		if (Stream_GetRemainingLength(s) < blockLen - 6)
		{
			WLog_ERR(TAG, "%s: packet too small for blocklen=%"PRIu32"", __FUNCTION__, blockLen);
			goto fail;
		}

		pos = Stream_GetPosition(s) - 6 + blockLen;

		if (blockType > WBT_CONTEXT
		    && context->decodedHeaderBlocks != _RFX_DECODED_HEADERS)
		{
			WLog_ERR(TAG, "%s: incomplete header blocks processing", __FUNCTION__);
			goto fail;
		}

		if (blockType >= WBT_CONTEXT && blockType <= WBT_EXTENSION)
		{
			/* RFX_CODEC_CHANNELT */
			UINT8 codecId;
			UINT8 channelId;

			if (Stream_GetRemainingLength(s) < 2)
				goto fail;

			Stream_Read_UINT8(s, codecId); /* codecId (1 byte) must be set to 0x01 */
			Stream_Read_UINT8(s,
			                  channelId); /* channelId (1 byte) 0xFF or 0x00, see below */

			if (codecId != 0x01)
			{
				WLog_ERR(TAG, "%s: invalid codecId 0x%02"PRIX8"", __FUNCTION__, codecId);
				goto fail;
			}

			if (blockType == WBT_CONTEXT)
			{
				/* If the blockType is set to WBT_CONTEXT, then channelId MUST be set to 0xFF.*/
				if (channelId != 0xFF)
				{
					WLog_ERR(TAG, "%s: invalid channelId 0x%02"PRIX8" for blockType 0x%08"PRIX32"", __FUNCTION__,
					         channelId, blockType);
					goto fail;
				}
			}
			else
			{
				/* For all other values of blockType, channelId MUST be set to 0x00. */
				if (channelId != 0x00)
				{
					WLog_ERR(TAG, "%s: invalid channelId 0x%02"PRIX8" for blockType WBT_CONTEXT",
					         __FUNCTION__, channelId);
					goto fail;
				}
			}
		}

		switch (blockType)
		{
			/* Header messages:
			 * The stream MUST start with the header messages and any of these headers can appear
			 * in the stream at a later stage. The header messages can be repeated.
			 */
			case WBT_SYNC:
				ok = rfx_process_message_sync(context, s);
				break;

			case WBT_CONTEXT:
				ok = rfx_process_message_context(context, s);
				break;

			case WBT_CODEC_VERSIONS:
				ok = rfx_process_message_codec_versions(context, s);
				break;

			case WBT_CHANNELS:
				ok = rfx_process_message_channels(context, s);
				break;

			/* Data messages:
			 * The data associated with each encoded frame or image is always bracketed by the
			 * TS_RFX_FRAME_BEGIN (section 2.2.2.3.1) and TS_RFX_FRAME_END (section 2.2.2.3.2) messages.
			 * There MUST only be one TS_RFX_REGION (section 2.2.2.3.3) message per frame and one TS_RFX_TILESET
			 * (section 2.2.2.3.4) message per TS_RFX_REGION.
			 */

			case WBT_FRAME_BEGIN:
				ok = rfx_process_message_frame_begin(context, message, s,
				                                     &expectedDataBlockType);
				break;

			case WBT_REGION:
				ok = rfx_process_message_region(context, message, s, &expectedDataBlockType);
				break;

			case WBT_EXTENSION:
				ok = rfx_process_message_tileset(context, message, s, &expectedDataBlockType);
				break;

			case WBT_FRAME_END:
				ok = rfx_process_message_frame_end(context, message, s, &expectedDataBlockType);
				break;

			default:
				WLog_ERR(TAG, "%s: unknown blockType 0x%"PRIX32"", __FUNCTION__, blockType);
				goto fail;
		}

		Stream_SetPosition(s, pos);
	}

	if (ok)
	{
		UINT32 i, j;
		UINT32 nbUpdateRects;
		REGION16 clippingRects;
		const RECTANGLE_16* updateRects;
		const DWORD formatSize = GetBytesPerPixel(context->pixel_format);
		const UINT32 dstWidth = dstStride / GetBytesPerPixel(dstFormat);
		region16_init(&clippingRects);

		for (i = 0; i < message->numRects; i++)
		{
			RECTANGLE_16 clippingRect;
			const RFX_RECT* rect = &(message->rects[i]);
			clippingRect.left = MIN(left + rect->x, dstWidth);
			clippingRect.top = MIN(top + rect->y, dstHeight);
			clippingRect.right = MIN(clippingRect.left + rect->width, dstWidth);
			clippingRect.bottom = MIN(clippingRect.top + rect->height, dstHeight);
			region16_union_rect(&clippingRects, &clippingRects, &clippingRect);
		}

		for (i = 0; i < message->numTiles; i++)
		{
			RECTANGLE_16 updateRect;
			const RFX_TILE* tile = rfx_message_get_tile(message, i);
			updateRect.left = left + tile->x;
			updateRect.top = top + tile->y;
			updateRect.right = updateRect.left + 64;
			updateRect.bottom = updateRect.top + 64;
			region16_init(&updateRegion);
			region16_intersect_rect(&updateRegion, &clippingRects, &updateRect);
			updateRects = region16_rects(&updateRegion, &nbUpdateRects);

			for (j = 0; j < nbUpdateRects; j++)
			{
				const UINT32 stride = 64 * formatSize;
				const UINT32 nXDst = updateRects[j].left;
				const UINT32 nYDst = updateRects[j].top;
				const UINT32 nXSrc = nXDst - updateRect.left;
				const UINT32 nYSrc = nYDst - updateRect.top;
				const UINT32 nWidth = updateRects[j].right - updateRects[j].left;
				const UINT32 nHeight = updateRects[j].bottom - updateRects[j].top;

				if (!freerdp_image_copy(dst, dstFormat, dstStride,
				                        nXDst, nYDst, nWidth, nHeight,
				                        tile->data, context->pixel_format, stride, nXSrc, nYSrc, NULL, FREERDP_FLIP_NONE))
				{
					region16_uninit(&updateRegion);
					goto fail;
				}

				if (invalidRegion)
					region16_union_rect(invalidRegion, invalidRegion, &updateRects[j]);
			}

			region16_uninit(&updateRegion);
		}

		region16_uninit(&clippingRects);
		Stream_Free(s, FALSE);
		rfx_message_free(context, message);
		return TRUE;
	}

fail:
	Stream_Free(s, FALSE);
	rfx_message_free(context, message);
	return FALSE;
}

UINT16 rfx_message_get_tile_count(RFX_MESSAGE* message)
{
	return message->numTiles;
}

UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message)
{
	return message->numRects;
}

void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message)
{
	int i;
	RFX_TILE* tile;

	if (message)
	{
		if ((message->rects) && (message->freeRects))
		{
			free(message->rects);
		}

		if (message->tiles)
		{
			for (i = 0; i < message->numTiles; i++)
			{
				if (!(tile = message->tiles[i]))
					continue;

				if (tile->YCbCrData)
				{
					BufferPool_Return(context->priv->BufferPool, tile->YCbCrData);
					tile->YCbCrData = NULL;
				}

				ObjectPool_Return(context->priv->TilePool, (void*) tile);
			}

			free(message->tiles);
		}

		if (!message->freeArray)
			free(message);
	}
}

static void rfx_update_context_properties(RFX_CONTEXT* context)
{
	UINT16 properties;
	/* properties in tilesets: note that this has different format from the one in TS_RFX_CONTEXT */
	properties = 1; /* lt */
	properties |= (context->flags << 1); /* flags */
	properties |= (COL_CONV_ICT << 4); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3)
	               << 10); /* et */
	properties |= (SCALAR_QUANTIZATION << 14); /* qt */
	context->properties = properties;
}

static void rfx_write_message_sync(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_SYNC); /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 12); /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT32(s, WF_MAGIC); /* magic (4 bytes) */
	Stream_Write_UINT16(s, WF_VERSION_1_0); /* version (2 bytes) */
}

static void rfx_write_message_codec_versions(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 10); /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* numCodecs (1 byte) */
	Stream_Write_UINT8(s, 1); /* codecs.codecId (1 byte) */
	Stream_Write_UINT16(s, WF_VERSION_1_0); /* codecs.version (2 bytes) */
}

static void rfx_write_message_channels(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_CHANNELS); /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 12); /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* numChannels (1 byte) */
	Stream_Write_UINT8(s, 0); /* Channel.channelId (1 byte) */
	Stream_Write_UINT16(s, context->width); /* Channel.width (2 bytes) */
	Stream_Write_UINT16(s, context->height); /* Channel.height (2 bytes) */
}

static void rfx_write_message_context(RFX_CONTEXT* context, wStream* s)
{
	UINT16 properties;
	Stream_Write_UINT16(s, WBT_CONTEXT); /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, 13); /* CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0xFF); /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 0); /* ctxId (1 byte) */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize (2 bytes) */
	/* properties */
	properties = context->flags; /* flags */
	properties |= (COL_CONV_ICT << 3); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3)
	               << 9); /* et */
	properties |= (SCALAR_QUANTIZATION << 13); /* qt */
	Stream_Write_UINT16(s, properties); /* properties (2 bytes) */
	rfx_update_context_properties(context);
}

static BOOL rfx_compose_message_header(RFX_CONTEXT* context, wStream* s)
{
	if (!Stream_EnsureRemainingCapacity(s, 12 + 10 + 12 + 13))
		return FALSE;

	rfx_write_message_sync(context, s);
	rfx_write_message_context(context, s);
	rfx_write_message_codec_versions(context, s);
	rfx_write_message_channels(context, s);
	return TRUE;
}

static int rfx_tile_length(RFX_TILE* tile)
{
	return 19 + tile->YLen + tile->CbLen + tile->CrLen;
}

static BOOL rfx_write_tile(RFX_CONTEXT* context, wStream* s, RFX_TILE* tile)
{
	UINT32 blockLen;
	blockLen = rfx_tile_length(tile);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, CBT_TILE); /* BlockT.blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* BlockT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, tile->quantIdxY); /* quantIdxY (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCb); /* quantIdxCb (1 byte) */
	Stream_Write_UINT8(s, tile->quantIdxCr); /* quantIdxCr (1 byte) */
	Stream_Write_UINT16(s, tile->xIdx); /* xIdx (2 bytes) */
	Stream_Write_UINT16(s, tile->yIdx); /* yIdx (2 bytes) */
	Stream_Write_UINT16(s, tile->YLen); /* YLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CbLen); /* CbLen (2 bytes) */
	Stream_Write_UINT16(s, tile->CrLen); /* CrLen (2 bytes) */
	Stream_Write(s, tile->YData, tile->YLen); /* YData */
	Stream_Write(s, tile->CbData, tile->CbLen); /* CbData */
	Stream_Write(s, tile->CrData, tile->CrLen); /* CrData */
	return TRUE;
}

struct _RFX_TILE_COMPOSE_WORK_PARAM
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};

static void CALLBACK rfx_compose_message_tile_work_callback(PTP_CALLBACK_INSTANCE
        instance, void* context, PTP_WORK work)
{
	RFX_TILE_COMPOSE_WORK_PARAM* param = (RFX_TILE_COMPOSE_WORK_PARAM*) context;
	rfx_encode_rgb(param->context, param->tile);
}


static BOOL computeRegion(const RFX_RECT* rects, int numRects, REGION16* region,
                          int width, int height)
{
	int i;
	const RFX_RECT* rect = rects;
	const RECTANGLE_16 mainRect = { 0, 0, width, height };

	for (i = 0; i < numRects; i++, rect++)
	{
		RECTANGLE_16 rect16;
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

BOOL setupWorkers(RFX_CONTEXT* context, int nbTiles)
{
	RFX_CONTEXT_PRIV* priv = context->priv;
	void* pmem;

	if (!context->priv->UseThreads)
		return TRUE;

	if (!(pmem = realloc((void*) priv->workObjects, sizeof(PTP_WORK) * nbTiles)))
		return FALSE;

	priv->workObjects = (PTP_WORK*) pmem;

	if (!(pmem = realloc((void*) priv->tileWorkParams,
	                     sizeof(RFX_TILE_COMPOSE_WORK_PARAM) * nbTiles)))
		return FALSE;

	priv->tileWorkParams = (RFX_TILE_COMPOSE_WORK_PARAM*) pmem;
	return TRUE;
}

RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* context, const RFX_RECT* rects,
                                int numRects,
                                BYTE* data, int width, int height, int scanline)
{
	UINT32 i, maxNbTiles, maxTilesX, maxTilesY;
	UINT32 xIdx, yIdx, regionNbRects;
	UINT32 gridRelX, gridRelY, ax, ay, bytesPerPixel;
	RFX_TILE* tile;
	RFX_RECT* rfxRect;
	RFX_MESSAGE* message = NULL;
	PTP_WORK* workObject = NULL;
	RFX_TILE_COMPOSE_WORK_PARAM* workParam = NULL;
	BOOL success = FALSE;
	REGION16 rectsRegion, tilesRegion;
	RECTANGLE_16 currentTileRect;
	const RECTANGLE_16* regionRect;
	const RECTANGLE_16* extents;
	assert(data);
	assert(rects);
	assert(numRects > 0);
	assert(width > 0);
	assert(height > 0);
	assert(scanline > 0);

	if (!(message = (RFX_MESSAGE*)calloc(1, sizeof(RFX_MESSAGE))))
		return NULL;

	region16_init(&tilesRegion);
	region16_init(&rectsRegion);

	if (context->state == RFX_STATE_SEND_HEADERS)
		rfx_update_context_properties(context);

	message->frameIdx = context->frameIdx++;

	if (!context->numQuant)
	{
		if (!(context->quants = (UINT32*) malloc(sizeof(
		                            rfx_default_quantization_values))))
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
	bytesPerPixel = (context->bits_per_pixel / 8);

	if (!computeRegion(rects, numRects, &rectsRegion, width, height))
		goto skip_encoding_loop;

	extents = region16_extents(&rectsRegion);
	assert(extents->right - extents->left > 0);
	assert(extents->bottom - extents->top > 0);
	maxTilesX = 1 + TILE_NO(extents->right - 1) - TILE_NO(extents->left);
	maxTilesY = 1 + TILE_NO(extents->bottom - 1) - TILE_NO(extents->top);
	maxNbTiles = maxTilesX * maxTilesY;

	if (!(message->tiles = calloc(maxNbTiles, sizeof(RFX_TILE*))))
		goto skip_encoding_loop;

	if (!setupWorkers(context, maxNbTiles))
		goto skip_encoding_loop;

	if (context->priv->UseThreads)
	{
		workObject = context->priv->workObjects;
		workParam = context->priv->tileWorkParams;
	}

	regionRect = region16_rects(&rectsRegion, &regionNbRects);

	if (!(message->rects = calloc(regionNbRects, sizeof(RFX_RECT))))
		goto skip_encoding_loop;

	message->numRects = regionNbRects;

	for (i = 0, rfxRect = message->rects; i < regionNbRects;
	     i++, regionRect++, rfxRect++)
	{
		int startTileX = regionRect->left / 64;
		int endTileX = (regionRect->right - 1) / 64;
		int startTileY = regionRect->top / 64;
		int endTileY = (regionRect->bottom - 1) / 64;
		rfxRect->x = regionRect->left;
		rfxRect->y = regionRect->top;
		rfxRect->width = (regionRect->right - regionRect->left);
		rfxRect->height = (regionRect->bottom - regionRect->top);

		for (yIdx = startTileY, gridRelY = startTileY * 64; yIdx <= endTileY;
		     yIdx++, gridRelY += 64)
		{
			int tileHeight = 64;

			if ((yIdx == endTileY) && (gridRelY + 64 > height))
				tileHeight = height - gridRelY;

			currentTileRect.top = gridRelY;
			currentTileRect.bottom = gridRelY + tileHeight;

			for (xIdx = startTileX, gridRelX = startTileX * 64; xIdx <= endTileX;
			     xIdx++, gridRelX += 64)
			{
				int tileWidth = 64;

				if ((xIdx == endTileX) && (gridRelX + 64 > width))
					tileWidth = width - gridRelX;

				currentTileRect.left = gridRelX;
				currentTileRect.right = gridRelX + tileWidth;

				/* checks if this tile is already treated */
				if (region16_intersects_rect(&tilesRegion, &currentTileRect))
					continue;

				if (!(tile = (RFX_TILE*) ObjectPool_Take(context->priv->TilePool)))
					goto skip_encoding_loop;

				tile->xIdx = xIdx;
				tile->yIdx = yIdx;
				tile->x = gridRelX;
				tile->y = gridRelY;
				tile->scanline = scanline;
				tile->width = tileWidth;
				tile->height = tileHeight;
				ax = gridRelX;
				ay = gridRelY;

				if (tile->data && tile->allocated)
				{
					free(tile->data);
					tile->allocated = FALSE;
				}

				tile->data = &data[(ay * scanline) + (ax * bytesPerPixel)];
				tile->quantIdxY = context->quantIdxY;
				tile->quantIdxCb = context->quantIdxCb;
				tile->quantIdxCr = context->quantIdxCr;
				tile->YLen = tile->CbLen = tile->CrLen = 0;

				if (!(tile->YCbCrData = (BYTE*)BufferPool_Take(context->priv->BufferPool, -1)))
					goto skip_encoding_loop;

				tile->YData = (BYTE*) & (tile->YCbCrData[((8192 + 32) * 0) + 16]);
				tile->CbData = (BYTE*) & (tile->YCbCrData[((8192 + 32) * 1) + 16]);
				tile->CrData = (BYTE*) & (tile->YCbCrData[((8192 + 32) * 2) + 16]);
				message->tiles[message->numTiles] = tile;
				message->numTiles++;

				if (context->priv->UseThreads)
				{
					workParam->context = context;
					workParam->tile = tile;

					if (!(*workObject = CreateThreadpoolWork(
					                        rfx_compose_message_tile_work_callback,
					                        (void*) workParam,
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
		}  /* yIdx */
	}  /* rects */

	success = TRUE;
skip_encoding_loop:

	if (success && message->numTiles != maxNbTiles)
	{
		if (message->numTiles > 0)
		{
			void* pmem = realloc((void*) message->tiles,
			                     sizeof(RFX_TILE*) * message->numTiles);

			if (pmem)
				message->tiles = (RFX_TILE**) pmem;
			else
				success = FALSE;
		}
		else
			success = FALSE;
	}

	/* when using threads ensure all computations are done */
	if (success)
	{
		message->tilesDataSize = 0;
		workObject = context->priv->workObjects;

		for (i = 0; i < message->numTiles; i++)
		{
			tile = message->tiles[i];

			if (context->priv->UseThreads)
			{
				if (*workObject)
				{
					WaitForThreadpoolWorkCallbacks(*workObject, FALSE);
					CloseThreadpoolWork(*workObject);
				}

				workObject++;
			}

			message->tilesDataSize += rfx_tile_length(tile);
		}

		region16_uninit(&tilesRegion);
		region16_uninit(&rectsRegion);

		return message;
	}

	WLog_ERR(TAG, "%s: failed", __FUNCTION__);
	message->freeRects = TRUE;
	rfx_message_free(context, message);
	return NULL;
}

RFX_MESSAGE* rfx_split_message(RFX_CONTEXT* context, RFX_MESSAGE* message,
                               int* numMessages, int maxDataSize)
{
	int i, j;
	UINT32 tileDataSize;
	RFX_MESSAGE* messages;
	maxDataSize -= 1024; /* reserve enough space for headers */
	*numMessages = ((message->tilesDataSize + maxDataSize) / maxDataSize) * 4;

	if (!(messages = (RFX_MESSAGE*) calloc((*numMessages), sizeof(RFX_MESSAGE))))
		return NULL;

	j = 0;

	for (i = 0; i < message->numTiles; i++)
	{
		tileDataSize = rfx_tile_length(message->tiles[i]);

		if ((messages[j].tilesDataSize + tileDataSize) > ((UINT32) maxDataSize))
			j++;

		if (!messages[j].numTiles)
		{
			messages[j].frameIdx = message->frameIdx + j;
			messages[j].numQuant = message->numQuant;
			messages[j].quantVals = message->quantVals;
			messages[j].numRects = message->numRects;
			messages[j].rects = message->rects;
			messages[j].freeRects = FALSE;
			messages[j].freeArray = TRUE;

			if (!(messages[j].tiles = (RFX_TILE**) calloc(message->numTiles,
			                          sizeof(RFX_TILE*))))
				goto free_messages;
		}

		messages[j].tilesDataSize += tileDataSize;
		messages[j].tiles[messages[j].numTiles++] = message->tiles[i];
		message->tiles[i] = NULL;
	}

	*numMessages = j + 1;
	context->frameIdx += j;
	message->numTiles = 0;
	return messages;
free_messages:

	for (i = 0; i < j; i++)
		free(messages[i].tiles);

	free(messages);
	return NULL;
}

RFX_MESSAGE* rfx_encode_messages(RFX_CONTEXT* context, const RFX_RECT* rects,
                                 int numRects,
                                 BYTE* data, int width, int height, int scanline, int* numMessages,
                                 int maxDataSize)
{
	RFX_MESSAGE* message;
	RFX_MESSAGE* messageList;

	if (!(message = rfx_encode_message(context, rects, numRects, data, width,
	                                   height, scanline)))
		return NULL;

	if (!(messageList = rfx_split_message(context, message, numMessages,
	                                      maxDataSize)))
	{
		message->freeRects = TRUE;
		rfx_message_free(context, message);
		return NULL;
	}

	rfx_message_free(context, message);
	return messageList;
}

static BOOL rfx_write_message_tileset(RFX_CONTEXT* context, wStream* s,
                                      RFX_MESSAGE* message)
{
	int i;
	RFX_TILE* tile;
	UINT32 blockLen;
	UINT32* quantVals;
	blockLen = 22 + (message->numQuant * 5) + message->tilesDataSize;

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, WBT_EXTENSION); /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* set CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT16(s, CBT_TILESET); /* subtype (2 bytes) */
	Stream_Write_UINT16(s, 0); /* idx (2 bytes) */
	Stream_Write_UINT16(s, context->properties); /* properties (2 bytes) */
	Stream_Write_UINT8(s, message->numQuant); /* numQuant (1 byte) */
	Stream_Write_UINT8(s, 0x40); /* tileSize (1 byte) */
	Stream_Write_UINT16(s, message->numTiles); /* numTiles (2 bytes) */
	Stream_Write_UINT32(s, message->tilesDataSize); /* tilesDataSize (4 bytes) */
	quantVals = message->quantVals;

	for (i = 0; i < message->numQuant * 5; i++)
	{
		Stream_Write_UINT8(s, quantVals[0] + (quantVals[1] << 4));
		quantVals += 2;
	}

	for (i = 0; i < message->numTiles; i++)
	{
		if (!(tile = message->tiles[i]))
			return FALSE;

		if (!rfx_write_tile(context, s, tile))
			return FALSE;
	}

#ifdef WITH_DEBUG_RFX
	WLog_Print(context->priv->log, WLOG_DEBUG,
	           "numQuant: %"PRIu16" numTiles: %"PRIu16" tilesDataSize: %"PRIu32"",
	           message->numQuant, message->numTiles, message->tilesDataSize);
#endif
	return TRUE;
}

static BOOL rfx_write_message_frame_begin(RFX_CONTEXT* context, wStream* s,
        RFX_MESSAGE* message)
{
	if (!Stream_EnsureRemainingCapacity(s, 14))
		return FALSE;

	Stream_Write_UINT16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 14); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT32(s, message->frameIdx); /* frameIdx */
	Stream_Write_UINT16(s, 1); /* numRegions */
	return TRUE;
}

static BOOL rfx_write_message_region(RFX_CONTEXT* context, wStream* s,
                                     RFX_MESSAGE* message)
{
	int i;
	UINT32 blockLen;
	blockLen = 15 + (message->numRects * 8);

	if (!Stream_EnsureRemainingCapacity(s, blockLen))
		return FALSE;

	Stream_Write_UINT16(s, WBT_REGION); /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* set CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 1); /* regionFlags (1 byte) */
	Stream_Write_UINT16(s, message->numRects); /* numRects (2 bytes) */

	for (i = 0; i < message->numRects; i++)
	{
		const RFX_RECT* rect = rfx_message_get_rect(message, i);
		/* Clipping rectangles are relative to destLeft, destTop */
		Stream_Write_UINT16(s, rect->x); /* x (2 bytes) */
		Stream_Write_UINT16(s, rect->y); /* y (2 bytes) */
		Stream_Write_UINT16(s, rect->width); /* width (2 bytes) */
		Stream_Write_UINT16(s, rect->height); /* height (2 bytes) */
	}

	Stream_Write_UINT16(s, CBT_REGION); /* regionType (2 bytes) */
	Stream_Write_UINT16(s, 1); /* numTilesets (2 bytes) */
	return TRUE;
}

BOOL rfx_write_message_frame_end(RFX_CONTEXT* context, wStream* s,
                                 RFX_MESSAGE* message)
{
	if (!Stream_EnsureRemainingCapacity(s, 8))
		return FALSE;

	Stream_Write_UINT16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 8); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	return TRUE;
}

BOOL rfx_write_message(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
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

BOOL rfx_compose_message(RFX_CONTEXT* context, wStream* s,
                         const RFX_RECT* rects, int numRects, BYTE* data, int width, int height,
                         int scanline)
{
	RFX_MESSAGE* message;
	BOOL ret = TRUE;

	if (!(message = rfx_encode_message(context, rects, numRects, data, width,
	                                   height, scanline)))
		return FALSE;

	ret = rfx_write_message(context, s, message);
	message->freeRects = TRUE;
	rfx_message_free(context, message);
	return ret;
}
