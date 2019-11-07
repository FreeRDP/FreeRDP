/**
 * WinPR: Windows Portable Runtime
 * Localization Functions
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/locale.h>

/**
 * api-ms-win-core-localization-l1-2-1.dll:
 *
 * ConvertDefaultLocale
 * EnumSystemGeoID
 * EnumSystemLocalesA
 * EnumSystemLocalesEx
 * EnumSystemLocalesW
 * FindNLSString
 * FindNLSStringEx
 * FormatMessageA
 * FormatMessageW
 * GetACP
 * GetCalendarInfoEx
 * GetCalendarInfoW
 * GetCPInfo
 * GetCPInfoExW
 * GetFileMUIInfo
 * GetFileMUIPath
 * GetGeoInfoW
 * GetLocaleInfoA
 * GetLocaleInfoEx
 * GetLocaleInfoW
 * GetNLSVersion
 * GetNLSVersionEx
 * GetOEMCP
 * GetProcessPreferredUILanguages
 * GetSystemDefaultLangID
 * GetSystemDefaultLCID
 * GetSystemPreferredUILanguages
 * GetThreadLocale
 * GetThreadPreferredUILanguages
 * GetThreadUILanguage
 * GetUILanguageInfo
 * GetUserDefaultLangID
 * GetUserDefaultLCID
 * GetUserDefaultLocaleName
 * GetUserGeoID
 * GetUserPreferredUILanguages
 * IdnToAscii
 * IdnToUnicode
 * IsDBCSLeadByte
 * IsDBCSLeadByteEx
 * IsNLSDefinedString
 * IsValidCodePage
 * IsValidLanguageGroup
 * IsValidLocale
 * IsValidLocaleName
 * IsValidNLSVersion
 * LCMapStringA
 * LCMapStringEx
 * LCMapStringW
 * LocaleNameToLCID
 * ResolveLocaleName
 * SetCalendarInfoW
 * SetLocaleInfoW
 * SetProcessPreferredUILanguages
 * SetThreadLocale
 * SetThreadPreferredUILanguages
 * SetThreadUILanguage
 * SetUserGeoID
 * VerLanguageNameA
 * VerLanguageNameW
 */

#ifndef _WIN32

DWORD WINAPI FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                            LPSTR lpBuffer, DWORD nSize, va_list* Arguments)
{
	return 0;
}

DWORD WINAPI FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                            LPWSTR lpBuffer, DWORD nSize, va_list* Arguments)
{
	return 0;
}

#endif
