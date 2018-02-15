
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "prim_test.h"

#include <winpr/wlog.h>
#include <winpr/crypto.h>
#include <freerdp/primitives.h>
#include <freerdp/utils/profiler.h>

#define TAG __FILE__

/* YUV to RGB conversion is lossy, so consider every value only
 * differing by less than 2 abs equal. */
static BOOL similar(const BYTE* src, const BYTE* dst, size_t size)
{
	size_t x;

	for (x = 0; x < size; x++)
	{
		int diff = src[x] - dst[x];

		if (abs(diff) > 4)
		{
			fprintf(stderr, "%"PRIuz" %02"PRIX8" : %02"PRIX8" diff=%d\n", x, src[x], dst[x], abs(diff));
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL similarRGB(const BYTE* src, const BYTE* dst, size_t size, UINT32 format)
{
	size_t x;
	const UINT32 bpp = GetBytesPerPixel(format);

	for (x = 0; x < size; x++)
	{
		const double maxDiff = 4.0;
		UINT32 sColor, dColor;
		BYTE sR, sG, sB, sA;
		BYTE dR, dG, dB, dA;
		sColor = ReadColor(src, format);
		dColor = ReadColor(dst, format);
		src += bpp;
		dst += bpp;
		SplitColor(sColor, format, &sR, &sG, &sB, &sA, NULL);
		SplitColor(dColor, format, &dR, &dG, &dB, &dA, NULL);

		if ((abs(sR - dR) > maxDiff) || (abs(sG - dG) > maxDiff) || (abs(sB - dB) > maxDiff))
		{
			fprintf(stderr, "Color value  mismatch R[%02X %02X], G[%02X %02X], B[%02X %02X] at position %lu",
			        sR, dR, sG, dG, sA, dA, x);
			return FALSE;
		}

		if (dA != 0xFF)
		{
			fprintf(stderr, "Invalid destination alpha value %02X at position %lu", dA, x);
			return FALSE;
		}
	}

	return TRUE;
}

static void get_size(BOOL large, UINT32* width, UINT32* height)
{
	UINT32 shift = large ? 8 : 1;
	winpr_RAND((BYTE*)width, sizeof(*width));
	winpr_RAND((BYTE*)height, sizeof(*height));
	// TODO: Algorithm only works on even resolutions...
	*width = (*width % 64 + 1) << shift;
	*height = (*height % 64 + 1) << shift;
}

static BOOL check_padding(const BYTE* psrc, size_t size, size_t padding,
                          const char* buffer)
{
	size_t x;
	BOOL rc = TRUE;
	const BYTE* src;
	const BYTE* esrc;
	size_t halfPad = (padding + 1) / 2;

	if (!psrc)
		return FALSE;

	src = psrc - halfPad;
	esrc = src + size + halfPad;

	for (x = 0; x < halfPad; x++)
	{
		const BYTE s = *src++;
		const BYTE d = *esrc++;

		if (s != 'A')
		{
			size_t start = x;

			while ((x < halfPad) && (*esrc++ != 'A'))
				x++;

			fprintf(stderr, "Buffer underflow detected %02"PRIx8" != %02X %s [%"PRIuz"-%"PRIuz"]\n",
			        d, 'A', buffer, start, x);
			return FALSE;
		}

		if (d != 'A')
		{
			size_t start = x;

			while ((x < halfPad) && (*esrc++ != 'A'))
				x++;

			fprintf(stderr, "Buffer overflow detected %02"PRIx8" != %02X %s [%"PRIuz"-%"PRIuz"]\n",
			        d, 'A', buffer, start, x);
			return FALSE;
		}
	}

	return rc;
}

static void* set_padding(size_t size, size_t padding)
{
	size_t halfPad = (padding + 1) / 2;
	BYTE* psrc;
	BYTE* src = _aligned_malloc(size + 2 * halfPad, 16);

	if (!src)
		return NULL;

	memset(&src[0], 'A', halfPad);
	memset(&src[halfPad], 0, size);
	memset(&src[halfPad + size], 'A', halfPad);
	psrc = &src[halfPad];

	if (!check_padding(psrc, size, padding, "init"))
	{
		_aligned_free(src);
		return NULL;
	}

	return psrc;
}

static void free_padding(void* src, size_t padding)
{
	BYTE* ptr;

	if (!src)
		return;

	ptr = ((BYTE*)src) - (padding + 1) / 2;
	_aligned_free(ptr);
}

/* Create 2 pseudo YUV420 frames of same size.
 * Combine them and check, if the data is at the expected position. */
static BOOL TestPrimitiveYUVCombine(primitives_t* prims, prim_size_t roi)
{
	UINT32 x, y, i;
	UINT32 awidth, aheight;
	BOOL rc = FALSE;
	BYTE* luma[3] = { 0 };
	BYTE* chroma[3] = { 0 };
	BYTE* yuv[3] = { 0 };
	BYTE* pmain[3] = { 0 };
	BYTE* paux[3] = { 0 };
	UINT32 lumaStride[3];
	UINT32 chromaStride[3];
	UINT32 yuvStride[3];
	const size_t padding = 10000;
	RECTANGLE_16 rect;
	PROFILER_DEFINE(yuvCombine)
	PROFILER_DEFINE(yuvSplit)
	awidth = roi.width + 16 - roi.width % 16;
	aheight = roi.height + 16 - roi.height % 16;
	fprintf(stderr, "Running YUVCombine on frame size %"PRIu32"x%"PRIu32" [%"PRIu32"x%"PRIu32"]\n",
	        roi.width, roi.height, awidth, aheight);
	PROFILER_CREATE(yuvCombine, "YUV420CombineToYUV444")
	PROFILER_CREATE(yuvSplit, "YUV444SplitToYUV420")
	rect.left = 0;
	rect.top = 0;
	rect.right = roi.width;
	rect.bottom = roi.height;

	if (!prims || !prims->YUV420CombineToYUV444)
		goto fail;

	for (x = 0; x < 3; x++)
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

		memset(luma[x], 0xAB + 3 * x, halfSize);
		memset(chroma[x], 0x80 + 2 * x, halfSize);

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

	if (prims->YUV420CombineToYUV444(AVC444_LUMA,
	                                 (const BYTE**)luma, lumaStride,
	                                 roi.width, roi.height,
	                                 yuv, yuvStride, &rect) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvCombine)
		goto fail;
	}

	if (prims->YUV420CombineToYUV444(AVC444_CHROMAv1,
	                                 (const BYTE**)chroma, chromaStride,
	                                 roi.width, roi.height,
	                                 yuv, yuvStride, &rect) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvCombine)
		goto fail;
	}

	PROFILER_EXIT(yuvCombine)

	for (x = 0; x < 3; x++)
	{
		size_t halfStride = ((x > 0) ? awidth / 2 : awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x > 0) ? halfStride * aheight / 2 : awidth * aheight);

		if (!check_padding(luma[x], halfSize, padding, "luma"))
			goto fail;

		if (!check_padding(chroma[x], halfSize, padding, "chroma"))
			goto fail;

		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	PROFILER_ENTER(yuvSplit)

	if (prims->YUV444SplitToYUV420((const BYTE**)yuv, yuvStride, pmain, lumaStride,
	                               paux, chromaStride, &roi) != PRIMITIVES_SUCCESS)
	{
		PROFILER_EXIT(yuvSplit)
		goto fail;
	}

	PROFILER_EXIT(yuvSplit)

	for (x = 0; x < 3; x++)
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

	for (i = 0; i < 3; i++)
	{
		for (y = 0; y < roi.height; y++)
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

			if (!similar(luma[i] + y * lstride,
			             pmain[i]  + y * lstride,
			             w))
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

			if (!similar(chroma[i] + y * cstride,
			             paux[i]  + y * cstride,
			             w))
				goto fail;
		}
	}

	PROFILER_PRINT_HEADER
	PROFILER_PRINT(yuvSplit)
	PROFILER_PRINT(yuvCombine)
	PROFILER_PRINT_FOOTER
	rc = TRUE;
fail:
	PROFILER_FREE(yuvCombine)
	PROFILER_FREE(yuvSplit)

	for (x = 0; x < 3; x++)
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
	BOOL rc = FALSE;
	UINT32 x, y;
	UINT32 awidth, aheight;
	BYTE* yuv[3] = {0};
	UINT32 yuv_step[3];
	BYTE* rgb = NULL;
	BYTE* rgb_dst = NULL;
	size_t size;
	size_t uvsize, uvwidth;
	size_t padding = 100 * 16;
	UINT32 stride;
	const UINT32 formats[] =
	{
		PIXEL_FORMAT_XRGB32,
		PIXEL_FORMAT_XBGR32,
		PIXEL_FORMAT_ARGB32,
		PIXEL_FORMAT_ABGR32,
		PIXEL_FORMAT_RGBA32,
		PIXEL_FORMAT_RGBX32,
		PIXEL_FORMAT_BGRA32,
		PIXEL_FORMAT_BGRX32
	};
	PROFILER_DEFINE(rgbToYUV420)
	PROFILER_DEFINE(rgbToYUV444)
	PROFILER_DEFINE(yuv420ToRGB)
	PROFILER_DEFINE(yuv444ToRGB)
	/* Buffers need to be 16x16 aligned. */
	awidth = roi.width + 16 - roi.width % 16;
	aheight = roi.height + 16 - roi.height % 16;
	stride = awidth * sizeof(UINT32);
	size = awidth * aheight;

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

	fprintf(stderr, "Running AVC%s on frame size %"PRIu32"x%"PRIu32"\n", use444 ? "444" : "420",
	        roi.width, roi.height);

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

	for (y = 0; y < roi.height; y++)
	{
		BYTE* line = &rgb[y * stride];

		for (x = 0; x < roi.width; x++)
		{
			line[x * 4 + 0] = 0x81;
			line[x * 4 + 1] = 0x33;
			line[x * 4 + 2] = 0xAB;
			line[x * 4 + 3] = 0xFF;
		}
	}

	yuv_step[0] = awidth;
	yuv_step[1] = uvwidth;
	yuv_step[2] = uvwidth;

	for (x = 0; x < sizeof(formats) / sizeof(formats[0]); x++)
	{
		pstatus_t rc;
		const UINT32 DstFormat = formats[x];
		printf("Testing destination color format %s\n", FreeRDPGetColorFormatName(DstFormat));
		PROFILER_CREATE(rgbToYUV420, "RGBToYUV420")
		PROFILER_CREATE(rgbToYUV444, "RGBToYUV444")
		PROFILER_CREATE(yuv420ToRGB, "YUV420ToRGB")
		PROFILER_CREATE(yuv444ToRGB, "YUV444ToRGB")

		if (use444)
		{
			PROFILER_ENTER(rgbToYUV444)
			rc = prims->RGBToYUV444_8u_P3AC4R(rgb, DstFormat,
			                                  stride, yuv, yuv_step,
			                                  &roi);
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
			rc = prims->RGBToYUV420_8u_P3AC4R(rgb, DstFormat,
			                                  stride, yuv, yuv_step,
			                                  &roi);
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

		if (use444)
		{
			PROFILER_ENTER(yuv444ToRGB)
			rc = prims->YUV444ToRGB_8u_P3AC4R((const BYTE**)yuv, yuv_step, rgb_dst, stride,
			                                  DstFormat,
			                                  &roi);
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

			if (prims->YUV420ToRGB_8u_P3AC4R((const BYTE**)yuv, yuv_step, rgb_dst,
			                                 stride, DstFormat, &roi) != PRIMITIVES_SUCCESS)
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

		for (y = 0; y < roi.height; y++)
		{
			BYTE* srgb = &rgb[y * stride];
			BYTE* drgb = &rgb_dst[y * stride];

			if (!similarRGB(srgb, drgb, roi.width, DstFormat))
				goto fail;
		}

		PROFILER_FREE(rgbToYUV420)
		PROFILER_FREE(rgbToYUV444)
		PROFILER_FREE(yuv420ToRGB)
		PROFILER_FREE(yuv444ToRGB)
	}

	rc = TRUE;
fail:
	free_padding(rgb, padding);
	free_padding(rgb_dst, padding);
	free_padding(yuv[0], padding);
	free_padding(yuv[1], padding);
	free_padding(yuv[2], padding);
	return rc;
}

static BOOL allocate_yuv420(BYTE** planes, UINT32 width, UINT32 height, UINT32 padding)
{
	const size_t size = width * height;
	const size_t uvwidth = (width + 1) / 2;
	const size_t uvsize = (height + 1) / 2 * uvwidth;

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
	const size_t size = width * height;
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
	UINT32 x;

	for (x = 0; x < size; x++)
	{
		const BYTE a = planeA[x];
		const BYTE b = planeB[x];

		if (fabsf(a - b) > 2.0f)
		{
			rc = TRUE;
			fprintf(stderr, "[%08x] %02x != %02x\n",
			        x, a, b);
		}
	}

	return rc;
}

static BOOL compare_yuv420(BYTE** planesA, BYTE** planesB, UINT32 width, UINT32 height,
                           UINT32 padding)
{
	BOOL rc = TRUE;
	const size_t size = width * height;
	const size_t uvwidth = (width + 1) / 2;
	const size_t uvsize = (height + 1) / 2 * uvwidth;

	if (check_for_mismatches(planesA[0], planesB[0], size))
	{
		fprintf(stderr, "Mismatch in Y planes!");
		rc = FALSE;
	}

	if (check_for_mismatches(planesA[1], planesB[1], uvsize))
	{
		fprintf(stderr, "Mismatch in U planes!");
		rc = FALSE;
	}

	if (check_for_mismatches(planesA[2], planesB[2], uvsize))
	{
		fprintf(stderr, "Mismatch in V planes!");
		rc = FALSE;
	}

	return rc;
}

static BOOL TestPrimitiveRgbToLumaChroma(primitives_t* prims, prim_size_t roi, UINT32 version)
{
	BOOL rc = FALSE;
	UINT32 x, y, cnt;
	UINT32 awidth, aheight;
	BYTE* luma[3] = {0};
	BYTE* chroma[3] = {0};
	BYTE* lumaGeneric[3] = {0};
	BYTE* chromaGeneric[3] = {0};
	UINT32 yuv_step[3];
	BYTE* rgb = NULL;
	size_t size;
	size_t uvsize, uvwidth;
	const size_t padding = 0x1000;
	UINT32 stride;
	__RGBToAVC444YUV_t fkt, gen;
	const UINT32 formats[] =
	{
		PIXEL_FORMAT_XRGB32,
		PIXEL_FORMAT_XBGR32,
		PIXEL_FORMAT_ARGB32,
		PIXEL_FORMAT_ABGR32,
		PIXEL_FORMAT_RGBA32,
		PIXEL_FORMAT_RGBX32,
		PIXEL_FORMAT_BGRA32,
		PIXEL_FORMAT_BGRX32
	};
	PROFILER_DEFINE(rgbToYUV444)
	PROFILER_DEFINE(rgbToYUV444opt)
	/* Buffers need to be 16x16 aligned. */
	awidth = roi.width;

	if (awidth % 16 != 0)
		awidth += 16 - roi.width % 16;

	aheight = roi.height;

	if (aheight % 16 != 0)
		aheight += 16 - roi.height % 16;

	stride = awidth * sizeof(UINT32);
	size = awidth * aheight;
	uvwidth = (awidth + 1) / 2;
	uvsize = (aheight + 1) / 2 * uvwidth;

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

	fprintf(stderr, "Running AVC444 on frame size %"PRIu32"x%"PRIu32"\n",
	        roi.width, roi.height);

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

	for (y = 0; y < roi.height; y++)
	{
		BYTE* line = &rgb[y * stride];

		for (x = 0; x < roi.width; x++)
		{
#if 1
			line[x * 4 + 0] = rand();
			line[x * 4 + 1] = rand();
			line[x * 4 + 2] = rand();
			line[x * 4 + 3] = rand();
#else
			line[x * 4 + 0] = (y * roi.width + x) * 16 + 5;
			line[x * 4 + 1] = (y * roi.width + x) * 16 + 7;
			line[x * 4 + 2] = (y * roi.width + x) * 16 + 11;
			line[x * 4 + 3] = (y * roi.width + x) * 16 + 0;
#endif
		}
	}

	yuv_step[0] = awidth;
	yuv_step[1] = uvwidth;
	yuv_step[2] = uvwidth;

	for (x = 0; x < sizeof(formats) / sizeof(formats[0]); x++)
	{
		pstatus_t rc;
		const UINT32 DstFormat = formats[x];
		printf("Testing destination color format %s\n", FreeRDPGetColorFormatName(DstFormat));
		PROFILER_CREATE(rgbToYUV444, "RGBToYUV444-generic")
		PROFILER_CREATE(rgbToYUV444opt, "RGBToYUV444-optimized")

		for (cnt = 0; cnt < 10; cnt++)
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

		for (cnt = 0; cnt < 10; cnt++)
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

	rc = TRUE;
fail:
	free_padding(rgb, padding);
	free_yuv420(luma, padding);
	free_yuv420(chroma, padding);
	free_yuv420(lumaGeneric, padding);
	free_yuv420(chromaGeneric, padding);
	return rc;
}

int TestPrimitivesYUV(int argc, char* argv[])
{
	BOOL large = (argc > 1);
	UINT32 x;
	int rc = -1;
	prim_test_setup(FALSE);
	primitives_t* prims = primitives_get();
	WLog_Init();

	for (x = 0; x < 10; x++)
	{
		prim_size_t roi;

		if (argc > 1)
		{
			int rc = sscanf(argv[1], "%l"PRIu32"x%"PRIu32, &roi.width, &roi.height);

			if (rc != 2)
			{
				roi.width = 1920;
				roi.height = 1080;
			}
		}
		else
			get_size(large, &roi.width, &roi.height);

		printf("-------------------- GENERIC ------------------------\n");

		if (!TestPrimitiveYUV(generic, roi, TRUE))
		{
			printf("TestPrimitiveYUV (444) failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("------------------- OPTIMIZED -----------------------\n");

		if (!TestPrimitiveYUV(prims, roi, TRUE))
		{
			printf("TestPrimitiveYUV (444) failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("-------------------- GENERIC ------------------------\n");

		if (!TestPrimitiveYUV(generic, roi, FALSE))
		{
			printf("TestPrimitiveYUV (420) failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("------------------- OPTIMIZED -----------------------\n");

		if (!TestPrimitiveYUV(prims, roi, FALSE))
		{
			printf("TestPrimitiveYUV (420) failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("-------------------- GENERIC ------------------------\n");

		if (!TestPrimitiveYUVCombine(generic, roi))
		{
			printf("TestPrimitiveYUVCombine failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("------------------- OPTIMIZED -----------------------\n");

		if (!TestPrimitiveYUVCombine(prims, roi))
		{
			printf("TestPrimitiveYUVCombine failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("------------------- OPTIMIZED -----------------------\n");

		if (!TestPrimitiveRgbToLumaChroma(prims, roi, 1))
		{
			printf("TestPrimitiveRgbToLumaChroma failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
		printf("-------------------- GENERIC ------------------------\n");

		if (!TestPrimitiveRgbToLumaChroma(prims, roi, 2))
		{
			printf("TestPrimitiveYUVCombine failed.\n");
			goto end;
		}

		printf("---------------------- END --------------------------\n");
	}

	rc = 0;
end:
	WLog_Uninit();
	return rc;
}

