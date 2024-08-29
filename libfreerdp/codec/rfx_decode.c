/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Decode
 *
 * Copyright 2011 Vic Lee
 * Copyright 2011 Norbert Federa <norbert.federa@thincast.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/stream.h>
#include <freerdp/primitives.h>

#include "rfx_types.h"
#include "rfx_rlgr.h"
#include "rfx_differential.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"

#include "rfx_decode.h"

static INLINE void rfx_decode_component(RFX_CONTEXT* WINPR_RESTRICT context,
                                        const UINT32* WINPR_RESTRICT quantization_values,
                                        const BYTE* WINPR_RESTRICT data, size_t size,
                                        INT16* WINPR_RESTRICT buffer)
{
	INT16* dwt_buffer = NULL;
	dwt_buffer = BufferPool_Take(context->priv->BufferPool, -1); /* dwt_buffer */
	PROFILER_ENTER(context->priv->prof_rfx_decode_component)
	PROFILER_ENTER(context->priv->prof_rfx_rlgr_decode)
	context->rlgr_decode(context->mode, data, size, buffer, 4096);
	PROFILER_EXIT(context->priv->prof_rfx_rlgr_decode)
	PROFILER_ENTER(context->priv->prof_rfx_differential_decode)
	rfx_differential_decode(buffer + 4032, 64);
	PROFILER_EXIT(context->priv->prof_rfx_differential_decode)
	PROFILER_ENTER(context->priv->prof_rfx_quantization_decode)
	context->quantization_decode(buffer, quantization_values);
	PROFILER_EXIT(context->priv->prof_rfx_quantization_decode)
	PROFILER_ENTER(context->priv->prof_rfx_dwt_2d_decode)
	context->dwt_2d_decode(buffer, dwt_buffer);
	PROFILER_EXIT(context->priv->prof_rfx_dwt_2d_decode)
	PROFILER_EXIT(context->priv->prof_rfx_decode_component)
	BufferPool_Return(context->priv->BufferPool, dwt_buffer);
}

/* rfx_decode_ycbcr_to_rgb code now resides in the primitives library. */

/* stride is bytes between rows in the output buffer. */
BOOL rfx_decode_rgb(RFX_CONTEXT* WINPR_RESTRICT context, const RFX_TILE* WINPR_RESTRICT tile,
                    BYTE* WINPR_RESTRICT rgb_buffer, UINT32 stride)
{
	union
	{
		const INT16** cpv;
		INT16** pv;
	} cnv;
	BOOL rc = TRUE;
	BYTE* pBuffer = NULL;
	INT16* pSrcDst[3];
	UINT32* y_quants = NULL;
	UINT32* cb_quants = NULL;
	UINT32* cr_quants = NULL;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t* prims = primitives_get();
	PROFILER_ENTER(context->priv->prof_rfx_decode_rgb)
	y_quants = context->quants + (10ULL * tile->quantIdxY);
	cb_quants = context->quants + (10ULL * tile->quantIdxCb);
	cr_quants = context->quants + (10ULL * tile->quantIdxCr);
	pBuffer = (BYTE*)BufferPool_Take(context->priv->BufferPool, -1);
	pSrcDst[0] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 0ULL) + 16ULL]));        /* y_r_buffer */
	pSrcDst[1] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 1ULL) + 16ULL]));        /* cb_g_buffer */
	pSrcDst[2] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 2ULL) + 16ULL]));        /* cr_b_buffer */
	rfx_decode_component(context, y_quants, tile->YData, tile->YLen, pSrcDst[0]); /* YData */
	rfx_decode_component(context, cb_quants, tile->CbData, tile->CbLen, pSrcDst[1]); /* CbData */
	rfx_decode_component(context, cr_quants, tile->CrData, tile->CrLen, pSrcDst[2]); /* CrData */
	PROFILER_ENTER(context->priv->prof_rfx_ycbcr_to_rgb)

	cnv.pv = pSrcDst;
	if (prims->yCbCrToRGB_16s8u_P3AC4R(cnv.cpv, 64 * sizeof(INT16), rgb_buffer, stride,
	                                   context->pixel_format, &roi_64x64) != PRIMITIVES_SUCCESS)
		rc = FALSE;

	PROFILER_EXIT(context->priv->prof_rfx_ycbcr_to_rgb)
	PROFILER_EXIT(context->priv->prof_rfx_decode_rgb)
	BufferPool_Return(context->priv->BufferPool, pBuffer);
	return rc;
}
