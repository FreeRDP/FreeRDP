/**
 * WinPR: Windows Portable Runtime
 * Clipboard Functions
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

#include <winpr/config.h>

#include <errno.h>
#include <winpr/crt.h>
#include <winpr/user.h>
#include <winpr/image.h>

#include "../utils/image.h"
#include "clipboard.h"
#include "../crt/unicode.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard.synthetic")

static const char mime_html[] = "text/html";
static const char mime_ms_html[] = "HTML Format";

#define BITMAP_MIME_TYPES "image/bmp", "image/x-bmp", "image/x-MS-bmp", "image/x-win-bitmap"
static const char* mime_bitmap[] = { BITMAP_MIME_TYPES };

#if defined(WINPR_UTILS_IMAGE_WEBP)
static const char mime_webp[] = "image/webp";
#endif
#if defined(WINPR_UTILS_IMAGE_PNG)
static const char mime_png[] = "image/png";
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
static const char mime_jpeg[] = "image/jpeg";
#endif
static const char mime_tiff[] = "image/tiff";

static const char* mime_images[] = {
#if defined(WINPR_UTILS_IMAGE_WEBP)
	mime_webp,
#endif
#if defined(WINPR_UTILS_IMAGE_PNG)
	mime_png,
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
	mime_jpeg,
#endif
	mime_tiff, BITMAP_MIME_TYPES
};

static const BYTE enc_base64url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

WINPR_ATTR_NODISCARD
static inline char* b64_encode(const BYTE* WINPR_RESTRICT data, size_t length, size_t* plen)
{
	WINPR_ASSERT(plen);
	const BYTE* WINPR_RESTRICT alphabet = enc_base64url;
	int c = 0;
	size_t blocks = 0;
	size_t outLen = (length + 3) * 4 / 3;
	size_t extra = 0;

	const BYTE* q = data;
	const size_t alen = outLen + extra + 1ull;
	BYTE* p = malloc(alen);
	if (!p)
		return nullptr;

	BYTE* ret = p;

	/* b1, b2, b3 are input bytes
	 *
	 * 0         1         2
	 * 012345678901234567890123
	 * |  b1  |  b2   |  b3   |
	 *
	 * [ c1 ]     [  c3 ]
	 *      [  c2 ]     [  c4 ]
	 *
	 * c1, c2, c3, c4 are output chars in base64
	 */

	/* first treat complete blocks */
	blocks = length - (length % 3);
	for (size_t i = 0; i < blocks; i += 3, q += 3)
	{
		c = (q[0] << 16) + (q[1] << 8) + q[2];

		*p++ = alphabet[(c & 0x00FC0000) >> 18];
		*p++ = alphabet[(c & 0x0003F000) >> 12];
		*p++ = alphabet[(c & 0x00000FC0) >> 6];
		*p++ = alphabet[c & 0x0000003F];
	}

	/* then remainder */
	switch (length % 3)
	{
		case 0:
			break;
		case 1:
			c = (q[0] << 16);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			break;
		case 2:
			c = (q[0] << 16) + (q[1] << 8);
			*p++ = alphabet[(c & 0x00FC0000) >> 18];
			*p++ = alphabet[(c & 0x0003F000) >> 12];
			*p++ = alphabet[(c & 0x00000FC0) >> 6];
			break;
		default:
			break;
	}

	*p = 0;
	*plen = WINPR_ASSERTING_INT_CAST(size_t, p - ret);

	return (char*)ret;
}

/**
 * Standard Clipboard Formats:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ff729168/
 */

/**
 * "CF_LOCALE":
 *
 * System locale identifier associated with CF_TEXT
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_cf_locale(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                            UINT32 dstFormatId, WINPR_ATTR_UNUSED const void* data,
                                            WINPR_ATTR_UNUSED UINT32* pSize)
{
	if (dstFormatId != CF_LOCALE)
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}

	switch (clipboard->formatId)
	{
		case CF_TEXT:
		case CF_UNICODETEXT:
		case CF_OEMTEXT:
			break;
		default:
		{
			const UINT32 formatIdText = ClipboardRegisterFormat(clipboard, mime_text_plain);
			const UINT32 formatIdUtf = ClipboardRegisterFormat(clipboard, mime_text_plain);
			if ((clipboard->formatId != formatIdText) && (clipboard->formatId != formatIdUtf))
			{
				WLog_ERR(TAG,
				         "Unuspported source format %s [0x%04" PRIx32
				         "], trying to convert to %s [0x%04" PRIx32 "]",
				         ClipboardGetFormatName(clipboard, clipboard->formatId),
				         clipboard->formatId, ClipboardGetFormatName(clipboard, dstFormatId),
				         dstFormatId);
				return nullptr;
			}
		}
		break;
	}

	UINT32* pDstData = (UINT32*)calloc(1, sizeof(UINT32));

	if (!pDstData)
		return nullptr;

	*pDstData = 0x0409; /* English - United States */
	return (void*)pDstData;
}

/**
 * mime_utf8_string:
 *
 * Null-terminated UTF-8 string with LF line endings.
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_string(wClipboard* clipboard, UINT32 dstFormatId,
                                         const void* data, UINT32* pSize)
{
	WINPR_ASSERT(clipboard);

	// Step 1: convert to utf-8
	char* utf8 = nullptr;
	size_t utf8len = 0;
	if (clipboard->formatId == CF_UNICODETEXT)
	{
		size_t size = 0;
		utf8 = ConvertWCharNToUtf8Alloc(data, *pSize / sizeof(WCHAR), &size);

		if (!utf8)
			return nullptr;

		const size_t rc = ConvertLineEndingToLF(utf8, size);
		utf8len = rc;
	}
	else if ((clipboard->formatId == CF_TEXT) || (clipboard->formatId == CF_OEMTEXT) ||
	         (clipboard->formatId == ClipboardGetFormatId(clipboard, mime_text_plain)))
	{
		const size_t size = *pSize;
		utf8 = calloc(size + 1, sizeof(char));

		if (!utf8)
			return nullptr;

		CopyMemory(utf8, data, size);
		const size_t rc = ConvertLineEndingToLF(utf8, size);
		const SSIZE_T res = winpr_utfEscapedStringToUtf8(utf8, rc);
		if (res < 0)
		{
			winpr_znfree(utf8, size);
			return nullptr;
		}
		utf8len = (size_t)res;
	}
	else if ((clipboard->formatId == ClipboardGetFormatId(clipboard, mime_text_utf8)) ||
	         (clipboard->formatId == ClipboardGetFormatId(clipboard, mime_text_UTF8_STRING)))
	{
		const size_t size = *pSize;
		utf8 = strndup(data, size);
		if (!utf8)
			return nullptr;
		utf8len = size;
	}
	else
	{
		WLog_ERR(TAG,
		         "Unuspported source format %s [0x%04" PRIx32
		         "], trying to convert to %s [0x%04" PRIx32 "]",

		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId,
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId);
		return nullptr;
	}

	switch (dstFormatId)
	{
		case CF_UNICODETEXT:
		{
			size_t crlfLen = utf8len;
			char* crlf = ConvertLineEndingToCRLF(utf8, &crlfLen);
			winpr_znfree(utf8, utf8len);
			if (!crlf)
				return nullptr;

			size_t wlen = 0;
			WCHAR* wstr = ConvertUtf8NToWCharAlloc(crlf, crlfLen, &wlen);
			winpr_znfree(crlf, crlfLen);
			if (!wstr || (wlen == 0) || (wlen > UINT32_MAX / sizeof(WCHAR)))
			{
				winpr_znfree(wstr, wlen * sizeof(WCHAR));
				return nullptr;
			}

			*pSize = WINPR_ASSERTING_INT_CAST(UINT32, wlen * sizeof(WCHAR));
			return wstr;
		}
		case CF_OEMTEXT:
		case CF_TEXT:
		{
			size_t esclen = 0;
			char* escaped = winpr_utf8ToUtfEscapedString(utf8, utf8len, &esclen);
			winpr_znfree(utf8, utf8len);
			if (!escaped || (esclen == 0) || (esclen > UINT32_MAX))
			{
				winpr_znfree(escaped, esclen);
				return nullptr;
			}
			*pSize = WINPR_ASSERTING_INT_CAST(UINT32, esclen);
			return escaped;
		}
		default:
			if ((dstFormatId == ClipboardGetFormatId(clipboard, mime_text_utf8)) ||
			    (dstFormatId == ClipboardGetFormatId(clipboard, mime_text_UTF8_STRING)))
			{
				if (utf8len > UINT32_MAX)
				{
					winpr_znfree(utf8, utf8len);
					return nullptr;
				}
				*pSize = WINPR_ASSERTING_INT_CAST(UINT32, utf8len);
				return utf8;
			}

			if (dstFormatId == ClipboardGetFormatId(clipboard, mime_text_plain))
			{
				size_t esclen = 0;
				char* escaped = winpr_utf8ToUtfEscapedString(utf8, utf8len, &esclen);
				winpr_znfree(utf8, utf8len);
				if (!escaped || (esclen == 0) || (esclen > UINT32_MAX))
				{
					winpr_znfree(escaped, esclen);
					return nullptr;
				}
				*pSize = WINPR_ASSERTING_INT_CAST(UINT32, esclen);
				return escaped;
			}

			WLog_ERR(TAG,
			         "Unuspported destination format %s [0x%04" PRIx32
			         "], trying to convert from %s [0x%04" PRIx32 "]",
			         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
			         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
			winpr_znfree(utf8, utf8len);
			return nullptr;
	}
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_prepend_bmp_header(const WINPR_BITMAP_INFO_HEADER* pInfoHeader,
                                          size_t offset, const void* data, size_t size,
                                          UINT32* pSize)
{
	WINPR_ASSERT(pInfoHeader);
	WINPR_ASSERT(pSize);

	*pSize = 0;
	if ((pInfoHeader->biBitCount < 1) || (pInfoHeader->biBitCount > 32))
		return nullptr;

	if (size > (UINT32_MAX - sizeof(WINPR_BITMAP_FILE_HEADER)))
		return nullptr;
	const size_t DstSize = sizeof(WINPR_BITMAP_FILE_HEADER) + size;
	if ((pInfoHeader->biSize > size) || (offset > (size - pInfoHeader->biSize)))
		return nullptr;

	const size_t bitmapOffset = sizeof(WINPR_BITMAP_FILE_HEADER) + pInfoHeader->biSize + offset;
	if (bitmapOffset > DstSize)
		return nullptr;

	wStream* s = Stream_New(nullptr, DstSize);
	if (!s)
		return nullptr;

	WINPR_BITMAP_FILE_HEADER fileHeader = WINPR_C_ARRAY_INIT;
	fileHeader.bfType[0] = 'B';
	fileHeader.bfType[1] = 'M';
	fileHeader.bfSize = (UINT32)DstSize;
	fileHeader.bfOffBits = (UINT32)bitmapOffset;
	if (!writeBitmapFileHeader(s, &fileHeader))
		goto fail;

	if (!Stream_EnsureRemainingCapacity(s, size))
		goto fail;
	Stream_Write(s, data, size);

	{
		const size_t len = Stream_GetPosition(s);
		if (len != DstSize)
			goto fail;
	}

	*pSize = (UINT32)DstSize;

	{
		BYTE* dst = Stream_Buffer(s);
		Stream_Free(s, FALSE);
		return dst;
	}

fail:
	Stream_Free(s, TRUE);
	return nullptr;
}

/**
 * "image/bmp":
 *
 * Bitmap file format.
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_image_bmp(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                            UINT32 dstFormatId, const void* data, UINT32* pSize)
{
	UINT32 SrcSize = *pSize;

	if (dstFormatId == CF_DIB)
	{
		if (SrcSize < sizeof(BITMAPINFOHEADER))
			return nullptr;

		wStream sbuffer = WINPR_C_ARRAY_INIT;
		size_t offset = 0;
		WINPR_BITMAP_INFO_HEADER header = WINPR_C_ARRAY_INIT;
		wStream* s = Stream_StaticConstInit(&sbuffer, data, SrcSize);
		if (!readBitmapInfoHeader(s, &header, &offset))
			return nullptr;

		return clipboard_prepend_bmp_header(&header, offset, data, SrcSize, pSize);
	}
#if defined(WINPR_UTILS_IMAGE_DIBv5)
	else if (dstFormatId == CF_DIBV5)
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
	}
#endif
	else
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
	}

	return nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL format_is_image(wClipboard* clipboard, UINT32 formatId)
{
	switch (formatId)
	{
		case CF_DIB:
		case CF_DIBV5:
		case CF_TIFF:
			return TRUE;
		default:

			for (size_t x = 0; x < ARRAYSIZE(mime_images); x++)
			{
				const char* mime = mime_images[x];
				const UINT32 id = ClipboardRegisterFormat(clipboard, mime);
				if (formatId == id)
					return TRUE;
			}
			return FALSE;
	}
}

WINPR_ATTR_NODISCARD
static UINT32 image_format(wClipboard* clipboard, UINT32 formatId)
{
#if defined(WINPR_UTILS_IMAGE_PNG)
	if (formatId == ClipboardRegisterFormat(clipboard, mime_png))
		return WINPR_IMAGE_PNG;
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
	if (formatId == ClipboardRegisterFormat(clipboard, mime_jpeg))
		return WINPR_IMAGE_JPEG;
#endif
#if defined(WINPR_UTILS_IMAGE_WEBP)
	if (formatId == ClipboardRegisterFormat(clipboard, mime_webp))
		return WINPR_IMAGE_WEBP;
#endif
	return WINPR_IMAGE_BITMAP;
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_image_dib_to_format(wClipboard* clipboard, UINT32 dstFormatId,
                                                      const void* data, UINT32* pSize)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);
	WINPR_ASSERT(pSize);

	if ((clipboard->formatId != CF_DIB) && (clipboard->formatId != CF_DIBV5))
	{
		WLog_ERR(TAG,
		         "Unuspported source format %s [0x%04" PRIx32
		         "], trying to convert to %s [0x%04" PRIx32 "]",

		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId,
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId);
		return nullptr;
	}
	if (!format_is_image(clipboard, dstFormatId))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",

		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}

	const UINT32 format = image_format(clipboard, dstFormatId);
	size_t dsize = 0;
	void* result = nullptr;

	wImage* img = winpr_image_new();
	void* bmp = clipboard_synthesize_image_bmp(clipboard, clipboard->formatId, data, pSize);
	const UINT32 SrcSize = *pSize;
	*pSize = 0;

	if (!bmp || !img)
		goto fail;

	if (winpr_image_read_buffer(img, bmp, SrcSize) <= 0)
		goto fail;

	result = winpr_image_write_buffer(img, format, &dsize);
	if (result)
	{
		if (dsize <= UINT32_MAX)
			*pSize = (UINT32)dsize;
		else
		{
			free(result);
			result = nullptr;
		}
	}

fail:
	free(bmp);
	winpr_image_free(img, TRUE);
	return result;
}

/**
 * "CF_DIB":
 *
 * BITMAPINFO structure followed by the bitmap bits.
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_cf_dib(const void* pvdata, size_t dataLen, UINT32* pSize)
{
	WINPR_ASSERT(pSize);
	const BYTE* data = pvdata;

	WINPR_BITMAP_FILE_HEADER pFileHeader = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, data, dataLen);
	if (!readBitmapFileHeader(s, &pFileHeader))
		return nullptr;

	const size_t DstSize = dataLen - sizeof(BITMAPFILEHEADER);
	BYTE* pDstData = (BYTE*)calloc(DstSize, 1);

	if (!pDstData)
		return nullptr;

	const void* src = &data[sizeof(BITMAPFILEHEADER)];
	memcpy(pDstData, src, DstSize);
	*pSize = WINPR_ASSERTING_INT_CAST(UINT32, DstSize);
	return pDstData;
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_image_format_to_cf_dib(wClipboard* clipboard, UINT32 dstFormatId,
                                                         const void* data, UINT32* pSize)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);
	WINPR_ASSERT(pSize);

	if ((dstFormatId != CF_DIB) && (dstFormatId != CF_DIBV5))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}

	if (!format_is_image(clipboard, dstFormatId))
	{
		WLog_ERR(TAG,
		         "Unuspported source format %s [0x%04" PRIx32
		         "], trying to convert to %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId,
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId);
		return nullptr;
	}

	void* result = nullptr;
	BYTE* dst = nullptr;
	const UINT32 SrcSize = *pSize;
	size_t size = 0;
	wImage* image = winpr_image_new();
	if (!image)
		goto fail;

	const int res = winpr_image_read_buffer(image, data, SrcSize);
	if (res <= 0)
		goto fail;

	dst = winpr_image_write_buffer(image, WINPR_IMAGE_BITMAP, &size);
	if ((size < sizeof(WINPR_BITMAP_FILE_HEADER)) || (size > UINT32_MAX) || !dst)
		goto fail;

	result = clipboard_synthesize_cf_dib(dst, size, pSize);

fail:
	winpr_image_free(image, TRUE);
	free(dst);
	return result;
}

/**
 * "image/png" <-> "image/bmp" <-> "image/jpeg" ...:
 *
 * Image file formats WinPR can read and write, converted into each other directly.
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_image_format_to_format(wClipboard* clipboard, UINT32 dstFormatId,
                                                         const void* data, UINT32* pSize)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);
	WINPR_ASSERT(pSize);

	if (!format_is_image(clipboard, clipboard->formatId) ||
	    !format_is_image(clipboard, dstFormatId))
	{
		WLog_ERR(TAG, "Unsupported conversion from %s [0x%04" PRIx32 "] to %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId,
		         ClipboardGetFormatName(clipboard, dstFormatId), dstFormatId);
		return nullptr;
	}

	void* result = nullptr;
	size_t size = 0;
	const UINT32 SrcSize = *pSize;
	*pSize = 0;

	wImage* image = winpr_image_new();
	if (!image)
		goto fail;

	if (winpr_image_read_buffer(image, data, SrcSize) <= 0)
		goto fail;

	result = winpr_image_write_buffer(image, image_format(clipboard, dstFormatId), &size);
	if (result)
	{
		if (size <= UINT32_MAX)
			*pSize = (UINT32)size;
		else
		{
			free(result);
			result = nullptr;
		}
	}

fail:
	winpr_image_free(image, TRUE);
	return result;
}

/**
 * "HTML Format":
 *
 * HTML clipboard format: msdn.microsoft.com/en-us/library/windows/desktop/ms649015/
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_ms_html_format(wClipboard* clipboard, UINT32 formatId,
                                                 const void* pData, UINT32* pSize)
{
	if (formatId != ClipboardGetFormatId(clipboard, mime_ms_html))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, formatId), formatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}
	if (clipboard->formatId != ClipboardGetFormatId(clipboard, mime_html))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, formatId), formatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}
	union
	{
		const void* cpv;
		const char* cpc;
		const BYTE* cpb;
		WCHAR* pv;
	} pSrcData;
	char* pDstData = nullptr;

	pSrcData.cpv = nullptr;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(pSize);

	if (formatId == ClipboardGetFormatId(clipboard, mime_html))
	{
		const size_t SrcSize = (size_t)*pSize;
		const size_t DstSize = SrcSize + 200;
		char* body = nullptr;
		char num[20] = WINPR_C_ARRAY_INIT;

		/* Create a copy, we modify the input data */
		pSrcData.pv = calloc(1, SrcSize + 1);
		if (!pSrcData.pv)
			goto fail;
		memcpy(pSrcData.pv, pData, SrcSize);

		if (SrcSize > 2)
		{
			if (SrcSize > INT_MAX)
				goto fail;

			/* Check the BOM (Byte Order Mark) */
			if ((pSrcData.cpb[0] == 0xFE) && (pSrcData.cpb[1] == 0xFF))
			{
				if (!ByteSwapUnicode(pSrcData.pv, (SrcSize / 2)))
					goto fail;
			}

			/* Check if we have WCHAR, convert to UTF-8 */
			if ((pSrcData.cpb[0] == 0xFF) && (pSrcData.cpb[1] == 0xFE))
			{
				char* utfString = ConvertWCharNToUtf8Alloc(&pSrcData.pv[1],
				                                           (SrcSize / sizeof(WCHAR)) - 1, nullptr);
				free(pSrcData.pv);
				pSrcData.cpc = utfString;
				if (!utfString)
					goto fail;
			}
		}

		pDstData = (char*)calloc(1, DstSize);

		if (!pDstData)
			goto fail;

		(void)sprintf_s(pDstData, DstSize,
		                "Version:0.9\r\n"
		                "StartHTML:0000000000\r\n"
		                "EndHTML:0000000000\r\n"
		                "StartFragment:0000000000\r\n"
		                "EndFragment:0000000000\r\n");
		body = strstr(pSrcData.cpc, "<body");

		if (!body)
			body = strstr(pSrcData.cpc, "<BODY");

		/* StartHTML */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, DstSize));
		CopyMemory(&pDstData[23], num, 10);

		if (!body)
		{
			if (!winpr_str_append("<HTML><BODY>", pDstData, DstSize, nullptr))
				goto fail;
		}

		if (!winpr_str_append("<!--StartFragment-->", pDstData, DstSize, nullptr))
			goto fail;

		/* StartFragment */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, SrcSize + 200));
		CopyMemory(&pDstData[69], num, 10);

		if (!winpr_str_append(pSrcData.cpc, pDstData, DstSize, nullptr))
			goto fail;

		/* EndFragment */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, SrcSize + 200));
		CopyMemory(&pDstData[93], num, 10);

		if (!winpr_str_append("<!--EndFragment-->", pDstData, DstSize, nullptr))
			goto fail;

		if (!body)
		{
			if (!winpr_str_append("</BODY></HTML>", pDstData, DstSize, nullptr))
				goto fail;
		}

		/* EndHTML */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, DstSize));
		CopyMemory(&pDstData[43], num, 10);
		*pSize = (UINT32)strnlen(pDstData, DstSize) + 1;
	}
fail:
	free(pSrcData.pv);
	return pDstData;
}

WINPR_ATTR_MALLOC(free, 1)
static char* html_pre_write(wStream* s, const char* what)
{
	const size_t len = strlen(what);
	Stream_Write(s, what, len);
	char* startHTML = Stream_PointerAs(s, char);
	for (size_t x = 0; x < 10; x++)
		Stream_Write_INT8(s, '0');
	Stream_Write(s, "\r\n", 2);
	return startHTML;
}

static void html_fill_number(char* pos, size_t val)
{
	char str[11] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(str, sizeof(str), "%010" PRIuz, val);
	memcpy(pos, str, 10);
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_wrap_html(const char* mime, const char* idata, size_t ilength,
                                 uint32_t* plen)
{
	WINPR_ASSERT(mime);
	WINPR_ASSERT(plen);

	*plen = 0;

	size_t b64len = 0;
	char* b64 = b64_encode((const BYTE*)idata, ilength, &b64len);
	if (!b64)
		return nullptr;

	const size_t mimelen = strlen(mime);
	wStream* s = Stream_New(nullptr, b64len + 225 + mimelen);
	if (!s)
	{
		free(b64);
		return nullptr;
	}

	char* startHTML = html_pre_write(s, "Version:0.9\r\nStartHTML:");
	char* endHTML = html_pre_write(s, "EndHTML:");
	char* startFragment = html_pre_write(s, "StartFragment:");
	char* endFragment = html_pre_write(s, "EndFragment:");

	html_fill_number(startHTML, Stream_GetPosition(s));
	const char html[] = "<html><!--StartFragment-->";
	Stream_Write(s, html, strnlen(html, sizeof(html)));

	html_fill_number(startFragment, Stream_GetPosition(s));

	const char body[] = "<body><img alt=\"FreeRDP clipboard image\" src=\"data:";
	Stream_Write(s, body, strnlen(body, sizeof(body)));

	Stream_Write(s, mime, mimelen);

	const char base64[] = ";base64,";
	Stream_Write(s, base64, strnlen(base64, sizeof(base64)));
	Stream_Write(s, b64, b64len);

	const char end[] = "\"/></body>";
	Stream_Write(s, end, strnlen(end, sizeof(end)));

	html_fill_number(endFragment, Stream_GetPosition(s));

	const char fragend[] = "<!--EndFragment--></html>";
	Stream_Write(s, fragend, strnlen(fragend, sizeof(fragend)));
	html_fill_number(endHTML, Stream_GetPosition(s));

	void* res = Stream_Buffer(s);
	const size_t pos = Stream_GetPosition(s);
	*plen = WINPR_ASSERTING_INT_CAST(uint32_t, pos);
	Stream_Free(s, FALSE);
	free(b64);
	return res;
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_wrap_format_to_html(uint32_t bmpFormat, const char* idata, size_t ilength,
                                           uint32_t* plen)
{
	void* res = nullptr;
	wImage* img = winpr_image_new();
	if (!img)
		goto fail;

	if (winpr_image_read_buffer(img, (const BYTE*)idata, ilength) <= 0)
		goto fail;

	{
		size_t bmpsize = 0;
		void* bmp = winpr_image_write_buffer(img, bmpFormat, &bmpsize);
		if (!bmp)
			goto fail;

		res = clipboard_wrap_html(winpr_image_format_mime(bmpFormat), bmp, bmpsize, plen);
		free(bmp);
	}
fail:
	winpr_image_free(img, TRUE);
	return res;
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_wrap_bmp_to_ms_html(const char* idata, size_t ilength, uint32_t* plen)
{
	const uint32_t formats[] = { WINPR_IMAGE_WEBP, WINPR_IMAGE_PNG, WINPR_IMAGE_JPEG };

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		const uint32_t format = formats[x];
		if (winpr_image_format_is_supported(format))
		{
			return clipboard_wrap_format_to_html(format, idata, ilength, plen);
		}
	}
	const uint32_t bmpFormat = WINPR_IMAGE_BITMAP;
	return clipboard_wrap_html(winpr_image_format_mime(bmpFormat), idata, ilength, plen);
}

WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_image_ms_html(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                                UINT32 formatId, const void* data, UINT32* pSize)
{
	if (formatId != ClipboardGetFormatId(clipboard, mime_ms_html))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, formatId), formatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}

	WINPR_ASSERT(pSize);

	const size_t datalen = *pSize;

	switch (clipboard->formatId)
	{
		case CF_TIFF:
			return clipboard_wrap_html(mime_tiff, data, datalen, pSize);
		case CF_DIB:
		case CF_DIBV5:
		{
			uint32_t bmplen = *pSize;
			void* bmp =
			    clipboard_synthesize_image_bmp(clipboard, clipboard->formatId, data, &bmplen);
			if (!bmp)
			{
				WLog_WARN(TAG, "failed to convert formatId 0x%08" PRIx32 " [%s]",
				          clipboard->formatId,
				          ClipboardGetFormatName(clipboard, clipboard->formatId));
				*pSize = 0;
				return nullptr;
			}

			void* res = clipboard_wrap_bmp_to_ms_html(bmp, bmplen, pSize);
			free(bmp);
			return res;
		}
		default:
		{
#if defined(WINPR_UTILS_IMAGE_WEBP)
			const uint32_t idWebp = ClipboardRegisterFormat(clipboard, mime_webp);
			if (clipboard->formatId == idWebp)
				return clipboard_wrap_html(mime_webp, data, datalen, pSize);
#endif

#if defined(WINPR_UTILS_IMAGE_PNG)
			const uint32_t idPng = ClipboardRegisterFormat(clipboard, mime_png);
			if (clipboard->formatId == idPng)
				return clipboard_wrap_html(mime_png, data, datalen, pSize);
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
			const uint32_t idJpeg = ClipboardRegisterFormat(clipboard, mime_jpeg);
			if (clipboard->formatId == idJpeg)
				return clipboard_wrap_html(mime_jpeg, data, datalen, pSize);
#endif

			const uint32_t idTiff = ClipboardRegisterFormat(clipboard, mime_tiff);
			if (clipboard->formatId == idTiff)
				return clipboard_wrap_html(mime_tiff, data, datalen, pSize);

			for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
			{
				const char* mime = mime_bitmap[x];
				const uint32_t id = ClipboardRegisterFormat(clipboard, mime);

				if (formatId == id)
					return clipboard_wrap_bmp_to_ms_html(data, datalen, pSize);
			}

			WLog_WARN(TAG, "Unsupported image format id 0x%08" PRIx32 " [%s]", clipboard->formatId,
			          ClipboardGetFormatName(clipboard, clipboard->formatId));
			*pSize = 0;
			return nullptr;
		}
	}
}

/**
 * "text/html":
 *
 * HTML text format.
 */
WINPR_ATTR_MALLOC(free, 1)
static void* clipboard_synthesize_html_format(wClipboard* clipboard, UINT32 formatId,
                                              const void* data, UINT32* pSize)
{
	if (formatId != ClipboardGetFormatId(clipboard, mime_html))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, formatId), formatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}
	if (clipboard->formatId != ClipboardGetFormatId(clipboard, mime_ms_html))
	{
		WLog_ERR(TAG,
		         "Unuspported destination format %s [0x%04" PRIx32
		         "], trying to convert from %s [0x%04" PRIx32 "]",
		         ClipboardGetFormatName(clipboard, formatId), formatId,
		         ClipboardGetFormatName(clipboard, clipboard->formatId), clipboard->formatId);
		return nullptr;
	}

	char* pDstData = nullptr;

	if (formatId == ClipboardGetFormatId(clipboard, mime_ms_html))
	{
		const char* str = (const char*)data;
		const size_t SrcSize = *pSize;
		const char* begStr = strstr(str, "StartHTML:");
		const char* endStr = strstr(str, "EndHTML:");

		if (!begStr || !endStr)
			return nullptr;

		errno = 0;
		const long beg = strtol(&begStr[10], nullptr, 10);

		if (errno != 0)
			return nullptr;

		const long end = strtol(&endStr[8], nullptr, 10);

		if ((beg < 0) || (end < 0) || ((size_t)beg > SrcSize) || ((size_t)end > SrcSize) ||
		    (beg >= end) || (errno != 0))
			return nullptr;

		const size_t DstSize = (size_t)(end - beg);
		pDstData = calloc(DstSize + 1, sizeof(char));

		if (!pDstData)
			return nullptr;

		CopyMemory(pDstData, &str[beg], DstSize);
		const size_t rc = ConvertLineEndingToLF(pDstData, DstSize);
		WINPR_ASSERT(rc <= UINT32_MAX);
		*pSize = (UINT32)rc;
	}

	return pDstData;
}

BOOL ClipboardInitSynthesizers(wClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	const UINT32 formatIdUtf = ClipboardRegisterFormat(clipboard, mime_text_utf8);
	const UINT32 formatIdUtf8String = ClipboardRegisterFormat(clipboard, mime_text_UTF8_STRING);
	const UINT32 formatIdPlain = ClipboardRegisterFormat(clipboard, mime_text_plain);
	const UINT32 textFormatIds[] = { CF_TEXT,     CF_OEMTEXT,         CF_UNICODETEXT,
		                             formatIdUtf, formatIdUtf8String, formatIdPlain };
	/**
	 * CF_TEXT
	 */
	for (size_t x = 0; x < ARRAYSIZE(textFormatIds); x++)
	{
		const UINT32 formatId = textFormatIds[x];

		for (size_t y = 0; y < ARRAYSIZE(textFormatIds); y++)
		{
			const UINT32 dstFormatId = textFormatIds[y];
			if (formatId == dstFormatId)
				continue;

			if (!ClipboardRegisterSynthesizerEx(clipboard, formatId, dstFormatId,
			                                    clipboard_synthesize_string))
				return FALSE;
		}

		if (!ClipboardRegisterSynthesizerEx(clipboard, formatId, CF_LOCALE,
		                                    clipboard_synthesize_cf_locale))
			return FALSE;
	}

	const uint32_t msHtmlFormat = ClipboardRegisterFormat(clipboard, mime_ms_html);

	/**
	 * CF_TIFF
	 */
	if (!ClipboardRegisterSynthesizerEx(clipboard, CF_TIFF, msHtmlFormat,
	                                    clipboard_synthesize_image_ms_html))
		return FALSE;

	/**
	 * CF_DIB / CF_DIBv5
	 */
	if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIBV5, CF_DIB,
	                                    clipboard_synthesize_image_dib_to_format))
		return FALSE;
	if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIB, msHtmlFormat,
	                                    clipboard_synthesize_image_ms_html))
		return FALSE;

#if defined(WINPR_UTILS_IMAGE_DIBv5)
	if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIBV5, CF_DIB,
	                                    clipboard_synthesize_image_dib_to_format))
		return FALSE;
	if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIBV5, msHtmlFormat,
	                                    clipboard_synthesize_image_ms_html))
		return FALSE;
#endif

	/**
	 * image/
	 */
	for (size_t x = 0; x < ARRAYSIZE(mime_images); x++)
	{
		const char* mime = mime_images[x];
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime);
		if (altFormatId == 0)
			continue;
		if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIB, altFormatId,
		                                    clipboard_synthesize_image_dib_to_format))
			return FALSE;
		if (!ClipboardRegisterSynthesizerEx(clipboard, altFormatId, CF_DIB,
		                                    clipboard_synthesize_image_format_to_cf_dib))
			return FALSE;
		if (!ClipboardRegisterSynthesizerEx(clipboard, altFormatId, msHtmlFormat,
		                                    clipboard_synthesize_image_ms_html))
			return FALSE;
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		if (!ClipboardRegisterSynthesizerEx(clipboard, CF_DIBV5, altFormatId,
		                                    clipboard_synthesize_image_dib_to_format))
			return FALSE;
		if (!ClipboardRegisterSynthesizerEx(clipboard, altFormatId, CF_DIBV5,
		                                    clipboard_synthesize_image_format_to_cf_dib))
			return FALSE;
#endif
	}

	for (size_t x = 0; x < ARRAYSIZE(mime_images); x++)
	{
		const char* mime = mime_images[x];
		if (strcmp(mime, mime_tiff) == 0) /* WinPR can not read or write TIFF */
			continue;
		const UINT32 formatId = ClipboardRegisterFormat(clipboard, mime);
		for (size_t y = 0; y < ARRAYSIZE(mime_images); y++)
		{
			const char* altMime = mime_images[y];
			if ((x == y) || (strcmp(altMime, mime_tiff) == 0))
				continue;
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, altMime);
			if ((formatId == 0) || (altFormatId == 0))
				continue;
			if (!ClipboardRegisterSynthesizerEx(clipboard, formatId, altFormatId,
			                                    clipboard_synthesize_image_format_to_format))
				return FALSE;
		}
	}

	/**
	 * HTML Format
	 */
	if (msHtmlFormat)
	{
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_html);
		if (!ClipboardRegisterSynthesizerEx(clipboard, msHtmlFormat, altFormatId,
		                                    clipboard_synthesize_html_format))
			return FALSE;
	}

	/**
	 * text/html
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, mime_html);

		if (formatId)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_ms_html);
			if (!ClipboardRegisterSynthesizerEx(clipboard, formatId, altFormatId,
			                                    clipboard_synthesize_ms_html_format))
				return FALSE;
		}
	}

	return TRUE;
}
