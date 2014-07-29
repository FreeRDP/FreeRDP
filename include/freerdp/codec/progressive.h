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

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>

#define PROGRESSIVE_WBT_SYNC				0xCCC0
#define PROGRESSIVE_WBT_FRAME_BEGIN			0xCCC1
#define PROGRESSIVE_WBT_FRAME_END			0xCCC2
#define PROGRESSIVE_WBT_CONTEXT				0xCCC3
#define PROGRESSIVE_WBT_REGION				0xCCC4
#define PROGRESSIVE_WBT_TILE_SIMPLE			0xCCC5
#define PROGRESSIVE_WBT_TILE_PROGRESSIVE_FIRST		0xCCC6
#define PROGRESSIVE_WBT_TILE_PROGRESSIVE_UPGRADE	0xCCC7

struct _PROGRESSIVE_SYNC
{
	UINT16 blockType;
	UINT32 blockLen;

	UINT32 magic;
	UINT16 version;
};
typedef struct _PROGRESSIVE_SYNC PROGRESSIVE_SYNC;

struct _PROGRESSIVE_REGION
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
	UINT32* quantVals;
	UINT32* quantProgVals;
};
typedef struct _PROGRESSIVE_REGION PROGRESSIVE_REGION;

struct _PROGRESSIVE_FRAME_BEGIN
{
	UINT16 blockType;
	UINT32 blockLen;

	UINT32 frameIndex;
	UINT16 regionCount;
	PROGRESSIVE_REGION* regions;
};
typedef struct _PROGRESSIVE_FRAME_BEGIN PROGRESSIVE_FRAME_BEGIN;

struct _PROGRESSIVE_FRAME_END
{
	UINT16 blockType;
	UINT32 blockLen;
};
typedef struct _PROGRESSIVE_FRAME_END PROGRESSIVE_FRAME_END;

struct _PROGRESSIVE_TILE_SIMPLE
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
typedef struct _PROGRESSIVE_TILE_SIMPLE PROGRESSIVE_TILE_SIMPLE;

struct _PROGRESSIVE_TILE_FIRST
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
typedef struct _PROGRESSIVE_TILE_FIRST PROGRESSIVE_TILE_FIRST;

struct _PROGRESSIVE_TILE_UPGRADE
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
typedef struct _PROGRESSIVE_TILE_UPGRADE PROGRESSIVE_TILE_UPGRADE;

struct _PROGRESSIVE_CONTEXT
{
	BOOL Compressor;
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
 
