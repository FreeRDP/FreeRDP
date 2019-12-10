/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Progressive Codec Bitmap Compression
 *
 * Copyright 2017 Armin Novak <anovak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#ifndef INTERNAL_CODEC_PROGRESSIVE_H
#define INTERNAL_CODEC_PROGRESSIVE_H

#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/codec/rfx.h>

#define RFX_SUBBAND_DIFFING 0x01

#define RFX_TILE_DIFFERENCE 0x01

#define RFX_DWT_REDUCE_EXTRAPOLATE 0x01

#define PROGRESSIVE_WBT_SYNC 0xCCC0
#define PROGRESSIVE_WBT_FRAME_BEGIN 0xCCC1
#define PROGRESSIVE_WBT_FRAME_END 0xCCC2
#define PROGRESSIVE_WBT_CONTEXT 0xCCC3
#define PROGRESSIVE_WBT_REGION 0xCCC4
#define PROGRESSIVE_WBT_TILE_SIMPLE 0xCCC5
#define PROGRESSIVE_WBT_TILE_FIRST 0xCCC6
#define PROGRESSIVE_WBT_TILE_UPGRADE 0xCCC7

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

struct _RFX_PROGRESSIVE_CODEC_QUANT
{
	BYTE quality;
	RFX_COMPONENT_CODEC_QUANT yQuantValues;
	RFX_COMPONENT_CODEC_QUANT cbQuantValues;
	RFX_COMPONENT_CODEC_QUANT crQuantValues;
};
typedef struct _RFX_PROGRESSIVE_CODEC_QUANT RFX_PROGRESSIVE_CODEC_QUANT;

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
	const BYTE* yData;
	const BYTE* cbData;
	const BYTE* crData;
	const BYTE* tailData;

	UINT16 ySrlLen;
	UINT16 yRawLen;
	UINT16 cbSrlLen;
	UINT16 cbRawLen;
	UINT16 crSrlLen;
	UINT16 crRawLen;
	const BYTE* ySrlData;
	const BYTE* yRawData;
	const BYTE* cbSrlData;
	const BYTE* cbRawData;
	const BYTE* crSrlData;
	const BYTE* crRawData;

	UINT32 x;
	UINT32 y;
	UINT32 width;
	UINT32 height;
	UINT32 stride;

	BYTE* data;
	BYTE* current;

	UINT16 pass;
	BYTE* sign;

	RFX_COMPONENT_CODEC_QUANT yBitPos;
	RFX_COMPONENT_CODEC_QUANT cbBitPos;
	RFX_COMPONENT_CODEC_QUANT crBitPos;
	RFX_COMPONENT_CODEC_QUANT yQuant;
	RFX_COMPONENT_CODEC_QUANT cbQuant;
	RFX_COMPONENT_CODEC_QUANT crQuant;
	RFX_COMPONENT_CODEC_QUANT yProgQuant;
	RFX_COMPONENT_CODEC_QUANT cbProgQuant;
	RFX_COMPONENT_CODEC_QUANT crProgQuant;
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
	UINT16 usedTiles;
	UINT32 tileDataSize;
	RFX_RECT rects[0x10000];
	RFX_COMPONENT_CODEC_QUANT quantVals[0x100];
	RFX_PROGRESSIVE_CODEC_QUANT quantProgVals[0x100];
	RFX_PROGRESSIVE_TILE* tiles[0x10000];
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

struct _PROGRESSIVE_SURFACE_CONTEXT
{
	UINT16 id;
	UINT32 width;
	UINT32 height;
	UINT32 gridWidth;
	UINT32 gridHeight;
	UINT32 gridSize;
	RFX_PROGRESSIVE_TILE* tiles;
};
typedef struct _PROGRESSIVE_SURFACE_CONTEXT PROGRESSIVE_SURFACE_CONTEXT;

enum _WBT_STATE_FLAG
{
	FLAG_WBT_SYNC = 0x01,
	FLAG_WBT_FRAME_BEGIN = 0x02,
	FLAG_WBT_FRAME_END = 0x04,
	FLAG_WBT_CONTEXT = 0x08,
	FLAG_WBT_REGION = 0x10
};
typedef enum _WBT_STATE_FLAG WBT_STATE_FLAG;

struct _PROGRESSIVE_CONTEXT
{
	BOOL Compressor;

	wBufferPool* bufferPool;

	UINT32 format;
	UINT32 state;

	PROGRESSIVE_BLOCK_CONTEXT context;
	PROGRESSIVE_BLOCK_REGION region;
	RFX_PROGRESSIVE_CODEC_QUANT quantProgValFull;

	wHashTable* SurfaceContexts;
	wLog* log;
};

#endif /* INTERNAL_CODEC_PROGRESSIVE_H */
