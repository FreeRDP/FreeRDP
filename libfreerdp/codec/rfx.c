/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library
 *
 * Copyright 2011 Vic Lee
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>
#include <winpr/tchar.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/constants.h>
#include <freerdp/primitives.h>
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


#ifndef RFX_INIT_SIMD
#define RFX_INIT_SIMD(_rfx_context) do { } while (0)
#endif

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
	PROFILER_CREATE(context->priv->prof_rfx_decode_rgb, "rfx_decode_rgb");
	PROFILER_CREATE(context->priv->prof_rfx_decode_component, "rfx_decode_component");
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_decode, "rfx_rlgr_decode");
	PROFILER_CREATE(context->priv->prof_rfx_differential_decode, "rfx_differential_decode");
	PROFILER_CREATE(context->priv->prof_rfx_quantization_decode, "rfx_quantization_decode");
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_decode, "rfx_dwt_2d_decode");
	PROFILER_CREATE(context->priv->prof_rfx_ycbcr_to_rgb, "prims->yCbCrToRGB");
	PROFILER_CREATE(context->priv->prof_rfx_decode_format_rgb, "rfx_decode_format_rgb");

	PROFILER_CREATE(context->priv->prof_rfx_encode_rgb, "rfx_encode_rgb");
	PROFILER_CREATE(context->priv->prof_rfx_encode_component, "rfx_encode_component");
	PROFILER_CREATE(context->priv->prof_rfx_rlgr_encode, "rfx_rlgr_encode");
	PROFILER_CREATE(context->priv->prof_rfx_differential_encode, "rfx_differential_encode");
	PROFILER_CREATE(context->priv->prof_rfx_quantization_encode, "rfx_quantization_encode");
	PROFILER_CREATE(context->priv->prof_rfx_dwt_2d_encode, "rfx_dwt_2d_encode");
	PROFILER_CREATE(context->priv->prof_rfx_rgb_to_ycbcr, "prims->RGBToYCbCr");
	PROFILER_CREATE(context->priv->prof_rfx_encode_format_rgb, "rfx_encode_format_rgb");
}

static void rfx_profiler_free(RFX_CONTEXT* context)
{
	PROFILER_FREE(context->priv->prof_rfx_decode_rgb);
	PROFILER_FREE(context->priv->prof_rfx_decode_component);
	PROFILER_FREE(context->priv->prof_rfx_rlgr_decode);
	PROFILER_FREE(context->priv->prof_rfx_differential_decode);
	PROFILER_FREE(context->priv->prof_rfx_quantization_decode);
	PROFILER_FREE(context->priv->prof_rfx_dwt_2d_decode);
	PROFILER_FREE(context->priv->prof_rfx_ycbcr_to_rgb);
	PROFILER_FREE(context->priv->prof_rfx_decode_format_rgb);

	PROFILER_FREE(context->priv->prof_rfx_encode_rgb);
	PROFILER_FREE(context->priv->prof_rfx_encode_component);
	PROFILER_FREE(context->priv->prof_rfx_rlgr_encode);
	PROFILER_FREE(context->priv->prof_rfx_differential_encode);
	PROFILER_FREE(context->priv->prof_rfx_quantization_encode);
	PROFILER_FREE(context->priv->prof_rfx_dwt_2d_encode);
	PROFILER_FREE(context->priv->prof_rfx_rgb_to_ycbcr);
	PROFILER_FREE(context->priv->prof_rfx_encode_format_rgb);
}

static void rfx_profiler_print(RFX_CONTEXT* context)
{
	PROFILER_PRINT_HEADER;

	PROFILER_PRINT(context->priv->prof_rfx_decode_rgb);
	PROFILER_PRINT(context->priv->prof_rfx_decode_component);
	PROFILER_PRINT(context->priv->prof_rfx_rlgr_decode);
	PROFILER_PRINT(context->priv->prof_rfx_differential_decode);
	PROFILER_PRINT(context->priv->prof_rfx_quantization_decode);
	PROFILER_PRINT(context->priv->prof_rfx_dwt_2d_decode);
	PROFILER_PRINT(context->priv->prof_rfx_ycbcr_to_rgb);
	PROFILER_PRINT(context->priv->prof_rfx_decode_format_rgb);

	PROFILER_PRINT(context->priv->prof_rfx_encode_rgb);
	PROFILER_PRINT(context->priv->prof_rfx_encode_component);
	PROFILER_PRINT(context->priv->prof_rfx_rlgr_encode);
	PROFILER_PRINT(context->priv->prof_rfx_differential_encode);
	PROFILER_PRINT(context->priv->prof_rfx_quantization_encode);
	PROFILER_PRINT(context->priv->prof_rfx_dwt_2d_encode);
	PROFILER_PRINT(context->priv->prof_rfx_rgb_to_ycbcr);
	PROFILER_PRINT(context->priv->prof_rfx_encode_format_rgb);

	PROFILER_PRINT_FOOTER;
}

void rfx_tile_init(RFX_TILE* tile)
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

RFX_TILE* rfx_decoder_tile_new()
{
	RFX_TILE* tile = NULL;

	tile = (RFX_TILE*) malloc(sizeof(RFX_TILE));

	if (tile)
	{
		ZeroMemory(tile, sizeof(RFX_TILE));

		tile->data = (BYTE*) malloc(4096 * 4); /* 64x64 * 4 */
		tile->allocated = TRUE;
	}

	return tile;
}

void rfx_decoder_tile_free(RFX_TILE* tile)
{
	if (tile)
	{
		if (tile->allocated)
			free(tile->data);
		free(tile);
	}
}

RFX_TILE* rfx_encoder_tile_new()
{
	return (RFX_TILE *)calloc(1, sizeof(RFX_TILE));
}

void rfx_encoder_tile_free(RFX_TILE* tile)
{
	if (tile)
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
	wObject *pool;
	RFX_CONTEXT_PRIV *priv;

	context = (RFX_CONTEXT*)calloc(1, sizeof(RFX_CONTEXT));
	if (!context)
		return NULL;

	context->encoder = encoder;
	context->priv = priv = (RFX_CONTEXT_PRIV *)calloc(1, sizeof(RFX_CONTEXT_PRIV) );
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
		isVistaOrLater = ((verinfo.dwMajorVersion >= 6) && (verinfo.dwMinorVersion >= 0)) ? TRUE : FALSE;

		priv->UseThreads = isVistaOrLater;
	}
#else
	priv->UseThreads = TRUE;
#endif

	GetNativeSystemInfo(&sysinfo);

	priv->MinThreadCount = sysinfo.dwNumberOfProcessors;
	priv->MaxThreadCount = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\RemoteFX"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		dwSize = sizeof(dwValue);

		if (RegQueryValueEx(hKey, _T("UseThreads"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			priv->UseThreads = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("MinThreadCount"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			priv->MinThreadCount = dwValue;

		if (RegQueryValueEx(hKey, _T("MaxThreadCount"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
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
			SetThreadpoolThreadMinimum(priv->ThreadPool, priv->MinThreadCount);

		if (priv->MaxThreadCount)
			SetThreadpoolThreadMaximum(priv->ThreadPool, priv->MaxThreadCount);
	}

	/* initialize the default pixel format */
	rfx_context_set_pixel_format(context, RDP_PIXEL_FORMAT_B8G8R8A8);

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
	RFX_CONTEXT_PRIV *priv;

	assert(NULL != context);
	assert(NULL != context->priv);
	assert(NULL != context->priv->TilePool);
	assert(NULL != context->priv->BufferPool);

	priv = context->priv;
	if (context->quants)
		free(context->quants);

	ObjectPool_Free(priv->TilePool);

	rfx_profiler_print(context);
	rfx_profiler_free(context);

	if (priv->UseThreads)
	{
		CloseThreadpool(context->priv->ThreadPool);
		DestroyThreadpoolEnvironment(&context->priv->ThreadPoolEnv);

		if (priv->workObjects)
			free(priv->workObjects);
		if (priv->tileWorkParams)
			free(priv->tileWorkParams);

#ifdef WITH_PROFILER
		fprintf(stderr, "\nWARNING: Profiling results probably unusable with multithreaded RemoteFX codec!\n");
#endif
	}

	BufferPool_Free(context->priv->BufferPool);

	free(context->priv);
	free(context);
}

void rfx_context_set_pixel_format(RFX_CONTEXT* context, RDP_PIXEL_FORMAT pixel_format)
{
	context->pixel_format = pixel_format;

	switch (pixel_format)
	{
		case RDP_PIXEL_FORMAT_B8G8R8A8:
		case RDP_PIXEL_FORMAT_R8G8B8A8:
			context->bits_per_pixel = 32;
			break;
		case RDP_PIXEL_FORMAT_B8G8R8:
		case RDP_PIXEL_FORMAT_R8G8B8:
			context->bits_per_pixel = 24;
			break;
		case RDP_PIXEL_FORMAT_B5G6R5_LE:
		case RDP_PIXEL_FORMAT_R5G6B5_LE:
			context->bits_per_pixel = 16;
			break;
		case RDP_PIXEL_FORMAT_P4_PLANER:
			context->bits_per_pixel = 4;
			break;
		case RDP_PIXEL_FORMAT_P8:
			context->bits_per_pixel = 8;
			break;
		default:
			context->bits_per_pixel = 0;
			break;
	}
}

void rfx_context_reset(RFX_CONTEXT* context)
{
	context->state = RFX_STATE_SEND_HEADERS;
	context->frameIdx = 0;
}

static BOOL rfx_process_message_sync(RFX_CONTEXT* context, wStream* s)
{
	UINT32 magic;

	/* RFX_SYNC */
	if (Stream_GetRemainingLength(s) < 6)
	{
		DEBUG_WARN("RfxSync packet too small");
		return FALSE;
	}
	Stream_Read_UINT32(s, magic); /* magic (4 bytes), 0xCACCACCA */

	if (magic != WF_MAGIC)
	{
		DEBUG_WARN("invalid magic number 0x%X", magic);
		return FALSE;
	}

	Stream_Read_UINT16(s, context->version); /* version (2 bytes), WF_VERSION_1_0 (0x0100) */

	if (context->version != WF_VERSION_1_0)
	{
		DEBUG_WARN("unknown version number 0x%X", context->version);
		return FALSE;
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "version 0x%X", context->version);

	return TRUE;
}

static BOOL rfx_process_message_codec_versions(RFX_CONTEXT* context, wStream* s)
{
	BYTE numCodecs;

	if (Stream_GetRemainingLength(s) < 1)
	{
		DEBUG_WARN("RfxCodecVersion packet too small");
		return FALSE;
	}
	Stream_Read_UINT8(s, numCodecs); /* numCodecs (1 byte), must be set to 0x01 */

	if (numCodecs != 1)
	{
		DEBUG_WARN("numCodecs: %d, expected:1", numCodecs);
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < (size_t) (2 * numCodecs))
	{
		DEBUG_WARN("RfxCodecVersion packet too small for numCodecs=%d", numCodecs);
		return FALSE;
	}

	/* RFX_CODEC_VERSIONT */
	Stream_Read_UINT8(s, context->codec_id); /* codecId (1 byte) */
	Stream_Read_UINT8(s, context->codec_version); /* version (2 bytes) */

	WLog_Print(context->priv->log, WLOG_DEBUG, "id %d version 0x%X.", context->codec_id, context->codec_version);

	return TRUE;
}

static BOOL rfx_process_message_channels(RFX_CONTEXT* context, wStream* s)
{
	BYTE channelId;
	BYTE numChannels;

	if (Stream_GetRemainingLength(s) < 1)
	{
		DEBUG_WARN("RfxMessageChannels packet too small");
		return FALSE;
	}

	Stream_Read_UINT8(s, numChannels); /* numChannels (1 byte), must bet set to 0x01 */

	/* In RDVH sessions, numChannels will represent the number of virtual monitors 
	 * configured and does not always be set to 0x01 as [MS-RDPRFX] said.
	 */
	if (numChannels < 1)
	{
		DEBUG_WARN("numChannels:%d, expected:1", numChannels);
		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < (size_t) (numChannels * 5))
	{
		DEBUG_WARN("RfxMessageChannels packet too small for numChannels=%d", numChannels);
		return FALSE;
	}

	/* RFX_CHANNELT */
	Stream_Read_UINT8(s, channelId); /* channelId (1 byte) */
	Stream_Read_UINT16(s, context->width); /* width (2 bytes) */
	Stream_Read_UINT16(s, context->height); /* height (2 bytes) */

	/* Now, only the first monitor can be used, therefore the other channels will be ignored. */
	Stream_Seek(s, 5 * (numChannels - 1));

	WLog_Print(context->priv->log, WLOG_DEBUG, "numChannels %d id %d, %dx%d.",
		numChannels, channelId, context->width, context->height);

	return TRUE;
}

static BOOL rfx_process_message_context(RFX_CONTEXT* context, wStream* s)
{
	BYTE ctxId;
	UINT16 tileSize;
	UINT16 properties;

	if (Stream_GetRemainingLength(s) < 5)
	{
		DEBUG_WARN("RfxMessageContext packet too small");
		return FALSE;
	}

	Stream_Read_UINT8(s, ctxId); /* ctxId (1 byte), must be set to 0x00 */
	Stream_Read_UINT16(s, tileSize); /* tileSize (2 bytes), must be set to CT_TILE_64x64 (0x0040) */
	Stream_Read_UINT16(s, properties); /* properties (2 bytes) */

	WLog_Print(context->priv->log, WLOG_DEBUG, "ctxId %d tileSize %d properties 0x%X.",
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
			DEBUG_WARN("unknown RLGR algorithm.");
			break;
	}

	return TRUE;
}

static BOOL rfx_process_message_frame_begin(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	UINT32 frameIdx;
	UINT16 numRegions;

	if (Stream_GetRemainingLength(s) < 6)
	{
		DEBUG_WARN("RfxMessageFrameBegin packet too small");
		return FALSE;
	}

	Stream_Read_UINT32(s, frameIdx); /* frameIdx (4 bytes), if codec is in video mode, must be ignored */
	Stream_Read_UINT16(s, numRegions); /* numRegions (2 bytes) */

	WLog_Print(context->priv->log, WLOG_DEBUG, "RFX_FRAME_BEGIN: frameIdx: %d numRegions: %d", frameIdx, numRegions);

	return TRUE;
}

static void rfx_process_message_frame_end(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	WLog_Print(context->priv->log, WLOG_DEBUG, "RFX_FRAME_END");
}

static BOOL rfx_process_message_region(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	int i;

	if (Stream_GetRemainingLength(s) < 3)
	{
		DEBUG_WARN("RfxMessageRegion packet too small");
		return FALSE;
	}

	Stream_Seek_UINT8(s); /* regionFlags (1 byte) */
	Stream_Read_UINT16(s, message->numRects); /* numRects (2 bytes) */

	if (message->numRects < 1)
	{
		/* Unfortunately, it isn't documented.
		It seems that server asks to clip whole session when numRects = 0.
		Issue: https://github.com/FreeRDP/FreeRDP/issues/1738 */

		DEBUG_WARN("no rects. Clip whole session.");
		message->numRects = 1;
		message->rects = (RFX_RECT*) realloc(message->rects, message->numRects * sizeof(RFX_RECT));
		if (!message->rects)
			return FALSE;
		message->rects->x = 0;
		message->rects->y = 0;
		message->rects->width = context->width;
		message->rects->height = context->height;

		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < (size_t) (8 * message->numRects))
	{
		DEBUG_WARN("RfxMessageRegion packet too small for num_rects=%d", message->numRects);
		return FALSE;
	}

	message->rects = (RFX_RECT*) realloc(message->rects, message->numRects * sizeof(RFX_RECT));
	if (!message->rects)
		return FALSE;

	/* rects */
	for (i = 0; i < message->numRects; i++)
	{
		/* RFX_RECT */
		Stream_Read_UINT16(s, message->rects[i].x); /* x (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].y); /* y (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].width); /* width (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].height); /* height (2 bytes) */

		WLog_Print(context->priv->log, WLOG_DEBUG, "rect %d (x,y=%d,%d w,h=%d %d).", i,
				message->rects[i].x, message->rects[i].y,
				message->rects[i].width, message->rects[i].height);
	}

	return TRUE;
}

struct _RFX_TILE_PROCESS_WORK_PARAM
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};
typedef struct _RFX_TILE_PROCESS_WORK_PARAM RFX_TILE_PROCESS_WORK_PARAM;

void CALLBACK rfx_process_message_tile_work_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	RFX_TILE_PROCESS_WORK_PARAM* param = (RFX_TILE_PROCESS_WORK_PARAM*) context;
	rfx_decode_rgb(param->context, param->tile, param->tile->data, 64 * 4);
}

static BOOL rfx_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	BOOL rc;
	int i, close_cnt;
	int pos;
	BYTE quant;
	RFX_TILE* tile;
	UINT32* quants;
	UINT16 subtype;
	UINT32 blockLen;
	UINT32 blockType;
	UINT32 tilesDataSize;
	PTP_WORK* work_objects = NULL;
	RFX_TILE_PROCESS_WORK_PARAM* params = NULL;

	if (Stream_GetRemainingLength(s) < 14)
	{
		DEBUG_WARN("RfxMessageTileSet packet too small");
		return FALSE;
	}

	Stream_Read_UINT16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */

	if (subtype != CBT_TILESET)
	{
		DEBUG_WARN("invalid subtype, expected CBT_TILESET.");
		return FALSE;
	}

	Stream_Seek_UINT16(s); /* idx (2 bytes), must be set to 0x0000 */
	Stream_Seek_UINT16(s); /* properties (2 bytes) */

	Stream_Read_UINT8(s, context->numQuant); /* numQuant (1 byte) */
	Stream_Seek_UINT8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (context->numQuant < 1)
	{
		DEBUG_WARN("no quantization value.");
		return TRUE;
	}

	Stream_Read_UINT16(s, message->numTiles); /* numTiles (2 bytes) */

	if (message->numTiles < 1)
	{
		DEBUG_WARN("no tiles.");
		return TRUE;
	}

	Stream_Read_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	context->quants = (UINT32 *)realloc((void*) context->quants, context->numQuant * 10 * sizeof(UINT32));

	quants = context->quants;

	/* quantVals */
	if (Stream_GetRemainingLength(s) < (size_t) (context->numQuant * 5))
	{
		DEBUG_WARN("RfxMessageTileSet packet too small for num_quants=%d", context->numQuant);
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

		WLog_Print(context->priv->log, WLOG_DEBUG, "quant %d (%d %d %d %d %d %d %d %d %d %d).",
			i, context->quants[i * 10], context->quants[i * 10 + 1],
			context->quants[i * 10 + 2], context->quants[i * 10 + 3],
			context->quants[i * 10 + 4], context->quants[i * 10 + 5],
			context->quants[i * 10 + 6], context->quants[i * 10 + 7],
			context->quants[i * 10 + 8], context->quants[i * 10 + 9]);
	}

	message->tiles = (RFX_TILE**) malloc(sizeof(RFX_TILE*) * message->numTiles);
	ZeroMemory(message->tiles, sizeof(RFX_TILE*) * message->numTiles);

	if (context->priv->UseThreads)
	{
		work_objects = (PTP_WORK*) malloc(sizeof(PTP_WORK) * message->numTiles);
		params = (RFX_TILE_PROCESS_WORK_PARAM*)
			malloc(sizeof(RFX_TILE_PROCESS_WORK_PARAM) * message->numTiles);

		if (!work_objects)
		{
			if (params)
				free(params);
			return FALSE;
		}

		if (!params)
		{
			if (work_objects)
				free(work_objects);
			return FALSE;
		}

		ZeroMemory(work_objects, sizeof(PTP_WORK) * message->numTiles);
		ZeroMemory(params, sizeof(RFX_TILE_PROCESS_WORK_PARAM) * message->numTiles);
	}

	/* tiles */
	close_cnt = 0;
	rc = TRUE;
	for (i = 0; i < message->numTiles; i++)
	{
		tile = message->tiles[i] = (RFX_TILE*) ObjectPool_Take(context->priv->TilePool);

		/* RFX_TILE */
		if (Stream_GetRemainingLength(s) < 6)
		{
			DEBUG_WARN("RfxMessageTileSet packet too small to read tile %d/%d", i, message->numTiles);
			rc = FALSE;
			break;
		}

		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */

		if (Stream_GetRemainingLength(s) < blockLen - 6)
		{
			DEBUG_WARN("RfxMessageTileSet not enough bytes to read tile %d/%d with blocklen=%d",
					i, message->numTiles, blockLen);
			rc = FALSE;
			break;
		}

		pos = Stream_GetPosition(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			DEBUG_WARN("unknown block type 0x%X, expected CBT_TILE (0xCAC3).", blockType);
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

			work_objects[i] = CreateThreadpoolWork((PTP_WORK_CALLBACK) rfx_process_message_tile_work_callback,
					(void*) &params[i], &context->priv->ThreadPoolEnv);

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

	if (work_objects)
		free(work_objects);
	if (params)
		free(params);

	for (i = 0; i < message->numTiles; i++)
	{
		tile = message->tiles[i];
		tile->YLen = tile->CbLen = tile->CrLen = 0;
		tile->YData = tile->CbData = tile->CrData = NULL;
	}

	return rc;
}

RFX_MESSAGE* rfx_process_message(RFX_CONTEXT* context, BYTE* data, UINT32 length)
{
	int pos;
	wStream* s;
	UINT32 blockLen;
	UINT32 blockType;
	RFX_MESSAGE* message;

	message = (RFX_MESSAGE*) malloc(sizeof(RFX_MESSAGE));
	ZeroMemory(message, sizeof(RFX_MESSAGE));

	message->freeRects = TRUE;

	s = Stream_New(data, length);

	while (Stream_GetRemainingLength(s) > 6)
	{
		/* RFX_BLOCKT */
		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */

		WLog_Print(context->priv->log, WLOG_DEBUG, "blockType 0x%X blockLen %d", blockType, blockLen);

		if (blockLen == 0)
		{
			DEBUG_WARN("zero blockLen");
			break;
		}

		if (Stream_GetRemainingLength(s) < blockLen - 6)
		{
			DEBUG_WARN("rfx_process_message: packet too small for blocklen=%d", blockLen);
			break;
		}


		pos = Stream_GetPosition(s) - 6 + blockLen;

		if (blockType >= WBT_CONTEXT && blockType <= WBT_EXTENSION)
		{
			/* RFX_CODEC_CHANNELT */
			/* codecId (1 byte) must be set to 0x01 */
			/* channelId (1 byte) must be set to 0x00 */
			if (!Stream_SafeSeek(s, 2))
			{
				DEBUG_WARN("rfx_process_message: unable to skip RFX_CODEC_CHANNELT");
				break;
			}
		}

		switch (blockType)
		{
			case WBT_SYNC:
				rfx_process_message_sync(context, s);
				break;

			case WBT_CODEC_VERSIONS:
				rfx_process_message_codec_versions(context, s);
				break;

			case WBT_CHANNELS:
				rfx_process_message_channels(context, s);
				break;

			case WBT_CONTEXT:
				rfx_process_message_context(context, s);
				break;

			case WBT_FRAME_BEGIN:
				rfx_process_message_frame_begin(context, message, s);
				break;

			case WBT_FRAME_END:
				rfx_process_message_frame_end(context, message, s);
				break;

			case WBT_REGION:
				rfx_process_message_region(context, message, s);
				break;

			case WBT_EXTENSION:
				rfx_process_message_tileset(context, message, s);
				break;

			default:
				DEBUG_WARN("unknown blockType 0x%X", blockType);
				break;
		}

		Stream_SetPosition(s, pos);
	}

	Stream_Free(s, FALSE);

	return message;
}

UINT16 rfx_message_get_tile_count(RFX_MESSAGE* message)
{
	return message->numTiles;
}

RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, int index)
{
	return message->tiles[index];
}

UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message)
{
	return message->numRects;
}

RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, int index)
{
	return &message->rects[index];
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
				tile = message->tiles[i];

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
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 10); /* et */
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
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 0); /* ctxId (1 byte) */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize (2 bytes) */

	/* properties */
	properties = context->flags; /* flags */
	properties |= (COL_CONV_ICT << 3); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 9); /* et */
	properties |= (SCALAR_QUANTIZATION << 13); /* qt */
	Stream_Write_UINT16(s, properties); /* properties (2 bytes) */

	rfx_update_context_properties(context);
}

void rfx_compose_message_header(RFX_CONTEXT* context, wStream* s)
{
	Stream_EnsureRemainingCapacity(s, 12 + 10 + 12 + 13);

	rfx_write_message_sync(context, s);
	rfx_write_message_context(context, s);
	rfx_write_message_codec_versions(context, s);
	rfx_write_message_channels(context, s);
}

static int rfx_tile_length(RFX_TILE* tile)
{
	return 19 + tile->YLen + tile->CbLen + tile->CrLen;
}

static void rfx_write_tile(RFX_CONTEXT* context, wStream* s, RFX_TILE* tile)
{
	UINT32 blockLen;

	blockLen = rfx_tile_length(tile);
	Stream_EnsureRemainingCapacity(s, blockLen);

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
}

struct _RFX_TILE_COMPOSE_WORK_PARAM
{
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};

void CALLBACK rfx_compose_message_tile_work_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	RFX_TILE_COMPOSE_WORK_PARAM* param = (RFX_TILE_COMPOSE_WORK_PARAM*) context;
	rfx_encode_rgb(param->context, param->tile);
}


static BOOL computeRegion(const RFX_RECT* rects, int numRects, REGION16 *region, int width, int height)
{
	int i;
	const RFX_RECT *rect = rects;
	const RECTANGLE_16 mainRect = { 0, 0, width, height };

	for(i = 0; i < numRects; i++, rect++) {
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

BOOL setupWorkers(RFX_CONTEXT *context, int nbTiles)
{
	RFX_CONTEXT_PRIV *priv = context->priv;

	if (!context->priv->UseThreads)
		return TRUE;

	priv->workObjects = (PTP_WORK *)realloc(priv->workObjects, sizeof(PTP_WORK) * nbTiles);
	if (!priv->workObjects)
	 return FALSE;

	priv->tileWorkParams = (RFX_TILE_COMPOSE_WORK_PARAM *)
			realloc(priv->tileWorkParams, sizeof(RFX_TILE_COMPOSE_WORK_PARAM) * nbTiles);
	if (!priv->tileWorkParams)
		return FALSE;

	return TRUE;
}


RFX_MESSAGE* rfx_encode_message(RFX_CONTEXT* context, const RFX_RECT* rects, int numRects,
		BYTE* data, int width, int height, int scanline)
{
	int i, maxNbTiles, maxTilesX, maxTilesY;
	int xIdx, yIdx, regionNbRects;
	int gridRelX, gridRelY, ax, ay, bytesPerPixel;
	RFX_TILE* tile;
	RFX_RECT* rfxRect;
	RFX_MESSAGE* message = NULL;
	PTP_WORK* workObject = NULL;
	RFX_TILE_COMPOSE_WORK_PARAM *workParam = NULL;

	REGION16 rectsRegion, tilesRegion;
	RECTANGLE_16 currentTileRect;
	const RECTANGLE_16 *regionRect;
	const RECTANGLE_16 *extents;

	assert(data);
	assert(rects);
	assert(numRects > 0);
	assert(width > 0);
	assert(height > 0);
	assert(scanline > 0);

	message = (RFX_MESSAGE *)calloc(1, sizeof(RFX_MESSAGE));
	if (!message)
		return NULL;

	if (context->state == RFX_STATE_SEND_HEADERS)
		rfx_update_context_properties(context);

	message->frameIdx = context->frameIdx++;
	if (!context->numQuant)
	{
		context->numQuant = 1;
		context->quants = (UINT32*) malloc(sizeof(rfx_default_quantization_values));
		CopyMemory(context->quants, &rfx_default_quantization_values, sizeof(rfx_default_quantization_values));
		context->quantIdxY = 0;
		context->quantIdxCb = 0;
		context->quantIdxCr = 0;
	}
	message->numQuant = context->numQuant;
	message->quantVals = context->quants;

	bytesPerPixel = (context->bits_per_pixel / 8);

	region16_init(&rectsRegion);
	if (!computeRegion(rects, numRects, &rectsRegion, width, height))
		goto out_free_message;

	extents = region16_extents(&rectsRegion);
	assert(extents->right - extents->left > 0);
	assert(extents->bottom - extents->top > 0);

	maxTilesX = 1 + TILE_NO(extents->right - 1) - TILE_NO(extents->left);
	maxTilesY = 1 + TILE_NO(extents->bottom - 1) - TILE_NO(extents->top);
	maxNbTiles = maxTilesX * maxTilesY;

	message->tiles = calloc(maxNbTiles, sizeof(RFX_TILE*));
	if (!message->tiles)
		goto out_free_message;

	if (!setupWorkers(context, maxNbTiles))
		goto out_clean_tiles;

	if (context->priv->UseThreads)
	{
		workObject = context->priv->workObjects;
		workParam = context->priv->tileWorkParams;
	}

	regionRect = region16_rects(&rectsRegion, &regionNbRects);
	message->rects = rfxRect = calloc(regionNbRects, sizeof(RFX_RECT));
	if (!message->rects)
		goto out_clean_tiles;
	message->numRects = regionNbRects;

	region16_init(&tilesRegion);

	for (i = 0; i < regionNbRects; i++, regionRect++, rfxRect++)
	{
		int startTileX = regionRect->left / 64;
		int endTileX = (regionRect->right - 1) / 64;

		int startTileY = regionRect->top / 64;
		int endTileY = (regionRect->bottom - 1) / 64;

		rfxRect->x = regionRect->left;
		rfxRect->y = regionRect->top;
		rfxRect->width = (regionRect->right - regionRect->left);
		rfxRect->height = (regionRect->bottom - regionRect->top);


		for (yIdx = startTileY, gridRelY = startTileY * 64; yIdx <= endTileY; yIdx++, gridRelY += 64 )
		{
			int tileHeight = 64;
			if ((yIdx == endTileY) && (gridRelY + 64 > height))
				tileHeight = height - gridRelY;

			currentTileRect.top = gridRelY;
			currentTileRect.bottom = gridRelY + tileHeight;

			for (xIdx = startTileX, gridRelX = startTileX * 64; xIdx <= endTileX; xIdx++, gridRelX += 64)
			{
				int tileWidth = 64;
				if ((xIdx == endTileX) && (gridRelX + 64 > width))
					tileWidth = width - gridRelX;

				currentTileRect.left = gridRelX;
				currentTileRect.right = gridRelX + tileWidth;

				/* checks if this tile is already treated */
				if (region16_intersects_rect(&tilesRegion, &currentTileRect))
					continue;

				tile = (RFX_TILE *)ObjectPool_Take(context->priv->TilePool);
				if (!tile)
					goto out_clean_rects;;

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

				tile->YCbCrData = (BYTE *)BufferPool_Take(context->priv->BufferPool, -1);
				if (!tile->YCbCrData)
					goto out_clean_rects;

				tile->YData = (BYTE*) &(tile->YCbCrData[((8192 + 32) * 0) + 16]);
				tile->CbData = (BYTE*) &(tile->YCbCrData[((8192 + 32) * 1) + 16]);
				tile->CrData = (BYTE*) &(tile->YCbCrData[((8192 + 32) * 2) + 16]);

				if (context->priv->UseThreads)
				{
					workParam->context = context;
					workParam->tile = tile;

					*workObject = CreateThreadpoolWork(
							(PTP_WORK_CALLBACK)rfx_compose_message_tile_work_callback,
							(void *)workParam,
							&context->priv->ThreadPoolEnv
					);

					SubmitThreadpoolWork(*workObject);

					workObject++;
					workParam++;
				}
				else
				{
					rfx_encode_rgb(context, tile);
				}

				message->tiles[message->numTiles] = tile;
				message->numTiles++;

				if (!region16_union_rect(&tilesRegion, &tilesRegion, &currentTileRect))
					goto out_clean_rects;
			} /* xIdx */
		}  /* yIdx */
	}  /* rects */

	if (message->numTiles != maxNbTiles)
	{
		message->tiles = realloc(message->tiles, sizeof(RFX_TILE *) * message->numTiles);
		if (!message->tiles)
			goto out_clean_rects;
	}

	region16_uninit(&tilesRegion);

	/* when using threads ensure all computations are done */
	message->tilesDataSize = 0;
	workObject = context->priv->workObjects;
	for (i = 0; i < message->numTiles; i++)
	{
		tile = message->tiles[i];
		if (context->priv->UseThreads)
		{
			WaitForThreadpoolWorkCallbacks(*workObject, FALSE);
			CloseThreadpoolWork(*workObject);
			workObject++;
		}

		message->tilesDataSize += rfx_tile_length(tile);
	}


	region16_uninit(&rectsRegion);
	return message;

out_clean_rects:
	free(message->rects);
out_clean_tiles:
	free(message->tiles);
	region16_uninit(&tilesRegion);
out_free_message:
	fprintf(stderr, "remoteFx error\n");
	region16_uninit(&rectsRegion);
	free(message);
	return 0;
}


RFX_MESSAGE* rfx_split_message(RFX_CONTEXT* context, RFX_MESSAGE* message, int* numMessages, int maxDataSize)
{
	int i, j;
	UINT32 tileDataSize;
	RFX_MESSAGE* messages;

	maxDataSize -= 1024; /* reserve enough space for headers */

	*numMessages = ((message->tilesDataSize + maxDataSize) / maxDataSize) * 4;

	messages = (RFX_MESSAGE*) malloc(sizeof(RFX_MESSAGE) * (*numMessages));
	ZeroMemory(messages, sizeof(RFX_MESSAGE) * (*numMessages));

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
			messages[j].tiles = (RFX_TILE**) malloc(sizeof(RFX_TILE*) * message->numTiles);
			messages[j].freeRects = FALSE;
			messages[j].freeArray = TRUE;
		}

		messages[j].tilesDataSize += tileDataSize;
		messages[j].tiles[messages[j].numTiles++] = message->tiles[i];
		message->tiles[i] = NULL;
	}

	*numMessages = j + 1;
	context->frameIdx += j;
	message->numTiles = 0;

	for (i = 0; i < *numMessages; i++)
	{
		for (j = 0; j < messages[i].numTiles; j++)
		{

		}
	}

	return messages;
}

RFX_MESSAGE* rfx_encode_messages(RFX_CONTEXT* context, const RFX_RECT* rects, int numRects,
		BYTE* data, int width, int height, int scanline, int* numMessages, int maxDataSize)
{
	RFX_MESSAGE* message;
	RFX_MESSAGE* messages;

	message = rfx_encode_message(context, rects, numRects, data, width, height, scanline);
	messages = rfx_split_message(context, message, numMessages, maxDataSize);
	rfx_message_free(context, message);

	return messages;
}

static void rfx_write_message_tileset(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
	int i;
	RFX_TILE* tile;
	UINT32 blockLen;
	UINT32* quantVals;

	blockLen = 22 + (message->numQuant * 5) + message->tilesDataSize;
	Stream_EnsureRemainingCapacity(s, blockLen);

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
		tile = message->tiles[i];
		rfx_write_tile(context, s, tile);
	}

	WLog_Print(context->priv->log, WLOG_DEBUG, "numQuant: %d numTiles: %d tilesDataSize: %d",
			message->numQuant, message->numTiles, message->tilesDataSize);
}

void rfx_write_message_frame_begin(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
	Stream_EnsureRemainingCapacity(s, 14);

	Stream_Write_UINT16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 14); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT32(s, message->frameIdx); /* frameIdx */
	Stream_Write_UINT16(s, 1); /* numRegions */
}

void rfx_write_message_region(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
	int i;
	UINT32 blockLen;

	blockLen = 15 + (message->numRects * 8);
	Stream_EnsureRemainingCapacity(s, blockLen);

	Stream_Write_UINT16(s, WBT_REGION); /* CodecChannelT.blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* set CodecChannelT.blockLen (4 bytes) */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId (1 byte) */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId (1 byte) */
	Stream_Write_UINT8(s, 1); /* regionFlags (1 byte) */
	Stream_Write_UINT16(s, message->numRects); /* numRects (2 bytes) */

	for (i = 0; i < message->numRects; i++)
	{
		/* Clipping rectangles are relative to destLeft, destTop */

		Stream_Write_UINT16(s, message->rects[i].x); /* x (2 bytes) */
		Stream_Write_UINT16(s, message->rects[i].y); /* y (2 bytes) */
		Stream_Write_UINT16(s, message->rects[i].width); /* width (2 bytes) */
		Stream_Write_UINT16(s, message->rects[i].height); /* height (2 bytes) */
	}

	Stream_Write_UINT16(s, CBT_REGION); /* regionType (2 bytes) */
	Stream_Write_UINT16(s, 1); /* numTilesets (2 bytes) */
}

void rfx_write_message_frame_end(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 8); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
}

void rfx_write_message(RFX_CONTEXT* context, wStream* s, RFX_MESSAGE* message)
{
	if (context->state == RFX_STATE_SEND_HEADERS)
	{
		rfx_compose_message_header(context, s);
		context->state = RFX_STATE_SEND_FRAME_DATA;
	}

	rfx_write_message_frame_begin(context, s, message);
	rfx_write_message_region(context, s, message);
	rfx_write_message_tileset(context, s, message);
	rfx_write_message_frame_end(context, s, message);
}

void rfx_compose_message(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int numRects, BYTE* data, int width, int height, int scanline)
{
	RFX_MESSAGE* message;

 	message = rfx_encode_message(context, rects, numRects, data, width, height, scanline);

	rfx_write_message(context, s, message);

	rfx_message_free(context, message);
}

