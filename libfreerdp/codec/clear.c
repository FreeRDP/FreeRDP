/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/clear.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("codec.clear")

#define CLEARCODEC_FLAG_GLYPH_INDEX 0x01
#define CLEARCODEC_FLAG_GLYPH_HIT 0x02
#define CLEARCODEC_FLAG_CACHE_RESET 0x04

#define CLEARCODEC_VBAR_SIZE 32768
#define CLEARCODEC_VBAR_SHORT_SIZE 16384

/* Encoder-only: open-addressed hash index mapping V-Bar column content to its slot in
 * clear->VBarStorage, so clear_compress_bands_data() can find VBAR_CACHE_HIT candidates in
 * O(1) instead of scanning the whole cache. Must be a power of two, comfortably larger than
 * CLEARCODEC_VBAR_SIZE so insert() always finds a free/tombstoned slot. */
#define CLEAR_VBAR_HASH_SIZE 65536u
#define CLEAR_VBAR_HASH_EMPTY 0u
#define CLEAR_VBAR_HASH_DELETED 0xFFFFFFFFu
#define CLEAR_BAND_MAX_HEIGHT 52u

typedef struct
{
	UINT32 size;
	UINT32 count;
	UINT32* pixels;
} CLEAR_GLYPH_ENTRY;

typedef struct
{
	UINT32 size;
	UINT32 count;
	BYTE* pixels;
} CLEAR_VBAR_ENTRY;

struct S_CLEAR_CONTEXT
{
	BOOL Compressor;
	NSC_CONTEXT* nsc;
	UINT32 seqNumber;
	BYTE* TempBuffer;
	size_t TempSize;
	UINT32 format;
	BOOL formatSet;
	CLEAR_GLYPH_ENTRY GlyphCache[4000];
	UINT32 VBarStorageCursor;
	CLEAR_VBAR_ENTRY VBarStorage[CLEARCODEC_VBAR_SIZE];
	UINT32 ShortVBarStorageCursor;
	CLEAR_VBAR_ENTRY ShortVBarStorage[CLEARCODEC_VBAR_SHORT_SIZE];
	wLog* log;
	wStream* bs;           /* clear_compress() output, owned by this context, Compressor only */
	UINT32* VBarHashSlots; /* clear_compress() V-Bar lookup index, Compressor only, see above */
};

static const UINT32 CLEAR_LOG2_FLOOR[256] = {
	0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static const BYTE CLEAR_8BIT_MASKS[9] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };

static void clear_reset_vbar_storage(CLEAR_CONTEXT* WINPR_RESTRICT clear, BOOL zero)
{
	if (zero)
	{
		for (size_t i = 0; i < ARRAYSIZE(clear->VBarStorage); i++)
			winpr_aligned_free(clear->VBarStorage[i].pixels);

		ZeroMemory(clear->VBarStorage, sizeof(clear->VBarStorage));

		if (clear->VBarHashSlots)
			ZeroMemory(clear->VBarHashSlots, CLEAR_VBAR_HASH_SIZE * sizeof(UINT32));
	}

	clear->VBarStorageCursor = 0;

	if (zero)
	{
		for (size_t i = 0; i < ARRAYSIZE(clear->ShortVBarStorage); i++)
			winpr_aligned_free(clear->ShortVBarStorage[i].pixels);

		ZeroMemory(clear->ShortVBarStorage, sizeof(clear->ShortVBarStorage));
	}

	clear->ShortVBarStorageCursor = 0;
}

static void clear_reset_glyph_cache(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	for (size_t i = 0; i < ARRAYSIZE(clear->GlyphCache); i++)
		winpr_aligned_free(clear->GlyphCache[i].pixels);

	ZeroMemory(clear->GlyphCache, sizeof(clear->GlyphCache));
}

static BOOL convert_color(BYTE* WINPR_RESTRICT dst, UINT32 nDstStep, UINT32 DstFormat, UINT32 nXDst,
                          UINT32 nYDst, UINT32 nWidth, UINT32 nHeight,
                          const BYTE* WINPR_RESTRICT src, UINT32 nSrcStep, UINT32 SrcFormat,
                          UINT32 nDstWidth, UINT32 nDstHeight,
                          const gdiPalette* WINPR_RESTRICT palette)
{
	if (nWidth + nXDst > nDstWidth)
		nWidth = nDstWidth - nXDst;

	if (nHeight + nYDst > nDstHeight)
		nHeight = nDstHeight - nYDst;

	return freerdp_image_copy_no_overlap(dst, DstFormat, nDstStep, nXDst, nYDst, nWidth, nHeight,
	                                     src, SrcFormat, nSrcStep, 0, 0, palette,
	                                     FREERDP_KEEP_DST_ALPHA);
}

static BOOL clear_decompress_nscodec(wLog* log, NSC_CONTEXT* WINPR_RESTRICT nsc, UINT32 width,
                                     UINT32 height, wStream* WINPR_RESTRICT s,
                                     UINT32 bitmapDataByteCount, BYTE* WINPR_RESTRICT pDstData,
                                     UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDstRel,
                                     UINT32 nYDstRel, UINT32 nDstWidth, UINT32 nDstHeight)
{
	BOOL rc = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bitmapDataByteCount))
		return FALSE;

	rc = nsc_process_message(nsc, 32, width, height, Stream_Pointer(s), bitmapDataByteCount,
	                         pDstData, DstFormat, nDstStep, nXDstRel, nYDstRel, nDstWidth,
	                         nDstHeight, FREERDP_FLIP_NONE);
	Stream_Seek(s, bitmapDataByteCount);
	return rc;
}

static BOOL clear_decompress_subcode_rlex(wLog* log, wStream* WINPR_RESTRICT s,
                                          UINT32 bitmapDataByteCount, UINT32 width, UINT32 height,
                                          BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                          UINT32 nDstStep, UINT32 nXDstRel, UINT32 nYDstRel,
                                          UINT32 nDstWidth, UINT32 nDstHeight)
{
	UINT32 x = 0;
	UINT32 y = 0;
	UINT32 pixelCount = 0;
	UINT32 bitmapDataOffset = 0;
	size_t pixelIndex = 0;
	UINT32 numBits = 0;
	BYTE startIndex = 0;
	BYTE stopIndex = 0;
	BYTE suiteIndex = 0;
	BYTE suiteDepth = 0;
	BYTE paletteCount = 0;
	UINT32 palette[128] = WINPR_C_ARRAY_INIT;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, bitmapDataByteCount))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 1))
		return FALSE;
	Stream_Read_UINT8(s, paletteCount);
	bitmapDataOffset = 1 + (paletteCount * 3);

	if ((paletteCount > 127) || (paletteCount < 1))
	{
		WLog_Print(log, WLOG_ERROR, "paletteCount %" PRIu8 "", paletteCount);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, paletteCount, 3ull))
		return FALSE;

	for (UINT32 i = 0; i < paletteCount; i++)
	{
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		Stream_Read_UINT8(s, b);
		Stream_Read_UINT8(s, g);
		Stream_Read_UINT8(s, r);
		palette[i] = FreeRDPGetColor(DstFormat, r, g, b, 0xFF);
	}

	pixelIndex = 0;
	pixelCount = width * height;
	numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

	while (bitmapDataOffset < bitmapDataByteCount)
	{
		UINT32 tmp = 0;
		UINT32 color = 0;
		UINT32 runLengthFactor = 0;

		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
			return FALSE;

		Stream_Read_UINT8(s, tmp);
		Stream_Read_UINT8(s, runLengthFactor);
		bitmapDataOffset += 2;
		suiteDepth = (tmp >> numBits) & CLEAR_8BIT_MASKS[(8 - numBits)];
		stopIndex = tmp & CLEAR_8BIT_MASKS[numBits];
		startIndex = stopIndex - suiteDepth;

		if (runLengthFactor >= 0xFF)
		{
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, runLengthFactor);
			bitmapDataOffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
					return FALSE;

				Stream_Read_UINT32(s, runLengthFactor);
				bitmapDataOffset += 4;
			}
		}

		if (startIndex >= paletteCount)
		{
			WLog_Print(log, WLOG_ERROR, "startIndex %" PRIu8 " > paletteCount %" PRIu8 "]",
			           startIndex, paletteCount);
			return FALSE;
		}

		if (stopIndex >= paletteCount)
		{
			WLog_Print(log, WLOG_ERROR, "stopIndex %" PRIu8 " > paletteCount %" PRIu8 "]",
			           stopIndex, paletteCount);
			return FALSE;
		}

		suiteIndex = startIndex;

		if (suiteIndex > 127)
		{
			WLog_Print(log, WLOG_ERROR, "suiteIndex %" PRIu8 " > 127]", suiteIndex);
			return FALSE;
		}

		color = palette[suiteIndex];

		if ((pixelIndex + runLengthFactor) > pixelCount)
		{
			WLog_Print(log, WLOG_ERROR,
			           "pixelIndex %" PRIuz " + runLengthFactor %" PRIu32 " > pixelCount %" PRIu32
			           "",
			           pixelIndex, runLengthFactor, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i < runLengthFactor; i++)
		{
			BYTE* pTmpData = &pDstData[(nXDstRel + x) * FreeRDPGetBytesPerPixel(DstFormat) +
			                           (nYDstRel + y) * nDstStep];

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				FreeRDPWriteColor(pTmpData, DstFormat, color);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += runLengthFactor;

		if ((pixelIndex + (suiteDepth + 1)) > pixelCount)
		{
			WLog_Print(log, WLOG_ERROR,
			           "pixelIndex %" PRIuz " + suiteDepth %" PRIu8 " + 1 > pixelCount %" PRIu32 "",
			           pixelIndex, suiteDepth, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i <= suiteDepth; i++)
		{
			BYTE* pTmpData = &pDstData[(nXDstRel + x) * FreeRDPGetBytesPerPixel(DstFormat) +
			                           (nYDstRel + y) * nDstStep];
			UINT32 ccolor = palette[suiteIndex];

			if (suiteIndex > 127)
			{
				WLog_Print(log, WLOG_ERROR, "suiteIndex %" PRIu8 " > 127", suiteIndex);
				return FALSE;
			}

			suiteIndex++;

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				FreeRDPWriteColor(pTmpData, DstFormat, ccolor);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += (suiteDepth + 1);
	}

	if (pixelIndex != pixelCount)
	{
		WLog_Print(log, WLOG_ERROR, "pixelIndex %" PRIuz " != pixelCount %" PRIu32 "", pixelIndex,
		           pixelCount);
		return FALSE;
	}

	return TRUE;
}

static BOOL clear_resize_buffer(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 width, UINT32 height)
{
	if (!clear)
		return FALSE;

	const UINT64 size = 1ull * (width + 16ull) * (height + 16ull);
	const size_t bpp = FreeRDPGetBytesPerPixel(clear->format);
	if (size > UINT32_MAX / bpp)
		return FALSE;

	if (size > clear->TempSize / bpp)
	{
		BYTE* tmp = (BYTE*)winpr_aligned_recalloc(clear->TempBuffer,
		                                          WINPR_ASSERTING_INT_CAST(size_t, size), bpp, 32);

		if (!tmp)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "clear->TempBuffer winpr_aligned_recalloc failed for %" PRIu64 " bytes",
			           size);
			return FALSE;
		}

		clear->TempSize = WINPR_ASSERTING_INT_CAST(size_t, size* bpp);
		clear->TempBuffer = tmp;
	}

	return TRUE;
}

static BOOL clear_decompress_residual_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                           wStream* WINPR_RESTRICT s, UINT32 residualByteCount,
                                           UINT32 nWidth, UINT32 nHeight,
                                           BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                           UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                           UINT32 nDstWidth, UINT32 nDstHeight,
                                           const gdiPalette* WINPR_RESTRICT palette)
{
	UINT32 nSrcStep = 0;
	UINT32 suboffset = 0;
	BYTE* dstBuffer = nullptr;
	UINT32 pixelIndex = 0;
	UINT32 pixelCount = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, residualByteCount))
		return FALSE;

	suboffset = 0;
	pixelIndex = 0;
	pixelCount = nWidth * nHeight;

	if (!clear_resize_buffer(clear, nWidth, nHeight))
		return FALSE;

	dstBuffer = clear->TempBuffer;

	while (suboffset < residualByteCount)
	{
		BYTE r = 0;
		BYTE g = 0;
		BYTE b = 0;
		UINT32 runLengthFactor = 0;
		UINT32 color = 0;

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 4))
			return FALSE;

		Stream_Read_UINT8(s, b);
		Stream_Read_UINT8(s, g);
		Stream_Read_UINT8(s, r);
		Stream_Read_UINT8(s, runLengthFactor);
		suboffset += 4;
		color = FreeRDPGetColor(clear->format, r, g, b, 0xFF);

		if (runLengthFactor >= 0xFF)
		{
			if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, runLengthFactor);
			suboffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 4))
					return FALSE;

				Stream_Read_UINT32(s, runLengthFactor);
				suboffset += 4;
			}
		}

		if ((pixelIndex >= pixelCount) || (runLengthFactor > (pixelCount - pixelIndex)))
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "pixelIndex %" PRIu32 " + runLengthFactor %" PRIu32 " > pixelCount %" PRIu32
			           "",
			           pixelIndex, runLengthFactor, pixelCount);
			return FALSE;
		}

		for (UINT32 i = 0; i < runLengthFactor; i++)
		{
			FreeRDPWriteColor(dstBuffer, clear->format, color);
			dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
		}

		pixelIndex += runLengthFactor;
	}

	nSrcStep = nWidth * FreeRDPGetBytesPerPixel(clear->format);

	if (pixelIndex != pixelCount)
	{
		WLog_Print(clear->log, WLOG_ERROR, "pixelIndex %" PRIu32 " != pixelCount %" PRIu32 "",
		           pixelIndex, pixelCount);
		return FALSE;
	}

	return convert_color(pDstData, nDstStep, DstFormat, nXDst, nYDst, nWidth, nHeight,
	                     clear->TempBuffer, nSrcStep, clear->format, nDstWidth, nDstHeight,
	                     palette);
}

static BOOL clear_decompress_subcodecs_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                            wStream* WINPR_RESTRICT s, UINT32 subcodecByteCount,
                                            UINT32 nWidth, UINT32 nHeight,
                                            BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                            UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                            UINT32 nDstWidth, UINT32 nDstHeight,
                                            const gdiPalette* WINPR_RESTRICT palette)
{
	UINT32 suboffset = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, subcodecByteCount))
		return FALSE;

	while (suboffset < subcodecByteCount)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 13))
			return FALSE;

		const UINT16 xStart = Stream_Get_UINT16(s);
		const UINT16 yStart = Stream_Get_UINT16(s);
		const UINT16 width = Stream_Get_UINT16(s);
		const UINT16 height = Stream_Get_UINT16(s);
		const UINT32 bitmapDataByteCount = Stream_Get_UINT32(s);
		const UINT8 subcodecId = Stream_Get_UINT8(s);
		suboffset += 13;

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, bitmapDataByteCount))
			return FALSE;

		const UINT32 nXDstRel = nXDst + xStart;
		const UINT32 nYDstRel = nYDst + yStart;
		if (1ull * nXDstRel + width > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "nXDstRel %" PRIu32 " + width %" PRIu16 " > nDstWidth %" PRIu32 "", nXDstRel,
			           width, nDstWidth);
			return FALSE;
		}
		if (1ull * nYDstRel + height > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "nYDstRel %" PRIu32 " + height %" PRIu16 " > nDstHeight %" PRIu32 "",
			           nYDstRel, height, nDstHeight);
			return FALSE;
		}

		if (1ull * xStart + width > nWidth)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "xStart %" PRIu16 " + width %" PRIu16 " > nWidth %" PRIu32 "", xStart, width,
			           nWidth);
			return FALSE;
		}
		if (1ull * yStart + height > nHeight)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "yStart %" PRIu16 " + height %" PRIu16 " > nHeight %" PRIu32 "", yStart,
			           height, nHeight);
			return FALSE;
		}

		if (!clear_resize_buffer(clear, width, height))
			return FALSE;

		switch (subcodecId)
		{
			case 0: /* Uncompressed */
			{
				const UINT32 nSrcStep = width * FreeRDPGetBytesPerPixel(PIXEL_FORMAT_BGR24);
				const size_t nSrcSize = 1ull * nSrcStep * height;

				if (bitmapDataByteCount != nSrcSize)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "bitmapDataByteCount %" PRIu32 " != nSrcSize %" PRIuz "",
					           bitmapDataByteCount, nSrcSize);
					return FALSE;
				}

				if (!convert_color(pDstData, nDstStep, DstFormat, nXDstRel, nYDstRel, width, height,
				                   Stream_Pointer(s), nSrcStep, PIXEL_FORMAT_BGR24, nDstWidth,
				                   nDstHeight, palette))
					return FALSE;

				Stream_Seek(s, bitmapDataByteCount);
			}
			break;

			case 1: /* NSCodec */
				if (!clear_decompress_nscodec(clear->log, clear->nsc, width, height, s,
				                              bitmapDataByteCount, pDstData, DstFormat, nDstStep,
				                              nXDstRel, nYDstRel, nDstWidth, nDstHeight))
					return FALSE;

				break;

			case 2: /* CLEARCODEC_SUBCODEC_RLEX */
				if (!clear_decompress_subcode_rlex(clear->log, s, bitmapDataByteCount, width,
				                                   height, pDstData, DstFormat, nDstStep, nXDstRel,
				                                   nYDstRel, nDstWidth, nDstHeight))
					return FALSE;

				break;

			default:
				WLog_Print(clear->log, WLOG_ERROR, "Unknown subcodec ID %" PRIu8 "", subcodecId);
				return FALSE;
		}

		suboffset += bitmapDataByteCount;
	}

	return TRUE;
}

static BOOL resize_vbar_entry(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                              CLEAR_VBAR_ENTRY* WINPR_RESTRICT vBarEntry, UINT32 bpp)
{
	if (vBarEntry->count > vBarEntry->size)
	{
		const UINT32 oldPos = vBarEntry->size * bpp;
		const UINT32 diffSize = (vBarEntry->count - vBarEntry->size) * bpp;

		BYTE* tmp =
		    (BYTE*)winpr_aligned_recalloc(vBarEntry->pixels, vBarEntry->count, 1ull * bpp, 32);

		if (!tmp)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "vBarEntry->pixels winpr_aligned_recalloc %" PRIu32 " failed",
			           vBarEntry->count * bpp);
			return FALSE;
		}

		memset(&tmp[oldPos], 0, diffSize);
		vBarEntry->pixels = tmp;
		vBarEntry->size = vBarEntry->count;
	}

	if (!vBarEntry->pixels && vBarEntry->size)
	{
		WLog_Print(clear->log, WLOG_ERROR,
		           "vBarEntry->pixels is nullptr but vBarEntry->size is %" PRIu32 "",
		           vBarEntry->size);
		return FALSE;
	}

	return TRUE;
}

static BOOL clear_decompress_bands_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                        wStream* WINPR_RESTRICT s, UINT32 bandsByteCount,
                                        UINT32 nWidth, UINT32 nHeight,
                                        BYTE* WINPR_RESTRICT pDstData, UINT32 DstFormat,
                                        UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                                        UINT32 nDstWidth, UINT32 nDstHeight)
{
	UINT32 suboffset = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, bandsByteCount))
		return FALSE;

	while (suboffset < bandsByteCount)
	{
		BYTE cr = 0;
		BYTE cg = 0;
		BYTE cb = 0;
		UINT16 xStart = 0;
		UINT16 xEnd = 0;
		UINT16 yStart = 0;
		UINT16 yEnd = 0;
		UINT32 colorBkg = 0;
		UINT16 vBarHeader = 0;
		UINT16 vBarYOn = 0;
		UINT16 vBarYOff = 0;
		UINT32 vBarCount = 0;
		UINT32 vBarPixelCount = 0;
		UINT32 vBarShortPixelCount = 0;

		if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 11))
			return FALSE;

		Stream_Read_UINT16(s, xStart);
		Stream_Read_UINT16(s, xEnd);
		Stream_Read_UINT16(s, yStart);
		Stream_Read_UINT16(s, yEnd);
		Stream_Read_UINT8(s, cb);
		Stream_Read_UINT8(s, cg);
		Stream_Read_UINT8(s, cr);
		suboffset += 11;
		colorBkg = FreeRDPGetColor(clear->format, cr, cg, cb, 0xFF);

		if (xEnd < xStart)
		{
			WLog_Print(clear->log, WLOG_ERROR, "xEnd %" PRIu16 " < xStart %" PRIu16 "", xEnd,
			           xStart);
			return FALSE;
		}

		if (yEnd < yStart)
		{
			WLog_Print(clear->log, WLOG_ERROR, "yEnd %" PRIu16 " < yStart %" PRIu16 "", yEnd,
			           yStart);
			return FALSE;
		}

		vBarCount = (xEnd - xStart) + 1;

		for (UINT32 i = 0; i < vBarCount; i++)
		{
			UINT32 vBarHeight = 0;
			CLEAR_VBAR_ENTRY* vBarEntry = nullptr;
			CLEAR_VBAR_ENTRY* vBarShortEntry = nullptr;
			BOOL vBarUpdate = FALSE;
			const BYTE* cpSrcPixel = nullptr;

			if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
				return FALSE;

			Stream_Read_UINT16(s, vBarHeader);
			suboffset += 2;
			vBarHeight = (yEnd - yStart + 1);

			if (vBarHeight > 52)
			{
				WLog_Print(clear->log, WLOG_ERROR, "vBarHeight (%" PRIu32 ") > 52", vBarHeight);
				return FALSE;
			}

			if ((vBarHeader & 0xC000) == 0x4000) /* SHORT_VBAR_CACHE_HIT */
			{
				const UINT16 vBarIndex = (vBarHeader & 0x3FFF);
				vBarShortEntry = &(clear->ShortVBarStorage[vBarIndex]);

				if (!vBarShortEntry)
				{
					WLog_Print(clear->log, WLOG_ERROR, "missing vBarShortEntry %" PRIu16 "",
					           vBarIndex);
					return FALSE;
				}

				if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 1))
					return FALSE;

				Stream_Read_UINT8(s, vBarYOn);
				suboffset += 1;
				vBarShortPixelCount = vBarShortEntry->count;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0xC000) == 0x0000) /* SHORT_VBAR_CACHE_MISS */
			{
				vBarYOn = (vBarHeader & 0xFF);
				vBarYOff = ((vBarHeader >> 8) & 0x3F);

				if (vBarYOff < vBarYOn)
				{
					WLog_Print(clear->log, WLOG_ERROR, "vBarYOff %" PRIu16 " < vBarYOn %" PRIu16 "",
					           vBarYOff, vBarYOn);
					return FALSE;
				}

				vBarShortPixelCount = (vBarYOff - vBarYOn);

				if (vBarShortPixelCount > 52)
				{
					WLog_Print(clear->log, WLOG_ERROR, "vBarShortPixelCount %" PRIu32 " > 52",
					           vBarShortPixelCount);
					return FALSE;
				}

				if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(clear->log, s, vBarShortPixelCount,
				                                                3ull))
					return FALSE;

				if (clear->ShortVBarStorageCursor >= CLEARCODEC_VBAR_SHORT_SIZE)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "clear->ShortVBarStorageCursor %" PRIu32
					           " >= CLEARCODEC_VBAR_SHORT_SIZE (%" PRId32 ")",
					           clear->ShortVBarStorageCursor, CLEARCODEC_VBAR_SHORT_SIZE);
					return FALSE;
				}

				vBarShortEntry = &(clear->ShortVBarStorage[clear->ShortVBarStorageCursor]);
				vBarShortEntry->count = vBarShortPixelCount;

				if (!resize_vbar_entry(clear, vBarShortEntry,
				                       FreeRDPGetBytesPerPixel(clear->format)))
					return FALSE;

				for (size_t y = 0; y < vBarShortPixelCount; y++)
				{
					BYTE r = 0;
					BYTE g = 0;
					BYTE b = 0;
					BYTE* dstBuffer =
					    &vBarShortEntry->pixels[y * FreeRDPGetBytesPerPixel(clear->format)];
					UINT32 color = 0;
					Stream_Read_UINT8(s, b);
					Stream_Read_UINT8(s, g);
					Stream_Read_UINT8(s, r);
					color = FreeRDPGetColor(clear->format, r, g, b, 0xFF);

					if (!FreeRDPWriteColor(dstBuffer, clear->format, color))
						return FALSE;
				}

				suboffset += (vBarShortPixelCount * 3);
				clear->ShortVBarStorageCursor =
				    (clear->ShortVBarStorageCursor + 1) % CLEARCODEC_VBAR_SHORT_SIZE;
				vBarUpdate = TRUE;
			}
			else if ((vBarHeader & 0x8000) == 0x8000) /* VBAR_CACHE_HIT */
			{
				const UINT16 vBarIndex = (vBarHeader & 0x7FFF);
				vBarEntry = &(clear->VBarStorage[vBarIndex]);

				/* If the cache was reset we need to fill in some dummy data. */
				if (vBarEntry->size == 0)
				{
					WLog_Print(clear->log, WLOG_WARN,
					           "Empty cache index %" PRIu16 ", filling dummy data", vBarIndex);
					vBarEntry->count = vBarHeight;

					if (!resize_vbar_entry(clear, vBarEntry,
					                       FreeRDPGetBytesPerPixel(clear->format)))
						return FALSE;
				}
			}
			else
			{
				WLog_Print(clear->log, WLOG_ERROR, "invalid vBarHeader 0x%04" PRIX16 "",
				           vBarHeader);
				return FALSE; /* invalid vBarHeader */
			}

			if (vBarUpdate)
			{
				BYTE* pSrcPixel = nullptr;
				BYTE* dstBuffer = nullptr;

				if (clear->VBarStorageCursor >= CLEARCODEC_VBAR_SIZE)
				{
					WLog_Print(clear->log, WLOG_ERROR,
					           "clear->VBarStorageCursor %" PRIu32
					           " >= CLEARCODEC_VBAR_SIZE %" PRId32 "",
					           clear->VBarStorageCursor, CLEARCODEC_VBAR_SIZE);
					return FALSE;
				}

				vBarEntry = &(clear->VBarStorage[clear->VBarStorageCursor]);
				vBarPixelCount = vBarHeight;
				vBarEntry->count = vBarPixelCount;

				if (!resize_vbar_entry(clear, vBarEntry, FreeRDPGetBytesPerPixel(clear->format)))
					return FALSE;

				dstBuffer = vBarEntry->pixels;
				/* if (y < vBarYOn), use colorBkg */
				UINT32 y = 0;
				UINT32 count = vBarYOn;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					while (count--)
					{
						FreeRDPWriteColor(dstBuffer, clear->format, colorBkg);
						dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
					}
				}

				/*
				 * if ((y >= vBarYOn) && (y < (vBarYOn + vBarShortPixelCount))),
				 * use vBarShortPixels at index (y - shortVBarYOn)
				 */
				y = vBarYOn;
				count = vBarShortPixelCount;

				if ((y + count) > vBarPixelCount)
					count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					const size_t offset =
					    (1ull * y - vBarYOn) * FreeRDPGetBytesPerPixel(clear->format);
					pSrcPixel = &vBarShortEntry->pixels[offset];
					if (offset + count > vBarShortEntry->count)
					{
						WLog_Print(clear->log, WLOG_ERROR,
						           "offset + count > vBarShortEntry->count");
						return FALSE;
					}
				}
				for (size_t x = 0; x < count; x++)
				{
					UINT32 color = 0;
					color = FreeRDPReadColor(&pSrcPixel[x * FreeRDPGetBytesPerPixel(clear->format)],
					                         clear->format);

					if (!FreeRDPWriteColor(dstBuffer, clear->format, color))
						return FALSE;

					dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
				}

				/* if (y >= (vBarYOn + vBarShortPixelCount)), use colorBkg */
				y = vBarYOn + vBarShortPixelCount;
				count = (vBarPixelCount > y) ? (vBarPixelCount - y) : 0;

				if (count > 0)
				{
					while (count--)
					{
						if (!FreeRDPWriteColor(dstBuffer, clear->format, colorBkg))
							return FALSE;

						dstBuffer += FreeRDPGetBytesPerPixel(clear->format);
					}
				}

				vBarEntry->count = vBarPixelCount;
				clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % CLEARCODEC_VBAR_SIZE;
			}

			if (vBarEntry->count != vBarHeight)
			{
				WLog_Print(clear->log, WLOG_ERROR,
				           "vBarEntry->count %" PRIu32 " != vBarHeight %" PRIu32 "",
				           vBarEntry->count, vBarHeight);
				vBarEntry->count = vBarHeight;

				if (!resize_vbar_entry(clear, vBarEntry, FreeRDPGetBytesPerPixel(clear->format)))
					return FALSE;
			}

			const UINT32 nXDstRel = nXDst + xStart;
			const UINT32 nYDstRel = nYDst + yStart;
			cpSrcPixel = vBarEntry->pixels;

			if (i < nWidth)
			{
				UINT32 count = vBarEntry->count;

				if (count > nHeight)
					count = nHeight;

				if (nXDstRel + i >= nDstWidth)
					return FALSE;

				for (UINT32 y = 0; y < count; y++)
				{
					if (nYDstRel + y >= nDstHeight)
						return FALSE;

					BYTE* pDstPixel8 =
					    &pDstData[((nYDstRel + y) * nDstStep) +
					              ((nXDstRel + i) * FreeRDPGetBytesPerPixel(DstFormat))];
					UINT32 color = FreeRDPReadColor(cpSrcPixel, clear->format);
					color = FreeRDPConvertColor(color, clear->format, DstFormat, nullptr);

					if (!FreeRDPWriteColor(pDstPixel8, DstFormat, color))
						return FALSE;

					cpSrcPixel += FreeRDPGetBytesPerPixel(clear->format);
				}
			}
		}
	}

	return TRUE;
}

static BOOL clear_decompress_glyph_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                        wStream* WINPR_RESTRICT s, UINT32 glyphFlags, UINT32 nWidth,
                                        UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
                                        UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
                                        UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
                                        const gdiPalette* WINPR_RESTRICT palette,
                                        BYTE** WINPR_RESTRICT ppGlyphData)
{
	UINT16 glyphIndex = 0;

	if (ppGlyphData)
		*ppGlyphData = nullptr;

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT) && !(glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX))
	{
		WLog_Print(clear->log, WLOG_ERROR, "Invalid glyph flags %08" PRIX32 "", glyphFlags);
		return FALSE;
	}

	if ((glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX) == 0)
		return TRUE;

	if ((nWidth * nHeight) > (1024 * 1024))
	{
		WLog_Print(clear->log, WLOG_ERROR, "glyph too large: %" PRIu32 "x%" PRIu32 "", nWidth,
		           nHeight);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, glyphIndex);

	if (glyphIndex >= 4000)
	{
		WLog_Print(clear->log, WLOG_ERROR, "Invalid glyphIndex %" PRIu16 "", glyphIndex);
		return FALSE;
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_HIT)
	{
		UINT32 nSrcStep = 0;
		CLEAR_GLYPH_ENTRY* glyphEntry = &(clear->GlyphCache[glyphIndex]);
		BYTE* glyphData = nullptr;

		if (!glyphEntry)
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear->GlyphCache[%" PRIu16 "]=nullptr",
			           glyphIndex);
			return FALSE;
		}

		glyphData = (BYTE*)glyphEntry->pixels;

		if (!glyphData)
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear->GlyphCache[%" PRIu16 "]->pixels=nullptr",
			           glyphIndex);
			return FALSE;
		}

		if ((nWidth * nHeight) > glyphEntry->count)
		{
			WLog_Print(clear->log, WLOG_ERROR,
			           "(nWidth %" PRIu32 " * nHeight %" PRIu32 ") > glyphEntry->count %" PRIu32 "",
			           nWidth, nHeight, glyphEntry->count);
			return FALSE;
		}

		nSrcStep = nWidth * FreeRDPGetBytesPerPixel(clear->format);
		return convert_color(pDstData, nDstStep, DstFormat, nXDst, nYDst, nWidth, nHeight,
		                     glyphData, nSrcStep, clear->format, nDstWidth, nDstHeight, palette);
	}

	if (glyphFlags & CLEARCODEC_FLAG_GLYPH_INDEX)
	{
		const UINT32 bpp = FreeRDPGetBytesPerPixel(clear->format);
		CLEAR_GLYPH_ENTRY* glyphEntry = &(clear->GlyphCache[glyphIndex]);
		const size_t count = 1ull * nWidth * nHeight;
		const size_t hlimit = SIZE_MAX / ((nWidth > 0) ? nWidth : 1);
		if ((nWidth == 0) || (nHeight == 0) || (hlimit < nHeight))
		{
			const char* exceeded = (hlimit < nHeight) ? "within" : "outside";
			WLog_Print(clear->log, WLOG_ERROR,
			           "CLEARCODEC_FLAG_GLYPH_INDEX: nWidth=%" PRIu32 ", nHeight=%" PRIu32
			           ", nWidth * nHeight is %s allowed range",
			           nWidth, nHeight, exceeded);
			return FALSE;
		}

		if (count > glyphEntry->size)
		{
			BYTE* tmp = winpr_aligned_recalloc(glyphEntry->pixels, count, 1ull * bpp, 32);

			if (!tmp)
			{
				WLog_Print(clear->log, WLOG_ERROR,
				           "glyphEntry->pixels winpr_aligned_recalloc %" PRIuz " failed!",
				           count * bpp);
				return FALSE;
			}

			glyphEntry->count = WINPR_ASSERTING_INT_CAST(UINT32, count);
			glyphEntry->size = glyphEntry->count;
			glyphEntry->pixels = WINPR_PACKED_ALIGN_CAST(UINT32*, tmp);
		}

		if (!glyphEntry->pixels)
		{
			WLog_Print(clear->log, WLOG_ERROR, "glyphEntry->pixels=nullptr");
			return FALSE;
		}

		if (ppGlyphData)
			*ppGlyphData = (BYTE*)glyphEntry->pixels;

		return TRUE;
	}

	return TRUE;
}

static inline BOOL updateContextFormat(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 DstFormat,
                                       BOOL fromNew)
{
	if (!clear || !clear->nsc)
		return FALSE;

	if (!fromNew)
	{
		if (clear->formatSet)
		{
			if (!FreeRDPAreColorFormatsEqualNoAlpha(clear->format, DstFormat))
			{
				WLog_Print(
				    clear->log, WLOG_ERROR,
				    "Color format changed from %s to %s during decompression calls, usage error!",
				    FreeRDPGetColorFormatName(clear->format), FreeRDPGetColorFormatName(DstFormat));
				return FALSE;
			}
		}
		else
			clear->formatSet = TRUE;
	}
	clear->format = DstFormat;
	return nsc_context_set_parameters(clear->nsc, NSC_COLOR_FORMAT, DstFormat);
}

INT32 clear_decompress(CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData,
                       UINT32 SrcSize, UINT32 nWidth, UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
                       UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
                       UINT32 nDstWidth, UINT32 nDstHeight,
                       const gdiPalette* WINPR_RESTRICT palette)
{
	INT32 rc = -1;
	BYTE seqNumber = 0;
	BYTE glyphFlags = 0;
	UINT32 residualByteCount = 0;
	UINT32 bandsByteCount = 0;
	UINT32 subcodecByteCount = 0;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	BYTE* glyphData = nullptr;

	if (!pDstData)
		return -1002;

	if ((nDstWidth == 0) || (nDstHeight == 0))
		return -1022;

	if ((nWidth > 0xFFFF) || (nHeight > 0xFFFF))
		return -1004;

	if (nXDst > nDstWidth)
	{
		WLog_Print(clear->log, WLOG_WARN, "nXDst %" PRIu32 " > nDstWidth %" PRIu32, nXDst,
		           nDstWidth);
		return -1005;
	}

	if (nYDst > nDstHeight)
	{
		WLog_Print(clear->log, WLOG_WARN, "nYDst %" PRIu32 " > nDstHeight %" PRIu32, nYDst,
		           nDstHeight);
		return -1006;
	}

	wStream* s = Stream_StaticConstInit(&sbuffer, pSrcData, SrcSize);

	if (!s)
		return -2005;

	if (!Stream_CheckAndLogRequiredLengthWLog(clear->log, s, 2))
		goto fail;

	if (!updateContextFormat(clear, DstFormat, FALSE))
		goto fail;

	Stream_Read_UINT8(s, glyphFlags);
	Stream_Read_UINT8(s, seqNumber);

	if (!clear->seqNumber && seqNumber)
		clear->seqNumber = seqNumber;

	if (seqNumber != clear->seqNumber)
	{
		WLog_Print(clear->log, WLOG_ERROR, "Sequence number unexpected %" PRIu8 " - %" PRIu32 "",
		           seqNumber, clear->seqNumber);
		WLog_Print(clear->log, WLOG_ERROR, "seqNumber %" PRIu8 " != clear->seqNumber %" PRIu32 "",
		           seqNumber, clear->seqNumber);
		goto fail;
	}

	clear->seqNumber = (seqNumber + 1) % 256;

	if (glyphFlags & CLEARCODEC_FLAG_CACHE_RESET)
	{
		clear_reset_vbar_storage(clear, FALSE);
	}

	if (!clear_decompress_glyph_data(clear, s, glyphFlags, nWidth, nHeight, pDstData, DstFormat,
	                                 nDstStep, nXDst, nYDst, nDstWidth, nDstHeight, palette,
	                                 &glyphData))
	{
		WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_glyph_data failed!");
		goto fail;
	}

	/* Read composition payload header parameters */
	if (Stream_GetRemainingLength(s) < 12)
	{
		const UINT32 mask = (CLEARCODEC_FLAG_GLYPH_HIT | CLEARCODEC_FLAG_GLYPH_INDEX);

		if ((glyphFlags & mask) == mask)
			goto finish;

		WLog_Print(clear->log, WLOG_ERROR,
		           "invalid glyphFlags, missing flags: 0x%02" PRIx8 " & 0x%02" PRIx32
		           " == 0x%02" PRIx32,
		           glyphFlags, mask, glyphFlags & mask);
		goto fail;
	}

	Stream_Read_UINT32(s, residualByteCount);
	Stream_Read_UINT32(s, bandsByteCount);
	Stream_Read_UINT32(s, subcodecByteCount);

	if (residualByteCount > 0)
	{
		if (!clear_decompress_residual_data(clear, s, residualByteCount, nWidth, nHeight, pDstData,
		                                    DstFormat, nDstStep, nXDst, nYDst, nDstWidth,
		                                    nDstHeight, palette))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_residual_data failed!");
			goto fail;
		}
	}

	if (bandsByteCount > 0)
	{
		if (!clear_decompress_bands_data(clear, s, bandsByteCount, nWidth, nHeight, pDstData,
		                                 DstFormat, nDstStep, nXDst, nYDst, nDstWidth, nDstHeight))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_bands_data failed!");
			goto fail;
		}
	}

	if (subcodecByteCount > 0)
	{
		if (!clear_decompress_subcodecs_data(clear, s, subcodecByteCount, nWidth, nHeight, pDstData,
		                                     DstFormat, nDstStep, nXDst, nYDst, nDstWidth,
		                                     nDstHeight, palette))
		{
			WLog_Print(clear->log, WLOG_ERROR, "clear_decompress_subcodecs_data failed!");
			goto fail;
		}
	}

	if (glyphData)
	{
		uint32_t w = MIN(nWidth, nDstWidth);
		if (nXDst > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area x exceeds destination: x=%" PRIu32 " > %" PRIu32, nXDst,
			           nDstWidth);
			w = 0;
		}
		else if (nXDst + w > nDstWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area x + width exceeds destination: x=%" PRIu32 " + %" PRIu32
			           " > %" PRIu32,
			           nXDst, w, nDstWidth);
			w = nDstWidth - nXDst;
		}

		if (w != nWidth)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area width truncated: requested=%" PRIu32
			           ", truncated to %" PRIu32,
			           nWidth, w);
		}

		uint32_t h = MIN(nHeight, nDstHeight);
		if (nYDst > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area y exceeds destination: y=%" PRIu32 " > %" PRIu32, nYDst,
			           nDstHeight);
			h = 0;
		}
		else if (nYDst + h > nDstHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area y + height exceeds destination: x=%" PRIu32
			           " + %" PRIu32 " > %" PRIu32,
			           nYDst, h, nDstHeight);
			h = nDstHeight - nYDst;
		}

		if (h != nHeight)
		{
			WLog_Print(clear->log, WLOG_WARN,
			           "glyphData copy area height truncated: requested=%" PRIu32
			           ", truncated to %" PRIu32,
			           nHeight, h);
		}

		if (!freerdp_image_copy_no_overlap(glyphData, clear->format, 0, 0, 0, w, h, pDstData,
		                                   DstFormat, nDstStep, nXDst, nYDst, palette,
		                                   FREERDP_KEEP_DST_ALPHA))
			goto fail;
	}

finish:
	rc = 0;
fail:
	return rc;
}

/* Write one CLEARCODEC_RGB_RUN_SEGMENT: BGR triplet + a 1/2/4 byte run length using the
 * 0xFF / 0xFFFF escape scheme mirrored from clear_decompress_residual_data(). */
static void clear_write_run_segment(wStream* WINPR_RESTRICT s, BYTE r, BYTE g, BYTE b,
                                    UINT32 runLength)
{
	Stream_Write_UINT8(s, b);
	Stream_Write_UINT8(s, g);
	Stream_Write_UINT8(s, r);

	if (runLength < 0xFF)
		Stream_Write_UINT8(s, WINPR_ASSERTING_INT_CAST(UINT8, runLength));
	else if (runLength < 0xFFFF)
	{
		Stream_Write_UINT8(s, 0xFF);
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, runLength));
	}
	else
	{
		Stream_Write_UINT8(s, 0xFF);
		Stream_Write_UINT16(s, 0xFFFF);
		Stream_Write_UINT32(s, runLength);
	}
}

/* Encode the whole rectangle as a single CLEARCODEC_RESIDUAL_DATA layer: a row-major run-length
 * encoding of BGR pixel runs. This is always spec-valid on its own (bands/subcodec layers are
 * optional) */
static BOOL clear_compress_residual_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                         wStream* WINPR_RESTRICT s,
                                         const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                         UINT32 nSrcStep, UINT32 nWidth, UINT32 nHeight)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	const UINT64 pixelCount = 1ull * nWidth * nHeight;
	/* Worst case: every pixel differs from its predecessor, i.e. one 4-byte segment per pixel. */
	const UINT64 requiredBytes = pixelCount * 4ull;
	BOOL havePending = FALSE;
	BYTE pr = 0;
	BYTE pg = 0;
	BYTE pb = 0;
	UINT32 runLength = 0;

	if (!Stream_EnsureRemainingCapacity(s, WINPR_ASSERTING_INT_CAST(size_t, requiredBytes)))
	{
		WLog_Print(clear->log, WLOG_ERROR,
		           "Stream_EnsureRemainingCapacity failed for %" PRIu64 " bytes", requiredBytes);
		return FALSE;
	}

	for (UINT32 y = 0; y < nHeight; y++)
	{
		const BYTE* pSrcPixel = &pSrcData[1ull * y * nSrcStep];

		for (UINT32 x = 0; x < nWidth; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;
			BYTE a = 0;
			const UINT32 color = FreeRDPReadColor(pSrcPixel, SrcFormat);
			pSrcPixel += bpp;
			FreeRDPSplitColor(color, SrcFormat, &r, &g, &b, &a, nullptr);

			if (havePending && (r == pr) && (g == pg) && (b == pb))
			{
				runLength++;
				continue;
			}

			if (havePending)
				clear_write_run_segment(s, pr, pg, pb, runLength);

			pr = r;
			pg = g;
			pb = b;
			runLength = 1;
			havePending = TRUE;
		}
	}

	if (havePending)
		clear_write_run_segment(s, pr, pg, pb, runLength);

	return TRUE;
}

/* FNV-1a 64 bit, used only to pick a starting probe bucket for the encoder's V-Bar lookup
 * index; every candidate is still verified with a full memcmp, so collisions only cost an
 * extra probe, never a wrong answer. */
static UINT64 clear_vbar_content_hash(const BYTE* WINPR_RESTRICT data, size_t len)
{
	UINT64 hash = 0xcbf29ce484222325ull;

	for (size_t i = 0; i < len; i++)
	{
		hash ^= data[i];
		hash *= 0x100000001b3ull;
	}

	return hash;
}

/* Find an existing clear->VBarStorage slot whose content exactly matches [data, data+count*3).
 * Read-only: never mutates clear->VBarStorage or the hash index. Returns the slot index, or
 * UINT32_MAX if no such content is currently cached. */
static UINT32 clear_vbar_hash_lookup(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                     const BYTE* WINPR_RESTRICT data, UINT32 count)
{
	const size_t len = 1ull * count * 3;
	const UINT64 hash = clear_vbar_content_hash(data, len);
	UINT32 i = WINPR_ASSERTING_INT_CAST(UINT32, hash & (CLEAR_VBAR_HASH_SIZE - 1));

	for (UINT32 probe = 0; probe < CLEAR_VBAR_HASH_SIZE; probe++)
	{
		const UINT32 slot = clear->VBarHashSlots[i];

		if (slot == CLEAR_VBAR_HASH_EMPTY)
			return UINT32_MAX;

		if (slot != CLEAR_VBAR_HASH_DELETED)
		{
			const UINT32 index = slot - 1;
			const CLEAR_VBAR_ENTRY* entry = &clear->VBarStorage[index];

			if ((entry->count == count) && (memcmp(entry->pixels, data, len) == 0))
				return index;
		}

		i = (i + 1) & (CLEAR_VBAR_HASH_SIZE - 1);
	}

	return UINT32_MAX;
}

/* Remove whatever entry currently maps to clear->VBarStorage[index] from the hash index, based
 * on its CURRENT content. Must be called before that slot's content is overwritten, otherwise
 * the index would keep pointing a stale lookup at content that is no longer there. */
static void clear_vbar_hash_remove(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 index)
{
	const CLEAR_VBAR_ENTRY* entry = &clear->VBarStorage[index];

	if ((entry->count == 0) || !entry->pixels)
		return;

	const size_t len = 1ull * entry->count * 3;
	const UINT64 hash = clear_vbar_content_hash(entry->pixels, len);
	UINT32 i = WINPR_ASSERTING_INT_CAST(UINT32, hash & (CLEAR_VBAR_HASH_SIZE - 1));

	for (UINT32 probe = 0; probe < CLEAR_VBAR_HASH_SIZE; probe++)
	{
		if (clear->VBarHashSlots[i] == (index + 1))
		{
			clear->VBarHashSlots[i] = CLEAR_VBAR_HASH_DELETED;
			return;
		}

		if (clear->VBarHashSlots[i] == CLEAR_VBAR_HASH_EMPTY)
			return;

		i = (i + 1) & (CLEAR_VBAR_HASH_SIZE - 1);
	}
}

/* Index clear->VBarStorage[index] (using its current content) so future lookups can find it. */
static void clear_vbar_hash_insert(CLEAR_CONTEXT* WINPR_RESTRICT clear, UINT32 index)
{
	const CLEAR_VBAR_ENTRY* entry = &clear->VBarStorage[index];
	const size_t len = 1ull * entry->count * 3;
	const UINT64 hash = clear_vbar_content_hash(entry->pixels, len);
	UINT32 i = WINPR_ASSERTING_INT_CAST(UINT32, hash & (CLEAR_VBAR_HASH_SIZE - 1));

	for (UINT32 probe = 0; probe < CLEAR_VBAR_HASH_SIZE; probe++)
	{
		const UINT32 slot = clear->VBarHashSlots[i];

		if ((slot == CLEAR_VBAR_HASH_EMPTY) || (slot == CLEAR_VBAR_HASH_DELETED))
		{
			clear->VBarHashSlots[i] = index + 1;
			return;
		}

		i = (i + 1) & (CLEAR_VBAR_HASH_SIZE - 1);
	}
}

/* Emit a VBAR_CACHE_HIT: the decoder already has this column at clear->VBarStorage[index]. */
static BOOL clear_compress_write_vbar_hit(wStream* WINPR_RESTRICT s, UINT32 index)
{
	if (!Stream_EnsureRemainingCapacity(s, 2))
		return FALSE;

	Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, 0x8000u | index));
	return TRUE;
}

/* Emit a SHORT_VBAR_CACHE_MISS (header + raw BGR pixels): a column never seen before. Also
 * mirrors clear_decompress_bands_data()'s cache update so the decoder's and encoder's V-Bar
 * caches stay in lockstep, keeping future VBAR_CACHE_HIT indices valid. shortVBarYOn is always
 * 0 here since this encoder never trims a column against the band's background color. */
static BOOL clear_compress_write_vbar_miss(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                           wStream* WINPR_RESTRICT s,
                                           const BYTE* WINPR_RESTRICT columnBgr, UINT32 count)
{
	if (!Stream_EnsureRemainingCapacity(s, 2ull + 1ull * count * 3))
		return FALSE;

	Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, count << 8)); /* yOn=0, yOff=count */
	Stream_Write(s, columnBgr, 1ull * count * 3);

	const UINT32 index = clear->VBarStorageCursor;
	CLEAR_VBAR_ENTRY* entry = &clear->VBarStorage[index];

	clear_vbar_hash_remove(clear, index);
	entry->count = count;

	if (!resize_vbar_entry(clear, entry, 3))
		return FALSE;

	CopyMemory(entry->pixels, columnBgr, 1ull * count * 3);
	clear_vbar_hash_insert(clear, index);

	clear->VBarStorageCursor = (clear->VBarStorageCursor + 1) % CLEARCODEC_VBAR_SIZE;
	return TRUE;
}

/* Encode CLEARCODEC_BANDS_DATA: fixed-height (<=52 row), full-width bands, one V-Bar per
 * column. Every column is cache-checked against clear->VBarStorage; unseen columns are sent
 * raw and added to the cache for later frames to reference. Mutates clear's V-Bar cache state,
 * so unlike the residual layer this cannot be spuriously discarded once written: whatever this
 * writes to the wire must reach the decoder, or the two caches fall out of sync. */
static BOOL clear_compress_bands_data(CLEAR_CONTEXT* WINPR_RESTRICT clear,
                                      wStream* WINPR_RESTRICT s,
                                      const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
                                      UINT32 nSrcStep, UINT32 nWidth, UINT32 nHeight)
{
	const UINT32 bpp = FreeRDPGetBytesPerPixel(SrcFormat);
	BYTE columnBgr[1ull * CLEAR_BAND_MAX_HEIGHT * 3];

	for (UINT32 yStart = 0; yStart < nHeight; yStart += CLEAR_BAND_MAX_HEIGHT)
	{
		const UINT32 bandHeight = MIN(CLEAR_BAND_MAX_HEIGHT, nHeight - yStart);
		const UINT32 yEnd = yStart + bandHeight - 1;

		if (!Stream_EnsureRemainingCapacity(s, 11))
			return FALSE;

		Stream_Write_UINT16(s, 0);                                            /* xStart */
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, nWidth - 1)); /* xEnd */
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, yStart));
		Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(UINT16, yEnd));
		Stream_Write_UINT8(s, 0); /* blueBkg: unused, columns are never background-trimmed */
		Stream_Write_UINT8(s, 0); /* greenBkg */
		Stream_Write_UINT8(s, 0); /* redBkg */

		for (UINT32 x = 0; x < nWidth; x++)
		{
			for (UINT32 y = 0; y < bandHeight; y++)
			{
				BYTE r = 0;
				BYTE g = 0;
				BYTE b = 0;
				BYTE a = 0;
				const BYTE* pPixel = &pSrcData[(1ull * (yStart + y)) * nSrcStep + 1ull * x * bpp];
				const UINT32 color = FreeRDPReadColor(pPixel, SrcFormat);
				FreeRDPSplitColor(color, SrcFormat, &r, &g, &b, &a, nullptr);
				columnBgr[y * 3 + 0] = b;
				columnBgr[y * 3 + 1] = g;
				columnBgr[y * 3 + 2] = r;
			}

			const UINT32 hit = clear_vbar_hash_lookup(clear, columnBgr, bandHeight);

			if (hit != UINT32_MAX)
			{
				if (!clear_compress_write_vbar_hit(s, hit))
					return FALSE;
			}
			else if (!clear_compress_write_vbar_miss(clear, s, columnBgr, bandHeight))
				return FALSE;
		}
	}

	return TRUE;
}

BOOL clear_compress(CLEAR_CONTEXT* WINPR_RESTRICT clear, const BYTE* WINPR_RESTRICT pSrcData,
                    UINT32 SrcFormat, UINT32 nSrcStep, UINT32 nWidth, UINT32 nHeight,
                    BYTE** WINPR_RESTRICT ppDstData, UINT32* WINPR_RESTRICT pDstSize)
{
	if (!clear || !clear->Compressor || !clear->bs)
		return FALSE;

	if (!pSrcData || !ppDstData || !pDstSize)
		return FALSE;

	if ((nWidth == 0) || (nHeight == 0) || (nWidth > 0xFFFF) || (nHeight > 0xFFFF))
		return FALSE;

	if (nSrcStep == 0)
		nSrcStep = nWidth * FreeRDPGetBytesPerPixel(SrcFormat);

	wStream* s = clear->bs;
	if (!Stream_SetPosition(s, 0))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 14))
		return FALSE;

	Stream_Write_UINT8(s, 0); /* glyphFlags: no glyph cache use */
	Stream_Write_UINT8(s, WINPR_ASSERTING_INT_CAST(UINT8, clear->seqNumber));

	const size_t residualByteCountPos = Stream_GetPosition(s);
	Stream_Write_UINT32(s, 0); /* residualByteCount, backpatched below */
	const size_t bandsByteCountPos = Stream_GetPosition(s);
	Stream_Write_UINT32(s, 0); /* bandsByteCount, backpatched below */
	Stream_Write_UINT32(s, 0); /* subcodecByteCount: subcodec layer not implemented yet */

	const size_t residualStart = Stream_GetPosition(s);

	if (!clear_compress_residual_data(clear, s, pSrcData, SrcFormat, nSrcStep, nWidth, nHeight))
		return FALSE;

	UINT32 residualByteCount =
	    WINPR_ASSERTING_INT_CAST(UINT32, Stream_GetPosition(s) - residualStart);

	/* Bands and residual both cover 100% of the rectangle on their own (this encoder never
	 * trims a band down to a partial region), so there is no benefit to sending both - it would
	 * just mean the decoder redundantly redraws every pixel twice. Pick one.
	 *
	 * The band layer costs at least 11 header bytes per band plus 2 bytes per column (the
	 * cheapest possible outcome, a VBAR_CACHE_HIT for every single column). If the residual
	 * layer already beats that floor, bands cannot possibly shrink the output further, so skip
	 * them entirely. Once that gate passes we commit to bands unconditionally rather than
	 * comparing actual sizes afterwards: clear_compress_bands_data() mutates the V-Bar cache as
	 * it goes (new columns are inserted so later frames can reference them), and undoing that
	 * after the fact to fall back to residual would desync the encoder's cache from the
	 * decoder's, which never rolls back what it has already decoded. */
	UINT32 bandsByteCount = 0;
	const UINT32 numBands = (nHeight + CLEAR_BAND_MAX_HEIGHT - 1) / CLEAR_BAND_MAX_HEIGHT;
	const UINT64 minBandsCost = (1ull * numBands * 11ull) + (2ull * numBands * nWidth);

	if (clear->VBarHashSlots && (residualByteCount > minBandsCost))
	{
		if (!Stream_SetPosition(s, residualStart))
			return FALSE;

		if (!clear_compress_bands_data(clear, s, pSrcData, SrcFormat, nSrcStep, nWidth, nHeight))
			return FALSE;

		bandsByteCount = WINPR_ASSERTING_INT_CAST(UINT32, Stream_GetPosition(s) - residualStart);
		residualByteCount = 0;
	}

	winpr_Data_Write_UINT32(Stream_Buffer(s) + residualByteCountPos, residualByteCount);
	winpr_Data_Write_UINT32(Stream_Buffer(s) + bandsByteCountPos, bandsByteCount);

	clear->seqNumber = (clear->seqNumber + 1) % 256;

	*ppDstData = Stream_Buffer(s);
	*pDstSize = WINPR_ASSERTING_INT_CAST(UINT32, Stream_GetPosition(s));
	return TRUE;
}

BOOL clear_context_reset(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	if (!clear)
		return FALSE;

	/**
	 * The ClearCodec context is not bound to a particular surface,
	 * and its internal caches must NOT be reset on the ResetGraphics PDU.
	 */
	clear->seqNumber = 0;
	return TRUE;
}

CLEAR_CONTEXT* clear_context_new(BOOL Compressor)
{
	CLEAR_CONTEXT* clear = (CLEAR_CONTEXT*)winpr_aligned_calloc(1, sizeof(CLEAR_CONTEXT), 32);

	if (!clear)
		return nullptr;
	clear->log = WLog_Get(TAG);
	clear->Compressor = Compressor;
	clear->nsc = nsc_context_new();

	if (!clear->nsc)
		goto error_nsc;

	if (!updateContextFormat(clear, PIXEL_FORMAT_BGRX32, TRUE))
		goto error_nsc;

	if (!clear_resize_buffer(clear, 512, 512))
		goto error_nsc;

	if (!clear->TempBuffer)
		goto error_nsc;

	if (Compressor)
	{
		clear->bs = Stream_New(nullptr, 1024);
		if (!clear->bs)
			goto error_nsc;

		clear->VBarHashSlots =
		    (UINT32*)winpr_aligned_calloc(CLEAR_VBAR_HASH_SIZE, sizeof(UINT32), 32);
		if (!clear->VBarHashSlots)
			goto error_nsc;
	}

	if (!clear_context_reset(clear))
		goto error_nsc;

	return clear;
error_nsc:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	clear_context_free(clear);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void clear_context_free(CLEAR_CONTEXT* WINPR_RESTRICT clear)
{
	if (!clear)
		return;

	nsc_context_free(clear->nsc);
	winpr_aligned_free(clear->TempBuffer);
	Stream_Free(clear->bs, TRUE);

	clear_reset_vbar_storage(clear, TRUE);
	clear_reset_glyph_cache(clear);
	winpr_aligned_free(clear->VBarHashSlots);

	winpr_aligned_free(clear);
}
