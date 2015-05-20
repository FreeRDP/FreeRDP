/**
 * WinPR: Windows Portable Runtime
 * Shell Functions
 *
 * Copyright 2015 Dell Software <Mike.McDonald@software.dell.com>
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

#include <winpr/shell.h>

/**
 * shell32.dll:
 *
 * GetUserProfileDirectoryA
 * GetUserProfileDirectoryW
 */

#ifndef _WIN32

#include <winpr/crt.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <pwd.h>
#include <grp.h>

#include "../handle/handle.h"

#include "../security/security.h"

BOOL GetUserProfileDirectoryA(HANDLE hToken, LPSTR lpProfileDir, LPDWORD lpcchSize)
{
	DWORD cchDirSize;
	struct passwd* pw;
	WINPR_ACCESS_TOKEN* token;

	token = (WINPR_ACCESS_TOKEN*) hToken;

	if ((token == NULL) || (token->Type	!= HANDLE_TYPE_ACCESS_TOKEN) || (lpcchSize == NULL))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	pw = getpwnam(token->Username);
	if (pw == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	cchDirSize = strlen(pw->pw_dir) + 1;

	if ((lpProfileDir == NULL) || (*lpcchSize < cchDirSize))
	{
		*lpcchSize = cchDirSize;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	ZeroMemory(lpProfileDir, *lpcchSize);
	strcpy(lpProfileDir, pw->pw_dir);
	*lpcchSize = cchDirSize;

	return TRUE;
}

BOOL GetUserProfileDirectoryW(HANDLE hToken, LPWSTR lpProfileDir, LPDWORD lpcchSize)
{
	BOOL bStatus;
	DWORD cchSizeA;
	LPSTR lpProfileDirA;

	if (lpcchSize == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	cchSizeA = *lpcchSize;
	lpProfileDirA = NULL;

	if (lpProfileDir)
	{
		lpProfileDirA = (LPSTR) malloc(cchSizeA);
		if (lpProfileDirA == NULL)
		{
			SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}
	}

	bStatus = GetUserProfileDirectoryA(hToken, lpProfileDirA, &cchSizeA);
	if (bStatus)
	{
		MultiByteToWideChar(CP_ACP, 0, lpProfileDirA, cchSizeA, lpProfileDir, *lpcchSize);
	}

	if (lpProfileDirA)
	{
		free(lpProfileDirA);
	}

	*lpcchSize = cchSizeA;
	
	return bStatus;
}

#endif

