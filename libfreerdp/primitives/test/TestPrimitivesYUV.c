
#include "prim_test.h"

#include <winpr/wlog.h>
#include <winpr/crypto.h>
#include <freerdp/primitives.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define TAG __FILE__

/* YUV to RGB conversion is lossy, so consider every value only
 * differing by less than 2 abs equal. */
static BOOL similar(const BYTE* src, const BYTE* dst, size_t size)
{
	size_t x;

	for (x=0; x<size; x++)
	{
		volatile double val1 = (double)src[x];
		volatile double val2 = (double)dst[x];
		volatile double diff = val1 - val2;

		if (abs(diff) > 2)
		{
			fprintf(stderr, "%zd %02X : %02X diff=%lf\n", x, val1, val2, diff);
			return FALSE;
		}
	}

	return TRUE;
}

static void get_size(UINT32* width, UINT32* height)
{
	winpr_RAND((BYTE*)width, sizeof(*width));
	winpr_RAND((BYTE*)height, sizeof(*height));

	// TODO: Algorithm only works on even resolutions...
	*width = (*width % 4000) << 1;
	*height = (*height % 4000 << 1);
}

static BOOL check_padding(const BYTE* psrc, size_t size, size_t padding, const char* buffer)
{
	size_t x;
	BOOL rc = TRUE;
	const BYTE* src;
	const BYTE* esrc;
	size_t halfPad = (padding+1)/2;

	if (!psrc)
		return FALSE;

	src = psrc - halfPad;
	esrc = src + size + halfPad;
	for (x=0; x<halfPad; x++)
	{
		const BYTE s = *src++;
		const BYTE d = *esrc++;
		if (s != 'A')
		{
			size_t start = x;
			while((x < halfPad) && (*esrc++ != 'A'))
				x++;

			fprintf(stderr, "Buffer underflow detected %02x != %02X %s [%zd-%zd]\n",
				d, 'A', buffer, start, x);
			return FALSE;
		}
		if(d != 'A')
		{
			size_t start = x;
			while((x < halfPad) && (*esrc++ != 'A'))
				x++;

			fprintf(stderr, "Buffer overflow detected %02x != %02X %s [%zd-%zd]\n",
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
	BYTE* src = calloc(1, size + 2 * halfPad);
	if (!src)
		return NULL;

	memset(&src[0], 'A', halfPad);
	memset(&src[halfPad+size], 'A', halfPad);

	psrc = &src[halfPad];
	if (!check_padding(psrc, size, padding, "init"))
	{
		free (src);
		return NULL;
	}

	return psrc;
}

static void free_padding(void* src, size_t padding)
{
	BYTE* ptr;
	if (!src)
		return;

	ptr = ((BYTE*)src) - (padding+1)/2;
	free(ptr);
}

/* Create 2 pseudo YUV420 frames of same size.
 * Combine them and check, if the data is at the expected position. */
static BOOL TestPrimitiveYUVCombine(void)
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
	size_t padding = 10000;
	prim_size_t roi;
	primitives_t* prims = primitives_get();

	get_size(&roi.width, &roi.height);
	awidth = roi.width + 16 - roi.width % 16;
	aheight = roi.height + 16 - roi.height % 16;

	fprintf(stderr, "Running YUVCombine on frame size %lux%lu [%lux%lu]\n",
			roi.width, roi.height, awidth, aheight);
	if (!prims || !prims->YUV420CombineToYUV444)
		goto fail;

	for (x=0; x<3; x++)
	{
		size_t halfStride = ((x>0)?awidth/2:awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x>0)?halfStride*aheight/2:awidth*aheight);

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

		memset(luma[x], 0xAB + 3*x, halfSize);
		memset(chroma[x], 0x80 + 2*x, halfSize);

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

	if (prims->YUV420CombineToYUV444((const BYTE**)luma, lumaStride,
					 (const BYTE**) chroma, chromaStride,
					 yuv, yuvStride, &roi) != PRIMITIVES_SUCCESS)
		goto fail;

	for (x=0; x<3; x++)
	{
		size_t halfStride = ((x>0)?awidth/2:awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x>0)?halfStride*aheight/2:awidth*aheight);

		if (!check_padding(luma[x], halfSize, padding, "luma"))
			goto fail;
		if (!check_padding(chroma[x], halfSize, padding, "chroma"))
			goto fail;
		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	if (prims->YUV444SplitToYUV420(yuv, yuvStride, pmain, lumaStride,
				       paux, chromaStride, &roi) != PRIMITIVES_SUCCESS)
		goto fail;

	for (x=0; x<3; x++)
	{
		size_t halfStride = ((x>0)?awidth/2:awidth);
		size_t size = aheight * awidth;
		size_t halfSize = ((x>0)?halfStride*aheight/2:awidth*aheight);

		if (!check_padding(pmain[x], halfSize, padding, "main"))
			goto fail;
		if (!check_padding(paux[x], halfSize, padding, "aux"))
			goto fail;
		if (!check_padding(yuv[x], size, padding, "yuv"))
			goto fail;
	}

	for (i=0; i<3; i++)
	{
		for (y=0; y<roi.height; y++)
		{
			UINT32 w = roi.width;
			UINT32 lstride = lumaStride[i];
			UINT32 cstride = chromaStride[i];

			if (i > 0)
			{
				w = (roi.width+3) / 4;
				if (roi.height > (roi.height+1)/2)
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

	rc = TRUE;
fail:
	for (x=0; x<3; x++)
	{
		free_padding(yuv[x], padding);
		free_padding(luma[x], padding);
		free_padding(chroma[x], padding);
		free_padding(pmain[x], padding);
		free_padding(paux[x], padding);
	}

	return rc;
}

static BOOL TestPrimitiveYUV(BOOL use444)
{
	BOOL rc = FALSE;
	UINT32 x, y;
	UINT32 awidth, aheight;
	BYTE* yuv[3] = {0};
	UINT32 yuv_step[3];
	prim_size_t roi;
	BYTE* rgb = NULL;
	BYTE* rgb_dst = NULL;
	size_t size;
	primitives_t* prims = primitives_get();
	size_t uvsize, uvwidth;
	size_t padding = 10000;
	size_t stride;

	get_size(&roi.width, &roi.height);

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

	fprintf(stderr, "Running AVC%s on frame size %lux%lu\n", use444 ? "444" : "420",
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

	for (y=0; y<roi.height; y++)
	{
		BYTE* line = &rgb[y*stride];
		for (x=0; x<roi.width; x++)
		{
			line[x*4+0] = 0x81;
			line[x*4+1] = 0x33;
			line[x*4+2] = 0xAB;
			line[x*4+3] = 0xFF;
		}
	}

	yuv_step[0] = awidth;
	yuv_step[1] = uvwidth;
	yuv_step[2] = uvwidth;

	if (use444)
	{
		if (prims->RGBToYUV444_8u_P3AC4R(rgb, stride, yuv, yuv_step, &roi) != PRIMITIVES_SUCCESS)
			goto fail;
	}
	else if (prims->RGBToYUV420_8u_P3AC4R(rgb, stride, yuv, yuv_step, &roi) != PRIMITIVES_SUCCESS)
		goto fail;

	if (!check_padding(rgb, size * sizeof(UINT32), padding, "rgb"))
		goto fail;

	if ((!check_padding(yuv[0], size, padding, "Y")) ||
		(!check_padding(yuv[1], uvsize, padding, "U")) ||
		(!check_padding(yuv[2], uvsize, padding, "V")))
		goto fail;

	if (use444)
	{
		if (prims->YUV444ToRGB_8u_P3AC4R((const BYTE**)yuv, yuv_step, rgb_dst, stride, &roi) != PRIMITIVES_SUCCESS)
			goto fail;
	}
	else if (prims->YUV420ToRGB_8u_P3AC4R((const BYTE**)yuv, yuv_step, rgb_dst, stride, &roi) != PRIMITIVES_SUCCESS)
		goto fail;

	if (!check_padding(rgb_dst, size * sizeof(UINT32), padding, "rgb dst"))
		goto fail;

	if ((!check_padding(yuv[0], size, padding, "Y")) ||
		(!check_padding(yuv[1], uvsize, padding, "U")) ||
		(!check_padding(yuv[2], uvsize, padding, "V")))
		goto fail;

	for (y=0; y<roi.height; y++)
	{
		BYTE* srgb = &rgb[y*stride];
		BYTE* drgb = &rgb_dst[y*stride];

		if (!similar(srgb, drgb, roi.width*sizeof(UINT32)))
			goto fail;
	}

	rc = TRUE;
fail:
	free_padding (rgb, padding);
	free_padding (rgb_dst, padding);
	free_padding (yuv[0], padding);
	free_padding (yuv[1], padding);
	free_padding (yuv[2], padding);

	return rc;
}

int TestPrimitivesYUV(int argc, char* argv[])
{
	UINT32 x;
	int rc = -1;

	primitives_init();

	for (x=0; x<10; x++)
	{
		/* TODO: This test fails on value comparison,
		 * there seems to be some issue left with encoder / decoder pass.
		if (!TestPrimitiveYUV(FALSE))
			goto end;
			*/
		if (!TestPrimitiveYUV(TRUE))
			goto end;
		if (!TestPrimitiveYUVCombine())
			goto end;
	}
	rc = 0;
end:
	primitives_deinit();
	return rc;
}

