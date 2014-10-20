
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/print.h>
#include <winpr/image.h>

int test_image_png_to_bmp()
{
	int status;
	wImage* image;

	if (!PathFileExistsA("/tmp/test.png"))
		return 1; /* skip */

	image = winpr_image_new();

	if (!image)
		return -1;

	status = winpr_image_read(image, "/tmp/test.png");

	if (status < 0)
		return -1;

	image->type = WINPR_IMAGE_BITMAP;
	status = winpr_image_write(image, "/tmp/test_out.bmp");

	if (status < 0)
		return -1;

	image->type = WINPR_IMAGE_PNG;
	status = winpr_image_write(image, "/tmp/test_out.png");

	if (status < 0)
		return -1;

	winpr_image_free(image, TRUE);

	return 1;
}

int TestImage(int argc, char* argv[])
{
	test_image_png_to_bmp();

	return 0;
}

