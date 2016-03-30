/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP6 Planar Codec
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CODEC_PLANAR_H
#define FREERDP_CODEC_PLANAR_H

#include <winpr/crt.h>

typedef struct _BITMAP_PLANAR_CONTEXT BITMAP_PLANAR_CONTEXT;

#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#define PLANAR_FORMAT_HEADER_CS		(1 << 3)
#define PLANAR_FORMAT_HEADER_RLE	(1 << 4)
#define PLANAR_FORMAT_HEADER_NA		(1 << 5)
#define PLANAR_FORMAT_HEADER_CLL_MASK	0x07

#define PLANAR_CONTROL_BYTE(_nRunLength, _cRawBytes) \
	(_nRunLength & 0x0F) | ((_cRawBytes & 0x0F) << 4)

#define PLANAR_CONTROL_BYTE_RUN_LENGTH(_controlByte)	(_controlByte & 0x0F)
#define PLANAR_CONTROL_BYTE_RAW_BYTES(_controlByte)	((_controlByte >> 4) & 0x0F)

struct _RDP6_RLE_SEGMENT
{
	/**
	 * controlByte:
	 * [0-3]: nRunLength
	 * [4-7]: cRawBytes
	 */
	BYTE controlByte;
	BYTE* rawValues;
};
typedef struct _RDP6_RLE_SEGMENT RDP6_RLE_SEGMENT;

struct _RDP6_RLE_SEGMENTS
{
	UINT32 cSegments;
	RDP6_RLE_SEGMENT* segments;
};
typedef struct _RDP6_RLE_SEGMENTS RDP6_RLE_SEGMENTS;

struct _RDP6_BITMAP_STREAM
{
	/**
	 * formatHeader:
	 * [0-2]: Color Loss Level (CLL)
	 *  [3] : Chroma Subsampling (CS)
	 *  [4] : Run Length Encoding (RLE)
	 *  [5] : No Alpha (NA)
	 * [6-7]: Reserved
	 */
	BYTE formatHeader;
};
typedef struct _RDP6_BITMAP_STREAM RDP6_BITMAP_STREAM;

struct _BITMAP_PLANAR_CONTEXT
{
	int maxWidth;
	int maxHeight;
	int maxPlaneSize;

	BOOL AllowSkipAlpha;
	BOOL AllowRunLengthEncoding;
	BOOL AllowColorSubsampling;
	BOOL AllowDynamicColorFidelity;

	int ColorLossLevel;

	BYTE* planes[4];
	BYTE* planesBuffer;

	BYTE* deltaPlanes[4];
	BYTE* deltaPlanesBuffer;

	BYTE* rlePlanes[4];
	BYTE* rlePlanesBuffer;

	UINT32 TempSize;
	BYTE* TempBuffer;
};

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API int freerdp_split_color_planes(BYTE* data, UINT32 format, int width, int height, int scanline, BYTE* planes[4]);
FREERDP_API BYTE* freerdp_bitmap_planar_compress_plane_rle(BYTE* plane, int width, int height, BYTE* outPlane, int* dstSize);
FREERDP_API BYTE* freerdp_bitmap_planar_delta_encode_plane(BYTE* inPlane, int width, int height, BYTE* outPlane);
FREERDP_API BOOL freerdp_bitmap_planar_delta_encode_planes(BYTE* inPlanes[4], int width, int height, BYTE* outPlanes[4]);

FREERDP_API BYTE* freerdp_bitmap_compress_planar(BITMAP_PLANAR_CONTEXT* context, BYTE* data, UINT32 format,
		int width, int height, int scanline, BYTE* dstData, int* pDstSize);

FREERDP_API BOOL freerdp_bitmap_planar_context_reset(BITMAP_PLANAR_CONTEXT* context);

FREERDP_API BITMAP_PLANAR_CONTEXT* freerdp_bitmap_planar_context_new(DWORD flags, int maxWidth, int maxHeight);
FREERDP_API void freerdp_bitmap_planar_context_free(BITMAP_PLANAR_CONTEXT* context);

FREERDP_API int planar_decompress(BITMAP_PLANAR_CONTEXT* planar, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight, BOOL vFlip);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_CODEC_PLANAR_H */

