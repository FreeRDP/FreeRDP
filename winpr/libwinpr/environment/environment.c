/**
 * WinPR: Windows Portable Runtime
 * Process Environment Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/platform.h>
#include <winpr/error.h>
#include <winpr/string.h>

#include <winpr/environment.h>

#ifndef _WIN32

#include <winpr/crt.h>
#include <winpr/platform.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#if defined(__IOS__)

#elif defined(__MACOSX__)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif

DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer)
{
	char* cwd;
	size_t length;

	cwd = getcwd(NULL, 0);

	if (!cwd)
		return 0;

	length = strlen(cwd);

	if ((nBufferLength == 0) && (lpBuffer == NULL))
	{
		free(cwd);

		return (DWORD)length;
	}
	else
	{
		if (lpBuffer == NULL)
		{
			free(cwd);
			return 0;
		}

		if ((length + 1) > nBufferLength)
		{
			free(cwd);
			return (DWORD)(length + 1);
		}

		memcpy(lpBuffer, cwd, length + 1);
		free(cwd);
		return (DWORD)length;
	}
}

DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer)
{
	return 0;
}

BOOL SetCurrentDirectoryA(LPCSTR lpPathName)
{
	return TRUE;
}

BOOL SetCurrentDirectoryW(LPCWSTR lpPathName)
{
	return TRUE;
}

DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName, LPCSTR lpExtension, DWORD nBufferLength,
                  LPSTR lpBuffer, LPSTR* lpFilePart)
{
	return 0;
}

DWORD SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName, LPCWSTR lpExtension, DWORD nBufferLength,
                  LPWSTR lpBuffer, LPWSTR* lpFilePart)
{
	return 0;
}

LPSTR GetCommandLineA(VOID)
{
	return NULL;
}

LPWSTR GetCommandLineW(VOID)
{
	return NULL;
}

BOOL NeedCurrentDirectoryForExePathA(LPCSTR ExeName)
{
	return TRUE;
}

BOOL NeedCurrentDirectoryForExePathW(LPCWSTR ExeName)
{
	return TRUE;
}

#endif

#if !defined(_WIN32) || defined(_UWP)

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
#if !defined(_UWP)
	size_t length;
	char* env = NULL;

	env = getenv(lpName);

	if (!env)
	{
		SetLastError(ERROR_ENVVAR_NOT_FOUND);
		return 0;
	}

	length = strlen(env);

	if ((length + 1 > nSize) || (!lpBuffer))
		return (DWORD)length + 1;

	CopyMemory(lpBuffer, env, length);
	lpBuffer[length] = '\0';

	return (DWORD)length;
#else
	SetLastError(ERROR_ENVVAR_NOT_FOUND);
	return 0;
#endif
}

DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize)
{
	SetLastError(ERROR_ENVVAR_NOT_FOUND);
	return 0;
}

BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue)
{
#if !defined(_UWP)
	if (!lpName)
		return FALSE;

	if (lpValue)
	{
		if (0 != setenv(lpName, lpValue, 1))
			return FALSE;
	}
	else
	{
		if (0 != unsetenv(lpName))
			return FALSE;
	}

	return TRUE;
#else
	return FALSE;
#endif
}

BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue)
{
	return FALSE;
}

/**
 * GetEnvironmentStrings function:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms683187/
 *
 * The GetEnvironmentStrings function returns a pointer to a block of memory
 * that contains the environment variables of the calling process (both the
 * system and the user environment variables). Each environment block contains
 * the environment variables in the following format:
 *
 * Var1=Value1\0
 * Var2=Value2\0
 * Var3=Value3\0
 * ...
 * VarN=ValueN\0\0
 */

extern char** environ;

LPCH GetEnvironmentStringsA(VOID)
{
#if !defined(_UWP)
	char* p;
	size_t offset;
	size_t length;
	char** envp;
	DWORD cchEnvironmentBlock;
	LPCH lpszEnvironmentBlock;

	offset = 0;
	envp = environ;

	cchEnvironmentBlock = 128;
	lpszEnvironmentBlock = (LPCH)calloc(cchEnvironmentBlock, sizeof(CHAR));
	if (!lpszEnvironmentBlock)
		return NULL;

	while (*envp)
	{
		length = strlen(*envp);

		while ((offset + length + 8) > cchEnvironmentBlock)
		{
			DWORD new_size;
			LPCH new_blk;

			new_size = cchEnvironmentBlock * 2;
			new_blk = (LPCH)realloc(lpszEnvironmentBlock, new_size * sizeof(CHAR));
			if (!new_blk)
			{
				free(lpszEnvironmentBlock);
				return NULL;
			}

			lpszEnvironmentBlock = new_blk;
			cchEnvironmentBlock = new_size;
		}

		p = &(lpszEnvironmentBlock[offset]);

		CopyMemory(p, *envp, length * sizeof(CHAR));
		p[length] = '\0';

		offset += (length + 1);
		envp++;
	}

	lpszEnvironmentBlock[offset] = '\0';

	return lpszEnvironmentBlock;
#else
	return NULL;
#endif
}

LPWCH GetEnvironmentStringsW(VOID)
{
	return NULL;
}

BOOL SetEnvironmentStringsA(LPCH NewEnvironment)
{
	return TRUE;
}

BOOL SetEnvironmentStringsW(LPWCH NewEnvironment)
{
	return TRUE;
}

DWORD ExpandEnvironmentStringsA(LPCSTR lpSrc, LPSTR lpDst, DWORD nSize)
{
	return 0;
}

DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize)
{
	return 0;
}

BOOL FreeEnvironmentStringsA(LPCH lpszEnvironmentBlock)
{
	free(lpszEnvironmentBlock);

	return TRUE;
}

BOOL FreeEnvironmentStringsW(LPWCH lpszEnvironmentBlock)
{
	return TRUE;
}

#endif

LPCH MergeEnvironmentStrings(PCSTR original, PCSTR merge)
{
	const char* cp;
	char* p;
	size_t offset;
	size_t length;
	const char* envp;
	DWORD cchEnvironmentBlock;
	LPCH lpszEnvironmentBlock;
	const char** mergeStrings;
	size_t mergeStringLength;
	size_t mergeArraySize = 128;
	size_t run;
	size_t mergeLength;
	size_t foundMerge;
	char* foundEquals;

	mergeStrings = (LPCSTR*)calloc(mergeArraySize, sizeof(char*));

	if (!mergeStrings)
		return NULL;

	mergeStringLength = 0;

	cp = merge;

	while (*cp && *(cp + 1))
	{
		length = strlen(cp);

		if (mergeStringLength == mergeArraySize)
		{
			const char** new_str;

			mergeArraySize += 128;
			new_str = (const char**)realloc((void*)mergeStrings, mergeArraySize * sizeof(char*));

			if (!new_str)
			{
				free((void*)mergeStrings);
				return NULL;
			}
			mergeStrings = new_str;
		}

		mergeStrings[mergeStringLength] = cp;
		cp += length + 1;
		mergeStringLength++;
	}

	offset = 0;

	cchEnvironmentBlock = 128;
	lpszEnvironmentBlock = (LPCH)calloc(cchEnvironmentBlock, sizeof(CHAR));

	if (!lpszEnvironmentBlock)
	{
		free((void*)mergeStrings);
		return NULL;
	}

	envp = original;

	while ((original != NULL) && (*envp && *(envp + 1)))
	{
		size_t old_offset = offset;
		length = strlen(envp);

		while ((offset + length + 8) > cchEnvironmentBlock)
		{
			LPCH tmp;
			cchEnvironmentBlock *= 2;
			tmp = (LPCH)realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

			if (!tmp)
			{
				free((void*)lpszEnvironmentBlock);
				free((void*)mergeStrings);
				return NULL;
			}
			lpszEnvironmentBlock = tmp;
		}

		p = &(lpszEnvironmentBlock[offset]);

		// check if this value is in the mergeStrings
		foundMerge = 0;
		for (run = 0; run < mergeStringLength; run++)
		{
			if (!mergeStrings[run])
				continue;

			mergeLength = strlen(mergeStrings[run]);
			foundEquals = strstr(mergeStrings[run], "=");

			if (!foundEquals)
				continue;

			if (strncmp(envp, mergeStrings[run], foundEquals - mergeStrings[run] + 1) == 0)
			{
				// found variable in merge list ... use this ....
				if (*(foundEquals + 1) == '\0')
				{
					// check if the argument is set ... if not remove variable ...
					foundMerge = 1;
				}
				else
				{
					while ((offset + mergeLength + 8) > cchEnvironmentBlock)
					{
						LPCH tmp;
						cchEnvironmentBlock *= 2;
						tmp =
						    (LPCH)realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

						if (!tmp)
						{
							free((void*)lpszEnvironmentBlock);
							free((void*)mergeStrings);
							return NULL;
						}
						lpszEnvironmentBlock = tmp;
						p = &(lpszEnvironmentBlock[old_offset]);
					}

					foundMerge = 1;
					CopyMemory(p, mergeStrings[run], mergeLength);
					mergeStrings[run] = NULL;
					p[mergeLength] = '\0';
					offset += (mergeLength + 1);
				}
			}
		}

		if (foundMerge == 0)
		{
			CopyMemory(p, envp, length * sizeof(CHAR));
			p[length] = '\0';
			offset += (length + 1);
		}

		envp += (length + 1);
	}

	// now merge the not already merged env
	for (run = 0; run < mergeStringLength; run++)
	{
		if (!mergeStrings[run])
			continue;

		mergeLength = strlen(mergeStrings[run]);

		while ((offset + mergeLength + 8) > cchEnvironmentBlock)
		{
			LPCH tmp;
			cchEnvironmentBlock *= 2;
			tmp = (LPCH)realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

			if (!tmp)
			{
				free((void*)lpszEnvironmentBlock);
				free((void*)mergeStrings);
				return NULL;
			}

			lpszEnvironmentBlock = tmp;
		}

		p = &(lpszEnvironmentBlock[offset]);

		CopyMemory(p, mergeStrings[run], mergeLength);
		mergeStrings[run] = NULL;
		p[mergeLength] = '\0';
		offset += (mergeLength + 1);
	}

	lpszEnvironmentBlock[offset] = '\0';

	free((void*)mergeStrings);

	return lpszEnvironmentBlock;
}

DWORD GetEnvironmentVariableEBA(LPCSTR envBlock, LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
	size_t vLength = 0;
	char* env = NULL;
	char* foundEquals;
	const char* penvb = envBlock;
	size_t nLength, fLength, lpNameLength;

	if (!lpName || NULL == envBlock)
		return 0;

	lpNameLength = strlen(lpName);

	if (lpNameLength < 1)
		return 0;

	while (*penvb && *(penvb + 1))
	{
		fLength = strlen(penvb);
		foundEquals = strstr(penvb, "=");

		if (!foundEquals)
		{
			/* if no = sign is found the envBlock is broken */
			return 0;
		}

		nLength = (foundEquals - penvb);

		if (nLength != lpNameLength)
		{
			penvb += (fLength + 1);
			continue;
		}

		if (strncmp(penvb, lpName, nLength) == 0)
		{
			env = foundEquals + 1;
			break;
		}

		penvb += (fLength + 1);
	}

	if (!env)
		return 0;

	vLength = strlen(env);
	if (vLength >= UINT32_MAX)
		return 0;

	if ((vLength + 1 > nSize) || (!lpBuffer))
		return (DWORD)vLength + 1;

	CopyMemory(lpBuffer, env, vLength + 1);

	return (DWORD)vLength;
}

BOOL SetEnvironmentVariableEBA(LPSTR* envBlock, LPCSTR lpName, LPCSTR lpValue)
{
	size_t length;
	char* envstr;
	char* newEB;

	if (!lpName)
		return FALSE;

	if (lpValue)
	{
		length = (strlen(lpName) + strlen(lpValue) + 2); /* +2 because of = and \0 */
		envstr = (char*)malloc(length + 1);              /* +1 because of closing \0 */

		if (!envstr)
			return FALSE;

		sprintf_s(envstr, length, "%s=%s", lpName, lpValue);
	}
	else
	{
		length = strlen(lpName) + 2;        /* +2 because of = and \0 */
		envstr = (char*)malloc(length + 1); /* +1 because of closing \0 */

		if (!envstr)
			return FALSE;

		sprintf_s(envstr, length, "%s=", lpName);
	}

	envstr[length] = '\0';

	newEB = MergeEnvironmentStrings((LPCSTR)*envBlock, envstr);

	free(envstr);
	free(*envBlock);

	*envBlock = newEB;

	return TRUE;
}

char** EnvironmentBlockToEnvpA(LPCH lpszEnvironmentBlock)
{
	char* p;
	SSIZE_T index;
	size_t count;
	size_t length;
	char** envp = NULL;

	count = 0;
	if (!lpszEnvironmentBlock)
		return NULL;

	p = (char*)lpszEnvironmentBlock;

	while (p[0] && p[1])
	{
		length = strlen(p);
		p += (length + 1);
		count++;
	}

	index = 0;
	p = (char*)lpszEnvironmentBlock;

	envp = (char**)calloc(count + 1, sizeof(char*));
	if (!envp)
		return NULL;
	envp[count] = NULL;

	while (p[0] && p[1])
	{
		length = strlen(p);
		envp[index] = _strdup(p);
		if (!envp[index])
		{
			for (index -= 1; index >= 0; --index)
			{
				free(envp[index]);
			}
			free(envp);
			return NULL;
		}
		p += (length + 1);
		index++;
	}

	return envp;
}

#ifdef _WIN32

// https://devblogs.microsoft.com/oldnewthing/20100203-00/?p=15083
#define WINPR_MAX_ENVIRONMENT_LENGTH 2048

DWORD GetEnvironmentVariableX(const char* lpName, char* lpBuffer, DWORD nSize)
{
	DWORD result = 0;
	DWORD nSizeW = 0;
	LPWSTR lpNameW = NULL;
	LPWSTR lpBufferW = NULL;
	LPSTR lpBufferA = lpBuffer;

	lpNameW = ConvertUtf8ToWCharAlloc(lpName, NULL);
	if (!lpNameW)
		goto cleanup;

	if (!lpBuffer)
	{
		char lpBufferMaxA[WINPR_MAX_ENVIRONMENT_LENGTH] = { 0 };
		WCHAR lpBufferMaxW[WINPR_MAX_ENVIRONMENT_LENGTH] = { 0 };
		LPSTR lpTmpBuffer = lpBufferMaxA;

		nSizeW = ARRAYSIZE(lpBufferMaxW);

		result = GetEnvironmentVariableW(lpNameW, lpBufferMaxW, nSizeW);

		SSIZE_T rc =
		    ConvertWCharNToUtf8(lpBufferMaxW, nSizeW, lpTmpBuffer, ARRAYSIZE(lpBufferMaxA));
		if ((rc < 0) || (rc >= UINT32_MAX))
			goto cleanup;

		result = (DWORD)rc + 1;
	}
	else
	{
		nSizeW = nSize;
		lpBufferW = calloc(nSizeW + 1, sizeof(WCHAR));

		if (!lpBufferW)
			goto cleanup;

		result = GetEnvironmentVariableW(lpNameW, lpBufferW, nSizeW);

		if (result == 0)
			goto cleanup;

		SSIZE_T rc = ConvertWCharNToUtf8(lpBufferW, nSizeW, lpBufferA, nSize);
		if ((rc < 0) || (rc > UINT32_MAX))
			goto cleanup;

		result = (DWORD)rc;
	}

cleanup:
	free(lpBufferW);
	free(lpNameW);

	return result;
}

#else

DWORD GetEnvironmentVariableX(const char* lpName, char* lpBuffer, DWORD nSize)
{
	return GetEnvironmentVariableA(lpName, lpBuffer, nSize);
}

#endif
