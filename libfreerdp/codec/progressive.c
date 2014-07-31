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

#include <freerdp/codec/color.h>
#include <freerdp/codec/progressive.h>

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

int progressive_decompress_tile_simple(PROGRESSIVE_CONTEXT* progressive, PROGRESSIVE_BLOCK_TILE_SIMPLE* tile)
{
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

	printf("ProgressiveTileSimple: quantIdx Y: %d Cb: %d Cr: %d xIdx: %d yIdx: %d flags: %d yLen: %d cbLen: %d crLen: %d tailLen: %d\n",
			tile->quantIdxY, tile->quantIdxCb, tile->quantIdxCr, tile->xIdx, tile->yIdx, tile->flags, tile->yLen, tile->cbLen, tile->crLen, tile->tailLen);

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

	quantProgVal = &(progressive->quantProgValFull);

	return 1;
}

int progressive_decompress_tile_first(PROGRESSIVE_CONTEXT* progressive, PROGRESSIVE_BLOCK_TILE_FIRST* tile)
{
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

	printf("ProgressiveTileFirst: quantIdx Y: %d Cb: %d Cr: %d xIdx: %d yIdx: %d flags: %d quality: %d yLen: %d cbLen: %d crLen: %d tailLen: %d\n",
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

	return 1;
}

int progressive_decompress_tile_upgrade(PROGRESSIVE_CONTEXT* progressive, PROGRESSIVE_BLOCK_TILE_UPGRADE* tile)
{
	PROGRESSIVE_BLOCK_REGION* region;
	RFX_COMPONENT_CODEC_QUANT* quantY;
	RFX_COMPONENT_CODEC_QUANT* quantCb;
	RFX_COMPONENT_CODEC_QUANT* quantCr;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

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
		quantProgVal = &(progressive->quantProgValFull);
	}
	else
	{
		if (tile->quality >= region->numProgQuant)
			return -1;

		quantProgVal = &(region->quantProgVals[tile->quality]);
	}

	return 1;
}

int progressive_process_blocks(PROGRESSIVE_CONTEXT* progressive, BYTE* blocks, UINT32 blocksLen, UINT32 blockCount, UINT32 flags)
{
	int status;
	BYTE* block;
	UINT16 index;
	UINT32 boffset;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 count = 0;
	UINT32 offset = 0;
	RFX_RECT* rect = NULL;
	PROGRESSIVE_BLOCK_SYNC sync;
	PROGRESSIVE_BLOCK_REGION* region;
	PROGRESSIVE_BLOCK_CONTEXT context;
	PROGRESSIVE_BLOCK_FRAME_BEGIN frameBegin;
	PROGRESSIVE_BLOCK_FRAME_END frameEnd;
	PROGRESSIVE_BLOCK_TILE_SIMPLE simple;
	PROGRESSIVE_BLOCK_TILE_FIRST first;
	PROGRESSIVE_BLOCK_TILE_UPGRADE upgrade;
	RFX_COMPONENT_CODEC_QUANT* quantVal;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

	region = &(progressive->region);

	while ((blocksLen - offset) >= 6)
	{
		boffset = 0;
		block = &blocks[offset];

		blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		if (flags & PROGRESSIVE_BLOCKS_REGION)
		{
			if ((count + 1) > blockCount)
				break;

			if (blockType != PROGRESSIVE_WBT_REGION)
				return -1001;
		}
		else if (flags & PROGRESSIVE_BLOCKS_TILE)
		{
			if ((count + 1) > blockCount)
				break;

			if ((blockType != PROGRESSIVE_WBT_TILE_SIMPLE) &&
				(blockType != PROGRESSIVE_WBT_TILE_FIRST) &&
				(blockType != PROGRESSIVE_WBT_TILE_UPGRADE))
			{
				return -1002;
			}
		}

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

				if ((blockLen - boffset) > 0)
				{
					status = progressive_process_blocks(progressive, &block[boffset],
							blockLen - boffset, frameBegin.regionCount, PROGRESSIVE_BLOCKS_REGION);

					if (status < 0)
						return status;

					boffset += status;
				}

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
					quantVal->LL3 = block[boffset + 0] & 0x0F;
					quantVal->HL3 = block[boffset + 0] >> 4;
					quantVal->LH3 = block[boffset + 1] & 0x0F;
					quantVal->HH3 = block[boffset + 1] >> 4;
					quantVal->HL2 = block[boffset + 2] & 0x0F;
					quantVal->LH2 = block[boffset + 2] >> 4;
					quantVal->HH2 = block[boffset + 3] & 0x0F;
					quantVal->HL1 = block[boffset + 3] >> 4;
					quantVal->LH1 = block[boffset + 4] & 0x0F;
					quantVal->HH1 = block[boffset + 4] >> 4;
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
					CopyMemory(quantProgVal->yQuantValues, &block[boffset + 1], 5);
					CopyMemory(quantProgVal->cbQuantValues, &block[boffset + 6], 5);
					CopyMemory(quantProgVal->crQuantValues, &block[boffset + 11], 5);
					boffset += 16;
				}

				if ((blockLen - boffset) < region->tileDataSize)
					return -1021;

				printf("numRects: %d numTiles: %d numQuant: %d numProgQuant: %d\n",
						region->numRects, region->numTiles, region->numQuant, region->numProgQuant);

				status = progressive_process_blocks(progressive, &block[boffset],
						region->tileDataSize, region->numTiles, PROGRESSIVE_BLOCKS_TILE);

				if (status < 0)
					return status;

				boffset += (UINT32) status;

				break;

			case PROGRESSIVE_WBT_TILE_SIMPLE:

				simple.blockType = blockType;
				simple.blockLen = blockLen;

				if ((blockLen - boffset) < 16)
					return -1022;

				simple.quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				simple.quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				simple.quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				simple.xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				simple.yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				simple.flags = block[boffset + 7]; /* flags (1 byte) */
				simple.yLen = *((UINT16*) &block[boffset + 8]); /* yLen (2 bytes) */
				simple.cbLen = *((UINT16*) &block[boffset + 10]); /* cbLen (2 bytes) */
				simple.crLen = *((UINT16*) &block[boffset + 12]); /* crLen (2 bytes) */
				simple.tailLen = *((UINT16*) &block[boffset + 14]); /* tailLen (2 bytes) */
				boffset += 16;

				if ((blockLen - boffset) < simple.yLen)
					return -1023;

				simple.yData = &block[boffset];
				boffset += simple.yLen;

				if ((blockLen - boffset) < simple.cbLen)
					return -1024;

				simple.cbData = &block[boffset];
				boffset += simple.cbLen;

				if ((blockLen - boffset) < simple.crLen)
					return -1025;

				simple.crData = &block[boffset];
				boffset += simple.crLen;

				if ((blockLen - boffset) < simple.tailLen)
					return -1026;

				simple.tailData = &block[boffset];
				boffset += simple.tailLen;

				status = progressive_decompress_tile_simple(progressive, &simple);

				break;

			case PROGRESSIVE_WBT_TILE_FIRST:

				first.blockType = blockType;
				first.blockLen = blockLen;

				if ((blockLen - boffset) < 17)
					return -1027;

				first.quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				first.quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				first.quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				first.xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				first.yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				first.flags = block[boffset + 7]; /* flags (1 byte) */
				first.quality = block[boffset + 8]; /* quality (1 byte) */
				first.yLen = *((UINT16*) &block[boffset + 9]); /* yLen (2 bytes) */
				first.cbLen = *((UINT16*) &block[boffset + 11]); /* cbLen (2 bytes) */
				first.crLen = *((UINT16*) &block[boffset + 13]); /* crLen (2 bytes) */
				first.tailLen = *((UINT16*) &block[boffset + 15]); /* tailLen (2 bytes) */
				boffset += 17;

				if ((blockLen - boffset) < first.yLen)
					return -1028;

				first.yData = &block[boffset];
				boffset += first.yLen;

				if ((blockLen - boffset) < first.cbLen)
					return -1029;

				first.cbData = &block[boffset];
				boffset += first.cbLen;

				if ((blockLen - boffset) < first.crLen)
					return -1030;

				first.crData = &block[boffset];
				boffset += first.crLen;

				if ((blockLen - boffset) < first.tailLen)
					return -1031;

				first.tailData = &block[boffset];
				boffset += first.tailLen;

				status = progressive_decompress_tile_first(progressive, &first);

				if (status < 0)
					return -1;

				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:

				upgrade.blockType = blockType;
				upgrade.blockLen = blockLen;

				if ((blockLen - boffset) < 20)
					return -1032;

				upgrade.quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				upgrade.quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				upgrade.quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				upgrade.xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				upgrade.yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				upgrade.quality = block[boffset + 7]; /* quality (1 byte) */
				upgrade.ySrlLen = *((UINT16*) &block[boffset + 8]); /* ySrlLen (2 bytes) */
				upgrade.yRawLen = *((UINT16*) &block[boffset + 10]); /* yRawLen (2 bytes) */
				upgrade.cbSrlLen = *((UINT16*) &block[boffset + 12]); /* cbSrlLen (2 bytes) */
				upgrade.cbRawLen = *((UINT16*) &block[boffset + 14]); /* cbRawLen (2 bytes) */
				upgrade.crSrlLen = *((UINT16*) &block[boffset + 16]); /* crSrlLen (2 bytes) */
				upgrade.crRawLen = *((UINT16*) &block[boffset + 18]); /* crRawLen (2 bytes) */
				boffset += 20;

				if ((blockLen - boffset) < upgrade.ySrlLen)
					return -1033;

				upgrade.ySrlData = &block[boffset];
				boffset += upgrade.ySrlLen;

				if ((blockLen - boffset) < upgrade.yRawLen)
					return -1034;

				upgrade.yRawData = &block[boffset];
				boffset += upgrade.yRawLen;

				if ((blockLen - boffset) < upgrade.cbSrlLen)
					return -1035;

				upgrade.cbSrlData = &block[boffset];
				boffset += upgrade.cbSrlLen;

				if ((blockLen - boffset) < upgrade.cbRawLen)
					return -1036;

				upgrade.cbRawData = &block[boffset];
				boffset += upgrade.cbRawLen;

				if ((blockLen - boffset) < upgrade.crSrlLen)
					return -1037;

				upgrade.crSrlData = &block[boffset];
				boffset += upgrade.crSrlLen;

				if ((blockLen - boffset) < upgrade.crRawLen)
					return -1038;

				upgrade.crRawData = &block[boffset];
				boffset += upgrade.crRawLen;

				status = progressive_decompress_tile_upgrade(progressive, &upgrade);

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

	return (int) offset;
}

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	int status;

	status = progressive_process_blocks(progressive, pSrcData, SrcSize, 0, PROGRESSIVE_BLOCKS_ALL);

	if (status >= 0)
		status = 1;

	return status;
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

		progressive->cRects = 64;
		progressive->rects = (RFX_RECT*) malloc(progressive->cRects * sizeof(RFX_RECT));

		if (!progressive->rects)
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

		progressive_context_reset(progressive);
	}

	return progressive;
}

void progressive_context_free(PROGRESSIVE_CONTEXT* progressive)
{
	if (!progressive)
		return;

	free(progressive->rects);
	free(progressive->quantVals);
	free(progressive->quantProgVals);

	free(progressive);
}

