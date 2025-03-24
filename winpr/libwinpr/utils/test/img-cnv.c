#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/image.h>

static const int formats[] = { WINPR_IMAGE_BITMAP, WINPR_IMAGE_PNG, WINPR_IMAGE_JPEG,
	                           WINPR_IMAGE_WEBP };

static void usage(int argc, char* argv[])
{
	const char* prg = "INVALID";
	if (argc > 0)
		prg = argv[0];

	(void)fprintf(stdout, "%s <src> <dst>\n", prg);
	(void)fprintf(stdout, "\tConvert image <src> to <dst>\n");
	(void)fprintf(stdout, "\tSupported formats (for this build):\n");

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const int format = formats[x];
		const char* ext = winpr_image_format_extension(format);
		const char* mime = winpr_image_format_mime(format);
		const BOOL supported = winpr_image_format_is_supported(format);
		if (supported)
		{
			(void)fprintf(stdout, "\t\t%s [.%s]\n", mime, ext);
		}
	}
}

static int detect_format(const char* name)
{
	const char* dot = strrchr(name, '.');
	if (!dot)
	{
		(void)fprintf(stderr, "'%s' does not have a file extension\n", name);
		return -1;
	}

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const int format = formats[x];
		const char* ext = winpr_image_format_extension(format);
		const char* mime = winpr_image_format_mime(format);
		const BOOL supported = winpr_image_format_is_supported(format);
		if (strcmp(&dot[1], ext) == 0)
		{
			(void)fprintf(stdout, "'%s' is of format %s [supported:%s]\n", name, mime,
			              supported ? "true" : "false");
			if (!supported)
				return -2;
			return format;
		}
	}

	(void)fprintf(stderr, "'%s' is a unsupported format\n", name);
	return -3;
}

int main(int argc, char* argv[])
{
	int rc = -4;
	if (argc != 3)
	{
		usage(argc, argv);
		return -1;
	}

	const char* src = argv[1];
	const char* dst = argv[2];

	const int sfmt = detect_format(src);
	const int dfmt = detect_format(dst);
	if ((sfmt < 0) || (dfmt < 0))
	{
		usage(argc, argv);
		return -2;
	}

	wImage* img = winpr_image_new();
	if (!img)
	{
		return -3;
	}

	const int rrc = winpr_image_read(img, src);
	if (rrc <= 0)
	{
		(void)fprintf(stderr, "Failed to read image '%s': %d\n", src, rrc);
		goto fail;
	}
	const int wrc = winpr_image_write(img, dst);
	if (wrc <= 0)
	{
		(void)fprintf(stderr, "Failed to write image '%s': %d\n", dst, wrc);
		goto fail;
	}

	(void)fprintf(stdout, "Successfully converted '%s' to '%s'\n", src, dst);
	rc = 0;
fail:
	winpr_image_free(img, TRUE);
	return rc;
}
