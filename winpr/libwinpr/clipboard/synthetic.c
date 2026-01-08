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

#include "../log.h"
#define TAG WINPR_TAG("clipboard.synthetic")

static const char mime_html[] = "text/html";
static const char mime_ms_html[] = "HTML Format";
static const char* mime_bitmap[] = { "image/bmp", "image/x-bmp", "image/x-MS-bmp",
	                                 "image/x-win-bitmap" };

static const char mime_webp[] = "image/webp";
static const char mime_png[] = "image/png";
static const char mime_jpeg[] = "image/jpeg";
static const char mime_tiff[] = "image/tiff";

static const BYTE enc_base64url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
		return NULL;

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

		if (!str || (size > UINT32_MAX))
		{
			free(str);
			return NULL;
		}

		pDstData = ConvertLineEndingToCRLF(str, &size);
		free(str);
		*pSize = (UINT32)size;
		return pDstData;
	}
	else if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
	         (formatId == ClipboardGetFormatId(clipboard, mime_text_plain)))
	{
		size = *pSize;
		pDstData = ConvertLineEndingToCRLF(data, &size);

		if (!pDstData || (size > *pSize))
		{
			free(pDstData);
			return NULL;
		}

		*pSize = (UINT32)size;
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

static void* clipboard_synthesize_cf_locale(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                            WINPR_ATTR_UNUSED UINT32 formatId,
                                            WINPR_ATTR_UNUSED const void* data,
                                            WINPR_ATTR_UNUSED UINT32* pSize)
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

		if ((len < 1) || ((len + 1) > UINT32_MAX / sizeof(WCHAR)))
		{
			free(pDstData);
			return NULL;
		}

		const size_t slen = (len + 1) * sizeof(WCHAR);
		*pSize = (UINT32)slen;
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

#if defined(WINPR_UTILS_IMAGE_DIBv5)
	if (formatId == CF_DIBV5)
	{
		WLog_WARN(TAG, "[DIB] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
	}
	else
#endif
	    if (is_format_bitmap(clipboard, formatId))
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
	else
	{
		WLog_WARN(TAG, "[DIB] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
	}

	return NULL;
}

/**
 * "CF_DIBV5":
 *
 * BITMAPV5HEADER structure followed by the bitmap color space information and the bitmap bits.
 */
#if defined(WINPR_UTILS_IMAGE_DIBv5)
static void* clipboard_synthesize_cf_dibv5(wClipboard* clipboard, UINT32 formatId,
                                           WINPR_ATTR_UNUSED const void* data,
                                           WINPR_ATTR_UNUSED UINT32* pSize)
{
	if (formatId == CF_DIB)
	{
		WLog_WARN(TAG, "[DIBv5] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
	}
	else if (is_format_bitmap(clipboard, formatId))
	{
		WLog_WARN(TAG, "[DIBv5] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
	}
	else
	{
		BOOL handled = FALSE;
#if defined(WINPR_UTILS_IMAGE_PNG)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_png);
			if (formatId == altFormatId)
			{
			}
		}
#endif
#if defined(WINPR_UTILS_IMAGE_JPEG)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_jpeg);
			if (formatId == altFormatId)
			{
			}
		}
#endif
		if (!handled)
		{
			WLog_WARN(TAG, "[DIBv5] Unsupported destination format %s",
			          ClipboardGetFormatName(clipboard, formatId));
		}
	}

	return NULL;
}
#endif

static void* clipboard_prepend_bmp_header(const WINPR_BITMAP_INFO_HEADER* pInfoHeader,
                                          const void* data, size_t size, UINT32* pSize)
{
	WINPR_ASSERT(pInfoHeader);
	WINPR_ASSERT(pSize);

	*pSize = 0;
	if ((pInfoHeader->biBitCount < 1) || (pInfoHeader->biBitCount > 32))
		return NULL;

	const size_t DstSize = sizeof(WINPR_BITMAP_FILE_HEADER) + size;
	if (DstSize > UINT32_MAX)
		return NULL;

	wStream* s = Stream_New(NULL, DstSize);
	if (!s)
		return NULL;

	WINPR_BITMAP_FILE_HEADER fileHeader = { 0 };
	fileHeader.bfType[0] = 'B';
	fileHeader.bfType[1] = 'M';
	fileHeader.bfSize = (UINT32)DstSize;
	fileHeader.bfOffBits = sizeof(WINPR_BITMAP_FILE_HEADER) + sizeof(WINPR_BITMAP_INFO_HEADER);
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
	return NULL;
}

/**
 * "image/bmp":
 *
 * Bitmap file format.
 */

static void* clipboard_synthesize_image_bmp(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                            UINT32 formatId, const void* data, UINT32* pSize)
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
#if defined(WINPR_UTILS_IMAGE_DIBv5)
	else if (formatId == CF_DIBV5)
	{
		WLog_WARN(TAG, "[BMP] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
	}
#endif
	else
	{
		WLog_WARN(TAG, "[BMP] Unsupported destination format %s",
		          ClipboardGetFormatName(clipboard, formatId));
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
	{
		if (dsize <= UINT32_MAX)
			*pSize = (UINT32)dsize;
		else
		{
			free(result);
			result = NULL;
		}
	}

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
#endif

#if defined(WINPR_UTILS_IMAGE_PNG) || defined(WINPR_UTILS_IMAGE_WEBP) || \
    defined(WINPR_UTILS_IMAGE_JPEG)
static void* clipboard_synthesize_image_format_to_bmp(wClipboard* clipboard,
                                                      WINPR_ATTR_UNUSED UINT32 srcFormatId,
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
#endif

#if defined(WINPR_UTILS_IMAGE_PNG)
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

	if (formatId == ClipboardGetFormatId(clipboard, mime_html))
	{
		const size_t SrcSize = (size_t)*pSize;
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
	char str[11] = { 0 };
	(void)_snprintf(str, sizeof(str), "%010" PRIuz, val);
	memcpy(pos, str, 10);
}

static void* clipboard_wrap_html(const char* mime, const char* idata, size_t ilength,
                                 uint32_t* plen)
{
	WINPR_ASSERT(mime);
	WINPR_ASSERT(plen);

	*plen = 0;

	size_t b64len = 0;
	char* b64 = b64_encode((const BYTE*)idata, ilength, &b64len);
	if (!b64)
		return NULL;

	const size_t mimelen = strlen(mime);
	wStream* s = Stream_New(NULL, b64len + 225 + mimelen);
	if (!s)
	{
		free(b64);
		return NULL;
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

static void* clipboard_wrap_format_to_html(uint32_t bmpFormat, const char* idata, size_t ilength,
                                           uint32_t* plen)
{
	void* res = NULL;
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

static void* clipboard_wrap_bmp_to_html(const char* idata, size_t ilength, uint32_t* plen)
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

static void* clipboard_synthesize_image_html(WINPR_ATTR_UNUSED wClipboard* clipboard,
                                             UINT32 formatId, const void* data, UINT32* pSize)
{
	WINPR_ASSERT(pSize);

	const size_t datalen = *pSize;

	switch (formatId)
	{
		case CF_TIFF:
			return clipboard_wrap_html(mime_tiff, data, datalen, pSize);
		case CF_DIB:
		case CF_DIBV5:
		{
			uint32_t bmplen = *pSize;
			void* bmp = clipboard_synthesize_image_bmp(clipboard, formatId, data, &bmplen);
			if (!bmp)
			{
				WLog_WARN(TAG, "failed to convert formatId 0x%08" PRIx32 " [%s]", formatId,
				          ClipboardGetFormatName(clipboard, formatId));
				*pSize = 0;
				return NULL;
			}

			void* res = clipboard_wrap_bmp_to_html(bmp, bmplen, pSize);
			free(bmp);
			return res;
		}
		default:
		{
			const uint32_t idWebp = ClipboardRegisterFormat(clipboard, mime_webp);
			const uint32_t idPng = ClipboardRegisterFormat(clipboard, mime_png);
			const uint32_t idJpeg = ClipboardRegisterFormat(clipboard, mime_jpeg);
			const uint32_t idTiff = ClipboardRegisterFormat(clipboard, mime_tiff);
			if (formatId == idWebp)
			{
				return clipboard_wrap_html(mime_webp, data, datalen, pSize);
			}
			else if (formatId == idPng)
			{
				return clipboard_wrap_html(mime_png, data, datalen, pSize);
			}
			else if (formatId == idJpeg)
			{
				return clipboard_wrap_html(mime_jpeg, data, datalen, pSize);
			}
			else if (formatId == idTiff)
			{
				return clipboard_wrap_html(mime_tiff, data, datalen, pSize);
			}
			else
			{
				for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
				{
					const char* mime = mime_bitmap[x];
					const uint32_t id = ClipboardRegisterFormat(clipboard, mime);

					if (formatId == id)
						return clipboard_wrap_bmp_to_html(data, datalen, pSize);
				}
			}

			WLog_WARN(TAG, "Unsupported image format id 0x%08" PRIx32 " [%s]", formatId,
			          ClipboardGetFormatName(clipboard, formatId));
			*pSize = 0;
			return NULL;
		}
	}
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

	if (formatId == ClipboardGetFormatId(clipboard, mime_ms_html))
	{
		const char* str = (const char*)data;
		const size_t SrcSize = *pSize;
		const char* begStr = strstr(str, "StartHTML:");
		const char* endStr = strstr(str, "EndHTML:");

		if (!begStr || !endStr)
			return NULL;

		errno = 0;
		const long beg = strtol(&begStr[10], NULL, 10);

		if (errno != 0)
			return NULL;

		const long end = strtol(&endStr[8], NULL, 10);

		if ((beg < 0) || (end < 0) || ((size_t)beg > SrcSize) || ((size_t)end > SrcSize) ||
		    (beg >= end) || (errno != 0))
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

	const uint32_t htmlFormat = ClipboardRegisterFormat(clipboard, mime_ms_html);
	const uint32_t tiffFormat = ClipboardRegisterFormat(clipboard, mime_tiff);

	/**
	 * CF_TIFF
	 */
	ClipboardRegisterSynthesizer(clipboard, CF_TIFF, htmlFormat, clipboard_synthesize_image_html);
	ClipboardRegisterSynthesizer(clipboard, tiffFormat, htmlFormat,
	                             clipboard_synthesize_image_html);

	/**
	 * CF_DIB
	 */
	{
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, CF_DIBV5, clipboard_synthesize_cf_dibv5);
#endif
		for (size_t x = 0; x < ARRAYSIZE(mime_bitmap); x++)
		{
			const char* mime = mime_bitmap[x];
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime);
			if (altFormatId == 0)
				continue;
			ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
			                             clipboard_synthesize_image_bmp);
		}
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, htmlFormat,
		                             clipboard_synthesize_image_html);
	}

	/**
	 * CF_DIBV5
	 */
#if defined(WINPR_UTILS_IMAGE_DIBv5)
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
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, htmlFormat,
		                             clipboard_synthesize_image_html);
	}
#endif

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
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_cf_dibv5);
#endif
		ClipboardRegisterSynthesizer(clipboard, altFormatId, htmlFormat,
		                             clipboard_synthesize_image_html);
	}

	/**
	 * image/png
	 */
#if defined(WINPR_UTILS_IMAGE_PNG)
	{
		const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_png);
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
		                             clipboard_synthesize_image_bmp_to_png);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_png_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, htmlFormat,
		                             clipboard_synthesize_image_html);
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_bmp_to_png);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_png_to_bmp);
#endif
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
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_webp_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, htmlFormat,
		                             clipboard_synthesize_image_html);
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_bmp_to_webp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_webp_to_bmp);
#endif
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
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIB,
		                             clipboard_synthesize_image_jpeg_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, altFormatId, htmlFormat,
		                             clipboard_synthesize_image_html);
#if defined(WINPR_UTILS_IMAGE_DIBv5)
		ClipboardRegisterSynthesizer(clipboard, altFormatId, CF_DIBV5,
		                             clipboard_synthesize_image_jpeg_to_bmp);
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
		                             clipboard_synthesize_image_bmp_to_jpeg);
#endif
	}
#endif

	/**
	 * HTML Format
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, mime_ms_html);

		if (formatId)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_html);
			ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
			                             clipboard_synthesize_text_html);
		}
	}

	/**
	 * text/html
	 */
	{
		UINT32 formatId = ClipboardRegisterFormat(clipboard, mime_html);

		if (formatId)
		{
			const UINT32 altFormatId = ClipboardRegisterFormat(clipboard, mime_ms_html);
			ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
			                             clipboard_synthesize_html_format);
		}
	}

	return TRUE;
}
