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

int int_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                            LPWSTR lpWideCharStr, int cchWideChar)
{
	const BOOL isNullTerminated = cbMultiByte < 0;
	LPWSTR targetStart;

	WINPR_UNUSED(dwFlags);

	/* If cbMultiByte is 0, the function fails */

	if ((cbMultiByte == 0) || (cbMultiByte < -1))
		return 0;

	size_t len;
	if (isNullTerminated)
		len = strlen(lpMultiByteStr) + 1;
	else
		len = cbMultiByte;

	if (len >= INT_MAX)
		return -1;
	cbMultiByte = (int)len;

	/*
	 * if cchWideChar is 0, the function returns the required buffer size
	 * in characters for lpWideCharStr and makes no use of the output parameter itself.
	 */
	{
		UErrorCode error;
		int32_t targetLength;
		int32_t targetCapacity;

		switch (CodePage)
		{
			case CP_ACP:
			case CP_UTF8:
				break;

			default:
				WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
				return 0;
		}

		targetStart = lpWideCharStr;
		targetCapacity = cchWideChar;
		error = U_ZERO_ERROR;

		u_strFromUTF8(targetStart, targetCapacity, &targetLength, lpMultiByteStr, cbMultiByte,
		              &error);

		switch (error)
		{
			case U_BUFFER_OVERFLOW_ERROR:
				if (targetCapacity > 0)
				{
					cchWideChar = 0;
					WLog_ERR(TAG, "[%s] insufficient buffer supplied, got %d, required %d",
					         __FUNCTION__, targetCapacity, targetLength);
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
				WLog_WARN(TAG, "[%s] unexpected ICU error code %s [0x%08" PRIx32 "]", __func__,
				          u_errorName(error), error);
				if (U_FAILURE(error))
				{
					WLog_ERR(TAG, "[%s] unexpected ICU error code %s [0x%08" PRIx32 "] is fatal",
					         __func__, u_errorName(error), error);
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
	char* targetStart;

	/* If cchWideChar is 0, the function fails */

	if ((cchWideChar == 0) || (cchWideChar < -1))
		return 0;

	/* If cchWideChar is -1, the string is null-terminated */

	size_t len;
	if (cchWideChar == -1)
		len = _wcslen(lpWideCharStr) + 1;
	else
		len = cchWideChar;

	if (len >= INT32_MAX)
		return 0;
	cchWideChar = (int)len;

	/*
	 * if cbMultiByte is 0, the function returns the required buffer size
	 * in bytes for lpMultiByteStr and makes no use of the output parameter itself.
	 */
	{
		UErrorCode error;
		int32_t targetLength;
		int32_t targetCapacity;

		switch (CodePage)
		{
			case CP_ACP:
			case CP_UTF8:
				break;

			default:
				WLog_ERR(TAG, "Unsupported encoding %u", CodePage);
				return 0;
		}

		targetStart = lpMultiByteStr;
		targetCapacity = cbMultiByte;
		error = U_ZERO_ERROR;

		u_strToUTF8(targetStart, targetCapacity, &targetLength, lpWideCharStr, cchWideChar, &error);
		switch (error)
		{
			case U_BUFFER_OVERFLOW_ERROR:
				if (targetCapacity > 0)
				{
					WLog_ERR(TAG, "[%s] insufficient buffer supplied, got %d, required %d",
					         __FUNCTION__, targetCapacity, targetLength);
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
				WLog_WARN(TAG, "[%s] unexpected ICU error code %s [0x%08" PRIx32 "]", __func__,
				          u_errorName(error), error);
				if (U_FAILURE(error))
				{
					WLog_ERR(TAG, "[%s] unexpected ICU error code %s [0x%08" PRIx32 "] is fatal",
					         __func__, u_errorName(error), error);
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
