/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Compressed jpeg
 *
 * Copyright 2012 Jay Sorg <jay.sorg@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <winpr/stream.h>
#include <winpr/image.h>

#include <freerdp/codec/color.h>

#include <freerdp/codec/jpeg.h>

#ifdef WITH_JPEG

/* jpeg decompress */
BOOL jpeg_decompress(const BYTE* input, BYTE* output, int width, int height, int size, int bpp)
{
	BOOL rc = FALSE;

	if (bpp != 24)
		return FALSE;
	if (width <= 0)
		return FALSE;
	if (height <= 0)
		return FALSE;
	if (size <= 0)
		return FALSE;

	wImage* image = winpr_image_new();
	if (!image)
		goto fail;

	if (winpr_image_read_buffer(image, input, WINPR_ASSERTING_INT_CAST(size_t, size)) <= 0)
		goto fail;

	if ((image->width != WINPR_ASSERTING_INT_CAST(size_t, width)) ||
	    (image->height != WINPR_ASSERTING_INT_CAST(size_t, height)) ||
	    (image->bitsPerPixel != WINPR_ASSERTING_INT_CAST(size_t, bpp)))
		goto fail;

	memcpy(output, image->data, 1ull * image->scanline * image->height);
	rc = TRUE;

fail:
	winpr_image_free(image, TRUE);
	return rc;
}

#else

BOOL jpeg_decompress(WINPR_ATTR_UNUSED const BYTE* input, WINPR_ATTR_UNUSED BYTE* output,
                     WINPR_ATTR_UNUSED int width, WINPR_ATTR_UNUSED int height,
                     WINPR_ATTR_UNUSED int size, WINPR_ATTR_UNUSED int bpp)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

#endif
