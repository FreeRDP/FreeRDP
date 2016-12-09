/**
 * WinPR: Windows Portable Runtime
 * Process Argument Vector Functions
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
#include <winpr/heap.h>
#include <winpr/handle.h>

#include <winpr/thread.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../log.h"
#define TAG WINPR_TAG("thread")

/**
 * CommandLineToArgvW function:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/bb776391/
 *
 * CommandLineToArgvW has a special interpretation of backslash characters
 * when they are followed by a quotation mark character ("), as follows:
 *
 * 2n backslashes followed by a quotation mark produce n backslashes followed by a quotation mark.
 * (2n) + 1 backslashes followed by a quotation mark again produce n backslashes followed by a quotation mark.
 * n backslashes not followed by a quotation mark simply produce n backslashes.
 *
 * The address returned by CommandLineToArgvW is the address of the first element in an array of LPWSTR values;
 * the number of pointers in this array is indicated by pNumArgs. Each pointer to a null-terminated Unicode
 * string represents an individual argument found on the command line.
 *
 * CommandLineToArgvW allocates a block of contiguous memory for pointers to the argument strings,
 * and for the argument strings themselves; the calling application must free the memory used by the
 * argument list when it is no longer needed. To free the memory, use a single call to the LocalFree function.
 */

/**
 * Parsing C++ Command-Line Arguments:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft
 *
 * Microsoft C/C++ startup code uses the following rules when
 * interpreting arguments given on the operating system command line:
 *
 * Arguments are delimited by white space, which is either a space or a tab.
 *
 * The caret character (^) is not recognized as an escape character or delimiter.
 * The character is handled completely by the command-line parser in the operating
 * system before being passed to the argv array in the program.
 *
 * A string surrounded by double quotation marks ("string") is interpreted as a
 * single argument, regardless of white space contained within. A quoted string
 * can be embedded in an argument.
 *
 * A double quotation mark preceded by a backslash (\") is interpreted as a
 * literal double quotation mark character (").
 *
 * Backslashes are interpreted literally, unless they immediately
 * precede a double quotation mark.
 *
 * If an even number of backslashes is followed by a double quotation mark,
 * one backslash is placed in the argv array for every pair of backslashes,
 * and the double quotation mark is interpreted as a string delimiter.
 *
 * If an odd number of backslashes is followed by a double quotation mark,
 * one backslash is placed in the argv array for every pair of backslashes,
 * and the double quotation mark is "escaped" by the remaining backslash,
 * causing a literal double quotation mark (") to be placed in argv.
 *
 */

LPSTR* CommandLineToArgvA(LPCSTR lpCmdLine, int* pNumArgs)
{
	char* p;
	int length;
	char* pBeg;
	char* pEnd;
	char* buffer;
	char* pOutput;
	int numArgs;
	LPSTR* pArgs;
	int maxNumArgs;
	int maxBufferSize;
	int cmdLineLength;
	BOOL* lpEscapedChars;
	LPSTR lpEscapedCmdLine;

	if (!lpCmdLine)
		return NULL;

	if (!pNumArgs)
		return NULL;

	pArgs = NULL;
	numArgs = 0;
	lpEscapedCmdLine = NULL;
	cmdLineLength = (int) strlen(lpCmdLine);
	lpEscapedChars = (BOOL*) calloc(1, (cmdLineLength + 1) * sizeof(BOOL));
	if (!lpEscapedChars)
		return NULL;

	if (strstr(lpCmdLine, "\\\""))
	{
		int i, n;
		char* pLastEnd = NULL;
		lpEscapedCmdLine = (char*) malloc((cmdLineLength + 1) * sizeof(char));
		if (!lpEscapedCmdLine)
		{
			free(lpEscapedChars);
			return NULL;
		}
		p = (char*) lpCmdLine;
		pLastEnd = (char*) lpCmdLine;
		pOutput = (char*) lpEscapedCmdLine;

		while (p < &lpCmdLine[cmdLineLength])
		{
			pBeg = strstr(p, "\\\"");

			if (!pBeg)
			{
				length = (int) strlen(p);
				CopyMemory(pOutput, p, length);
				pOutput += length;
				p += length;
				break;
			}

			pEnd = pBeg + 2;

			while (pBeg >= lpCmdLine)
			{
				if (*pBeg != '\\')
				{
					pBeg++;
					break;
				}

				pBeg--;
			}

			n = (int) ((pEnd - pBeg) - 1);
			length = (int) (pBeg - pLastEnd);
			CopyMemory(pOutput, p, length);
			pOutput += length;
			p += length;

			for (i = 0; i < (n / 2); i++)
				*pOutput++ = '\\';

			p += n + 1;

			if ((n % 2) != 0)
				lpEscapedChars[pOutput - lpEscapedCmdLine] = TRUE;

			*pOutput++ = '"';
			pLastEnd = p;
		}

		*pOutput++ = '\0';
		lpCmdLine = (LPCSTR) lpEscapedCmdLine;
		cmdLineLength = (int) strlen(lpCmdLine);
	}

	maxNumArgs = 2;
	p = (char*) lpCmdLine;

	while (p < lpCmdLine + cmdLineLength)
	{
		p += strcspn(p, " \t");
		p += strspn(p, " \t");
		maxNumArgs++;
	}

	maxBufferSize = (maxNumArgs * (sizeof(char*))) + (cmdLineLength + 1);
	buffer = (char*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, maxBufferSize);

	if (!buffer)
		return NULL;

	pArgs = (LPSTR*) buffer;
	pOutput = (char*) &buffer[maxNumArgs * (sizeof(char*))];
	numArgs = 0;
	p = (char*) lpCmdLine;

	while (p < lpCmdLine + cmdLineLength)
	{
		pBeg = p;

		while (1)
		{
			p += strcspn(p, " \t\"\0");
			if ((*p != '"') || !lpEscapedChars[p - lpCmdLine])
				break;
			p++;
		}

		if (*p != '"')
		{
			/* no whitespace escaped with double quotes */
			length = (int) (p - pBeg);
			CopyMemory(pOutput, pBeg, length);
			pOutput[length] = '\0';
			pArgs[numArgs++] = pOutput;
			pOutput += (length + 1);
		}
		else
		{
			p++;

			while (1)
			{
				p += strcspn(p, "\"\0");
				if ((*p != '"') || !lpEscapedChars[p - lpCmdLine])
					break;
				p++;
			}

			if (*p != '"')
				WLog_ERR(TAG, "parsing error: uneven number of unescaped double quotes!");

			if (*p && *(++p))
				p += strcspn(p, " \t\0");

			pArgs[numArgs++] = pOutput;

			while (pBeg < p)
			{
				if (*pBeg != '"')
					*pOutput++ = *pBeg;
				pBeg++;
			}

			*pOutput++ = '\0';
		}

		p += strspn(p, " \t");
	}

	free(lpEscapedCmdLine);
	free(lpEscapedChars);

	*pNumArgs = numArgs;
	return pArgs;
}

#ifndef _WIN32

LPWSTR* CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs)
{
	return NULL;
}

#endif
