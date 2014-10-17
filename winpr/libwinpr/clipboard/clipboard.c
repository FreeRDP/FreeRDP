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
#include <winpr/collections.h>

#include <winpr/clipboard.h>

#include "clipboard.h"

/**
 * Clipboard (Windows):
 * msdn.microsoft.com/en-us/library/windows/desktop/ms648709/
 *
 * W3C Clipboard API and events:
 * http://www.w3.org/TR/clipboard-apis/
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

const char* CF_STANDARD_STRINGS[CF_MAX] =
{
	"CF_RAW",		/* 0 */
	"CF_TEXT",		/* 1 */
	"CF_BITMAP",		/* 2 */
	"CF_METAFILEPICT",	/* 3 */
	"CF_SYLK",		/* 4 */
	"CF_DIF",		/* 5 */
	"CF_TIFF",		/* 6 */
	"CF_OEMTEXT",		/* 7 */
	"CF_DIB",		/* 8 */
	"CF_PALETTE",		/* 9 */
	"CF_PENDATA",		/* 10 */
	"CF_RIFF",		/* 11 */
	"CF_WAVE",		/* 12 */
	"CF_UNICODETEXT",	/* 13 */
	"CF_ENHMETAFILE",	/* 14 */
	"CF_HDROP",		/* 15 */
	"CF_LOCALE",		/* 16 */
	"CF_DIBV5"		/* 17 */
};

wClipboardFormat* ClipboardFindFormat(wClipboard* clipboard, UINT32 formatId, const char* name)
{
	UINT32 index;
	wClipboardFormat* format = NULL;

	if (formatId)
	{
		for (index = 0; index < clipboard->numFormats; index++)
		{
			if (formatId == clipboard->formats[index].formatId)
			{
				format = &clipboard->formats[index];
				break;
			}
		}
	}
	else if (name)
	{
		for (index = 0; index < clipboard->numFormats; index++)
		{
			if (strcmp(name, clipboard->formats[index].formatName) == 0)
			{
				format = &clipboard->formats[index];
				break;
			}
		}
	}
	else
	{
		/* special "CF_RAW" case */

		if (clipboard->numFormats > 0)
		{
			format = &clipboard->formats[0];

			if (format->formatId)
				return NULL;

			if (!format->formatName || (strcmp(format->formatName, CF_STANDARD_STRINGS[0]) == 0))
				return format;
		}
	}

	return format;
}

void ClipboardLock(wClipboard* clipboard)
{
	EnterCriticalSection(&(clipboard->lock));
}

void ClipboardUnlock(wClipboard* clipboard)
{
	LeaveCriticalSection(&(clipboard->lock));
}

BOOL ClipboardEmpty(wClipboard* clipboard)
{
	if (clipboard->data)
	{
		free((void*) clipboard->data);
		clipboard->data = NULL;
	}

	clipboard->size = 0;
	clipboard->sequenceNumber++;

	return TRUE;
}

UINT32 ClipboardCountFormats(wClipboard* clipboard)
{
	return clipboard->numFormats;
}

UINT32 ClipboardGetFormatIds(wClipboard* clipboard, UINT32** ppFormatIds)
{
	UINT32 index;
	UINT32* pFormatIds;
	wClipboardFormat* format;

	if (!ppFormatIds)
		return 0;

	pFormatIds = *ppFormatIds;

	if (!pFormatIds)
	{
		pFormatIds = malloc(clipboard->numFormats * sizeof(UINT32));

		if (!pFormatIds)
			return 0;

		*ppFormatIds = pFormatIds;
	}

	for (index = 0; index < clipboard->numFormats; index++)
	{
		format = &(clipboard->formats[index]);
		pFormatIds[index] = format->formatId;
	}

	return clipboard->numFormats;
}

UINT32 ClipboardRegisterFormat(wClipboard* clipboard, const char* name)
{
	wClipboardFormat* format;

	format = ClipboardFindFormat(clipboard, 0, name);

	if (format)
		return format->formatId;

	if ((clipboard->numFormats + 1) >= clipboard->maxFormats)
	{
		clipboard->maxFormats *= 2;

		clipboard->formats = (wClipboardFormat*) realloc(clipboard->formats,
				clipboard->maxFormats * sizeof(wClipboardFormat));

		if (!clipboard->formats)
			return 0;
	}

	format = &(clipboard->formats[clipboard->numFormats]);
	ZeroMemory(format, sizeof(wClipboardFormat));

	if (name)
	{
		format->formatName = _strdup(name);

		if (!format->formatName)
			return 0;
	}

	format->formatId = clipboard->nextFormatId++;
	clipboard->numFormats++;

	return format->formatId;
}

BOOL ClipboardInitFormats(wClipboard* clipboard)
{
	UINT32 formatId = 0;
	wClipboardFormat* format;

	for (formatId = 0; formatId < CF_MAX; formatId++)
	{
		format = &(clipboard->formats[clipboard->numFormats++]);
		ZeroMemory(format, sizeof(wClipboardFormat));

		format->formatId = formatId;
		format->formatName = _strdup(CF_STANDARD_STRINGS[formatId]);

		if (!format->formatName)
			return FALSE;
	}

	return TRUE;
}

const char* ClipboardGetFormatName(wClipboard* clipboard, UINT32 formatId)
{
	wClipboardFormat* format;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return NULL;

	return format->formatName;
}

const void* ClipboardGetData(wClipboard* clipboard, UINT32 formatId, UINT32* pSize)
{
	wClipboardFormat* format;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return NULL;

	*pSize = clipboard->size;

	return clipboard->data;
}

BOOL ClipboardSetData(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32 size)
{
	wClipboardFormat* format;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return FALSE;

	free((void*) clipboard->data);
	clipboard->data = data;
	clipboard->size = size;

	clipboard->formatId = formatId;
	clipboard->sequenceNumber++;

	return TRUE;
}

UINT64 ClipboardGetOwner(wClipboard* clipboard)
{
	return clipboard->ownerId;
}

void ClipboardSetOwner(wClipboard* clipboard, UINT64 ownerId)
{
	clipboard->ownerId = ownerId;
}

wClipboard* ClipboardCreate()
{
	wClipboard* clipboard;

	clipboard = (wClipboard*) calloc(1, sizeof(wClipboard));

	if (clipboard)
	{
		clipboard->nextFormatId = 0xC000;
		clipboard->sequenceNumber = 0;

		InitializeCriticalSectionAndSpinCount(&(clipboard->lock), 4000);

		clipboard->numFormats = 0;
		clipboard->maxFormats = 64;
		clipboard->formats = (wClipboardFormat*) malloc(clipboard->maxFormats * sizeof(wClipboardFormat));

		if (!clipboard->formats)
		{
			free(clipboard);
			return NULL;
		}

		ClipboardInitFormats(clipboard);
	}

	return clipboard;
}

void ClipboardDestroy(wClipboard* clipboard)
{
	UINT32 index;
	wClipboardFormat* format;

	if (!clipboard)
		return;

	for (index = 0; index < clipboard->numFormats; index++)
	{
		format = &(clipboard->formats[index]);
		free((void*) format->formatName);
	}

	clipboard->numFormats = 0;
	free(clipboard->formats);

	DeleteCriticalSection(&(clipboard->lock));

	free(clipboard);
}
