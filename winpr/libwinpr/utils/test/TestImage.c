#include <stdio.h>
#include <winpr/string.h>
#include <winpr/assert.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/image.h>

static const char test_src_filename[] = TEST_SOURCE_PATH "/rgb";
static const char test_bin_filename[] = TEST_BINARY_PATH "/rgb";

static BOOL test_image_equal(const wImage* imageA, const wImage* imageB)
{
	return winpr_image_equal(imageA, imageB,
	                         WINPR_IMAGE_CMP_IGNORE_DEPTH | WINPR_IMAGE_CMP_IGNORE_ALPHA |
	                             WINPR_IMAGE_CMP_FUZZY);
}

static BOOL test_equal_to(const wImage* bmp, const char* name, UINT32 format)
{
	BOOL rc = FALSE;
	wImage* cmp = winpr_image_new();
	if (!cmp)
		goto fail;

	char path[MAX_PATH] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s.%s", name, winpr_image_format_extension(format));
	const int cmpSize = winpr_image_read(cmp, path);
	if (cmpSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_read failed for %s", __func__, path);
		goto fail;
	}

	rc = test_image_equal(bmp, cmp);
	if (!rc)
		(void)fprintf(stderr, "[%s] winpr_image_eqal failed", __func__);

fail:
	winpr_image_free(cmp, TRUE);
	return rc;
}

static BOOL test_equal(void)
{
	BOOL rc = FALSE;
	wImage* bmp = winpr_image_new();

	if (!bmp)
		goto fail;

	char path[MAX_PATH] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s.bmp", test_src_filename);
	PathCchConvertStyleA(path, sizeof(path), PATH_STYLE_NATIVE);

	const int bmpSize = winpr_image_read(bmp, path);
	if (bmpSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_read failed for %s", __func__, path);
		goto fail;
	}

	for (UINT32 x = 0; x < UINT8_MAX; x++)
	{
		if (!winpr_image_format_is_supported(x))
			continue;
		if (!test_equal_to(bmp, test_src_filename, x))
			goto fail;
	}

	rc = TRUE;
fail:
	winpr_image_free(bmp, TRUE);

	return rc;
}

static BOOL test_read_write_compare(const char* tname, const char* tdst, UINT32 format)
{
	BOOL rc = FALSE;
	wImage* bmp1 = winpr_image_new();
	wImage* bmp2 = winpr_image_new();
	wImage* bmp3 = winpr_image_new();
	if (!bmp1 || !bmp2 || !bmp3)
		goto fail;

	char spath[MAX_PATH] = WINPR_C_ARRAY_INIT;
	char dpath[MAX_PATH] = WINPR_C_ARRAY_INIT;
	char bpath1[MAX_PATH] = WINPR_C_ARRAY_INIT;
	char bpath2[MAX_PATH] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(spath, sizeof(spath), "%s.%s", tname, winpr_image_format_extension(format));
	(void)_snprintf(dpath, sizeof(dpath), "%s.%s", tdst, winpr_image_format_extension(format));
	(void)_snprintf(bpath1, sizeof(bpath1), "%s.src.%s", dpath,
	                winpr_image_format_extension(WINPR_IMAGE_BITMAP));
	(void)_snprintf(bpath2, sizeof(bpath2), "%s.bin.%s", dpath,
	                winpr_image_format_extension(WINPR_IMAGE_BITMAP));
	PathCchConvertStyleA(spath, sizeof(spath), PATH_STYLE_NATIVE);
	PathCchConvertStyleA(dpath, sizeof(dpath), PATH_STYLE_NATIVE);
	PathCchConvertStyleA(bpath1, sizeof(bpath1), PATH_STYLE_NATIVE);
	PathCchConvertStyleA(bpath2, sizeof(bpath2), PATH_STYLE_NATIVE);

	const int bmpRSize = winpr_image_read(bmp1, spath);
	if (bmpRSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_read failed for %s", __func__, spath);
		goto fail;
	}

	const int bmpWSize = winpr_image_write(bmp1, dpath);
	if (bmpWSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_write failed for %s", __func__, dpath);
		goto fail;
	}

	const int bmp2RSize = winpr_image_read(bmp2, dpath);
	if (bmp2RSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_read failed for %s", __func__, dpath);
		goto fail;
	}

	const int bmpSrcWSize = winpr_image_write_ex(bmp1, WINPR_IMAGE_BITMAP, bpath1);
	if (bmpSrcWSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_write_ex failed for %s", __func__, bpath1);
		goto fail;
	}

	/* write a bitmap and read it back.
	 * this tests if we have the proper internal format */
	const int bmpBinWSize = winpr_image_write_ex(bmp2, WINPR_IMAGE_BITMAP, bpath2);
	if (bmpBinWSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_write_ex failed for %s", __func__, bpath2);
		goto fail;
	}

	const int bmp3RSize = winpr_image_read(bmp3, bpath2);
	if (bmp3RSize <= 0)
	{
		(void)fprintf(stderr, "[%s] winpr_image_read failed for %s", __func__, bpath2);
		goto fail;
	}

	if (!winpr_image_equal(bmp1, bmp2,
	                       WINPR_IMAGE_CMP_IGNORE_DEPTH | WINPR_IMAGE_CMP_IGNORE_ALPHA |
	                           WINPR_IMAGE_CMP_FUZZY))
	{
		(void)fprintf(stderr, "[%s] winpr_image_eqal failed bmp1 bmp2", __func__);
		goto fail;
	}

	rc = winpr_image_equal(bmp3, bmp2,
	                       WINPR_IMAGE_CMP_IGNORE_DEPTH | WINPR_IMAGE_CMP_IGNORE_ALPHA |
	                           WINPR_IMAGE_CMP_FUZZY);
	if (!rc)
		(void)fprintf(stderr, "[%s] winpr_image_eqal failed bmp3 bmp2", __func__);
fail:
	winpr_image_free(bmp1, TRUE);
	winpr_image_free(bmp2, TRUE);
	winpr_image_free(bmp3, TRUE);
	return rc;
}

static BOOL test_read_write(void)
{
	BOOL rc = TRUE;
	for (UINT32 x = 0; x < UINT8_MAX; x++)
	{
		if (!winpr_image_format_is_supported(x))
			continue;
		if (!test_read_write_compare(test_src_filename, test_bin_filename, x))
			rc = FALSE;
	}
	return rc;
}

static BOOL test_load_file(const char* name)
{
	BOOL rc = FALSE;
	wImage* image = winpr_image_new();
	if (!image || !name)
		goto fail;

	const int res = winpr_image_read(image, name);
	rc = (res > 0);

fail:
	winpr_image_free(image, TRUE);
	return rc;
}

static void put_u16(BYTE* p, UINT16 v)
{
	p[0] = (BYTE)(v & 0xff);
	p[1] = (BYTE)((v >> 8) & 0xff);
}

static void put_u32(BYTE* p, UINT32 v)
{
	p[0] = (BYTE)(v & 0xff);
	p[1] = (BYTE)((v >> 8) & 0xff);
	p[2] = (BYTE)((v >> 16) & 0xff);
	p[3] = (BYTE)((v >> 24) & 0xff);
}

/* A 24bpp BMP whose biSizeImage carries the unaligned image size
 * (width * bytesPerPixel * height) selects the unaligned read path. The decoder
 * must not consume more than biSizeImage bytes from the input buffer. The buffer
 * here is allocated to the exact BMP size, so any over-read is detectable. */
static BOOL test_unaligned_no_overread(void)
{
	BOOL rc = FALSE;
	const UINT32 width = 1;
	const UINT32 height = 10;
	const UINT32 bpp = 3;
	const UINT32 uscanline = width * bpp;          /* 3, not a multiple of 4 */
	const UINT32 biSizeImage = uscanline * height; /* 30 */
	const size_t offBits = 54;                     /* 14 + 40 */
	const size_t size = offBits + biSizeImage;     /* 84 */

	wImage* image = winpr_image_new();
	BYTE* bmp = (BYTE*)calloc(1, size);
	if (!image || !bmp)
		goto fail;

	bmp[0] = 'B';
	bmp[1] = 'M';
	put_u32(&bmp[2], (UINT32)size);              /* bfSize */
	put_u32(&bmp[10], (UINT32)offBits);          /* bfOffBits */
	put_u32(&bmp[14], 40);                       /* biSize */
	put_u32(&bmp[18], width);                    /* biWidth */
	put_u32(&bmp[22], (UINT32)(-(INT32)height)); /* biHeight, top-down */
	put_u16(&bmp[26], 1);                        /* biPlanes */
	put_u16(&bmp[28], 24);                       /* biBitCount */
	put_u32(&bmp[30], 0);                        /* biCompression BI_RGB */
	put_u32(&bmp[34], biSizeImage);              /* biSizeImage */
	for (UINT32 i = 0; i < biSizeImage; i++)
		bmp[offBits + i] = (BYTE)i;

	if (winpr_image_read_buffer(image, bmp, size) <= 0)
		goto fail;

	if ((image->width != width) || (image->height != height))
		goto fail;

	for (UINT32 row = 0; row < height; row++)
	{
		for (UINT32 b = 0; b < uscanline; b++)
		{
			if (image->data[1ULL * row * image->scanline + b] != (BYTE)(row * uscanline + b))
				goto fail;
		}
	}

	rc = TRUE;
fail:
	free(bmp);
	winpr_image_free(image, TRUE);
	return rc;
}

static BOOL test_load(void)
{
	const char* names[] = {
		"rgb.16a.bmp", "rgb.16a.nocolor.bmp", "rgb.16.bmp",  "rgb.16.nocolor.bmp",
		"rgb.16x.bmp", "rgb.16x.nocolor.bmp", "rgb.24.bmp",  "rgb.24.nocolor.bmp",
		"rgb.32.bmp",  "rgb.32.nocolor.bmp",  "rgb.32x.bmp", "rgb.32x.nocolor.bmp",
		"rgb.bmp"
	};

	for (size_t x = 0; x < ARRAYSIZE(names); x++)
	{
		const char* name = names[x];
		char* fname = GetCombinedPath(TEST_SOURCE_PATH, name);
		const BOOL res = test_load_file(fname);
		free(fname);
		if (!res)
			return FALSE;
	}

	return TRUE;
}

int TestImage(int argc, char* argv[])
{
	int rc = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_equal())
		rc -= 1;

	if (!test_read_write())
		rc -= 2;

	if (!test_load())
		rc -= 4;

	if (!test_unaligned_no_overread())
		rc -= 8;

	return rc;
}
