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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/error.h>

#include <winpr/environment.h>

#ifndef _WIN32

#define stricmp strcasecmp
#define strnicmp strncasecmp

#include <winpr/crt.h>
#include <winpr/platform.h>

#ifdef HAVE_UNISTD_H
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
	int length;

	cwd = getcwd(NULL, 0);

	if (!cwd)
		return 0;

	length = strlen(cwd);

	if ((nBufferLength == 0) && (lpBuffer == NULL))
	{
		free(cwd);

		return (DWORD) length;
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
			return (DWORD) (length + 1);
		}

		memcpy(lpBuffer, cwd, length + 1);
		return length;
	}

	return 0;
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

DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName, LPCSTR lpExtension, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart)
{
	return 0;
}

DWORD SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName, LPCWSTR lpExtension, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart)
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
	int length;
	char* env = NULL;

	env = getenv(lpName);

	if (!env)
	{
		SetLastError(ERROR_ENVVAR_NOT_FOUND);
		return 0;
	}

	length = strlen(env);

	if ((length + 1 > nSize) || (!lpBuffer))
		return length + 1;

	CopyMemory(lpBuffer, env, length + 1);

	return length;
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
		if (0 != setenv(lpName,lpValue,1))
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
	int offset;
	int length;
	char** envp;
	DWORD cchEnvironmentBlock;
	LPCH lpszEnvironmentBlock;

	offset = 0;
	envp = environ;

	cchEnvironmentBlock = 128;
	lpszEnvironmentBlock = (LPCH) malloc(cchEnvironmentBlock * sizeof(CHAR));
	if (!lpszEnvironmentBlock)
		return NULL;

	while (*envp)
	{
		length = (int) strlen(*envp);

		while ((offset + length + 8) > (int) cchEnvironmentBlock)
		{
			DWORD new_size;
			LPCH new_blk;

			new_size = cchEnvironmentBlock * 2;
			new_blk = (LPCH) realloc(lpszEnvironmentBlock, new_size * sizeof(CHAR));
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
	int offset;
	int length;
	const char* envp;
	DWORD cchEnvironmentBlock;
	LPCH lpszEnvironmentBlock;
	const char** mergeStrings;
	int mergeStringLength;
	int mergeArraySize = 128;
	int run;
	int mergeLength;
	int foundMerge;
	char* foundEquals;

	mergeStrings = (LPCSTR*) calloc(mergeArraySize, sizeof(char*));

	if (!mergeStrings)
		return NULL;

	mergeStringLength = 0;

	cp = merge;

	while (*cp && *(cp + 1))
	{
		length = (int) strlen(cp);

		if (mergeStringLength == mergeArraySize)
		{
			const char** new_str;

			mergeArraySize += 128;
			new_str = (const char**) realloc((void*) mergeStrings, mergeArraySize * sizeof(char*));

			if (!new_str)
			{
				free((void*) mergeStrings);
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
	lpszEnvironmentBlock = (LPCH) malloc(cchEnvironmentBlock * sizeof(CHAR));

	if (!lpszEnvironmentBlock)
	{
		free((void*) mergeStrings);
		return NULL;
	}

	envp  = original;

	while ((original != NULL) && (*envp && *(envp+1)))
	{
		ULONG old_offset = offset;
		length = (int) strlen(envp);

		while ((offset + length + 8) > (int) cchEnvironmentBlock)
		{
			LPCH tmp;
			cchEnvironmentBlock *= 2;
			tmp = (LPCH) realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

			if (!tmp)
			{
				free((void*) lpszEnvironmentBlock);
				free((void*) mergeStrings);
				return NULL;
			}
			lpszEnvironmentBlock = tmp;
		}

		p = &(lpszEnvironmentBlock[offset]);

		// check if this value is in the mergeStrings
		foundMerge = 0;
		for (run = 0; run < mergeStringLength; run ++)
		{
			if (!mergeStrings[run])
				continue;

			mergeLength = (int) strlen(mergeStrings[run]);
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
					while ((offset + mergeLength + 8) > (int) cchEnvironmentBlock)
					{
						LPCH tmp;
						cchEnvironmentBlock *= 2;
						tmp = (LPCH) realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

						if (!tmp)
						{
							free((void*) lpszEnvironmentBlock);
							free((void*) mergeStrings);
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

		envp += (length +1);
	}

	// now merge the not already merged env
	for (run = 0; run < mergeStringLength; run ++)
	{
		if (!mergeStrings[run])
			continue;

		mergeLength = (int) strlen(mergeStrings[run]);

		while ((offset + mergeLength + 8) > (int) cchEnvironmentBlock)
		{
			LPCH tmp;
			cchEnvironmentBlock *= 2;
			tmp = (LPCH) realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));

			if (!tmp)
			{
				free((void*) lpszEnvironmentBlock);
				free((void*) mergeStrings);
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

	free((void*) mergeStrings);

	return lpszEnvironmentBlock;
}

DWORD GetEnvironmentVariableEBA(LPCSTR envBlock, LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
	int vLength = 0;
	char* env = NULL;
	char* foundEquals;
	const char* penvb = envBlock;
	int nLength, fLength, lpNameLength;

	if (!lpName || NULL == envBlock)
		return 0;

	lpNameLength = (int) strlen(lpName);

	if (lpNameLength < 1)
		return 0;

	while (*penvb && *(penvb + 1))
	{
		fLength = (int) strlen(penvb);
		foundEquals = strstr(penvb,"=");

		if (!foundEquals)
		{
			/* if no = sign is found the envBlock is broken */
			return 0;
		}

		nLength = (int) (foundEquals - penvb);

		if (nLength != lpNameLength)
		{
			penvb += (fLength +1);
			continue;
		}

		if (strncmp(penvb, lpName, nLength) == 0)
		{
			env = foundEquals + 1;
			break;
		}

		penvb += (fLength +1);
	}

	if (!env)
		return 0;

	vLength = (int) strlen(env);

	if ((vLength + 1 > (int) nSize) || (!lpBuffer))
		return vLength + 1;

	CopyMemory(lpBuffer, env, vLength + 1);

	return vLength;
}

BOOL SetEnvironmentVariableEBA(LPSTR* envBlock, LPCSTR lpName, LPCSTR lpValue)
{
	int length;
	char* envstr;
	char* newEB;

	if (!lpName)
		return FALSE;

	if (lpValue)
	{
		length = (int) (strlen(lpName) + strlen(lpValue) + 2); /* +2 because of = and \0 */
		envstr = (char*) malloc(length + 1); /* +1 because of closing \0 */

		if (!envstr)
			return FALSE;

		sprintf_s(envstr, length, "%s=%s", lpName, lpValue);
	}
	else
	{
		length = (int) strlen(lpName) + 2; /* +2 because of = and \0 */
		envstr = (char*) malloc(length + 1); /* +1 because of closing \0 */

		if (!envstr)
			return FALSE;

		sprintf_s(envstr, length, "%s=", lpName);
	}

	envstr[length] = '\0';

	newEB = MergeEnvironmentStrings((LPCSTR) *envBlock, envstr);

	free(envstr);
	free(*envBlock);

	*envBlock = newEB;

	return TRUE;
}

char** EnvironmentBlockToEnvpA(LPCH lpszEnvironmentBlock)
{
	char* p;
	int index;
	int count;
	int length;
	char** envp = NULL;

	count = 0;
	if (!lpszEnvironmentBlock)
		return NULL;

	p = (char*) lpszEnvironmentBlock;

	while (p[0] && p[1])
	{
		length = (int) strlen(p);
		p += (length + 1);
		count++;
	}

	index = 0;
	p = (char*) lpszEnvironmentBlock;

	envp = (char**) calloc(count + 1, sizeof(char*));
	if (!envp)
		return NULL;
	envp[count] = NULL;

	while (p[0] && p[1])
	{
		length = (int) strlen(p);
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
