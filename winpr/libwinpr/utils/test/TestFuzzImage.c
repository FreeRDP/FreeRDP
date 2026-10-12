/*
 * libFuzzer harness for WinPR's image parser/writer (winpr_image).
 *
 * This is the path that consumes untrusted image data from a remote peer:
 * clipboard formats (image/png, image/jpeg, image/bmp, CF_DIB) are read via
 * winpr_image_read_buffer() and re-encoded via winpr_image_write_buffer()
 * (see winpr/libwinpr/clipboard/synthetic.c). GHSA-f3vg (PNG->CF_DIB OOB read)
 * lived on exactly this chain; the upstream tree has no dedicated image fuzzer.
 *
 * Input: raw image bytes (BMP/PNG/JPEG/WebP - format sniffed by the reader).
 */
#include <winpr/crt.h>
#include <winpr/image.h>

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if ((Size < 1) || (Size > (1u << 24)))
		return 0;

	wImage* img = winpr_image_new();
	if (!img)
		return 0;

	const int rc = winpr_image_read_buffer(img, Data, Size);
	if (rc > 0)
	{
		const UINT32 formats[] = {
			WINPR_IMAGE_BITMAP,
#if defined(WINPR_UTILS_IMAGE_PNG)
			WINPR_IMAGE_PNG,
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
			WINPR_IMAGE_JPEG,
#endif
#if defined(WINPR_UTILS_IMAGE_WEBP)
			WINPR_IMAGE_WEBP,
#endif
		};
		for (size_t i = 0; i < ARRAYSIZE(formats); i++)
		{
			size_t osize = 0;
			void* out = winpr_image_write_buffer(img, formats[i], &osize);
			free(out);
		}
	}

	winpr_image_free(img, TRUE);
	return 0;
}
