
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/print.h>
#include <winpr/image.h>
#include <winpr/environment.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void *read_image(const char *src, size_t *size)
{
	int success = 0;
	void *a = NULL;
	long src_size;
	FILE *fsrc = fopen(src, "r");

	if (!fsrc)
		goto cleanup;

	if (fseek(fsrc, 0, SEEK_END))
		goto cleanup;

	src_size = ftell(fsrc);

	if (fseek(fsrc, 0, SEEK_SET))
		goto cleanup;

	a = malloc(src_size);

	if (!a)
		goto cleanup;

	if (fread(a, sizeof(char), src_size,  fsrc) != src_size)
		goto cleanup;

	success = 1;
	*size = src_size;

cleanup:
	if (a && !success)
	{
		free(a);
		a = NULL;
	}
	if (fsrc)
		fclose(fsrc);

	return a;
}


static int compare(const char *src, const char *dst)
{
	int cmp = -1;
	size_t asize, bsize;
	void *a, *b;

	a = read_image(src, &asize);
	b = read_image(dst, &bsize);

	if (!a || !b || (asize != bsize))
		goto cleanup;

	cmp = memcmp(a, b, asize);

cleanup:
	free(a);
	free(b);

	return cmp;
}

static int img_compare(wImage *image, wImage *image2)
{
	int rc = -1;
	if (image->type != image2->type)
		goto cleanup;

	if (image->width != image2->width)
		goto cleanup;

	if (image->height != image2->height)
		goto cleanup;

	if (image->scanline != image2->scanline)
		goto cleanup;

	if (image->bitsPerPixel != image2->bitsPerPixel)
		goto cleanup;

	if (image->bytesPerPixel != image2->bytesPerPixel)
		goto cleanup;

	rc = memcmp(image->data, image2->data, image->scanline * image->height);

cleanup:
	return rc;
}

static wImage *get_image(const char *src)
{
	int status;
	wImage* image = NULL;

	image = winpr_image_new();

	if (!image)
		goto cleanup;

	status = winpr_image_read(image, src);

	if (status < 0)
	{
		winpr_image_free(image, TRUE);
		image = NULL;
	}

cleanup:

	return image;
}

static int create_test(const char *src, const char *dst_png, const char *dst_bmp)
{
	int rc = -1;
	int ret = -1;
	int status;
	size_t bsize;
	void *buffer = NULL;
	wImage* image = NULL, *image2 = NULL, *image3 = NULL, *image4 = NULL;

	if (!PathFileExistsA(src))
		return -1;

	image = get_image(src);

	/* Read from file using image methods. */
	if (!image)
		goto cleanup;

	/* Write different formats to tmp. */
	image->type = WINPR_IMAGE_BITMAP;
	status = winpr_image_write(image, dst_bmp);

	if (status < 0)
		goto cleanup;

	image->type = WINPR_IMAGE_PNG;
	status = winpr_image_write(image, dst_png);

	if (status < 0)
		goto cleanup;

	/* Read image from buffer, compare. */
	buffer = read_image(src, &bsize);
	if (!buffer)
		goto cleanup;

	image2 = winpr_image_new();

	if (!image2)
		goto cleanup;

	status = winpr_image_read_buffer(image2, buffer, bsize);

	if (status < 0)
		goto cleanup;

	rc = img_compare(image, image2);
	if (rc)
		goto cleanup;

	image3 = get_image(dst_png);
	if (!image3)
		goto cleanup;

	rc = img_compare(image, image3);
	if (rc)
		goto cleanup;

	image4 = get_image(dst_bmp);
	if (!image4)
		goto cleanup;

	image->type = WINPR_IMAGE_BITMAP;
	rc = img_compare(image, image4);
	if (rc)
		goto cleanup;

	ret = 0;
cleanup:
	if (image)
		winpr_image_free(image, TRUE);
	if (image2)
		winpr_image_free(image2, TRUE);
	if (image3)
		winpr_image_free(image3, TRUE);
	if (image4)
		winpr_image_free(image4, TRUE);

	free(buffer);

	return ret;
}

int test_image_png_to_bmp()
{
	char *buffer = TEST_SOURCE_PATH;
	char src_png[PATH_MAX];
	char src_bmp[PATH_MAX];
	char dst_png[PATH_MAX];
	char dst_bmp[PATH_MAX];
	char dst_png2[PATH_MAX];
	char dst_bmp2[PATH_MAX];
	char *tmp = GetKnownPath(KNOWN_PATH_TEMP);

	if (!tmp)
		return -1;

	if (!buffer)
		return -1;

	sprintf_s(src_png, sizeof(src_png), "%s/lodepng_32bit.png", buffer);
	sprintf_s(src_bmp, sizeof(src_bmp), "%s/lodepng_32bit.bmp", buffer);
	sprintf_s(dst_png, sizeof(dst_png), "%s/lodepng_32bit.png", tmp);
	sprintf_s(dst_bmp, sizeof(dst_bmp), "%s/lodepng_32bit.bmp", tmp);
	sprintf_s(dst_png2, sizeof(dst_png2), "%s/lodepng_32bit-2.png", tmp);
	sprintf_s(dst_bmp2, sizeof(dst_bmp2), "%s/lodepng_32bit-2.bmp", tmp);

	if (create_test(src_png, dst_png, dst_bmp))
		return -1;

	if (create_test(src_bmp, dst_png2, dst_bmp2))
		return -1;

#if 0
	if (compare(dst_png2, dst_png))
		return -1;

	if (compare(dst_bmp2, dst_bmp))
		return -1;
#endif

	return 1;
}

int TestImage(int argc, char* argv[])
{
	test_image_png_to_bmp();

	return 0;
}

