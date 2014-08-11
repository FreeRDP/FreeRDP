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

int progressive_rfx_decode_component(PROGRESSIVE_CONTEXT* progressive,
		RFX_COMPONENT_CODEC_QUANT* quant, const BYTE* data, int length, INT16* buffer)
{
	int status;

	status = rfx_rlgr_decode(data, length, buffer, 4096, 1);

	if (status < 0)
		return status;

	return 1;
}

int progressive_decompress_tile_first(PROGRESSIVE_CONTEXT* progressive, RFX_PROGRESSIVE_TILE* tile)
{
	BYTE* pBuffer;
	INT16* pSrcDst[3];
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

	pBuffer = (BYTE*) BufferPool_Take(progressive->bufferPool, -1);
	pSrcDst[0] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 0) + 16])); /* Y/R buffer */
	pSrcDst[1] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 1) + 16])); /* Cb/G buffer */
	pSrcDst[2] = (INT16*)((BYTE*)(&pBuffer[((8192 + 32) * 2) + 16])); /* Cr/B buffer */

	progressive_rfx_decode_component(progressive, quantY, tile->yData, tile->yLen, pSrcDst[0]); /* Y */
	progressive_rfx_decode_component(progressive, quantCb, tile->cbData, tile->cbLen, pSrcDst[1]); /* Cb */
	progressive_rfx_decode_component(progressive, quantCr, tile->crData, tile->crLen, pSrcDst[2]); /* Cr */

	BufferPool_Return(progressive->bufferPool, pBuffer);

	return 1;
}

int progressive_decompress_tile_upgrade(PROGRESSIVE_CONTEXT* progressive, RFX_PROGRESSIVE_TILE* tile)
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

int progressive_process_tiles(PROGRESSIVE_CONTEXT* progressive, BYTE* blocks, UINT32 blocksLen)
{
	BYTE* block;
	UINT16 index;
	UINT32 boffset;
	UINT32 count = 0;
	UINT32 offset = 0;
	RFX_PROGRESSIVE_TILE* tile;
	RFX_PROGRESSIVE_TILE* tiles;
	PROGRESSIVE_BLOCK_REGION* region;

	region = &(progressive->region);

	tiles = region->tiles;

	while ((blocksLen - offset) >= 6)
	{
		boffset = 0;
		block = &blocks[offset];

		tile = &tiles[count];

		tile->blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		tile->blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		printf("%s\n", progressive_get_block_type_string(tile->blockType));

		if ((blocksLen - offset) < tile->blockLen)
			return -1003;

		switch (tile->blockType)
		{
			case PROGRESSIVE_WBT_TILE_SIMPLE:

				if ((tile->blockLen - boffset) < 16)
					return -1022;

				tile->quality = 0xFF; /* simple tiles use no progressive techniques */

				tile->quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				tile->quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				tile->quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				tile->xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				tile->yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				tile->flags = block[boffset + 7]; /* flags (1 byte) */
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

				break;

			case PROGRESSIVE_WBT_TILE_FIRST:

				if ((tile->blockLen - boffset) < 17)
					return -1027;

				tile->quantIdxY = block[boffset + 0]; /* quantIdxY (1 byte) */
				tile->quantIdxCb = block[boffset + 1]; /* quantIdxCb (1 byte) */
				tile->quantIdxCr = block[boffset + 2]; /* quantIdxCr (1 byte) */
				tile->xIdx = *((UINT16*) &block[boffset + 3]); /* xIdx (2 bytes) */
				tile->yIdx = *((UINT16*) &block[boffset + 5]); /* yIdx (2 bytes) */
				tile->flags = block[boffset + 7]; /* flags (1 byte) */
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

				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:

				if ((tile->blockLen - boffset) < 20)
					return -1032;

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

				break;

			default:
				return -1039;
				break;
		}

		if (boffset != tile->blockLen)
			return -1040;

		offset += tile->blockLen;
		count++;
	}

	if (offset != blocksLen)
		return -1041;

	for (index = 0; index < region->numTiles; index++)
	{
		tile = &tiles[index];

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

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
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
	RFX_COMPONENT_CODEC_QUANT* quantVal;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

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

				if (region->numTiles > progressive->cTiles)
				{
					progressive->tiles = (RFX_PROGRESSIVE_TILE*) realloc(progressive->tiles,
							region->numTiles * sizeof(RFX_PROGRESSIVE_TILE));
					progressive->cTiles = region->numTiles;
				}

				region->tiles = progressive->tiles;

				if (!region->tiles)
					return -1;

				printf("numRects: %d numTiles: %d numQuant: %d numProgQuant: %d\n",
						region->numRects, region->numTiles, region->numQuant, region->numProgQuant);

				status = progressive_process_tiles(progressive, &block[boffset], region->tileDataSize);

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

		progressive->bufferPool = BufferPool_New(TRUE, (8192 + 32) * 3, 16);

		progressive->cRects = 64;
		progressive->rects = (RFX_RECT*) malloc(progressive->cRects * sizeof(RFX_RECT));

		if (!progressive->rects)
			return NULL;

		progressive->cTiles = 64;
		progressive->tiles = (RFX_PROGRESSIVE_TILE*) malloc(progressive->cTiles * sizeof(RFX_PROGRESSIVE_TILE));

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

	free(progressive);
}

