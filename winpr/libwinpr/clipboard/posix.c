/**
 * WinPR: Windows Portable Runtime
 * Clipboard Functions: POSIX file handling
 *
 * Copyright 2017 Alexei Lozovsky <a.lozovsky@gmail.com>
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

#include <stddef.h>
#include <stdlib.h>

#include <winpr/clipboard.h>
#include <winpr/collections.h>
#include <winpr/shell.h>
#include <winpr/string.h>
#include <winpr/wlog.h>

#include "clipboard.h"
#include "posix.h"

#include "../log.h"
#define TAG WINPR_TAG("clipboard.posix")

struct posix_file
{
	char* local_name;
	WCHAR* remote_name;
};

static struct posix_file* make_posix_file(const char* local_name, const WCHAR* remote_name)
{
	struct posix_file* file = NULL;

	file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;

	file->local_name = _strdup(local_name);
	file->remote_name = _wcsdup(remote_name);

	if (!file->local_name || !file->remote_name)
		goto error;

	return file;

error:
	free(file->local_name);
	free(file->remote_name);
	free(file);

	return NULL;
}

static void free_posix_file(void* the_file)
{
	struct posix_file* file = the_file;

	if (!file)
		return;

	free(file->local_name);
	free(file->remote_name);
	free(file);
}

static BOOL process_uri_list(const char* data, size_t length, wArrayList* files)
{
	ArrayList_Clear(files);
	/* TBD: parse text/uri-list `data` and store it into `files` */
	return TRUE;
}

static FILEDESCRIPTOR* convert_local_file_list_to_filedescriptors(wArrayList* files)
{
	/* TBD: convert `files` into a FILEDESCRIPTOR array */
	return NULL;
}

static void* convert_uri_list_to_filedescriptors(wClipboard* clipboard, UINT32 formatId,
		const void* data, UINT32* pSize)
{
	FILEDESCRIPTOR* descriptors = NULL;

	if (!clipboard || !data || !pSize)
		return NULL;

	if (formatId != ClipboardGetFormatId(clipboard, "text/uri-list"))
		return NULL;

	if (!process_uri_list((const char*) data, *pSize, clipboard->localFiles))
		return NULL;

	descriptors = convert_local_file_list_to_filedescriptors(clipboard->localFiles);
	if (!descriptors)
		return NULL;

	*pSize = ArrayList_Count(clipboard->localFiles) * sizeof(FILEDESCRIPTOR);

	return descriptors;
}

static BOOL register_file_formats_and_synthesizers(wClipboard* clipboard)
{
	UINT32 file_group_format_id;
	UINT32 local_file_format_id;

	file_group_format_id = ClipboardRegisterFormat(clipboard, "FileGroupDescriptorW");
	local_file_format_id = ClipboardRegisterFormat(clipboard, "text/uri-list");
	if (!file_group_format_id || !local_file_format_id)
		goto error;

	clipboard->localFiles = ArrayList_New(FALSE);
	if (!clipboard->localFiles)
		goto error;

	ArrayList_Object(clipboard->localFiles)->fnObjectFree = free_posix_file;

	if (!ClipboardRegisterSynthesizer(clipboard,
			local_file_format_id, file_group_format_id,
			convert_uri_list_to_filedescriptors))
		goto error_free_local_files;

	return TRUE;

error_free_local_files:
	ArrayList_Free(clipboard->localFiles);
	clipboard->localFiles = NULL;
error:
	return FALSE;
}

BOOL ClipboardInitPosixFileSubsystem(wClipboard* clipboard)
{
	if (!clipboard)
		return FALSE;

	if (!register_file_formats_and_synthesizers(clipboard))
		return FALSE;

	return TRUE;
}
