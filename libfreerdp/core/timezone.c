/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Time Zone Redirection
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/timezone.h>

#include "settings.h"
#include "timezone.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.timezone")

#if !defined(WITH_DEBUG_TIMEZONE)
#define log_timezone(tzif, result)
#else
#define log_timezone(tzif, result) log_timezone_((tzif), (result), __FILE__, __func__, __LINE__)
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
		_snprintf(buffer, len, "{ not set }");
	else
	{
		_snprintf(buffer, len,
		          "{ %" PRIu16 "-%" PRIu16 "-%" PRIu16 " [%s] %" PRIu16 ":%" PRIu16 ":%" PRIu16
		          ".%" PRIu16 "}",
		          t->wYear, t->wMonth, t->wDay, weekday2str(t->wDayOfWeek), t->wHour, t->wMinute,
		          t->wSecond, t->wMilliseconds);
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

static void log_timezone_(const TIME_ZONE_INFORMATION* tzif, DWORD result, const char* file,
                          const char* fkt, size_t line)
{
	WINPR_ASSERT(tzif);

	char buffer[64] = { 0 };
	DWORD level = WLOG_TRACE;
	wLog* log = WLog_Get(TIMEZONE_TAG);
	log_print(log, level, file, fkt, line, "TIME_ZONE_INFORMATION {");
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
#endif

static BOOL rdp_read_system_time(wStream* s, SYSTEMTIME* system_time);
static BOOL rdp_write_system_time(wStream* s, const SYSTEMTIME* system_time);

/**
 * Read SYSTEM_TIME structure (TS_SYSTEMTIME).
 * msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

BOOL rdp_read_system_time(wStream* s, SYSTEMTIME* system_time)
{
	WINPR_ASSERT(system_time);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16ull))
		return FALSE;

	Stream_Read_UINT16(s, system_time->wYear);         /* wYear, must be set to 0 */
	Stream_Read_UINT16(s, system_time->wMonth);        /* wMonth */
	Stream_Read_UINT16(s, system_time->wDayOfWeek);    /* wDayOfWeek */
	Stream_Read_UINT16(s, system_time->wDay);          /* wDay */
	Stream_Read_UINT16(s, system_time->wHour);         /* wHour */
	Stream_Read_UINT16(s, system_time->wMinute);       /* wMinute */
	Stream_Read_UINT16(s, system_time->wSecond);       /* wSecond */
	Stream_Read_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
	return TRUE;
}

/**
 * Write SYSTEM_TIME structure (TS_SYSTEMTIME).
 * msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

BOOL rdp_write_system_time(wStream* s, const SYSTEMTIME* system_time)
{
	WINPR_ASSERT(system_time);
	if (!Stream_EnsureRemainingCapacity(s, 16ull))
		return FALSE;

	Stream_Write_UINT16(s, system_time->wYear);         /* wYear, must be set to 0 */
	Stream_Write_UINT16(s, system_time->wMonth);        /* wMonth */
	Stream_Write_UINT16(s, system_time->wDayOfWeek);    /* wDayOfWeek */
	Stream_Write_UINT16(s, system_time->wDay);          /* wDay */
	Stream_Write_UINT16(s, system_time->wHour);         /* wHour */
	Stream_Write_UINT16(s, system_time->wMinute);       /* wMinute */
	Stream_Write_UINT16(s, system_time->wSecond);       /* wSecond */
	Stream_Write_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
	return TRUE;
}

/**
 * Read client time zone information (TS_TIME_ZONE_INFORMATION).
 * msdn{cc240477}
 * @param s stream
 * @param settings settings
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_read_client_time_zone(wStream* s, rdpSettings* settings)
{
	LPTIME_ZONE_INFORMATION tz = { 0 };

	if (!s || !settings)
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 172))
		return FALSE;

	tz = settings->ClientTimeZone;

	if (!tz)
		return FALSE;

	Stream_Read_UINT32(s, tz->Bias); /* Bias */
	/* standardName (64 bytes) */
	Stream_Read(s, tz->StandardName, sizeof(tz->StandardName));
	if (!rdp_read_system_time(s, &tz->StandardDate)) /* StandardDate */
		return FALSE;
	Stream_Read_UINT32(s, tz->StandardBias);    /* StandardBias */
	/* daylightName (64 bytes) */
	Stream_Read(s, tz->DaylightName, sizeof(tz->DaylightName));
	if (!rdp_read_system_time(s, &tz->DaylightDate)) /* DaylightDate */
		return FALSE;
	Stream_Read_UINT32(s, tz->DaylightBias);    /* DaylightBias */
	log_timezone(tz, 0);
	return TRUE;
}

/**
 * Write client time zone information (TS_TIME_ZONE_INFORMATION).
 * msdn{cc240477}
 * @param s stream
 * @param settings settings
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL rdp_write_client_time_zone(wStream* s, rdpSettings* settings)
{
	LPTIME_ZONE_INFORMATION tz = { 0 };

	WINPR_ASSERT(settings);
	tz = settings->ClientTimeZone;

	if (!tz)
		return FALSE;

	log_timezone(tz, 0);
	if (!Stream_EnsureRemainingCapacity(s, 4ull + sizeof(tz->StandardName)))
		return FALSE;

	/* Bias */
	Stream_Write_UINT32(s, tz->Bias);
	/* standardName (64 bytes) */
	Stream_Write(s, tz->StandardName, sizeof(tz->StandardName));
	/* StandardDate */
	if (!rdp_write_system_time(s, &tz->StandardDate))
		return FALSE;

	/* Note that StandardBias is ignored if no valid standardDate is provided. */
	/* StandardBias */
	if (!Stream_EnsureRemainingCapacity(s, 4ull + sizeof(tz->DaylightName)))
		return FALSE;
	Stream_Write_UINT32(s, tz->StandardBias);

	/* daylightName (64 bytes) */
	Stream_Write(s, tz->DaylightName, sizeof(tz->DaylightName));
	/* DaylightDate */
	if (!rdp_write_system_time(s, &tz->DaylightDate))
		return FALSE;
	/* Note that DaylightBias is ignored if no valid daylightDate is provided. */
	/* DaylightBias */
	if (!Stream_EnsureRemainingCapacity(s, 4ull))
		return FALSE;
	Stream_Write_UINT32(s, tz->DaylightBias);

	return TRUE;
}
