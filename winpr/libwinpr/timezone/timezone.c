/**
 * WinPR: Windows Portable Runtime
 * Time Zone
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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
#include "TimeZoneIanaAbbrevMap.h"

#ifndef _WIN32

#include <time.h>
#include <unistd.h>

#endif

#if !defined(_WIN32)
static char* winpr_read_unix_timezone_identifier_from_file(FILE* fp)
{
	const INT CHUNK_SIZE = 32;
	size_t rc = 0;
	size_t read = 0;
	size_t length = CHUNK_SIZE;

	char* tzid = malloc(length);
	if (!tzid)
		return NULL;

	do
	{
		rc = fread(tzid + read, 1, length - read - 1UL, fp);
		if (rc > 0)
			read += rc;

		if (read < (length - 1UL))
			break;

		if (read > length - 1UL)
		{
			free(tzid);
			return NULL;
		}

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
	(void)fclose(fp);
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
		st.wYear = (WORD)(1900 + t->tm_year);
		st.wMonth = (WORD)t->tm_mon + 1;
		st.wDay = (WORD)t->tm_mday;
		st.wDayOfWeek = (WORD)t->tm_wday;
		st.wHour = (WORD)t->tm_hour;
		st.wMinute = (WORD)t->tm_min;
		st.wSecond = (WORD)t->tm_sec;
		st.wMilliseconds = 0;
	}
	return st;
}

static struct tm systemtime2tm(const SYSTEMTIME* st)
{
	struct tm t = { 0 };
	if (st)
	{
		if (st->wYear >= 1900)
			t.tm_year = st->wYear - 1900;
		if (st->wMonth > 0)
			t.tm_mon = st->wMonth - 1;
		t.tm_mday = st->wDay;
		t.tm_wday = st->wDayOfWeek;
		t.tm_hour = st->wHour;
		t.tm_min = st->wMinute;
		t.tm_sec = st->wSecond;
	}
	return t;
}

static LONG get_gmtoff_min(const struct tm* t)
{
	WINPR_ASSERT(t);
	return -(LONG)(t->tm_gmtoff / 60l);
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
	(void)localtime_r(&t, &cur);
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
	(void)localtime_r(&t, &cur);
	return cur;
}

/* [MS-RDPBCGR] 2.2.1.11.1.1.1.1.1  System Time (TS_SYSTEMTIME) */
static WORD get_transition_weekday_occurrence(const SYSTEMTIME* st)
{
	WORD count = 0;
	struct tm start = systemtime2tm(st);

	WORD last = 0;
	struct tm next = start;
	next.tm_mday = 1;
	next.tm_isdst = -1;
	do
	{

		const time_t t = mktime(&next);
		next.tm_mday++;

		struct tm cur = { 0 };
		(void)localtime_r(&t, &cur);

		if (cur.tm_mon + 1 != st->wMonth)
			break;

		if (cur.tm_wday == st->wDayOfWeek)
		{
			if (cur.tm_mday <= st->wDay)
				count++;
			last++;
		}

	} while (TRUE);

	if (count == last)
		count = 5;

	return count;
}

static SYSTEMTIME tm2transitiontime(const struct tm* cur)
{
	SYSTEMTIME t = tm2systemtime(cur);
	if (cur)
	{
		t.wDay = get_transition_weekday_occurrence(&t);
		t.wYear = 0;
	}

	return t;
}

static SYSTEMTIME get_transition_time(const struct tm* start, BOOL toDst)
{
	BOOL toggled = FALSE;
	struct tm first = adjust_time(start, 0, 0);
	for (int hour = 0; hour < 24; hour++)
	{
		struct tm cur = adjust_time(start, hour, 0);
		if (cur.tm_isdst != first.tm_isdst)
			toggled = TRUE;

		if (toggled)
		{
			if (toDst && (cur.tm_isdst > 0))
				return tm2transitiontime(&cur);
			else if (!toDst && (cur.tm_isdst == 0))
				return tm2transitiontime(&cur);
		}
	}
	return tm2transitiontime(start);
}

static BOOL get_transition_date(const struct tm* start, BOOL toDst, SYSTEMTIME* pdate)
{
	WINPR_ASSERT(start);
	WINPR_ASSERT(pdate);

	*pdate = tm2transitiontime(NULL);

	if (start->tm_isdst < 0)
		return FALSE;

	BOOL val = start->tm_isdst > 0; // the year starts with DST or not
	BOOL toggled = FALSE;
	struct tm cur = *start;
	struct tm last = cur;
	for (int day = 1; day <= 365; day++)
	{
		last = cur;
		cur = next_day(&cur);
		const BOOL curDst = (cur.tm_isdst > 0);
		if ((val != curDst) && !toggled)
			toggled = TRUE;

		if (toggled)
		{
			if (toDst && curDst)
			{
				*pdate = get_transition_time(&last, toDst);
				return TRUE;
			}
			if (!toDst && !curDst)
			{
				*pdate = get_transition_time(&last, toDst);
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

static BOOL map_iana_id(const char* iana, LPDYNAMIC_TIME_ZONE_INFORMATION tz)
{
	const char* winId = TimeZoneIanaToWindows(iana, TIME_ZONE_NAME_ID);
	const char* winStd = TimeZoneIanaToWindows(iana, TIME_ZONE_NAME_STANDARD);
	const char* winDst = TimeZoneIanaToWindows(iana, TIME_ZONE_NAME_DAYLIGHT);

	if (winStd)
		(void)ConvertUtf8ToWChar(winStd, tz->StandardName, ARRAYSIZE(tz->StandardName));
	if (winDst)
		(void)ConvertUtf8ToWChar(winDst, tz->DaylightName, ARRAYSIZE(tz->DaylightName));
	if (winId)
		(void)ConvertUtf8ToWChar(winId, tz->TimeZoneKeyName, ARRAYSIZE(tz->TimeZoneKeyName));

	return winId != NULL;
}

static const char* weekday2str(WORD wDayOfWeek)
{
	switch (wDayOfWeek)
	{
		case 0:
			return "SUNDAY";
		case 1:
			return "MONDAY";
		case 2:
			return "TUESDAY";
		case 3:
			return "WEDNESDAY";
		case 4:
			return "THURSDAY";
		case 5:
			return "FRIDAY";
		case 6:
			return "SATURDAY";
		default:
			return "DAY-OF-MAGIC";
	}
}

static char* systemtime2str(const SYSTEMTIME* t, char* buffer, size_t len)
{
	const SYSTEMTIME empty = { 0 };

	if (memcmp(t, &empty, sizeof(SYSTEMTIME)) == 0)
		(void)_snprintf(buffer, len, "{ not set }");
	else
	{
		(void)_snprintf(buffer, len,
		                "{ %" PRIu16 "-%" PRIu16 "-%" PRIu16 " [%s] %" PRIu16 ":%" PRIu16
		                ":%" PRIu16 ".%" PRIu16 "}",
		                t->wYear, t->wMonth, t->wDay, weekday2str(t->wDayOfWeek), t->wHour,
		                t->wMinute, t->wSecond, t->wMilliseconds);
	}
	return buffer;
}

static void log_print(wLog* log, DWORD level, const char* file, const char* fkt, size_t line, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	va_list ap = { 0 };
	va_start(ap, line);
	WLog_PrintMessageVA(log, WLOG_MESSAGE_TEXT, level, line, file, fkt, ap);
	va_end(ap);
}

#define log_timezone(tzif, result) log_timezone_((tzif), (result), __FILE__, __func__, __LINE__)
static void log_timezone_(const DYNAMIC_TIME_ZONE_INFORMATION* tzif, DWORD result, const char* file,
                          const char* fkt, size_t line)
{
	WINPR_ASSERT(tzif);

	char buffer[130] = { 0 };
	DWORD level = WLOG_TRACE;
	wLog* log = WLog_Get(TAG);
	log_print(log, level, file, fkt, line, "DYNAMIC_TIME_ZONE_INFORMATION {");

	log_print(log, level, file, fkt, line, "  Bias=%" PRIu32, tzif->Bias);
	(void)ConvertWCharNToUtf8(tzif->StandardName, ARRAYSIZE(tzif->StandardName), buffer,
	                          ARRAYSIZE(buffer));
	log_print(log, level, file, fkt, line, "  StandardName=%s", buffer);
	log_print(log, level, file, fkt, line, "  StandardDate=%s",
	          systemtime2str(&tzif->StandardDate, buffer, sizeof(buffer)));
	log_print(log, level, file, fkt, line, "  StandardBias=%" PRIu32, tzif->StandardBias);

	(void)ConvertWCharNToUtf8(tzif->DaylightName, ARRAYSIZE(tzif->DaylightName), buffer,
	                          ARRAYSIZE(buffer));
	log_print(log, level, file, fkt, line, "  DaylightName=%s", buffer);
	log_print(log, level, file, fkt, line, "  DaylightDate=%s",
	          systemtime2str(&tzif->DaylightDate, buffer, sizeof(buffer)));
	log_print(log, level, file, fkt, line, "  DaylightBias=%" PRIu32, tzif->DaylightBias);
	(void)ConvertWCharNToUtf8(tzif->TimeZoneKeyName, ARRAYSIZE(tzif->TimeZoneKeyName), buffer,
	                          ARRAYSIZE(buffer));
	log_print(log, level, file, fkt, line, "  TimeZoneKeyName=%s", buffer);
	log_print(log, level, file, fkt, line, "  DynamicDaylightTimeDisabled=DST-%s",
	          tzif->DynamicDaylightTimeDisabled ? "disabled" : "enabled");
	switch (result)
	{
		case TIME_ZONE_ID_DAYLIGHT:
			log_print(log, level, file, fkt, line, "  DaylightDate in use");
			break;
		case TIME_ZONE_ID_STANDARD:
			log_print(log, level, file, fkt, line, "  StandardDate in use");
			break;
		default:
			log_print(log, level, file, fkt, line, "  UnknownDate in use");
			break;
	}
	log_print(log, level, file, fkt, line, "}");
}

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	DYNAMIC_TIME_ZONE_INFORMATION dyn = { 0 };
	DWORD rc = GetDynamicTimeZoneInformation(&dyn);
	lpTimeZoneInformation->Bias = dyn.Bias;
	lpTimeZoneInformation->DaylightBias = dyn.DaylightBias;
	lpTimeZoneInformation->DaylightDate = dyn.DaylightDate;
	lpTimeZoneInformation->StandardBias = dyn.StandardBias;
	lpTimeZoneInformation->StandardDate = dyn.StandardDate;
	memcpy(lpTimeZoneInformation->StandardName, dyn.StandardName,
	       sizeof(lpTimeZoneInformation->StandardName));
	memcpy(lpTimeZoneInformation->DaylightName, dyn.DaylightName,
	       sizeof(lpTimeZoneInformation->DaylightName));
	return rc;
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

typedef enum
{
	HAVE_TRANSITION_DATES = 0,
	HAVE_NO_STANDARD_TRANSITION_DATE = 1,
	HAVE_NO_DAYLIGHT_TRANSITION_DATE = 2
} dyn_transition_result;

static int dynamic_time_zone_from_localtime(const struct tm* local_time,
                                            PDYNAMIC_TIME_ZONE_INFORMATION tz)
{
	WINPR_ASSERT(local_time);
	WINPR_ASSERT(tz);
	int rc = HAVE_TRANSITION_DATES;

	tz->Bias = get_bias(local_time, FALSE);
	if (local_time->tm_isdst >= 0)
	{
		/* DST bias is the difference between standard time and DST in minutes */
		const LONG d = get_bias(local_time, TRUE);
		tz->DaylightBias = -1 * (LONG)labs(tz->Bias - d);
		if (!get_transition_date(local_time, FALSE, &tz->StandardDate))
			rc |= HAVE_NO_STANDARD_TRANSITION_DATE;
		if (!get_transition_date(local_time, TRUE, &tz->DaylightDate))
			rc |= HAVE_NO_DAYLIGHT_TRANSITION_DATE;
	}
	return rc;
}

DWORD GetDynamicTimeZoneInformation(PDYNAMIC_TIME_ZONE_INFORMATION tz)
{
	BOOL doesNotHaveStandardDate = FALSE;
	BOOL doesNotHaveDaylightDate = FALSE;
	const char** list = NULL;
	char* tzid = NULL;
	const char* defaultName = "Client Local Time";
	DWORD res = TIME_ZONE_ID_UNKNOWN;
	const DYNAMIC_TIME_ZONE_INFORMATION empty = { 0 };

	WINPR_ASSERT(tz);

	*tz = empty;
	(void)ConvertUtf8ToWChar(defaultName, tz->StandardName, ARRAYSIZE(tz->StandardName));

	const time_t t = time(NULL);
	struct tm tres = { 0 };
	struct tm* local_time = localtime_r(&t, &tres);
	if (!local_time)
		goto out_error;

	tz->Bias = get_bias(local_time, FALSE);
	if (local_time->tm_isdst >= 0)
	{
		const int rc = dynamic_time_zone_from_localtime(local_time, tz);
		if (rc & HAVE_NO_STANDARD_TRANSITION_DATE)
			doesNotHaveStandardDate = TRUE;
		if (rc & HAVE_NO_DAYLIGHT_TRANSITION_DATE)
			doesNotHaveDaylightDate = TRUE;
	}

	tzid = winpr_guess_time_zone();
	if (!map_iana_id(tzid, tz))
	{
		const size_t len = TimeZoneIanaAbbrevGet(local_time->tm_zone, NULL, 0);
		list = (const char**)calloc(len, sizeof(const char*));
		if (!list)
			goto out_error;
		const size_t size = TimeZoneIanaAbbrevGet(local_time->tm_zone, list, len);
		for (size_t x = 0; x < size; x++)
		{
			const char* id = list[x];
			if (map_iana_id(id, tz))
			{
				res = (local_time->tm_isdst) ? TIME_ZONE_ID_DAYLIGHT : TIME_ZONE_ID_STANDARD;
				break;
			}
		}
	}
	else
		res = (local_time->tm_isdst) ? TIME_ZONE_ID_DAYLIGHT : TIME_ZONE_ID_STANDARD;

	if (doesNotHaveDaylightDate)
		tz->DaylightBias = 0;

	if (doesNotHaveStandardDate)
		tz->StandardBias = 0;

out_error:
	free(tzid);
	free((void*)list);

	log_timezone(tz, res);
	return res;
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

#if !defined(_WIN32)

DWORD EnumDynamicTimeZoneInformation(const DWORD dwIndex,
                                     PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	if (!lpTimeZoneInformation)
		return ERROR_INVALID_PARAMETER;
	const DYNAMIC_TIME_ZONE_INFORMATION empty = { 0 };
	*lpTimeZoneInformation = empty;

	const TimeZoneNameMapEntry* entry = TimeZoneGetAt(dwIndex);
	if (!entry)
		return ERROR_NO_MORE_ITEMS;

	if (entry->DaylightName)
		(void)ConvertUtf8ToWChar(entry->DaylightName, lpTimeZoneInformation->DaylightName,
		                         ARRAYSIZE(lpTimeZoneInformation->DaylightName));
	if (entry->StandardName)
		(void)ConvertUtf8ToWChar(entry->StandardName, lpTimeZoneInformation->StandardName,
		                         ARRAYSIZE(lpTimeZoneInformation->StandardName));
	if (entry->Id)
		(void)ConvertUtf8ToWChar(entry->Id, lpTimeZoneInformation->TimeZoneKeyName,
		                         ARRAYSIZE(lpTimeZoneInformation->TimeZoneKeyName));

	const time_t t = time(NULL);
	struct tm tres = { 0 };

	const char* tz = getenv("TZ");
	char* tzcopy = NULL;
	if (tz)
	{
		size_t tzianalen = 0;
		winpr_asprintf(&tzcopy, &tzianalen, "TZ=%s", tz);
	}

	char* tziana = NULL;
	{
		size_t tzianalen = 0;
		winpr_asprintf(&tziana, &tzianalen, "TZ=%s", entry->Iana);
	}
	if (tziana)
		putenv(tziana);

	tzset();
	struct tm* local_time = localtime_r(&t, &tres);
	free(tziana);
	if (tzcopy)
		putenv(tzcopy);
	else
		unsetenv("TZ");
	free(tzcopy);

	if (local_time)
		dynamic_time_zone_from_localtime(local_time, lpTimeZoneInformation);

	return ERROR_SUCCESS;
}

// NOLINTBEGIN(readability-non-const-parameter)
DWORD GetDynamicTimeZoneInformationEffectiveYears(
    const PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation, LPDWORD FirstYear, LPDWORD LastYear)
// NOLINTEND(readability-non-const-parameter)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(FirstYear);
	WINPR_UNUSED(LastYear);
	return ERROR_FILE_NOT_FOUND;
}

#elif _WIN32_WINNT < 0x0602 /* Windows 8 */
DWORD EnumDynamicTimeZoneInformation(const DWORD dwIndex,
                                     PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	WINPR_UNUSED(dwIndex);
	WINPR_UNUSED(lpTimeZoneInformation);
	return ERROR_NO_MORE_ITEMS;
}

DWORD GetDynamicTimeZoneInformationEffectiveYears(
    const PDYNAMIC_TIME_ZONE_INFORMATION lpTimeZoneInformation, LPDWORD FirstYear, LPDWORD LastYear)
{
	WINPR_UNUSED(lpTimeZoneInformation);
	WINPR_UNUSED(FirstYear);
	WINPR_UNUSED(LastYear);
	return ERROR_FILE_NOT_FOUND;
}
#endif
