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

static const char* mime_bitmap[] = { "image/bmp", "image/x-bmp", "image/x-MS-bmp",
	                                 "image/x-win-bitmap" };

#if defined(WINPR_UTILS_IMAGE_WEBP)
static const char mime_webp[] = "image/webp";
#endif
#if defined(WINPR_UTILS_IMAGE_PNG)
static const char mime_png[] = "image/png";
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
static const char mime_jpeg[] = "image/jpeg";
#endif
/**
 * Standard Clipboard Formats:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ff729168/
 */

/**
 * "CF_TEXT":
 *
 * Null-terminated ANSI text with CR/LF line endings.
 */

static void* clipboard_synthesize_cf_text(wClipboard* clipboard, UINT32 formatId, const void* data,
                                          UINT32* pSize)
{
	size_t size = 0;
	char* pDstData = NULL;

	if (formatId == CF_UNICODETEXT)
	{
		char* str = ConvertWCharNToUtf8Alloc(data, *pSize / sizeof(WCHAR), &size);

		if (!str)
			return NULL;

		pDstData = ConvertLineEndingToCRLF(str, &size);
		free(str);
		*pSize = size;
		return pDstData;
	}
	else if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
	         (formatId == ClipboardGetFormatId(clipboard, mime_text_plain)))
	{
		size = *pSize;
		pDstData = ConvertLineEndingToCRLF(data, &size);

		if (!pDstData)
			return NULL;

		*pSize = size;
		return pDstData;
	}

	return NULL;
}

/**
 * "CF_OEMTEXT":
 *
 * Null-terminated OEM text with CR/LF line endings.
 */

static void* clipboard_synthesize_cf_oemtext(wClipboard* clipboard, UINT32 formatId,
                                             const void* data, UINT32* pSize)
{
	return clipboard_synthesize_cf_text(clipboard, formatId, data, pSize);
}

/**
 * "CF_LOCALE":
 *
 * System locale identifier associated with CF_TEXT
 */

static void* clipboard_synthesize_cf_locale(wClipboard* clipboard, UINT32 formatId,
                                            const void* data, UINT32* pSize)
{
	UINT32* pDstData = NULL;
	pDstData = (UINT32*)malloc(sizeof(UINT32));

	if (!pDstData)
		return NULL;

	*pDstData = 0x0409; /* English - United States */
	return (void*)pDstData;
}

/**
 * "CF_UNICODETEXT":
 *
 * Null-terminated UTF-16 text with CR/LF line endings.
 */

static void* clipboard_synthesize_cf_unicodetext(wClipboard* clipboard, UINT32 formatId,
                                                 const void* data, UINT32* pSize)
{
	size_t size = 0;
	char* crlfStr = NULL;
	WCHAR* pDstData = NULL;

	if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
	    (formatId == ClipboardGetFormatId(clipboard, mime_text_plain)))
	{
		size_t len = 0;
		if (!pSize || (*pSize > INT32_MAX))
			return NULL;

		size = *pSize;
		crlfStr = ConvertLineEndingToCRLF((const char*)data, &size);

		if (!crlfStr)
			return NULL;

		pDstData = ConvertUtf8NToWCharAlloc(crlfStr, size, &len);
		free(crlfStr);

		if ((len < 1) || (len > UINT32_MAX / sizeof(WCHAR)))
		{
			free(pDstData);
			return NULL;
		}

		*pSize = (len + 1) * sizeof(WCHAR);
	}

	return (void*)pDstData;
}

/**
 * mime_utf8_string:
 *
 * Null-terminated UTF-8 string with LF line endings.
 */

static void* clipboard_synthesize_utf8_string(wClipboard* clipboard, UINT32 formatId,
                                              const void* data, UINT32* pSize)
{
	if (formatId == CF_UNICODETEXT)
	{
		size_t size = 0;
		char* pDstData = ConvertWCharNToUtf8Alloc(data, *pSize / sizeof(WCHAR), &size);

		if (!pDstData)
			return NULL;

		const size_t rc = ConvertLineEndingToLF(pDstData, size);
		WINPR_ASSERT(rc <= UINT32_MAX);
		*pSize = (UINT32)rc;
		return pDstData;
	}
	else if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
	         (formatId == ClipboardGetFormatId(clipboard, mime_text_plain)))
	{
		const size_t size = *pSize;
		char* pDstData = calloc(size + 1, sizeof(char));

		if (!pDstData)
			return NULL;

		CopyMemory(pDstData, data, size);
		const size_t rc = ConvertLineEndingToLF(pDstData, size);
		WINPR_ASSERT(rc <= UINT32_MAX);
		*pSize = (UINT32)rc;
		return pDstData;
	}

	return NULL;
}

static BOOL is_format_bitmap(wClipboard* clipboard, UINT32 formatId)
{
	for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
	{
		const char* mime = mime_bitmap[x];
		const UINT32 altFormatId = ClipboardGetFormatId(clipboard, mime);
		if (altFormatId == formatId)
			return TRUE;
	}

	return FALSE;
}

/**
 * "CF_DIB":
 *
 * BITMAPINFO structure followed by the bitmap bits.
 */

static void* clipboard_synthesize_cf_dib(wClipboard* clipboard, UINT32 formatId, const void* data,
                                         UINT32* pSize)
{
	UINT32 SrcSize = 0;
	UINT32 DstSize = 0;
	BYTE* pDstData = NULL;
	SrcSize = *pSize;

	if (formatId == CF_DIBV5)
	{
	}
	else if (is_format_bitmap(clipboard, formatId))
	{
		WINPR_BITMAP_FILE_HEADER pFileHeader = { 0 };
		wStream sbuffer = { 0 };
		wStream* s = Stream_StaticConstInit(&sbuffer, data, SrcSize);
		if (!readBitmapFileHeader(s, &pFileHeader))
			return NULL;

		DstSize = SrcSize - sizeof(BITMAPFILEHEADER);
		pDstData = (BYTE*)malloc(DstSize);

		if (!pDstData)
			return NULL;

		data = (const void*)&((const BYTE*)data)[sizeof(BITMAPFILEHEADER)];
		CopyMemory(pDstData, data, DstSize);
		*pSize = DstSize;
		return pDstData;
	}

	return NULL;
}

/**
 * "CF_DIBV5":
 *
 * BITMAPV5HEADER structure followed by the bitmap color space information and the bitmap bits.
 */

static void* clipboard_synthesize_cf_dibv5(wClipboard* clipboard, UINT32 formatId, const void* data,
                                           UINT32* pSize)
{
	if (formatId == CF_DIB)
	{
	}
	else if (is_format_bitmap(clipboard, formatId))
	{
	}

	return NULL;
}

static void* clipboard_prepend_bmp_header(const WINPR_BITMAP_INFO_HEADER* pInfoHeader,
                                          const void* data, size_t size, UINT32* pSize)
{
	WINPR_ASSERT(pInfoHeader);
	WINPR_ASSERT(pSize);

	*pSize = 0;
	if ((pInfoHeader->biBitCount < 1) || (pInfoHeader->biBitCount > 32))
		return NULL;

	const size_t DstSize = sizeof(WINPR_BITMAP_FILE_HEADER) + size;
	wStream* s = Stream_New(NULL, DstSize);
	if (!s)
		return NULL;

	WINPR_BITMAP_FILE_HEADER fileHeader = { 0 };
	fileHeader.bfType[0] = 'B';
	fileHeader.bfType[1] = 'M';
	fileHeader.bfSize = DstSize;
	fileHeader.bfOffBits = sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(WINPR_BITMAP_INFO_HEADER);
	if (!writeBitmapFileHeader(s, &fileHeader))
		goto fail;

	if (!Stream_EnsureRemainingCapacity(s, size))
		goto fail;
	Stream_Write(s, data, size);
	const size_t len = Stream_GetPosition(s);
	if (len != DstSize)
		goto fail;
	*pSize = DstSize;

	BYTE* dst = Stream_Buffer(s);
	Stream_Free(s, FALSE);
	return dst;

fail:
	Stream_Free(s, TRUE);
	return NULL;
}

/**
 * "image/bmp":
 *
 * Bitmap file format.
 */

static void* clipboard_synthesize_image_bmp(wClipboard* clipboard, UINT32 formatId,
                                            const void* data, UINT32* pSize)
{
	UINT32 SrcSize = *pSize;

	if (formatId == CF_DIB)
	{
		if (SrcSize < sizeof(BITMAPINFOHEADER))
			return NULL;

		wStream sbuffer = { 0 };
		size_t offset = 0;
		WINPR_BITMAP_INFO_HEADER header = { 0 };
		wStream* s = Stream_StaticConstInit(&sbuffer, data, SrcSize);
		if (!readBitmapInfoHeader(s, &header, &offset))
			return NULL;

		return clipboard_prepend_bmp_header(&header, data, SrcSize, pSize);
	}
	else if (formatId == CF_DIBV5)
	{
	}

	return NULL;
}

#if defined(WINPR_UTILS_IMAGE_PNG) || defined(WINPR_UTILS_IMAGE_WEBP) || \
    defined(WINPR_UTILS_IMAGE_JPEG)
static void* clipboard_synthesize_image_bmp_to_format(wClipboard* clipboard, UINT32 formatId,
                                                      UINT32 bmpFormat, const void* data,
                                                      UINT32* pSize)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);
	WINPR_ASSERT(pSize);

	size_t dsize = 0;
	void* result = NULL;

	wImage* img = winpr_image_new();
	void* bmp = clipboard_synthesize_image_bmp(clipboard, formatId, data, pSize);
	const UINT32 SrcSize = *pSize;
	*pSize = 0;

	if (!bmp || !img)
		goto fail;

	if (winpr_image_read_buffer(img, bmp, SrcSize) <= 0)
		goto fail;

	result = winpr_image_write_buffer(img, bmpFormat, &dsize);
	if (result)
		*pSize = dsize;

fail:
	free(bmp);
	winpr_image_free(img, TRUE);
	return result;
}
#endif

#if defined(WINPR_UTILS_IMAGE_PNG)
static void* clipboard_synthesize_image_bmp_to_png(wClipboard* clipboard, UINT32 formatId,
                                                   const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_bmp_to_format(clipboard, formatId, WINPR_IMAGE_PNG, data,
	                                                pSize);
}

static void* clipboard_synthesize_image_format_to_bmp(wClipboard* clipboard, UINT32 srcFormatId,
                                                      const void* data, UINT32* pSize)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data);
	WINPR_ASSERT(pSize);

	BYTE* dst = NULL;
	const UINT32 SrcSize = *pSize;
	size_t size = 0;
	wImage* image = winpr_image_new();
	if (!image)
		goto fail;

	const int res = winpr_image_read_buffer(image, data, SrcSize);
	if (res <= 0)
		goto fail;

	dst = winpr_image_write_buffer(image, WINPR_IMAGE_BITMAP, &size);
	if ((size < sizeof(WINPR_BITMAP_FILE_HEADER)) || (size > UINT32_MAX))
	{
		free(dst);
		dst = NULL;
		goto fail;
	}
	*pSize = (UINT32)size;

fail:
	winpr_image_free(image, TRUE);

	if (dst)
		memmove(dst, &dst[sizeof(WINPR_BITMAP_FILE_HEADER)],
		        size - sizeof(WINPR_BITMAP_FILE_HEADER));
	return dst;
}

static void* clipboard_synthesize_image_png_to_bmp(wClipboard* clipboard, UINT32 formatId,
                                                   const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_format_to_bmp(clipboard, formatId, data, pSize);
}
#endif

#if defined(WINPR_UTILS_IMAGE_WEBP)
static void* clipboard_synthesize_image_bmp_to_webp(wClipboard* clipboard, UINT32 formatId,
                                                    const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_bmp_to_format(clipboard, formatId, WINPR_IMAGE_WEBP, data,
	                                                pSize);
}

static void* clipboard_synthesize_image_webp_to_bmp(wClipboard* clipboard, UINT32 formatId,
                                                    const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_format_to_bmp(clipboard, formatId, data, pSize);
}
#endif

#if defined(WINPR_UTILS_IMAGE_JPEG)
static void* clipboard_synthesize_image_bmp_to_jpeg(wClipboard* clipboard, UINT32 formatId,
                                                    const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_bmp_to_format(clipboard, formatId, WINPR_IMAGE_JPEG, data,
	                                                pSize);
}

static void* clipboard_synthesize_image_jpeg_to_bmp(wClipboard* clipboard, UINT32 formatId,
                                                    const void* data, UINT32* pSize)
{
	return clipboard_synthesize_image_format_to_bmp(clipboard, formatId, data, pSize);
}
#endif

/**
 * "HTML Format":
 *
 * HTML clipboard format: msdn.microsoft.com/en-us/library/windows/desktop/ms649015/
 */

static void* clipboard_synthesize_html_format(wClipboard* clipboard, UINT32 formatId,
                                              const void* pData, UINT32* pSize)
{
	union
	{
		const void* cpv;
		const char* cpc;
		const BYTE* cpb;
		WCHAR* pv;
	} pSrcData;
	char* pDstData = NULL;

	pSrcData.cpv = NULL;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(pSize);

	if (formatId == ClipboardGetFormatId(clipboard, "text/html"))
	{
		const INT64 SrcSize = (INT64)*pSize;
		const size_t DstSize = SrcSize + 200;
		char* body = NULL;
		char num[20] = { 0 };

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
				ByteSwapUnicode(pSrcData.pv, (SrcSize / 2));

			/* Check if we have WCHAR, convert to UTF-8 */
			if ((pSrcData.cpb[0] == 0xFF) && (pSrcData.cpb[1] == 0xFE))
			{
				char* utfString =
				    ConvertWCharNToUtf8Alloc(&pSrcData.pv[1], SrcSize / sizeof(WCHAR), NULL);
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
			if (!winpr_str_append("<HTML><BODY>", pDstData, DstSize, NULL))
				goto fail;
		}

		if (!winpr_str_append("<!--StartFragment-->", pDstData, DstSize, NULL))
			goto fail;

		/* StartFragment */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, SrcSize + 200));
		CopyMemory(&pDstData[69], num, 10);

		if (!winpr_str_append(pSrcData.cpc, pDstData, DstSize, NULL))
			goto fail;

		/* EndFragment */
		(void)sprintf_s(num, sizeof(num), "%010" PRIuz "", strnlen(pDstData, SrcSize + 200));
		CopyMemory(&pDstData[93], num, 10);

		if (!winpr_str_append("<!--EndFragment-->", pDstData, DstSize, NULL))
			goto fail;

		if (!body)
		{
			if (!winpr_str_append("</BODY></HTML>", pDstData, DstSize, NULL))
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

/**
 * "text/html":
 *
 * HTML text format.
 */

static void* clipboard_synthesize_text_html(wClipboard* clipboard, UINT32 formatId,
                                            const void* data, UINT32* pSize)
{
	char* pDstData = NULL;

	if (formatId == ClipboardGetFormatId(clipboard, "HTML Format"))
	{
		const char* str = (const char*)data;
		const size_t SrcSize = (INT64)*pSize;
		const char* begStr = strstr(str, "StartHTML:");
		const char* endStr = strstr(str, "EndHTML:");

		if (!begStr || !endStr)
			return NULL;

		errno = 0;
		const long beg = strtol(&begStr[10], NULL, 10);

		if (errno != 0)
			return NULL;

		const long end = strtol(&endStr[8], NULL, 10);

		if (beg < 0 || end < 0 || (beg > SrcSize) || (end > SrcSize) || (beg >= end) ||
		    (errno != 0))
			return NULL;

		const size_t DstSize = (size_t)(end - beg);
		pDstData = calloc(DstSize + 1, sizeof(char));

		if (!pDstData)
			return NULL;

		CopyMemory(pDstData, &str[beg], DstSize);
		const size_t rc = ConvertLineEndingToLF(pDstData, DstSize);
		WINPR_ASSERT(rc <= UINT32_MAX);
		*pSize = (UINT32)rc;
	}

	return pDstData;
}

BOOL ClipboardInitSynthesizers(wClipboard* clipboard)
{
	/**
	 * CF_TEXT
	 */
	{
		ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_OEMTEXT,
		                             clipboard_synthesize_cf_oemtext);
		ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_UNICODETEXT,
		                             clipboard_synthesize_cf_unicodetext);
		ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_LOCALE, clipboard_synthesize_cf_locale);

		UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_text_plain);
		ClipboardRegisterSynthesizer(clipboard, CF_TEXT, altFormatId,
		                             clipboard_synthesize_utf8_string);
	}
	/**
	 * CF_OEMTEXT
	 */
	{
		ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_TEXT, clipboard_synthesize_cf_text);
		ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_UNICODETEXT,
		                             clipboard_synthesize_cf_unicodetext);
		ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_LOCALE,
		                             clipboard_synthesize_cf_locale);
		UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_text_plain);
		ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, altFormatId,
		                             clipboard_synthesize_utf8_string);
	}
	/**
	 * CF_UNICODETEXT
	 */
	{
		ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_TEXT,
		                             clipboard_synthesize_cf_text);
		ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_OEMTEXT,
		                             clipboard_synthesize_cf_oemtext);
		ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_LOCALE,
		                             clipboard_synthesize_cf_locale);
		UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_text_plain);
		ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, altFormatId,
		                             clipboard_synthesize_utf8_string);
	}
	/**
	 * UTF8_STRING
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, mime_text_plain);

		if (formatId)
		{
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_TEXT,
			                             clipboard_synthesize_cf_text);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_OEMTEXT,
			                             clipboard_synthesize_cf_oemtext);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_UNICODETEXT,
			                             clipboard_synthesize_cf_unicodetext);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_LOCALE,
			                             clipboard_synthesize_cf_locale);
		}
	}
	/**
	 * text/plain
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, mime_text_plain);

		if (formatId)
		{
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_TEXT,
			                             clipboard_synthesize_cf_text);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_OEMTEXT,
			                             clipboard_synthesize_cf_oemtext);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_UNICODETEXT,
			                             clipboard_synthesize_cf_unicodetext);
			ClipboardRegisterSynthesizer(clipboard, formatId, CF_LOCALE,
			                             clipboard_synthesize_cf_locale);
		}
	}
	/**
	 * CF_DIB
	 */
	{
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, CF_DIBV5, clipboard_synthesize_cf_dibv5);
		for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
		{
			const char* mime = mime_bitmap[x];
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime);
			if (altFormatId == 0)
				continue;
			ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
			                             clipboard_synthesize_image_bmp);
		}
	}

	/**
	 * CF_DIBV5
	 */

	if (0)
	{
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, CF_DIB, clipboard_synthesize_cf_dib);

		for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
		{
			const char* mime = mime_bitmap[x];
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime);
			if (altFormatId == 0)
				continue;
			ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
			                             clipboard_synthesize_image_bmp);
		}
	}

	/**
	 * image/bmp
	 */
	for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
	{
		const char* mime = mime_bitmap[x];
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime);
		if (altFormatId == 0)
			continue;
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB, clipboard_synthesize_cf_dib);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_cf_dibv5);
	}

	/**
	 * image/png
	 */
#if defined(WINPR_UTILS_IMAGE_PNG)
	{
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_png);
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
		                             clipboard_synthesize_image_bmp_to_png);
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_bmp_to_png);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_png_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_png_to_bmp);
	}
#endif

	/**
	 * image/webp
	 */
#if defined(WINPR_UTILS_IMAGE_WEBP)
	{
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_webp);
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
		                             clipboard_synthesize_image_bmp_to_webp);
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_webp_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_bmp_to_webp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_webp_to_bmp);
	}
#endif

	/**
	 * image/jpeg
	 */
#if defined(WINPR_UTILS_IMAGE_JPEG)
	{
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_jpeg);
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
		                             clipboard_synthesize_image_bmp_to_jpeg);
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_jpeg_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_bmp_to_jpeg);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_jpeg_to_bmp);
	}
#endif

	/**
	 * HTML Format
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, "HTML Format");

		if (formatId)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, "text/html");
			ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
			                             clipboard_synthesize_text_html);
		}
	}

	/**
	 * text/html
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, "text/html");

		if (formatId)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, "HTML Format");
			ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
			                             clipboard_synthesize_html_format);
		}
	}

	return TRUE;
}
