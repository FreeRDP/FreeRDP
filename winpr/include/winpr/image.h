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

#pragma pack(push, 1)

typedef struct
{
	BYTE bfType[2];
	UINT32 bfSize;
	UINT16 bfReserved1;
	UINT16 bfReserved2;
	UINT32 bfOffBits;
} WINPR_BITMAP_FILE_HEADER;

typedef struct
{
	UINT32 biSize;
	INT32 biWidth;
	INT32 biHeight;
	UINT16 biPlanes;
	UINT16 biBitCount;
	UINT32 biCompression;
	UINT32 biSizeImage;
	INT32 biXPelsPerMeter;
	INT32 biYPelsPerMeter;
	UINT32 biClrUsed;
	UINT32 biClrImportant;
} WINPR_BITMAP_INFO_HEADER;

typedef struct
{
	UINT32 bcSize;
	UINT16 bcWidth;
	UINT16 bcHeight;
	UINT16 bcPlanes;
	UINT16 bcBitCount;
} WINPR_BITMAP_CORE_HEADER;

#pragma pack(pop)

/** @defgrop WINPR_IMAGE_FORMAT
 *  #{
 */
#define WINPR_IMAGE_BITMAP 0
#define WINPR_IMAGE_PNG 1
#define WINPR_IMAGE_JPEG 2 /** @since version 3.3.0 */
#define WINPR_IMAGE_WEBP 3 /** @since version 3.3.0 */
/** #} */

#define WINPR_IMAGE_BMP_HEADER_LEN 54

typedef struct
{
	int type;
	UINT32 width;
	UINT32 height;
	BYTE* data;
	UINT32 scanline;
	UINT32 bitsPerPixel;
	UINT32 bytesPerPixel;
} wImage;

/** @defgroup WINPR_IMAGE_CMP_FLAGS WINPR_IMAGE_CMP_FLAGS
 *  @since version 3.3.0
 *  @{
 */
typedef enum
{
	WINPR_IMAGE_CMP_NO_FLAGS = 0,
	WINPR_IMAGE_CMP_IGNORE_DEPTH = 1,
	WINPR_IMAGE_CMP_IGNORE_ALPHA = 2,
	WINPR_IMAGE_CMP_FUZZY = 4
} wImageFlags;

/** @} */

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API int winpr_bitmap_write(const char* filename, const BYTE* data, size_t width,
	                                 size_t height, size_t bpp);

	/** @brief write a bitmap to a file
	 *
	 *  @param filename the name of the file to write to
	 *  @param data the data of the bitmap without headers
	 *  @param stride the byte size of a line in the image
	 *  @param width the width in pixels of a line
	 *  @param height the height of the bitmap
	 *  @param bpp the color depth of the bitmap
	 *
	 *  @since version 3.3.0
	 *
	 *  @return \b >=0 for success, /b <0 for an error
	 */
	WINPR_API int winpr_bitmap_write_ex(const char* filename, const BYTE* data, size_t stride,
	                                    size_t width, size_t height, size_t bpp);
	WINPR_API BYTE* winpr_bitmap_construct_header(size_t width, size_t height, size_t bpp);

	WINPR_API int winpr_image_write(wImage* image, const char* filename);
	WINPR_API int winpr_image_write_ex(wImage* image, UINT32 format, const char* filename);
	WINPR_API int winpr_image_read(wImage* image, const char* filename);

	/** @brief write a bitmap to a buffer and return it
	 *
	 *  @param image the image to write
	 *  @param format the format of type @ref WINPR_IMAGE_FORMAT
	 *  @param size a pointer to hold the size in bytes of the allocated bitmap
	 *
	 *  @since version 3.3.0
	 *
	 *  @return \b NULL in case of failure, a pointer to an allocated buffer otherwise. Use \b free
	 * as deallocator
	 */
	WINPR_API void* winpr_image_write_buffer(wImage* image, UINT32 format, size_t* size);
	WINPR_API int winpr_image_read_buffer(wImage* image, const BYTE* buffer, size_t size);

	WINPR_API void winpr_image_free(wImage* image, BOOL bFreeBuffer);

	WINPR_ATTR_MALLOC(winpr_image_free, 1)
	WINPR_API wImage* winpr_image_new(void);

	/** @brief Check if a image format is supported
	 *
	 *  @param format the format of type @ref WINPR_IMAGE_FORMAT
	 *
	 *  @since version 3.3.0
	 *
	 *  @return \b TRUE if the format is supported, \b FALSE otherwise
	 */
	WINPR_API BOOL winpr_image_format_is_supported(UINT32 format);

	/** @brief Return the file extension of a format
	 *
	 *  @param format the format of type @ref WINPR_IMAGE_FORMAT
	 *
	 *  @since version 3.3.0
	 *
	 *  @return a extension string if format has one or \b NULL
	 */
	WINPR_API const char* winpr_image_format_extension(UINT32 format);

	/** @brief Return the mime type of a format
	 *
	 *  @param format the format of type @ref WINPR_IMAGE_FORMAT
	 *
	 *  @since version 3.3.0
	 *
	 *  @return a mime type string if format has one or \b NULL
	 */
	WINPR_API const char* winpr_image_format_mime(UINT32 format);

	/** @brief Check if two images are content equal
	 *
	 *  @param imageA the first image for the comparison
	 *  @param imageB the second image for the comparison
	 *  @param flags Comparions flags @ref WINPR_IMAGE_CMP_FLAGS
	 *
	 *  @since version 3.3.0
	 *
	 *  @return \b TRUE if they are equal, \b FALSE otherwise
	 */
	WINPR_API BOOL winpr_image_equal(const wImage* imageA, const wImage* imageB, UINT32 flags);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_IMAGE_H */
