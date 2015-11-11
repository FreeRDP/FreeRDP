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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>

#include "wlog.h"

#include "Message.h"

char* WLog_Message_GetOutputFileName(int id, const char* ext)
{
	DWORD ProcessId;
	char* FilePath;
	char* FileName;
	char* FullFileName;


	if (!(FileName = (char*) malloc(256)))
		return NULL;

	FilePath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");

	if (!PathFileExistsA(FilePath))
	{
		if (!PathMakePathA(FilePath, NULL))
		{
			free(FileName);
			free(FilePath);
			return NULL;
		}
	}

	ProcessId = GetCurrentProcessId();
	if (id >= 0)
		sprintf_s(FileName, 256, "%u-%d.%s", (unsigned int) ProcessId, id, ext);
	else
		sprintf_s(FileName, 256, "%u.%s", (unsigned int) ProcessId, ext);

	FullFileName = GetCombinedPath(FilePath, FileName);

	free(FileName);
	free(FilePath);

	return FullFileName;
}
