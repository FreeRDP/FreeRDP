/**
 * WinPR: Windows Portable Runtime
 * Image Utils
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_IMAGE_H
#define WINPR_IMAGE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

struct _wImage
{
	int width;
	int height;
	BYTE* data;
	int scanline;
	int bitsPerPixel;
	int bytesPerPixel;
};
typedef struct _wImage wImage;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API int winpr_bitmap_write(const char* filename, BYTE* data, int width, int height, int bpp);

WINPR_API int winpr_image_write(wImage* image, const char* filename);
WINPR_API int winpr_image_read(wImage* image, const char* filename);

WINPR_API wImage* winpr_image_new();
WINPR_API void winpr_image_free(wImage* image, BOOL bFreeBuffer);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_IMAGE_H */
