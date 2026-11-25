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

#ifndef WINPR_CLIPBOARD_PRIVATE_H
#define WINPR_CLIPBOARD_PRIVATE_H

#include <winpr/winpr.h>
#include <winpr/clipboard.h>

#include <winpr/collections.h>

typedef struct
{
	UINT32 syntheticId;
	CLIPBOARD_SYNTHESIZE_FN pfnSynthesize;
	CLIPBOARD_SYNTHESIZE_FN pfnSynthesizeEx;
} wClipboardSynthesizer;

typedef struct
{
	UINT32 formatId;
	char* formatName;

	UINT32 numSynthesizers;
	wClipboardSynthesizer* synthesizers;
} wClipboardFormat;

struct s_wClipboard
{
	UINT64 ownerId;

	/* clipboard formats */

	UINT32 numFormats;
	UINT32 maxFormats;
	UINT32 nextFormatId;
	wClipboardFormat* formats;

	/* clipboard data */

	UINT32 size;
	void* data;
	UINT32 formatId;
	UINT32 sequenceNumber;

	/* clipboard file handling */

	wArrayList* localFiles;
	UINT32 fileListSequenceNumber;

	wClipboardDelegate delegate;

	CRITICAL_SECTION lock;
};

WINPR_LOCAL WINPR_ATTR_NODISCARD BOOL ClipboardInitSynthesizers(wClipboard* clipboard);

WINPR_LOCAL WINPR_ATTR_NODISCARD char* parse_uri_to_local_file(const char* uri, size_t uri_len);

extern const char* const mime_text_plain;
extern const char* const mime_text_utf8;
extern const char* const mime_text_UTF8_STRING;

/** @brief Same as \ref ClipboardRegisterSynthesizer but with different \ref pfnSynthesize calling
 * convention. While functions registered with \ref ClipboardRegisterSynthesizer will call \ref
 * pfnSynthesize with the \ref formatId of the data in the clipboard functions registered with this
 * will be called with \ref syntheticId instead.
 *
 *  @param clipboard The clipboard to use
 *  @param formatId The format of the data in the clipboard
 *  @param syntheticId The format of the data to synthesize
 *  @param pfnSynthesize The function to call to synthesize
 *  @return TRUE for success, FALSE otherwise
 *
 *  @since version 3.32.2
 */
WINPR_ATTR_NODISCARD
WINPR_LOCAL BOOL ClipboardRegisterSynthesizerEx(wClipboard* clipboard, UINT32 formatId,
                                                UINT32 syntheticId,
                                                CLIPBOARD_SYNTHESIZE_FN pfnSynthesize);

#endif /* WINPR_CLIPBOARD_PRIVATE_H */
