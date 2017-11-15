#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/print.h>
#include <winpr/image.h>
#include <winpr/environment.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void* read_image(const char* src, size_t* size)
{
	int success = 0;
	void* a = NULL;
	INT64 src_size;
	FILE* fsrc = fopen(src, "rb");

	if (!fsrc)
	{
		fprintf(stderr, "Failed to open file %s\n", src);
		goto cleanup;
	}

	if (_fseeki64(fsrc, 0, SEEK_END))
	{
		fprintf(stderr, "Failed to seek to file end\n");
		goto cleanup;
	}

	src_size = _ftelli64(fsrc);

	if (_fseeki64(fsrc, 0, SEEK_SET))
	{
		fprintf(stderr, "Failed to seek to SEEK_SET\n");
		goto cleanup;
	}

	a = malloc(src_size);

	if (!a)
	{
		fprintf(stderr, "Failed malloc %ld bytes\n", src_size);
		goto cleanup;
	}

	if (fread(a, sizeof(char), src_size, fsrc) != src_size)
	{
		fprintf(stderr, "Failed read %ld bytes\n", src_size);
		goto cleanup;
	}

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

static int img_compare(wImage* image, wImage* image2, BOOL ignoreType)
{
	int rc = -1;

	if ((image->type != image2->type) && !ignoreType)
	{
		fprintf(stderr, "Image type mismatch %d:%d\n", image->type, image2->type);
		goto cleanup;
	}

	if (image->width != image2->width)
	{
		fprintf(stderr, "Image width mismatch %d:%d\n", image->width, image2->width);
		goto cleanup;
	}

	if (image->height != image2->height)
	{
		fprintf(stderr, "Image height mismatch %d:%d\n", image->height, image2->height);
		goto cleanup;
	}

	if (image->scanline != image2->scanline)
	{
		fprintf(stderr, "Image scanline mismatch %d:%d\n", image->scanline, image2->scanline);
		goto cleanup;
	}

	if (image->bitsPerPixel != image2->bitsPerPixel)
	{
		fprintf(stderr, "Image bitsPerPixel mismatch %d:%d\n", image->bitsPerPixel, image2->bitsPerPixel);
		goto cleanup;
	}

	if (image->bytesPerPixel != image2->bytesPerPixel)
	{
		fprintf(stderr, "Image bytesPerPixel mismatch %d:%d\n", image->bytesPerPixel,
		        image2->bytesPerPixel);
		goto cleanup;
	}

	rc = memcmp(image->data, image2->data, image->scanline * image->height);

	if (rc)
		fprintf(stderr, "Image data mismatch!\n");

cleanup:
	return rc;
}

static wImage* get_image(const char* src)
{
	int status;
	wImage* image = NULL;
	image = winpr_image_new();

	if (!image)
	{
		fprintf(stderr, "Failed to create image!");
		goto cleanup;
	}

	status = winpr_image_read(image, src);

	if (status < 0)
	{
		fprintf(stderr, "Failed to read image %s!", src);
		winpr_image_free(image, TRUE);
		image = NULL;
	}

cleanup:
	return image;
}

static int create_test(const char* src, const char* dst_png, const char* dst_bmp)
{
	int rc = -1;
	int ret = -1;
	int status;
	size_t bsize;
	void* buffer = NULL;
	wImage* image = NULL, *image2 = NULL, *image3 = NULL, *image4 = NULL;

	if (!PathFileExistsA(src))
	{
		fprintf(stderr, "File %s does not exist!", src);
		return -1;
	}

	image = get_image(src);

	/* Read from file using image methods. */
	if (!image)
		goto cleanup;

	/* Write different formats to tmp. */
	image->type = WINPR_IMAGE_BITMAP;
	status = winpr_image_write(image, dst_bmp);

	if (status < 0)
	{
		fprintf(stderr, "Failed to write image %s!\n", dst_bmp);
		goto cleanup;
	}

	image->type = WINPR_IMAGE_PNG;
	status = winpr_image_write(image, dst_png);

	if (status < 0)
	{
		fprintf(stderr, "Failed to write image %s!\n", dst_png);
		goto cleanup;
	}

	/* Read image from buffer, compare. */
	buffer = read_image(src, &bsize);

	if (!buffer)
	{
		fprintf(stderr, "Failed to read image %s!\n", src);
		goto cleanup;
	}

	image2 = winpr_image_new();

	if (!image2)
	{
		fprintf(stderr, "Failed to create image!\n");
		goto cleanup;
	}

	status = winpr_image_read_buffer(image2, buffer, bsize);

	if (status < 0)
	{
		fprintf(stderr, "Failed to read buffer!\n");
		goto cleanup;
	}

	rc = img_compare(image, image2, TRUE);

	if (rc)
		goto cleanup;

	image3 = get_image(dst_png);

	if (!image3)
		goto cleanup;

	rc = img_compare(image, image3, TRUE);

	if (rc)
		goto cleanup;

	image4 = get_image(dst_bmp);

	if (!image4)
		goto cleanup;

	rc = img_compare(image, image4, TRUE);

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

static int test_image_png_to_bmp(void)
{
	char* buffer = TEST_SOURCE_PATH;
	char src_png[PATH_MAX];
	char src_bmp[PATH_MAX];
	char dst_png[PATH_MAX];
	char dst_bmp[PATH_MAX];
	char dst_png2[PATH_MAX];
	char dst_bmp2[PATH_MAX];
	char* tmp = GetKnownPath(KNOWN_PATH_TEMP);

	if (!tmp)
		return -1;

	if (!buffer)
	{
		free(tmp);
		return -1;
	}

	sprintf_s(src_png, sizeof(src_png), "%s/lodepng_32bit.png", buffer);
	sprintf_s(src_bmp, sizeof(src_bmp), "%s/lodepng_32bit.bmp", buffer);
	sprintf_s(dst_png, sizeof(dst_png), "%s/lodepng_32bit.png", tmp);
	sprintf_s(dst_bmp, sizeof(dst_bmp), "%s/lodepng_32bit.bmp", tmp);
	sprintf_s(dst_png2, sizeof(dst_png2), "%s/lodepng_32bit-2.png", tmp);
	sprintf_s(dst_bmp2, sizeof(dst_bmp2), "%s/lodepng_32bit-2.bmp", tmp);
	free(tmp);

	if (create_test(src_png, dst_png, dst_bmp))
		return -1;

	if (create_test(src_bmp, dst_png2, dst_bmp2))
		return -1;

	return 0;
}

int TestImage(int argc, char* argv[])
{
	int rc = test_image_png_to_bmp();
	return rc;
}

