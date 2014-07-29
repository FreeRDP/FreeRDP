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

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	BYTE* block;
	UINT16 index;
	UINT32 boffset;
	UINT16 blockType;
	UINT32 blockLen;
	RFX_RECT* rect;
	UINT32 offset = 0;
	PROGRESSIVE_BLOCK_SYNC sync;
	PROGRESSIVE_BLOCK_REGION region;
	PROGRESSIVE_BLOCK_CONTEXT context;
	PROGRESSIVE_BLOCK_FRAME_BEGIN frameBegin;
	PROGRESSIVE_BLOCK_FRAME_END frameEnd;
	PROGRESSIVE_BLOCK_TILE_SIMPLE simple;
	PROGRESSIVE_BLOCK_TILE_FIRST first;
	PROGRESSIVE_BLOCK_TILE_UPGRADE upgrade;
	RFX_COMPONENT_CODEC_QUANT* quantVal;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVal;

	printf("ProgressiveDecompress\n");

	if (SrcSize < 6)
		return -1001;

	while ((SrcSize - offset) >= 6)
	{
		boffset = 0;
		block = &pSrcData[offset];

		blockType = *((UINT16*) &block[boffset + 0]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		if ((SrcSize - offset) < blockLen)
			return -1002;

		switch (blockType)
		{
			case PROGRESSIVE_WBT_SYNC:

				sync.blockType = blockType;
				sync.blockLen = blockLen;

				if (blockLen != 12)
					return -1003;

				sync.magic = (UINT32) *((UINT32*) &block[boffset + 0]); /* magic (4 bytes) */
				sync.version = (UINT32) *((UINT16*) &block[boffset + 4]); /* version (2 bytes) */
				boffset += 6;

				/* magic SHOULD be set to 0xCACCACCA, but the decoder SHOULD ignore it */
				/* version SHOULD be set to 0x0100, but the decoder SHOULD ignore it */

				break;

			case PROGRESSIVE_WBT_FRAME_BEGIN:

				frameBegin.blockType = blockType;
				frameBegin.blockLen = blockLen;

				if ((blockLen - boffset) < 6)
					return -1004;

				frameBegin.frameIndex = (UINT32) *((UINT32*) &block[boffset + 0]); /* frameIndex (4 bytes) */
				frameBegin.regionCount = (UINT32) *((UINT16*) &block[boffset + 4]); /* regionCount (2 bytes) */
				boffset += 6;

				/**
				 * If the number of elements specified by the regionCount field is
				 * larger than the actual number of elements in the regions field,
				 * the decoder SHOULD ignore this inconsistency.
				 */

				if ((blockLen - boffset) > 6)
				{
					fprintf(stderr, "warning: regions present in frame begin block are ignored\n");
				}

				boffset = blockLen;

				break;

			case PROGRESSIVE_WBT_FRAME_END:

				frameEnd.blockType = blockType;
				frameEnd.blockLen = blockLen;

				if (blockLen != 6)
					return -1005;

				break;

			case PROGRESSIVE_WBT_CONTEXT:

				context.blockType = blockType;
				context.blockLen = blockLen;

				if (blockLen != 10)
					return -1006;

				context.ctxId = block[boffset + 0]; /* ctxId (1 byte) */
				context.tileSize = *((UINT16*) &block[boffset + 1]); /* tileSize (2 bytes) */
				context.flags = block[boffset + 3]; /* flags (1 byte) */
				boffset += 4;

				if (context.tileSize != 64)
					return -1007;

				break;

			case PROGRESSIVE_WBT_REGION:

				region.blockType = blockType;
				region.blockLen = blockLen;

				if ((blockLen - boffset) < 12)
					return -1008;

				region.tileSize = block[boffset + 0]; /* tileSize (1 byte) */
				region.numRects = *((UINT16*) &block[boffset + 1]); /* numRects (2 bytes) */
				region.numQuant = block[boffset + 3]; /* numQuant (1 byte) */
				region.numProgQuant = block[boffset + 4]; /* numProgQuant (1 byte) */
				region.flags = block[boffset + 5]; /* flags (1 byte) */
				region.numTiles = *((UINT16*) &block[boffset + 6]); /* numTiles (2 bytes) */
				region.tileDataSize = *((UINT32*) &block[boffset + 8]); /* tileDataSize (4 bytes) */
				boffset += 12;

				if (region.tileSize != 64)
					return -1;

				if (region.numRects < 1)
					return -1;

				if (region.numQuant > 7)
					return -1;

				if ((blockLen - boffset) < (region.numRects * 8))
					return -1;

				region.rects = (RFX_RECT*) malloc(region.numRects * sizeof(RFX_RECT));

				if (!region.rects)
					return -1;

				for (index = 0; index < region.numRects; index++)
				{
					rect = &(region.rects[index]);
					rect->x = *((UINT16*) &block[boffset + 0]);
					rect->y = *((UINT16*) &block[boffset + 2]);
					rect->width = *((UINT16*) &block[boffset + 4]);
					rect->height = *((UINT16*) &block[boffset + 6]);
					boffset += 8;
				}

				if (region.numQuant > 0)
				{
					if ((blockLen - boffset) < (region.numQuant * 5))
						return -1;

					region.quantVals = (RFX_COMPONENT_CODEC_QUANT*) malloc(region.numQuant * sizeof(RFX_COMPONENT_CODEC_QUANT));

					if (!region.quantVals)
						return -1;

					for (index = 0; index < region.numQuant; index++)
					{
						quantVal = &(region.quantVals[index]);
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
				}

				if (region.numProgQuant > 0)
				{
					if ((blockLen - boffset) < (region.numProgQuant * 16))
						return -1;

					region.quantProgVals = (RFX_PROGRESSIVE_CODEC_QUANT*) malloc(region.numProgQuant * sizeof(RFX_PROGRESSIVE_CODEC_QUANT));

					if (!region.quantVals)
						return -1;

					for (index = 0; index < region.numProgQuant; index++)
					{
						quantProgVal = &(region.quantProgVals[index]);
						quantProgVal->quality = block[boffset + 0];
						CopyMemory(quantProgVal->yQuantValues, &block[boffset + 1], 5);
						CopyMemory(quantProgVal->cbQuantValues, &block[boffset + 6], 5);
						CopyMemory(quantProgVal->crQuantValues, &block[boffset + 11], 5);
						boffset += 16;
					}
				}

				printf("numTiles: %d tileDataSize: %d numQuant: %d numProgQuant: %d\n",
						region.numTiles, region.tileDataSize, region.numQuant, region.numProgQuant);

				boffset += region.tileDataSize; /* skip for now */

				break;

			case PROGRESSIVE_WBT_TILE_SIMPLE:

				simple.blockType = blockType;
				simple.blockLen = blockLen;

				if ((blockLen - boffset) < 16)
					return -1009;

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

				simple.yData = &block[boffset];
				boffset += simple.yLen;

				simple.cbData = &block[boffset];
				boffset += simple.cbLen;

				simple.crData = &block[boffset];
				boffset += simple.crLen;

				simple.tailData = &block[boffset];
				boffset += simple.tailLen;

				break;

			case PROGRESSIVE_WBT_TILE_FIRST:

				first.blockType = blockType;
				first.blockLen = blockLen;

				if ((blockLen - boffset) < 17)
					return -1010;

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
					return -1011;

				first.yData = &block[boffset];
				boffset += first.yLen;

				if ((blockLen - boffset) < first.cbLen)
					return -1012;

				first.cbData = &block[boffset];
				boffset += first.cbLen;

				if ((blockLen - boffset) < first.crLen)
					return -1013;

				first.crData = &block[boffset];
				boffset += first.crLen;

				if ((blockLen - boffset) < first.tailLen)
					return -1014;

				first.tailData = &block[boffset];
				boffset += first.tailLen;

				break;

			case PROGRESSIVE_WBT_TILE_UPGRADE:

				upgrade.blockType = blockType;
				upgrade.blockLen = blockLen;

				if ((blockLen - boffset) < 18)
					return -1015;

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
				boffset += 18;

				if ((blockLen - boffset) < upgrade.ySrlLen)
					return -1016;

				upgrade.ySrlData = &block[boffset];
				boffset += upgrade.ySrlLen;

				if ((blockLen - boffset) < upgrade.yRawLen)
					return -1017;

				upgrade.yRawData = &block[boffset];
				boffset += upgrade.yRawLen;

				if ((blockLen - boffset) < upgrade.cbSrlLen)
					return -1018;

				upgrade.cbSrlData = &block[boffset];
				boffset += upgrade.cbSrlLen;

				if ((blockLen - boffset) < upgrade.cbRawLen)
					return -1019;

				upgrade.cbRawData = &block[boffset];
				boffset += upgrade.cbRawLen;

				if ((blockLen - boffset) < upgrade.crSrlLen)
					return -1020;

				upgrade.crSrlData = &block[boffset];
				boffset += upgrade.crSrlLen;

				if ((blockLen - boffset) < upgrade.crRawLen)
					return -1021;

				upgrade.crRawData = &block[boffset];
				boffset += upgrade.crRawLen;

				break;

			default:
				return -1022;
				break;
		}

		if (boffset != blockLen)
		{
			printf("failure %s\n", progressive_get_block_type_string(blockType));
			return -1023;
		}

		offset += blockLen;
	}

	if (offset != SrcSize)
		return -1024;

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
	}

	return progressive;
}

void progressive_context_free(PROGRESSIVE_CONTEXT* progressive)
{
	if (!progressive)
		return;

	free(progressive);
}

