/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Library
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
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

#ifndef FREERDP_LIB_CODEC_NSC_TYPES_H
#define FREERDP_LIB_CODEC_NSC_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/collections.h>


#include <freerdp/utils/profiler.h>

#define ROUND_UP_TO(_b, _n) (_b + ((~(_b & (_n-1)) + 0x1) & (_n-1)))
#define MINMAX(_v,_l,_h) ((_v) < (_l) ? (_l) : ((_v) > (_h) ? (_h) : (_v)))

struct _NSC_CONTEXT_PRIV
{
	wLog* log;

	wBufferPool* PlanePool;

	BYTE* PlaneBuffers[5];		/* Decompressed Plane Buffers in the respective order */
	UINT32 PlaneBuffersLength;	/* Lengths of each plane buffer */

	/* profilers */
	PROFILER_DEFINE(prof_nsc_rle_decompress_data)
	PROFILER_DEFINE(prof_nsc_decode)
	PROFILER_DEFINE(prof_nsc_rle_compress_data)
	PROFILER_DEFINE(prof_nsc_encode)
};

#endif /* FREERDP_LIB_CODEC_NSC_TYPES_H */
