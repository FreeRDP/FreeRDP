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

#include "rfx_constants.h"
#include "rfx_types.h"
#include "rfx_decode.h"
#include "rfx_encode.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"

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

RFX_CONTEXT* rfx_context_new(void)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	SYSTEM_INFO sysinfo;
	RFX_CONTEXT* context;

	context = (RFX_CONTEXT*) malloc(sizeof(RFX_CONTEXT));
	ZeroMemory(context, sizeof(RFX_CONTEXT));

	context->priv = (RFX_CONTEXT_PRIV*) malloc(sizeof(RFX_CONTEXT_PRIV));
	ZeroMemory(context->priv, sizeof(RFX_CONTEXT_PRIV));

	context->priv->TilePool = Queue_New(TRUE, -1, -1);
	context->priv->TileQueue = Queue_New(TRUE, -1, -1);

	/*
	 * align buffers to 16 byte boundary (needed for SSE/NEON instructions)
	 *
	 * y_r_buffer, cb_g_buffer, cr_b_buffer: 64 * 64 * 4 = 16384 (0x4000)
	 * dwt_buffer: 32 * 32 * 2 * 2 * 4 = 16384, maximum sub-band width is 32
	 *
	 * Additionally we add 32 bytes (16 in front and 16 at the back of the buffer)
	 * in order to allow optimized functions (SEE, NEON) to read from positions 
	 * that are actually in front/beyond the buffer. Offset calculations are
	 * performed at the BufferPool_Take function calls in rfx_encode/decode.c.
	 */

	context->priv->BufferPool = BufferPool_New(TRUE, 16384 + 32, 16);

#ifdef _WIN32
	{
		BOOL isVistaOrLater;
		OSVERSIONINFOA verinfo;

		ZeroMemory(&verinfo, sizeof(OSVERSIONINFOA));
		verinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);

		GetVersionExA(&verinfo);
		isVistaOrLater = ((verinfo.dwMajorVersion >= 6) && (verinfo.dwMinorVersion >= 0)) ? TRUE : FALSE;

		context->priv->UseThreads = isVistaOrLater;
	}
#else
	context->priv->UseThreads = TRUE;
#endif

	GetNativeSystemInfo(&sysinfo);

	context->priv->MinThreadCount = sysinfo.dwNumberOfProcessors;
	context->priv->MaxThreadCount = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\RemoteFX"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		dwSize = sizeof(dwValue);

		if (RegQueryValueEx(hKey, _T("UseThreads"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			context->priv->UseThreads = dwValue ? 1 : 0;

		if (RegQueryValueEx(hKey, _T("MinThreadCount"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			context->priv->MinThreadCount = dwValue;

		if (RegQueryValueEx(hKey, _T("MaxThreadCount"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			context->priv->MaxThreadCount = dwValue;

		RegCloseKey(hKey);
	}

	if (context->priv->UseThreads)
	{
		/* Call primitives_get here in order to avoid race conditions when using primitives_get */
		/* from multiple threads. This call will initialize all function pointers correctly     */
		/* before any decoding threads are started */
		primitives_get();

		context->priv->ThreadPool = CreateThreadpool(NULL);
		InitializeThreadpoolEnvironment(&context->priv->ThreadPoolEnv);
		SetThreadpoolCallbackPool(&context->priv->ThreadPoolEnv, context->priv->ThreadPool);

		if (context->priv->MinThreadCount)
			SetThreadpoolThreadMinimum(context->priv->ThreadPool, context->priv->MinThreadCount);

		if (context->priv->MaxThreadCount)
			SetThreadpoolThreadMaximum(context->priv->ThreadPool, context->priv->MaxThreadCount);
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

	RFX_INIT_SIMD(context);
	
	return context;
}

void rfx_context_free(RFX_CONTEXT* context)
{
	free(context->quants);

	Queue_Free(context->priv->TilePool);
	Queue_Free(context->priv->TileQueue);

	rfx_profiler_print(context);
	rfx_profiler_free(context);

	if (context->priv->UseThreads)
	{
		CloseThreadpool(context->priv->ThreadPool);
		DestroyThreadpoolEnvironment(&context->priv->ThreadPoolEnv);
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
	context->header_processed = FALSE;
	context->frame_idx = 0;
}

RFX_TILE* rfx_tile_pool_take(RFX_CONTEXT* context)
{
	RFX_TILE* tile = NULL;

	if (WaitForSingleObject(Queue_Event(context->priv->TilePool), 0) == WAIT_OBJECT_0)
		tile = Queue_Dequeue(context->priv->TilePool);

	if (!tile)
	{
		tile = (RFX_TILE*) malloc(sizeof(RFX_TILE));

		tile->x = tile->y = 0;
		tile->data = (BYTE*) malloc(4096 * 4); /* 64x64 * 4 */
	}

	return tile;
}

int rfx_tile_pool_return(RFX_CONTEXT* context, RFX_TILE* tile)
{
	Queue_Enqueue(context->priv->TilePool, tile);
	return 0;
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

	DEBUG_RFX("version 0x%X", context->version);
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

	if (Stream_GetRemainingLength(s) <  2 * numCodecs)
	{
		DEBUG_WARN("RfxCodecVersion packet too small for numCodecs=%d", numCodecs);
		return FALSE;
	}

	/* RFX_CODEC_VERSIONT */
	Stream_Read_UINT8(s, context->codec_id); /* codecId (1 byte) */
	Stream_Read_UINT8(s, context->codec_version); /* version (2 bytes) */

	DEBUG_RFX("id %d version 0x%X.", context->codec_id, context->codec_version);
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

	if (Stream_GetRemainingLength(s) < numChannels * 5)
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

	DEBUG_RFX("numChannels %d id %d, %dx%d.",
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

	DEBUG_RFX("ctxId %d tileSize %d properties 0x%X.", ctxId, tileSize, properties);

	context->properties = properties;
	context->flags = (properties & 0x0007);

	if (context->flags == CODEC_MODE)
		DEBUG_RFX("codec is in image mode.");
	else
		DEBUG_RFX("codec is in video mode.");

	switch ((properties & 0x1E00) >> 9)
	{
		case CLW_ENTROPY_RLGR1:
			context->mode = RLGR1;
			DEBUG_RFX("RLGR1.");
			break;

		case CLW_ENTROPY_RLGR3:
			context->mode = RLGR3;
			DEBUG_RFX("RLGR3.");
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

	DEBUG_RFX("RFX_FRAME_BEGIN: frameIdx:%d numRegions:%d", frameIdx, numRegions);
	return TRUE;
}

static void rfx_process_message_frame_end(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	DEBUG_RFX("RFX_FRAME_END");
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
	Stream_Read_UINT16(s, message->num_rects); /* numRects (2 bytes) */

	if (message->num_rects < 1)
	{
		DEBUG_WARN("no rects.");
		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < 8 * message->num_rects)
	{
		DEBUG_WARN("RfxMessageRegion packet too small for num_rects=%d", message->num_rects);
		return FALSE;
	}

	if (message->rects != NULL)
		message->rects = (RFX_RECT*) realloc(message->rects, message->num_rects * sizeof(RFX_RECT));
	else
		message->rects = (RFX_RECT*) malloc(message->num_rects * sizeof(RFX_RECT));

	/* rects */
	for (i = 0; i < message->num_rects; i++)
	{
		/* RFX_RECT */
		Stream_Read_UINT16(s, message->rects[i].x); /* x (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].y); /* y (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].width); /* width (2 bytes) */
		Stream_Read_UINT16(s, message->rects[i].height); /* height (2 bytes) */

		DEBUG_RFX("rect %d (%d %d %d %d).",
			i, message->rects[i].x, message->rects[i].y, message->rects[i].width, message->rects[i].height);
	}
	return TRUE;
}

static BOOL rfx_process_message_tile(RFX_CONTEXT* context, RFX_TILE* tile, wStream* s)
{
	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx, yIdx;
	UINT16 YLen, CbLen, CrLen;

	if (Stream_GetRemainingLength(s) < 13)
	{
		DEBUG_WARN("RfxMessageTile packet too small");
		return FALSE;
	}

	/* RFX_TILE */
	Stream_Read_UINT8(s, quantIdxY); /* quantIdxY (1 byte) */
	Stream_Read_UINT8(s, quantIdxCb); /* quantIdxCb (1 byte) */
	Stream_Read_UINT8(s, quantIdxCr); /* quantIdxCr (1 byte) */
	Stream_Read_UINT16(s, xIdx); /* xIdx (2 bytes) */
	Stream_Read_UINT16(s, yIdx); /* yIdx (2 bytes) */
	Stream_Read_UINT16(s, YLen); /* YLen (2 bytes) */
	Stream_Read_UINT16(s, CbLen); /* CbLen (2 bytes) */
	Stream_Read_UINT16(s, CrLen); /* CrLen (2 bytes) */

	DEBUG_RFX("quantIdxY:%d quantIdxCb:%d quantIdxCr:%d xIdx:%d yIdx:%d YLen:%d CbLen:%d CrLen:%d",
		quantIdxY, quantIdxCb, quantIdxCr, xIdx, yIdx, YLen, CbLen, CrLen);

	tile->x = xIdx * 64;
	tile->y = yIdx * 64;

	return rfx_decode_rgb(context, s,
		YLen, context->quants + (quantIdxY * 10),
		CbLen, context->quants + (quantIdxCb * 10),
		CrLen, context->quants + (quantIdxCr * 10),
		tile->data, 64 * 4);
}

struct _RFX_TILE_WORK_PARAM
{
	wStream s;
	RFX_TILE* tile;
	RFX_CONTEXT* context;
};
typedef struct _RFX_TILE_WORK_PARAM RFX_TILE_WORK_PARAM;

void CALLBACK rfx_process_message_tile_work_callback(PTP_CALLBACK_INSTANCE instance, void* context, PTP_WORK work)
{
	RFX_TILE_WORK_PARAM* param = (RFX_TILE_WORK_PARAM*) context;
	rfx_process_message_tile(param->context, param->tile, &(param->s));
}

static BOOL rfx_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s)
{
	int i;
	int pos;
	BYTE quant;
	UINT32* quants;
	UINT16 subtype;
	UINT32 blockLen;
	UINT32 blockType;
	UINT32 tilesDataSize;
	PTP_WORK* work_objects = NULL;
	RFX_TILE_WORK_PARAM* params = NULL;

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

	Stream_Read_UINT8(s, context->num_quants); /* numQuant (1 byte) */
	Stream_Seek_UINT8(s); /* tileSize (1 byte), must be set to 0x40 */

	if (context->num_quants < 1)
	{
		DEBUG_WARN("no quantization value.");
		return TRUE;
	}

	Stream_Read_UINT16(s, message->num_tiles); /* numTiles (2 bytes) */

	if (message->num_tiles < 1)
	{
		DEBUG_WARN("no tiles.");
		return TRUE;
	}

	Stream_Read_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	if (context->quants != NULL)
		context->quants = (UINT32*) realloc((void*) context->quants, context->num_quants * 10 * sizeof(UINT32));
	else
		context->quants = (UINT32*) malloc(context->num_quants * 10 * sizeof(UINT32));
	quants = context->quants;

	/* quantVals */
	if (Stream_GetRemainingLength(s) < context->num_quants * 5)
	{
		DEBUG_WARN("RfxMessageTileSet packet too small for num_quants=%d", context->num_quants);
		return FALSE;
	}

	for (i = 0; i < context->num_quants; i++)
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

		DEBUG_RFX("quant %d (%d %d %d %d %d %d %d %d %d %d).",
			i, context->quants[i * 10], context->quants[i * 10 + 1],
			context->quants[i * 10 + 2], context->quants[i * 10 + 3],
			context->quants[i * 10 + 4], context->quants[i * 10 + 5],
			context->quants[i * 10 + 6], context->quants[i * 10 + 7],
			context->quants[i * 10 + 8], context->quants[i * 10 + 9]);
	}

	message->tiles = (RFX_TILE**) malloc(sizeof(RFX_TILE*) * message->num_tiles);
	ZeroMemory(message->tiles, sizeof(RFX_TILE*) * message->num_tiles);

	if (context->priv->UseThreads)
	{
		work_objects = (PTP_WORK*) malloc(sizeof(PTP_WORK) * message->num_tiles);
		params = (RFX_TILE_WORK_PARAM*) malloc(sizeof(RFX_TILE_WORK_PARAM) * message->num_tiles);
	}

	/* tiles */
	for (i = 0; i < message->num_tiles; i++)
	{
		/* RFX_TILE */
		if (Stream_GetRemainingLength(s) < 6)
		{
			DEBUG_WARN("RfxMessageTileSet packet too small to read tile %d/%d", i, message->num_tiles);
			return FALSE;
		}

		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */

		if (Stream_GetRemainingLength(s) < blockLen - 6)
		{
			DEBUG_WARN("RfxMessageTileSet not enough bytes to read tile %d/%d with blocklen=%d", i, message->num_tiles, blockLen);
			return FALSE;
		}

		pos = Stream_GetPosition(s) - 6 + blockLen;

		if (blockType != CBT_TILE)
		{
			DEBUG_WARN("unknown block type 0x%X, expected CBT_TILE (0xCAC3).", blockType);
			break;
		}

		message->tiles[i] = rfx_tile_pool_take(context);

		if (context->priv->UseThreads)
		{
			params[i].context = context;
			params[i].tile = message->tiles[i];
			CopyMemory(&(params[i].s), s, sizeof(wStream));

			work_objects[i] = CreateThreadpoolWork((PTP_WORK_CALLBACK) rfx_process_message_tile_work_callback,
					(void*) &params[i], &context->priv->ThreadPoolEnv);

			SubmitThreadpoolWork(work_objects[i]);
		}
		else
		{
			rfx_process_message_tile(context, message->tiles[i], s);
		}

		Stream_SetPosition(s, pos);
	}

	if (context->priv->UseThreads)
	{
		for (i = 0; i < message->num_tiles; i++)
		{
			WaitForThreadpoolWorkCallbacks(work_objects[i], FALSE);
			CloseThreadpoolWork(work_objects[i]);
		}

		free(work_objects);
		free(params);
	}
	return TRUE;
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

	s = Stream_New(data, length);

	while (Stream_GetRemainingLength(s) > 6)
	{
		/* RFX_BLOCKT */
		Stream_Read_UINT16(s, blockType); /* blockType (2 bytes) */
		Stream_Read_UINT32(s, blockLen); /* blockLen (4 bytes) */

		DEBUG_RFX("blockType 0x%X blockLen %d", blockType, blockLen);

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
	return message->num_tiles;
}

RFX_TILE* rfx_message_get_tile(RFX_MESSAGE* message, int index)
{
	return message->tiles[index];
}

UINT16 rfx_message_get_rect_count(RFX_MESSAGE* message)
{
	return message->num_rects;
}

RFX_RECT* rfx_message_get_rect(RFX_MESSAGE* message, int index)
{
	return &message->rects[index];
}

void rfx_message_free(RFX_CONTEXT* context, RFX_MESSAGE* message)
{
	int i;

	if (message != NULL)
	{
		free(message->rects);

		if (message->tiles)
		{
			for (i = 0; i < message->num_tiles; i++)
				rfx_tile_pool_return(context, message->tiles[i]);

			free(message->tiles);
		}

		free(message);
	}
}

static void rfx_compose_message_sync(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_SYNC); /* BlockT.blockType */
	Stream_Write_UINT32(s, 12); /* BlockT.blockLen */
	Stream_Write_UINT32(s, WF_MAGIC); /* magic */
	Stream_Write_UINT16(s, WF_VERSION_1_0); /* version */
}

static void rfx_compose_message_codec_versions(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_CODEC_VERSIONS); /* BlockT.blockType */
	Stream_Write_UINT32(s, 10); /* BlockT.blockLen */
	Stream_Write_UINT8(s, 1); /* numCodecs */
	Stream_Write_UINT8(s, 1); /* codecs.codecId */
	Stream_Write_UINT16(s, WF_VERSION_1_0); /* codecs.version */
}

static void rfx_compose_message_channels(RFX_CONTEXT* context, wStream* s)
{
	Stream_Write_UINT16(s, WBT_CHANNELS); /* BlockT.blockType */
	Stream_Write_UINT32(s, 12); /* BlockT.blockLen */
	Stream_Write_UINT8(s, 1); /* numChannels */
	Stream_Write_UINT8(s, 0); /* Channel.channelId */
	Stream_Write_UINT16(s, context->width); /* Channel.width */
	Stream_Write_UINT16(s, context->height); /* Channel.height */
}

static void rfx_compose_message_context(RFX_CONTEXT* context, wStream* s)
{
	UINT16 properties;

	Stream_Write_UINT16(s, WBT_CONTEXT); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 13); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT8(s, 0); /* ctxId */
	Stream_Write_UINT16(s, CT_TILE_64x64); /* tileSize */

	/* properties */
	properties = context->flags; /* flags */
	properties |= (COL_CONV_ICT << 3); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 5); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 9); /* et */
	properties |= (SCALAR_QUANTIZATION << 13); /* qt */
	Stream_Write_UINT16(s, properties);

	/* properties in tilesets: note that this has different format from the one in TS_RFX_CONTEXT */
	properties = 1; /* lt */
	properties |= (context->flags << 1); /* flags */
	properties |= (COL_CONV_ICT << 4); /* cct */
	properties |= (CLW_XFORM_DWT_53_A << 6); /* xft */
	properties |= ((context->mode == RLGR1 ? CLW_ENTROPY_RLGR1 : CLW_ENTROPY_RLGR3) << 10); /* et */
	properties |= (SCALAR_QUANTIZATION << 14); /* qt */
	context->properties = properties;
}

void rfx_compose_message_header(RFX_CONTEXT* context, wStream* s)
{
	Stream_EnsureRemainingCapacity(s, 12 + 10 + 12 + 13);

	rfx_compose_message_sync(context, s);
	rfx_compose_message_context(context, s);
	rfx_compose_message_codec_versions(context, s);
	rfx_compose_message_channels(context, s);

	context->header_processed = TRUE;
}

static void rfx_compose_message_frame_begin(RFX_CONTEXT* context, wStream* s)
{
	Stream_EnsureRemainingCapacity(s, 14);

	Stream_Write_UINT16(s, WBT_FRAME_BEGIN); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 14); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT32(s, context->frame_idx); /* frameIdx */
	Stream_Write_UINT16(s, 1); /* numRegions */

	context->frame_idx++;
}

static void rfx_compose_message_region(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int num_rects)
{
	int size;
	int i;

	size = 15 + num_rects * 8;
	Stream_EnsureRemainingCapacity(s, size);

	Stream_Write_UINT16(s, WBT_REGION); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, size); /* set CodecChannelT.blockLen later */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT8(s, 1); /* regionFlags */
	Stream_Write_UINT16(s, num_rects); /* numRects */

	for (i = 0; i < num_rects; i++)
	{
		Stream_Write_UINT16(s, rects[i].x);
		Stream_Write_UINT16(s, rects[i].y);
		Stream_Write_UINT16(s, rects[i].width);
		Stream_Write_UINT16(s, rects[i].height);
	}

	Stream_Write_UINT16(s, CBT_REGION); /* regionType */
	Stream_Write_UINT16(s, 1); /* numTilesets */
}

static void rfx_compose_message_tile(RFX_CONTEXT* context, wStream* s,
	BYTE* tile_data, int tile_width, int tile_height, int rowstride,
	const UINT32* quantVals, int quantIdxY, int quantIdxCb, int quantIdxCr,
	int xIdx, int yIdx)
{
	int YLen = 0;
	int CbLen = 0;
	int CrLen = 0;
	int start_pos, end_pos;

	Stream_EnsureRemainingCapacity(s, 19);
	start_pos = Stream_GetPosition(s);

	Stream_Write_UINT16(s, CBT_TILE); /* BlockT.blockType */
	Stream_Seek_UINT32(s); /* set BlockT.blockLen later */
	Stream_Write_UINT8(s, quantIdxY);
	Stream_Write_UINT8(s, quantIdxCb);
	Stream_Write_UINT8(s, quantIdxCr);
	Stream_Write_UINT16(s, xIdx);
	Stream_Write_UINT16(s, yIdx);

	Stream_Seek(s, 6); /* YLen, CbLen, CrLen */

	rfx_encode_rgb(context, tile_data, tile_width, tile_height, rowstride,
		quantVals + quantIdxY * 10, quantVals + quantIdxCb * 10, quantVals + quantIdxCr * 10,
		s, &YLen, &CbLen, &CrLen);

	DEBUG_RFX("xIdx=%d yIdx=%d width=%d height=%d YLen=%d CbLen=%d CrLen=%d",
		xIdx, yIdx, tile_width, tile_height, YLen, CbLen, CrLen);

	end_pos = Stream_GetPosition(s);

	Stream_SetPosition(s, start_pos + 2);
	Stream_Write_UINT32(s, 19 + YLen + CbLen + CrLen); /* BlockT.blockLen */
	Stream_SetPosition(s, start_pos + 13);
	Stream_Write_UINT16(s, YLen);
	Stream_Write_UINT16(s, CbLen);
	Stream_Write_UINT16(s, CrLen);

	Stream_SetPosition(s, end_pos);
}

static void rfx_compose_message_tileset(RFX_CONTEXT* context, wStream* s,
	BYTE* image_data, int width, int height, int rowstride)
{
	int i;
	int size;
	int start_pos, end_pos;
	int numQuants;
	const UINT32* quantVals;
	const UINT32* quantValsPtr;
	int quantIdxY;
	int quantIdxCb;
	int quantIdxCr;
	int numTiles;
	int numTilesX;
	int numTilesY;
	int xIdx;
	int yIdx;
	int tilesDataSize;

	if (context->num_quants == 0)
	{
		numQuants = 1;
		quantVals = rfx_default_quantization_values;
		quantIdxY = 0;
		quantIdxCb = 0;
		quantIdxCr = 0;
	}
	else
	{
		numQuants = context->num_quants;
		quantVals = context->quants;
		quantIdxY = context->quant_idx_y;
		quantIdxCb = context->quant_idx_cb;
		quantIdxCr = context->quant_idx_cr;
	}

	numTilesX = (width + 63) / 64;
	numTilesY = (height + 63) / 64;
	numTiles = numTilesX * numTilesY;

	size = 22 + numQuants * 5;
	Stream_EnsureRemainingCapacity(s, size);
	start_pos = Stream_GetPosition(s);

	Stream_Write_UINT16(s, WBT_EXTENSION); /* CodecChannelT.blockType */
	Stream_Seek_UINT32(s); /* set CodecChannelT.blockLen later */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
	Stream_Write_UINT16(s, CBT_TILESET); /* subtype */
	Stream_Write_UINT16(s, 0); /* idx */
	Stream_Write_UINT16(s, context->properties); /* properties */
	Stream_Write_UINT8(s, numQuants); /* numQuants */
	Stream_Write_UINT8(s, 0x40); /* tileSize */
	Stream_Write_UINT16(s, numTiles); /* numTiles */
	Stream_Seek_UINT32(s); /* set tilesDataSize later */

	quantValsPtr = quantVals;
	for (i = 0; i < numQuants * 5; i++)
	{
		Stream_Write_UINT8(s, quantValsPtr[0] + (quantValsPtr[1] << 4));
		quantValsPtr += 2;
	}

	DEBUG_RFX("width:%d height:%d rowstride:%d", width, height, rowstride);

	end_pos = Stream_GetPosition(s);
	for (yIdx = 0; yIdx < numTilesY; yIdx++)
	{
		for (xIdx = 0; xIdx < numTilesX; xIdx++)
		{
			rfx_compose_message_tile(context, s,
				image_data + yIdx * 64 * rowstride + xIdx * 8 * context->bits_per_pixel,
				(xIdx < numTilesX - 1) ? 64 : width - xIdx * 64,
				(yIdx < numTilesY - 1) ? 64 : height - yIdx * 64,
				rowstride, quantVals, quantIdxY, quantIdxCb, quantIdxCr, xIdx, yIdx);
		}
	}
	tilesDataSize = Stream_GetPosition(s) - end_pos;
	size += tilesDataSize;
	end_pos = Stream_GetPosition(s);

	Stream_SetPosition(s, start_pos + 2);
	Stream_Write_UINT32(s, size); /* CodecChannelT.blockLen */
	Stream_SetPosition(s, start_pos + 18);
	Stream_Write_UINT32(s, tilesDataSize);

	Stream_SetPosition(s, end_pos);
}

static void rfx_compose_message_frame_end(RFX_CONTEXT* context, wStream* s)
{
	Stream_EnsureRemainingCapacity(s, 8);

	Stream_Write_UINT16(s, WBT_FRAME_END); /* CodecChannelT.blockType */
	Stream_Write_UINT32(s, 8); /* CodecChannelT.blockLen */
	Stream_Write_UINT8(s, 1); /* CodecChannelT.codecId */
	Stream_Write_UINT8(s, 0); /* CodecChannelT.channelId */
}

static void rfx_compose_message_data(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int num_rects, BYTE* image_data, int width, int height, int rowstride)
{
	rfx_compose_message_frame_begin(context, s);
	rfx_compose_message_region(context, s, rects, num_rects);
	rfx_compose_message_tileset(context, s, image_data, width, height, rowstride);
	rfx_compose_message_frame_end(context, s);
}

FREERDP_API void rfx_compose_message(RFX_CONTEXT* context, wStream* s,
	const RFX_RECT* rects, int num_rects, BYTE* image_data, int width, int height, int rowstride)
{
	/* Only the first frame should send the RemoteFX header */
	if (context->frame_idx == 0 && !context->header_processed)
		rfx_compose_message_header(context, s);

	rfx_compose_message_data(context, s, rects, num_rects, image_data, width, height, rowstride);
}

