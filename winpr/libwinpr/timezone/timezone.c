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

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#include "TimeZoneNameMap.h"

#ifndef _WIN32

#include <time.h>
#include <unistd.h>

#endif

#if !defined(_WIN32)
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

static SYSTEMTIME tm2systemtime(const struct tm* t)
{
	SYSTEMTIME st = { 0 };

	if (t)
	{
		st.wYear = (WORD)1900 + t->tm_year;
		st.wMonth = (WORD)t->tm_mon;
		st.wDay = (WORD)t->tm_mday;
		st.wDayOfWeek = (WORD)t->tm_wday;
		st.wHour = (WORD)t->tm_hour;
		st.wMinute = (WORD)t->tm_min;
		st.wSecond = (WORD)t->tm_sec;
		st.wMilliseconds = 0;
	}
	return st;
}

static LONG get_gmtoff_min(struct tm* t)
{
	WINPR_ASSERT(t);
	return -t->tm_gmtoff / 60l;
}

static struct tm next_day(const struct tm* start)
{
	struct tm cur = *start;
	cur.tm_hour = 0;
	cur.tm_min = 0;
	cur.tm_sec = 0;
	cur.tm_isdst = -1;
	cur.tm_mday++;
	const time_t t = mktime(&cur);
	localtime_r(&t, &cur);
	return cur;
}

static struct tm adjust_time(const struct tm* start, int hour, int minute)
{
	struct tm cur = *start;
	cur.tm_hour = hour;
	cur.tm_min = minute;
	cur.tm_sec = 0;
	cur.tm_isdst = -1;
	const time_t t = mktime(&cur);
	localtime_r(&t, &cur);
	return cur;
}

static SYSTEMTIME get_transition_time(const struct tm* start, BOOL toDst)
{
	BOOL toggled = FALSE;
	struct tm first = adjust_time(start, 0, 0);
	for (int hour = 0; hour < 24; hour++)
	{
		for (int minute = 0; minute < 60; minute++)
		{
			struct tm cur = adjust_time(start, hour, minute);
			if (cur.tm_isdst != first.tm_isdst)
				toggled = TRUE;

			if (toggled)
			{
				if (toDst && (cur.tm_isdst > 0))
					return tm2systemtime(&cur);
				else if (!toDst && (cur.tm_isdst == 0))
					return tm2systemtime(&cur);
			}
		}
	}
	return tm2systemtime(start);
}

static BOOL get_transition_date(const struct tm* start, BOOL toDst, SYSTEMTIME* pdate)
{
	WINPR_ASSERT(start);
	WINPR_ASSERT(pdate);

	*pdate = tm2systemtime(NULL);

	if (start->tm_isdst < 0)
		return FALSE;

	BOOL val = start->tm_isdst > 0; // the year starts with DST or not
	BOOL toggled = FALSE;
	struct tm cur = *start;
	for (int day = 1; day <= 365; day++)
	{
		cur = next_day(&cur);
		const BOOL curDst = (cur.tm_isdst > 0);
		if ((val != curDst) && !toggled)
			toggled = TRUE;

		if (toggled)
		{
			if (toDst && curDst)
			{
				*pdate = get_transition_time(&cur, toDst);
				return TRUE;
			}
			if (!toDst && !curDst)
			{
				*pdate = get_transition_time(&cur, toDst);
				return TRUE;
			}
		}
	}
	return FALSE;
}

static LONG get_bias(const struct tm* start, BOOL dstBias)
{
	if ((start->tm_isdst > 0) && dstBias)
		return get_gmtoff_min(start);
	if ((start->tm_isdst == 0) && !dstBias)
		return get_gmtoff_min(start);
	if (start->tm_isdst < 0)
		return get_gmtoff_min(start);

	struct tm cur = *start;
	for (int day = 1; day <= 365; day++)
	{
		cur = next_day(&cur);
		if ((cur.tm_isdst > 0) && dstBias)
			return get_gmtoff_min(&cur);
		else if ((cur.tm_isdst == 0) && !dstBias)
			return get_gmtoff_min(&cur);
	}
	return 0;
}

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	const char* defaultName = "Client Local Time";
	DWORD res = TIME_ZONE_ID_UNKNOWN;
	const TIME_ZONE_INFORMATION empty = { 0 };
	LPTIME_ZONE_INFORMATION tz = lpTimeZoneInformation;

	WINPR_ASSERT(tz);

	*tz = empty;
	ConvertUtf8ToWChar(defaultName, tz->StandardName, ARRAYSIZE(tz->StandardName));

	char* tzid = winpr_guess_time_zone();

	const time_t t = time(NULL);
	struct tm tres = { 0 };
	struct tm* local_time = localtime_r(&t, &tres);
	if (!local_time)
		goto out_error;

	tz->Bias = get_bias(local_time, FALSE);
	if (local_time->tm_isdst >= 0)
	{
		/* DST bias is the difference between standard time and DST in minutes */
		const LONG d = get_bias(local_time, TRUE);
		tz->DaylightBias = -1 * labs(tz->Bias - d);
		get_transition_date(local_time, FALSE, &tz->StandardDate);
		get_transition_date(local_time, TRUE, &tz->DaylightDate);
	}

	ConvertUtf8ToWChar(local_time->tm_zone, tz->StandardName, ARRAYSIZE(tz->StandardName));

	const char* winId = TimeZoneIanaToWindows(tzid, TIME_ZONE_NAME_ID);
	if (winId)
	{
		const char* winStd = TimeZoneIanaToWindows(tzid, TIME_ZONE_NAME_STANDARD);
		const char* winDst = TimeZoneIanaToWindows(tzid, TIME_ZONE_NAME_DAYLIGHT);

		ConvertUtf8ToWChar(winStd, tz->StandardName, ARRAYSIZE(tz->StandardName));
		ConvertUtf8ToWChar(winDst, tz->DaylightName, ARRAYSIZE(tz->DaylightName));

		res = (local_time->tm_isdst) ? TIME_ZONE_ID_DAYLIGHT : TIME_ZONE_ID_STANDARD;
	}

out_error:
	free(tzid);
	switch (res)
	{
		case TIME_ZONE_ID_DAYLIGHT:
		case TIME_ZONE_ID_STANDARD:
			WLog_DBG(TAG, "tz: Bias=%" PRId32 " sn='%s' dln='%s'", tz->Bias, tz->StandardName,
			         tz->DaylightName);
			break;
		default:
			WLog_DBG(TAG, "tz not found, using computed bias %" PRId32 ".", tz->Bias);
			break;
	}

	return res;
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
	TIME_ZONE_INFORMATION tz = { 0 };
	const DWORD rc = GetTimeZoneInformation(&tz);

	WINPR_ASSERT(pTimeZoneInformation);
	pTimeZoneInformation->Bias = tz.Bias;
	memcpy(pTimeZoneInformation->StandardName, tz.StandardName,
	       MIN(sizeof(tz.StandardName), sizeof(pTimeZoneInformation->StandardName)));
	pTimeZoneInformation->StandardDate = tz.StandardDate;
	pTimeZoneInformation->StandardBias = tz.StandardBias;

	memcpy(pTimeZoneInformation->DaylightName, tz.DaylightName,
	       MIN(sizeof(tz.DaylightName), sizeof(pTimeZoneInformation->DaylightName)));
	pTimeZoneInformation->DaylightDate = tz.DaylightDate;
	pTimeZoneInformation->DaylightBias = tz.DaylightBias;

	memcpy(pTimeZoneInformation->TimeZoneKeyName, tz.StandardName,
	       MIN(sizeof(tz.StandardName), sizeof(pTimeZoneInformation->TimeZoneKeyName)));

	/* https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-dynamic_time_zone_information
	 */
	pTimeZoneInformation->DynamicDaylightTimeDisabled = FALSE;
	return rc;
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
