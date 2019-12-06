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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/wtypes.h>
#include <winpr/timezone.h>
#include <winpr/crt.h>
#include "../log.h"

#define TAG WINPR_TAG("timezone")

#ifndef _WIN32

#include <time.h>
#include <unistd.h>

#include "TimeZones.h"
#include "WindowsZones.h"

static UINT64 winpr_windows_gmtime(void)
{
	time_t unix_time;
	UINT64 windows_time;
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
	INT64 length;
	char* tzid = NULL;

	if (_fseeki64(fp, 0, SEEK_END) != 0)
		return NULL;

	length = _ftelli64(fp);

	if (_fseeki64(fp, 0, SEEK_SET) != 0)
		return NULL;

	if (length < 2)
		return NULL;

	tzid = (char*)malloc((size_t)length + 1);

	if (!tzid)
		return NULL;

	if (fread(tzid, (size_t)length, 1, fp) != 1)
	{
		free(tzid);
		return NULL;
	}

	tzid[length] = '\0';

	if (tzid[length - 1] == '\n')
		tzid[length - 1] = '\0';

	return tzid;
}

static char* winpr_get_timezone_from_link(void)
{
	const char* links[] = { "/etc/localtime", "/etc/TZ" };
	size_t x;
	ssize_t len;
	char buf[1024];
	char* tzid = NULL;

	/*
	 * On linux distros such as Redhat or Archlinux, a symlink at /etc/localtime
	 * will point to /usr/share/zoneinfo/region/place where region/place could be
	 * America/Montreal for example.
	 * Some distributions do have to symlink at /etc/TZ.
	 */

	for (x = 0; x < sizeof(links) / sizeof(links[0]); x++)
	{
		const char* link = links[x];

		if ((len = readlink(link, buf, sizeof(buf) - 1)) != -1)
		{
			int num = 0;
			size_t alloc;
			SSIZE_T pos = len;
			buf[len] = '\0';

			/* find the position of the 2nd to last "/" */

			while (num < 2)
			{
				if (pos == 0)
					break;

				pos -= 1;

				if (buf[pos] == '/')
					num++;
			}

			if ((len < 0) || (pos < 0) || (pos > len))
				return NULL;

			alloc = (size_t)len - (size_t)pos;
			tzid = (char*)malloc(alloc + 1);

			if (!tzid)
				return NULL;

			strncpy(tzid, buf + pos + 1, alloc);
			return tzid;
		}
	}

	return NULL;
}

#if defined(ANDROID)
#include <jni.h>
static JavaVM* jniVm = NULL;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	jniVm = vm;
	return JNI_VERSION_1_6;
}

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
	FILE* fp;
	char* tzid = NULL;
#if defined(__FreeBSD__) || defined(__OpenBSD__)
	fp = fopen("/var/db/zoneinfo", "r");
#else
	fp = fopen("/etc/timezone", "r");
#endif

	if (NULL == fp)
		return NULL;

	tzid = winpr_read_unix_timezone_identifier_from_file(fp);
	fclose(fp);
	return tzid;
#endif
}

static BOOL winpr_match_unix_timezone_identifier_with_list(const char* tzid, const char* list)
{
	char* p;
	char* list_copy;
	list_copy = _strdup(list);

	if (!list_copy)
		return FALSE;

	p = strtok(list_copy, " ");

	while (p != NULL)
	{
		if (strcmp(p, tzid) == 0)
		{
			free(list_copy);
			return TRUE;
		}

		p = strtok(NULL, " ");
	}

	free(list_copy);
	return FALSE;
}

static TIME_ZONE_ENTRY* winpr_detect_windows_time_zone(void)
{
	size_t i, j;
	char* tzid;

	tzid = winpr_get_unix_timezone_identifier_from_file();

	if (tzid == NULL)
		tzid = winpr_get_timezone_from_link();

	if (tzid == NULL)
		return NULL;

	for (i = 0; i < TimeZoneTableNrElements; i++)
	{
		const TIME_ZONE_ENTRY* tze = &TimeZoneTable[i];

		for (j = 0; j < WindowsTimeZoneIdTableNrElements; j++)
		{
			const WINDOWS_TZID_ENTRY* wzid = &WindowsTimeZoneIdTable[j];

			if (strcmp(tze->Id, wzid->windows) != 0)
				continue;

			if (winpr_match_unix_timezone_identifier_with_list(tzid, wzid->tzid))
			{
				TIME_ZONE_ENTRY* timezone = (TIME_ZONE_ENTRY*)malloc(sizeof(TIME_ZONE_ENTRY));
				free(tzid);

				if (!timezone)
					return NULL;

				*timezone = TimeZoneTable[i];
				return timezone;
			}
		}
	}

	WLog_ERR(TAG, "Unable to find a match for unix timezone: %s", tzid);
	free(tzid);
	return NULL;
}

static const TIME_ZONE_RULE_ENTRY*
winpr_get_current_time_zone_rule(const TIME_ZONE_RULE_ENTRY* rules, UINT32 count)
{
	UINT32 i;
	UINT64 windows_time;
	windows_time = winpr_windows_gmtime();

	for (i = 0; i < count; i++)
	{
		if ((rules[i].TicksStart >= windows_time) && (windows_time >= rules[i].TicksEnd))
		{
			/*WLog_ERR(TAG,  "Got rule %d from table at %p with count %"PRIu32"", i, (void*) rules,
			 * count);*/
			return &rules[i];
		}
	}

	WLog_ERR(TAG, "Unable to get current timezone rule");
	return NULL;
}

DWORD GetTimeZoneInformation(LPTIME_ZONE_INFORMATION lpTimeZoneInformation)
{
	time_t t;
	struct tm* local_time;
	TIME_ZONE_ENTRY* dtz;
	LPTIME_ZONE_INFORMATION tz = lpTimeZoneInformation;
	lpTimeZoneInformation->StandardBias = 0;
	time(&t);
	local_time = localtime(&t);
	memset(tz, 0, sizeof(TIME_ZONE_INFORMATION));
#ifdef HAVE_TM_GMTOFF
	{
		long bias = -(local_time->tm_gmtoff / 60L);

		if (bias > INT32_MAX)
			bias = INT32_MAX;

		tz->Bias = (LONG)bias;
	}
#else
	tz->Bias = 0;
#endif
	dtz = winpr_detect_windows_time_zone();

	if (dtz != NULL)
	{
		int status;
		WLog_DBG(TAG, "tz: Bias=%" PRId32 " sn='%s' dln='%s'", dtz->Bias, dtz->StandardName,
		         dtz->DaylightName);
		tz->Bias = dtz->Bias;
		tz->StandardBias = 0;
		tz->DaylightBias = 0;
		ZeroMemory(tz->StandardName, sizeof(tz->StandardName));
		ZeroMemory(tz->DaylightName, sizeof(tz->DaylightName));
		status = MultiByteToWideChar(CP_UTF8, 0, dtz->StandardName, -1, tz->StandardName,
		                             sizeof(tz->StandardName) / sizeof(WCHAR) - 1);

		if (status < 1)
		{
			WLog_ERR(TAG, "StandardName conversion failed - using default");
			goto out_error;
		}

		status = MultiByteToWideChar(CP_UTF8, 0, dtz->DaylightName, -1, tz->DaylightName,
		                             sizeof(tz->DaylightName) / sizeof(WCHAR) - 1);

		if (status < 1)
		{
			WLog_ERR(TAG, "DaylightName conversion failed - using default");
			goto out_error;
		}

		if ((dtz->SupportsDST) && (dtz->RuleTableCount > 0))
		{
			const TIME_ZONE_RULE_ENTRY* rule =
			    winpr_get_current_time_zone_rule(dtz->RuleTable, dtz->RuleTableCount);

			if (rule != NULL)
			{
				tz->DaylightBias = -rule->DaylightDelta;
				tz->StandardDate = rule->StandardDate;
				tz->DaylightDate = rule->DaylightDate;
			}
		}

		free(dtz);
		/* 1 ... TIME_ZONE_ID_STANDARD
		 * 2 ... TIME_ZONE_ID_DAYLIGHT */
		return local_time->tm_isdst ? 2 : 1;
	}

	/* could not detect timezone, use computed bias from tm_gmtoff */
	WLog_DBG(TAG, "tz not found, using computed bias %" PRId32 ".", tz->Bias);
out_error:
	free(dtz);
	memcpy(tz->StandardName, L"Client Local Time", sizeof(tz->StandardName));
	memcpy(tz->DaylightName, L"Client Local Time", sizeof(tz->DaylightName));
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
