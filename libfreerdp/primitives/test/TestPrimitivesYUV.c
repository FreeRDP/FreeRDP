
#include <freerdp/config.h>

#include <stdlib.h>
#include <math.h>

#include "prim_test.h"

#include <winpr/print.h>

#include <winpr/wlog.h>
#include <winpr/crypto.h>
#include <freerdp/primitives.h>
#include <freerdp/utils/profiler.h>

#include "../prim_internal.h"

#define TAG __FILE__

#define PADDING_FILL_VALUE 0x37

/* YUV to RGB conversion is lossy, so consider every value only
 * differing by less than 2 abs equal. */
static BOOL similar(const BYTE* src, const BYTE* dst, size_t size)
{
	for (size_t x = 0; x < size; x++)
	{
		int diff = src[x] - dst[x];

		if (abs(diff) > 4)
		{
			(void)fprintf(stderr, "%" PRIuz " %02" PRIX8 " : %02" PRIX8 " diff=%d\n", x, src[x],
			              dst[x], abs(diff));
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL similarRGB(size_t y, const BYTE* src, const BYTE* dst, size_t size, UINT32 format,
                       BOOL use444)
{
	BOOL rc = TRUE;
	/* Skip YUV420, too lossy */
	if (!use444)
		return TRUE;

	const UINT32 bpp = FreeRDPGetBytesPerPixel(format);
	BYTE fill = PADDING_FILL_VALUE;
	if (!FreeRDPColorHasAlpha(format))
		fill = 0xFF;

	for (size_t x = 0; x < size; x++)
	{
		const LONG maxDiff = 4;
		UINT32 sColor = 0;
		UINT32 dColor = 0;
		BYTE sR = 0;
		BYTE sG = 0;
		BYTE sB = 0;
		BYTE sA = 0;
		BYTE dR = 0;
		BYTE dG = 0;
		BYTE dB = 0;
		BYTE dA = 0;
		sColor = FreeRDPReadColor(src, format);
		dColor = FreeRDPReadColor(dst, format);
		src += bpp;
		dst += bpp;
		FreeRDPSplitColor(sColor, format, &sR, &sG, &sB, &sA, NULL);
		FreeRDPSplitColor(dColor, format, &dR, &dG, &dB, &dA, NULL);

		const long diffr = labs(1L * sR - dR);
		const long diffg = labs(1L * sG - dG);
		const long diffb = labs(1L * sB - dB);
		if ((diffr > maxDiff) || (diffg > maxDiff) || (diffb > maxDiff))
		{
			/* AVC444 uses an averaging filter for luma pixel U/V and reverses it in YUV444 -> RGB
			 * this is lossy and does not handle all combinations well so the 2x,2y pixel can be
			 * quite different after RGB -> YUV444 -> RGB conversion.
			 *
			 * skip these pixels to avoid failing the test
			 */
			if (use444 && ((x % 2) == 0) && ((y % 2) == 0))
			{
				continue;
			}

			const BYTE sY = RGB2Y(sR, sG, sB);
			const BYTE sU = RGB2U(sR, sG, sB);
			const BYTE sV = RGB2V(sR, sG, sB);
			const BYTE dY = RGB2Y(dR, dG, dB);
			const BYTE dU = RGB2U(dR, dG, dB);
			const BYTE dV = RGB2V(dR, dG, dB);
			(void)fprintf(stderr,
			              "[%s] Color value  mismatch R[%02X %02X], G[%02X %02X], B[%02X %02X] at "
			              "position %" PRIuz "x%" PRIuz "\n",
			              use444 ? "AVC444" : "AVC420", sR, dR, sG, dG, sA, dA, x, y);
			(void)fprintf(stderr,
			              "[%s] Color value  mismatch Y[%02X %02X], U[%02X %02X], V[%02X %02X] at "
			              "position %" PRIuz "x%" PRIuz "\n",
			              use444 ? "AVC444" : "AVC420", sY, dY, sU, dU, sV, dV, x, y);
			rc = FALSE;
			continue;
		}

		if (dA != fill)
		{
			(void)fprintf(
			    stderr,
			    "[%s] Invalid destination alpha value 0x%02X [expected 0x%02X] at position %" PRIuz
			    "x%" PRIuz "\n",
			    use444 ? "AVC444" : "AVC420", dA, fill, x, y);
			rc = FALSE;
			continue;
		}
	}

	return rc;
}

static void get_size(BOOL large, UINT32* width, UINT32* height)
{
	UINT32 shift = large ? 8 : 1;
	winpr_RAND(width, sizeof(*width));
	winpr_RAND(height, sizeof(*height));
	*width = (*width % 64 + 1) << shift;
	*height = (*height % 64 + 1);
}

static BOOL check_padding(const BYTE* psrc, size_t size, size_t padding, const char* buffer)
{
	BOOL rc = TRUE;
	const BYTE* src = NULL;
	const BYTE* esrc = NULL;
	size_t halfPad = (padding + 1) / 2;

	if (!psrc)
		return FALSE;

	src = psrc - halfPad;
	esrc = src + size + halfPad;

	for (size_t x = 0; x < halfPad; x++)
	{
		const BYTE s = *src++;
		const BYTE d = *esrc++;

		if (s != 'A')
		{
			size_t start = x;

			while ((x < halfPad) && (*esrc++ != 'A'))
				x++;

			(void)fprintf(stderr,
			              "Buffer underflow detected %02" PRIx8 " != %02X %s [%" PRIuz "-%" PRIuz
			              "]\n",
			              d, 'A', buffer, start, x);
			return FALSE;
		}

		if (d != 'A')
		{
			size_t start = x;

			while ((x < halfPad) && (*esrc++ != 'A'))
				x++;

			(void)fprintf(stderr,
			              "Buffer overflow detected %02" PRIx8 " != %02X %s [%" PRIuz "-%" PRIuz
			              "]\n",
			              d, 'A', buffer, start, x);
			return FALSE;
		}
	}

	return rc;
}

static void* set_padding(size_t size, size_t padding)
{
	size_t halfPad = (padding + 1) / 2;
	BYTE* psrc = NULL;
	BYTE* src = winpr_aligned_malloc(size + 2 * halfPad, 16);

	if (!src)
		return NULL;

	memset(&src[0], 'A', halfPad);
	memset(&src[halfPad], PADDING_FILL_VALUE, size);
	memset(&src[halfPad + size], 'A', halfPad);
	psrc = &src[halfPad];

	if (!check_padding(psrc, size, padding, "init"))
	{
		winpr_aligned_free(src);
		return NULL;
	}

	return psrc;
}

static void free_padding(void* src, size_t padding)
{
	BYTE* ptr = NULL;

	if (!src)
		return;

	ptr = ((BYTE*)src) - (padding + 1) / 2;
	winpr_aligned_free(ptr);
}

/* Create 2 pseudo YUV420 frames of same size.
 * Combine them and check, if the data is at the expected position. */
static BOOL TestPrimitiveYUVCombine(primitives_t* prims, prim_size_t roi)
{
	union
	{
		const BYTE** cpv;
		BYTE** pv;
	} cnv;
	size_t awidth = 0;
	size_t aheight = 0;
	BOOL rc = FALSE;
	BYTE* luma[3] = { 0 };
	BYTE* chroma[3] = { 0 };
	BYTE* yuv[3] = { 0 };
	BYTE* pmain[3] = { 0 };
	BYTE* paux[3] = { 0 };
	UINT32 lumaStride[3] = { 0 };
	UINT32 chromaStride[3] = { 0 };
	UINT32 yuvStride[3] = { 0 };
	const size_t padding = 10000;
	RECTANGLE_16 rect = { 0 };
	PROFILER_DEFINE(yuvCombine)
	PROFILER_DEFINE(yuvSplit)

	// TODO: we only support even height values at the moment
	if (roi.height % 2)
		roi.height++;

	awidth = roi.width + 16 - roi.width % 16;
	aheight = roi.height + 16 - roi.height % 16;
	(void)fprintf(stderr,
	              "Running YUVCombine on frame size %" PRIu32 "x%" PRIu32 " [%" PRIuz "x%" PRIuz
	              "]\n",
	              roi.width, roi.height, awidth, aheight);
	PROFILER_CREATE(yuvCombine, "YUV420CombineToYUV444")
	PROFILER_CREATE(yuvSplit, "YUV444SplitToYUV420")
	rect.left = 0;
	rect.top = 0;
	rect.right = roi.width;
	rect.bottom = roi.height;

	if (!prims || !prims->YUV420CombineToYUV444)
		goto fail;

	for (UINT32 x = 0; x < 3; x++)
	{
		size_t halfStride = ((x > 0) ? awidth / 2 : awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x > 0) ? halfStride * aheight / 2 : awidth * aheight);
		yuvStride[x] = awidth;

		if (!(yuv[x] = set_padding(size, padding)))
			goto fail;

		lumaStride[x] = halfStride;

		if (!(luma[x] = set_padding(halfSize, padding)))
			goto fail;

		if (!(pmain[x] = set_padding(halfSize, padding)))
			goto fail;

		chromaStride[x] = halfStride;

		if (!(chroma[x] = set_padding(halfSize, padding)))
			goto fail;

		if (!(paux[x] = set_padding(halfSize, padding)))
			goto fail;

		memset(luma[x], WINPR_ASSERTING_INT_CAST(int, 0xAB + 3 * x), halfSize);
		memset(chroma[x], WINPR_ASSERTING_INT_CAST(int, 0x80 + 2 * x), halfSize);

		if (!check_padding(luma[x], halfSize, padding, "luma"))
			goto fail;

		if (!check_padding(chroma[x], halfSize, padding, "chroma"))
			goto fail;

		if (!check_padding(pmain[x], halfSize, padding, "main"))
			goto fail;

		if (!check_padding(paux[x], halfSize, padding, "aux"))
			goto fail;

		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	PROFILER_ENTER(yuvCombine)

	cnv.pv = luma;
	if (prims->YUV420CombineToYUV444(AVC444_LUMA, cnv.cpv, lumaStride, roi.width, roi.height, yuv,
	                                 yuvStride, &rect) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvCombine)
		goto fail;
	}

	cnv.pv = chroma;
	if (prims->YUV420CombineToYUV444(AVC444_CHROMAv1, cnv.cpv, chromaStride, roi.width, roi.height,
	                                 yuv, yuvStride, &rect) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvCombine)
		goto fail;
	}

	PROFILER_EXIT(yuvCombine)

	for (size_t x = 0; x < 3; x++)
	{
		size_t halfStride = ((x > 0) ? awidth / 2 : awidth);
		size_t size = 1ULL * aheight * awidth;
		size_t halfSize = ((x > 0) ? halfStride * aheight / 2 : awidth * aheight);

		if (!check_padding(luma[x], halfSize, padding, "luma"))
			goto fail;

		if (!check_padding(chroma[x], halfSize, padding, "chroma"))
			goto fail;

		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	PROFILER_ENTER(yuvSplit)

	cnv.pv = yuv;
	if (prims->YUV444SplitToYUV420(cnv.cpv, yuvStride, pmain, lumaStride, paux, chromaStride,
	                               &roi) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvSplit)
		goto fail;
	}

	PROFILER_EXIT(yuvSplit)

	for (UINT32 x = 0; x < 3; x++)
	{
		size_t halfStride = ((x > 0) ? awidth / 2 : awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x > 0) ? halfStride * aheight / 2 : awidth * aheight);

		if (!check_padding(pmain[x], halfSize, padding, "main"))
			goto fail;

		if (!check_padding(paux[x], halfSize, padding, "aux"))
			goto fail;

		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	for (size_t i = 0; i < 3; i++)
	{
		for (size_t y = 0; y < roi.height; y++)
		{
			UINT32 w = roi.width;
			UINT32 lstride = lumaStride[i];
			UINT32 cstride = chromaStride[i];

			if (i > 0)
			{
				w = (roi.width + 3) / 4;

				if (roi.height > (roi.height + 1) / 2)
					continue;
			}

			if (!similar(luma[i] + y * lstride, pmain[i] + y * lstride, w))
				goto fail;

			/* Need to ignore lines of destination Y plane,
			 * if the lines are not a multiple of 16
			 * as the UV planes are packed in 8 line stripes. */
			if (i == 0)
			{
				/* TODO: This check is not perfect, it does not
				 * include the last V lines packed to the Y
				 * frame. */
				UINT32 rem = roi.height % 16;

				if (y > roi.height - rem)
					continue;
			}

			if (!similar(chroma[i] + y * cstride, paux[i] + y * cstride, w))
				goto fail;
		}
	}

	PROFILER_PRINT_HEADER
	PROFILER_PRINT(yuvSplit)
	PROFILER_PRINT(yuvCombine)
	PROFILER_PRINT_FOOTER
	rc = TRUE;
fail:
	printf("[%s] run %s.\n", __func__, (rc) ? "SUCCESS" : "FAILED");
	PROFILER_FREE(yuvCombine)
	PROFILER_FREE(yuvSplit)

	for (UINT32 x = 0; x < 3; x++)
	{
		free_padding(yuv[x], padding);
		free_padding(luma[x], padding);
		free_padding(chroma[x], padding);
		free_padding(pmain[x], padding);
		free_padding(paux[x], padding);
	}

	return rc;
}

static BOOL TestPrimitiveYUV(primitives_t* prims, prim_size_t roi, BOOL use444)
{
	union
	{
		const BYTE** cpv;
		BYTE** pv;
	} cnv;
	BOOL res = FALSE;
	UINT32 awidth = 0;
	UINT32 aheight = 0;
	BYTE* yuv[3] = { 0 };
	UINT32 yuv_step[3] = { 0 };
	BYTE* rgb = NULL;
	BYTE* rgb_dst = NULL;
	size_t size = 0;
	size_t uvsize = 0;
	size_t uvwidth = 0;
	size_t padding = 100ULL * 16ULL;
	UINT32 stride = 0;
	const UINT32 formats[] = { PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_XBGR32, PIXEL_FORMAT_ARGB32,
		                       PIXEL_FORMAT_ABGR32, PIXEL_FORMAT_RGBA32, PIXEL_FORMAT_RGBX32,
		                       PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_BGRX32 };
	PROFILER_DEFINE(rgbToYUV420)
	PROFILER_DEFINE(rgbToYUV444)
	PROFILER_DEFINE(yuv420ToRGB)
	PROFILER_DEFINE(yuv444ToRGB)
	/* Buffers need to be 16x16 aligned. */
	awidth = roi.width + 16 - roi.width % 16;
	aheight = roi.height + 16 - roi.height % 16;
	stride = 1ULL * awidth * sizeof(UINT32);
	size = 1ULL * awidth * aheight;

	if (use444)
	{
		uvwidth = awidth;
		uvsize = size;

		if (!prims || !prims->RGBToYUV444_8u_P3AC4R || !prims->YUV444ToRGB_8u_P3AC4R)
			return FALSE;
	}
	else
	{
		uvwidth = (awidth + 1) / 2;
		uvsize = (aheight + 1) / 2 * uvwidth;

		if (!prims || !prims->RGBToYUV420_8u_P3AC4R || !prims->YUV420ToRGB_8u_P3AC4R)
			return FALSE;
	}

	(void)fprintf(stderr, "Running AVC%s on frame size %" PRIu32 "x%" PRIu32 "\n",
	              use444 ? "444" : "420", roi.width, roi.height);

	/* Test RGB to YUV444 conversion and vice versa */
	if (!(rgb = set_padding(size * sizeof(UINT32), padding)))
		goto fail;

	if (!(rgb_dst = set_padding(size * sizeof(UINT32), padding)))
		goto fail;

	if (!(yuv[0] = set_padding(size, padding)))
		goto fail;

	if (!(yuv[1] = set_padding(uvsize, padding)))
		goto fail;

	if (!(yuv[2] = set_padding(uvsize, padding)))
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		BYTE* line = &rgb[y * stride];
		winpr_RAND(line, stride);
	}

	yuv_step[0] = awidth;
	yuv_step[1] = uvwidth;
	yuv_step[2] = uvwidth;

	for (UINT32 x = 0; x < ARRAYSIZE(formats); x++)
	{
		pstatus_t rc = 0;
		const UINT32 DstFormat = formats[x];
		printf("Testing destination color format %s\n", FreeRDPGetColorFormatName(DstFormat));
		memset(rgb_dst, PADDING_FILL_VALUE, size * sizeof(UINT32));

		PROFILER_CREATE(rgbToYUV420, "RGBToYUV420")
		PROFILER_CREATE(rgbToYUV444, "RGBToYUV444")
		PROFILER_CREATE(yuv420ToRGB, "YUV420ToRGB")
		PROFILER_CREATE(yuv444ToRGB, "YUV444ToRGB")

		if (use444)
		{
			PROFILER_ENTER(rgbToYUV444)
			rc = prims->RGBToYUV444_8u_P3AC4R(rgb, DstFormat, stride, yuv, yuv_step, &roi);
			PROFILER_EXIT(rgbToYUV444)

			if (rc != PRIMITIVES_SUCCESS)
				goto loop_fail;

			PROFILER_PRINT_HEADER
			PROFILER_PRINT(rgbToYUV444)
			PROFILER_PRINT_FOOTER
		}
		else
		{
			PROFILER_ENTER(rgbToYUV420)
			rc = prims->RGBToYUV420_8u_P3AC4R(rgb, DstFormat, stride, yuv, yuv_step, &roi);
			PROFILER_EXIT(rgbToYUV420)

			if (rc != PRIMITIVES_SUCCESS)
				goto loop_fail;

			PROFILER_PRINT_HEADER
			PROFILER_PRINT(rgbToYUV420)
			PROFILER_PRINT_FOOTER
		}

		if (!check_padding(rgb, size * sizeof(UINT32), padding, "rgb"))
		{
			rc = -1;
			goto loop_fail;
		}

		if ((!check_padding(yuv[0], size, padding, "Y")) ||
		    (!check_padding(yuv[1], uvsize, padding, "U")) ||
		    (!check_padding(yuv[2], uvsize, padding, "V")))
		{
			rc = -1;
			goto loop_fail;
		}

		cnv.pv = yuv;
		if (use444)
		{
			PROFILER_ENTER(yuv444ToRGB)
			rc = prims->YUV444ToRGB_8u_P3AC4R(cnv.cpv, yuv_step, rgb_dst, stride, DstFormat, &roi);
			PROFILER_EXIT(yuv444ToRGB)

			if (rc != PRIMITIVES_SUCCESS)
				goto loop_fail;

		loop_fail:
			PROFILER_EXIT(yuv444ToRGB)
			PROFILER_PRINT_HEADER
			PROFILER_PRINT(yuv444ToRGB)
			PROFILER_PRINT_FOOTER

			if (rc != PRIMITIVES_SUCCESS)
				goto fail;
		}
		else
		{
			PROFILER_ENTER(yuv420ToRGB)

			if (prims->YUV420ToRGB_8u_P3AC4R(cnv.cpv, yuv_step, rgb_dst, stride, DstFormat, &roi) !=
			    PRIMITIVES_SUCCESS)
			{
				PROFILER_EXIT(yuv420ToRGB)
				goto fail;
			}

			PROFILER_EXIT(yuv420ToRGB)
			PROFILER_PRINT_HEADER
			PROFILER_PRINT(yuv420ToRGB)
			PROFILER_PRINT_FOOTER
		}

		if (!check_padding(rgb_dst, size * sizeof(UINT32), padding, "rgb dst"))
			goto fail;

		if ((!check_padding(yuv[0], size, padding, "Y")) ||
		    (!check_padding(yuv[1], uvsize, padding, "U")) ||
		    (!check_padding(yuv[2], uvsize, padding, "V")))
			goto fail;

		BOOL equal = TRUE;
		for (size_t y = 0; y < roi.height; y++)
		{
			BYTE* srgb = &rgb[y * stride];
			BYTE* drgb = &rgb_dst[y * stride];

			if (!similarRGB(y, srgb, drgb, roi.width, DstFormat, use444))
				equal = FALSE;
		}
		if (!equal)
			goto fail;

		PROFILER_FREE(rgbToYUV420)
		PROFILER_FREE(rgbToYUV444)
		PROFILER_FREE(yuv420ToRGB)
		PROFILER_FREE(yuv444ToRGB)
	}

	res = TRUE;
fail:
	printf("[%s] run %s.\n", __func__, (res) ? "SUCCESS" : "FAILED");
	free_padding(rgb, padding);
	free_padding(rgb_dst, padding);
	free_padding(yuv[0], padding);
	free_padding(yuv[1], padding);
	free_padding(yuv[2], padding);
	return res;
}

static BOOL allocate_yuv420(BYTE** planes, UINT32 width, UINT32 height, UINT32 padding)
{
	const size_t size = 1ULL * width * height;
	const size_t uvwidth = (1ULL + width) / 2;
	const size_t uvsize = (1ULL + height) / 2 * uvwidth;

	if (!(planes[0] = set_padding(size, padding)))
		goto fail;

	if (!(planes[1] = set_padding(uvsize, padding)))
		goto fail;

	if (!(planes[2] = set_padding(uvsize, padding)))
		goto fail;

	return TRUE;
fail:
	free_padding(planes[0], padding);
	free_padding(planes[1], padding);
	free_padding(planes[2], padding);
	return FALSE;
}

static void free_yuv420(BYTE** planes, UINT32 padding)
{
	if (!planes)
		return;

	free_padding(planes[0], padding);
	free_padding(planes[1], padding);
	free_padding(planes[2], padding);
	planes[0] = NULL;
	planes[1] = NULL;
	planes[2] = NULL;
}

static BOOL check_yuv420(BYTE** planes, UINT32 width, UINT32 height, UINT32 padding)
{
	const size_t size = 1ULL * width * height;
	const size_t uvwidth = (width + 1) / 2;
	const size_t uvsize = (height + 1) / 2 * uvwidth;
	const BOOL yOk = check_padding(planes[0], size, padding, "Y");
	const BOOL uOk = check_padding(planes[1], uvsize, padding, "U");
	const BOOL vOk = check_padding(planes[2], uvsize, padding, "V");
	return (yOk && uOk && vOk);
}

static BOOL check_for_mismatches(const BYTE* planeA, const BYTE* planeB, UINT32 size)
{
	BOOL rc = FALSE;

	for (UINT32 x = 0; x < size; x++)
	{
		const BYTE a = planeA[x];
		const BYTE b = planeB[x];

		if (fabsf((float)a - (float)b) > 2.0f)
		{
			rc = TRUE;
			(void)fprintf(stderr, "[%08x] %02x != %02x\n", x, a, b);
		}
	}

	return rc;
}

static BOOL compare_yuv420(BYTE** planesA, BYTE** planesB, UINT32 width, UINT32 height,
                           UINT32 padding)
{
	BOOL rc = TRUE;
	const size_t size = 1ULL * width * height;
	const size_t uvwidth = (1ULL * width + 1) / 2;
	const size_t uvsize = (1ULL * height + 1) / 2 * uvwidth;

	if (check_for_mismatches(planesA[0], planesB[0], size))
	{
		(void)fprintf(stderr, "Mismatch in Y planes!\n");
		rc = FALSE;
	}

	if (check_for_mismatches(planesA[1], planesB[1], uvsize))
	{
		(void)fprintf(stderr, "Mismatch in U planes!\n");
		rc = FALSE;
	}

	if (check_for_mismatches(planesA[2], planesB[2], uvsize))
	{
		(void)fprintf(stderr, "Mismatch in V planes!\n");
		rc = FALSE;
	}

	return rc;
}

static UINT32 prand(UINT32 max)
{
	UINT32 tmp = 0;
	if (max <= 1)
		return 1;
	winpr_RAND(&tmp, sizeof(tmp));
	return tmp % (max - 1) + 1;
}

static BOOL TestPrimitiveRgbToLumaChroma(primitives_t* prims, prim_size_t roi, UINT32 version)
{
	BOOL res = FALSE;
	UINT32 awidth = 0;
	UINT32 aheight = 0;
	BYTE* luma[3] = { 0 };
	BYTE* chroma[3] = { 0 };
	BYTE* lumaGeneric[3] = { 0 };
	BYTE* chromaGeneric[3] = { 0 };
	UINT32 yuv_step[3];
	BYTE* rgb = NULL;
	size_t size = 0;
	size_t uvwidth = 0;
	const size_t padding = 0x1000;
	UINT32 stride = 0;
	fn_RGBToAVC444YUV_t fkt = NULL;
	fn_RGBToAVC444YUV_t gen = NULL;
	const UINT32 formats[] = { PIXEL_FORMAT_XRGB32, PIXEL_FORMAT_XBGR32, PIXEL_FORMAT_ARGB32,
		                       PIXEL_FORMAT_ABGR32, PIXEL_FORMAT_RGBA32, PIXEL_FORMAT_RGBX32,
		                       PIXEL_FORMAT_BGRA32, PIXEL_FORMAT_BGRX32 };
	PROFILER_DEFINE(rgbToYUV444)
	PROFILER_DEFINE(rgbToYUV444opt)
	/* Buffers need to be 16x16 aligned. */
	awidth = roi.width;

	if (awidth % 16 != 0)
		awidth += 16 - roi.width % 16;

	aheight = roi.height;

	if (aheight % 16 != 0)
		aheight += 16 - roi.height % 16;

	stride = 1ULL * awidth * sizeof(UINT32);
	size = 1ULL * awidth * aheight;
	uvwidth = 1ULL * (awidth + 1) / 2;

	if (!prims || !generic)
		return FALSE;

	switch (version)
	{
		case 1:
			fkt = prims->RGBToAVC444YUV;
			gen = generic->RGBToAVC444YUV;
			break;

		case 2:
			fkt = prims->RGBToAVC444YUVv2;
			gen = generic->RGBToAVC444YUVv2;
			break;

		default:
			return FALSE;
	}

	if (!fkt || !gen)
		return FALSE;

	(void)fprintf(stderr, "Running AVC444 on frame size %" PRIu32 "x%" PRIu32 "\n", roi.width,
	              roi.height);

	/* Test RGB to YUV444 conversion and vice versa */
	if (!(rgb = set_padding(size * sizeof(UINT32), padding)))
		goto fail;

	if (!allocate_yuv420(luma, awidth, aheight, padding))
		goto fail;

	if (!allocate_yuv420(chroma, awidth, aheight, padding))
		goto fail;

	if (!allocate_yuv420(lumaGeneric, awidth, aheight, padding))
		goto fail;

	if (!allocate_yuv420(chromaGeneric, awidth, aheight, padding))
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		BYTE* line = &rgb[y * stride];

		winpr_RAND(line, 4ULL * roi.width);
	}

	yuv_step[0] = awidth;
	yuv_step[1] = uvwidth;
	yuv_step[2] = uvwidth;

	for (UINT32 x = 0; x < ARRAYSIZE(formats); x++)
	{
		pstatus_t rc = -1;
		const UINT32 DstFormat = formats[x];
		printf("Testing destination color format %s\n", FreeRDPGetColorFormatName(DstFormat));
		PROFILER_CREATE(rgbToYUV444, "RGBToYUV444-generic")
		PROFILER_CREATE(rgbToYUV444opt, "RGBToYUV444-optimized")

		for (UINT32 cnt = 0; cnt < 10; cnt++)
		{
			PROFILER_ENTER(rgbToYUV444opt)
			rc = fkt(rgb, DstFormat, stride, luma, yuv_step, chroma, yuv_step, &roi);
			PROFILER_EXIT(rgbToYUV444opt)

			if (rc != PRIMITIVES_SUCCESS)
				goto loop_fail;
		}

		PROFILER_PRINT_HEADER
		PROFILER_PRINT(rgbToYUV444opt)
		PROFILER_PRINT_FOOTER

		if (!check_padding(rgb, size * sizeof(UINT32), padding, "rgb"))
		{
			rc = -1;
			goto loop_fail;
		}

		if (!check_yuv420(luma, awidth, aheight, padding) ||
		    !check_yuv420(chroma, awidth, aheight, padding))
		{
			rc = -1;
			goto loop_fail;
		}

		for (UINT32 cnt = 0; cnt < 10; cnt++)
		{
			PROFILER_ENTER(rgbToYUV444)
			rc = gen(rgb, DstFormat, stride, lumaGeneric, yuv_step, chromaGeneric, yuv_step, &roi);
			PROFILER_EXIT(rgbToYUV444)

			if (rc != PRIMITIVES_SUCCESS)
				goto loop_fail;
		}

		PROFILER_PRINT_HEADER
		PROFILER_PRINT(rgbToYUV444)
		PROFILER_PRINT_FOOTER

		if (!check_padding(rgb, size * sizeof(UINT32), padding, "rgb"))
		{
			rc = -1;
			goto loop_fail;
		}

		if (!check_yuv420(lumaGeneric, awidth, aheight, padding) ||
		    !check_yuv420(chromaGeneric, awidth, aheight, padding))
		{
			rc = -1;
			goto loop_fail;
		}

		if (!compare_yuv420(luma, lumaGeneric, awidth, aheight, padding) ||
		    !compare_yuv420(chroma, chromaGeneric, awidth, aheight, padding))
		{
			rc = -1;
			goto loop_fail;
		}

	loop_fail:
		PROFILER_FREE(rgbToYUV444)
		PROFILER_FREE(rgbToYUV444opt)

		if (rc != PRIMITIVES_SUCCESS)
			goto fail;
	}

	res = TRUE;
fail:
	printf("[%s][version %u] run %s.\n", __func__, (unsigned)version, (res) ? "SUCCESS" : "FAILED");
	free_padding(rgb, padding);
	free_yuv420(luma, padding);
	free_yuv420(chroma, padding);
	free_yuv420(lumaGeneric, padding);
	free_yuv420(chromaGeneric, padding);
	return res;
}

static BOOL run_tests(prim_size_t roi)
{
	BOOL rc = FALSE;
	for (UINT32 type = PRIMITIVES_PURE_SOFT; type <= PRIMITIVES_AUTODETECT; type++)
	{
		primitives_t* prims = primitives_get_by_type(type);
		if (!prims)
		{
			printf("primitives type %d not supported\n", type);
			continue;
		}

		for (UINT32 x = 0; x < 5; x++)
		{

			printf("-------------------- GENERIC ------------------------\n");

			if (!TestPrimitiveYUV(prims, roi, TRUE))
				goto fail;

			printf("---------------------- END --------------------------\n");
			printf("-------------------- GENERIC ------------------------\n");

			if (!TestPrimitiveYUV(prims, roi, FALSE))
				goto fail;

			printf("---------------------- END --------------------------\n");
			printf("-------------------- GENERIC ------------------------\n");

			if (!TestPrimitiveYUVCombine(prims, roi))
				goto fail;

			printf("---------------------- END --------------------------\n");
			printf("-------------------- GENERIC ------------------------\n");

			if (!TestPrimitiveRgbToLumaChroma(prims, roi, 1))
				goto fail;

			printf("---------------------- END --------------------------\n");
			printf("-------------------- GENERIC ------------------------\n");

			if (!TestPrimitiveRgbToLumaChroma(prims, roi, 2))
				goto fail;

			printf("---------------------- END --------------------------\n");
		}
	}
	rc = TRUE;
fail:
	printf("[%s] run %s.\n", __func__, (rc) ? "SUCCESS" : "FAILED");
	return rc;
}

static void free_yuv(BYTE* yuv[3])
{
	for (size_t x = 0; x < 3; x++)
	{
		free(yuv[x]);
		yuv[x] = NULL;
	}
}

static BOOL allocate_yuv(BYTE* yuv[3], prim_size_t roi)
{
	yuv[0] = calloc(roi.width, roi.height);
	yuv[1] = calloc(roi.width, roi.height);
	yuv[2] = calloc(roi.width, roi.height);

	if (!yuv[0] || !yuv[1] || !yuv[2])
	{
		free_yuv(yuv);
		return FALSE;
	}

	winpr_RAND(yuv[0], 1ULL * roi.width * roi.height);
	winpr_RAND(yuv[1], 1ULL * roi.width * roi.height);
	winpr_RAND(yuv[2], 1ULL * roi.width * roi.height);
	return TRUE;
}

static BOOL yuv444_to_rgb(BYTE* rgb, size_t stride, const BYTE* yuv[3], const UINT32 yuvStep[3],
                          prim_size_t roi)
{
	for (size_t y = 0; y < roi.height; y++)
	{
		const BYTE* yline[3] = {
			yuv[0] + y * roi.width,
			yuv[1] + y * roi.width,
			yuv[2] + y * roi.width,
		};
		BYTE* line = &rgb[y * stride];

		for (size_t x = 0; x < roi.width; x++)
		{
			const BYTE Y = yline[0][x];
			const BYTE U = yline[1][x];
			const BYTE V = yline[2][x];

			writeYUVPixel(&line[x * 4], PIXEL_FORMAT_BGRX32, Y, U, V, writePixelBGRX);
		}
	}
}

/* Check the result of generic matches the optimized routine.
 *
 */
static BOOL compare_yuv444_to_rgb(prim_size_t roi, DWORD type)
{
	BOOL rc = FALSE;
	const UINT32 format = PIXEL_FORMAT_BGRA32;
	BYTE* yuv[3] = { 0 };
	const UINT32 yuvStep[3] = { roi.width, roi.width, roi.width };
	const size_t stride = 4ULL * roi.width;

	primitives_t* prims = primitives_get_by_type(type);
	if (!prims)
	{
		printf("primitives type %" PRIu32 " not supported, skipping\n", type);
		return TRUE;
	}

	BYTE* rgb1 = calloc(roi.height, stride);
	BYTE* rgb2 = calloc(roi.height, stride);

	primitives_t* soft = primitives_get_by_type(PRIMITIVES_PURE_SOFT);
	if (!soft)
		goto fail;
	if (!allocate_yuv(yuv, roi) || !rgb1 || !rgb2)
		goto fail;

	const BYTE* cyuv[] = { yuv[0], yuv[1], yuv[2] };
	if (soft->YUV444ToRGB_8u_P3AC4R(cyuv, yuvStep, rgb1, stride, format, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;
	if (prims->YUV444ToRGB_8u_P3AC4R(cyuv, yuvStep, rgb2, stride, format, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		const BYTE* yline[3] = {
			yuv[0] + y * roi.width,
			yuv[1] + y * roi.width,
			yuv[2] + y * roi.width,
		};
		const BYTE* line1 = &rgb1[y * stride];
		const BYTE* line2 = &rgb2[y * stride];

		for (size_t x = 0; x < roi.width; x++)
		{
			const int Y = yline[0][x];
			const int U = yline[1][x];
			const int V = yline[2][x];
			const UINT32 color1 = FreeRDPReadColor(&line1[x * 4], format);
			const UINT32 color2 = FreeRDPReadColor(&line2[x * 4], format);

			BYTE r1 = 0;
			BYTE g1 = 0;
			BYTE b1 = 0;
			FreeRDPSplitColor(color1, format, &r1, &g1, &b1, NULL, NULL);

			BYTE r2 = 0;
			BYTE g2 = 0;
			BYTE b2 = 0;
			FreeRDPSplitColor(color2, format, &r2, &g2, &b2, NULL, NULL);

			const int dr12 = abs(r1 - r2);
			const int dg12 = abs(g1 - g2);
			const int db12 = abs(b1 - b2);

			if ((dr12 != 0) || (dg12 != 0) || (db12 != 0))
			{
				printf("{\n");
				printf("\tdiff 1/2: yuv {%d, %d, %d}, rgb {%d, %d, %d}\n", Y, U, V, dr12, dg12,
				       db12);
				printf("}\n");
			}

			if ((dr12 > 0) || (dg12 > 0) || (db12 > 0))
			{
				(void)fprintf(stderr,
				              "[%" PRIuz "x%" PRIuz
				              "] generic and optimized data mismatch: r[0x%" PRIx8 "|0x%" PRIx8
				              "] g[0x%" PRIx8 "|0x%" PRIx8 "] b[0x%" PRIx8 "|0x%" PRIx8 "]\n",
				              x, y, r1, r2, g1, g2, b1, b2);
				(void)fprintf(stderr, "roi: %dx%d\n", roi.width, roi.height);
				winpr_HexDump("y0", WLOG_INFO, &yline[0][x], 16);
				winpr_HexDump("y1", WLOG_INFO, &yline[0][x + roi.width], 16);
				winpr_HexDump("u0", WLOG_INFO, &yline[1][x], 16);
				winpr_HexDump("u1", WLOG_INFO, &yline[1][x + roi.width], 16);
				winpr_HexDump("v0", WLOG_INFO, &yline[2][x], 16);
				winpr_HexDump("v1", WLOG_INFO, &yline[2][x + roi.width], 16);
				winpr_HexDump("foo1", WLOG_INFO, &line1[x * 4], 16);
				winpr_HexDump("foo2", WLOG_INFO, &line2[x * 4], 16);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	printf("%s finished with %s\n", __func__, rc ? "SUCCESS" : "FAILURE");
	free_yuv(yuv);
	free(rgb1);
	free(rgb2);

	return rc;
}

/* Check the result of generic matches the optimized routine.
 *
 */
static BOOL compare_rgb_to_yuv444(prim_size_t roi, DWORD type)
{
	BOOL rc = FALSE;
	const UINT32 format = PIXEL_FORMAT_BGRA32;
	const size_t stride = 4ULL * roi.width;
	const UINT32 yuvStep[] = { roi.width, roi.width, roi.width };
	BYTE* yuv1[3] = { 0 };
	BYTE* yuv2[3] = { 0 };

	primitives_t* prims = primitives_get_by_type(type);
	if (!prims)
	{
		printf("primitives type %" PRIu32 " not supported, skipping\n", type);
		return TRUE;
	}

	BYTE* rgb = calloc(roi.height, stride);

	primitives_t* soft = primitives_get_by_type(PRIMITIVES_PURE_SOFT);
	if (!soft || !rgb)
		goto fail;

	if (!allocate_yuv(yuv1, roi) || !allocate_yuv(yuv2, roi))
		goto fail;

	if (soft->RGBToYUV444_8u_P3AC4R(rgb, format, stride, yuv1, yuvStep, &roi) != PRIMITIVES_SUCCESS)
		goto fail;
	if (prims->RGBToYUV444_8u_P3AC4R(rgb, format, stride, yuv2, yuvStep, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		const BYTE* yline1[3] = {
			yuv1[0] + y * roi.width,
			yuv1[1] + y * roi.width,
			yuv1[2] + y * roi.width,
		};
		const BYTE* yline2[3] = {
			yuv2[0] + y * roi.width,
			yuv2[1] + y * roi.width,
			yuv2[2] + y * roi.width,
		};

		for (size_t x = 0; x < ARRAYSIZE(yline1); x++)
		{
			if (memcmp(yline1[x], yline2[x], yuvStep[x]) != 0)
			{
				(void)fprintf(stderr, "[%s] compare failed in line %" PRIuz, __func__, x);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	printf("%s finished with %s\n", __func__, rc ? "SUCCESS" : "FAILURE");
	free(rgb);
	free_yuv(yuv1);
	free_yuv(yuv2);

	return rc;
}

/* Check the result of generic matches the optimized routine.
 *
 */
static BOOL compare_yuv420_to_rgb(prim_size_t roi, DWORD type)
{
	BOOL rc = FALSE;
	const UINT32 format = PIXEL_FORMAT_BGRA32;
	BYTE* yuv[3] = { 0 };
	const UINT32 yuvStep[3] = { roi.width, roi.width / 2, roi.width / 2 };
	const size_t stride = 4ULL * roi.width;

	primitives_t* prims = primitives_get_by_type(type);
	if (!prims)
	{
		printf("primitives type %" PRIu32 " not supported, skipping\n", type);
		return TRUE;
	}

	BYTE* rgb1 = calloc(roi.height, stride);
	BYTE* rgb2 = calloc(roi.height, stride);

	primitives_t* soft = primitives_get_by_type(PRIMITIVES_PURE_SOFT);
	if (!soft)
		goto fail;
	if (!allocate_yuv(yuv, roi) || !rgb1 || !rgb2)
		goto fail;

	const BYTE* cyuv[3] = { yuv[0], yuv[1], yuv[2] };
	if (soft->YUV420ToRGB_8u_P3AC4R(cyuv, yuvStep, rgb1, stride, format, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;
	if (prims->YUV420ToRGB_8u_P3AC4R(cyuv, yuvStep, rgb2, stride, format, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		const BYTE* yline[3] = {
			yuv[0] + y * yuvStep[0],
			yuv[1] + y * yuvStep[1],
			yuv[2] + y * yuvStep[2],
		};
		const BYTE* line1 = &rgb1[y * stride];
		const BYTE* line2 = &rgb2[y * stride];

		for (size_t x = 0; x < roi.width; x++)
		{
			const int Y = yline[0][x];
			const int U = yline[1][x / 2];
			const int V = yline[2][x / 2];
			const UINT32 color1 = FreeRDPReadColor(&line1[x * 4], format);
			const UINT32 color2 = FreeRDPReadColor(&line2[x * 4], format);

			BYTE r1 = 0;
			BYTE g1 = 0;
			BYTE b1 = 0;
			FreeRDPSplitColor(color1, format, &r1, &g1, &b1, NULL, NULL);

			BYTE r2 = 0;
			BYTE g2 = 0;
			BYTE b2 = 0;
			FreeRDPSplitColor(color2, format, &r2, &g2, &b2, NULL, NULL);

			const int dr12 = abs(r1 - r2);
			const int dg12 = abs(g1 - g2);
			const int db12 = abs(b1 - b2);

			if ((dr12 != 0) || (dg12 != 0) || (db12 != 0))
			{
				printf("{\n");
				printf("\tdiff 1/2: yuv {%d, %d, %d}, rgb {%d, %d, %d}\n", Y, U, V, dr12, dg12,
				       db12);
				printf("}\n");
			}

			if ((dr12 > 0) || (dg12 > 0) || (db12 > 0))
			{
				printf("[%s] failed: r[%" PRIx8 "|%" PRIx8 "] g[%" PRIx8 "|%" PRIx8 "] b[%" PRIx8
				       "|%" PRIx8 "]\n",
				       __func__, r1, r2, g1, g2, b1, b2);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	printf("%s finished with %s\n", __func__, rc ? "SUCCESS" : "FAILURE");
	free_yuv(yuv);
	free(rgb1);
	free(rgb2);

	return rc;
}

static BOOL similarYUV(const BYTE* line1, const BYTE* line2, size_t len)
{
	for (size_t x = 0; x < len; x++)
	{
		const int a = line1[x];
		const int b = line2[x];
		const int diff = abs(a - b);
		if (diff >= 2)
			return FALSE;
		return TRUE;
	}
}

/* Due to optimizations the Y value might be off by +/- 1 */
static int similarY(const BYTE* a, const BYTE* b, size_t size, size_t type)
{
	switch (type)
	{
		case 0:
		case 1:
		case 2:
			for (size_t x = 0; x < size; x++)
			{
				const int ba = a[x];
				const int bb = b[x];
				const int diff = abs(ba - bb);
				if (diff > 2)
					return diff;
			}
			return 0;
			break;
		default:
			return memcmp(a, b, size);
	}
}
/* Check the result of generic matches the optimized routine.
 *
 */
static BOOL compare_rgb_to_yuv420(prim_size_t roi, DWORD type)
{
	BOOL rc = FALSE;
	const UINT32 format = PIXEL_FORMAT_BGRA32;
	const size_t stride = 4ULL * roi.width;
	const UINT32 yuvStep[] = { roi.width, roi.width / 2, roi.width / 2 };
	BYTE* yuv1[3] = { 0 };
	BYTE* yuv2[3] = { 0 };

	primitives_t* prims = primitives_get_by_type(type);
	if (!prims)
	{
		printf("primitives type %" PRIu32 " not supported, skipping\n", type);
		return TRUE;
	}

	BYTE* rgb = calloc(roi.height, stride);
	BYTE* rgbcopy = calloc(roi.height, stride);

	primitives_t* soft = primitives_get_by_type(PRIMITIVES_PURE_SOFT);
	if (!soft || !rgb || !rgbcopy)
		goto fail;

	winpr_RAND(rgb, roi.height * stride);
	memcpy(rgbcopy, rgb, roi.height * stride);

	if (!allocate_yuv(yuv1, roi) || !allocate_yuv(yuv2, roi))
		goto fail;

	if (soft->RGBToYUV420_8u_P3AC4R(rgb, format, stride, yuv1, yuvStep, &roi) != PRIMITIVES_SUCCESS)
		goto fail;
	if (memcmp(rgb, rgbcopy, roi.height * stride) != 0)
		goto fail;
	if (prims->RGBToYUV420_8u_P3AC4R(rgb, format, stride, yuv2, yuvStep, &roi) !=
	    PRIMITIVES_SUCCESS)
		goto fail;

	for (size_t y = 0; y < roi.height; y++)
	{
		// odd lines do produce artefacts in last line, skip check
		if (((y + 1) >= roi.height) && ((y % 2) == 0))
			continue;

		const BYTE* yline1[3] = {
			&yuv1[0][y * yuvStep[0]],
			&yuv1[1][(y / 2) * yuvStep[1]],
			&yuv1[2][(y / 2) * yuvStep[2]],
		};
		const BYTE* yline2[3] = {
			&yuv2[0][y * yuvStep[0]],
			&yuv2[1][(y / 2) * yuvStep[1]],
			&yuv2[2][(y / 2) * yuvStep[2]],
		};

		for (size_t x = 0; x < ARRAYSIZE(yline1); x++)
		{
			if (similarY(yline1[x], yline2[x], yuvStep[x], x) != 0)
			{
				(void)fprintf(stderr,
				              "[%s] compare failed in component %" PRIuz ", line %" PRIuz "\n",
				              __func__, x, y);
				(void)fprintf(stderr, "[%s] roi %" PRIu32 "x%" PRIu32 "\n", __func__, roi.width,
				              roi.height);
				winpr_HexDump(TAG, WLOG_WARN, yline1[x], yuvStep[x]);
				winpr_HexDump(TAG, WLOG_WARN, yline2[x], yuvStep[x]);
				winpr_HexDump(TAG, WLOG_WARN, &rgb[y * stride], stride);
				goto fail;
			}
		}
	}

	rc = TRUE;
fail:
	printf("%s finished with %s\n", __func__, rc ? "SUCCESS" : "FAILURE");
	free(rgb);
	free(rgbcopy);
	free_yuv(yuv1);
	free_yuv(yuv2);

	return rc;
}

int TestPrimitivesYUV(int argc, char* argv[])
{
	BOOL large = (argc > 1);
	int rc = -1;
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	prim_size_t roi = { 0 };

	if (argc > 1)
	{
		// NOLINTNEXTLINE(cert-err34-c)
		int crc = sscanf(argv[1], "%" PRIu32 "x%" PRIu32, &roi.width, &roi.height);

		if (crc != 2)
		{
			roi.width = 1920;
			roi.height = 1080;
		}
	}
	else
		get_size(large, &roi.width, &roi.height);

	prim_test_setup(FALSE);

	for (UINT32 type = PRIMITIVES_PURE_SOFT; type <= PRIMITIVES_AUTODETECT; type++)
	{
		if (!compare_yuv444_to_rgb(roi, type))
			goto end;
		if (!compare_rgb_to_yuv444(roi, type))
			goto end;

		if (!compare_yuv420_to_rgb(roi, type))
			goto end;
		if (!compare_rgb_to_yuv420(roi, type))
			goto end;
	}

	if (!run_tests(roi))
		goto end;

	rc = 0;
end:
	printf("[%s] finished, status %s [%d]\n", __func__, (rc == 0) ? "SUCCESS" : "FAILURE", rc);
	return rc;
}
