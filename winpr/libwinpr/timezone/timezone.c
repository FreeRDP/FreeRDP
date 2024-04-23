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

#include <winpr/config.h>

#include <winpr/environment.h>
#include <winpr/wtypes.h>
#include <winpr/timezone.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/file.h>
#include "../log.h"

#define TAG WINPR_TAG("timezone")

#if defined(WITH_TIMEZONE_BUILTIN)
#include "WindowsZones.h"
#include "TimeZones.h"
#else
#include <unicode/ucal.h>
#endif

#ifndef _WIN32

#include <time.h>
#include <unistd.h>

#endif

#if defined(WITH_TIMEZONE_BUILTIN)
static BOOL winpr_match_unix_timezone_identifier_with_list(const char* tzid, const char* list)
{
	char* p = NULL;
	char* list_copy = NULL;
	char* context = NULL;

	list_copy = _strdup(list);

	if (!list_copy)
		return FALSE;

	p = strtok_s(list_copy, " ", &context);

	while (p != NULL)
	{
		if (strcmp(p, tzid) == 0)
		{
			free(list_copy);
			return TRUE;
		}

		p = strtok_s(NULL, " ", &context);
	}

	free(list_copy);
	return FALSE;
}
#endif

#if defined(WITH_TIMEZONE_BUILTIN)
static TIME_ZONE_INFORMATION* winpr_get_from_id_builtin(const char* tzid, const WCHAR* wzid);
#elif defined(_WIN32)
static TIME_ZONE_INFORMATION* winpr_get_from_id_win32(const char* tzid, const WCHAR* wzid)
{
	DWORD dwResult = 0;
	DWORD i = 0;
	TIME_ZONE_INFORMATION* entry = NULL;

	WINPR_UNUSED(tzid);
	do
	{
		DYNAMIC_TIME_ZONE_INFORMATION dynamicTimezone = { 0 };
		dwResult = EnumDynamicTimeZoneInformation(i++, &dynamicTimezone);
		if (dwResult == ERROR_SUCCESS)
		{
			if (_wcscmp(wzid, dynamicTimezone.StandardName) == 0)
			{
				entry = calloc(1, sizeof(TIME_ZONE_INFORMATION));
				if (entry)
				{
					entry->Bias = dynamicTimezone.Bias;
					wcsncpy_s(entry->StandardName, ARRAYSIZE(entry->StandardName),
					          dynamicTimezone.StandardName,
					          ARRAYSIZE(dynamicTimezone.StandardName));
					entry->StandardDate = dynamicTimezone.StandardDate;
					entry->StandardBias = dynamicTimezone.StandardBias;
					wcsncpy_s(entry->DaylightName, ARRAYSIZE(entry->DaylightName),
					          dynamicTimezone.DaylightName,
					          ARRAYSIZE(dynamicTimezone.DaylightName));
					entry->DaylightDate = dynamicTimezone.DaylightDate;
					entry->DaylightBias = dynamicTimezone.DaylightBias;
				}
			}
		}
	} while ((dwResult != ERROR_NO_MORE_ITEMS) && (entry == NULL));
	return entry;
}
#elif !defined(_WIN32)
static TIME_ZONE_INFORMATION* winpr_get_from_id_icu(const char* tzid, const WCHAR* wzid)
{
	TIME_ZONE_INFORMATION* entry = NULL;
	UChar* utzid = ConvertUtf8ToWCharAlloc(tzid, NULL);
	if (!utzid)
		goto end;

	UErrorCode error = U_ZERO_ERROR;

	int32_t standardOffset = 0;
	int32_t zoneOffset = 0;
	int32_t dstOffset = 0;

	const char* locale = NULL;
	UCalendar* cal = ucal_open(utzid, -1, locale, UCAL_DEFAULT, &error);
	if (!cal || U_FAILURE(error))
		goto fail;

	dstOffset = ucal_get(cal, UCAL_DST_OFFSET, &error);
	if (U_FAILURE(error))
		goto fail;
	dstOffset /= 1000 * 60;
	zoneOffset = ucal_get(cal, UCAL_ZONE_OFFSET, &error);
	if (U_FAILURE(error))
		goto fail;
	zoneOffset /= 1000 * 60;

fail:
	ucal_close(cal);
	free(utzid);
	if (U_FAILURE(error))
		goto end;

	entry = calloc(1, sizeof(TIME_ZONE_INFORMATION));
	if (!entry)
		goto end;

	memcpy(entry->StandardName, wzid,
	       _wcsnlen(wzid, ARRAYSIZE(entry->StandardName) - 1) * sizeof(WCHAR));
	entry->StandardBias = standardOffset;

	memcpy(entry->DaylightName, wzid,
	       _wcsnlen(wzid, ARRAYSIZE(entry->DaylightName) - 1) * sizeof(WCHAR));
	entry->DaylightBias = dstOffset;

	entry->Bias = -zoneOffset;
	entry->DaylightBias = -dstOffset;
	entry->StandardBias = -standardOffset;

end:
	return entry;
}
#endif

static WCHAR* tz_map_iana_to_windows(const char* tzid)
{
	WCHAR* wzid = NULL;
	if (!tzid)
		goto fail;

#if defined(WITH_TIMEZONE_BUILTIN)
	for (size_t j = 0; j < WindowsTimeZoneIdTableNrElements; j++)
	{
		const WINDOWS_TZID_ENTRY* cur = &WindowsTimeZoneIdTable[j];

		if (winpr_match_unix_timezone_identifier_with_list(tzid, cur->tzid))
		{
			wzid = ConvertUtf8ToWCharAlloc(cur->windows, NULL);
			goto fail;
		}
	}
#else
	size_t utzid_len = 0;
	UChar* utzid = ConvertUtf8ToWCharAlloc(tzid, &utzid_len);
	if (utzid)
	{
		UErrorCode error = U_ZERO_ERROR;

		int32_t rc = ucal_getWindowsTimeZoneID(utzid, utzid_len, NULL, 0, &error);
		if ((error == U_BUFFER_OVERFLOW_ERROR) && (rc > 0))
		{
			rc++; // make space for '\0'
			wzid = calloc((size_t)rc + 1, sizeof(UChar));
			if (wzid)
			{
				UErrorCode error2 = U_ZERO_ERROR;
				int32_t rc2 = ucal_getWindowsTimeZoneID(utzid, utzid_len, wzid, rc, &error2);
				if (U_FAILURE(error2) || (rc2 <= 0))
				{
					free(wzid);
					wzid = NULL;
				}
			}
		}
		free(utzid);
	}
#endif

fail:
	return wzid;
}

static TIME_ZONE_INFORMATION* winpr_get_from_id(const char* tzid, const WCHAR* wzid)
{
#if defined(_WIN32)
	return winpr_get_from_id_win32(tzid, wzid);
#elif !defined(WITH_TIMEZONE_BUILTIN)
	return winpr_get_from_id_icu(tzid, wzid);
#else
	return winpr_get_from_id_builtin(tzid, wzid);
#endif
}

#if !defined(_WIN32)
static UINT64 winpr_windows_gmtime(void)
{
	time_t unix_time = 0;
	UINT64 windows_time = 0;
	time(&unix_time);

	if (unix_time < 0)
		return 0;

	windows_time = (UINT64)unix_time;
	windows_time *= 10000000;
	windows_time += 621355968000000000ULL;
	return windows_time;
}

static char* winpr_read_unix_timezone_identifier_from_file(FILE* fp)
{
	const INT CHUNK_SIZE = 32;
	INT64 rc = 0;
	INT64 read = 0;
	INT64 length = CHUNK_SIZE;
	char* tzid = NULL;

	tzid = (char*)malloc((size_t)length);
	if (!tzid)
		return NULL;

	do
	{
		rc = fread(tzid + read, 1, length - read - 1, fp);
		if (rc > 0)
			read += rc;

		if (read < (length - 1))
			break;

		length += CHUNK_SIZE;
		char* tmp = (char*)realloc(tzid, length);
		if (!tmp)
		{
			free(tzid);
			return NULL;
		}

		tzid = tmp;
	} while (rc > 0);

	if (ferror(fp))
	{
		free(tzid);
		return NULL;
	}

	tzid[read] = '\0';
	if (read > 0)
	{
		if (tzid[read - 1] == '\n')
			tzid[read - 1] = '\0';
	}

	return tzid;
}

static char* winpr_get_timezone_from_link(const char* links[], size_t count)
{
	const char* _links[] = { "/etc/localtime", "/etc/TZ" };

	if (links == NULL)
	{
		links = _links;
		count = ARRAYSIZE(_links);
	}

	/*
	 * On linux distros such as Redhat or Archlinux, a symlink at /etc/localtime
	 * will point to /usr/share/zoneinfo/region/place where region/place could be
	 * America/Montreal for example.
	 * Some distributions do have to symlink at /etc/TZ.
	 */

	for (size_t x = 0; x < count; x++)
	{
		char* tzid = NULL;
		const char* link = links[x];
		char* buf = realpath(link, NULL);

		if (buf)
		{
			size_t sep = 0;
			size_t alloc = 0;
			size_t pos = 0;
			size_t len = pos = strlen(buf);

			/* find the position of the 2nd to last "/" */
			for (size_t i = 1; i <= len; i++)
			{
				const size_t curpos = len - i;
				const char cur = buf[curpos];

				if (cur == '/')
					sep++;
				if (sep >= 2)
				{
					alloc = i;
					pos = len - i + 1;
					break;
				}
			}

			if ((len == 0) || (sep != 2))
				goto end;

			tzid = (char*)calloc(alloc + 1, sizeof(char));

			if (!tzid)
				goto end;

			strncpy(tzid, &buf[pos], alloc);
			WLog_DBG(TAG, "tzid: %s", tzid);
			goto end;
		}

	end:
		free(buf);
		if (tzid)
			return tzid;
	}

	return NULL;
}

#if defined(ANDROID)
#include "../utils/android.h"

static char* winpr_get_android_timezone_identifier(void)
{
	char* tzid = NULL;
	JNIEnv* jniEnv;

	/* Preferred: Try to get identifier from java TimeZone class */
	if (jniVm && ((*jniVm)->GetEnv(jniVm, (void**)&jniEnv, JNI_VERSION_1_6) == JNI_OK))
	{
		const char* raw;
		jclass jObjClass;
		jobject jObj;
		jmethodID jDefaultTimezone;
		jmethodID jTimezoneIdentifier;
		jstring tzJId;
		jboolean attached = (*jniVm)->AttachCurrentThread(jniVm, &jniEnv, NULL);
		jObjClass = (*jniEnv)->FindClass(jniEnv, "java/util/TimeZone");

		if (!jObjClass)
			goto fail;

		jDefaultTimezone =
		    (*jniEnv)->GetStaticMethodID(jniEnv, jObjClass, "getDefault", "()Ljava/util/TimeZone;");

		if (!jDefaultTimezone)
			goto fail;

		jObj = (*jniEnv)->CallStaticObjectMethod(jniEnv, jObjClass, jDefaultTimezone);

		if (!jObj)
			goto fail;

		jTimezoneIdentifier =
		    (*jniEnv)->GetMethodID(jniEnv, jObjClass, "getID", "()Ljava/lang/String;");

		if (!jTimezoneIdentifier)
			goto fail;

		tzJId = (*jniEnv)->CallObjectMethod(jniEnv, jObj, jTimezoneIdentifier);

		if (!tzJId)
			goto fail;

		raw = (*jniEnv)->GetStringUTFChars(jniEnv, tzJId, 0);

		if (raw)
			tzid = _strdup(raw);

		(*jniEnv)->ReleaseStringUTFChars(jniEnv, tzJId, raw);
	fail:

		if (attached)
			(*jniVm)->DetachCurrentThread(jniVm);
	}

	/* Fall back to property, might not be available. */
	if (!tzid)
	{
		FILE* fp = popen("getprop persist.sys.timezone", "r");

		if (fp)
		{
			tzid = winpr_read_unix_timezone_identifier_from_file(fp);
			pclose(fp);
		}
	}

	return tzid;
}
#endif

static char* winpr_get_unix_timezone_identifier_from_file(void)
{
#if defined(ANDROID)
	return winpr_get_android_timezone_identifier();
#else
	FILE* fp = NULL;
	char* tzid = NULL;
#if !defined(WINPR_TIMEZONE_FILE)
#error \
    "Please define WINPR_TIMEZONE_FILE with the path to your timezone file (e.g. /etc/timezone or similar)"
#else
	fp = winpr_fopen(WINPR_TIMEZONE_FILE, "r");
#endif

	if (NULL == fp)
		return NULL;

	tzid = winpr_read_unix_timezone_identifier_from_file(fp);
	fclose(fp);
	if (tzid != NULL)
		WLog_DBG(TAG, "tzid: %s", tzid);
	return tzid;
#endif
}

#if defined(WITH_TIMEZONE_BUILTIN)
static const TIME_ZONE_RULE_ENTRY*
winpr_get_current_time_zone_rule(const TIME_ZONE_RULE_ENTRY* rules, UINT32 count)
{
	UINT64 windows_time = 0;
	windows_time = winpr_windows_gmtime();

	for (UINT32 i = 0; i < count; i++)
	{
		if ((rules[i].TicksStart >= windows_time) && (windows_time >= rules[i].TicksEnd))
		{
			/*WLog_ERR(TAG,  "Got rule %" PRIu32 " from table at %p with count %"PRIu32"", i,
			 * (void*) rules, count);*/
			return &rules[i];
		}
	}

	WLog_ERR(TAG, "Unable to get current timezone rule");
	return NULL;
}

static const TIME_ZONE_ENTRY* get_entry_for(const WCHAR* wzid)
{
	const TIME_ZONE_ENTRY* entry = NULL;
	if (!wzid)
		return NULL;
	char* uwzid = ConvertWCharToUtf8Alloc(wzid, NULL);
	if (!uwzid)
		goto fail;

	for (size_t i = 0; i < TimeZoneTableNrElements; i++)
	{
		const TIME_ZONE_ENTRY* tze = &TimeZoneTable[i];

		if (strcmp(tze->Id, uwzid) != 0)
			continue;
		entry = tze;
		break;
	}

fail:
	free(uwzid);
	return entry;
}
#endif

static TIME_ZONE_INFORMATION* winpr_unix_to_windows_time_zone(const char* tzid)
{
	TIME_ZONE_INFORMATION* entry = NULL;
	WCHAR* wzid = tz_map_iana_to_windows(tzid);
	if (wzid == NULL)
		goto end;

	entry = winpr_get_from_id(tzid, wzid);

end:
	if (!entry)
		WLog_ERR(TAG, "Unable to find a match for unix timezone: %s", tzid);
	else
	{
		char buffer[128] = { 0 };
		ConvertWCharToUtf8(wzid, buffer, ARRAYSIZE(buffer));
		WLog_INFO(TAG, "tzid: %s -> wzid: %s", tzid, buffer);
	}
	free(wzid);
	return entry;
}

#if defined(WITH_TIMEZONE_BUILTIN)
static TIME_ZONE_INFORMATION* winpr_get_from_id_builtin(const char* tzid, const WCHAR* wzid)
{
	BOOL success = FALSE;
	TIME_ZONE_INFORMATION* entry = NULL;
	const TIME_ZONE_ENTRY* tze = get_entry_for(wzid);
	if (!tze)
		goto fail;

	entry = calloc(1, sizeof(TIME_ZONE_INFORMATION));
	if (!entry)
		goto fail;

	entry->Bias = tze->Bias;

	if (ConvertUtf8ToWChar(tze->StandardName, entry->StandardName, ARRAYSIZE(entry->StandardName)) <
	    0)
		goto fail;

	if (ConvertUtf8ToWChar(tze->DaylightName, entry->DaylightName, ARRAYSIZE(entry->DaylightName)) <
	    0)
		goto fail;

	if ((tze->SupportsDST) && (tze->RuleTableCount > 0))
	{
		const TIME_ZONE_RULE_ENTRY* rule =
		    winpr_get_current_time_zone_rule(tze->RuleTable, tze->RuleTableCount);

		if (rule != NULL)
		{
			entry->DaylightBias = -rule->DaylightDelta;
			entry->StandardDate = rule->StandardDate;
			entry->DaylightDate = rule->DaylightDate;
		}
	}
	success = TRUE;

fail:
	if (!success)
	{
		free(entry);
		return NULL;
	}
	return entry;
}
#endif

static char* winpr_time_zone_from_env(void)
{
	LPCSTR tz = "TZ";
	char* tzid = NULL;

	DWORD nSize = GetEnvironmentVariableA(tz, NULL, 0);
	if (nSize > 0)
	{
		tzid = (char*)calloc(nSize, sizeof(char));
		if (!tzid)
			goto fail;
		if (!GetEnvironmentVariableA(tz, tzid, nSize))
			goto fail;
		else if (tzid[0] == ':')
		{
			/* Remove leading colon, see tzset(3) */
			memmove(tzid, tzid + 1, nSize - sizeof(char));
		}
	}

	return tzid;

fail:
	free(tzid);
	return NULL;
}

static char* winpr_translate_time_zone(const char* tzid)
{
	const char* zipath = "/usr/share/zoneinfo/";
	char* buf = NULL;
	const char* links[] = { buf };

	if (!tzid)
		return NULL;

	if (tzid[0] == '/')
	{
		/* Full path given in TZ */
		links[0] = tzid;
	}
	else
	{
		size_t bsize = 0;
		winpr_asprintf(&buf, &bsize, "%s%s", zipath, tzid);
		links[0] = buf;
	}

	char* ntzid = winpr_get_timezone_from_link(links, 1);
	free(buf);
	return ntzid;
}

static char* winpr_guess_time_zone(void)
{
	char* tzid = winpr_time_zone_from_env();
	if (tzid)
		goto end;
	tzid = winpr_get_unix_timezone_identifier_from_file();
	if (tzid)
		goto end;
	tzid = winpr_get_timezone_from_link(NULL, 0);
	if (tzid)
		goto end;

end:
{
	char* ntzid = winpr_translate_time_zone(tzid);
	if (ntzid)
	{
		free(tzid);
		return ntzid;
	}
	return tzid;
}
}

static TIME_ZONE_INFORMATION* winpr_detect_windows_time_zone(const char* tzstr)
{
	char* tzid = NULL;

	if (tzstr)
		tzid = _strdup(tzstr);
	else
		tzid = winpr_guess_time_zone();
	if (!tzid)
		return NULL;

	TIME_ZONE_INFORMATION* ctimezone = winpr_unix_to_windows_time_zone(tzid);
	free(tzid);
	return ctimezone;
}

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	time_t t = 0;
	struct tm tres = { 0 };
	TIME_ZONE_INFORMATION* dtz = NULL;
	const TIME_ZONE_INFORMATION empty = { 0 };
	LPTIME_ZONE_INFORMATION tz = lpTimeZoneInformation;

	WINPR_ASSERT(tz);

	*tz = empty;

	t = time(NULL);
	struct tm* local_time = localtime_r(&t, &tres);
	if (!local_time)
		goto out_error;

#ifdef WINPR_HAVE_TM_GMTOFF
	{
		long bias = -(local_time->tm_gmtoff / 60L);

		if (bias > INT32_MAX)
			bias = INT32_MAX;

		tz->Bias = (LONG)bias;
	}
#endif
	dtz = winpr_detect_windows_time_zone(NULL);

	if (dtz != NULL)
	{
		WLog_DBG(TAG, "tz: Bias=%" PRId32 " sn='%s' dln='%s'", dtz->Bias, dtz->StandardName,
		         dtz->DaylightName);

		*tz = *dtz;

		free(dtz);
		/* 1 ... TIME_ZONE_ID_STANDARD
		 * 2 ... TIME_ZONE_ID_DAYLIGHT */
		return local_time->tm_isdst ? 2 : 1;
	}

	/* could not detect timezone, use computed bias from tm_gmtoff */
	WLog_DBG(TAG, "tz not found, using computed bias %" PRId32 ".", tz->Bias);
out_error:
	free(dtz);
	if (ConvertUtf8ToWChar("Client Local Time", tz->StandardName, ARRAYSIZE(tz->StandardName)) <= 0)
		WLog_WARN(TAG, "Failed to set default timezone name");
	memcpy(tz->DaylightName, tz->StandardName, sizeof(tz->DaylightName));
	return 0; /* TIME_ZONE_ID_UNKNOWN */
}

BOOL SetTimeZoneInformation(const TIME_ZONE_INFORMATION* lpTimeZoneInformation)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	return FALSE;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* lpSystemTime, LPFILETIME lpFileTime)
{
	WINPR_UNUSED(lpSystemTime);
	WINPR_UNUSED(lpFileTime);
	return FALSE;
}

BOOL FileTimeToSystemTime(const FILETIME* lpFileTime, LPSYSTEMTIME lpSystemTime)
{
	WINPR_UNUSED(lpFileTime);
	WINPR_UNUSED(lpSystemTime);
	return FALSE;
}

BOOL SystemTimeToTzSpecificLocalTime(LPTIME_ZONE_INFORMATION lpTimeZone,
                                     LPSYSTEMTIME lpUniversalTime, LPSYSTEMTIME lpLocalTime)
{
	WINPR_UNUSED(lpTimeZone);
	WINPR_UNUSED(lpUniversalTime);
	WINPR_UNUSED(lpLocalTime);
	return FALSE;
}

BOOL TzSpecificLocalTimeToSystemTime(LPTIME_ZONE_INFORMATION lpTimeZoneInformation,
                                     LPSYSTEMTIME lpLocalTime, LPSYSTEMTIME lpUniversalTime)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(lpLocalTime);
	WINPR_UNUSED(lpUniversalTime);
	return FALSE;
}

#endif

/*
 * GetDynamicTimeZoneInformation is provided by the SDK if _WIN32_WINNT >= 0x0600 in SDKs above 7.1A
 * and incorrectly if _WIN32_WINNT >= 0x0501 in older SDKs
 */
#if !defined(_WIN32) ||                                                  \
    (defined(_WIN32) && (defined(NTDDI_WIN8) && _WIN32_WINNT < 0x0600 || \
                         !defined(NTDDI_WIN8) && _WIN32_WINNT < 0x0501)) /* Windows Vista */

DWORD GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation)
{
	WINPR_UNUSED(pTimeZoneInformation);
	return 0;
}

BOOL SetDynamicTimeZoneInformation(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	return FALSE;
}

BOOL GetTimeZoneInformationForYear(USHORT wYear, PDYNAMIC_TIME_ZONE_INFORMATION pdtzi,
                                   LPTIME_ZONE_INFORMATION ptzi)
{
	WINPR_UNUSED(wYear);
	WINPR_UNUSED(pdtzi);
	WINPR_UNUSED(ptzi);
	return FALSE;
}

#endif

#if !defined(_WIN32) || (defined(_WIN32) && (_WIN32_WINNT < 0x0601)) /* Windows 7 */

BOOL SystemTimeToTzSpecificLocalTimeEx(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation,
                                       const SYSTEMTIME* lpUniversalTime, LPSYSTEMTIME lpLocalTime)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(lpUniversalTime);
	WINPR_UNUSED(lpLocalTime);
	return FALSE;
}

BOOL TzSpecificLocalTimeToSystemTimeEx(const DYNAMIC_TIME_ZONE_INFORMATION* lpTimeZoneInformation,
                                       const SYSTEMTIME* lpLocalTime, LPSYSTEMTIME lpUniversalTime)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(lpLocalTime);
	WINPR_UNUSED(lpUniversalTime);
	return FALSE;
}

#endif

#if !defined(_WIN32) || (defined(_WIN32) && (_WIN32_WINNT < 0x0602)) /* Windows 8 */

DWORD EnumDynamicTimeZoneInformation(const DWORD dwIndex,
                                     PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	WINPR_UNUSED(dwIndex);
	WINPR_UNUSED(lpTimeZoneInformation);
	return 0;
}

DWORD GetDynamicTimeZoneInformationEffectiveYears(
    const PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation, LPDWORD FirstYear, LPDWORD LastYear)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(FirstYear);
	WINPR_UNUSED(LastYear);
	return 0;
}

#endif
