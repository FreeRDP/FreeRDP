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
#include <winpr/wlog.h>

#include <winpr/clipboard.h>

#include "clipboard.h"

#ifdef WITH_WCLIPBOARD_POSIX
#include "posix.h"
#endif

#include "../log.h"
#define TAG WINPR_TAG("clipboard")

/**
 * Clipboard (Windows):
 * msdn.microsoft.com/en-us/library/windows/desktop/ms648709/
 *
 * W3C Clipboard API and events:
 * http://www.w3.org/TR/clipboard-apis/
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

wClipboardFormat* ClipboardFindFormat(wClipboard* clipboard, UINT32 formatId,
                                      const char* name)
{
	UINT32 index;
	wClipboardFormat* format = NULL;

	if (!clipboard)
		return NULL;

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
			if (!clipboard->formats[index].formatName)
				continue;

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

			if (!format->formatName
			    || (strcmp(format->formatName, CF_STANDARD_STRINGS[0]) == 0))
				return format;
		}
	}

	return format;
}

wClipboardSynthesizer* ClipboardFindSynthesizer(wClipboardFormat* format,
        UINT32 formatId)
{
	UINT32 index;
	wClipboardSynthesizer* synthesizer;

	if (!format)
		return NULL;

	if (format->numSynthesizers < 1)
		return NULL;

	for (index = 0; index < format->numSynthesizers; index++)
	{
		synthesizer = &(format->synthesizers[index]);

		if (formatId == synthesizer->syntheticId)
			return synthesizer;
	}

	return NULL;
}

void ClipboardLock(wClipboard* clipboard)
{
	if (!clipboard)
		return;

	EnterCriticalSection(&(clipboard->lock));
}

void ClipboardUnlock(wClipboard* clipboard)
{
	if (!clipboard)
		return;

	LeaveCriticalSection(&(clipboard->lock));
}

BOOL ClipboardEmpty(wClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (clipboard->data)
	{
		free((void*) clipboard->data);
		clipboard->data = NULL;
	}

	clipboard->size = 0;
	clipboard->formatId = 0;
	clipboard->sequenceNumber++;
	return TRUE;
}

UINT32 ClipboardCountRegisteredFormats(wClipboard* clipboard)
{
	if (!clipboard)
		return 0;

	return clipboard->numFormats;
}

UINT32 ClipboardGetRegisteredFormatIds(wClipboard* clipboard,
                                       UINT32** ppFormatIds)
{
	UINT32 index;
	UINT32* pFormatIds;
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	if (!ppFormatIds)
		return 0;

	pFormatIds = *ppFormatIds;

	if (!pFormatIds)
	{
		pFormatIds = calloc(clipboard->numFormats, sizeof(UINT32));

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

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, 0, name);

	if (format)
		return format->formatId;

	if ((clipboard->numFormats + 1) >= clipboard->maxFormats)
	{
		UINT32 numFormats = clipboard->maxFormats * 2;
		wClipboardFormat* tmpFormat;
		tmpFormat = (wClipboardFormat*) realloc(clipboard->formats,
		                                        numFormats * sizeof(wClipboardFormat));

		if (!tmpFormat)
			return 0;

		clipboard->formats  = tmpFormat;
		clipboard->maxFormats = numFormats;
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

BOOL ClipboardRegisterSynthesizer(wClipboard* clipboard, UINT32 formatId,
                                  UINT32 syntheticId, CLIPBOARD_SYNTHESIZE_FN pfnSynthesize)
{
	UINT32 index;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return FALSE;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return FALSE;

	if (format->formatId == syntheticId)
		return FALSE;

	synthesizer = ClipboardFindSynthesizer(format, formatId);

	if (!synthesizer)
	{
		wClipboardSynthesizer* tmpSynthesizer;
		UINT32 numSynthesizers = format->numSynthesizers + 1;
		tmpSynthesizer = (wClipboardSynthesizer*) realloc(format->synthesizers,
		                 numSynthesizers * sizeof(wClipboardSynthesizer));

		if (!tmpSynthesizer)
			return FALSE;

		format->synthesizers  = tmpSynthesizer;
		format->numSynthesizers = numSynthesizers;
		index = numSynthesizers - 1;
		synthesizer = &(format->synthesizers[index]);
	}

	ZeroMemory(synthesizer, sizeof(wClipboardSynthesizer));
	synthesizer->syntheticId = syntheticId;
	synthesizer->pfnSynthesize = pfnSynthesize;
	return TRUE;
}

UINT32 ClipboardCountFormats(wClipboard* clipboard)
{
	UINT32 count;
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return 0;

	count = 1 + format->numSynthesizers;
	return count;
}

UINT32 ClipboardGetFormatIds(wClipboard* clipboard, UINT32** ppFormatIds)
{
	UINT32 index;
	UINT32 count;
	UINT32* pFormatIds;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return 0;

	count = 1 + format->numSynthesizers;

	if (!ppFormatIds)
		return 0;

	pFormatIds = *ppFormatIds;

	if (!pFormatIds)
	{
		pFormatIds = calloc(count, sizeof(UINT32));

		if (!pFormatIds)
			return 0;

		*ppFormatIds = pFormatIds;
	}

	pFormatIds[0] = format->formatId;

	for (index = 1; index < count; index++)
	{
		synthesizer = &(format->synthesizers[index - 1]);
		pFormatIds[index] = synthesizer->syntheticId;
	}

	return count;
}

BOOL ClipboardInitFormats(wClipboard* clipboard)
{
	UINT32 formatId = 0;
	wClipboardFormat* format;

	if (!clipboard)
		return FALSE;

	for (formatId = 0; formatId < CF_MAX; formatId++, clipboard->numFormats++)
	{
		format = &(clipboard->formats[clipboard->numFormats]);
		ZeroMemory(format, sizeof(wClipboardFormat));
		format->formatId = formatId;
		format->formatName = _strdup(CF_STANDARD_STRINGS[formatId]);
		if (!format->formatName)
			goto error;
	}

	if (!ClipboardInitSynthesizers(clipboard))
		goto error;

	return TRUE;

error:
	for (formatId = 0; formatId < clipboard->numFormats; formatId++)
	{
		free(clipboard->formats[formatId].formatName);
		free(clipboard->formats[formatId].synthesizers);
	}
	return FALSE;
}

UINT32 ClipboardGetFormatId(wClipboard* clipboard, const char* name)
{
	wClipboardFormat* format;

	if (!clipboard)
		return 0;

	format = ClipboardFindFormat(clipboard, 0, name);

	if (!format)
		return 0;

	return format->formatId;
}

const char* ClipboardGetFormatName(wClipboard* clipboard, UINT32 formatId)
{
	wClipboardFormat* format;

	if (!clipboard)
		return NULL;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return NULL;

	return format->formatName;
}

void* ClipboardGetData(wClipboard* clipboard, UINT32 formatId, UINT32* pSize)
{
	UINT32 SrcSize = 0;
	UINT32 DstSize = 0;
	void* pSrcData = NULL;
	void* pDstData = NULL;
	wClipboardFormat* format;
	wClipboardSynthesizer* synthesizer;

	if (!clipboard)
		return NULL;

	if (!pSize)
		return NULL;

	format = ClipboardFindFormat(clipboard, clipboard->formatId, NULL);

	if (!format)
		return NULL;

	SrcSize = clipboard->size;
	pSrcData = (void*) clipboard->data;

	if (formatId == format->formatId)
	{
		DstSize = SrcSize;
		pDstData = malloc(DstSize);

		if (!pDstData)
			return NULL;

		CopyMemory(pDstData, pSrcData, SrcSize);
		*pSize = DstSize;
	}
	else
	{
		synthesizer = ClipboardFindSynthesizer(format, formatId);

		if (!synthesizer || !synthesizer->pfnSynthesize)
			return NULL;

		DstSize = SrcSize;
		pDstData = synthesizer->pfnSynthesize(clipboard, format->formatId, pSrcData,
		                                      &DstSize);
		*pSize = DstSize;
	}

	return pDstData;
}

BOOL ClipboardSetData(wClipboard* clipboard, UINT32 formatId, const void* data,
                      UINT32 size)
{
	wClipboardFormat* format;

	if (!clipboard)
		return FALSE;

	format = ClipboardFindFormat(clipboard, formatId, NULL);

	if (!format)
		return FALSE;

	free((void*) clipboard->data);
	clipboard->data = malloc(size);

	if (!clipboard->data)
		return FALSE;

	memcpy(clipboard->data, data, size);
	clipboard->size = size;
	clipboard->formatId = formatId;
	clipboard->sequenceNumber++;
	return TRUE;
}

UINT64 ClipboardGetOwner(wClipboard* clipboard)
{
	if (!clipboard)
		return 0;

	return clipboard->ownerId;
}

void ClipboardSetOwner(wClipboard* clipboard, UINT64 ownerId)
{
	if (!clipboard)
		return;

	clipboard->ownerId = ownerId;
}

wClipboardDelegate* ClipboardGetDelegate(wClipboard* clipboard)
{
	if (!clipboard)
		return NULL;

	return &clipboard->delegate;
}

void ClipboardInitLocalFileSubsystem(wClipboard* clipboard)
{
	/*
	 * There can be only one local file subsystem active.
	 * Return as soon as initialization succeeds.
	 */

#ifdef WITH_WCLIPBOARD_POSIX
	if (ClipboardInitPosixFileSubsystem(clipboard))
	{
		WLog_INFO(TAG, "initialized POSIX local file subsystem");
		return;
	}
	else
	{
		WLog_WARN(TAG, "failed to initialize POSIX local file subsystem");
	}
#endif

	WLog_INFO(TAG, "failed to initialize local file subsystem, file transfer not available");
}

wClipboard* ClipboardCreate()
{
	wClipboard* clipboard;

	clipboard = (wClipboard*) calloc(1, sizeof(wClipboard));
	if (!clipboard)
		return NULL;

	clipboard->nextFormatId = 0xC000;
	clipboard->sequenceNumber = 0;

	if (!InitializeCriticalSectionAndSpinCount(&(clipboard->lock), 4000))
		goto error_free_clipboard;

	clipboard->numFormats = 0;
	clipboard->maxFormats = 64;

	clipboard->formats = (wClipboardFormat*)
		calloc(clipboard->maxFormats, sizeof(wClipboardFormat));
	if (!clipboard->formats)
		goto error_free_lock;

	if (!ClipboardInitFormats(clipboard))
		goto error_free_formats;

	clipboard->delegate.clipboard = clipboard;

	ClipboardInitLocalFileSubsystem(clipboard);

	return clipboard;

error_free_formats:
	free(clipboard->formats);
error_free_lock:
	DeleteCriticalSection(&(clipboard->lock));
error_free_clipboard:
	free(clipboard);
	return NULL;
}

void ClipboardDestroy(wClipboard* clipboard)
{
	UINT32 index;
	wClipboardFormat* format;

	if (!clipboard)
		return;

	ArrayList_Free(clipboard->localFiles);
	clipboard->localFiles = NULL;

	for (index = 0; index < clipboard->numFormats; index++)
	{
		format = &(clipboard->formats[index]);
		free((void*) format->formatName);

		if (format->synthesizers)
		{
			free(format->synthesizers);
			format->synthesizers = NULL;
			format->numSynthesizers = 0;
		}
	}

	free((void*) clipboard->data);
	clipboard->data = NULL;
	clipboard->size = 0;
	clipboard->numFormats = 0;
	free(clipboard->formats);
	DeleteCriticalSection(&(clipboard->lock));
	free(clipboard);
}
