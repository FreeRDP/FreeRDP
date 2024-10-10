/* test_copy.c
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <stdio.h>

#include <freerdp/config.h>
#include <winpr/crypto.h>

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define COPY_TESTSIZE (256 * 2 + 16 * 2 + 15 + 15)

/* ------------------------------------------------------------------------- */
static BOOL test_copy8u_func(void)
{
	primitives_t* prims = primitives_get();
	BYTE ALIGN(data[COPY_TESTSIZE + 15]) = { 0 };
	winpr_RAND(data, sizeof(data));

	for (int soff = 0; soff < 16; ++soff)
	{
		for (int doff = 0; doff < 16; ++doff)
		{
			for (int length = 1; length <= COPY_TESTSIZE - doff; ++length)
			{
				BYTE ALIGN(dest[COPY_TESTSIZE + 15]) = { 0 };

				if (prims->copy_8u(data + soff, dest + doff, length) != PRIMITIVES_SUCCESS)
					return FALSE;

				for (int i = 0; i < length; ++i)
				{
					if (dest[i + doff] != data[i + soff])
					{
						printf("COPY8U FAIL: off=%d len=%d, dest[%d]=0x%02" PRIx8 ""
						       "data[%d]=0x%02" PRIx8 "\n",
						       doff, length, i + doff, dest[i + doff], i + soff, data[i + soff]);
						return FALSE;
					}
				}
			}
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_copy8u_speed(void)
{
	BYTE ALIGN(src[MAX_TEST_SIZE + 4]);
	BYTE ALIGN(dst[MAX_TEST_SIZE + 4]);

	if (!speed_test("copy_8u", "aligned", g_Iterations, (speed_test_fkt)generic->copy_8u,
	                (speed_test_fkt)optimized->copy_8u, src, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("copy_8u", "unaligned", g_Iterations, (speed_test_fkt)generic->copy_8u,
	                (speed_test_fkt)optimized->copy_8u, src + 1, dst + 1, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

static BYTE* rand_alloc(size_t w, size_t h, size_t bpp, size_t pad, BYTE** copy)
{
	const size_t s = w * bpp + pad;
	BYTE* ptr = calloc(s, h);
	if (!ptr)
		return NULL;

	winpr_RAND(ptr, s * h);

	if (copy)
	{
		BYTE* ptr2 = calloc(s, h);
		if (!ptr2)
		{
			free(ptr);
			return NULL;
		}
		memcpy(ptr2, ptr, s * h);
		*copy = ptr2;
	}
	return ptr;
}

static size_t runcount = 0;

static BOOL test_copy_no_overlap_off(BOOL verbose, UINT32 srcFormat, UINT32 dstFormat, UINT32 flags,
                                     UINT32 pad, UINT32 w, UINT32 h, UINT32 dxoff, UINT32 dyoff,
                                     UINT32 sxoff, UINT32 syoff)
{
	BOOL rc = FALSE;
	primitives_t* gen = primitives_get_generic();
	primitives_t* prims = primitives_get();
	if (!gen || !prims)
		return FALSE;

	runcount++;

	WINPR_ASSERT(dxoff < w);
	WINPR_ASSERT(sxoff < w);
	WINPR_ASSERT(dyoff < h);
	WINPR_ASSERT(syoff < h);

	const UINT32 sbpp = FreeRDPGetBytesPerPixel(srcFormat);
	const UINT32 dbpp = FreeRDPGetBytesPerPixel(dstFormat);

	if (verbose)
	{
		(void)fprintf(stderr,
		              "run src: %s, dst: %s [flags 0x%08" PRIx32 "] %" PRIu32 "x%" PRIu32
		              ", soff=%" PRIu32 "x%" PRIu32 ", doff=%" PRIu32 "x%" PRIu32 ", pad=%" PRIu32
		              "\n",
		              FreeRDPGetColorFormatName(srcFormat), FreeRDPGetColorFormatName(dstFormat),
		              flags, w, h, sxoff, syoff, dxoff, dyoff, pad);
	}

	const UINT32 sstride = (w + sxoff) * sbpp + pad;
	const UINT32 dstride = (w + dxoff) * dbpp + pad;
	BYTE* dst2 = NULL;
	BYTE* src2 = NULL;
	BYTE* dst1 = rand_alloc(w + dxoff, h + dyoff, dbpp, pad, &dst2);
	BYTE* src1 = rand_alloc(w + sxoff, h + syoff, sbpp, pad, &src2);
	if (!dst1 || !dst2 || !src1 || !src2)
		goto fail;

	if (gen->copy_no_overlap(dst1, dstFormat, dstride, dxoff, dyoff, w, h, src1, srcFormat, sstride,
	                         sxoff, syoff, NULL, flags) != PRIMITIVES_SUCCESS)
		goto fail;

	if (memcmp(src1, src2, 1ULL * sstride * h) != 0)
		goto fail;

	if (prims->copy_no_overlap(dst2, dstFormat, dstride, dxoff, dyoff, w, h, src1, srcFormat,
	                           sstride, sxoff, syoff, NULL, flags) != PRIMITIVES_SUCCESS)
		goto fail;

	if (memcmp(src1, src2, 1ULL * sstride * h) != 0)
		goto fail;

	if (memcmp(dst1, dst2, 1ULL * dstride * h) != 0)
		goto fail;

	if (flags == FREERDP_KEEP_DST_ALPHA)
	{
		for (size_t y = 0; y < h; y++)
		{
			const BYTE* d1 = &dst1[(y + dyoff) * dstride];
			const BYTE* d2 = &dst2[(y + dyoff) * dstride];
			for (size_t x = 0; x < w; x++)
			{
				const UINT32 c1 = FreeRDPReadColor(&d1[(x + dxoff) * dbpp], dstFormat);
				const UINT32 c2 = FreeRDPReadColor(&d2[(x + dxoff) * dbpp], dstFormat);
				BYTE a1 = 0;
				BYTE a2 = 0;
				FreeRDPSplitColor(c1, dstFormat, NULL, NULL, NULL, &a1, NULL);
				FreeRDPSplitColor(c2, dstFormat, NULL, NULL, NULL, &a2, NULL);
				if (a1 != a2)
					goto fail;
			}
		}
	}
	rc = TRUE;

fail:
	if (!rc)
	{
		(void)fprintf(stderr, "failed to compare copy_no_overlap(%s -> %s [0x%08" PRIx32 "])\n",
		              FreeRDPGetColorFormatName(srcFormat), FreeRDPGetColorFormatName(dstFormat),
		              flags);
	}
	free(dst1);
	free(dst2);
	free(src1);
	free(src2);
	return rc;
}

static BOOL test_copy_no_overlap(BOOL verbose, UINT32 srcFormat, UINT32 dstFormat, UINT32 flags,
                                 UINT32 width, UINT32 height)
{
	BOOL rc = TRUE;
	const UINT32 mw = 4;
	const UINT32 mh = 4;
	for (UINT32 dxoff = 0; dxoff < mw; dxoff++)
	{
		for (UINT32 dyoff = 0; dyoff <= mh; dyoff++)
		{
			for (UINT32 sxoff = 0; sxoff <= mw; sxoff++)
			{
				for (UINT32 syoff = 0; syoff <= mh; syoff++)
				{
					/* We need minimum alignment of 8 bytes.
					 * AVX2 can read 8 pixels (at most 8x4=32 bytes) per step
					 * if we have 24bpp input that is 24 bytes with 8 bytes read
					 * out of bound */
					for (UINT32 pad = 8; pad <= 12; pad++)
					{
						if (!test_copy_no_overlap_off(verbose, srcFormat, dstFormat, flags, pad,
						                              width, height, dxoff, dyoff, sxoff, syoff))
							rc = FALSE;
					}
				}
			}
		}
	}

	return rc;
}

int TestPrimitivesCopy(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	const BOOL verbose = argc > 1;

	prim_test_setup(FALSE);

	if (!test_copy8u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_copy8u_speed())
			return 1;
	}

	const UINT32 flags[] = {
		FREERDP_FLIP_NONE,
		FREERDP_KEEP_DST_ALPHA,
		FREERDP_FLIP_HORIZONTAL,
		FREERDP_KEEP_DST_ALPHA | FREERDP_FLIP_HORIZONTAL,
#if 0
	                         FREERDP_FLIP_VERTICAL,
	                                                      FREERDP_FLIP_VERTICAL | FREERDP_FLIP_HORIZONTAL,
	                                                      FREERDP_KEEP_DST_ALPHA | FREERDP_FLIP_VERTICAL,
	                         FREERDP_KEEP_DST_ALPHA | FREERDP_FLIP_VERTICAL | FREERDP_FLIP_HORIZONTAL
#endif
	};
	const UINT32 formats[] = {
		PIXEL_FORMAT_BGRA32,
		PIXEL_FORMAT_BGRX32,
		PIXEL_FORMAT_BGR24
#if 0 /* Only the previous 3 have SIMD optimizations, so skip the rest */
	                           ,  PIXEL_FORMAT_RGB24,
		                       PIXEL_FORMAT_ABGR32, PIXEL_FORMAT_ARGB32, PIXEL_FORMAT_XBGR32,
	                           PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_RGBA32, PIXEL_FORMAT_RGBX32
#endif
	};

	int rc = 0;
	for (size_t z = 0; z < ARRAYSIZE(flags); z++)
	{
		const UINT32 flag = flags[z];
		for (size_t x = 0; x < ARRAYSIZE(formats); x++)
		{
			const UINT32 sformat = formats[x];
			for (size_t y = 0; y < ARRAYSIZE(formats); y++)
			{
				const UINT32 dformat = formats[y];

				if (!test_copy_no_overlap(verbose, sformat, dformat, flag, 21, 17))
					rc = -1;
			}
		}
	}

	if (verbose)
		(void)fprintf(stderr, "runcount=%" PRIuz "\n", runcount);

	return rc;
}
