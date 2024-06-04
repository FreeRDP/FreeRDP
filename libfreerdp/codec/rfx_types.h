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

#ifndef FREERDP_LIB_CODEC_RFX_TYPES_H
#define FREERDP_LIB_CODEC_RFX_TYPES_H

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/log.h>
#include <freerdp/utils/profiler.h>

#define RFX_TAG FREERDP_TAG("codec.rfx")
#ifdef WITH_DEBUG_RFX
#define DEBUG_RFX(...) WLog_DBG(RFX_TAG, __VA_ARGS__)
#else
#define DEBUG_RFX(...) \
	do                 \
	{                  \
	} while (0)
#endif

#define RFX_DECODED_SYNC 0x00000001
#define RFX_DECODED_CONTEXT 0x00000002
#define RFX_DECODED_VERSIONS 0x00000004
#define RFX_DECODED_CHANNELS 0x00000008
#define RFX_DECODED_HEADERS 0x0000000F

typedef enum
{
	RFX_STATE_INITIAL,
	RFX_STATE_SERVER_UNINITIALIZED,
	RFX_STATE_SEND_HEADERS,
	RFX_STATE_SEND_FRAME_DATA,
	RFX_STATE_FRAME_DATA_SENT,
	RFX_STATE_FINAL
} RFX_STATE;

typedef struct S_RFX_TILE_COMPOSE_WORK_PARAM RFX_TILE_COMPOSE_WORK_PARAM;

typedef struct S_RFX_CONTEXT_PRIV RFX_CONTEXT_PRIV;
struct S_RFX_CONTEXT_PRIV
{
	wLog* log;
	wObjectPool* TilePool;

	BOOL UseThreads;
	PTP_WORK* workObjects;
	RFX_TILE_COMPOSE_WORK_PARAM* tileWorkParams;

	DWORD MinThreadCount;
	DWORD MaxThreadCount;

	PTP_POOL ThreadPool;
	TP_CALLBACK_ENVIRON ThreadPoolEnv;

	wBufferPool* BufferPool;

	/* profilers */
	PROFILER_DEFINE(prof_rfx_decode_rgb)
	PROFILER_DEFINE(prof_rfx_decode_component)
	PROFILER_DEFINE(prof_rfx_rlgr_decode)
	PROFILER_DEFINE(prof_rfx_differential_decode)
	PROFILER_DEFINE(prof_rfx_quantization_decode)
	PROFILER_DEFINE(prof_rfx_dwt_2d_decode)
	PROFILER_DEFINE(prof_rfx_ycbcr_to_rgb)

	PROFILER_DEFINE(prof_rfx_encode_rgb)
	PROFILER_DEFINE(prof_rfx_encode_component)
	PROFILER_DEFINE(prof_rfx_rlgr_encode)
	PROFILER_DEFINE(prof_rfx_differential_encode)
	PROFILER_DEFINE(prof_rfx_quantization_encode)
	PROFILER_DEFINE(prof_rfx_dwt_2d_encode)
	PROFILER_DEFINE(prof_rfx_rgb_to_ycbcr)
	PROFILER_DEFINE(prof_rfx_encode_format_rgb)
};

struct S_RFX_MESSAGE
{
	UINT32 frameIdx;

	/**
	 * The rects array represents the updated region of the frame. The UI
	 * requires to clip drawing destination base on the union of the rects.
	 */
	UINT16 numRects;
	RFX_RECT* rects;

	/**
	 * The tiles array represents the actual frame data. Each tile is always
	 * 64x64. Note that only pixels inside the updated region (represented as
	 * rects described above) are valid. Pixels outside of the region may
	 * contain arbitrary data.
	 */
	UINT16 numTiles;
	size_t allocatedTiles;
	RFX_TILE** tiles;

	UINT16 numQuant;
	UINT32* quantVals;

	UINT32 tilesDataSize;

	BOOL freeArray;
};

struct S_RFX_MESSAGE_LIST
{
	struct S_RFX_MESSAGE* list;
	size_t count;
	RFX_CONTEXT* context;
};

struct S_RFX_CONTEXT
{
	RFX_STATE state;

	BOOL encoder;
	UINT16 flags;
	UINT16 properties;
	UINT16 width;
	UINT16 height;
	RLGR_MODE mode;
	UINT32 version;
	UINT32 codec_id;
	UINT32 codec_version;
	UINT32 pixel_format;
	BYTE bits_per_pixel;

	/* color palette allocated by the application */
	const BYTE* palette;

	/* temporary data within a frame */
	UINT32 frameIdx;
	BYTE numQuant;
	UINT32* quants;
	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;

	/* decoded header blocks */
	UINT32 decodedHeaderBlocks;
	UINT16 expectedDataBlockType;
	struct S_RFX_MESSAGE currentMessage;

	/* routines */
	void (*quantization_decode)(INT16* WINPR_RESTRICT buffer,
	                            const UINT32* WINPR_RESTRICT quantization_values);
	void (*quantization_encode)(INT16* WINPR_RESTRICT buffer,
	                            const UINT32* WINPR_RESTRICT quantization_values);
	void (*dwt_2d_decode)(INT16* WINPR_RESTRICT buffer, INT16* WINPR_RESTRICT dwt_buffer);
	void (*dwt_2d_extrapolate_decode)(INT16* WINPR_RESTRICT src, INT16* WINPR_RESTRICT temp);
	void (*dwt_2d_encode)(INT16* WINPR_RESTRICT buffer, INT16* WINPR_RESTRICT dwt_buffer);
	int (*rlgr_decode)(RLGR_MODE mode, const BYTE* WINPR_RESTRICT data, UINT32 data_size,
	                   INT16* WINPR_RESTRICT buffer, UINT32 buffer_size);
	int (*rlgr_encode)(RLGR_MODE mode, const INT16* WINPR_RESTRICT data, UINT32 data_size,
	                   BYTE* WINPR_RESTRICT buffer, UINT32 buffer_size);

	/* private definitions */
	RFX_CONTEXT_PRIV* priv;
};

#endif /* FREERDP_LIB_CODEC_RFX_TYPES_H */
