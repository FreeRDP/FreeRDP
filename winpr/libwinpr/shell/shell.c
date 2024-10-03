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

#include <winpr/config.h>

#include <winpr/shell.h>

/**
 * shell32.dll:
 *
 * GetUserProfileDirectoryA
 * GetUserProfileDirectoryW
 */

#ifndef _WIN32

#include <winpr/crt.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <pwd.h>
#include <grp.h>

#include "../handle/handle.h"

#include "../security/security.h"

BOOL GetUserProfileDirectoryA(HANDLE hToken, LPSTR lpProfileDir, LPDWORD lpcchSize)
{
	struct passwd pwd = { 0 };
	struct passwd* pw = NULL;
	WINPR_ACCESS_TOKEN* token = (WINPR_ACCESS_TOKEN*)hToken;

	if (!AccessTokenIsValid(hToken))
		return FALSE;

	if (!lpcchSize)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	long buflen = sysconf(_SC_GETPW_R_SIZE_MAX);

	if (buflen == -1)
		buflen = 8196;

	char* buf = (char*)malloc(buflen);

	if (!buf)
		return FALSE;

	const int status = getpwnam_r(token->Username, &pwd, buf, buflen, &pw);

	if ((status != 0) || !pw)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		free(buf);
		return FALSE;
	}

	const size_t cchDirSize = strlen(pw->pw_dir) + 1;

	if (!lpProfileDir || (*lpcchSize < cchDirSize))
	{
		*lpcchSize = cchDirSize;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		free(buf);
		return FALSE;
	}

	ZeroMemory(lpProfileDir, *lpcchSize);
	(void)sprintf_s(lpProfileDir, *lpcchSize, "%s", pw->pw_dir);
	*lpcchSize = cchDirSize;
	free(buf);
	return TRUE;
}

BOOL GetUserProfileDirectoryW(HANDLE hToken, LPWSTR lpProfileDir, LPDWORD lpcchSize)
{
	BOOL bStatus = 0;
	DWORD cchSizeA = 0;
	LPSTR lpProfileDirA = NULL;

	if (!lpcchSize)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	cchSizeA = *lpcchSize;
	lpProfileDirA = NULL;

	if (lpProfileDir)
	{
		lpProfileDirA = (LPSTR)malloc(cchSizeA);

		if (lpProfileDirA == NULL)
		{
			SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}
	}

	bStatus = GetUserProfileDirectoryA(hToken, lpProfileDirA, &cchSizeA);

	if (bStatus)
	{
		SSIZE_T size = ConvertUtf8NToWChar(lpProfileDirA, cchSizeA, lpProfileDir, *lpcchSize);
		bStatus = size >= 0;
	}

	if (lpProfileDirA)
	{
		free(lpProfileDirA);
	}

	*lpcchSize = cchSizeA;
	return bStatus;
}

#endif
