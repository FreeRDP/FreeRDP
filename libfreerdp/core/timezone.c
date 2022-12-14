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

#include "timezone.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core.timezone")

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
	DEBUG_TIMEZONE("Time: y=%" PRIu16 ",m=%" PRIu16 ",dow=%" PRIu16 ",d=%" PRIu16 ", %02" PRIu16
	               ":%02" PRIu16 ":%02" PRIu16 ".%03" PRIu16 "",
	               system_time->wYear, system_time->wMonth, system_time->wDayOfWeek,
	               system_time->wDay, system_time->wHour, system_time->wMinute,
	               system_time->wSecond, system_time->wMilliseconds);
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

	GetTimeZoneInformation(tz);
	if (!Stream_EnsureRemainingCapacity(s, 4ull + sizeof(tz->StandardName)))
		return FALSE;

	/* Bias */
	Stream_Write_UINT32(s, tz->Bias);
	/* standardName (64 bytes) */
	Stream_Write(s, tz->StandardName, sizeof(tz->StandardName));
	/* StandardDate */
	if (!rdp_write_system_time(s, &tz->StandardDate))
		return FALSE;

#ifdef WITH_DEBUG_TIMEZONE
	WLog_DBG(TIMEZONE_TAG, "bias=%" PRId32 "", tz->Bias);
	WLog_DBG(TIMEZONE_TAG, "StandardName:");
	winpr_HexDump(TIMEZONE_TAG, WLOG_DEBUG, (const BYTE*)tz->StandardName,
	              sizeof(tz->StandardName));
	WLog_DBG(TIMEZONE_TAG, "DaylightName:");
	winpr_HexDump(TIMEZONE_TAG, WLOG_DEBUG, (const BYTE*)tz->DaylightName,
	              sizeof(tz->DaylightName));
#endif
	/* Note that StandardBias is ignored if no valid standardDate is provided. */
	/* StandardBias */
	if (!Stream_EnsureRemainingCapacity(s, 4ull + sizeof(tz->DaylightName)))
		return FALSE;
	Stream_Write_UINT32(s, tz->StandardBias);
	DEBUG_TIMEZONE("StandardBias=%" PRId32 "", tz->StandardBias);
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
	DEBUG_TIMEZONE("DaylightBias=%" PRId32 "", tz->DaylightBias);
	return TRUE;
}
