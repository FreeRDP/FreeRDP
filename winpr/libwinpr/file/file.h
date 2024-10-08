/**
 * WinPR: Windows Portable Runtime
 * File Functions
 *
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#ifndef WINPR_FILE_PRIV_H
#define WINPR_FILE_PRIV_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/nt.h>
#include <winpr/io.h>
#include <winpr/error.h>

#ifndef _WIN32

#include <stdio.h>
#include "../handle/handle.h"

#define EPOCH_DIFF 11644473600LL
#define STAT_TIME_TO_FILETIME(_t) (((UINT64)(_t) + EPOCH_DIFF) * 10000000LL)

struct winpr_file
{
	WINPR_HANDLE common;

	FILE* fp;

	char* lpFileName;

	DWORD dwOpenMode;
	DWORD dwShareMode;
	DWORD dwFlagsAndAttributes;

	LPSECURITY_ATTRIBUTES lpSecurityAttributes;
	DWORD dwCreationDisposition;
	HANDLE hTemplateFile;

	BOOL bLocked;
};
typedef struct winpr_file WINPR_FILE;

const HANDLE_CREATOR* GetFileHandleCreator(void);

UINT32 map_posix_err(int fs_errno);

#endif /* _WIN32 */

#endif /* WINPR_FILE_PRIV_H */
