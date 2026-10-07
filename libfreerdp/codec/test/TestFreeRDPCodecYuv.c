
#include <string.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/wlog.h>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/yuv.h>

/* The threaded YUV encoder splits every region into slices of a fixed number of rows that are
 * converted by different workers. Every slice has to convert its own rows only and the last slice
 * (which can be shorter or longer than a full slice) has to reach the bottom of the region.
 *
 * Convert the same random image with and without worker threads and require identical output.
 */

#define TEST_TAG "TestFreeRDPCodecYuv"
#define TEST_WIDTH 320u
#define TEST_HEIGHT 200u
#define TEST_PLANES 6u

/* The H264 layer pads the planes (32 more columns and rows, see yuv_ensure_buffer) so that the
 * SIMD code can write whole vectors at the edges. Do the same here. */
#define TEST_STRIDE (TEST_WIDTH + 32u - TEST_WIDTH % 16u)
#define TEST_PHEIGHT (TEST_HEIGHT + 32u - TEST_HEIGHT % 16u)
#define TEST_PLANE_SIZE (1ull * TEST_STRIDE * TEST_PHEIGHT)

static void free_planes(BYTE* planes[TEST_PLANES])
{
	for (size_t x = 0; x < TEST_PLANES; x++)
	{
		free(planes[x]);
		planes[x] = nullptr;
	}
}

static BOOL alloc_planes(BYTE* planes[TEST_PLANES])
{
	for (size_t x = 0; x < TEST_PLANES; x++)
	{
		planes[x] = calloc(1, TEST_PLANE_SIZE);
		if (!planes[x])
			return FALSE;
	}
	return TRUE;
}

static BOOL encode(YUV_CONTEXT* context, BYTE version, const BYTE* src, UINT32 srcStep,
                   BYTE* planes[TEST_PLANES], const RECTANGLE_16* rect)
{
	const UINT32 stride[3] = { TEST_STRIDE, (TEST_STRIDE + 1) / 2, (TEST_STRIDE + 1) / 2 };
	BYTE* luma[3] = { planes[0], planes[1], planes[2] };
	BYTE* chroma[3] = { planes[3], planes[4], planes[5] };

	if (version == 0)
		return yuv420_context_encode(context, src, srcStep, PIXEL_FORMAT_BGRX32, stride, luma,
		                             rect, 1);
	return yuv444_context_encode(context, version, src, srcStep, PIXEL_FORMAT_BGRX32, stride, luma,
	                             chroma, rect, 1);
}

static BOOL compare_rect(YUV_CONTEXT* threaded, YUV_CONTEXT* single, BYTE version, const BYTE* src,
                         UINT32 srcStep, const RECTANGLE_16* rect)
{
	BOOL rc = FALSE;
	BYTE* planesA[TEST_PLANES] = WINPR_C_ARRAY_INIT;
	BYTE* planesB[TEST_PLANES] = WINPR_C_ARRAY_INIT;

	if (!alloc_planes(planesA) || !alloc_planes(planesB))
		goto fail;

	if (!encode(threaded, version, src, srcStep, planesA, rect) ||
	    !encode(single, version, src, srcStep, planesB, rect))
		goto fail;

	for (size_t x = 0; x < TEST_PLANES; x++)
	{
		if (memcmp(planesA[x], planesB[x], TEST_PLANE_SIZE) != 0)
		{
			(void)fprintf(stderr,
			              "[%s] version %d: plane %" PRIuz " differs for rect [%" PRIu16 ",%" PRIu16
			              "-%" PRIu16 ",%" PRIu16 "]\n",
			              TEST_TAG, version, x, rect->left, rect->top, rect->right, rect->bottom);
			goto fail;
		}
	}
	rc = TRUE;
fail:
	free_planes(planesA);
	free_planes(planesB);
	return rc;
}

int TestFreeRDPCodecYuv(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	int rc = -1;
	const UINT32 srcStep = TEST_WIDTH * 4u;
	BYTE* src = calloc(TEST_HEIGHT, srcStep);
	YUV_CONTEXT* threaded = yuv_context_new(TRUE, 0);
	YUV_CONTEXT* single = yuv_context_new(TRUE, THREADING_FLAGS_DISABLE_THREADS);

	/* region tops have to be even, the chroma planes are subsampled */
	const UINT16 tops[] = { 0, 2, 16, 34 };
	const UINT16 heights[] = { 1, 2, 3, 7, 8, 9, 15, 16, 17, 23, 24, 25, 31, 32, 33, 40, 100, 198 };
	const UINT16 lefts[] = { 0, 16 };
	const UINT16 rights[] = { TEST_WIDTH, TEST_WIDTH - 16 };
	size_t tested = 0;

	if (!src || !threaded || !single)
		goto fail;
	if (winpr_RAND_pseudo(src, 1ull * TEST_HEIGHT * srcStep) < 0)
		goto fail;
	if (!yuv_context_reset(threaded, TEST_WIDTH, TEST_HEIGHT) ||
	    !yuv_context_reset(single, TEST_WIDTH, TEST_HEIGHT))
		goto fail;

	/* 0 = YUV420, 1 and 2 = the two YUV444 variants used for AVC444 and AVC444v2 */
	for (BYTE version = 0; version < 3; version++)
	{
		for (size_t t = 0; t < ARRAYSIZE(tops); t++)
		{
			for (size_t h = 0; h < ARRAYSIZE(heights); h++)
			{
				for (size_t s = 0; s < ARRAYSIZE(lefts); s++)
				{
					RECTANGLE_16 rect = { 0 };
					rect.left = lefts[s];
					rect.right = rights[s];
					rect.top = tops[t];
					rect.bottom = (UINT16)(tops[t] + heights[h]);
					if (rect.bottom > TEST_HEIGHT)
						continue;

					if (!compare_rect(threaded, single, version, src, srcStep, &rect))
						goto fail;
					tested++;
				}
			}
		}
	}

	/* make sure the loops above did not silently skip everything */
	if (tested < 150)
	{
		(void)fprintf(stderr, "[%s] only %" PRIuz " regions tested\n", TEST_TAG, tested);
		goto fail;
	}
	rc = 0;
fail:
	yuv_context_free(threaded);
	yuv_context_free(single);
	free(src);
	return rc;
}
