/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * libFuzzer harness for the ClearCodec compressor
 *
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/clear.h>

/* Each input is a few frames compressed by one encoder and decompressed by one decoder, so the
 * V-Bar caches of both sides must stay in sync from frame to frame.
 *
 * Input layout:
 *   [0..1] width - 1 (9 bits)
 *   [2]    height - 1
 *   [3]    source pixel format index
 *   [4]    bits 0-1: number of frames - 1, bits 2-4: extra bytes per source line,
 *          bit 5: pass a 0 stride (only without extra bytes)
 *   [5..]  pixel data, repeated to fill the image, shifted by one byte per frame
 */

#define FUZZ_CLEAR_MAX_PIXELS 65536u /* 256x256 */

/* NSCodec subcodec: color loss level 3 */
#define FUZZ_CLEAR_NSC_TOLERANCE 16

static const UINT32 FUZZ_CLEAR_FORMATS[] = { PIXEL_FORMAT_BGRX32, PIXEL_FORMAT_BGRA32,
	                                         PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_ARGB32,
	                                         PIXEL_FORMAT_RGBX32, PIXEL_FORMAT_RGBA32,
	                                         PIXEL_FORMAT_BGR24,  PIXEL_FORMAT_RGB24 };

static UINT32 fuzz_clear_subcodec_size(const BYTE* pCompressed, UINT32 compressedSize)
{
	if (compressedSize < 14)
		return 0;
	return (UINT32)pCompressed[10] | ((UINT32)pCompressed[11] << 8) |
	       ((UINT32)pCompressed[12] << 16) | ((UINT32)pCompressed[13] << 24);
}

WINPR_NORETURN(static void fuzz_clear_fail(const char* what, UINT32 frame, UINT32 width,
                                           UINT32 height, UINT32 format));

static void fuzz_clear_fail(const char* what, UINT32 frame, UINT32 width, UINT32 height,
                            UINT32 format)
{
	(void)fprintf(stderr, "clear compressor: %s, frame %" PRIu32 " %" PRIu32 "x%" PRIu32 " %s\n",
	              what, frame, width, height, FreeRDPGetColorFormatName(format));
	abort();
}

static void fuzz_clear_check(const BYTE* pSrc, size_t srcSize, UINT32 SrcFormat, UINT32 nSrcStep,
                             const BYTE* pDst, size_t dstSize, UINT32 nDstStep, UINT32 width,
                             UINT32 height, UINT32 tolerance, UINT32 frame)
{
	const UINT32 srcBpp = FreeRDPGetBytesPerPixel(SrcFormat);

	for (UINT32 y = 0; y < height; y++)
	{
		for (UINT32 x = 0; x < width; x++)
		{
			BYTE src[3] = WINPR_C_ARRAY_INIT;
			BYTE dst[3] = WINPR_C_ARRAY_INIT;

			const size_t srcOffset = 1ull * y * nSrcStep + 1ull * x * srcBpp;
			const size_t dstOffset = 1ull * y * nDstStep + 4ull * x;
			if ((srcOffset >= srcSize) || (srcSize - srcOffset < srcBpp) ||
			    (dstOffset >= dstSize) || (dstSize - dstOffset < 4))
				fuzz_clear_fail("pixel outside of the image", frame, width, height, SrcFormat);

			const UINT32 srcColor = FreeRDPReadColor(&pSrc[srcOffset], SrcFormat);
			FreeRDPSplitColor(srcColor, SrcFormat, &src[0], &src[1], &src[2], nullptr, nullptr);

			const UINT32 dstColor = FreeRDPReadColor(&pDst[dstOffset], PIXEL_FORMAT_BGRX32);
			FreeRDPSplitColor(dstColor, PIXEL_FORMAT_BGRX32, &dst[0], &dst[1], &dst[2], nullptr,
			                  nullptr);

			for (size_t c = 0; c < ARRAYSIZE(src); c++)
			{
				const int diff = (int)src[c] - (int)dst[c];
				if ((UINT32)abs(diff) > tolerance)
					fuzz_clear_fail("decoded image differs", frame, width, height, SrcFormat);
			}
		}
	}
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static BOOL loggingInitialized = FALSE;

	if (!loggingInitialized)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		loggingInitialized = TRUE;
	}

	if (size < 6)
		return 0;

	const UINT32 width = 1u + (data[0] | ((data[1] & 0x01u) << 8));
	UINT32 height = 1u + data[2];
	if (1ull * width * height > FUZZ_CLEAR_MAX_PIXELS)
		height = FUZZ_CLEAR_MAX_PIXELS / width;

	const UINT32 SrcFormat = FUZZ_CLEAR_FORMATS[data[3] % ARRAYSIZE(FUZZ_CLEAR_FORMATS)];
	const UINT32 frames = 1u + (data[4] & 0x03u);
	const UINT32 extra = (data[4] >> 2) & 0x07u;
	const BOOL zeroStride = (extra == 0) && ((data[4] & 0x20u) != 0);
	const uint8_t* pixels = &data[5];
	const size_t pixelsSize = size - 5;

	const UINT32 nSrcStep = width * FreeRDPGetBytesPerPixel(SrcFormat) + extra;
	const UINT32 nDstStep = width * 4;
	const size_t srcSize = 1ull * nSrcStep * height;
	const size_t dstSize = 1ull * nDstStep * height;
	BYTE* pSrc = malloc(srcSize);
	BYTE* pDst = calloc(dstSize, 1);
	CLEAR_CONTEXT* encoder = clear_context_new(TRUE);
	CLEAR_CONTEXT* decoder = clear_context_new(FALSE);

	if (!pSrc || !pDst || !encoder || !decoder)
		goto fail;

	for (UINT32 frame = 0; frame < frames; frame++)
	{
		for (size_t i = 0; i < srcSize; i++)
			pSrc[i] = pixels[(i + frame) % pixelsSize];

		BYTE* pCompressed = nullptr;
		UINT32 compressedSize = 0;
		if (!clear_compress(encoder, pSrc, SrcFormat, zeroStride ? 0 : nSrcStep, width, height,
		                    &pCompressed, &compressedSize))
			fuzz_clear_fail("clear_compress failed", frame, width, height, SrcFormat);

		if (clear_decompress(decoder, pCompressed, compressedSize, width, height, pDst,
		                     PIXEL_FORMAT_BGRX32, nDstStep, 0, 0, width, height, nullptr) != 0)
			fuzz_clear_fail("clear_decompress failed", frame, width, height, SrcFormat);

		const UINT32 tolerance = (fuzz_clear_subcodec_size(pCompressed, compressedSize) > 0)
		                             ? FUZZ_CLEAR_NSC_TOLERANCE
		                             : 0;
		fuzz_clear_check(pSrc, srcSize, SrcFormat, nSrcStep, pDst, dstSize, nDstStep, width, height,
		                 tolerance, frame);
	}

fail:
	clear_context_free(encoder);
	clear_context_free(decoder);
	free(pSrc);
	free(pDst);
	return 0;
}
