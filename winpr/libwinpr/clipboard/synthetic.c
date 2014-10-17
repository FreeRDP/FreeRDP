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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include "clipboard.h"

/**
 * Standard Clipboard Formats:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ff729168/
 */

/**
 * Synthesized Clipboard Formats
 *
 * Clipboard Format		Conversion Format
 *
 * CF_BITMAP			CF_DIB
 * CF_BITMAP			CF_DIBV5
 * CF_DIB			CF_BITMAP
 * CF_DIB			CF_PALETTE
 * CF_DIB			CF_DIBV5
 * CF_DIBV5			CF_BITMAP
 * CF_DIBV5			CF_DIB
 * CF_DIBV5			CF_PALETTE
 * CF_ENHMETAFILE		CF_METAFILEPICT
 * CF_METAFILEPICT		CF_ENHMETAFILE
 * CF_OEMTEXT			CF_TEXT
 * CF_OEMTEXT			CF_UNICODETEXT
 * CF_TEXT			CF_OEMTEXT
 * CF_TEXT			CF_UNICODETEXT
 * CF_UNICODETEXT		CF_OEMTEXT
 * CF_UNICODETEXT		CF_TEXT
 */

/**
 * "CF_TEXT":
 *
 * Null-terminated ANSI text with CR/LF line endings.
 */

static void* clipboard_synthesize_cf_text(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	int size;
	char* pDstData = NULL;

	if (formatId == CF_UNICODETEXT)
	{
		char* str = NULL;

		size = (int) *pSize;
		size = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) data,
				size / 2, (CHAR**) &str, 0, NULL, NULL);

		if (!str)
			return NULL;

		pDstData = ConvertLineEndingToCRLF((const char*) str, &size);
		free(str);

		*pSize = size;

		return pDstData;
	}
	else if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
			(formatId == ClipboardGetFormatId(clipboard, "UTF8_STRING")) ||
			(formatId == ClipboardGetFormatId(clipboard, "text/plain")) ||
			(formatId == ClipboardGetFormatId(clipboard, "TEXT")) ||
			(formatId == ClipboardGetFormatId(clipboard, "STRING")))
	{
		size = (int) *pSize;
		pDstData = ConvertLineEndingToCRLF((const char*) data, &size);

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

static void* clipboard_synthesize_cf_oemtext(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	return clipboard_synthesize_cf_text(clipboard, formatId, data, pSize);
}

/**
 * "CF_LOCALE":
 *
 * System locale identifier associated with CF_TEXT
 */

static void* clipboard_synthesize_cf_locale(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	UINT32* pDstData = NULL;

	pDstData = (UINT32*) malloc(sizeof(UINT32));
	*pDstData = 0x0409; /* English - United States */

	return (void*) pDstData;
}

/**
 * "CF_UNICODETEXT":
 *
 * Null-terminated UTF-16 text with CR/LF line endings.
 */

static void* clipboard_synthesize_cf_unicodetext(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	int size;
	int status;
	char* crlfStr = NULL;
	WCHAR* pDstData = NULL;

	if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
			(formatId == ClipboardGetFormatId(clipboard, "UTF8_STRING")) ||
			(formatId == ClipboardGetFormatId(clipboard, "text/plain")) ||
			(formatId == ClipboardGetFormatId(clipboard, "TEXT")) ||
			(formatId == ClipboardGetFormatId(clipboard, "STRING")))
	{
		size = (int) *pSize;
		crlfStr = ConvertLineEndingToCRLF((char*) data, &size);

		if (!crlfStr)
			return NULL;

		status = ConvertToUnicode(CP_UTF8, 0, crlfStr, size, &pDstData, 0);
		free(crlfStr);

		if (status <= 0)
			return NULL;

		*pSize = ((status + 1) * 2);
	}

	return (void*) pDstData;
}

/**
 * "UTF8_STRING":
 *
 * Null-terminated UTF-8 string with LF line endings.
 */

static void* clipboard_synthesize_utf8_string(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	int size;
	char* pDstData = NULL;

	if (formatId == CF_UNICODETEXT)
	{
		size = (int) *pSize;
		size = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) data,
				size / 2, (CHAR**) &pDstData, 0, NULL, NULL);

		if (!pDstData)
			return NULL;

		size = ConvertLineEndingToLF(pDstData, size);

		*pSize = size;

		return pDstData;
	}
	else if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) ||
			(formatId == ClipboardGetFormatId(clipboard, "text/plain")) ||
			(formatId == ClipboardGetFormatId(clipboard, "TEXT")) ||
			(formatId == ClipboardGetFormatId(clipboard, "STRING")))
	{
		size = (int) *pSize;
		pDstData = (char*) malloc(size);

		if (!pDstData)
			return NULL;

		CopyMemory(pDstData, data, size);

		size = ConvertLineEndingToLF((char*) pDstData, size);

		*pSize = size;

		return pDstData;
	}

	return NULL;
}

/**
 * "CF_DIB":
 *
 * BITMAPINFO structure followed by the bitmap bits.
 */

static void* clipboard_synthesize_cf_dib(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	/* TODO */

	return NULL;
}

/**
 * "CF_DIBV5":
 *
 * BITMAPV5HEADER structure followed by the bitmap color space information and the bitmap bits.
 */

static void* clipboard_synthesize_cf_dibv5(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	/* TODO */

	return NULL;
}

/**
 * "image/bmp":
 *
 * Bitmap file format.
 */

static void* clipboard_synthesize_image_bmp(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	/* TODO */

	return NULL;
}

/**
 * "HTML Format":
 *
 * HTML clipboard format: msdn.microsoft.com/en-us/library/windows/desktop/ms649015/
 */

static void* clipboard_synthesize_html_format(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	/* TODO */

	return NULL;
}

/**
 * "text/html":
 *
 * HTML text format.
 */

static void* clipboard_synthesize_text_html(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize)
{
	/* TODO */

	return NULL;
}

BOOL ClipboardInitSynthesizers(wClipboard* clipboard)
{
	UINT32 formatId;
	UINT32 altFormatId;

	/**
	 * CF_TEXT
	 */

	ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_OEMTEXT,
			clipboard_synthesize_cf_oemtext);

	ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_UNICODETEXT,
			clipboard_synthesize_cf_unicodetext);

	ClipboardRegisterSynthesizer(clipboard, CF_TEXT, CF_LOCALE,
			clipboard_synthesize_cf_locale);

	altFormatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");

	ClipboardRegisterSynthesizer(clipboard, CF_TEXT, altFormatId,
			clipboard_synthesize_utf8_string);

	/**
	 * CF_OEMTEXT
	 */

	ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_TEXT,
			clipboard_synthesize_cf_text);

	ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_UNICODETEXT,
			clipboard_synthesize_cf_unicodetext);

	ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, CF_LOCALE,
			clipboard_synthesize_cf_locale);

	altFormatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");

	ClipboardRegisterSynthesizer(clipboard, CF_OEMTEXT, altFormatId,
			clipboard_synthesize_utf8_string);

	/**
	 * CF_UNICODETEXT
	 */

	ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_TEXT,
			clipboard_synthesize_cf_text);

	ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_OEMTEXT,
			clipboard_synthesize_cf_oemtext);

	ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, CF_LOCALE,
			clipboard_synthesize_cf_locale);

	altFormatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");

	ClipboardRegisterSynthesizer(clipboard, CF_UNICODETEXT, altFormatId,
			clipboard_synthesize_utf8_string);

	/**
	 * UTF8_STRING
	 */

	formatId = ClipboardRegisterFormat(clipboard, "UTF8_STRING");

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

	/**
	 * text/plain
	 */

	formatId = ClipboardRegisterFormat(clipboard, "text/plain");

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

	/**
	 * TEXT
	 */

	formatId = ClipboardRegisterFormat(clipboard, "TEXT");

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

	/**
	 * STRING
	 */

	formatId = ClipboardRegisterFormat(clipboard, "STRING");

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

	/**
	 * CF_DIB
	 */

	if (formatId)
	{
		ClipboardRegisterSynthesizer(clipboard, CF_DIB, CF_DIBV5,
				clipboard_synthesize_cf_dibv5);

		altFormatId = ClipboardRegisterFormat(clipboard, "image/bmp");

		ClipboardRegisterSynthesizer(clipboard, CF_DIB, altFormatId,
				clipboard_synthesize_image_bmp);
	}

	/**
	 * CF_DIBV5
	 */

	if (formatId)
	{
		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, CF_DIB,
				clipboard_synthesize_cf_dib);

		altFormatId = ClipboardRegisterFormat(clipboard, "image/bmp");

		ClipboardRegisterSynthesizer(clipboard, CF_DIBV5, altFormatId,
				clipboard_synthesize_image_bmp);
	}

	/**
	 * image/bmp
	 */

	formatId = ClipboardRegisterFormat(clipboard, "image/bmp");

	if (formatId)
	{
		ClipboardRegisterSynthesizer(clipboard, formatId, CF_DIB,
				clipboard_synthesize_cf_dib);

		ClipboardRegisterSynthesizer(clipboard, formatId, CF_DIBV5,
				clipboard_synthesize_cf_dibv5);
	}

	/**
	 * HTML Format
	 */

	formatId = ClipboardRegisterFormat(clipboard, "HTML Format");

	if (formatId)
	{
		altFormatId = ClipboardRegisterFormat(clipboard, "text/html");

		ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
				clipboard_synthesize_text_html);
	}

	/**
	 * text/html
	 */

	formatId = ClipboardRegisterFormat(clipboard, "text/html");

	if (formatId)
	{
		altFormatId = ClipboardRegisterFormat(clipboard, "HTML Format");

		ClipboardRegisterSynthesizer(clipboard, formatId, altFormatId,
				clipboard_synthesize_html_format);
	}

	return TRUE;
}
