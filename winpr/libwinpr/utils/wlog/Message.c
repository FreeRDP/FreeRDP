/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/string.h>

#include "wlog.h"

#include "Message.h"

char* WLog_Message_GetOutputFileName(int id, const char* ext)
{
	char* FilePath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");
	if (!FilePath)
		return nullptr;

	char* FileName = nullptr;
	size_t FileNameLen = 0;
	char* FullFileName = nullptr;
	if (!winpr_PathFileExists(FilePath))
	{
		if (!winpr_PathMakePath(FilePath, nullptr))
			goto fail;
	}

	const DWORD ProcessId = GetCurrentProcessId();
	if (id >= 0)
		winpr_asprintf(&FileName, &FileNameLen, "%" PRIu32 "-%d.%s", ProcessId, id, ext);
	else
		winpr_asprintf(&FileName, &FileNameLen, "%" PRIu32 ".%s", ProcessId, ext);

	FullFileName = GetCombinedPath(FilePath, FileName);

fail:
	free(FilePath);
	free(FileName);
	return FullFileName;
}
