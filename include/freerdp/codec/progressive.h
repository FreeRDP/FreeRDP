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

#ifndef FREERDP_CODEC_PROGRESSIVE_H
#define FREERDP_CODEC_PROGRESSIVE_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/collections.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>

#define RFX_SUBBAND_DIFFING				0x01

#define RFX_TILE_DIFFERENCE				0x01

#define RFX_DWT_REDUCE_EXTRAPOLATE			0x01

#define PROGRESSIVE_WBT_SYNC				0xCCC0
#define PROGRESSIVE_WBT_FRAME_BEGIN			0xCCC1
#define PROGRESSIVE_WBT_FRAME_END			0xCCC2
#define PROGRESSIVE_WBT_CONTEXT				0xCCC3
#define PROGRESSIVE_WBT_REGION				0xCCC4
#define PROGRESSIVE_WBT_TILE_SIMPLE			0xCCC5
#define PROGRESSIVE_WBT_TILE_FIRST			0xCCC6
#define PROGRESSIVE_WBT_TILE_UPGRADE			0xCCC7

#define PROGRESSIVE_BLOCKS_ALL				0x0001
#define PROGRESSIVE_BLOCKS_REGION			0x0002
#define PROGRESSIVE_BLOCKS_TILE				0x0004

struct _RFX_PROGRESSIVE_CODEC_QUANT
{
	BYTE quality;
	BYTE yQuantValues[5];
	BYTE cbQuantValues[5];
	BYTE crQuantValues[5];
};
typedef struct _RFX_PROGRESSIVE_CODEC_QUANT RFX_PROGRESSIVE_CODEC_QUANT;

struct _RFX_COMPONENT_CODEC_QUANT
{
	BYTE LL3;
	BYTE HL3;
	BYTE LH3;
	BYTE HH3;
	BYTE HL2;
	BYTE LH2;
	BYTE HH2;
	BYTE HL1;
	BYTE LH1;
	BYTE HH1;
};
typedef struct _RFX_COMPONENT_CODEC_QUANT RFX_COMPONENT_CODEC_QUANT;

struct _PROGRESSIVE_BLOCK
{
	UINT16 blockType;
	UINT32 blockLen;
};
typedef struct _PROGRESSIVE_BLOCK PROGRESSIVE_BLOCK;

struct _PROGRESSIVE_BLOCK_SYNC
{
	UINT16 blockType;
	UINT32 blockLen;

	UINT32 magic;
	UINT16 version;
};
typedef struct _PROGRESSIVE_BLOCK_SYNC PROGRESSIVE_BLOCK_SYNC;

struct _PROGRESSIVE_BLOCK_CONTEXT
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE ctxId;
	UINT16 tileSize;
	BYTE flags;
};
typedef struct _PROGRESSIVE_BLOCK_CONTEXT PROGRESSIVE_BLOCK_CONTEXT;

struct _PROGRESSIVE_BLOCK_TILE_SIMPLE
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx;
	UINT16 yIdx;
	BYTE flags;
	UINT16 yLen;
	UINT16 cbLen;
	UINT16 crLen;
	UINT16 tailLen;
	BYTE* yData;
	BYTE* cbData;
	BYTE* crData;
	BYTE* tailData;
};
typedef struct _PROGRESSIVE_BLOCK_TILE_SIMPLE PROGRESSIVE_BLOCK_TILE_SIMPLE;

struct _PROGRESSIVE_BLOCK_TILE_FIRST
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx;
	UINT16 yIdx;
	BYTE flags;
	BYTE quality;
	UINT16 yLen;
	UINT16 cbLen;
	UINT16 crLen;
	UINT16 tailLen;
	BYTE* yData;
	BYTE* cbData;
	BYTE* crData;
	BYTE* tailData;
};
typedef struct _PROGRESSIVE_BLOCK_TILE_FIRST PROGRESSIVE_BLOCK_TILE_FIRST;

struct _PROGRESSIVE_BLOCK_TILE_UPGRADE
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx;
	UINT16 yIdx;
	BYTE quality;
	UINT16 ySrlLen;
	UINT16 yRawLen;
	UINT16 cbSrlLen;
	UINT16 cbRawLen;
	UINT16 crSrlLen;
	UINT16 crRawLen;
	BYTE* ySrlData;
	BYTE* yRawData;
	BYTE* cbSrlData;
	BYTE* cbRawData;
	BYTE* crSrlData;
	BYTE* crRawData;
};
typedef struct _PROGRESSIVE_BLOCK_TILE_UPGRADE PROGRESSIVE_BLOCK_TILE_UPGRADE;

struct _RFX_PROGRESSIVE_TILE
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE quantIdxY;
	BYTE quantIdxCb;
	BYTE quantIdxCr;
	UINT16 xIdx;
	UINT16 yIdx;

	BYTE flags;
	BYTE quality;

	UINT16 yLen;
	UINT16 cbLen;
	UINT16 crLen;
	UINT16 tailLen;
	BYTE* yData;
	BYTE* cbData;
	BYTE* crData;
	BYTE* tailData;

	UINT16 ySrlLen;
	UINT16 yRawLen;
	UINT16 cbSrlLen;
	UINT16 cbRawLen;
	UINT16 crSrlLen;
	UINT16 crRawLen;
	BYTE* ySrlData;
	BYTE* yRawData;
	BYTE* cbSrlData;
	BYTE* cbRawData;
	BYTE* crSrlData;
	BYTE* crRawData;
};
typedef struct _RFX_PROGRESSIVE_TILE RFX_PROGRESSIVE_TILE;

struct _PROGRESSIVE_BLOCK_REGION
{
	UINT16 blockType;
	UINT32 blockLen;

	BYTE tileSize;
	UINT16 numRects;
	BYTE numQuant;
	BYTE numProgQuant;
	BYTE flags;
	UINT16 numTiles;
	UINT32 tileDataSize;
	RFX_RECT* rects;
	RFX_COMPONENT_CODEC_QUANT* quantVals;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVals;
	RFX_PROGRESSIVE_TILE* tiles;
};
typedef struct _PROGRESSIVE_BLOCK_REGION PROGRESSIVE_BLOCK_REGION;

struct _PROGRESSIVE_BLOCK_FRAME_BEGIN
{
	UINT16 blockType;
	UINT32 blockLen;

	UINT32 frameIndex;
	UINT16 regionCount;
	PROGRESSIVE_BLOCK_REGION* regions;
};
typedef struct _PROGRESSIVE_BLOCK_FRAME_BEGIN PROGRESSIVE_BLOCK_FRAME_BEGIN;

struct _PROGRESSIVE_BLOCK_FRAME_END
{
	UINT16 blockType;
	UINT32 blockLen;
};
typedef struct _PROGRESSIVE_BLOCK_FRAME_END PROGRESSIVE_BLOCK_FRAME_END;

struct _PROGRESSIVE_CONTEXT
{
	BOOL Compressor;

	wBufferPool* bufferPool;

	UINT32 cRects;
	RFX_RECT* rects;

	UINT32 cTiles;
	RFX_PROGRESSIVE_TILE* tiles;

	UINT32 cQuant;
	RFX_COMPONENT_CODEC_QUANT* quantVals;

	UINT32 cProgQuant;
	RFX_PROGRESSIVE_CODEC_QUANT* quantProgVals;

	PROGRESSIVE_BLOCK_REGION region;
	RFX_PROGRESSIVE_CODEC_QUANT quantProgValFull;
};
typedef struct _PROGRESSIVE_CONTEXT PROGRESSIVE_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API int progressive_compress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize);

FREERDP_API int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight);

FREERDP_API void progressive_context_reset(PROGRESSIVE_CONTEXT* progressive);

FREERDP_API PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor);
FREERDP_API void progressive_context_free(PROGRESSIVE_CONTEXT* progressive);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_PROGRESSIVE_H */
 
