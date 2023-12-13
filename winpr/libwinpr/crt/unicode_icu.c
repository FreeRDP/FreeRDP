/**
 * WinPR: Windows Portable Runtime
 * Unicode Conversion (CRT)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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
#include <winpr/assert.h>

#include <errno.h>
#include <wctype.h>

#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/print.h>

#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

#include <unicode/ucnv.h>
#include <unicode/ustring.h>

#include "unicode.h"

#include "../log.h"
#define TAG WINPR_TAG("unicode")

#define UCNV_CONVERT 1

int int_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                            LPWSTR lpWideCharStr, int cchWideChar)
{
	const BOOL isNullTerminated = cbMultiByte < 0;

	WINPR_UNUSED(dwFlags);

	/* If cbMultiByte is 0, the function fails */

	if ((cbMultiByte == 0) || (cbMultiByte < -1))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	size_t len = 0;
	if (isNullTerminated)
		len = strlen(lpMultiByteStr) + 1;
	else
		len = cbMultiByte;

	if (len >= INT_MAX)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	cbMultiByte = (int)len;

	/*
	 * if cchWideChar is 0, the function returns the required buffer size
	 * in characters for lpWideCharStr and makes no use of the output parameter itself.
	 */
	{
		UErrorCode error = U_ZERO_ERROR;
		int32_t targetLength = -1;

		switch (CodePage)
		{
			case CP_ACP:
			case CP_UTF8:
				break;

			default:
				WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
				SetLastError(ERROR_INVALID_PARAMETER);
				return 0;
		}

		const int32_t targetCapacity = cchWideChar;
#if defined(UCNV_CONVERT)
		char* targetStart = (char*)lpWideCharStr;
		targetLength =
		    ucnv_convert("UTF-16LE", "UTF-8", targetStart, targetCapacity * (int32_t)sizeof(WCHAR),
		                 lpMultiByteStr, cbMultiByte, &error);
		if (targetLength > 0)
			targetLength /= sizeof(WCHAR);
#else
		WCHAR* targetStart = lpWideCharStr;
		u_strFromUTF8(targetStart, targetCapacity, &targetLength, lpMultiByteStr, cbMultiByte,
		              &error);
#endif

		switch (error)
		{
			case U_BUFFER_OVERFLOW_ERROR:
				if (targetCapacity > 0)
				{
					cchWideChar = 0;
					WLog_ERR(TAG, "insufficient buffer supplied, got %d, required %d",
					         targetCapacity, targetLength);
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
				}
				else
					cchWideChar = targetLength;
				break;
			case U_STRING_NOT_TERMINATED_WARNING:
				cchWideChar = targetLength;
				break;
			case U_ZERO_ERROR:
				cchWideChar = targetLength;
				break;
			default:
				WLog_WARN(TAG, "unexpected ICU error code %s [0x%08" PRIx32 "]", u_errorName(error),
				          error);
				if (U_FAILURE(error))
				{
					WLog_ERR(TAG, "unexpected ICU error code %s [0x%08" PRIx32 "] is fatal",
					         u_errorName(error), error);
					cchWideChar = 0;
					SetLastError(ERROR_NO_UNICODE_TRANSLATION);
				}
				else
					cchWideChar = targetLength;
				break;
		}
	}

	return cchWideChar;
}

int int_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                            LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar,
                            LPBOOL lpUsedDefaultChar)
{
	/* If cchWideChar is 0, the function fails */

	if ((cchWideChar == 0) || (cchWideChar < -1))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}

	/* If cchWideChar is -1, the string is null-terminated */

	size_t len = 0;
	if (cchWideChar == -1)
		len = _wcslen(lpWideCharStr) + 1;
	else
		len = cchWideChar;

	if (len >= INT32_MAX)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	cchWideChar = (int)len;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */
	{
		UErrorCode error = U_ZERO_ERROR;
		int32_t targetLength = -1;

		switch (CodePage)
		{
			case CP_ACP:
			case CP_UTF8:
				break;

			default:
				WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
				SetLastError(ERROR_INVALID_PARAMETER);
				return 0;
		}

		char* targetStart = lpMultiByteStr;
		const int32_t targetCapacity = cbMultiByte;
#if defined(UCNV_CONVERT)
		const char* str = (const char*)lpWideCharStr;
		targetLength = ucnv_convert("UTF-8", "UTF-16LE", targetStart, targetCapacity, str,
		                            cchWideChar * (int32_t)sizeof(WCHAR), &error);
#else
		u_strToUTF8(targetStart, targetCapacity, &targetLength, lpWideCharStr, cchWideChar, &error);
#endif
		switch (error)
		{
			case U_BUFFER_OVERFLOW_ERROR:
				if (targetCapacity > 0)
				{
					WLog_ERR(TAG, "insufficient buffer supplied, got %d, required %d",
					         targetCapacity, targetLength);
					cbMultiByte = 0;
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
				}
				else
					cbMultiByte = targetLength;
				break;
			case U_STRING_NOT_TERMINATED_WARNING:
				cbMultiByte = targetLength;
				break;
			case U_ZERO_ERROR:
				cbMultiByte = targetLength;
				break;
			default:
				WLog_WARN(TAG, "unexpected ICU error code %s [0x%08" PRIx32 "]", u_errorName(error),
				          error);
				if (U_FAILURE(error))
				{
					WLog_ERR(TAG, "unexpected ICU error code %s [0x%08" PRIx32 "] is fatal",
					         u_errorName(error), error);
					cbMultiByte = 0;
					SetLastError(ERROR_NO_UNICODE_TRANSLATION);
				}
				else
					cbMultiByte = targetLength;
				break;
		}
	}
	return cbMultiByte;
}
