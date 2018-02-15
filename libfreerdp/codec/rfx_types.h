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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/pool.h>
#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/log.h>
#include <freerdp/utils/profiler.h>

#define RFX_TAG FREERDP_TAG("codec.rfx")
#ifdef WITH_DEBUG_RFX
#define DEBUG_RFX(...) WLog_DBG(RFX_TAG, __VA_ARGS__)
#else
#define DEBUG_RFX(...) do { } while (0)
#endif

typedef struct _RFX_TILE_COMPOSE_WORK_PARAM RFX_TILE_COMPOSE_WORK_PARAM;

struct _RFX_CONTEXT_PRIV
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

#endif /* FREERDP_LIB_CODEC_RFX_TYPES_H */
