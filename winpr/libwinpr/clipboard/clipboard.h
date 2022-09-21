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

WINPR_LOCAL BOOL ClipboardInitSynthesizers(wClipboard* clipboard);

WINPR_LOCAL char* parse_uri_to_local_file(const char* uri, size_t uri_len);

extern const char* mime_utf8_string;
extern const char* mime_uri_list;
extern const char* mime_FileGroupDescriptorW;
extern const char* mime_nautilus_clipboard;
extern const char* mime_gnome_copied_files;
extern const char* mime_mate_copied_files;

#endif /* WINPR_CLIPBOARD_PRIVATE_H */
