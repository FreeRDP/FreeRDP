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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/timezone.h>

#include "timezone.h"

static void rdp_read_system_time(wStream* s, SYSTEMTIME* system_time);
static void rdp_write_system_time(wStream* s, SYSTEMTIME* system_time);

/**
 * Read SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_read_system_time(wStream* s, SYSTEMTIME* system_time)
{
	Stream_Read_UINT16(s, system_time->wYear); /* wYear, must be set to 0 */
	Stream_Read_UINT16(s, system_time->wMonth); /* wMonth */
	Stream_Read_UINT16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	Stream_Read_UINT16(s, system_time->wDay); /* wDay */
	Stream_Read_UINT16(s, system_time->wHour); /* wHour */
	Stream_Read_UINT16(s, system_time->wMinute); /* wMinute */
	Stream_Read_UINT16(s, system_time->wSecond); /* wSecond */
	Stream_Read_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
}

/**
 * Write SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_write_system_time(wStream* s, SYSTEMTIME* system_time)
{
	Stream_Write_UINT16(s, system_time->wYear); /* wYear, must be set to 0 */
	Stream_Write_UINT16(s, system_time->wMonth); /* wMonth */
	Stream_Write_UINT16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	Stream_Write_UINT16(s, system_time->wDay); /* wDay */
	Stream_Write_UINT16(s, system_time->wHour); /* wHour */
	Stream_Write_UINT16(s, system_time->wMinute); /* wMinute */
	Stream_Write_UINT16(s, system_time->wSecond); /* wSecond */
	Stream_Write_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
	DEBUG_TIMEZONE("Time: y=%d,m=%d,dow=%d,d=%d, %02d:%02d:%02d.%03d",
		system_time->wYear, system_time->wMonth, system_time->wDayOfWeek,
		system_time->wDay, system_time->wHour, system_time->wMinute,
		system_time->wSecond, system_time->wMilliseconds);
}

/**
 * Read client time zone information (TS_TIME_ZONE_INFORMATION).\n
 * @msdn{cc240477}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_read_client_time_zone(wStream* s, rdpSettings* settings)
{
	LPTIME_ZONE_INFORMATION tz;

	if (!s || !settings)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 172)
		return FALSE;

	tz = settings->ClientTimeZone;
	if (!tz)
		return FALSE;

	Stream_Read_UINT32(s, tz->Bias); /* Bias */

	/* standardName (64 bytes) */
	Stream_Read(s, tz->StandardName, sizeof(tz->StandardName));

	rdp_read_system_time(s, &tz->StandardDate); /* StandardDate */
	Stream_Read_UINT32(s, tz->StandardBias); /* StandardBias */

	/* daylightName (64 bytes) */
	Stream_Read(s, tz->DaylightName, sizeof(tz->DaylightName));

	rdp_read_system_time(s, &tz->DaylightDate); /* DaylightDate */
	Stream_Read_UINT32(s, tz->DaylightBias); /* DaylightBias */

	return TRUE;
}

/**
 * Write client time zone information (TS_TIME_ZONE_INFORMATION).\n
 * @msdn{cc240477}
 * @param s stream
 * @param settings settings
 */

BOOL rdp_write_client_time_zone(wStream* s, rdpSettings* settings)
{
	LPTIME_ZONE_INFORMATION tz;
	DWORD rc;

	tz = settings->ClientTimeZone;
	if (!tz)
		return FALSE;

	rc = GetTimeZoneInformation(tz);

	/* Bias */
	Stream_Write_UINT32(s, tz->Bias);

	/* standardName (64 bytes) */
	Stream_Write(s, tz->StandardName, sizeof(tz->StandardName));

	/* StandardDate */
	rdp_write_system_time(s, &tz->StandardDate);

	DEBUG_TIMEZONE("bias=%d stdName='%s' dlName='%s'", tz->Bias,
			   tz->StandardName, tz->DaylightName);

	/* Note that StandardBias is ignored if no valid standardDate is provided. */
	/* StandardBias */
	Stream_Write_UINT32(s, tz->StandardBias);
	DEBUG_TIMEZONE("StandardBias=%d", tz->StandardBias);

	/* daylightName (64 bytes) */
	Stream_Write(s, tz->DaylightName, sizeof(tz->DaylightName));

	/* DaylightDate */
	rdp_write_system_time(s, &tz->DaylightDate);

	/* Note that DaylightBias is ignored if no valid daylightDate is provided. */
	/* DaylightBias */
	Stream_Write_UINT32(s, tz->DaylightBias);
	DEBUG_TIMEZONE("DaylightBias=%d", tz->DaylightBias);

	return TRUE;
}

