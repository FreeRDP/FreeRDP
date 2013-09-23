/**
 * WinPR: Windows Portable Runtime
 * Process Environment Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/environment.h>

#ifndef _WIN32

#include <winpr/crt.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
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

HANDLE GetStdHandle(DWORD nStdHandle)
{
	return NULL;
}

BOOL SetStdHandle(DWORD nStdHandle, HANDLE hHandle)
{
	return TRUE;
}

BOOL SetStdHandleEx(DWORD dwStdHandle, HANDLE hNewHandle, HANDLE* phOldHandle)
{
	return TRUE;
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

DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize)
{
	int length;
	char* env = NULL;

	env = getenv(lpName);

	if (!env)
		return 0;

	length = strlen(env);

	if ((length + 1 > nSize) || (!lpBuffer))
		return length + 1;

	CopyMemory(lpBuffer, env, length + 1);

	return length;
}

DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize)
{
	return 0;
}

BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue)
{
	int length;
	char* envstr;

	if (!lpName)
		return FALSE;

	if (lpValue)
	{
		length = strlen(lpName) + strlen(lpValue) + 1;
		envstr = (char*) malloc(length + 1);
		sprintf_s(envstr, length + 1, "%s=%s", lpName, lpValue);
		envstr[length] = '\0';
		putenv(envstr);
	}
	else
	{
		unsetenv(lpName);
	}

	return TRUE;
}

BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue)
{
	return TRUE;
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

LPCH GetEnvironmentStrings(VOID)
{
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

	while (*envp)
	{
		length = strlen(*envp);

		while ((offset + length + 8) > cchEnvironmentBlock)
		{
			cchEnvironmentBlock *= 2;
			lpszEnvironmentBlock = (LPCH) realloc(lpszEnvironmentBlock, cchEnvironmentBlock * sizeof(CHAR));
		}

		p = &(lpszEnvironmentBlock[offset]);

		CopyMemory(p, *envp, length * sizeof(CHAR));
		p[length] = '\0';

		offset += (length + 1);
		envp++;
	}

	lpszEnvironmentBlock[offset] = '\0';

	return lpszEnvironmentBlock;
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
	if (lpszEnvironmentBlock)
		free(lpszEnvironmentBlock);

	return TRUE;
}

BOOL FreeEnvironmentStringsW(LPWCH lpszEnvironmentBlock)
{
	return TRUE;
}

#endif

