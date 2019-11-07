/**
 * WinPR: Windows Portable Runtime
 * Time Zone
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

#ifndef WINPR_TIMEZONE_H
#define WINPR_TIMEZONE_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _WIN32

	typedef struct _TIME_ZONE_INFORMATION
	{
		LONG Bias;
		WCHAR StandardName[32];
		SYSTEMTIME StandardDate;
		LONG StandardBias;
		WCHAR DaylightName[32];
		SYSTEMTIME DaylightDate;
		LONG DaylightBias;
	} TIME_ZONE_INFORMATION, *PTIME_ZONE_INFORMATION, *LPTIME_ZONE_INFORMATION;

	typedef struct _TIME_DYNAMIC_ZONE_INFORMATION
	{
		LONG Bias;
		WCHAR StandardName[32];
		SYSTEMTIME StandardDate;
		LONG StandardBias;
		WCHAR DaylightName[32];
		SYSTEMTIME DaylightDate;
		LONG DaylightBias;
		WCHAR TimeZoneKeyName[128];
		BOOLEAN DynamicDaylightTimeDisabled;
	} DYNAMIC_TIME_ZONE_INFORMATION, *PDYNAMIC_TIME_ZONE_INFORMATION,
	    *LPDYNAMIC_TIME_ZONE_INFORMATION;

	WINPR_API DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation);
	WINPR_API BOOL SetTimeZoneInformation(const TIME_ZONE_INFORMATION* lpTimeZoneInformation);
	WINPR_API BOOL SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime);
	WINPR_API BOOL FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime);
	WINPR_API BOOL SystemTimeToTzSpecificLocalTime(LPTIME_ZONE_INFORMATION lpTimeZone,
	                                               LPSYSTEMTIME lpUniversalTime,
	                                               LPSYSTEMTIME lpLocalTime);
	WINPR_API BOOL TzSpecificLocalTimeToSystemTime(LPTIME_ZONE_INFORMATION lpTimeZoneInformation,
	                                               LPSYSTEMTIME lpLocalTime,
	                                               LPSYSTEMTIME lpUniversalTime);

#endif

/*
 * GetDynamicTimeZoneInformation is provided by the SDK if _WIN32_WINNT >= 0x0600 in SDKs above 7.1A
 * and incorrectly if _WIN32_WINNT >= 0x0501 in older SDKs
 */
#if !defined(_WIN32) ||                                                  \
    (defined(_WIN32) && (defined(NTDDI_WIN8) && _WIN32_WINNT < 0x0600 || \
                         !defined(NTDDI_WIN8) && _WIN32_WINNT < 0x0501)) /* Windows Vista */

	WINPR_API DWORD
	GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation);
	WINPR_API BOOL
	SetDynamicTimeZoneInformation(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation);
	WINPR_API BOOL GetTimeZoneInformationForYear(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi,
	                                             LPTIME_ZONE_INFORMATION ptzi);

#endif

#if !defined(_WIN32) || (defined(_WIN32) && (_WIN32_WINNT < 0x0601)) /* Windows 7 */

	WINPR_API BOOL
	SystemTimeToTzSpecificLocalTimeEx(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation,
	                                  const SYSTEMTIME* lpUniversalTime, LPSYSTEMTIME lpLocalTime);
	WINPR_API BOOL
	TzSpecificLocalTimeToSystemTimeEx(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation,
	                                  const SYSTEMTIME* lpLocalTime, LPSYSTEMTIME lpUniversalTime);

#endif

#if !defined(_WIN32) || (defined(_WIN32) && (_WIN32_WINNT < 0x0602)) /* Windows 8 */

	WINPR_API DWORD EnumDynamicTimeZoneInformation(
	    const DWORD dwIndex, PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation);
	WINPR_API DWORD GetDynamicTimeZoneInformationEffectiveYears(
	    const PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation, LPDWORD FirstYear,
	    LPDWORD LastYear);

#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_TIMEZONE_H */
