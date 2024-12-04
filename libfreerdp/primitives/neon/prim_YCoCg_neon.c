/* FreeRDP: A Remote Desktop Protocol Client
 * Optimized YCoCg<->RGB conversion operations.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include <winpr/sysinfo.h>

#include "prim_internal.h"
#include "prim_templates.h"
#include "prim_YCoCg.h"

#if defined(NEON_INTRINSICS_ENABLED)
#include <arm_neon.h>

static primitives_t* generic = NULL;

static pstatus_t neon_YCoCgToRGB_8u_X(const BYTE* WINPR_RESTRICT pSrc, INT32 srcStep,
                                      BYTE* WINPR_RESTRICT pDst, UINT32 DstFormat, INT32 dstStep,
                                      UINT32 width, UINT32 height, UINT8 shift, BYTE bPos,
                                      BYTE gPos, BYTE rPos, BYTE aPos, BOOL alpha)
{
	BYTE* dptr = pDst;
	const BYTE* sptr = pSrc;
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);
	const int8_t cll = shift - 1; /* -1 builds in the /2's */
	const UINT32 srcPad = srcStep - (width * 4);
	const UINT32 dstPad = dstStep - (width * formatSize);
	const UINT32 pad = width % 8;
	const uint8x8_t aVal = vdup_n_u8(0xFF);
	const int8x8_t cllv = vdup_n_s8(cll);

	for (UINT32 y = 0; y < height; y++)
	{
		for (UINT32 x = 0; x < width - pad; x += 8)
		{
			/* Note: shifts must be done before sign-conversion. */
			const uint8x8x4_t raw = vld4_u8(sptr);
			const int8x8_t CgRaw = vreinterpret_s8_u8(vshl_u8(raw.val[0], cllv));
			const int8x8_t CoRaw = vreinterpret_s8_u8(vshl_u8(raw.val[1], cllv));
			const int16x8_t Cg = vmovl_s8(CgRaw);
			const int16x8_t Co = vmovl_s8(CoRaw);
			const int16x8_t Y = vreinterpretq_s16_u16(vmovl_u8(raw.val[2])); /* UINT8 -> INT16 */
			const int16x8_t T = vsubq_s16(Y, Cg);
			const int16x8_t R = vaddq_s16(T, Co);
			const int16x8_t G = vaddq_s16(Y, Cg);
			const int16x8_t B = vsubq_s16(T, Co);
			uint8x8x4_t bgrx;
			bgrx.val[bPos] = vqmovun_s16(B);
			bgrx.val[gPos] = vqmovun_s16(G);
			bgrx.val[rPos] = vqmovun_s16(R);

			if (alpha)
				bgrx.val[aPos] = raw.val[3];
			else
				bgrx.val[aPos] = aVal;

			vst4_u8(dptr, bgrx);
			sptr += sizeof(raw);
			dptr += sizeof(bgrx);
		}

		for (UINT32 x = 0; x < pad; x++)
		{
			/* Note: shifts must be done before sign-conversion. */
			const INT16 Cg = (INT16)((INT8)((*sptr++) << cll));
			const INT16 Co = (INT16)((INT8)((*sptr++) << cll));
			const INT16 Y = (INT16)(*sptr++); /* UINT8->INT16 */
			const INT16 T = Y - Cg;
			const INT16 R = T + Co;
			const INT16 G = Y + Cg;
			const INT16 B = T - Co;
			BYTE bgra[4];
			bgra[bPos] = CLIP(B);
			bgra[gPos] = CLIP(G);
			bgra[rPos] = CLIP(R);
			bgra[aPos] = *sptr++;

			if (!alpha)
				bgra[aPos] = 0xFF;

			*dptr++ = bgra[0];
			*dptr++ = bgra[1];
			*dptr++ = bgra[2];
			*dptr++ = bgra[3];
		}

		sptr += srcPad;
		dptr += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

static pstatus_t neon_YCoCgToRGB_8u_AC4R(const BYTE* WINPR_RESTRICT pSrc, INT32 srcStep,
                                         BYTE* WINPR_RESTRICT pDst, UINT32 DstFormat, INT32 dstStep,
                                         UINT32 width, UINT32 height, UINT8 shift, BOOL withAlpha)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 2, 1, 0, 3, withAlpha);

		case PIXEL_FORMAT_BGRX32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 2, 1, 0, 3, withAlpha);

		case PIXEL_FORMAT_RGBA32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 0, 1, 2, 3, withAlpha);

		case PIXEL_FORMAT_RGBX32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 0, 1, 2, 3, withAlpha);

		case PIXEL_FORMAT_ARGB32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 1, 2, 3, 0, withAlpha);

		case PIXEL_FORMAT_XRGB32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 1, 2, 3, 0, withAlpha);

		case PIXEL_FORMAT_ABGR32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 3, 2, 1, 0, withAlpha);

		case PIXEL_FORMAT_XBGR32:
			return neon_YCoCgToRGB_8u_X(pSrc, srcStep, pDst, DstFormat, dstStep, width, height,
			                            shift, 3, 2, 1, 0, withAlpha);

		default:
			return generic->YCoCgToRGB_8u_AC4R(pSrc, srcStep, pDst, DstFormat, dstStep, width,
			                                   height, shift, withAlpha);
	}
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg_neon(primitives_t* WINPR_RESTRICT prims)
{
#if defined(NEON_INTRINSICS_ENABLED)
	generic = primitives_get_generic();
	primitives_init_YCoCg(prims);

	if (IsProcessorFeaturePresent(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE))
	{
		WLog_VRB(PRIM_TAG, "NEON optimizations");
		prims->YCoCgToRGB_8u_AC4R = neon_YCoCgToRGB_8u_AC4R;
	}
#else
	WLog_VRB(PRIM_TAG, "undefined WITH_SIMD or neon intrinsics not available");
	WINPR_UNUSED(prims);
#endif
}
