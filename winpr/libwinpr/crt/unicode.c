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
	LPWSTR targetStart;
	const BYTE* sourceStart;
	ConversionResult result;

	/* If cbMultiByte is 0, the function fails */

	if (cbMultiByte == 0)
		return 0;

	/* If cbMultiByte is -1, the string is null-terminated */

	if (cbMultiByte == -1)
		cbMultiByte = strlen((char*) lpMultiByteStr) + 1;

	/*
	 * if cchWideChar is 0, the function returns the required buffer size
	 * in characters for lpWideCharStr and makes no use of the output parameter itself.
	 */

	if (cchWideChar == 0)
	{
		sourceStart = (const BYTE*) lpMultiByteStr;
		targetStart = (WCHAR*) NULL;

		result = ConvertUTF8toUTF16(&sourceStart, &sourceStart[cbMultiByte],
				&targetStart, NULL, strictConversion);

		length = targetStart - ((WCHAR*) NULL);
		cchWideChar = length;
	}
	else
	{
		sourceStart = (const BYTE*) lpMultiByteStr;
		targetStart = lpWideCharStr;

		result = ConvertUTF8toUTF16(&sourceStart, &sourceStart[cbMultiByte],
				&targetStart, &targetStart[cchWideChar], strictConversion);

		length = targetStart - ((WCHAR*) lpWideCharStr);
		cchWideChar = length;
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
	BYTE* targetStart;
	const WCHAR* sourceStart;
	ConversionResult result;

	/* If cchWideChar is 0, the function fails */

	if (cchWideChar == 0)
		return 0;

	/* If cchWideChar is -1, the string is null-terminated */

	if (cchWideChar == -1)
		cchWideChar = _wcslen(lpWideCharStr) + 1;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */

	if (cbMultiByte == 0)
	{
		sourceStart = (WCHAR*) lpWideCharStr;
		targetStart = (BYTE*) NULL;

		result = ConvertUTF16toUTF8(&sourceStart, &sourceStart[cchWideChar],
				&targetStart, NULL, strictConversion);

		length = targetStart - ((BYTE*) NULL);
		cbMultiByte = length;
	}
	else
	{
		sourceStart = (WCHAR*) lpWideCharStr;
		targetStart = (BYTE*) lpMultiByteStr;

		result = ConvertUTF16toUTF8(&sourceStart, &sourceStart[cchWideChar],
				&targetStart, &targetStart[cbMultiByte], strictConversion);

		length = targetStart - ((BYTE*) lpMultiByteStr);
		cbMultiByte = length;
	}

	return cbMultiByte;
}

#endif

