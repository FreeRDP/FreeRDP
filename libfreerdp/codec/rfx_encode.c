/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Encode
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

#include <winpr/crt.h>
#include <winpr/collections.h>

#include <freerdp/primitives.h>

#include "rfx_types.h"
#include "rfx_rlgr.h"
#include "rfx_differential.h"
#include "rfx_quantization.h"
#include "rfx_dwt.h"

#include "rfx_encode.h"

static void rfx_encode_format_rgb(const BYTE* WINPR_RESTRICT rgb_data, uint32_t width,
                                  uint32_t height, uint32_t rowstride, UINT32 pixel_format,
                                  const BYTE* WINPR_RESTRICT palette, INT16* WINPR_RESTRICT r_buf,
                                  INT16* WINPR_RESTRICT g_buf, INT16* WINPR_RESTRICT b_buf)
{
	const BYTE* src = NULL;
	INT16 r = 0;
	INT16 g = 0;
	INT16 b = 0;
	INT16* r_last = NULL;
	INT16* g_last = NULL;
	INT16* b_last = NULL;
	uint32_t x_exceed = 64 - width;
	uint32_t y_exceed = 64 - height;

	for (uint32_t y = 0; y < height; y++)
	{
		src = rgb_data + 1ULL * y * rowstride;

		switch (pixel_format)
		{
			case PIXEL_FORMAT_BGRX32:
			case PIXEL_FORMAT_BGRA32:
				for (uint32_t x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
					src++;
				}

				break;

			case PIXEL_FORMAT_XBGR32:
			case PIXEL_FORMAT_ABGR32:
				for (size_t x = 0; x < width; x++)
				{
					src++;
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_RGBX32:
			case PIXEL_FORMAT_RGBA32:
				for (size_t x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
					src++;
				}

				break;

			case PIXEL_FORMAT_XRGB32:
			case PIXEL_FORMAT_ARGB32:
				for (size_t x = 0; x < width; x++)
				{
					src++;
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_BGR24:
				for (size_t x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*r_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_RGB24:
				for (size_t x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(*src++);
					*g_buf++ = (INT16)(*src++);
					*b_buf++ = (INT16)(*src++);
				}

				break;

			case PIXEL_FORMAT_BGR16:
				for (size_t x = 0; x < width; x++)
				{
					*b_buf++ = (INT16)(((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					*g_buf++ = (INT16)((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					*r_buf++ = (INT16)((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					src += 2;
				}

				break;

			case PIXEL_FORMAT_RGB16:
				for (size_t x = 0; x < width; x++)
				{
					*r_buf++ = (INT16)(((*(src + 1)) & 0xF8) | ((*(src + 1)) >> 5));
					*g_buf++ = (INT16)((((*(src + 1)) & 0x07) << 5) | (((*src) & 0xE0) >> 3));
					*b_buf++ = (INT16)((((*src) & 0x1F) << 3) | (((*src) >> 2) & 0x07));
					src += 2;
				}

				break;

			case PIXEL_FORMAT_RGB8:
				if (!palette)
					break;

				for (size_t x = 0; x < width; x++)
				{
					BYTE idx = 0;
					const size_t shift = (7 - (x % 8));
					idx = ((*src) >> shift) & 1;
					idx |= (((*(src + 1)) >> shift) & 1) << 1;
					idx |= (((*(src + 2)) >> shift) & 1) << 2;
					idx |= (((*(src + 3)) >> shift) & 1) << 3;
					idx *= 3;
					*r_buf++ = (INT16)palette[idx];
					*g_buf++ = (INT16)palette[idx + 1];
					*b_buf++ = (INT16)palette[idx + 2];

					if (shift == 0)
						src += 4;
				}

				break;

			case PIXEL_FORMAT_A4:
				if (!palette)
					break;

				for (size_t x = 0; x < width; x++)
				{
					int idx = (*src) * 3;
					*r_buf++ = (INT16)palette[idx];
					*g_buf++ = (INT16)palette[idx + 1];
					*b_buf++ = (INT16)palette[idx + 2];
					src++;
				}

				break;

			default:
				break;
		}

		/* Fill the horizontal region outside of 64x64 tile size with the right-most pixel for best
		 * quality */
		if (x_exceed > 0)
		{
			r = *(r_buf - 1);
			g = *(g_buf - 1);
			b = *(b_buf - 1);

			for (size_t x = 0; x < x_exceed; x++)
			{
				*r_buf++ = r;
				*g_buf++ = g;
				*b_buf++ = b;
			}
		}
	}

	/* Fill the vertical region outside of 64x64 tile size with the last line. */
	if (y_exceed > 0)
	{
		r_last = r_buf - 64;
		g_last = g_buf - 64;
		b_last = b_buf - 64;

		while (y_exceed > 0)
		{
			CopyMemory(r_buf, r_last, 64 * sizeof(INT16));
			CopyMemory(g_buf, g_last, 64 * sizeof(INT16));
			CopyMemory(b_buf, b_last, 64 * sizeof(INT16));
			r_buf += 64;
			g_buf += 64;
			b_buf += 64;
			y_exceed--;
		}
	}
}

/* rfx_encode_rgb_to_ycbcr code now resides in the primitives library. */

static void rfx_encode_component(RFX_CONTEXT* WINPR_RESTRICT context,
                                 const UINT32* WINPR_RESTRICT quantization_values,
                                 INT16* WINPR_RESTRICT data, BYTE* WINPR_RESTRICT buffer,
                                 uint32_t buffer_size, uint32_t* WINPR_RESTRICT size)
{
	INT16* dwt_buffer = BufferPool_Take(context->priv->BufferPool, -1); /* dwt_buffer */
	PROFILER_ENTER(context->priv->prof_rfx_encode_component)
	PROFILER_ENTER(context->priv->prof_rfx_dwt_2d_encode)
	context->dwt_2d_encode(data, dwt_buffer);
	PROFILER_EXIT(context->priv->prof_rfx_dwt_2d_encode)
	PROFILER_ENTER(context->priv->prof_rfx_quantization_encode)
	context->quantization_encode(data, quantization_values);
	PROFILER_EXIT(context->priv->prof_rfx_quantization_encode)
	PROFILER_ENTER(context->priv->prof_rfx_differential_encode)
	rfx_differential_encode(data + 4032, 64);
	PROFILER_EXIT(context->priv->prof_rfx_differential_encode)
	PROFILER_ENTER(context->priv->prof_rfx_rlgr_encode)
	const int rc = context->rlgr_encode(context->mode, data, 4096, buffer, buffer_size);
	PROFILER_EXIT(context->priv->prof_rfx_rlgr_encode)
	PROFILER_EXIT(context->priv->prof_rfx_encode_component)
	BufferPool_Return(context->priv->BufferPool, dwt_buffer);

	*size = WINPR_ASSERTING_INT_CAST(uint32_t, rc);
}

void rfx_encode_rgb(RFX_CONTEXT* WINPR_RESTRICT context, RFX_TILE* WINPR_RESTRICT tile)
{
	union
	{
		const INT16** cpv;
		INT16** pv;
	} cnv;
	BYTE* pBuffer = NULL;
	INT16* pSrcDst[3];
	uint32_t YLen = 0;
	uint32_t CbLen = 0;
	uint32_t CrLen = 0;
	UINT32* YQuant = NULL;
	UINT32* CbQuant = NULL;
	UINT32* CrQuant = NULL;
	primitives_t* prims = primitives_get();
	static const prim_size_t roi_64x64 = { 64, 64 };

	if (!(pBuffer = (BYTE*)BufferPool_Take(context->priv->BufferPool, -1)))
		return;

	YLen = CbLen = CrLen = 0;
	YQuant = context->quants + (10ULL * tile->quantIdxY);
	CbQuant = context->quants + (10ULL * tile->quantIdxCb);
	CrQuant = context->quants + (10ULL * tile->quantIdxCr);
	pSrcDst[0] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 0ULL) + 16ULL])); /* y_r_buffer */
	pSrcDst[1] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 1ULL) + 16ULL])); /* cb_g_buffer */
	pSrcDst[2] = (INT16*)((&pBuffer[((8192ULL + 32ULL) * 2ULL) + 16ULL])); /* cr_b_buffer */
	PROFILER_ENTER(context->priv->prof_rfx_encode_rgb)
	PROFILER_ENTER(context->priv->prof_rfx_encode_format_rgb)
	rfx_encode_format_rgb(tile->data, tile->width, tile->height, tile->scanline,
	                      context->pixel_format, context->palette, pSrcDst[0], pSrcDst[1],
	                      pSrcDst[2]);
	PROFILER_EXIT(context->priv->prof_rfx_encode_format_rgb)
	PROFILER_ENTER(context->priv->prof_rfx_rgb_to_ycbcr)

	cnv.pv = pSrcDst;
	prims->RGBToYCbCr_16s16s_P3P3(cnv.cpv, 64 * sizeof(INT16), pSrcDst, 64 * sizeof(INT16),
	                              &roi_64x64);
	PROFILER_EXIT(context->priv->prof_rfx_rgb_to_ycbcr)
	/**
	 * We need to clear the buffers as the RLGR encoder expects it to be initialized to zero.
	 * This allows simplifying and improving the performance of the encoding process.
	 */
	ZeroMemory(tile->YData, 4096);
	ZeroMemory(tile->CbData, 4096);
	ZeroMemory(tile->CrData, 4096);
	rfx_encode_component(context, YQuant, pSrcDst[0], tile->YData, 4096, &YLen);
	rfx_encode_component(context, CbQuant, pSrcDst[1], tile->CbData, 4096, &CbLen);
	rfx_encode_component(context, CrQuant, pSrcDst[2], tile->CrData, 4096, &CrLen);
	tile->YLen = WINPR_ASSERTING_INT_CAST(UINT16, YLen);
	tile->CbLen = WINPR_ASSERTING_INT_CAST(UINT16, CbLen);
	tile->CrLen = WINPR_ASSERTING_INT_CAST(UINT16, CrLen);
	PROFILER_EXIT(context->priv->prof_rfx_encode_rgb)
	BufferPool_Return(context->priv->BufferPool, pBuffer);
}
