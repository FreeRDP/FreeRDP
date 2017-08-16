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
#include <winpr/error.h>
#include <winpr/print.h>

#ifndef _WIN32

#if defined(WITH_ICU)
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#else
#include "utf.h"
#endif

#include "../log.h"
#define TAG WINPR_TAG("unicode")

/**
 * Notes on cross-platform Unicode portability:
 *
 * Unicode has many possible Unicode Transformation Format (UTF) encodings,
 * where some of the most commonly used are UTF-8, UTF-16 and sometimes UTF-32.
 *
 * The number in the UTF encoding name (8, 16, 32) refers to the number of bits
 * per code unit. A code unit is the minimal bit combination that can represent
 * a unit of encoded text in the given encoding. For instance, UTF-8 encodes
 * the English alphabet using 8 bits (or one byte) each, just like in ASCII.
 *
 * However, the total number of code points (values in the Unicode codespace)
 * only fits completely within 32 bits. This means that for UTF-8 and UTF-16,
 * more than one code unit may be required to fully encode a specific value.
 * UTF-8 and UTF-16 are variable-width encodings, while UTF-32 is fixed-width.
 *
 * UTF-8 has the advantage of being backwards compatible with ASCII, and is
 * one of the most commonly used Unicode encoding.
 *
 * UTF-16 is used everywhere in the Windows API. The strategy employed by
 * Microsoft to provide backwards compatibility in their API was to create
 * an ANSI and a Unicode version of the same function, ending with A (ANSI)
 * and W (Wide character, or UTF-16 Unicode). In headers, the original
 * function name is replaced by a macro that defines to either the ANSI
 * or Unicode version based on the definition of the _UNICODE macro.
 *
 * UTF-32 has the advantage of being fixed width, but wastes a lot of space
 * for English text (4x more than UTF-8, 2x more than UTF-16).
 *
 * In C, wide character strings are often defined with the wchar_t type.
 * Many functions are provided to deal with those wide character strings,
 * such as wcslen (strlen equivalent) or wprintf (printf equivalent).
 *
 * This may lead to some confusion, since many of these functions exist
 * on both Windows and Linux, but they are *not* the same!
 *
 * This sample hello world is a good example:
 *
 * #include <wchar.h>
 *
 * wchar_t hello[] = L"Hello, World!\n";
 *
 * int main(int argc, char** argv)
 * {
 * 	wprintf(hello);
 * 	wprintf(L"sizeof(wchar_t): %d\n", sizeof(wchar_t));
 * 	return 0;
 * }
 *
 * There is a reason why the sample prints the size of the wchar_t type:
 * On Windows, wchar_t is two bytes (UTF-16), while on most other systems
 * it is 4 bytes (UTF-32). This means that if you write code on Windows,
 * use L"" to define a string which is meant to be UTF-16 and not UTF-32,
 * you will have a little surprise when trying to port your code to Linux.
 *
 * Since the Windows API uses UTF-16, not UTF-32, WinPR defines the WCHAR
 * type to always be 2-bytes long and uses it instead of wchar_t. Do not
 * ever use wchar_t with WinPR unless you know what you are doing.
 *
 * As for L"", it is unfortunately unusable in a portable way, unless a
 * special option is passed to GCC to define wchar_t as being two bytes.
 * For string constants that must be UTF-16, it is a pain, but they can
 * be defined in a portable way like this:
 *
 * WCHAR hello[] = { 'H','e','l','l','o','\0' };
 *
 * Such strings cannot be passed to native functions like wcslen(), which
 * may expect a different wchar_t size. For this reason, WinPR provides
 * _wcslen, which expects UTF-16 WCHAR strings on all platforms.
 *
 */

/*
 * Conversion to Unicode (UTF-16)
 * MultiByteToWideChar: http://msdn.microsoft.com/en-us/library/windows/desktop/dd319072/
 *
 * cbMultiByte is an input size in bytes (BYTE)
 * cchWideChar is an output size in wide characters (WCHAR)
 *
 * Null-terminated UTF-8 strings:
 *
 * cchWideChar *cannot* be assumed to be cbMultiByte since UTF-8 is variable-width!
 *
 * Instead, obtain the required cchWideChar output size like this:
 * cchWideChar = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) lpMultiByteStr, -1, NULL, 0);
 *
 * A value of -1 for cbMultiByte indicates that the input string is null-terminated,
 * and the null terminator *will* be processed. The size returned by MultiByteToWideChar
 * will therefore include the null terminator. Equivalent behavior can be obtained by
 * computing the length in bytes of the input buffer, including the null terminator:
 *
 * cbMultiByte = strlen((char*) lpMultiByteStr) + 1;
 *
 * An output buffer of the proper size can then be allocated:
 *
 * lpWideCharStr = (LPWSTR) malloc(cchWideChar * sizeof(WCHAR));
 *
 * Since cchWideChar is an output size in wide characters, the actual buffer size is:
 * (cchWideChar * sizeof(WCHAR)) or (cchWideChar * 2)
 *
 * Finally, perform the conversion:
 *
 * cchWideChar = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) lpMultiByteStr, -1, lpWideCharStr, cchWideChar);
 *
 * The value returned by MultiByteToWideChar corresponds to the number of wide characters written
 * to the output buffer, and should match the value obtained on the first call to MultiByteToWideChar.
 *
 */

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
                        int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
{
	LPWSTR targetStart;
#if !defined(WITH_ICU)
	const BYTE* sourceStart;
	int length;
	ConversionResult result;
#endif

	/* If cbMultiByte is 0, the function fails */

	if ((cbMultiByte == 0) || (cbMultiByte < -1))
		return 0;

	/* If cbMultiByte is -1, the string is null-terminated */

	if (cbMultiByte == -1)
		cbMultiByte = strlen((char*) lpMultiByteStr) + 1;

	/*
	 * if cchWideChar is 0, the function returns the required buffer size
	 * in characters for lpWideCharStr and makes no use of the output parameter itself.
	 */
#if defined(WITH_ICU)
	{
		UErrorCode error;
		int32_t targetLength;
		int32_t targetCapacity;

		switch (CodePage)
		{
		case CP_UTF8:
			break;
		default:
			WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
			return 0;
		}

		if (cbMultiByte > UINT32_MAX)
			return 0;

		targetStart = lpWideCharStr;
		targetCapacity = cchWideChar;

		error = U_ZERO_ERROR;
		if (cchWideChar == 0)
		{
			u_strFromUTF8(NULL, 0, &targetLength,
					lpMultiByteStr, cbMultiByte, &error);
			cchWideChar = targetLength;
		}
		else
		{
			u_strFromUTF8(targetStart, targetCapacity, &targetLength,
					lpMultiByteStr, cbMultiByte, &error);
			cchWideChar = U_SUCCESS(error) ? targetLength : 0;
		}
	}
#else
	if (cchWideChar == 0)
	{
		sourceStart = (const BYTE*) lpMultiByteStr;
		targetStart = (WCHAR*) NULL;

		result = ConvertUTF8toUTF16(&sourceStart, &sourceStart[cbMultiByte],
		                            &targetStart, NULL, strictConversion);
		length = targetStart - ((WCHAR*) NULL);
	}
	else
	{
		sourceStart = (const BYTE*) lpMultiByteStr;
		targetStart = lpWideCharStr;

		result = ConvertUTF8toUTF16(&sourceStart, &sourceStart[cbMultiByte],
		                            &targetStart, &targetStart[cchWideChar], strictConversion);
		length = targetStart - ((WCHAR*) lpWideCharStr);
	}

	cchWideChar = (result == conversionOK) ? length : 0;
#endif
	return cchWideChar;
}

/*
 * Conversion from Unicode (UTF-16)
 * WideCharToMultiByte: http://msdn.microsoft.com/en-us/library/windows/desktop/dd374130/
 *
 * cchWideChar is an input size in wide characters (WCHAR)
 * cbMultiByte is an output size in bytes (BYTE)
 *
 * Null-terminated UTF-16 strings:
 *
 * cbMultiByte *cannot* be assumed to be cchWideChar since UTF-8 is variable-width!
 *
 * Instead, obtain the required cbMultiByte output size like this:
 * cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) lpWideCharStr, -1, NULL, 0, NULL, NULL);
 *
 * A value of -1 for cbMultiByte indicates that the input string is null-terminated,
 * and the null terminator *will* be processed. The size returned by WideCharToMultiByte
 * will therefore include the null terminator. Equivalent behavior can be obtained by
 * computing the length in bytes of the input buffer, including the null terminator:
 *
 * cchWideChar = _wcslen((WCHAR*) lpWideCharStr) + 1;
 *
 * An output buffer of the proper size can then be allocated:
 * lpMultiByteStr = (LPSTR) malloc(cbMultiByte);
 *
 * Since cbMultiByte is an output size in bytes, it is the same as the buffer size
 *
 * Finally, perform the conversion:
 *
 * cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) lpWideCharStr, -1, lpMultiByteStr, cbMultiByte, NULL, NULL);
 *
 * The value returned by WideCharToMultiByte corresponds to the number of bytes written
 * to the output buffer, and should match the value obtained on the first call to WideCharToMultiByte.
 *
 */

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
#if !defined(WITH_ICU)
	int length;
	const WCHAR* sourceStart;
	ConversionResult result;
	BYTE* targetStart;
#else
	char* targetStart;
#endif

	/* If cchWideChar is 0, the function fails */

	if ((cchWideChar == 0) || (cchWideChar < -1))
		return 0;

	/* If cchWideChar is -1, the string is null-terminated */

	if (cchWideChar == -1)
		cchWideChar = _wcslen(lpWideCharStr) + 1;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */
#if defined(WITH_ICU)
	{
		UErrorCode error;
		int32_t targetLength;
		int32_t targetCapacity;

		switch(CodePage)
		{
		case CP_UTF8:
			break;
		default:
			WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
			return 0;
		}

		if (cchWideChar > UINT32_MAX)
			return 0;

		targetStart = lpMultiByteStr;
		targetCapacity = cbMultiByte;

		error = U_ZERO_ERROR;
		if (cbMultiByte == 0)
		{
			u_strToUTF8(NULL, 0, &targetLength,
					lpWideCharStr, cchWideChar, &error);
			cbMultiByte = targetLength;
		}
		else
		{
			u_strToUTF8(targetStart, targetCapacity, &targetLength,
					lpWideCharStr, cchWideChar, &error);
			cbMultiByte = U_SUCCESS(error) ? targetLength : 0;
		}
	}
#else
	if (cbMultiByte == 0)
	{
		sourceStart = (WCHAR*) lpWideCharStr;
		targetStart = (BYTE*) NULL;

		result = ConvertUTF16toUTF8(&sourceStart, &sourceStart[cchWideChar],
		                            &targetStart, NULL, strictConversion);

		length = targetStart - ((BYTE*) NULL);
	}
	else
	{
		sourceStart = (WCHAR*) lpWideCharStr;
		targetStart = (BYTE*) lpMultiByteStr;

		result = ConvertUTF16toUTF8(&sourceStart, &sourceStart[cchWideChar],
		                            &targetStart, &targetStart[cbMultiByte], strictConversion);
		length = targetStart - ((BYTE*) lpMultiByteStr);
	}

	cbMultiByte = (result == conversionOK) ? length : 0;
#endif
	return cbMultiByte;
}

#endif

/**
 * ConvertToUnicode is a convenience wrapper for MultiByteToWideChar:
 *
 * If the lpWideCharStr parameter for the converted string points to NULL
 * or if the cchWideChar parameter is set to 0 this function will automatically
 * allocate the required memory which is guaranteed to be null-terminated
 * after the conversion, even if the source c string isn't.
 *
 * If the cbMultiByte parameter is set to -1 the passed lpMultiByteStr must
 * be null-terminated and the required length for the converted string will be
 * calculated accordingly.
 */

int ConvertToUnicode(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
                     int cbMultiByte, LPWSTR* lpWideCharStr, int cchWideChar)
{
	int status;
	BOOL allocate = FALSE;

	if (!lpMultiByteStr)
		return 0;

	if (!lpWideCharStr)
		return 0;

	if (cbMultiByte == -1)
		cbMultiByte = (int)(strlen(lpMultiByteStr) + 1);

	if (cchWideChar == 0)
	{
		cchWideChar = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, NULL, 0);
		allocate = TRUE;
	}

	if (cchWideChar < 1)
		return 0;

	if (!(*lpWideCharStr))
		allocate = TRUE;

	if (allocate)
	{
		*lpWideCharStr = (LPWSTR) calloc(cchWideChar + 1, sizeof(WCHAR));

		if (!(*lpWideCharStr))
		{
			//SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return 0;
		}
	}

	status = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, *lpWideCharStr,
	                             cchWideChar);

	if (status != cchWideChar)
	{
		if (allocate)
		{
			free(*lpWideCharStr);
			*lpWideCharStr = NULL;
		}

		status = 0;
	}

	return status;
}

/**
 * ConvertFromUnicode is a convenience wrapper for WideCharToMultiByte:
 *
 * If the lpMultiByteStr parameter for the converted string points to NULL
 * or if the cbMultiByte parameter is set to 0 this function will automatically
 * allocate the required memory which is guaranteed to be null-terminated
 * after the conversion, even if the source unicode string isn't.
 *
 * If the cchWideChar parameter is set to -1 the passed lpWideCharStr must
 * be null-terminated and the required length for the converted string will be
 * calculated accordingly.
 */

int ConvertFromUnicode(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                       LPSTR* lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
	int status;
	BOOL allocate = FALSE;

	if (!lpWideCharStr)
		return 0;

	if (!lpMultiByteStr)
		return 0;

	if (cchWideChar == -1)
		cchWideChar = (int)(_wcslen(lpWideCharStr) + 1);

	if (cbMultiByte == 0)
	{
		cbMultiByte = WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, NULL, 0, NULL,
		                                  NULL);
		allocate = TRUE;
	}

	if (cbMultiByte < 1)
		return 0;

	if (!(*lpMultiByteStr))
		allocate = TRUE;

	if (allocate)
	{
		*lpMultiByteStr = (LPSTR) calloc(1, cbMultiByte + 1);

		if (!(*lpMultiByteStr))
		{
			//SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return 0;
		}
	}

	status = WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar,
	                             *lpMultiByteStr, cbMultiByte, lpDefaultChar, lpUsedDefaultChar);

	if ((status != cbMultiByte) && allocate)
	{
		status = 0;
	}

	if ((status <= 0) && allocate)
	{
		free(*lpMultiByteStr);
		*lpMultiByteStr = NULL;
	}

	return status;
}

/**
 * Swap Unicode byte order (UTF16LE <-> UTF16BE)
 */

void ByteSwapUnicode(WCHAR* wstr, int length)
{
	WCHAR* end = &wstr[length];

	while (wstr < end)
	{
		*wstr = _byteswap_ushort(*wstr);
		wstr++;
	}
}
