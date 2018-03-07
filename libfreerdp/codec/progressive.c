/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Progressive Codec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/primitives.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/progressive.h>
#include <freerdp/codec/region.h>
#include <freerdp/log.h>

#include "rfx_differential.h"
#include "rfx_quantization.h"
#include "rfx_rlgr.h"
#include "progressive.h"

#define TAG FREERDP_TAG("codec.progressive")

struct _RFX_PROGRESSIVE_UPGRADE_STATE
{
	BOOL nonLL;
	wBitStream* srl;
	wBitStream* raw;

	/* SRL state */

	int kp;
	int nz;
	BOOL mode;
};
typedef struct _RFX_PROGRESSIVE_UPGRADE_STATE RFX_PROGRESSIVE_UPGRADE_STATE;

static const char* progressive_get_block_type_string(UINT16 blockType)
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

	return "PROGRESSIVE_WBT_UNKNOWN";
}

static INLINE void progressive_component_codec_quant_read(const BYTE* block,
        RFX_COMPONENT_CODEC_QUANT* quantVal)
{
	quantVal->LL3 = block[0] & 0x0F;
	quantVal->HL3 = block[0] >> 4;
	quantVal->LH3 = block[1] & 0x0F;
	quantVal->HH3 = block[1] >> 4;
	quantVal->HL2 = block[2] & 0x0F;
	quantVal->LH2 = block[2] >> 4;
	quantVal->HH2 = block[3] & 0x0F;
	quantVal->HL1 = block[3] >> 4;
	quantVal->LH1 = block[4] & 0x0F;
	quantVal->HH1 = block[4] >> 4;
}

static INLINE void progressive_rfx_quant_ladd(RFX_COMPONENT_CODEC_QUANT* q, int val)
{
	q->HL1 += val; /* HL1 */
	q->LH1 += val; /* LH1 */
	q->HH1 += val; /* HH1 */
	q->HL2 += val; /* HL2 */
	q->LH2 += val; /* LH2 */
	q->HH2 += val; /* HH2 */
	q->HL3 += val; /* HL3 */
	q->LH3 += val; /* LH3 */
	q->HH3 += val; /* HH3 */
	q->LL3 += val; /* LL3 */
}

static INLINE void progressive_rfx_quant_add(RFX_COMPONENT_CODEC_QUANT* q1,
        RFX_COMPONENT_CODEC_QUANT* q2,
        RFX_COMPONENT_CODEC_QUANT* dst)
{
	dst->HL1 = q1->HL1 + q2->HL1; /* HL1 */
	dst->LH1 = q1->LH1 + q2->LH1; /* LH1 */
	dst->HH1 = q1->HH1 + q2->HH1; /* HH1 */
	dst->HL2 = q1->HL2 + q2->HL2; /* HL2 */
	dst->LH2 = q1->LH2 + q2->LH2; /* LH2 */
	dst->HH2 = q1->HH2 + q2->HH2; /* HH2 */
	dst->HL3 = q1->HL3 + q2->HL3; /* HL3 */
	dst->LH3 = q1->LH3 + q2->LH3; /* LH3 */
	dst->HH3 = q1->HH3 + q2->HH3; /* HH3 */
	dst->LL3 = q1->LL3 + q2->LL3; /* LL3 */
}

static INLINE void progressive_rfx_quant_lsub(RFX_COMPONENT_CODEC_QUANT* q, int val)
{
	q->HL1 -= val; /* HL1 */
	q->LH1 -= val; /* LH1 */
	q->HH1 -= val; /* HH1 */
	q->HL2 -= val; /* HL2 */
	q->LH2 -= val; /* LH2 */
	q->HH2 -= val; /* HH2 */
	q->HL3 -= val; /* HL3 */
	q->LH3 -= val; /* LH3 */
	q->HH3 -= val; /* HH3 */
	q->LL3 -= val; /* LL3 */
}

static INLINE void progressive_rfx_quant_sub(RFX_COMPONENT_CODEC_QUANT* q1,
        RFX_COMPONENT_CODEC_QUANT* q2,
        RFX_COMPONENT_CODEC_QUANT* dst)
{
	dst->HL1 = q1->HL1 - q2->HL1; /* HL1 */
	dst->LH1 = q1->LH1 - q2->LH1; /* LH1 */
	dst->HH1 = q1->HH1 - q2->HH1; /* HH1 */
	dst->HL2 = q1->HL2 - q2->HL2; /* HL2 */
	dst->LH2 = q1->LH2 - q2->LH2; /* LH2 */
	dst->HH2 = q1->HH2 - q2->HH2; /* HH2 */
	dst->HL3 = q1->HL3 - q2->HL3; /* HL3 */
	dst->LH3 = q1->LH3 - q2->LH3; /* LH3 */
	dst->HH3 = q1->HH3 - q2->HH3; /* HH3 */
	dst->LL3 = q1->LL3 - q2->LL3; /* LL3 */
}

static INLINE BOOL progressive_rfx_quant_lcmp_less_equal(RFX_COMPONENT_CODEC_QUANT* q,
        int val)
{
	if (q->HL1 > val) return FALSE; /* HL1 */

	if (q->LH1 > val) return FALSE; /* LH1 */

	if (q->HH1 > val) return FALSE; /* HH1 */

	if (q->HL2 > val) return FALSE; /* HL2 */

	if (q->LH2 > val) return FALSE; /* LH2 */

	if (q->HH2 > val) return FALSE; /* HH2 */

	if (q->HL3 > val) return FALSE; /* HL3 */

	if (q->LH3 > val) return FALSE; /* LH3 */

	if (q->HH3 > val) return FALSE; /* HH3 */

	if (q->LL3 > val) return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_less_equal(RFX_COMPONENT_CODEC_QUANT* q1,
        RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 > q2->HL1) return FALSE; /* HL1 */

	if (q1->LH1 > q2->LH1) return FALSE; /* LH1 */

	if (q1->HH1 > q2->HH1) return FALSE; /* HH1 */

	if (q1->HL2 > q2->HL2) return FALSE; /* HL2 */

	if (q1->LH2 > q2->LH2) return FALSE; /* LH2 */

	if (q1->HH2 > q2->HH2) return FALSE; /* HH2 */

	if (q1->HL3 > q2->HL3) return FALSE; /* HL3 */

	if (q1->LH3 > q2->LH3) return FALSE; /* LH3 */

	if (q1->HH3 > q2->HH3) return FALSE; /* HH3 */

	if (q1->LL3 > q2->LL3) return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_lcmp_greater_equal(RFX_COMPONENT_CODEC_QUANT* q,
        int val)
{
	if (q->HL1 < val) return FALSE; /* HL1 */

	if (q->LH1 < val) return FALSE; /* LH1 */

	if (q->HH1 < val) return FALSE; /* HH1 */

	if (q->HL2 < val) return FALSE; /* HL2 */

	if (q->LH2 < val) return FALSE; /* LH2 */

	if (q->HH2 < val) return FALSE; /* HH2 */

	if (q->HL3 < val) return FALSE; /* HL3 */

	if (q->LH3 < val) return FALSE; /* LH3 */

	if (q->HH3 < val) return FALSE; /* HH3 */

	if (q->LL3 < val) return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_greater_equal(RFX_COMPONENT_CODEC_QUANT* q1,
        RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 < q2->HL1) return FALSE; /* HL1 */

	if (q1->LH1 < q2->LH1) return FALSE; /* LH1 */

	if (q1->HH1 < q2->HH1) return FALSE; /* HH1 */

	if (q1->HL2 < q2->HL2) return FALSE; /* HL2 */

	if (q1->LH2 < q2->LH2) return FALSE; /* LH2 */

	if (q1->HH2 < q2->HH2) return FALSE; /* HH2 */

	if (q1->HL3 < q2->HL3) return FALSE; /* HL3 */

	if (q1->LH3 < q2->LH3) return FALSE; /* LH3 */

	if (q1->HH3 < q2->HH3) return FALSE; /* HH3 */

	if (q1->LL3 < q2->LL3) return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_equal(RFX_COMPONENT_CODEC_QUANT* q1,
        RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 != q2->HL1) return FALSE; /* HL1 */

	if (q1->LH1 != q2->LH1) return FALSE; /* LH1 */

	if (q1->HH1 != q2->HH1) return FALSE; /* HH1 */

	if (q1->HL2 != q2->HL2) return FALSE; /* HL2 */

	if (q1->LH2 != q2->LH2) return FALSE; /* LH2 */

	if (q1->HH2 != q2->HH2) return FALSE; /* HH2 */

	if (q1->HL3 != q2->HL3) return FALSE; /* HL3 */

	if (q1->LH3 != q2->LH3) return FALSE; /* LH3 */

	if (q1->HH3 != q2->HH3) return FALSE; /* HH3 */

	if (q1->LL3 != q2->LL3) return FALSE; /* LL3 */

	return TRUE;
}

static void progressive_rfx_quant_print(RFX_COMPONENT_CODEC_QUANT* q, const char* name)
{
	fprintf(stderr,
	        "%s: HL1: %"PRIu8" LH1: %"PRIu8" HH1: %"PRIu8" HL2: %"PRIu8" LH2: %"PRIu8" HH2: %"PRIu8" HL3: %"PRIu8" LH3: %"PRIu8" HH3: %"PRIu8" LL3: %"PRIu8"\n",
	        name, q->HL1, q->LH1, q->HH1, q->HL2, q->LH2, q->HH2, q->HL3, q->LH3, q->HH3,
	        q->LL3);
}


static INLINE BOOL progressive_set_surface_data(PROGRESSIVE_CONTEXT* progressive,
        UINT16 surfaceId, void* pData)
{
	ULONG_PTR key;
	key = ((ULONG_PTR) surfaceId) + 1;

	if (pData)
		return HashTable_Add(progressive->SurfaceContexts, (void *)key, pData) >= 0;

	HashTable_Remove(progressive->SurfaceContexts, (void*) key);
	return TRUE;
}

static INLINE void* progressive_get_surface_data(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId)
{
	ULONG_PTR key;
	void* pData = NULL;
	key = ((ULONG_PTR) surfaceId) + 1;
	pData = HashTable_GetItemValue(progressive->SurfaceContexts, (void*) key);
	return pData;
}

static PROGRESSIVE_SURFACE_CONTEXT* progressive_surface_context_new(UINT16 surfaceId, UINT32 width, UINT32 height)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	surface = (PROGRESSIVE_SURFACE_CONTEXT*) calloc(
	              1, sizeof(PROGRESSIVE_SURFACE_CONTEXT));

	if (!surface)
		return NULL;

	surface->id = surfaceId;
	surface->width = width;
	surface->height = height;
	surface->gridWidth = (width + (64 - width % 64)) / 64;
	surface->gridHeight = (height + (64 - height % 64)) / 64;
	surface->gridSize = surface->gridWidth * surface->gridHeight;
	surface->tiles = (RFX_PROGRESSIVE_TILE*) calloc(surface->gridSize, sizeof(RFX_PROGRESSIVE_TILE));

	if (!surface->tiles)
	{
		free(surface);
		return NULL;
	}

	return surface;
}

static void progressive_surface_context_free(PROGRESSIVE_SURFACE_CONTEXT* surface)
{
	UINT32 index;
	RFX_PROGRESSIVE_TILE* tile;

	for (index = 0; index < surface->gridSize; index++)
	{
		tile = &(surface->tiles[index]);

		if (tile->data)
			_aligned_free(tile->data);

		if (tile->sign)
			_aligned_free(tile->sign);

		if (tile->current)
			_aligned_free(tile->current);
	}

	free(surface->tiles);
	free(surface);
}

INT32 progressive_create_surface_context(PROGRESSIVE_CONTEXT* progressive,
        UINT16 surfaceId, UINT32 width, UINT32 height)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	surface = (PROGRESSIVE_SURFACE_CONTEXT *)progressive_get_surface_data(progressive, surfaceId);

	if (!surface)
	{
		surface = progressive_surface_context_new(surfaceId, width, height);

		if (!surface)
			return -1;

		if (!progressive_set_surface_data(progressive, surfaceId, (void*) surface))
		{
			progressive_surface_context_free(surface);
			return -1;
		}
	}

	return 1;
}

int progressive_delete_surface_context(PROGRESSIVE_CONTEXT* progressive,
                                       UINT16 surfaceId)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	surface = (PROGRESSIVE_SURFACE_CONTEXT *)progressive_get_surface_data(progressive, surfaceId);

	if (surface)
	{
		progressive_set_surface_data(progressive, surfaceId, NULL);
		progressive_surface_context_free(surface);
	}

	return 1;
}

/*
 * Band		Offset		Dimensions	Size
 *
 * HL1		0		31x33		1023
 * LH1		1023		33x31		1023
 * HH1		2046		31x31		961
 *
 * HL2		3007		16x17		272
 * LH2		3279		17x16		272
 * HH2		3551		16x16		256
 *
 * HL3		3807		8x9		72
 * LH3		3879		9x8		72
 * HH3		3951		8x8		64
 *
 * LL3		4015		9x9		81
 */

static INLINE void progressive_rfx_idwt_x(INT16* pLowBand, int nLowStep,
        INT16* pHighBand,
        int nHighStep, INT16* pDstBand, int nDstStep,
        int nLowCount, int nHighCount, int nDstCount)
{
	int i, j;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;
	INT16* pL, *pH, *pX;

	for (i = 0; i < nDstCount; i++)
	{
		pL = pLowBand;
		pH = pHighBand;
		pX = pDstBand;
		H0 = *pH;
		pH++;
		L0 = *pL;
		pL++;
		X0 = L0 - H0;
		X2 = L0 - H0;

		for (j = 0; j < (nHighCount - 1); j++)
		{
			H1 = *pH;
			pH++;
			L0 = *pL;
			pL++;
			X2 = L0 - ((H0 + H1) / 2);
			X1 = ((X0 + X2) / 2) + (2 * H0);
			pX[0] = X0;
			pX[1] = X1;
			pX += 2;
			X0 = X2;
			H0 = H1;
		}

		if (nLowCount <= (nHighCount + 1))
		{
			if (nLowCount <= nHighCount)
			{
				pX[0] = X2;
				pX[1] = X2 + (2 * H0);
			}
			else
			{
				L0 = *pL;
				pL++;
				X0 = L0 - H0;
				pX[0] = X2;
				pX[1] = ((X0 + X2) / 2) + (2 * H0);
				pX[2] = X0;
			}
		}
		else
		{
			L0 = *pL;
			pL++;
			X0 = L0 - (H0 / 2);
			pX[0] = X2;
			pX[1] = ((X0 + X2) / 2) + (2 * H0);
			pX[2] = X0;
			L0 = *pL;
			pL++;
			pX[3] = (X0 + L0) / 2;
		}

		pLowBand += nLowStep;
		pHighBand += nHighStep;
		pDstBand += nDstStep;
	}
}

static INLINE void progressive_rfx_idwt_y(INT16* pLowBand, int nLowStep,
        INT16* pHighBand,
        int nHighStep, INT16* pDstBand, int nDstStep,
        int nLowCount, int nHighCount, int nDstCount)
{
	int i, j;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;
	INT16* pL, *pH, *pX;

	for (i = 0; i < nDstCount; i++)
	{
		pL = pLowBand;
		pH = pHighBand;
		pX = pDstBand;
		H0 = *pH;
		pH += nHighStep;
		L0 = *pL;
		pL += nLowStep;
		X0 = L0 - H0;
		X2 = L0 - H0;

		for (j = 0; j < (nHighCount - 1); j++)
		{
			H1 = *pH;
			pH += nHighStep;
			L0 = *pL;
			pL += nLowStep;
			X2 = L0 - ((H0 + H1) / 2);
			X1 = ((X0 + X2) / 2) + (2 * H0);
			*pX = X0;
			pX += nDstStep;
			*pX = X1;
			pX += nDstStep;
			X0 = X2;
			H0 = H1;
		}

		if (nLowCount <= (nHighCount + 1))
		{
			if (nLowCount <= nHighCount)
			{
				*pX = X2;
				pX += nDstStep;
				*pX = X2 + (2 * H0);
			}
			else
			{
				L0 = *pL;
				X0 = L0 - H0;
				*pX = X2;
				pX += nDstStep;
				*pX = ((X0 + X2) / 2) + (2 * H0);
				pX += nDstStep;
				*pX = X0;
			}
		}
		else
		{
			L0 = *pL;
			pL += nLowStep;
			X0 = L0 - (H0 / 2);
			*pX = X2;
			pX += nDstStep;
			*pX = ((X0 + X2) / 2) + (2 * H0);
			pX += nDstStep;
			*pX = X0;
			pX += nDstStep;
			L0 = *pL;
			*pX = (X0 + L0) / 2;
		}

		pLowBand++;
		pHighBand++;
		pDstBand++;
	}
}

static INLINE int progressive_rfx_get_band_l_count(int level)
{
	return (64 >> level) + 1;
}

static INLINE int progressive_rfx_get_band_h_count(int level)
{
	if (level == 1)
		return (64 >> 1) - 1;
	else
		return (64 + (1 << (level - 1))) >> level;
}

static INLINE void progressive_rfx_dwt_2d_decode_block(INT16* buffer, INT16* temp,
        int level)
{
	int offset;
	int nBandL;
	int nBandH;
	int nDstStepX;
	int nDstStepY;
	INT16* HL, *LH;
	INT16* HH, *LL;
	INT16* L, *H, *LLx;
	INT16* pLowBand[3];
	INT16* pHighBand[3];
	INT16* pDstBand[3];
	int nLowStep[3];
	int nHighStep[3];
	int nDstStep[3];
	int nLowCount[3];
	int nHighCount[3];
	int nDstCount[3];
	nBandL = progressive_rfx_get_band_l_count(level);
	nBandH = progressive_rfx_get_band_h_count(level);
	offset = 0;
	HL = &buffer[offset];
	offset += (nBandH * nBandL);
	LH = &buffer[offset];
	offset += (nBandL * nBandH);
	HH = &buffer[offset];
	offset += (nBandH * nBandH);
	LL = &buffer[offset];
	nDstStepX = (nBandL + nBandH);
	nDstStepY = (nBandL + nBandH);
	offset = 0;
	L = &temp[offset];
	offset += (nBandL * nDstStepX);
	H = &temp[offset];
	LLx = &buffer[0];
	/* horizontal (LL + HL -> L) */
	pLowBand[0] = LL;
	nLowStep[0] = nBandL;
	pHighBand[0] = HL;
	nHighStep[0] = nBandH;
	pDstBand[0] = L;
	nDstStep[0] = nDstStepX;
	nLowCount[0] = nBandL;
	nHighCount[0] = nBandH;
	nDstCount[0] = nBandL;
	progressive_rfx_idwt_x(pLowBand[0], nLowStep[0], pHighBand[0], nHighStep[0],
	                       pDstBand[0], nDstStep[0], nLowCount[0], nHighCount[0], nDstCount[0]);
	/* horizontal (LH + HH -> H) */
	pLowBand[1] = LH;
	nLowStep[1] = nBandL;
	pHighBand[1] = HH;
	nHighStep[1] = nBandH;
	pDstBand[1] = H;
	nDstStep[1] = nDstStepX;
	nLowCount[1] = nBandL;
	nHighCount[1] = nBandH;
	nDstCount[1] = nBandH;
	progressive_rfx_idwt_x(pLowBand[1], nLowStep[1], pHighBand[1], nHighStep[1],
	                       pDstBand[1], nDstStep[1], nLowCount[1], nHighCount[1], nDstCount[1]);
	/* vertical (L + H -> LL) */
	pLowBand[2] = pDstBand[0];
	nLowStep[2] = nDstStep[0];
	pHighBand[2] = pDstBand[1];
	nHighStep[2] = nDstStep[1];
	pDstBand[2] = LLx;
	nDstStep[2] = nDstStepY;
	nLowCount[2] = nBandL;
	nHighCount[2] = nBandH;
	nDstCount[2] = nBandL + nBandH;
	progressive_rfx_idwt_y(pLowBand[2], nLowStep[2], pHighBand[2], nHighStep[2],
	                       pDstBand[2], nDstStep[2], nLowCount[2], nHighCount[2], nDstCount[2]);
}

static INLINE void progressive_rfx_dwt_2d_decode(INT16* buffer, INT16* temp,
        INT16* current, INT16* sign, BOOL diff)
{
	const primitives_t* prims = primitives_get();

	if (diff)
		prims->add_16s(buffer, current, buffer, 4096);

	CopyMemory(current, buffer, 4096 * 2);
	progressive_rfx_dwt_2d_decode_block(&buffer[3807], temp, 3);
	progressive_rfx_dwt_2d_decode_block(&buffer[3007], temp, 2);
	progressive_rfx_dwt_2d_decode_block(&buffer[0], temp, 1);
}

static INLINE void progressive_rfx_decode_block(const primitives_t* prims,
        INT16* buffer,
        int length, UINT32 shift)
{
	if (!shift)
		return;

	prims->lShiftC_16s(buffer, shift, buffer, length);
}

static INLINE int progressive_rfx_decode_component(PROGRESSIVE_CONTEXT* progressive,
        RFX_COMPONENT_CODEC_QUANT* shift,
        const BYTE* data, int length,
        INT16* buffer, INT16* current,
        INT16* sign, BOOL diff)
{
	int status;
	INT16* temp;
	const primitives_t* prims = primitives_get();
	status = rfx_rlgr_decode(RLGR1, data, length, buffer, 4096);

	if (status < 0)
		return status;

	CopyMemory(sign, buffer, 4096 * 2);
	rfx_differential_decode(&buffer[4015], 81); /* LL3 */
	progressive_rfx_decode_block(prims, &buffer[0], 1023, shift->HL1); /* HL1 */
	progressive_rfx_decode_block(prims, &buffer[1023], 1023, shift->LH1); /* LH1 */
	progressive_rfx_decode_block(prims, &buffer[2046], 961, shift->HH1); /* HH1 */
	progressive_rfx_decode_block(prims, &buffer[3007], 272, shift->HL2); /* HL2 */
	progressive_rfx_decode_block(prims, &buffer[3279], 272, shift->LH2); /* LH2 */
	progressive_rfx_decode_block(prims, &buffer[3551], 256, shift->HH2); /* HH2 */
	progressive_rfx_decode_block(prims, &buffer[3807], 72, shift->HL3); /* HL3 */
	progressive_rfx_decode_block(prims, &buffer[3879], 72, shift->LH3); /* LH3 */
	progressive_rfx_decode_block(prims, &buffer[3951], 64, shift->HH3); /* HH3 */
	progressive_rfx_decode_block(prims, &buffer[4015], 81, shift->LL3); /* LL3 */
	temp = (INT16*) BufferPool_Take(progressive->bufferPool, -1); /* DWT buffer */
	progressive_rfx_dwt_2d_decode(buffer, temp, current, sign, diff);
	BufferPool_Return(progressive->bufferPool, temp);
	return 1;
}

static INLINE int progressive_decompress_tile_first(PROGRESSIVE_CONTEXT* progressive,
        RFX_PROGRESSIVE_TILE* tile)
{
	BOOL diff;
	BYTE* pBuffer;
	INT16* pSign[3];
	INT16* pSrcDst[3];
	INT16* pCurrent[3];
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT shiftY;
	RFX_COMPONENT_CODEC_QUANT shiftCb;
	RFX_COMPONENT_CODEC_QUANT shiftCr;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_COMPONENT_CODEC_QUANT* quantProgY;
	RFX_COMPONENT_CODEC_QUANT* quantProgCb;
	RFX_COMPONENT_CODEC_QUANT* quantProgCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t* prims = primitives_get();

	tile->pass = 1;
	diff = tile->flags & RFX_TILE_DIFFERENCE;
	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveTile%s: quantIdx Y: %"PRIu8" Cb: %"PRIu8" Cr: %"PRIu8" xIdx: %"PRIu16" yIdx: %"PRIu16" flags: 0x%02"PRIX8" quality: %"PRIu8" yLen: %"PRIu16" cbLen: %"PRIu16" crLen: %"PRIu16" tailLen: %"PRIu16"",
	           (tile->blockType == PROGRESSIVE_WBT_TILE_FIRST) ? "First" : "Simple",
	           tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr,
	           tile->xIdx, tile->yIdx, tile->flags, tile->quality, tile->yLen,
	           tile->cbLen, tile->crLen, tile->tailLen);
	region = &(progressive->region);

	if (tile->quantIdxY >= region->numQuant)
		return -1;

	quantY = &(region->quantVals[tile->quantIdxY]);

	if (tile->quantIdxCb >= region->numQuant)
		return -1;

	quantCb = &(region->quantVals[tile->quantIdxCb]);

	if (tile->quantIdxCr >= region->numQuant)
		return -1;

	quantCr = &(region->quantVals[tile->quantIdxCr]);

	if (tile->quality == 0xFF)
	{
		quantProgVal = &(progressive->quantProgValFull);
	}
	else
	{
		if (tile->quality >= region->numProgQuant)
			return -1;

		quantProgVal = &(region->quantProgVals[tile->quality]);
	}

	quantProgY = &(quantProgVal->yQuantValues);
	quantProgCb = &(quantProgVal->cbQuantValues);
	quantProgCr = &(quantProgVal->crQuantValues);

	CopyMemory(&(tile->yQuant), quantY, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbQuant), quantCb, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crQuant), quantCr, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->yProgQuant), quantProgY, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbProgQuant), quantProgCb, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crProgQuant), quantProgCr, sizeof(RFX_COMPONENT_CODEC_QUANT));

	progressive_rfx_quant_add(quantY, quantProgY, &(tile->yBitPos));
	progressive_rfx_quant_add(quantCb, quantProgCb, &(tile->cbBitPos));
	progressive_rfx_quant_add(quantCr, quantProgCr, &(tile->crBitPos));
	progressive_rfx_quant_add(quantY, quantProgY, &shiftY);
	progressive_rfx_quant_lsub(&shiftY, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCb, quantProgCb, &shiftCb);
	progressive_rfx_quant_lsub(&shiftCb, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCr, quantProgCr, &shiftCr);
	progressive_rfx_quant_lsub(&shiftCr, 1); /* -6 + 5 = -1 */

	if (!tile->data)
	{
		tile->data = (BYTE*) _aligned_malloc(64 * 64 * 4, 16);
		if (!tile->data)
			return -1;
	}

	if (!tile->sign)
	{
		tile->sign = (BYTE*) _aligned_malloc((8192 + 32) * 3, 16);
		if (!tile->sign)
			return -1;
	}

	if (!tile->current)
	{
		tile->current = (BYTE*) _aligned_malloc((8192 + 32) * 3, 16);
		if (!tile->current)
			return -1;
	}

	pBuffer = tile->sign;
	pSign[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSign[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSign[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = tile->current;
	pCurrent[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = (BYTE*) BufferPool_Take(progressive->bufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	progressive_rfx_decode_component(progressive, &shiftY, tile->yData, tile->yLen,
	                                 pSrcDst[0], pCurrent[0], pSign[0], diff); /* Y */
	progressive_rfx_decode_component(progressive, &shiftCb, tile->cbData, tile->cbLen,
	                                 pSrcDst[1], pCurrent[1], pSign[1], diff); /* Cb */
	progressive_rfx_decode_component(progressive, &shiftCr, tile->crData, tile->crLen,
	                                 pSrcDst[2], pCurrent[2], pSign[2], diff); /* Cr */

	prims->yCbCrToRGB_16s8u_P3AC4R((const INT16**) pSrcDst, 64 * 2, tile->data, tile->stride,
								tile->format, &roi_64x64);
	BufferPool_Return(progressive->bufferPool, pBuffer);
	return 1;
}

static INLINE INT16 progressive_rfx_srl_read(RFX_PROGRESSIVE_UPGRADE_STATE* state,
        UINT32 numBits)
{
	int k;
	UINT32 bit;
	UINT32 max;
	UINT32 mag;
	UINT32 sign;
	wBitStream* bs = state->srl;

	if (state->nz)
	{
		state->nz--;
		return 0;
	}

	k = state->kp / 8;

	if (!state->mode)
	{
		/* zero encoding */
		bit = (bs->accumulator & 0x80000000) ? 1 : 0;
		BitStream_Shift(bs, 1);

		if (!bit)
		{
			/* '0' bit, nz >= (1 << k), nz = (1 << k) */
			state->nz = (1 << k);
			state->kp += 4;

			if (state->kp > 80)
				state->kp = 80;

			state->nz--;
			return 0;
		}
		else
		{
			/* '1' bit, nz < (1 << k), nz = next k bits */
			state->nz = 0;
			state->mode = 1; /* unary encoding is next */

			if (k)
			{
				bs->mask = ((1 << k) - 1);
				state->nz = ((bs->accumulator >> (32 - k)) & bs->mask);
				BitStream_Shift(bs, k);
			}

			if (state->nz)
			{
				state->nz--;
				return 0;
			}
		}
	}

	state->mode = 0; /* zero encoding is next */
	/* unary encoding */
	/* read sign bit */
	sign = (bs->accumulator & 0x80000000) ? 1 : 0;
	BitStream_Shift(bs, 1);
	state->kp -= 6;

	if (state->kp < 0)
		state->kp = 0;

	if (numBits == 1)
		return sign ? -1 : 1;

	mag = 1;
	max = (1 << numBits) - 1;

	while (mag < max)
	{
		bit = (bs->accumulator & 0x80000000) ? 1 : 0;
		BitStream_Shift(bs, 1);

		if (bit)
			break;

		mag++;
	}

	return sign ? -1 * mag : mag;
}

static INLINE int progressive_rfx_upgrade_state_finish(RFX_PROGRESSIVE_UPGRADE_STATE* state)
{
	int pad;
	wBitStream* srl;
	wBitStream* raw;
	srl = state->srl;
	raw = state->raw;
	/* Read trailing bits from RAW/SRL bit streams */
	pad = (raw->position % 8) ? (8 - (raw->position % 8)) : 0;

	if (pad)
		BitStream_Shift(raw, pad);

	pad = (srl->position % 8) ? (8 - (srl->position % 8)) : 0;

	if (pad)
		BitStream_Shift(srl, pad);

	if (BitStream_GetRemainingLength(srl) == 8)
		BitStream_Shift(srl, 8);

	return 1;
}

static INLINE int progressive_rfx_upgrade_block(RFX_PROGRESSIVE_UPGRADE_STATE* state,
        INT16* buffer,	INT16* sign, UINT32 length,
        UINT32 shift, UINT32 bitPos, UINT32 numBits)
{
	int index;
	INT16 input;
	wBitStream* raw;

	if (!numBits)
		return 1;

	raw = state->raw;

	if (!state->nonLL)
	{
		for (index = 0; index < length; index++)
		{
			raw->mask = ((1 << numBits) - 1);
			input = (INT16)((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);
			buffer[index] += (input << shift);
		}

		return 1;
	}

	for (index = 0; index < length; index++)
	{
		if (sign[index] > 0)
		{
			/* sign > 0, read from raw */
			raw->mask = ((1 << numBits) - 1);
			input = (INT16)((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);
		}
		else if (sign[index] < 0)
		{
			/* sign < 0, read from raw */
			raw->mask = ((1 << numBits) - 1);
			input = (INT16)((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);
			input *= -1;
		}
		else
		{
			/* sign == 0, read from srl */
			input = progressive_rfx_srl_read(state, numBits);
			sign[index] = input;
		}

		buffer[index] += (input << shift);
	}

	return 1;
}

static INLINE int progressive_rfx_upgrade_component(PROGRESSIVE_CONTEXT* progressive,
        RFX_COMPONENT_CODEC_QUANT* shift,
        RFX_COMPONENT_CODEC_QUANT* bitPos,
        RFX_COMPONENT_CODEC_QUANT* numBits,
        INT16* buffer,
        INT16* current, INT16* sign,
        const BYTE* srlData,
        UINT32 srlLen, const BYTE* rawData,
        UINT32 rawLen)
{
	INT16* temp;
	UINT32 aRawLen;
	UINT32 aSrlLen;
	wBitStream s_srl;
	wBitStream s_raw;
	RFX_PROGRESSIVE_UPGRADE_STATE state;

	ZeroMemory(&s_srl, sizeof(wBitStream));
	ZeroMemory(&s_raw, sizeof(wBitStream));
	ZeroMemory(&state, sizeof(RFX_PROGRESSIVE_UPGRADE_STATE));
	state.kp = 8;
	state.mode = 0;
	state.srl = &s_srl;
	state.raw = &s_raw;
	BitStream_Attach(state.srl, srlData, srlLen);
	BitStream_Fetch(state.srl);
	BitStream_Attach(state.raw, rawData, rawLen);
	BitStream_Fetch(state.raw);

	state.nonLL = TRUE;
	progressive_rfx_upgrade_block(&state, &current[0], &sign[0], 1023, shift->HL1,
	                              bitPos->HL1, numBits->HL1); /* HL1 */
	progressive_rfx_upgrade_block(&state, &current[1023], &sign[1023], 1023,
	                              shift->LH1, bitPos->LH1, numBits->LH1); /* LH1 */
	progressive_rfx_upgrade_block(&state, &current[2046], &sign[2046], 961,
	                              shift->HH1, bitPos->HH1, numBits->HH1); /* HH1 */
	progressive_rfx_upgrade_block(&state, &current[3007], &sign[3007], 272,
	                              shift->HL2, bitPos->HL2, numBits->HL2); /* HL2 */
	progressive_rfx_upgrade_block(&state, &current[3279], &sign[3279], 272,
	                              shift->LH2, bitPos->LH2, numBits->LH2); /* LH2 */
	progressive_rfx_upgrade_block(&state, &current[3551], &sign[3551], 256,
	                              shift->HH2, bitPos->HH2, numBits->HH2); /* HH2 */
	progressive_rfx_upgrade_block(&state, &current[3807], &sign[3807], 72,
	                              shift->HL3, bitPos->HL3, numBits->HL3); /* HL3 */
	progressive_rfx_upgrade_block(&state, &current[3879], &sign[3879], 72,
	                              shift->LH3, bitPos->LH3, numBits->LH3); /* LH3 */
	progressive_rfx_upgrade_block(&state, &current[3951], &sign[3951], 64,
	                              shift->HH3, bitPos->HH3, numBits->HH3); /* HH3 */

	state.nonLL = FALSE;
	progressive_rfx_upgrade_block(&state, &current[4015], &sign[4015], 81,
	                              shift->LL3, bitPos->LL3, numBits->LL3); /* LL3 */
	progressive_rfx_upgrade_state_finish(&state);
	aRawLen = (state.raw->position + 7) / 8;
	aSrlLen = (state.srl->position + 7) / 8;

	if ((aRawLen != rawLen) || (aSrlLen != srlLen))
	{
		int pRawLen = 0;
		int pSrlLen = 0;

		if (rawLen)
			pRawLen = (int)((((float) aRawLen) / ((float) rawLen)) * 100.0f);

		if (srlLen)
			pSrlLen = (int)((((float) aSrlLen) / ((float) srlLen)) * 100.0f);

		WLog_Print(progressive->log, WLOG_INFO,
		           "RAW: %"PRIu32"/%"PRIu32" %d%% (%"PRIu32"/%"PRIu32":%"PRIu32")\tSRL: %"PRIu32"/%"PRIu32" %d%% (%"PRIu32"/%"PRIu32":%"PRIu32")",
		           aRawLen, rawLen, pRawLen, state.raw->position, rawLen * 8,
		           (rawLen * 8) - state.raw->position,
		           aSrlLen, srlLen, pSrlLen, state.srl->position, srlLen * 8,
		           (srlLen * 8) - state.srl->position);
		return -1;
	}

	temp = (INT16*) BufferPool_Take(progressive->bufferPool, -1); /* DWT buffer */
	CopyMemory(buffer, current, 4096 * 2);
	progressive_rfx_dwt_2d_decode_block(&buffer[3807], temp, 3);
	progressive_rfx_dwt_2d_decode_block(&buffer[3007], temp, 2);
	progressive_rfx_dwt_2d_decode_block(&buffer[0], temp, 1);
	BufferPool_Return(progressive->bufferPool, temp);
	return 1;
}

static INLINE int progressive_decompress_tile_upgrade(PROGRESSIVE_CONTEXT* progressive,
        RFX_PROGRESSIVE_TILE* tile)
{
	int status;
	BYTE* pBuffer;
	INT16* pSign[3];
	INT16* pSrcDst[3];
	INT16* pCurrent[3];
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT shiftY;
	RFX_COMPONENT_CODEC_QUANT shiftCb;
	RFX_COMPONENT_CODEC_QUANT shiftCr;
	RFX_COMPONENT_CODEC_QUANT yBitPos;
	RFX_COMPONENT_CODEC_QUANT cbBitPos;
	RFX_COMPONENT_CODEC_QUANT crBitPos;
	RFX_COMPONENT_CODEC_QUANT yNumBits;
	RFX_COMPONENT_CODEC_QUANT cbNumBits;
	RFX_COMPONENT_CODEC_QUANT crNumBits;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_COMPONENT_CODEC_QUANT* quantProgY;
	RFX_COMPONENT_CODEC_QUANT* quantProgCb;
	RFX_COMPONENT_CODEC_QUANT* quantProgCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProg;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t* prims = primitives_get();
	tile->pass++;
	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveTileUpgrade: pass: %"PRIu16" quantIdx Y: %"PRIu8" Cb: %"PRIu8" Cr: %"PRIu8" xIdx: %"PRIu16" yIdx: %"PRIu16" quality: %"PRIu8" ySrlLen: %"PRIu16" yRawLen: %"PRIu16" cbSrlLen: %"PRIu16" cbRawLen: %"PRIu16" crSrlLen: %"PRIu16" crRawLen: %"PRIu16"",
	           tile->pass, tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx,
	           tile->yIdx, tile->quality, tile->ySrlLen, tile->yRawLen, tile->cbSrlLen,
	           tile->cbRawLen, tile->crSrlLen, tile->crRawLen);
	region = &(progressive->region);

	if (tile->quantIdxY >= region->numQuant)
		return -1;

	quantY = &(region->quantVals[tile->quantIdxY]);

	if (tile->quantIdxCb >= region->numQuant)
		return -1;

	quantCb = &(region->quantVals[tile->quantIdxCb]);

	if (tile->quantIdxCr >= region->numQuant)
		return -1;

	quantCr = &(region->quantVals[tile->quantIdxCr]);

	if (tile->quality == 0xFF)
	{
		quantProg = &(progressive->quantProgValFull);
	}
	else
	{
		if (tile->quality >= region->numProgQuant)
			return -1;

		quantProg = &(region->quantProgVals[tile->quality]);
	}

	quantProgY = &(quantProg->yQuantValues);
	quantProgCb = &(quantProg->cbQuantValues);
	quantProgCr = &(quantProg->crQuantValues);

	if (!progressive_rfx_quant_cmp_equal(quantY, &(tile->yQuant)))
		WLog_Print(progressive->log, WLOG_WARN, "non-progressive quantY has changed!");

	if (!progressive_rfx_quant_cmp_equal(quantCb, &(tile->cbQuant)))
		WLog_Print(progressive->log, WLOG_WARN, "non-progressive quantCb has changed!");

	if (!progressive_rfx_quant_cmp_equal(quantCr, &(tile->crQuant)))
		WLog_Print(progressive->log, WLOG_WARN, "non-progressive quantCr has changed!");

	progressive_rfx_quant_add(quantY, quantProgY, &yBitPos);
	progressive_rfx_quant_add(quantCb, quantProgCb, &cbBitPos);
	progressive_rfx_quant_add(quantCr, quantProgCr, &crBitPos);
	progressive_rfx_quant_sub(&(tile->yBitPos), &yBitPos, &yNumBits);
	progressive_rfx_quant_sub(&(tile->cbBitPos), &cbBitPos, &cbNumBits);
	progressive_rfx_quant_sub(&(tile->crBitPos), &crBitPos, &crNumBits);
	progressive_rfx_quant_add(quantY, quantProgY, &shiftY);
	progressive_rfx_quant_lsub(&shiftY, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCb, quantProgCb, &shiftCb);
	progressive_rfx_quant_lsub(&shiftCb, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCr, quantProgCr, &shiftCr);
	progressive_rfx_quant_lsub(&shiftCr, 1); /* -6 + 5 = -1 */

	CopyMemory(&(tile->yBitPos), &yBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbBitPos), &cbBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crBitPos), &crBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->yQuant), quantY, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbQuant), quantCb, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crQuant), quantCr, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->yProgQuant), quantProgY, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbProgQuant), quantProgCb, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crProgQuant), quantProgCr, sizeof(RFX_COMPONENT_CODEC_QUANT));

	pBuffer = tile->sign;
	pSign[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSign[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSign[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = tile->current;
	pCurrent[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = (BYTE*) BufferPool_Take(progressive->bufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	status = progressive_rfx_upgrade_component(progressive, &shiftY, quantProgY,
	         &yNumBits,
	         pSrcDst[0], pCurrent[0], pSign[0], tile->ySrlData, tile->ySrlLen,
	         tile->yRawData, tile->yRawLen); /* Y */

	if (status < 0)
		return -1;

	status = progressive_rfx_upgrade_component(progressive, &shiftCb, quantProgCb,
	         &cbNumBits,
	         pSrcDst[1], pCurrent[1], pSign[1], tile->cbSrlData, tile->cbSrlLen,
	         tile->cbRawData, tile->cbRawLen); /* Cb */

	if (status < 0)
		return -1;

	status = progressive_rfx_upgrade_component(progressive, &shiftCr, quantProgCr,
	         &crNumBits,
	         pSrcDst[2], pCurrent[2], pSign[2], tile->crSrlData, tile->crSrlLen,
	         tile->crRawData, tile->crRawLen); /* Cr */

	if (status < 0)
		return -1;

	prims->yCbCrToRGB_16s8u_P3AC4R((const INT16**) pSrcDst, 64 * 2,
	                               tile->data, tile->stride, tile->format,
	                               &roi_64x64);
	BufferPool_Return(progressive->bufferPool, pBuffer);
	return 1;
}

static INLINE int progressive_process_tiles(PROGRESSIVE_CONTEXT* progressive,
        const BYTE* blocks, UINT32 blocksLen,
        const PROGRESSIVE_SURFACE_CONTEXT* surface)
{
	int status = -1;
	const BYTE* block;
	UINT16 xIdx;
	UINT16 yIdx;
	UINT16 zIdx;
	UINT16 index;
	UINT32 boffset;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 count = 0;
	UINT32 offset = 0;
	RFX_PROGRESSIVE_TILE* tile;
	RFX_PROGRESSIVE_TILE** tiles;
	PROGRESSIVE_BLOCK_REGION* region;
	region = &(progressive->region);
	tiles = region->tiles;

	while ((blocksLen - offset) >= 6)
	{
		boffset = 0;
		block = &blocks[offset];
		blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;
		WLog_Print(progressive->log, WLOG_DEBUG, "%s", progressive_get_block_type_string(blockType));

		if ((blocksLen - offset) < blockLen)
			return -1003;

		switch (blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:
				if ((blockLen - boffset) < 16)
					return -1022;

				xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				zIdx = (yIdx * surface->gridWidth) + xIdx;

				if (zIdx >= surface->gridSize)
					return -1;

				tiles[count] = tile = &(surface->tiles[zIdx]);
				tile->blockType = blockType;
				tile->blockLen = blockLen;
				tile->quality = 0xFF; /* simple tiles use no progressive techniques */
				tile->quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				tile->quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				tile->quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				tile->xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				tile->yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				tile->flags = block[boffset + 7] & 1; /* flags (1 byte) */
				tile->yLen = *((UINT16*) &block[boffset + 8]); /* yLen (2 bytes) */
				tile->cbLen = *((UINT16*) &block[boffset + 10]); /* cbLen (2 bytes) */
				tile->crLen = *((UINT16*) &block[boffset + 12]); /* crLen (2 bytes) */
				tile->tailLen = *((UINT16*) &block[boffset + 14]); /* tailLen (2 bytes) */
				boffset += 16;

				if ((tile->blockLen - boffset) < tile->yLen)
					return -1023;

				tile->yData = &block[boffset];
				boffset += tile->yLen;

				if ((tile->blockLen - boffset) < tile->cbLen)
					return -1024;

				tile->cbData = &block[boffset];
				boffset += tile->cbLen;

				if ((tile->blockLen - boffset) < tile->crLen)
					return -1025;

				tile->crData = &block[boffset];
				boffset += tile->crLen;

				if ((tile->blockLen - boffset) < tile->tailLen)
					return -1026;

				tile->tailData = &block[boffset];
				boffset += tile->tailLen;
				tile->width = 64;
				tile->height = 64;
				tile->x = tile->xIdx * 64;
				tile->y = tile->yIdx * 64;
				tile->format = progressive->format;
				tile->stride = GetBytesPerPixel(tile->format) * tile->width;
				tile->flags &= 1;
				break;

			case PROGRESSIVE_WBT_TILE_FIRST:
				if ((blockLen - boffset) < 17)
					return -1027;

				xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				zIdx = (yIdx * surface->gridWidth) + xIdx;

				if (zIdx >= surface->gridSize)
					return -1;

				tiles[count] = tile = &(surface->tiles[zIdx]);
				tile->blockType = blockType;
				tile->blockLen = blockLen;
				tile->quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				tile->quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				tile->quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				tile->xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				tile->yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				tile->flags = block[boffset + 7] & 1; /* flags (1 byte) */
				tile->quality = block[boffset + 8]; /* quality (1 byte) */
				tile->yLen = *((UINT16*) &block[boffset + 9]); /* yLen (2 bytes) */
				tile->cbLen = *((UINT16*) &block[boffset + 11]); /* cbLen (2 bytes) */
				tile->crLen = *((UINT16*) &block[boffset + 13]); /* crLen (2 bytes) */
				tile->tailLen = *((UINT16*) &block[boffset + 15]); /* tailLen (2 bytes) */
				boffset += 17;

				if ((tile->blockLen - boffset) < tile->yLen)
					return -1028;

				tile->yData = &block[boffset];
				boffset += tile->yLen;

				if ((tile->blockLen - boffset) < tile->cbLen)
					return -1029;

				tile->cbData = &block[boffset];
				boffset += tile->cbLen;

				if ((tile->blockLen - boffset) < tile->crLen)
					return -1030;

				tile->crData = &block[boffset];
				boffset += tile->crLen;

				if ((tile->blockLen - boffset) < tile->tailLen)
					return -1031;

				tile->tailData = &block[boffset];
				boffset += tile->tailLen;
				tile->width = 64;
				tile->height = 64;
				tile->x = tile->xIdx * 64;
				tile->y = tile->yIdx * 64;
				tile->format = progressive->format;
				tile->stride = GetBytesPerPixel(tile->format) * tile->width;
				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:
				if ((blockLen - boffset) < 20)
					return -1032;

				xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				zIdx = (yIdx * surface->gridWidth) + xIdx;

				if (zIdx >= surface->gridSize)
					return -1;

				tiles[count] = tile = &(surface->tiles[zIdx]);
				tile->blockType = blockType;
				tile->blockLen = blockLen;
				tile->flags = 0;
				tile->quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				tile->quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				tile->quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				tile->xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				tile->yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				tile->quality = block[boffset + 7]; /* quality (1 byte) */
				tile->ySrlLen = *((UINT16*) &block[boffset + 8]); /* ySrlLen (2 bytes) */
				tile->yRawLen = *((UINT16*) &block[boffset + 10]); /* yRawLen (2 bytes) */
				tile->cbSrlLen = *((UINT16*) &block[boffset + 12]); /* cbSrlLen (2 bytes) */
				tile->cbRawLen = *((UINT16*) &block[boffset + 14]); /* cbRawLen (2 bytes) */
				tile->crSrlLen = *((UINT16*) &block[boffset + 16]); /* crSrlLen (2 bytes) */
				tile->crRawLen = *((UINT16*) &block[boffset + 18]); /* crRawLen (2 bytes) */
				boffset += 20;

				if ((tile->blockLen - boffset) < tile->ySrlLen)
					return -1033;

				tile->ySrlData = &block[boffset];
				boffset += tile->ySrlLen;

				if ((tile->blockLen - boffset) < tile->yRawLen)
					return -1034;

				tile->yRawData = &block[boffset];
				boffset += tile->yRawLen;

				if ((tile->blockLen - boffset) < tile->cbSrlLen)
					return -1035;

				tile->cbSrlData = &block[boffset];
				boffset += tile->cbSrlLen;

				if ((tile->blockLen - boffset) < tile->cbRawLen)
					return -1036;

				tile->cbRawData = &block[boffset];
				boffset += tile->cbRawLen;

				if ((tile->blockLen - boffset) < tile->crSrlLen)
					return -1037;

				tile->crSrlData = &block[boffset];
				boffset += tile->crSrlLen;

				if ((tile->blockLen - boffset) < tile->crRawLen)
					return -1038;

				tile->crRawData = &block[boffset];
				boffset += tile->crRawLen;
				tile->width = 64;
				tile->height = 64;
				tile->x = tile->xIdx * 64;
				tile->y = tile->yIdx * 64;
				tile->format = progressive->format;
				tile->stride = GetBytesPerPixel(tile->format) * tile->width;
				break;

			default:
				return -1039;
		}

		if (boffset != blockLen)
			return -1040;

		offset += blockLen;
		count++;
	}

	if (offset != blocksLen)
		return -1041;

	if (count != region->numTiles)
	{
		WLog_Print(progressive->log, WLOG_WARN,
		           "numTiles inconsistency: actual: %"PRIu32", expected: %"PRIu16"\n", count,
		           region->numTiles);
	}

	for (index = 0; index < region->numTiles; index++)
	{
		tile = tiles[index];

		switch (tile->blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:
			case PROGRESSIVE_WBT_TILE_FIRST:
				status = progressive_decompress_tile_first(progressive, tile);
				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:
				status = progressive_decompress_tile_upgrade(progressive, tile);
				break;
		}

		if (status < 0)
			return -1;
	}

	return (int) offset;
}

INT32 progressive_decompress(PROGRESSIVE_CONTEXT* progressive,
                             const BYTE* pSrcData, UINT32 SrcSize,
                             BYTE* pDstData, UINT32 DstFormat,
                             UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                             REGION16* invalidRegion, UINT16 surfaceId)
{
	INT32 rc = 1;
	INT32 status;
	const BYTE* block;
	const BYTE* blocks;
	UINT16 i, j;
	UINT16 index;
	UINT16 boxLeft;
	UINT16 boxTop;
	UINT16 boxRight;
	UINT16 boxBottom;
	UINT16 idxLeft;
	UINT16 idxTop;
	UINT16 idxRight;
	UINT16 idxBottom;
	UINT32 boffset;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 blocksLen;
	UINT32 count = 0;
	UINT32 offset = 0;
	RFX_RECT* rect = NULL;
	REGION16 clippingRects, updateRegion;
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	PROGRESSIVE_BLOCK_REGION* region;
	surface = (PROGRESSIVE_SURFACE_CONTEXT*) progressive_get_surface_data(
	              progressive, surfaceId);

	if (!surface)
		return -1001;

	blocks = pSrcData;
	blocksLen = SrcSize;
	region = &(progressive->region);
	progressive->format = DstFormat;

	while ((blocksLen - offset) >= 6)
	{
		PROGRESSIVE_BLOCK_SYNC sync;
		PROGRESSIVE_BLOCK_CONTEXT context;
		PROGRESSIVE_BLOCK_FRAME_BEGIN frameBegin;
		PROGRESSIVE_BLOCK_FRAME_END frameEnd;
		RFX_COMPONENT_CODEC_QUANT* quantVal;
		RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;
		boffset = 0;
		block = &blocks[offset];
		blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		if ((blocksLen - offset) < blockLen)
			return -1003;

		switch (blockType)
		{
			case PROGRESSIVE_WBT_SYNC:
				WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveSync");
				sync.blockType = blockType;
				sync.blockLen = blockLen;

				if ((blockLen - boffset) != 6)
					return -1004;

				sync.magic = (UINT32) * ((UINT32*) &block[boffset + 0]); /* magic (4 bytes) */
				sync.version = (UINT32) * ((UINT16*) &block[boffset +
				                           4]); /* version (2 bytes) */
				boffset += 6;

				if (sync.magic != 0xCACCACCA)
					return -1005;

				if (sync.version != 0x0100)
					return -1006;

				break;

			case PROGRESSIVE_WBT_FRAME_BEGIN:
				frameBegin.blockType = blockType;
				frameBegin.blockLen = blockLen;

				if ((blockLen - boffset) < 6)
					return -1007;

				frameBegin.frameIndex = (UINT32) * ((UINT32*) &block[boffset +
				                                    0]); /* frameIndex (4 bytes) */
				frameBegin.regionCount = (UINT32) * ((UINT16*) &block[boffset +
				                                     4]); /* regionCount (2 bytes) */
				boffset += 6;
				WLog_Print(progressive->log, WLOG_DEBUG,
				           "ProgressiveFrameBegin: frameIndex: %"PRIu32" regionCount: %"PRIu16"",
				           frameBegin.frameIndex, frameBegin.regionCount);
				/**
				 * If the number of elements specified by the regionCount field is
				 * larger than the actual number of elements in the regions field,
				 * the decoder SHOULD ignore this inconsistency.
				 */
				break;

			case PROGRESSIVE_WBT_FRAME_END:
				WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveFrameEnd");
				frameEnd.blockType = blockType;
				frameEnd.blockLen = blockLen;

				if ((blockLen - boffset) != 0)
					return -1008;

				break;

			case PROGRESSIVE_WBT_CONTEXT:
				context.blockType = blockType;
				context.blockLen = blockLen;

				if ((blockLen - boffset) != 4)
					return -1009;

				context.ctxId = block[boffset + 0]; /* ctxId (1 byte) */
				context.tileSize = *((UINT16*) &block[boffset + 1]); /* tileSize (2 bytes) */
				context.flags = block[boffset + 3]; /* flags (1 byte) */
				boffset += 4;

				if (context.tileSize != 64)
					return -1010;

				WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveContext: flags: 0x%02"PRIX8"", context.flags);

				if (!(context.flags & RFX_SUBBAND_DIFFING))
					WLog_Print(progressive->log, WLOG_WARN, "RFX_SUBBAND_DIFFING is not set");

				break;

			case PROGRESSIVE_WBT_REGION:
				region->blockType = blockType;
				region->blockLen = blockLen;

				if ((blockLen - boffset) < 12)
					return -1011;

				region->tileSize = block[boffset + 0]; /* tileSize (1 byte) */
				region->numRects = *((UINT16*) &block[boffset + 1]); /* numRects (2 bytes) */
				region->numQuant = block[boffset + 3]; /* numQuant (1 byte) */
				region->numProgQuant = block[boffset + 4]; /* numProgQuant (1 byte) */
				region->flags = block[boffset + 5]; /* flags (1 byte) */
				region->numTiles = *((UINT16*) &block[boffset + 6]); /* numTiles (2 bytes) */
				region->tileDataSize = *((UINT32*) &block[boffset +
				                         8]); /* tileDataSize (4 bytes) */
				boffset += 12;

				if (region->tileSize != 64)
					return -1012;

				if (region->numRects < 1)
					return -1013;

				if (region->numQuant > 7)
					return -1014;

				if ((blockLen - boffset) < (region->numRects * 8))
					return -1015;

				if (region->numRects > progressive->cRects)
				{
					RFX_RECT* tmpBuf = (RFX_RECT*) realloc(progressive->rects,
					               region->numRects * sizeof(RFX_RECT));
					if (!tmpBuf)
						return -1016;

					progressive->rects = tmpBuf;
					progressive->cRects = region->numRects;
				}

				region->rects = progressive->rects;

				if (!region->rects)
					return -1017;

				for (index = 0; index < region->numRects; index++)
				{
					rect = &(region->rects[index]);
					rect->x = *((UINT16*) &block[boffset + 0]);
					rect->y = *((UINT16*) &block[boffset + 2]);
					rect->width = *((UINT16*) &block[boffset + 4]);
					rect->height = *((UINT16*) &block[boffset + 6]);
					boffset += 8;
				}

				if ((blockLen - boffset) < (region->numQuant * 5))
					return -1018;

				if (region->numQuant > progressive->cQuant)
				{
					RFX_COMPONENT_CODEC_QUANT* tmpBuf = (RFX_COMPONENT_CODEC_QUANT*) realloc(
					                             progressive->quantVals,
					                             region->numQuant * sizeof(RFX_COMPONENT_CODEC_QUANT));
					if (!tmpBuf)
						return -1019;

					progressive->quantVals = tmpBuf;
					progressive->cQuant = region->numQuant;
				}

				region->quantVals = progressive->quantVals;

				if (!region->quantVals)
					return -1020;

				for (index = 0; index < region->numQuant; index++)
				{
					quantVal = &(region->quantVals[index]);
					progressive_component_codec_quant_read(&block[boffset], quantVal);
					boffset += 5;

					if (!progressive_rfx_quant_lcmp_greater_equal(quantVal, 6))
						return -1;

					if (!progressive_rfx_quant_lcmp_less_equal(quantVal, 15))
						return -1;
				}

				if ((blockLen - boffset) < (region->numProgQuant * 16))
					return -1021;

				if (region->numProgQuant > progressive->cProgQuant)
				{
					RFX_PROGRESSIVE_CODEC_QUANT* tmpBuf = (RFX_PROGRESSIVE_CODEC_QUANT*) realloc(
					                                 progressive->quantProgVals,
					                                 region->numProgQuant * sizeof(RFX_PROGRESSIVE_CODEC_QUANT));
					if (!tmpBuf)
						return -1022;

					progressive->quantProgVals = tmpBuf;
					progressive->cProgQuant = region->numProgQuant;
				}

				region->quantProgVals = progressive->quantProgVals;

				if (!region->quantProgVals)
					return -1023;

				for (index = 0; index < region->numProgQuant; index++)
				{
					quantProgVal = &(region->quantProgVals[index]);
					quantProgVal->quality = block[boffset + 0];
					progressive_component_codec_quant_read(&block[boffset + 1],
					                                       &(quantProgVal->yQuantValues));
					progressive_component_codec_quant_read(&block[boffset + 6],
					                                       &(quantProgVal->cbQuantValues));
					progressive_component_codec_quant_read(&block[boffset + 11],
					                                       &(quantProgVal->crQuantValues));
					boffset += 16;
				}

				if ((blockLen - boffset) < region->tileDataSize)
					return -1024;

				if (progressive->cTiles < surface->gridSize)
				{
					RFX_PROGRESSIVE_TILE** tmpBuf = (RFX_PROGRESSIVE_TILE**) realloc(progressive->tiles,
					                     surface->gridSize * sizeof(RFX_PROGRESSIVE_TILE*));
					if (!tmpBuf)
						return -1025;

					progressive->tiles = tmpBuf;
					progressive->cTiles = surface->gridSize;
				}

				region->tiles = progressive->tiles;

				if (!region->tiles)
					return -1;

				WLog_Print(progressive->log, WLOG_DEBUG,
				           "ProgressiveRegion: numRects: %"PRIu16" numTiles: %"PRIu16" tileDataSize: %"PRIu32" flags: 0x%02"PRIX8" numQuant: %"PRIu8" numProgQuant: %"PRIu8"",
				           region->numRects, region->numTiles, region->tileDataSize, region->flags,
				           region->numQuant, region->numProgQuant);

				if (!(region->flags & RFX_DWT_REDUCE_EXTRAPOLATE))
					WLog_Print(progressive->log, WLOG_WARN, "RFX_DWT_REDUCE_EXTRAPOLATE is not set");

				boxLeft = surface->gridWidth;
				boxTop = surface->gridHeight;
				boxRight = 0;
				boxBottom = 0;

				for (index = 0; index < region->numRects; index++)
				{
					rect = &(region->rects[index]);
					idxLeft = rect->x / 64;
					idxTop = rect->y / 64;
					idxRight = (rect->x + rect->width + 63) / 64;
					idxBottom = (rect->y + rect->height + 63) / 64;

					if (idxLeft < boxLeft)
						boxLeft = idxLeft;

					if (idxTop < boxTop)
						boxTop = idxTop;

					if (idxRight > boxRight)
						boxRight = idxRight;

					if (idxBottom > boxBottom)
						boxBottom = idxBottom;

					WLog_Print(progressive->log, WLOG_DEBUG,
					           "rect[%"PRIu16"]: x: %"PRIu16" y: %"PRIu16" w: %"PRIu16" h: %"PRIu16"",
					           index, rect->x, rect->y, rect->width, rect->height);
				}

				status = progressive_process_tiles(progressive, &block[boffset],
				                                   region->tileDataSize, surface);

				if (status < 0)
					return status;

				region->numTiles = 0;

				for (index = 0; index < surface->gridSize; index++)
				{
					RFX_PROGRESSIVE_TILE* tile = &(surface->tiles[index]);

					if (!tile->data)
						continue;

					if ((tile->xIdx < boxLeft) || (tile->xIdx > boxRight))
						continue;

					if ((tile->yIdx < boxTop) || (tile->yIdx > boxBottom))
						continue;

					region->tiles[region->numTiles++] = tile;
				}

				boffset += (UINT32) status;
				break;

			default:
				return -1039;
				break;
		}

		if (boffset != blockLen)
			return -1040;

		offset += blockLen;
		count++;
	}

	if (offset != blocksLen)
		return -1041;

	region = &(progressive->region);
	region16_init(&clippingRects);

	for (i = 0; i < region->numRects; i++)
	{
		RECTANGLE_16 clippingRect;
		const RFX_RECT* rect = &(region->rects[i]);
		clippingRect.left = nXDst + rect->x;
		clippingRect.top = nYDst + rect->y;
		clippingRect.right = clippingRect.left + rect->width;
		clippingRect.bottom = clippingRect.top + rect->height;
		region16_union_rect(&clippingRects, &clippingRects, &clippingRect);
	}

	for (i = 0; i < region->numTiles; i++)
	{
		UINT32 nbUpdateRects;
		const RECTANGLE_16* updateRects;
		RECTANGLE_16 updateRect;
		RFX_PROGRESSIVE_TILE* tile = region->tiles[i];
		updateRect.left = nXDst + tile->x;
		updateRect.top = nYDst + tile->y;
		updateRect.right = updateRect.left + 64;
		updateRect.bottom = updateRect.top + 64;
		region16_init(&updateRegion);
		region16_intersect_rect(&updateRegion, &clippingRects, &updateRect);
		updateRects = region16_rects(&updateRegion, &nbUpdateRects);

		for (j = 0; j < nbUpdateRects; j++)
		{
			const RECTANGLE_16* rect = &updateRects[j];
			const UINT32 nXSrc = rect->left - (nXDst + tile->x);
			const UINT32 nYSrc = rect->top - (nYDst + tile->y);
			const UINT32 width = rect->right - rect->left;
			const UINT32 height = rect->bottom - rect->top;

			if (!freerdp_image_copy(pDstData, DstFormat, nDstStep,
			                        rect->left, rect->top,
			                        width, height, tile->data, tile->format,
			                        tile->stride,
			                        nXSrc, nYSrc, NULL, FREERDP_FLIP_NONE))
			{
				rc = -42;
				break;
			}

			if (invalidRegion)
				region16_union_rect(invalidRegion, invalidRegion, rect);
		}

		region16_uninit(&updateRegion);
	}

	region16_uninit(&clippingRects);
	return rc;
}

int progressive_compress(PROGRESSIVE_CONTEXT* progressive, const BYTE* pSrcData,
                         UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return -1;
}

BOOL progressive_context_reset(PROGRESSIVE_CONTEXT* progressive)
{
	if (!progressive)
		return FALSE;

	return TRUE;
}

PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor)
{
	PROGRESSIVE_CONTEXT* progressive;
	progressive = (PROGRESSIVE_CONTEXT*) calloc(1, sizeof(PROGRESSIVE_CONTEXT));

	if (progressive)
	{
		progressive->Compressor = Compressor;
		progressive->bufferPool = BufferPool_New(TRUE, (8192 + 32) * 3, 16);
		progressive->cRects = 64;
		progressive->rects = (RFX_RECT*) calloc(progressive->cRects, sizeof(RFX_RECT));

		if (!progressive->rects)
			goto cleanup;

		progressive->cTiles = 64;
		progressive->tiles = (RFX_PROGRESSIVE_TILE**) calloc(progressive->cTiles,
		                     sizeof(RFX_PROGRESSIVE_TILE*));

		if (!progressive->tiles)
			goto cleanup;

		progressive->cQuant = 8;
		progressive->quantVals = (RFX_COMPONENT_CODEC_QUANT*) calloc(
		                             progressive->cQuant, sizeof(RFX_COMPONENT_CODEC_QUANT));

		if (!progressive->quantVals)
			goto cleanup;

		progressive->cProgQuant = 8;
		progressive->quantProgVals = (RFX_PROGRESSIVE_CODEC_QUANT*) calloc(
		                                 progressive->cProgQuant, sizeof(RFX_PROGRESSIVE_CODEC_QUANT));

		if (!progressive->quantProgVals)
			goto cleanup;

		ZeroMemory(&(progressive->quantProgValFull),
		           sizeof(RFX_PROGRESSIVE_CODEC_QUANT));
		progressive->quantProgValFull.quality = 100;
		progressive->SurfaceContexts = HashTable_New(TRUE);
		progressive_context_reset(progressive);
		progressive->log = WLog_Get(TAG);
	}

	return progressive;
cleanup:
	progressive_context_free(progressive);
	return NULL;
}

void progressive_context_free(PROGRESSIVE_CONTEXT* progressive)
{
	int count;
	int index;
	ULONG_PTR* pKeys = NULL;
	PROGRESSIVE_SURFACE_CONTEXT* surface;

	if (!progressive)
		return;

	BufferPool_Free(progressive->bufferPool);
	free(progressive->rects);
	free(progressive->tiles);
	free(progressive->quantVals);
	free(progressive->quantProgVals);

	if (progressive->SurfaceContexts)
	{
		count = HashTable_GetKeys(progressive->SurfaceContexts, &pKeys);

		for (index = 0; index < count; index++)
		{
			surface = (PROGRESSIVE_SURFACE_CONTEXT*) HashTable_GetItemValue(progressive->SurfaceContexts, (void*) pKeys[index]);
			progressive_surface_context_free(surface);
		}

		free(pKeys);
		HashTable_Free(progressive->SurfaceContexts);
	}

	free(progressive);
}

