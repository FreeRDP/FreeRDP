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

#include "rfx_differential.h"
#include "rfx_quantization.h"

const char* progressive_get_block_type_string(UINT16 blockType)
{
	switch (blockType)
	{
		case PROGRESSIVE_WBT_SYNC:
			return "PROGRESSIVE_WBT_SYNC";
			break;

		case PROGRESSIVE_WBT_FRAME_BEGIN:
			return "PROGRESSIVE_WBT_FRAME_BEGIN";
			break;

		case PROGRESSIVE_WBT_FRAME_END:
			return "PROGRESSIVE_WBT_FRAME_END";
			break;

		case PROGRESSIVE_WBT_CONTEXT:
			return "PROGRESSIVE_WBT_CONTEXT";
			break;

		case PROGRESSIVE_WBT_REGION:
			return "PROGRESSIVE_WBT_REGION";
			break;

		case PROGRESSIVE_WBT_TILE_SIMPLE:
			return "PROGRESSIVE_WBT_TILE_SIMPLE";
			break;

		case PROGRESSIVE_WBT_TILE_FIRST:
			return "PROGRESSIVE_WBT_TILE_FIRST";
			break;

		case PROGRESSIVE_WBT_TILE_UPGRADE:
			return "PROGRESSIVE_WBT_TILE_UPGRADE";
			break;

		default:
			return "PROGRESSIVE_WBT_UNKNOWN";
			break;
	}

	return "PROGRESSIVE_WBT_UNKNOWN";
}

int progressive_set_surface_data(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId, void* pData)
{
	ULONG_PTR key;

	key = ((ULONG_PTR) surfaceId) + 1;

	if (pData)
		HashTable_Add(progressive->SurfaceContexts, (void*) key, pData);
	else
		HashTable_Remove(progressive->SurfaceContexts, (void*) key);

	return 1;
}

void* progressive_get_surface_data(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId)
{
	ULONG_PTR key;
	void* pData = NULL;

	key = ((ULONG_PTR) surfaceId) + 1;

	pData = HashTable_GetItemValue(progressive->SurfaceContexts, (void*) key);

	return pData;
}

int progressive_create_surface_context(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId, UINT32 width, UINT32 height)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;

	surface = (PROGRESSIVE_SURFACE_CONTEXT*) progressive_get_surface_data(progressive, surfaceId);

	if (!surface)
	{
		surface = (PROGRESSIVE_SURFACE_CONTEXT*) malloc(sizeof(PROGRESSIVE_SURFACE_CONTEXT));

		if (!surface)
			return -1;

		surface->id = surfaceId;
		surface->width = width;
		surface->height = height;
		surface->gridWidth = (width + (width % 64)) / 64;
		surface->gridHeight = (height + (height % 64)) / 64;
		surface->gridSize = surface->gridWidth * surface->gridHeight;

		surface->tiles = (RFX_PROGRESSIVE_TILE*) calloc(surface->gridSize, sizeof(RFX_PROGRESSIVE_TILE));

		if (!surface->tiles)
			return -1;

		progressive_set_surface_data(progressive, surfaceId, (void*) surface);
	}

	return 1;
}

int progressive_delete_surface_context(PROGRESSIVE_CONTEXT* progressive, UINT16 surfaceId)
{
	PROGRESSIVE_SURFACE_CONTEXT* surface;

	surface = (PROGRESSIVE_SURFACE_CONTEXT*) progressive_get_surface_data(progressive, surfaceId);

	if (surface)
	{
		progressive_set_surface_data(progressive, surfaceId, NULL);

		free(surface->tiles);
		free(surface);
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

static void progressive_rfx_idwt_x(INT16* pLowBand, int nLowStep, INT16* pHighBand, int nHighStep,
		INT16* pDstBand, int nDstStep, int nLowCount, int nHighCount, int nDstCount)
{
	int i, j;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;
	INT16 *pL, *pH, *pX;

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

static void progressive_rfx_idwt_y(INT16* pLowBand, int nLowStep, INT16* pHighBand, int nHighStep,
		INT16* pDstBand, int nDstStep, int nLowCount, int nHighCount, int nDstCount)
{
	int i, j;
	INT16 L0;
	INT16 H0, H1;
	INT16 X0, X1, X2;
	INT16 *pL, *pH, *pX;

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
				pX += nDstStep;
			}
			else
			{
				L0 = *pL;
				pL += nLowStep;

				X0 = L0 - H0;

				*pX = X2;
				pX += nDstStep;

				*pX = ((X0 + X2) / 2) + (2 * H0);
				pX += nDstStep;

				*pX = X0;
				pX += nDstStep;
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
			pL += nLowStep;

			*pX = (X0 + L0) / 2;
			pX += nDstStep;
		}

		pLowBand++;
		pHighBand++;
		pDstBand++;
	}
}

static int progressive_rfx_get_band_l_count(int level)
{
	return (64 >> level) + 1;
}

static int progressive_rfx_get_band_h_count(int level)
{
	if (level == 1)
		return (64 >> 1) - 1;
	else
		return (64 + (1 << (level - 1))) >> level;
}

static void progressive_rfx_dwt_2d_decode_block(INT16* buffer, INT16* temp, int level)
{
	int offset;
	int nBandL;
	int nBandH;
	int nDstStepX;
	int nDstStepY;
	INT16 *HL, *LH;
	INT16 *HH, *LL;
	INT16 *L, *H, *LLx;
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
	offset += (nBandL * nBandL);

	nDstStepX = (nBandL + nBandH);
	nDstStepY = (nBandL + nBandH);

	offset = 0;

	L = &temp[offset];
	offset += (nBandL * nDstStepX);

	H = &temp[offset];
	offset += (nBandH * nDstStepX);

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

	progressive_rfx_idwt_x(pLowBand[0], nLowStep[0], pHighBand[0], nHighStep[0], pDstBand[0], nDstStep[0], nLowCount[0], nHighCount[0], nDstCount[0]);

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

	progressive_rfx_idwt_x(pLowBand[1], nLowStep[1], pHighBand[1], nHighStep[1], pDstBand[1], nDstStep[1], nLowCount[1], nHighCount[1], nDstCount[1]);

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

	progressive_rfx_idwt_y(pLowBand[2], nLowStep[2], pHighBand[2], nHighStep[2], pDstBand[2], nDstStep[2], nLowCount[2], nHighCount[2], nDstCount[2]);
}

void progressive_rfx_dwt_2d_decode(INT16* buffer, INT16* temp, INT16* current, BOOL diff)
{
	const primitives_t* prims = primitives_get();

	if (diff)
		prims->add_16s(buffer, current, buffer, 4096);

	CopyMemory(current, buffer, 4096 * 2);

	progressive_rfx_dwt_2d_decode_block(&buffer[3807], temp, 3);
	progressive_rfx_dwt_2d_decode_block(&buffer[3007], temp, 2);
	progressive_rfx_dwt_2d_decode_block(&buffer[0], temp, 1);
}

int progressive_rfx_decode_component(PROGRESSIVE_CONTEXT* progressive, RFX_COMPONENT_CODEC_QUANT* bitPos,
		const BYTE* data, int length, INT16* buffer, INT16* current, BOOL diff)
{
	int status;
	INT16* temp;
	const primitives_t* prims = primitives_get();

	status = rfx_rlgr_decode(data, length, buffer, 4096, 1);

	if (status < 0)
		return status;

	rfx_differential_decode(&buffer[4015], 81); /* LL3 */

	rfx_quantization_decode_block(prims, &buffer[0], 1023, bitPos->HL1 - 1); /* HL1 */
	rfx_quantization_decode_block(prims, &buffer[1023], 1023, bitPos->LH1 - 1); /* LH1 */
	rfx_quantization_decode_block(prims, &buffer[2046], 961, bitPos->HH1 - 1); /* HH1 */
	rfx_quantization_decode_block(prims, &buffer[3007], 272, bitPos->HL2 - 1); /* HL2 */
	rfx_quantization_decode_block(prims, &buffer[3279], 272, bitPos->LH2 - 1); /* LH2 */
	rfx_quantization_decode_block(prims, &buffer[3551], 256, bitPos->HH2 - 1); /* HH2 */
	rfx_quantization_decode_block(prims, &buffer[3807], 72, bitPos->HL3 - 1); /* HL3 */
	rfx_quantization_decode_block(prims, &buffer[3879], 72, bitPos->LH3 - 1); /* LH3 */
	rfx_quantization_decode_block(prims, &buffer[3951], 64, bitPos->HH3 - 1); /* HH3 */
	rfx_quantization_decode_block(prims, &buffer[4015], 81, bitPos->LL3 - 1); /* LL3 */

	temp = (INT16*) BufferPool_Take(progressive->bufferPool, -1); /* DWT buffer */

	progressive_rfx_dwt_2d_decode(buffer, temp, current, diff);

	BufferPool_Return(progressive->bufferPool, temp);

	return 1;
}

struct _RFX_PROGRESSIVE_UPGRADE_STATE
{
	BOOL nonLL;
	wBitStream* srl;
	wBitStream* raw;

	/* SRL state */

	int k;
	int kp;
	int nz;
	BOOL mode;
};
typedef struct _RFX_PROGRESSIVE_UPGRADE_STATE RFX_PROGRESSIVE_UPGRADE_STATE;

int progressive_rfx_upgrade_block(RFX_PROGRESSIVE_UPGRADE_STATE* state, INT16* buffer, int length, UINT32 bitPos, UINT32 numBits)
{
	int index;
	INT16 input;
	wBitStream* srl;
	wBitStream* raw;

	srl = state->srl;
	raw = state->raw;

	if (!state->nonLL)
	{
		for (index = 0; index < length; index++)
		{
			raw->mask = ((1 << numBits) - 1);
			input = (INT16) ((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);

			buffer[index] += (input << bitPos);
		}

		return 1;
	}

	for (index = 0; index < length; index++)
	{
		if (buffer[index] > 0)
		{
			/* sign > 0, read from raw */

			raw->mask = ((1 << numBits) - 1);
			input = (INT16) ((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);

			buffer[index] += (input << bitPos);
		}
		else if (buffer[index] > 0)
		{
			/* sign < 0, read from raw */

			raw->mask = ((1 << numBits) - 1);
			input = (INT16) ((raw->accumulator >> (32 - numBits)) & raw->mask);
			BitStream_Shift(raw, numBits);

			buffer[index] -= (input << bitPos);
		}
		else
		{
			/* sign == 0, read from srl */
		}
	}

	return 1;
}

int progressive_rfx_upgrade_component(PROGRESSIVE_CONTEXT* progressive, RFX_COMPONENT_CODEC_QUANT* bitPos,
		RFX_COMPONENT_CODEC_QUANT* numBits, INT16* buffer, const BYTE* srlData, int srlLen, const BYTE* rawData, int rawLen)
{
	wBitStream s_srl;
	wBitStream s_raw;
	RFX_PROGRESSIVE_UPGRADE_STATE state;

	state.srl = &s_srl;
	state.raw = &s_raw;

	BitStream_Attach(state.srl, srlData, srlLen);
	BitStream_Fetch(state.srl);

	BitStream_Attach(state.raw, rawData, rawLen);
	BitStream_Fetch(state.raw);

	state.nonLL = TRUE;
	progressive_rfx_upgrade_block(&state, &buffer[0], 1023, bitPos->HL1, numBits->HL1); /* HL1 */
	progressive_rfx_upgrade_block(&state, &buffer[1023], 1023, bitPos->LH1, numBits->LH1); /* LH1 */
	progressive_rfx_upgrade_block(&state, &buffer[2046], 961, bitPos->HH1, numBits->HH1); /* HH1 */
	progressive_rfx_upgrade_block(&state, &buffer[3007], 272, bitPos->HL2, numBits->HL2); /* HL2 */
	progressive_rfx_upgrade_block(&state, &buffer[3279], 272, bitPos->LH2, numBits->LH2); /* LH2 */
	progressive_rfx_upgrade_block(&state, &buffer[3551], 256, bitPos->HH2, numBits->HH2); /* HH2 */
	progressive_rfx_upgrade_block(&state, &buffer[3807], 72, bitPos->HL3, numBits->HL3); /* HL3 */
	progressive_rfx_upgrade_block(&state, &buffer[3879], 72, bitPos->LH3, numBits->LH3); /* LH3 */
	progressive_rfx_upgrade_block(&state, &buffer[3951], 64, bitPos->HH3, numBits->HH3); /* HH3 */

	state.nonLL = FALSE;
	progressive_rfx_upgrade_block(&state, &buffer[4015], 81, bitPos->LL3, numBits->LL3); /* LL3 */

	return 1;
}

void progressive_rfx_quant_add(RFX_COMPONENT_CODEC_QUANT* q1, RFX_COMPONENT_CODEC_QUANT* q2, RFX_COMPONENT_CODEC_QUANT* dst)
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

void progressive_rfx_quant_sub(RFX_COMPONENT_CODEC_QUANT* q1, RFX_COMPONENT_CODEC_QUANT* q2, RFX_COMPONENT_CODEC_QUANT* dst)
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

int progressive_decompress_tile_first(PROGRESSIVE_CONTEXT* progressive, RFX_PROGRESSIVE_TILE* tile)
{
	BOOL diff;
	BYTE* pBuffer;
	INT16* pSrcDst[3];
	INT16* pCurrent[3];
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_COMPONENT_CODEC_QUANT* quantProgY;
	RFX_COMPONENT_CODEC_QUANT* quantProgCb;
	RFX_COMPONENT_CODEC_QUANT* quantProgCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;
	static const prim_size_t roi_64x64 = { 64, 64 };
	const primitives_t* prims = primitives_get();

	printf("ProgressiveTileFirst: quantIdx Y: %d Cb: %d Cr: %d xIdx: %d yIdx: %d flags: 0x%02X quality: %d yLen: %d cbLen: %d crLen: %d tailLen: %d\n",
			tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx, tile->yIdx, tile->flags, tile->quality, tile->yLen, tile->cbLen, tile->crLen, tile->tailLen);

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

	diff = tile->flags & RFX_TILE_DIFFERENCE;

	quantProgY = &(quantProgVal->yQuantValues);
	quantProgCb = &(quantProgVal->cbQuantValues);
	quantProgCr = &(quantProgVal->crQuantValues);

	progressive_rfx_quant_add(quantY, quantProgY, &(tile->yBitPos));
	progressive_rfx_quant_add(quantCb, quantProgCb, &(tile->cbBitPos));
	progressive_rfx_quant_add(quantCr, quantProgCr, &(tile->crBitPos));

	if (!tile->data)
	{
		tile->data = (BYTE*) _aligned_malloc(64 * 64 * 4, 16);
	}

	if (!tile->current)
	{
		tile->current = (BYTE*) _aligned_malloc((8192 + 32) * 3, 16);
	}

	pBuffer = tile->current;

	pCurrent[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	pBuffer = (BYTE*) BufferPool_Take(progressive->bufferPool, -1);

	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	progressive_rfx_decode_component(progressive, &(tile->yBitPos), tile->yData, tile->yLen, pSrcDst[0], pCurrent[0], diff); /* Y */
	progressive_rfx_decode_component(progressive, &(tile->crBitPos), tile->cbData, tile->cbLen, pSrcDst[1], pCurrent[1], diff); /* Cb */
	progressive_rfx_decode_component(progressive, &(tile->cbBitPos), tile->crData, tile->crLen, pSrcDst[2], pCurrent[2], diff); /* Cr */

	prims->yCbCrToRGB_16s16s_P3P3((const INT16**) pSrcDst, 64 * sizeof(INT16),
			pSrcDst, 64 * sizeof(INT16), &roi_64x64);

	prims->RGBToRGB_16s8u_P3AC4R((const INT16**) pSrcDst, 64 * sizeof(INT16),
			tile->data, 64 * 4, &roi_64x64);

	BufferPool_Return(progressive->bufferPool, pBuffer);

	WLog_Image(progressive->log, WLOG_TRACE, tile->data, 64, 64, 32);

	return 1;
}

int progressive_decompress_tile_upgrade(PROGRESSIVE_CONTEXT* progressive, RFX_PROGRESSIVE_TILE* tile)
{
	BYTE* pBuffer;
	INT16* pCurrent[3];
	PROGRESSIVE_BLOCK_REGION* region;
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

	printf("ProgressiveTileUpgrade: quantIdx Y: %d Cb: %d Cr: %d xIdx: %d yIdx: %d quality: %d ySrlLen: %d yRawLen: %d cbSrlLen: %d cbRawLen: %d crSrlLen: %d crRawLen: %d\n",
			tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx, tile->yIdx, tile->quality, tile->ySrlLen, tile->yRawLen, tile->cbSrlLen, tile->cbRawLen, tile->crSrlLen, tile->crRawLen);

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

	progressive_rfx_quant_add(quantY, quantProgY, &yBitPos);
	progressive_rfx_quant_add(quantCb, quantProgCb, &cbBitPos);
	progressive_rfx_quant_add(quantCr, quantProgCr, &crBitPos);

	progressive_rfx_quant_sub(&(tile->yBitPos), &yBitPos, &yNumBits);
	progressive_rfx_quant_sub(&(tile->cbBitPos), &cbBitPos, &cbNumBits);
	progressive_rfx_quant_sub(&(tile->crBitPos), &crBitPos, &crNumBits);

	CopyMemory(&(tile->yBitPos), &yBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->cbBitPos), &cbBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));
	CopyMemory(&(tile->crBitPos), &crBitPos, sizeof(RFX_COMPONENT_CODEC_QUANT));

	pBuffer = tile->current;
	pCurrent[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pCurrent[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pCurrent[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	progressive_rfx_upgrade_component(progressive, &yBitPos, &yNumBits, pCurrent[0],
			tile->ySrlData, tile->ySrlLen, tile->yRawData, tile->yRawLen); /* Y */
	progressive_rfx_upgrade_component(progressive, &cbBitPos, &cbNumBits, pCurrent[1],
			tile->cbSrlData, tile->cbSrlLen, tile->cbRawData, tile->cbRawLen); /* Cb */
	progressive_rfx_upgrade_component(progressive, &crBitPos, &crNumBits, pCurrent[2],
			tile->crSrlData, tile->crSrlLen, tile->crRawData, tile->crRawLen); /* Cr */

	return 1;
}

int progressive_process_tiles(PROGRESSIVE_CONTEXT* progressive, BYTE* blocks, UINT32 blocksLen, PROGRESSIVE_SURFACE_CONTEXT* surface)
{
	BYTE* block;
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

		//printf("%s\n", progressive_get_block_type_string(blockType));

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

	for (index = 0; index < region->numTiles; index++)
	{
		tile = tiles[index];

		switch (tile->blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:
			case PROGRESSIVE_WBT_TILE_FIRST:
				progressive_decompress_tile_first(progressive, tile);
				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:
				progressive_decompress_tile_upgrade(progressive, tile);
				break;
		}
	}

	return (int) offset;
}

void progressive_component_codec_quant_read(BYTE* block, RFX_COMPONENT_CODEC_QUANT* quantVal)
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

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, UINT16 surfaceId)
{
	int status;
	BYTE* block;
	BYTE* blocks;
	UINT16 index;
	UINT32 boffset;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 blocksLen;
	UINT32 count = 0;
	UINT32 offset = 0;
	RFX_RECT* rect = NULL;
	PROGRESSIVE_BLOCK_SYNC sync;
	PROGRESSIVE_BLOCK_REGION* region;
	PROGRESSIVE_BLOCK_CONTEXT context;
	PROGRESSIVE_BLOCK_FRAME_BEGIN frameBegin;
	PROGRESSIVE_BLOCK_FRAME_END frameEnd;
	PROGRESSIVE_SURFACE_CONTEXT* surface;
	RFX_COMPONENT_CODEC_QUANT* quantVal;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

	surface = (PROGRESSIVE_SURFACE_CONTEXT*) progressive_get_surface_data(progressive, surfaceId);

	if (!surface)
		return -1001;

	blocks = pSrcData;
	blocksLen = SrcSize;

	region = &(progressive->region);

	while ((blocksLen - offset) >= 6)
	{
		boffset = 0;
		block = &blocks[offset];

		blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		printf("%s\n", progressive_get_block_type_string(blockType));

		if ((blocksLen - offset) < blockLen)
			return -1003;

		switch (blockType)
		{
			case PROGRESSIVE_WBT_SYNC:

				sync.blockType = blockType;
				sync.blockLen = blockLen;

				if ((blockLen - boffset) != 6)
					return -1004;

				sync.magic = (UINT32) *((UINT32*) &block[boffset + 0]); /* magic (4 bytes) */
				sync.version = (UINT32) *((UINT16*) &block[boffset + 4]); /* version (2 bytes) */
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

				frameBegin.frameIndex = (UINT32) *((UINT32*) &block[boffset + 0]); /* frameIndex (4 bytes) */
				frameBegin.regionCount = (UINT32) *((UINT16*) &block[boffset + 4]); /* regionCount (2 bytes) */
				boffset += 6;

				/**
				 * If the number of elements specified by the regionCount field is
				 * larger than the actual number of elements in the regions field,
				 * the decoder SHOULD ignore this inconsistency.
				 */

				break;

			case PROGRESSIVE_WBT_FRAME_END:

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
				region->tileDataSize = *((UINT32*) &block[boffset + 8]); /* tileDataSize (4 bytes) */
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
					progressive->rects = (RFX_RECT*) realloc(progressive->rects, region->numRects * sizeof(RFX_RECT));
					progressive->cRects = region->numRects;
				}

				region->rects = progressive->rects;

				if (!region->rects)
					return -1016;

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
					return -1017;

				if (region->numQuant > progressive->cQuant)
				{
					progressive->quantVals = (RFX_COMPONENT_CODEC_QUANT*) realloc(progressive->quantVals,
							region->numQuant * sizeof(RFX_COMPONENT_CODEC_QUANT));
					progressive->cQuant = region->numQuant;
				}

				region->quantVals = progressive->quantVals;

				if (!region->quantVals)
					return -1018;

				for (index = 0; index < region->numQuant; index++)
				{
					quantVal = &(region->quantVals[index]);
					progressive_component_codec_quant_read(&block[boffset], quantVal);
					boffset += 5;
				}

				if ((blockLen - boffset) < (region->numProgQuant * 16))
					return -1019;

				if (region->numProgQuant > progressive->cProgQuant)
				{
					progressive->quantProgVals = (RFX_PROGRESSIVE_CODEC_QUANT*) realloc(progressive->quantProgVals,
							region->numProgQuant * sizeof(RFX_PROGRESSIVE_CODEC_QUANT));
					progressive->cProgQuant = region->numProgQuant;
				}

				region->quantProgVals = progressive->quantProgVals;

				if (!region->quantProgVals)
					return -1020;

				for (index = 0; index < region->numProgQuant; index++)
				{
					quantProgVal = &(region->quantProgVals[index]);
					quantProgVal->quality = block[boffset + 0];
					progressive_component_codec_quant_read(&block[boffset + 1], &(quantProgVal->yQuantValues));
					progressive_component_codec_quant_read(&block[boffset + 6], &(quantProgVal->cbQuantValues));
					progressive_component_codec_quant_read(&block[boffset + 11], &(quantProgVal->crQuantValues));
					boffset += 16;
				}

				if ((blockLen - boffset) < region->tileDataSize)
					return -1021;

				if (region->numTiles > progressive->cTiles)
				{
					progressive->tiles = (RFX_PROGRESSIVE_TILE**) realloc(progressive->tiles,
							region->numTiles * sizeof(RFX_PROGRESSIVE_TILE*));
					progressive->cTiles = region->numTiles;
				}

				region->tiles = progressive->tiles;

				if (!region->tiles)
					return -1;

				printf("numRects: %d numTiles: %d numQuant: %d numProgQuant: %d\n",
						region->numRects, region->numTiles, region->numQuant, region->numProgQuant);

				status = progressive_process_tiles(progressive, &block[boffset], region->tileDataSize, surface);

				if (status < 0)
					return status;

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

	return 1;
}

int progressive_compress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

void progressive_context_reset(PROGRESSIVE_CONTEXT* progressive)
{

}

PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor)
{
	PROGRESSIVE_CONTEXT* progressive;

	progressive = (PROGRESSIVE_CONTEXT*) calloc(1, sizeof(PROGRESSIVE_CONTEXT));

	if (progressive)
	{
		progressive->Compressor = Compressor;

		progressive->log = WLog_Get("com.freerdp.codec.progressive");

		progressive->bufferPool = BufferPool_New(TRUE, (8192 + 32) * 3, 16);

		progressive->cRects = 64;
		progressive->rects = (RFX_RECT*) malloc(progressive->cRects * sizeof(RFX_RECT));

		if (!progressive->rects)
			return NULL;

		progressive->cTiles = 64;
		progressive->tiles = (RFX_PROGRESSIVE_TILE**) malloc(progressive->cTiles * sizeof(RFX_PROGRESSIVE_TILE*));

		if (!progressive->tiles)
			return NULL;

		progressive->cQuant = 8;
		progressive->quantVals = (RFX_COMPONENT_CODEC_QUANT*) malloc(progressive->cQuant * sizeof(RFX_COMPONENT_CODEC_QUANT));

		if (!progressive->quantVals)
			return NULL;

		progressive->cProgQuant = 8;
		progressive->quantProgVals = (RFX_PROGRESSIVE_CODEC_QUANT*) malloc(progressive->cProgQuant * sizeof(RFX_PROGRESSIVE_CODEC_QUANT));

		if (!progressive->quantProgVals)
			return NULL;

		ZeroMemory(&(progressive->quantProgValFull), sizeof(RFX_PROGRESSIVE_CODEC_QUANT));
		progressive->quantProgValFull.quality = 100;

		progressive->SurfaceContexts = HashTable_New(TRUE);

		progressive_context_reset(progressive);
	}

	return progressive;
}

void progressive_context_free(PROGRESSIVE_CONTEXT* progressive)
{
	if (!progressive)
		return;

	BufferPool_Free(progressive->bufferPool);

	free(progressive->rects);
	free(progressive->tiles);
	free(progressive->quantVals);
	free(progressive->quantProgVals);

	HashTable_Free(progressive->SurfaceContexts);

	free(progressive);
}

