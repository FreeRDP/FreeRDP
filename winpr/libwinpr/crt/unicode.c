/**
 * WinPR: Windows Portable Runtime
 * Unicode Conversion (CRT)
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

#include <errno.h>
#include <wctype.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#ifndef _WIN32

#include "utf.h"

/*
 * Advanced String Techniques in C++ - Part I: Unicode
 * http://www.flipcode.com/archives/Advanced_String_Techniques_in_C-Part_I_Unicode.shtml
 */

/*
 * Conversion *to* Unicode
 * MultiByteToWideChar: http://msdn.microsoft.com/en-us/library/windows/desktop/dd319072/
 */

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
		int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
{
	int length;
	const BYTE* sourceStart;
	LPWSTR targetStart;
	ConversionResult result;

	/* If cbMultiByte is 0, the function fails */

	if (cbMultiByte == 0)
		return 0;

	/* If cbMultiByte is -1, the string is null-terminated */

	if (cbMultiByte == -1)
		cbMultiByte = strlen((char*) lpMultiByteStr) + 1;

	if (!lpWideCharStr)
		lpWideCharStr = (LPWSTR) malloc((cbMultiByte + 1) * sizeof(WCHAR) * 4);

	sourceStart = (const BYTE*) lpMultiByteStr;
	targetStart = lpWideCharStr;

	result = ConvertUTF8toUTF16(&sourceStart, &sourceStart[cbMultiByte],
			&targetStart, &targetStart[((cbMultiByte + 1) * 4) / sizeof(WCHAR)], strictConversion);
	length = targetStart - ((WCHAR*) lpWideCharStr);
	lpWideCharStr[length] = '\0';

	cchWideChar = length;

	/*
	 * if cchWideChar is 0, the function returns the required buffer size
	 * in characters for lpWideCharStr and makes no use of the output parameter itself.
	 */

	if (cchWideChar == 0)
	{
		free(lpWideCharStr);
		return cchWideChar;
	}

	return cchWideChar;
}

/*
 * Conversion *from* Unicode
 * WideCharToMultiByte: http://msdn.microsoft.com/en-us/library/windows/desktop/dd374130/
 */

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
	int length;
	const WCHAR* sourceStart;
	BYTE* targetStart;
	ConversionResult result;

	/* If cchWideChar is 0, the function fails */

	if (cchWideChar == 0)
		return 0;

	/* If cchWideChar is -1, the string is null-terminated */

	if (cchWideChar == -1)
		cchWideChar = _wcslen(lpWideCharStr) + 1;

	if (!lpMultiByteStr)
		lpMultiByteStr = (LPSTR) malloc((cchWideChar + 1) * 4);

	sourceStart = (WCHAR*) lpWideCharStr;
	targetStart = (BYTE*) lpMultiByteStr;

	result = ConvertUTF16toUTF8(&sourceStart, &sourceStart[cchWideChar],
			&targetStart, &targetStart[(cchWideChar + 1) * 4], strictConversion);
	length = targetStart - ((BYTE*) lpMultiByteStr);
	lpMultiByteStr[length] = '\0';

	cbMultiByte = length;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */

	if (cbMultiByte == 0)
	{
		free(lpMultiByteStr);
		return cbMultiByte;
	}

	return cbMultiByte;
}

#endif

