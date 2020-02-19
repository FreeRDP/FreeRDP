/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Progressive Codec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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
#include "rfx_dwt.h"
#include "rfx_rlgr.h"
#include "progressive.h"

#define TAG FREERDP_TAG("codec.progressive")

struct _RFX_PROGRESSIVE_UPGRADE_STATE
{
	BOOL nonLL;
	wBitStream* srl;
	wBitStream* raw;

	/* SRL state */

	UINT32 kp;
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
}

static INLINE void progressive_component_codec_quant_read(wStream* s,
                                                          RFX_COMPONENT_CODEC_QUANT* quantVal)
{
	BYTE b;
	Stream_Read_UINT8(s, b);
	quantVal->LL3 = b & 0x0F;
	quantVal->HL3 = b >> 4;
	Stream_Read_UINT8(s, b);
	quantVal->LH3 = b & 0x0F;
	quantVal->HH3 = b >> 4;
	Stream_Read_UINT8(s, b);
	quantVal->HL2 = b & 0x0F;
	quantVal->LH2 = b >> 4;
	Stream_Read_UINT8(s, b);
	quantVal->HH2 = b & 0x0F;
	quantVal->HL1 = b >> 4;
	Stream_Read_UINT8(s, b);
	quantVal->LH1 = b & 0x0F;
	quantVal->HH1 = b >> 4;
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

static INLINE void progressive_rfx_quant_add(const RFX_COMPONENT_CODEC_QUANT* q1,
                                             const RFX_COMPONENT_CODEC_QUANT* q2,
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

static INLINE void progressive_rfx_quant_sub(const RFX_COMPONENT_CODEC_QUANT* q1,
                                             const RFX_COMPONENT_CODEC_QUANT* q2,
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

static INLINE BOOL progressive_rfx_quant_lcmp_less_equal(const RFX_COMPONENT_CODEC_QUANT* q,
                                                         int val)
{
	if (q->HL1 > val)
		return FALSE; /* HL1 */

	if (q->LH1 > val)
		return FALSE; /* LH1 */

	if (q->HH1 > val)
		return FALSE; /* HH1 */

	if (q->HL2 > val)
		return FALSE; /* HL2 */

	if (q->LH2 > val)
		return FALSE; /* LH2 */

	if (q->HH2 > val)
		return FALSE; /* HH2 */

	if (q->HL3 > val)
		return FALSE; /* HL3 */

	if (q->LH3 > val)
		return FALSE; /* LH3 */

	if (q->HH3 > val)
		return FALSE; /* HH3 */

	if (q->LL3 > val)
		return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_less_equal(const RFX_COMPONENT_CODEC_QUANT* q1,
                                                        const RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 > q2->HL1)
		return FALSE; /* HL1 */

	if (q1->LH1 > q2->LH1)
		return FALSE; /* LH1 */

	if (q1->HH1 > q2->HH1)
		return FALSE; /* HH1 */

	if (q1->HL2 > q2->HL2)
		return FALSE; /* HL2 */

	if (q1->LH2 > q2->LH2)
		return FALSE; /* LH2 */

	if (q1->HH2 > q2->HH2)
		return FALSE; /* HH2 */

	if (q1->HL3 > q2->HL3)
		return FALSE; /* HL3 */

	if (q1->LH3 > q2->LH3)
		return FALSE; /* LH3 */

	if (q1->HH3 > q2->HH3)
		return FALSE; /* HH3 */

	if (q1->LL3 > q2->LL3)
		return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_lcmp_greater_equal(const RFX_COMPONENT_CODEC_QUANT* q,
                                                            int val)
{
	if (q->HL1 < val)
		return FALSE; /* HL1 */

	if (q->LH1 < val)
		return FALSE; /* LH1 */

	if (q->HH1 < val)
		return FALSE; /* HH1 */

	if (q->HL2 < val)
		return FALSE; /* HL2 */

	if (q->LH2 < val)
		return FALSE; /* LH2 */

	if (q->HH2 < val)
		return FALSE; /* HH2 */

	if (q->HL3 < val)
		return FALSE; /* HL3 */

	if (q->LH3 < val)
		return FALSE; /* LH3 */

	if (q->HH3 < val)
		return FALSE; /* HH3 */

	if (q->LL3 < val)
		return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_greater_equal(const RFX_COMPONENT_CODEC_QUANT* q1,
                                                           const RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 < q2->HL1)
		return FALSE; /* HL1 */

	if (q1->LH1 < q2->LH1)
		return FALSE; /* LH1 */

	if (q1->HH1 < q2->HH1)
		return FALSE; /* HH1 */

	if (q1->HL2 < q2->HL2)
		return FALSE; /* HL2 */

	if (q1->LH2 < q2->LH2)
		return FALSE; /* LH2 */

	if (q1->HH2 < q2->HH2)
		return FALSE; /* HH2 */

	if (q1->HL3 < q2->HL3)
		return FALSE; /* HL3 */

	if (q1->LH3 < q2->LH3)
		return FALSE; /* LH3 */

	if (q1->HH3 < q2->HH3)
		return FALSE; /* HH3 */

	if (q1->LL3 < q2->LL3)
		return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_rfx_quant_cmp_equal(const RFX_COMPONENT_CODEC_QUANT* q1,
                                                   const RFX_COMPONENT_CODEC_QUANT* q2)
{
	if (q1->HL1 != q2->HL1)
		return FALSE; /* HL1 */

	if (q1->LH1 != q2->LH1)
		return FALSE; /* LH1 */

	if (q1->HH1 != q2->HH1)
		return FALSE; /* HH1 */

	if (q1->HL2 != q2->HL2)
		return FALSE; /* HL2 */

	if (q1->LH2 != q2->LH2)
		return FALSE; /* LH2 */

	if (q1->HH2 != q2->HH2)
		return FALSE; /* HH2 */

	if (q1->HL3 != q2->HL3)
		return FALSE; /* HL3 */

	if (q1->LH3 != q2->LH3)
		return FALSE; /* LH3 */

	if (q1->HH3 != q2->HH3)
		return FALSE; /* HH3 */

	if (q1->LL3 != q2->LL3)
		return FALSE; /* LL3 */

	return TRUE;
}

static INLINE BOOL progressive_set_surface_data(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId,
                                                void* pData)
{
	ULONG_PTR key;
	key = ((ULONG_PTR)surfaceId) + 1;

	if (pData)
		return HashTable_Add(progressive->SurfaceContexts, (void*)key, pData) >= 0;

	HashTable_Remove(progressive->SurfaceContexts, (void*)key);
	return TRUE;
}

static INLINE PROGRESSIVE_SURFACE_CONTEXT*
progressive_get_surface_data(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId)
{
	void* key = (void*)(((ULONG_PTR)surfaceId) + 1);
	void* pData = NULL;

	if (!progressive)
		return NULL;

	pData = HashTable_GetItemValue(progressive->SurfaceContexts, key);
	return pData;
}

static void progressive_tile_free(RFX_PROGRESSIVE_TILE* tile)
{
	if (tile)
	{
		_aligned_free(tile->sign);
		_aligned_free(tile->current);
		_aligned_free(tile->data);
	}
}

static void progressive_surface_context_free(PROGRESSIVE_SURFACE_CONTEXT* surface)
{
	UINT32 index;

	for (index = 0; index < surface->gridSize; index++)
	{
		RFX_PROGRESSIVE_TILE* tile = &(surface->tiles[index]);
		progressive_tile_free(tile);
	}

	free(surface->tiles);
	free(surface);
}

static INLINE BOOL progressive_tile_allocate(RFX_PROGRESSIVE_TILE* tile)
{
	BOOL rc = FALSE;
	if (!tile)
		return FALSE;

	tile->width = 64;
	tile->height = 64;
	tile->stride = 4 * tile->width;

	{
		size_t dataLen = tile->stride * tile->height;
		tile->data = (BYTE*)_aligned_malloc(dataLen, 16);
	}

	{
		size_t signLen = (8192 + 32) * 3;
		tile->sign = (BYTE*)_aligned_malloc(signLen, 16);
	}

	{
		size_t currentLen = (8192 + 32) * 3;
		tile->current = (BYTE*)_aligned_malloc(currentLen, 16);
	}

	rc = tile->data && tile->sign && tile->current;

	if (!rc)
		progressive_tile_free(tile);

	return rc;
}

static PROGRESSIVE_SURFACE_CONTEXT* progressive_surface_context_new(UINT16 surfaceId, UINT32 width,
                                                                    UINT32 height)
{
	UINT32 x;
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	surface = (PROGRESSIVE_SURFACE_CONTEXT*)calloc(1, sizeof(PROGRESSIVE_SURFACE_CONTEXT));

	if (!surface)
		return NULL;

	surface->id = surfaceId;
	surface->width = width;
	surface->height = height;
	surface->gridWidth = (width + (64 - width % 64)) / 64;
	surface->gridHeight = (height + (64 - height % 64)) / 64;
	surface->gridSize = surface->gridWidth * surface->gridHeight;
	surface->tiles = (RFX_PROGRESSIVE_TILE*)calloc(surface->gridSize, sizeof(RFX_PROGRESSIVE_TILE));

	if (!surface->tiles)
	{
		free(surface);
		return NULL;
	}
	for (x = 0; x < surface->gridSize; x++)
	{
		if (!progressive_tile_allocate(&surface->tiles[x]))
		{
			progressive_surface_context_free(surface);
			return NULL;
		}
	}

	return surface;
}

static BOOL progressive_surface_tile_replace(PROGRESSIVE_SURFACE_CONTEXT* surface,
                                             PROGRESSIVE_BLOCK_REGION* region,
                                             const RFX_PROGRESSIVE_TILE* tile, BOOL upgrade)
{
	RFX_PROGRESSIVE_TILE* t;

	size_t zIdx;
	if (!surface || !tile)
		return FALSE;

	zIdx = (tile->yIdx * surface->gridWidth) + tile->xIdx;

	if (zIdx >= surface->gridSize)
	{
		WLog_ERR(TAG, "Invalid zIndex %" PRIuz, zIdx);
		return FALSE;
	}

	t = &surface->tiles[zIdx];

	if (upgrade)
	{
		t->blockType = tile->blockType;
		t->blockLen = tile->blockLen;
		t->quantIdxY = tile->quantIdxY;
		t->quantIdxCb = tile->quantIdxCb;
		t->quantIdxCr = tile->quantIdxCr;
		t->xIdx = tile->xIdx;
		t->yIdx = tile->yIdx;
		t->flags = tile->flags;
		t->quality = tile->quality;
		t->ySrlLen = tile->ySrlLen;
		t->yRawLen = tile->yRawLen;
		t->cbSrlLen = tile->cbSrlLen;
		t->cbRawLen = tile->cbRawLen;
		t->crSrlLen = tile->crSrlLen;
		t->crRawLen = tile->crRawLen;
		t->ySrlData = tile->ySrlData;
		t->yRawData = tile->yRawData;
		t->cbSrlData = tile->cbSrlData;
		t->cbRawData = tile->cbRawData;
		t->crSrlData = tile->crSrlData;
		t->crRawData = tile->crRawData;
		t->x = tile->xIdx * t->width;
		t->y = tile->yIdx * t->height;
	}
	else
	{
		t->blockType = tile->blockType;
		t->blockLen = tile->blockLen;
		t->quantIdxY = tile->quantIdxY;
		t->quantIdxCb = tile->quantIdxCb;
		t->quantIdxCr = tile->quantIdxCr;
		t->xIdx = tile->xIdx;
		t->yIdx = tile->yIdx;
		t->flags = tile->flags;
		t->quality = tile->quality;
		t->yLen = tile->yLen;
		t->cbLen = tile->cbLen;
		t->crLen = tile->crLen;
		t->tailLen = tile->tailLen;
		t->yData = tile->yData;
		t->cbData = tile->cbData;
		t->crData = tile->crData;
		t->tailData = tile->tailData;
		t->x = tile->xIdx * t->width;
		t->y = tile->yIdx * t->height;
	}

	if (region->usedTiles >= region->numTiles)
	{
		WLog_ERR(TAG, "Invalid tile count, only expected %" PRIu16 ", got %" PRIu16,
		         region->numTiles, region->usedTiles);
		return FALSE;
	}

	region->tiles[region->usedTiles++] = t;
	return TRUE;
}

INT32 progressive_create_surface_context(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId,
                                         UINT32 width, UINT32 height)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	surface = progressive_get_surface_data(progressive, surfaceId);

	if (!surface)
	{
		surface = progressive_surface_context_new(surfaceId, width, height);

		if (!surface)
			return -1;

		if (!progressive_set_surface_data(progressive, surfaceId, (void*)surface))
		{
			progressive_surface_context_free(surface);
			return -1;
		}
	}

	return 1;
}

int progressive_delete_surface_context(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface = progressive_get_surface_data(progressive, surfaceId);

	if (surface)
	{
		progressive_set_surface_data(progressive, surfaceId, NULL);
		progressive_surface_context_free(surface);
	}

	return 1;
}

/*
 * Band	    Offset      Dimensions  Size
 *
 * HL1      0           31x33       1023
 * LH1      1023        33x31       1023
 * HH1      2046        31x31       961
 *
 * HL2      3007        16x17       272
 * LH2      3279        17x16       272
 * HH2      3551        16x16       256
 *
 * HL3      3807        8x9         72
 * LH3      3879        9x8         72
 * HH3      3951        8x8         64
 *
 * LL3      4015        9x9         81
 */

static INLINE void progressive_rfx_idwt_x(const INT16* pLowBand, size_t nLowStep,
                                          const INT16* pHighBand, size_t nHighStep, INT16* pDstBand,
                                          size_t nDstStep, size_t nLowCount, size_t nHighCount,
                                          size_t nDstCount)
{
	size_t i;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;

	for (i = 0; i < nDstCount; i++)
	{
		size_t j;
		const INT16* pL = pLowBand;
		const INT16* pH = pHighBand;
		INT16* pX = pDstBand;
		H0 = *pH++;
		L0 = *pL++;
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

static INLINE void progressive_rfx_idwt_y(const INT16* pLowBand, size_t nLowStep,
                                          const INT16* pHighBand, size_t nHighStep, INT16* pDstBand,
                                          size_t nDstStep, size_t nLowCount, size_t nHighCount,
                                          size_t nDstCount)
{
	size_t i;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;

	for (i = 0; i < nDstCount; i++)
	{
		size_t j;
		const INT16* pL = pLowBand;
		const INT16* pH = pHighBand;
		INT16* pX = pDstBand;
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

static INLINE size_t progressive_rfx_get_band_l_count(size_t level)
{
	return (64 >> level) + 1;
}

static INLINE size_t progressive_rfx_get_band_h_count(size_t level)
{
	if (level == 1)
		return (64 >> 1) - 1;
	else
		return (64 + (1 << (level - 1))) >> level;
}

static INLINE void progressive_rfx_dwt_2d_decode_block(INT16* buffer, INT16* temp, size_t level)
{
	size_t nDstStepX;
	size_t nDstStepY;
	INT16 *HL, *LH;
	INT16 *HH, *LL;
	INT16 *L, *H, *LLx;

	const size_t nBandL = progressive_rfx_get_band_l_count(level);
	const size_t nBandH = progressive_rfx_get_band_h_count(level);
	size_t offset = 0;

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
	progressive_rfx_idwt_x(LL, nBandL, HL, nBandH, L, nDstStepX, nBandL, nBandH, nBandL);

	/* horizontal (LH + HH -> H) */
	progressive_rfx_idwt_x(LH, nBandL, HH, nBandH, H, nDstStepX, nBandL, nBandH, nBandH);

	/* vertical (L + H -> LL) */
	progressive_rfx_idwt_y(L, nDstStepX, H, nDstStepX, LLx, nDstStepY, nBandL, nBandH,
	                       nBandL + nBandH);
}

static INLINE int progressive_rfx_dwt_2d_decode(PROGRESSIVE_CONTEXT* progressive, INT16* buffer,
                                                INT16* current, BOOL coeffDiff, BOOL extrapolate,
                                                BOOL reverse)
{
	const primitives_t* prims = primitives_get();
	INT16* temp;

	if (!progressive || !buffer || !current)
		return -1;

	if (coeffDiff)
		prims->add_16s(buffer, current, buffer, 4096);

	if (reverse)
		CopyMemory(buffer, current, 4096 * 2);
	else
		CopyMemory(current, buffer, 4096 * 2);
	temp = (INT16*)BufferPool_Take(progressive->bufferPool, -1); /* DWT buffer */
	if (!temp)
		return -2;
	if (!extrapolate)
	{
		rfx_dwt_2d_decode_block(&buffer[3840], temp, 8);
		rfx_dwt_2d_decode_block(&buffer[3072], temp, 16);
		rfx_dwt_2d_decode_block(&buffer[0], temp, 32);
	}
	else
	{
		progressive_rfx_dwt_2d_decode_block(&buffer[3807], temp, 3);
		progressive_rfx_dwt_2d_decode_block(&buffer[3007], temp, 2);
		progressive_rfx_dwt_2d_decode_block(&buffer[0], temp, 1);
	}
	BufferPool_Return(progressive->bufferPool, temp);
	return 1;
}

static INLINE void progressive_rfx_decode_block(const primitives_t* prims, INT16* buffer,
                                                UINT32 length, UINT32 shift)
{
	if (!shift)
		return;

	prims->lShiftC_16s(buffer, shift, buffer, length);
}

static INLINE int progressive_rfx_decode_component(PROGRESSIVE_CONTEXT* progressive,
                                                   const RFX_COMPONENT_CODEC_QUANT* shift,
                                                   const BYTE* data, UINT32 length, INT16* buffer,
                                                   INT16* current, INT16* sign, BOOL coeffDiff,
                                                   BOOL subbandDiff, BOOL extrapolate)
{
	int status;
	const primitives_t* prims = primitives_get();

	if (!subbandDiff)
		WLog_WARN(TAG, "PROGRESSIVE_BLOCK_CONTEXT::flags & RFX_SUBBAND_DIFFING not set");

	status = rfx_rlgr_decode(RLGR1, data, length, buffer, 4096);

	if (status < 0)
		return status;

	CopyMemory(sign, buffer, 4096 * 2);

	progressive_rfx_decode_block(prims, &buffer[0], 1023, shift->HL1);    /* HL1 */
	progressive_rfx_decode_block(prims, &buffer[1023], 1023, shift->LH1); /* LH1 */
	progressive_rfx_decode_block(prims, &buffer[2046], 961, shift->HH1);  /* HH1 */
	progressive_rfx_decode_block(prims, &buffer[3007], 272, shift->HL2);  /* HL2 */
	progressive_rfx_decode_block(prims, &buffer[3279], 272, shift->LH2);  /* LH2 */
	progressive_rfx_decode_block(prims, &buffer[3551], 256, shift->HH2);  /* HH2 */
	progressive_rfx_decode_block(prims, &buffer[3807], 72, shift->HL3);   /* HL3 */
	progressive_rfx_decode_block(prims, &buffer[3879], 72, shift->LH3);   /* LH3 */
	progressive_rfx_decode_block(prims, &buffer[3951], 64, shift->HH3);   /* HH3 */
	rfx_differential_decode(&buffer[4015], 81);                           /* LL3 */
	progressive_rfx_decode_block(prims, &buffer[4015], 81, shift->LL3);   /* LL3 */

	return progressive_rfx_dwt_2d_decode(progressive, buffer, current, coeffDiff, extrapolate,
	                                     FALSE);
}

static INLINE int progressive_decompress_tile_first(PROGRESSIVE_CONTEXT* progressive,
                                                    RFX_PROGRESSIVE_TILE* tile,
                                                    PROGRESSIVE_BLOCK_REGION* region,
                                                    const PROGRESSIVE_BLOCK_CONTEXT* context)
{
	int rc;
	BOOL diff, sub, extrapolate;
	BYTE* pBuffer;
	INT16* pSign[3];
	INT16* pSrcDst[3];
	INT16* pCurrent[3];
	RFX_COMPONENT_CODEC_QUANT shiftY = { 0 };
	RFX_COMPONENT_CODEC_QUANT shiftCb = { 0 };
	RFX_COMPONENT_CODEC_QUANT shiftCr = { 0 };
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
	sub = context->flags & RFX_SUBBAND_DIFFING;
	extrapolate = region->flags & RFX_DWT_REDUCE_EXTRAPOLATE;

	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveTile%s: quantIdx Y: %" PRIu8 " Cb: %" PRIu8 " Cr: %" PRIu8
	           " xIdx: %" PRIu16 " yIdx: %" PRIu16 " flags: 0x%02" PRIX8 " quality: %" PRIu8
	           " yLen: %" PRIu16 " cbLen: %" PRIu16 " crLen: %" PRIu16 " tailLen: %" PRIu16 "",
	           (tile->blockType == PROGRESSIVE_WBT_TILE_FIRST) ? "First" : "Simple",
	           tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx, tile->yIdx,
	           tile->flags, tile->quality, tile->yLen, tile->cbLen, tile->crLen, tile->tailLen);

	if (tile->quantIdxY >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxY %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxY, region->numQuant);
		return -1;
	}

	quantY = &(region->quantVals[tile->quantIdxY]);

	if (tile->quantIdxCb >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxCb %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxCb,
		         region->numQuant);
		return -1;
	}

	quantCb = &(region->quantVals[tile->quantIdxCb]);

	if (tile->quantIdxCr >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxCr %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxCr,
		         region->numQuant);
		return -1;
	}

	quantCr = &(region->quantVals[tile->quantIdxCr]);

	if (tile->quality == 0xFF)
	{
		quantProgVal = &(progressive->quantProgValFull);
	}
	else
	{
		if (tile->quality >= region->numProgQuant)
		{
			WLog_ERR(TAG, "quality %" PRIu8 " > numProgQuant %" PRIu8, tile->quality,
			         region->numProgQuant);
			return -1;
		}

		quantProgVal = &(region->quantProgVals[tile->quality]);
	}

	quantProgY = &(quantProgVal->yQuantValues);
	quantProgCb = &(quantProgVal->cbQuantValues);
	quantProgCr = &(quantProgVal->crQuantValues);

	tile->yQuant = *quantY;
	tile->cbQuant = *quantCb;
	tile->crQuant = *quantCr;
	tile->yProgQuant = *quantProgY;
	tile->cbProgQuant = *quantProgCb;
	tile->crProgQuant = *quantProgCr;

	progressive_rfx_quant_add(quantY, quantProgY, &(tile->yBitPos));
	progressive_rfx_quant_add(quantCb, quantProgCb, &(tile->cbBitPos));
	progressive_rfx_quant_add(quantCr, quantProgCr, &(tile->crBitPos));
	progressive_rfx_quant_add(quantY, quantProgY, &shiftY);
	progressive_rfx_quant_lsub(&shiftY, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCb, quantProgCb, &shiftCb);
	progressive_rfx_quant_lsub(&shiftCb, 1); /* -6 + 5 = -1 */
	progressive_rfx_quant_add(quantCr, quantProgCr, &shiftCr);
	progressive_rfx_quant_lsub(&shiftCr, 1); /* -6 + 5 = -1 */

	pSign[0] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSign[1] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSign[2] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pCurrent[0] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = (BYTE*)BufferPool_Take(progressive->bufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	rc = progressive_rfx_decode_component(progressive, &shiftY, tile->yData, tile->yLen, pSrcDst[0],
	                                      pCurrent[0], pSign[0], diff, sub, extrapolate); /* Y */
	if (rc < 0)
		goto fail;
	rc = progressive_rfx_decode_component(progressive, &shiftCb, tile->cbData, tile->cbLen,
	                                      pSrcDst[1], pCurrent[1], pSign[1], diff, sub,
	                                      extrapolate); /* Cb */
	if (rc < 0)
		goto fail;
	rc = progressive_rfx_decode_component(progressive, &shiftCr, tile->crData, tile->crLen,
	                                      pSrcDst[2], pCurrent[2], pSign[2], diff, sub,
	                                      extrapolate); /* Cr */

	if (rc < 0)
		goto fail;

	rc = prims->yCbCrToRGB_16s8u_P3AC4R((const INT16* const*)pSrcDst, 64 * 2, tile->data,
	                                    tile->stride, progressive->format, &roi_64x64);
fail:
	BufferPool_Return(progressive->bufferPool, pBuffer);
	return rc;
}

static INLINE INT16 progressive_rfx_srl_read(RFX_PROGRESSIVE_UPGRADE_STATE* state, UINT32 numBits)
{
	UINT32 k;
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

	if (state->kp < 6)
		state->kp = 0;
	else
		state->kp -= 6;

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
	UINT32 pad;
	wBitStream* srl;
	wBitStream* raw;
	if (!state)
		return -1;

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

static INLINE int progressive_rfx_upgrade_block(RFX_PROGRESSIVE_UPGRADE_STATE* state, INT16* buffer,
                                                INT16* sign, UINT32 length, UINT32 shift,
                                                UINT32 bitPos, UINT32 numBits)
{
	UINT32 index;
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

static INLINE int progressive_rfx_upgrade_component(
    PROGRESSIVE_CONTEXT* progressive, const RFX_COMPONENT_CODEC_QUANT* shift,
    const RFX_COMPONENT_CODEC_QUANT* bitPos, const RFX_COMPONENT_CODEC_QUANT* numBits,
    INT16* buffer, INT16* current, INT16* sign, const BYTE* srlData, UINT32 srlLen,
    const BYTE* rawData, UINT32 rawLen, BOOL coeffDiff, BOOL subbandDiff, BOOL extrapolate)
{
	int rc;
	UINT32 aRawLen;
	UINT32 aSrlLen;
	wBitStream s_srl = { 0 };
	wBitStream s_raw = { 0 };
	RFX_PROGRESSIVE_UPGRADE_STATE state = { 0 };

	state.kp = 8;
	state.mode = 0;
	state.srl = &s_srl;
	state.raw = &s_raw;
	BitStream_Attach(state.srl, srlData, srlLen);
	BitStream_Fetch(state.srl);
	BitStream_Attach(state.raw, rawData, rawLen);
	BitStream_Fetch(state.raw);

	state.nonLL = TRUE;
	rc = progressive_rfx_upgrade_block(&state, &current[0], &sign[0], 1023, shift->HL1, bitPos->HL1,
	                                   numBits->HL1); /* HL1 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[1023], &sign[1023], 1023, shift->LH1,
	                                   bitPos->LH1, numBits->LH1); /* LH1 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[2046], &sign[2046], 961, shift->HH1,
	                                   bitPos->HH1, numBits->HH1); /* HH1 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3007], &sign[3007], 272, shift->HL2,
	                                   bitPos->HL2, numBits->HL2); /* HL2 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3279], &sign[3279], 272, shift->LH2,
	                                   bitPos->LH2, numBits->LH2); /* LH2 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3551], &sign[3551], 256, shift->HH2,
	                                   bitPos->HH2, numBits->HH2); /* HH2 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3807], &sign[3807], 72, shift->HL3,
	                                   bitPos->HL3, numBits->HL3); /* HL3 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3879], &sign[3879], 72, shift->LH3,
	                                   bitPos->LH3, numBits->LH3); /* LH3 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_block(&state, &current[3951], &sign[3951], 64, shift->HH3,
	                                   bitPos->HH3, numBits->HH3); /* HH3 */
	if (rc < 0)
		return rc;

	state.nonLL = FALSE;
	rc = progressive_rfx_upgrade_block(&state, &current[4015], &sign[4015], 81, shift->LL3,
	                                   bitPos->LL3, numBits->LL3); /* LL3 */
	if (rc < 0)
		return rc;
	rc = progressive_rfx_upgrade_state_finish(&state);
	if (rc < 0)
		return rc;
	aRawLen = (state.raw->position + 7) / 8;
	aSrlLen = (state.srl->position + 7) / 8;

	if ((aRawLen != rawLen) || (aSrlLen != srlLen))
	{
		int pRawLen = 0;
		int pSrlLen = 0;

		if (rawLen)
			pRawLen = (int)((((float)aRawLen) / ((float)rawLen)) * 100.0f);

		if (srlLen)
			pSrlLen = (int)((((float)aSrlLen) / ((float)srlLen)) * 100.0f);

		WLog_Print(progressive->log, WLOG_INFO,
		           "RAW: %" PRIu32 "/%" PRIu32 " %d%% (%" PRIu32 "/%" PRIu32 ":%" PRIu32
		           ")\tSRL: %" PRIu32 "/%" PRIu32 " %d%% (%" PRIu32 "/%" PRIu32 ":%" PRIu32 ")",
		           aRawLen, rawLen, pRawLen, state.raw->position, rawLen * 8,
		           (rawLen * 8) - state.raw->position, aSrlLen, srlLen, pSrlLen,
		           state.srl->position, srlLen * 8, (srlLen * 8) - state.srl->position);
		return -1;
	}

	return progressive_rfx_dwt_2d_decode(progressive, buffer, current, coeffDiff, extrapolate,
	                                     TRUE);
}

static INLINE int progressive_decompress_tile_upgrade(PROGRESSIVE_CONTEXT* progressive,
                                                      RFX_PROGRESSIVE_TILE* tile,
                                                      PROGRESSIVE_BLOCK_REGION* region,
                                                      const PROGRESSIVE_BLOCK_CONTEXT* context)
{
	int status;
	BOOL coeffDiff, sub, extrapolate;
	BYTE* pBuffer;
	INT16* pSign[3] = { 0 };
	INT16* pSrcDst[3] = { 0 };
	INT16* pCurrent[3] = { 0 };
	RFX_COMPONENT_CODEC_QUANT shiftY = { 0 };
	RFX_COMPONENT_CODEC_QUANT shiftCb = { 0 };
	RFX_COMPONENT_CODEC_QUANT shiftCr = { 0 };
	RFX_COMPONENT_CODEC_QUANT yBitPos = { 0 };
	RFX_COMPONENT_CODEC_QUANT cbBitPos = { 0 };
	RFX_COMPONENT_CODEC_QUANT crBitPos = { 0 };
	RFX_COMPONENT_CODEC_QUANT yNumBits = { 0 };
	RFX_COMPONENT_CODEC_QUANT cbNumBits = { 0 };
	RFX_COMPONENT_CODEC_QUANT crNumBits = { 0 };
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_COMPONENT_CODEC_QUANT* quantProgY;
	RFX_COMPONENT_CODEC_QUANT* quantProgCb;
	RFX_COMPONENT_CODEC_QUANT* quantProgCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProg;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t* prims = primitives_get();

	coeffDiff = tile->flags & RFX_TILE_DIFFERENCE;
	sub = context->flags & RFX_SUBBAND_DIFFING;
	extrapolate = region->flags & RFX_DWT_REDUCE_EXTRAPOLATE;

	tile->pass++;
	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveTileUpgrade: pass: %" PRIu16 " quantIdx Y: %" PRIu8 " Cb: %" PRIu8
	           " Cr: %" PRIu8 " xIdx: %" PRIu16 " yIdx: %" PRIu16 " quality: %" PRIu8
	           " ySrlLen: %" PRIu16 " yRawLen: %" PRIu16 " cbSrlLen: %" PRIu16 " cbRawLen: %" PRIu16
	           " crSrlLen: %" PRIu16 " crRawLen: %" PRIu16 "",
	           tile->pass, tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx,
	           tile->yIdx, tile->quality, tile->ySrlLen, tile->yRawLen, tile->cbSrlLen,
	           tile->cbRawLen, tile->crSrlLen, tile->crRawLen);

	if (tile->quantIdxY >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxY %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxY, region->numQuant);
		return -1;
	}

	quantY = &(region->quantVals[tile->quantIdxY]);

	if (tile->quantIdxCb >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxCb %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxCb,
		         region->numQuant);
		return -1;
	}

	quantCb = &(region->quantVals[tile->quantIdxCb]);

	if (tile->quantIdxCr >= region->numQuant)
	{
		WLog_ERR(TAG, "quantIdxCr %" PRIu8 " > numQuant %" PRIu8, tile->quantIdxCr,
		         region->numQuant);
		return -1;
	}

	quantCr = &(region->quantVals[tile->quantIdxCr]);

	if (tile->quality == 0xFF)
	{
		quantProg = &(progressive->quantProgValFull);
	}
	else
	{
		if (tile->quality >= region->numProgQuant)
		{
			WLog_ERR(TAG, "quality %" PRIu8 " > numProgQuant %" PRIu8, tile->quality,
			         region->numProgQuant);
			return -1;
		}

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

	if (!(context->flags & RFX_SUBBAND_DIFFING))
		WLog_WARN(TAG, "PROGRESSIVE_BLOCK_CONTEXT::flags & RFX_SUBBAND_DIFFING not set");

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

	tile->yBitPos = yBitPos;
	tile->cbBitPos = cbBitPos;
	tile->crBitPos = crBitPos;
	tile->yQuant = *quantY;
	tile->cbQuant = *quantCb;
	tile->crQuant = *quantCr;
	tile->yProgQuant = *quantProgY;
	tile->cbProgQuant = *quantProgCb;
	tile->crProgQuant = *quantProgCr;

	pSign[0] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSign[1] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSign[2] = (INT16*)((BYTE*)(&tile->sign[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pCurrent[0] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&tile->current[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = (BYTE*)BufferPool_Take(progressive->bufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	status = progressive_rfx_upgrade_component(progressive, &shiftY, quantProgY, &yNumBits,
	                                           pSrcDst[0], pCurrent[0], pSign[0], tile->ySrlData,
	                                           tile->ySrlLen, tile->yRawData, tile->yRawLen,
	                                           coeffDiff, sub, extrapolate); /* Y */

	if (status < 0)
		goto fail;

	status = progressive_rfx_upgrade_component(progressive, &shiftCb, quantProgCb, &cbNumBits,
	                                           pSrcDst[1], pCurrent[1], pSign[1], tile->cbSrlData,
	                                           tile->cbSrlLen, tile->cbRawData, tile->cbRawLen,
	                                           coeffDiff, sub, extrapolate); /* Cb */

	if (status < 0)
		goto fail;

	status = progressive_rfx_upgrade_component(progressive, &shiftCr, quantProgCr, &crNumBits,
	                                           pSrcDst[2], pCurrent[2], pSign[2], tile->crSrlData,
	                                           tile->crSrlLen, tile->crRawData, tile->crRawLen,
	                                           coeffDiff, sub, extrapolate); /* Cr */

	if (status < 0)
		goto fail;

	status = prims->yCbCrToRGB_16s8u_P3AC4R((const INT16* const*)pSrcDst, 64 * 2, tile->data,
	                                        tile->stride, progressive->format, &roi_64x64);
fail:
	BufferPool_Return(progressive->bufferPool, pBuffer);
	return status;
}

static INLINE BOOL progressive_tile_read_upgrade(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                                 UINT16 blockType, UINT32 blockLen,
                                                 PROGRESSIVE_SURFACE_CONTEXT* surface,
                                                 PROGRESSIVE_BLOCK_REGION* region,
                                                 const PROGRESSIVE_BLOCK_CONTEXT* context)
{
	RFX_PROGRESSIVE_TILE tile = { 0 };
	const size_t expect = 20;

	if (Stream_GetRemainingLength(s) < expect)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "Expected %" PRIuz " bytes, got %" PRIuz, expect,
		           Stream_GetRemainingLength(s));
		return FALSE;
	}

	tile.blockType = blockType;
	tile.blockLen = blockLen;
	tile.flags = 0;

	Stream_Read_UINT8(s, tile.quantIdxY);
	Stream_Read_UINT8(s, tile.quantIdxCb);
	Stream_Read_UINT8(s, tile.quantIdxCr);
	Stream_Read_UINT16(s, tile.xIdx);
	Stream_Read_UINT16(s, tile.yIdx);
	Stream_Read_UINT8(s, tile.quality);
	Stream_Read_UINT16(s, tile.ySrlLen);
	Stream_Read_UINT16(s, tile.yRawLen);
	Stream_Read_UINT16(s, tile.cbSrlLen);
	Stream_Read_UINT16(s, tile.cbRawLen);
	Stream_Read_UINT16(s, tile.crSrlLen);
	Stream_Read_UINT16(s, tile.crRawLen);

	tile.ySrlData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.ySrlLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.ySrlLen);
		return FALSE;
	}

	tile.yRawData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.yRawLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.yRawLen);
		return FALSE;
	}

	tile.cbSrlData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.cbSrlLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes",
		           tile.cbSrlLen);
		return FALSE;
	}

	tile.cbRawData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.cbRawLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes",
		           tile.cbRawLen);
		return FALSE;
	}

	tile.crSrlData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.crSrlLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes",
		           tile.crSrlLen);
		return FALSE;
	}

	tile.crRawData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.crRawLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes",
		           tile.crRawLen);
		return FALSE;
	}

	return progressive_surface_tile_replace(surface, region, &tile, TRUE);
}

static INLINE BOOL progressive_tile_read(PROGRESSIVE_CONTEXT* progressive, BOOL simple, wStream* s,
                                         UINT16 blockType, UINT32 blockLen,
                                         PROGRESSIVE_SURFACE_CONTEXT* surface,
                                         PROGRESSIVE_BLOCK_REGION* region,
                                         const PROGRESSIVE_BLOCK_CONTEXT* context)
{
	RFX_PROGRESSIVE_TILE tile = { 0 };
	size_t expect = simple ? 16 : 17;

	if (Stream_GetRemainingLength(s) < expect)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "Expected %" PRIuz " bytes, got %" PRIuz, expect,
		           Stream_GetRemainingLength(s));
		return FALSE;
	}

	tile.blockType = blockType;
	tile.blockLen = blockLen;

	Stream_Read_UINT8(s, tile.quantIdxY);
	Stream_Read_UINT8(s, tile.quantIdxCb);
	Stream_Read_UINT8(s, tile.quantIdxCr);
	Stream_Read_UINT16(s, tile.xIdx);
	Stream_Read_UINT16(s, tile.yIdx);
	Stream_Read_UINT8(s, tile.flags);

	if (!simple)
		Stream_Read_UINT8(s, tile.quality);
	else
		tile.quality = 0xFF;
	Stream_Read_UINT16(s, tile.yLen);
	Stream_Read_UINT16(s, tile.cbLen);
	Stream_Read_UINT16(s, tile.crLen);
	Stream_Read_UINT16(s, tile.tailLen);

	tile.yData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.yLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.yLen);
		return FALSE;
	}

	tile.cbData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.cbLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.cbLen);
		return FALSE;
	}

	tile.crData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.crLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.crLen);
		return FALSE;
	}

	tile.tailData = Stream_Pointer(s);
	if (!Stream_SafeSeek(s, tile.tailLen))
	{
		WLog_Print(progressive->log, WLOG_ERROR, " Failed to seek %" PRIu32 " bytes", tile.tailLen);
		return FALSE;
	}

	return progressive_surface_tile_replace(surface, region, &tile, FALSE);
}

static INLINE int progressive_process_tiles(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                            PROGRESSIVE_BLOCK_REGION* region,
                                            PROGRESSIVE_SURFACE_CONTEXT* surface,
                                            const PROGRESSIVE_BLOCK_CONTEXT* context)
{
	int status = -1;
	size_t end;
	const size_t start = Stream_GetPosition(s);
	UINT16 index;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 count = 0;

	if (Stream_GetRemainingLength(s) < region->tileDataSize)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "Short block %" PRIuz ", expected %" PRIu32,
		           Stream_GetRemainingLength(s), region->tileDataSize);
		return -1;
	}

	while ((Stream_GetRemainingLength(s) >= 6) &&
	       (region->tileDataSize > (Stream_GetPosition(s) - start)))
	{
		size_t rem;
		const size_t pos = Stream_GetPosition(s);
		rem = Stream_GetRemainingLength(s);

		Stream_Read_UINT16(s, blockType);
		Stream_Read_UINT32(s, blockLen);

		WLog_Print(progressive->log, WLOG_DEBUG, "%s",
		           progressive_get_block_type_string(blockType));

		if (rem < blockLen)
		{
			WLog_Print(progressive->log, WLOG_ERROR, "Expected %" PRIu32 " remaining %" PRIuz,
			           blockLen, rem);
			return -1003;
		}

		rem = Stream_GetRemainingLength(s);
		switch (blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:
				if (!progressive_tile_read(progressive, TRUE, s, blockType, blockLen, surface,
				                           region, context))
					return -1022;
				break;

			case PROGRESSIVE_WBT_TILE_FIRST:
				if (!progressive_tile_read(progressive, FALSE, s, blockType, blockLen, surface,
				                           region, context))
					return -1027;
				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:
				if (!progressive_tile_read_upgrade(progressive, s, blockType, blockLen, surface,
				                                   region, context))
					return -1032;
				break;
			default:
				WLog_ERR(TAG, "Invalid block type %04 (%s)" PRIx16, blockType,
				         progressive_get_block_type_string(blockType));
				return -1039;
		}

		rem = Stream_GetPosition(s);
		if ((rem - pos) != blockLen)
		{
			WLog_Print(progressive->log, WLOG_ERROR,
			           "Actual block read %" PRIuz " but expected %" PRIu32, rem - pos, blockLen);
			return -1040;
		}
		count++;
	}

	end = Stream_GetPosition(s);
	if ((end - start) != region->tileDataSize)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "Actual total blocks read %" PRIuz " but expected %" PRIu32, end - start,
		           region->tileDataSize);
		return -1041;
	}

	if (count != region->numTiles)
	{
		WLog_Print(progressive->log, WLOG_WARN,
		           "numTiles inconsistency: actual: %" PRIu32 ", expected: %" PRIu16 "\n", count,
		           region->numTiles);
		return -1044;
	}

	for (index = 0; index < region->numTiles; index++)
	{
		RFX_PROGRESSIVE_TILE* tile = region->tiles[index];

		switch (tile->blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:
			case PROGRESSIVE_WBT_TILE_FIRST:
				status = progressive_decompress_tile_first(progressive, tile, region, context);
				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:
				status = progressive_decompress_tile_upgrade(progressive, tile, region, context);
				break;
			default:
				WLog_Print(progressive->log, WLOG_ERROR, "Invalid block type %04 (%s)" PRIx16,
				           tile->blockType, progressive_get_block_type_string(tile->blockType));
				return -42;
		}

		if (status < 0)
		{
			WLog_Print(progressive->log, WLOG_ERROR, "Failed to decompress %s at %" PRIu16,
			           progressive_get_block_type_string(tile->blockType), index);
			return -1;
		}
	}

	return (int)end - start;
}

static INLINE INT32 progressive_wb_sync(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                        UINT16 blockType, UINT32 blockLen)
{
	const UINT32 magic = 0xCACCACCA;
	const UINT16 version = 0x0100;
	PROGRESSIVE_BLOCK_SYNC sync;

	sync.blockType = blockType;
	sync.blockLen = blockLen;

	if (sync.blockLen != 12)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "PROGRESSIVE_BLOCK_SYNC::blockLen = 0x%08" PRIx32 " != 0x%08" PRIx32,
		           sync.blockLen, 12);
		return -1005;
	}

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveSync short %" PRIuz ", expected %" PRIuz,
		           Stream_GetRemainingLength(s), 6);
		return -1004;
	}

	WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveSync");

	Stream_Read_UINT32(s, sync.magic);
	Stream_Read_UINT16(s, sync.version);

	if (sync.magic != magic)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "PROGRESSIVE_BLOCK_SYNC::magic = 0x%08" PRIx32 " != 0x%08" PRIx32, sync.magic,
		           magic);
		return -1005;
	}

	if (sync.version != 0x0100)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "PROGRESSIVE_BLOCK_SYNC::version = 0x%04" PRIx16 " != 0x%04" PRIu16,
		           sync.version, version);
		return -1006;
	}

	if ((progressive->state & FLAG_WBT_SYNC) != 0)
		WLog_WARN(TAG, "Duplicate PROGRESSIVE_BLOCK_SYNC, ignoring");

	progressive->state |= FLAG_WBT_SYNC;
	return 1;
}

static INLINE INT32 progressive_wb_frame_begin(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                               UINT16 blockType, UINT32 blockLen)
{
	PROGRESSIVE_BLOCK_FRAME_BEGIN frameBegin;

	frameBegin.blockType = blockType;
	frameBegin.blockLen = blockLen;

	if (frameBegin.blockLen != 12)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           " RFX_PROGRESSIVE_FRAME_BEGIN::blockLen = 0x%08" PRIx32 " != 0x%08" PRIx32,
		           frameBegin.blockLen, 12);
		return -1005;
	}

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveFrameBegin short %" PRIuz ", expected %" PRIuz,
		           Stream_GetRemainingLength(s), 6);
		return -1007;
	}

	Stream_Read_UINT32(s, frameBegin.frameIndex);
	Stream_Read_UINT16(s, frameBegin.regionCount);

	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveFrameBegin: frameIndex: %" PRIu32 " regionCount: %" PRIu16 "",
	           frameBegin.frameIndex, frameBegin.regionCount);
	/**
	 * If the number of elements specified by the regionCount field is
	 * larger than the actual number of elements in the regions field,
	 * the decoder SHOULD ignore this inconsistency.
	 */

	if ((progressive->state & FLAG_WBT_FRAME_BEGIN) != 0)
	{
		WLog_ERR(TAG, "Duplicate RFX_PROGRESSIVE_FRAME_BEGIN in stream, this is not allowed!");
		return -1008;
	}

	if ((progressive->state & FLAG_WBT_FRAME_END) != 0)
	{
		WLog_ERR(TAG, "RFX_PROGRESSIVE_FRAME_BEGIN after RFX_PROGRESSIVE_FRAME_END in stream, this "
		              "is not allowed!");
		return -1008;
	}

	progressive->state |= FLAG_WBT_FRAME_BEGIN;
	return 1;
}

static INLINE INT32 progressive_wb_frame_end(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                             UINT16 blockType, UINT32 blockLen)
{
	PROGRESSIVE_BLOCK_FRAME_END frameEnd;

	frameEnd.blockType = blockType;
	frameEnd.blockLen = blockLen;

	if (frameEnd.blockLen != 6)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           " RFX_PROGRESSIVE_FRAME_END::blockLen = 0x%08" PRIx32 " != 0x%08" PRIx32,
		           frameEnd.blockLen, 6);
		return -1005;
	}

	if (Stream_GetRemainingLength(s) != 0)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveFrameEnd short %" PRIuz ", expected %" PRIuz,
		           Stream_GetRemainingLength(s), 0);
		return -1008;
	}

	WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveFrameEnd");
	if ((progressive->state & FLAG_WBT_FRAME_BEGIN) == 0)
		WLog_WARN(TAG, "RFX_PROGRESSIVE_FRAME_END before RFX_PROGRESSIVE_FRAME_BEGIN, ignoring");
	if ((progressive->state & FLAG_WBT_FRAME_END) != 0)
		WLog_WARN(TAG, "Duplicate RFX_PROGRESSIVE_FRAME_END, ignoring");

	progressive->state |= FLAG_WBT_FRAME_END;
	return 1;
}

static INLINE int progressive_wb_context(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                         UINT16 blockType, UINT32 blockLen)
{
	PROGRESSIVE_BLOCK_CONTEXT* context = &progressive->context;
	context->blockType = blockType;
	context->blockLen = blockLen;

	if (context->blockLen != 10)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "RFX_PROGRESSIVE_CONTEXT::blockLen = 0x%08" PRIx32 " != 0x%08" PRIx32,
		           context->blockLen, 10);
		return -1005;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveContext short %" PRIuz ", expected %" PRIuz,
		           Stream_GetRemainingLength(s), 4);
		return -1009;
	}

	Stream_Read_UINT8(s, context->ctxId);
	Stream_Read_UINT16(s, context->tileSize);
	Stream_Read_UINT8(s, context->flags);

	if (context->ctxId != 0x00)
		WLog_WARN(TAG, "RFX_PROGRESSIVE_CONTEXT::ctxId != 0x00: %" PRIu8, context->ctxId);

	if (context->tileSize != 64)
	{
		WLog_ERR(TAG, "RFX_PROGRESSIVE_CONTEXT::tileSize != 0x40: %" PRIu16, context->tileSize);
		return -1010;
	}

	if ((progressive->state & FLAG_WBT_FRAME_BEGIN) != 0)
		WLog_WARN(TAG, "RFX_PROGRESSIVE_CONTEXT received after RFX_PROGRESSIVE_FRAME_BEGIN");
	if ((progressive->state & FLAG_WBT_FRAME_END) != 0)
		WLog_WARN(TAG, "RFX_PROGRESSIVE_CONTEXT received after RFX_PROGRESSIVE_FRAME_END");
	if ((progressive->state & FLAG_WBT_CONTEXT) != 0)
		WLog_WARN(TAG, "Duplicate RFX_PROGRESSIVE_CONTEXT received, ignoring.");

	WLog_Print(progressive->log, WLOG_DEBUG, "ProgressiveContext: flags: 0x%02" PRIX8 "",
	           context->flags);

	progressive->state |= FLAG_WBT_CONTEXT;
	return 1;
}

static INLINE INT32 progressive_wb_read_region_header(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                                      UINT16 blockType, UINT32 blockLen,
                                                      PROGRESSIVE_BLOCK_REGION* region)
{
	size_t offset, len;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveRegion short %" PRIuz ", expected %" PRIuz,
		           Stream_GetRemainingLength(s), 12);
		return -1011;
	}

	region->blockType = blockType;
	region->blockLen = blockLen;
	Stream_Read_UINT8(s, region->tileSize);
	Stream_Read_UINT16(s, region->numRects);
	Stream_Read_UINT8(s, region->numQuant);
	Stream_Read_UINT8(s, region->numProgQuant);
	Stream_Read_UINT8(s, region->flags);
	Stream_Read_UINT16(s, region->numTiles);
	Stream_Read_UINT32(s, region->tileDataSize);

	if (region->tileSize != 64)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveRegion tile size %" PRIu8 ", expected %" PRIuz, region->tileSize,
		           64);
		return -1012;
	}

	if (region->numRects < 1)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "ProgressiveRegion missing rect count %" PRIu16,
		           region->numRects);
		return -1013;
	}

	if (region->numQuant > 7)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveRegion quant count too high %" PRIu8 ", expected < %" PRIuz,
		           region->numQuant, 7);
		return -1014;
	}

	len = Stream_GetRemainingLength(s);
	offset = (region->numRects * 8);
	if (len < offset)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "ProgressiveRegion data short for region->rects");
		return -1015;
	}

	offset += (region->numQuant * 5);
	if (len < offset)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "ProgressiveRegion data short for region->cQuant");
		return -1018;
	}

	offset += (region->numProgQuant * 16);
	if (len < offset)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "ProgressiveRegion data short for region->cProgQuant");
		return -1021;
	}

	offset += region->tileDataSize;
	if (len < offset)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "ProgressiveRegion data short for region->tiles");
		return -1024;
	}

	return 0;
}

static INLINE INT32 progressive_wb_skip_region(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                               UINT16 blockType, UINT32 blockLen)
{
	INT32 rc;
	size_t total;
	PROGRESSIVE_BLOCK_REGION region = { 0 };

	rc = progressive_wb_read_region_header(progressive, s, blockType, blockLen, &region);
	if (rc < 0)
		return rc;

	total = (region.numRects * 8);
	total += (region.numQuant * 5);
	total += (region.numProgQuant * 16);
	total += region.tileDataSize;
	if (!Stream_SafeSeek(s, total))
		return -1111;

	return 0;
}

static INLINE INT32 progressive_wb_region(PROGRESSIVE_CONTEXT* progressive, wStream* s,
                                          UINT16 blockType, UINT32 blockLen,
                                          PROGRESSIVE_SURFACE_CONTEXT* surface,
                                          PROGRESSIVE_BLOCK_REGION* region)
{
	INT32 rc;
	UINT16 index;
	UINT16 boxLeft;
	UINT16 boxTop;
	UINT16 boxRight;
	UINT16 boxBottom;
	UINT16 idxLeft;
	UINT16 idxTop;
	UINT16 idxRight;
	UINT16 idxBottom;
	const PROGRESSIVE_BLOCK_CONTEXT* context = &progressive->context;

	if ((progressive->state & FLAG_WBT_FRAME_BEGIN) == 0)
	{
		WLog_WARN(TAG, "RFX_PROGRESSIVE_REGION before RFX_PROGRESSIVE_FRAME_BEGIN, ignoring");
		return progressive_wb_skip_region(progressive, s, blockType, blockLen);
	}
	if ((progressive->state & FLAG_WBT_FRAME_END) != 0)
	{
		WLog_WARN(TAG, "RFX_PROGRESSIVE_REGION after RFX_PROGRESSIVE_FRAME_END, ignoring");
		return progressive_wb_skip_region(progressive, s, blockType, blockLen);
	}

	progressive->state |= FLAG_WBT_REGION;

	rc = progressive_wb_read_region_header(progressive, s, blockType, blockLen, region);
	if (rc < 0)
		return rc;

	for (index = 0; index < region->numRects; index++)
	{
		RFX_RECT* rect = &(region->rects[index]);
		Stream_Read_UINT16(s, rect->x);
		Stream_Read_UINT16(s, rect->y);
		Stream_Read_UINT16(s, rect->width);
		Stream_Read_UINT16(s, rect->height);
	}

	for (index = 0; index < region->numQuant; index++)
	{
		RFX_COMPONENT_CODEC_QUANT* quantVal = &(region->quantVals[index]);
		progressive_component_codec_quant_read(s, quantVal);

		if (!progressive_rfx_quant_lcmp_greater_equal(quantVal, 6))
		{
			WLog_Print(progressive->log, WLOG_ERROR,
			           "ProgressiveRegion region->cQuant[%" PRIu32 "] < 6", index);
			return -1;
		}

		if (!progressive_rfx_quant_lcmp_less_equal(quantVal, 15))
		{
			WLog_Print(progressive->log, WLOG_ERROR,
			           "ProgressiveRegion region->cQuant[%" PRIu32 "] > 15", index);
			return -1;
		}
	}

	for (index = 0; index < region->numProgQuant; index++)
	{
		RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal = &(region->quantProgVals[index]);

		Stream_Read_UINT8(s, quantProgVal->quality);

		progressive_component_codec_quant_read(s, &(quantProgVal->yQuantValues));
		progressive_component_codec_quant_read(s, &(quantProgVal->cbQuantValues));
		progressive_component_codec_quant_read(s, &(quantProgVal->crQuantValues));
	}

	WLog_Print(progressive->log, WLOG_DEBUG,
	           "ProgressiveRegion: numRects: %" PRIu16 " numTiles: %" PRIu16
	           " tileDataSize: %" PRIu32 " flags: 0x%02" PRIX8 " numQuant: %" PRIu8
	           " numProgQuant: %" PRIu8 "",
	           region->numRects, region->numTiles, region->tileDataSize, region->flags,
	           region->numQuant, region->numProgQuant);

	boxLeft = surface->gridWidth;
	boxTop = surface->gridHeight;
	boxRight = 0;
	boxBottom = 0;

	for (index = 0; index < region->numRects; index++)
	{
		RFX_RECT* rect = &(region->rects[index]);
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
		           "rect[%" PRIu16 "]: x: %" PRIu16 " y: %" PRIu16 " w: %" PRIu16 " h: %" PRIu16 "",
		           index, rect->x, rect->y, rect->width, rect->height);
	}

	return progressive_process_tiles(progressive, s, region, surface, context);
}

INT32 progressive_decompress(PROGRESSIVE_CONTEXT* progressive, const BYTE* pSrcData, UINT32 SrcSize,
                             BYTE* pDstData, UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
                             UINT32 nYDst, REGION16* invalidRegion, UINT16 surfaceId)
{
	INT32 rc = 1;
	UINT16 i, j;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 count = 0;
	wStream *s, ss;
	size_t start, end;
	REGION16 clippingRects, updateRegion;
	PROGRESSIVE_BLOCK_REGION* region;
	PROGRESSIVE_SURFACE_CONTEXT* surface = progressive_get_surface_data(progressive, surfaceId);
	union {
		const BYTE* cbp;
		BYTE* bp;
	} sconv;

	sconv.cbp = pSrcData;

	if (!surface)
	{
		WLog_Print(progressive->log, WLOG_ERROR, "ProgressiveRegion no surface for %" PRIu16,
		           surfaceId);
		return -1001;
	}

	region = calloc(1, sizeof(PROGRESSIVE_BLOCK_REGION));
	if (!region)
		return -1111;

	Stream_StaticInit(&ss, sconv.bp, SrcSize);
	s = &ss;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			progressive->format = DstFormat;
			break;
		default:
			progressive->format = PIXEL_FORMAT_XRGB32;
			break;
	}

	start = Stream_GetPosition(s);
	progressive->state = 0; /* Set state to not initialized */
	while (Stream_GetRemainingLength(s) >= 6)
	{
		size_t rem, st, e;

		st = Stream_GetPosition(s);
		rem = Stream_GetRemainingLength(s);

		Stream_Read_UINT16(s, blockType);
		Stream_Read_UINT32(s, blockLen);

		if (rem < blockLen)
		{
			WLog_Print(progressive->log, WLOG_ERROR, "Short block %" PRIuz ", expected %" PRIu32,
			           rem, blockLen);
			rc = -1003;
			goto fail;
		}

		rem = Stream_GetRemainingLength(s);
		switch (blockType)
		{
			case PROGRESSIVE_WBT_SYNC:
				rc = progressive_wb_sync(progressive, s, blockType, blockLen);
				break;

			case PROGRESSIVE_WBT_FRAME_BEGIN:
				rc = progressive_wb_frame_begin(progressive, s, blockType, blockLen);
				break;

			case PROGRESSIVE_WBT_FRAME_END:
				rc = progressive_wb_frame_end(progressive, s, blockType, blockLen);
				break;

			case PROGRESSIVE_WBT_CONTEXT:
				rc = progressive_wb_context(progressive, s, blockType, blockLen);
				break;

			case PROGRESSIVE_WBT_REGION:
				rc = progressive_wb_region(progressive, s, blockType, blockLen, surface, region);
				break;

			default:
				WLog_Print(progressive->log, WLOG_ERROR, "Invalid block type %04" PRIx16,
				           blockType);
				rc = -1039;
		}

		if (rc < 0)
			goto fail;

		e = Stream_GetPosition(s);
		if ((e - st) != blockLen)
		{
			WLog_Print(progressive->log, WLOG_ERROR,
			           "block len %" PRIuz " does not match read data %" PRIu32, e - st, blockLen);
			rc = -1040;
			goto fail;
		}
		count++;
	}

	end = Stream_GetPosition(s);
	if ((end - start) != SrcSize)
	{
		WLog_Print(progressive->log, WLOG_ERROR,
		           "total block len %" PRIuz " does not match read data %" PRIu32, end - start,
		           SrcSize);
		rc = -1041;
		goto fail;
	}

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

			if (!freerdp_image_copy(pDstData, DstFormat, nDstStep, rect->left, rect->top, width,
			                        height, tile->data, progressive->format, tile->stride, nXSrc,
			                        nYSrc, NULL, FREERDP_FLIP_NONE))
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

fail:
	free(region);
	return rc;
}

int progressive_compress(PROGRESSIVE_CONTEXT* progressive, const BYTE* pSrcData, UINT32 SrcSize,
                         BYTE** ppDstData, UINT32* pDstSize)
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
	progressive = (PROGRESSIVE_CONTEXT*)calloc(1, sizeof(PROGRESSIVE_CONTEXT));

	if (progressive)
	{
		progressive->Compressor = Compressor;
		progressive->bufferPool = BufferPool_New(TRUE, (8192 + 32) * 3, 16);

		ZeroMemory(&(progressive->quantProgValFull), sizeof(RFX_PROGRESSIVE_CODEC_QUANT));
		progressive->quantProgValFull.quality = 100;
		progressive->SurfaceContexts = HashTable_New(TRUE);
		progressive_context_reset(progressive);
		progressive->log = WLog_Get(TAG);
	}

	return progressive;
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

	if (progressive->SurfaceContexts)
	{
		count = HashTable_GetKeys(progressive->SurfaceContexts, &pKeys);

		for (index = 0; index < count; index++)
		{
			surface = (PROGRESSIVE_SURFACE_CONTEXT*)HashTable_GetItemValue(
			    progressive->SurfaceContexts, (void*)pKeys[index]);
			progressive_surface_context_free(surface);
		}

		free(pKeys);
		HashTable_Free(progressive->SurfaceContexts);
	}

	free(progressive);
}
