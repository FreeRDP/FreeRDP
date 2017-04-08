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

#ifndef WINPR_CLIPBOARD_H
#define WINPR_CLIPBOARD_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef struct _wClipboard wClipboard;

typedef void* (*CLIPBOARD_SYNTHESIZE_FN)(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32* pSize);

struct _wClipboardFileSizeRequest
{
	UINT32 streamId;
	UINT32 listIndex;
};
typedef struct _wClipboardFileSizeRequest wClipboardFileSizeRequest;

struct _wClipboardFileRangeRequest
{
	UINT32 streamId;
	UINT32 listIndex;
	UINT32 nPositionLow;
	UINT32 nPositionHigh;
	UINT32 cbRequested;
};
typedef struct _wClipboardFileRangeRequest wClipboardFileRangeRequest;

typedef struct _wClipboardDelegate wClipboardDelegate;

struct _wClipboardDelegate
{
	wClipboard* clipboard;
	void* custom;

	UINT (*ClientRequestFileSize)(wClipboardDelegate*, const wClipboardFileSizeRequest*);
	UINT (*ClipboardFileSizeSuccess)(wClipboardDelegate*, const wClipboardFileSizeRequest*,
			UINT64 fileSize);
	UINT (*ClipboardFileSizeFailure)(wClipboardDelegate*, const wClipboardFileSizeRequest*,
			UINT errorCode);

	UINT (*ClientRequestFileRange)(wClipboardDelegate*, const wClipboardFileRangeRequest*);
	UINT (*ClipboardFileRangeSuccess)(wClipboardDelegate*, const wClipboardFileRangeRequest*,
			const BYTE* data, UINT32 size);
	UINT (*ClipboardFileRangeFailure)(wClipboardDelegate*, const wClipboardFileRangeRequest*,
			UINT errorCode);
};

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API void ClipboardLock(wClipboard* clipboard);
WINPR_API void ClipboardUnlock(wClipboard* clipboard);

WINPR_API BOOL ClipboardEmpty(wClipboard* clipboard);
WINPR_API UINT32 ClipboardCountFormats(wClipboard* clipboard);
WINPR_API UINT32 ClipboardGetFormatIds(wClipboard* clipboard, UINT32** ppFormatIds);

WINPR_API UINT32 ClipboardCountRegisteredFormats(wClipboard* clipboard);
WINPR_API UINT32 ClipboardGetRegisteredFormatIds(wClipboard* clipboard, UINT32** ppFormatIds);
WINPR_API UINT32 ClipboardRegisterFormat(wClipboard* clipboard, const char* name);

WINPR_API BOOL ClipboardRegisterSynthesizer(wClipboard* clipboard, UINT32 formatId,
		UINT32 syntheticId, CLIPBOARD_SYNTHESIZE_FN pfnSynthesize);

WINPR_API UINT32 ClipboardGetFormatId(wClipboard* clipboard, const char* name);
WINPR_API const char* ClipboardGetFormatName(wClipboard* clipboard, UINT32 formatId);
WINPR_API void* ClipboardGetData(wClipboard* clipboard, UINT32 formatId, UINT32* pSize);
WINPR_API BOOL ClipboardSetData(wClipboard* clipboard, UINT32 formatId, const void* data, UINT32 size);

WINPR_API UINT64 ClipboardGetOwner(wClipboard* clipboard);
WINPR_API void ClipboardSetOwner(wClipboard* clipboard, UINT64 ownerId);

WINPR_API wClipboardDelegate* ClipboardGetDelegate(wClipboard* clipboard);

WINPR_API wClipboard* ClipboardCreate();
WINPR_API void ClipboardDestroy(wClipboard* clipboard);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_CLIPBOARD_H */

