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

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	BYTE* block;
	UINT32 boffset;
	UINT32 ctxId;
	UINT32 flags;
	UINT32 tileSize;
	UINT32 magic;
	UINT32 version;
	UINT16 blockType;
	UINT32 blockLen;
	UINT32 offset = 0;
	UINT32 frameIndex;
	UINT32 regionCount;
	PROGRESSIVE_REGION region;
	PROGRESSIVE_TILE_SIMPLE simple;
	PROGRESSIVE_TILE_FIRST first;
	PROGRESSIVE_TILE_UPGRADE upgrade;

	printf("ProgressiveDecompress\n");

	while ((SrcSize - offset) > 6)
	{
		boffset = 0;
		block = &pSrcData[offset];

		blockType = *((UINT16*) &block[boffset]); /* blockType (2 bytes) */
		blockLen = *((UINT32*) &block[boffset + 2]); /* blockLen (4 bytes) */
		boffset += 6;

		switch (blockType)
		{
			case PROGRESSIVE_WBT_SYNC:

				if (blockLen != 12)
					return -1;

				magic = (UINT32) *((UINT32*) &block[boffset]); /* magic (4 bytes) */
				version = (UINT32) *((UINT16*) &block[boffset + 4]); /* version (2 bytes) */
				boffset += 6;

				break;

			case PROGRESSIVE_WBT_FRAME_BEGIN:

				frameIndex = (UINT32) *((UINT32*) &block[boffset]); /* frameIndex (4 bytes) */
				regionCount = (UINT32) *((UINT16*) &block[boffset + 4]); /* regionCount (2 bytes) */
				boffset += 6;

				break;

			case PROGRESSIVE_WBT_FRAME_END:

				if (blockLen != 6)
					return -1;

				break;

			case PROGRESSIVE_WBT_CONTEXT:

				if (blockLen != 10)
					return -1;

				ctxId = (UINT32) block[boffset]; /* ctxId (1 byte) */
				tileSize = (UINT32) *((UINT16*) &block[boffset + 1]); /* tileSize (2 bytes) */
				flags = (UINT32) block[boffset + 3]; /* flags (1 byte) */
				boffset += 4;

				if (tileSize != 64)
					return -1;

				break;

			case PROGRESSIVE_WBT_REGION:

				region.tileSize = block[boffset]; /* tileSize (1 byte) */
				region.numRects = *((UINT16*) &block[boffset + 1]); /* numRects (2 bytes) */
				region.numQuant = block[boffset + 3]; /* numQuant (1 byte) */
				region.numProgQuant = block[boffset + 4]; /* numProgQuant (1 byte) */
				region.flags = block[boffset + 5]; /* flags (1 byte) */
				region.numTiles = *((UINT16*) &block[boffset + 6]); /* numTiles (2 bytes) */
				region.tileDataSize = *((UINT32*) &block[boffset + 8]); /* tileDataSize (4 bytes) */
				boffset += 12;

				break;

			case PROGRESSIVE_WBT_TILE_SIMPLE:

				simple.quantIdxY = block[boffset]; /* quantIdxY (1 byte) */
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

			case PROGRESSIVE_WBT_TILE_PROGRESSIVE_FIRST:

				first.quantIdxY = block[boffset]; /* quantIdxY (1 byte) */
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

				first.yData = &block[boffset];
				boffset += first.yLen;

				first.cbData = &block[boffset];
				boffset += first.cbLen;

				first.crData = &block[boffset];
				boffset += first.crLen;

				first.tailData = &block[boffset];
				boffset += first.tailLen;

				break;

			case PROGRESSIVE_WBT_TILE_PROGRESSIVE_UPGRADE:

				upgrade.quantIdxY = block[boffset]; /* quantIdxY (1 byte) */
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

				upgrade.ySrlData = &block[boffset];
				boffset += upgrade.ySrlLen;

				upgrade.yRawData = &block[boffset];
				boffset += upgrade.yRawLen;

				upgrade.cbSrlData = &block[boffset];
				boffset += upgrade.cbSrlLen;

				upgrade.cbRawData = &block[boffset];
				boffset += upgrade.cbRawLen;

				upgrade.crSrlData = &block[boffset];
				boffset += upgrade.crSrlLen;

				upgrade.crRawData = &block[boffset];
				boffset += upgrade.crRawLen;

				break;
		}

		offset += blockLen;
	}

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

