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

#include "timezone.h"

/**
 * Read SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_read_system_time(wStream* s, SYSTEM_TIME* system_time)
{
	stream_read_UINT16(s, system_time->wYear); /* wYear, must be set to 0 */
	stream_read_UINT16(s, system_time->wMonth); /* wMonth */
	stream_read_UINT16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	stream_read_UINT16(s, system_time->wDay); /* wDay */
	stream_read_UINT16(s, system_time->wHour); /* wHour */
	stream_read_UINT16(s, system_time->wMinute); /* wMinute */
	stream_read_UINT16(s, system_time->wSecond); /* wSecond */
	stream_read_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
}

/**
 * Write SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_write_system_time(wStream* s, SYSTEM_TIME* system_time)
{
	stream_write_UINT16(s, system_time->wYear); /* wYear, must be set to 0 */
	stream_write_UINT16(s, system_time->wMonth); /* wMonth */
	stream_write_UINT16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	stream_write_UINT16(s, system_time->wDay); /* wDay */
	stream_write_UINT16(s, system_time->wHour); /* wHour */
	stream_write_UINT16(s, system_time->wMinute); /* wMinute */
	stream_write_UINT16(s, system_time->wSecond); /* wSecond */
	stream_write_UINT16(s, system_time->wMilliseconds); /* wMilliseconds */
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
	char* str = NULL;
	TIME_ZONE_INFO* clientTimeZone;

	if (stream_get_left(s) < 172)
		return FALSE;

	clientTimeZone = settings->ClientTimeZone;

	stream_read_UINT32(s, clientTimeZone->bias); /* Bias */

	/* standardName (64 bytes) */
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) stream_get_tail(s), 64 / 2, &str, 0, NULL, NULL);
	stream_seek(s, 64);
	strncpy(clientTimeZone->standardName, str, sizeof(clientTimeZone->standardName));
	free(str);
	str = NULL;

	rdp_read_system_time(s, &clientTimeZone->standardDate); /* StandardDate */
	stream_read_UINT32(s, clientTimeZone->standardBias); /* StandardBias */

	/* daylightName (64 bytes) */
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) stream_get_tail(s), 64 / 2, &str, 0, NULL, NULL);
	stream_seek(s, 64);
	strncpy(clientTimeZone->daylightName, str, sizeof(clientTimeZone->daylightName));
	free(str);

	rdp_read_system_time(s, &clientTimeZone->daylightDate); /* DaylightDate */
	stream_read_UINT32(s, clientTimeZone->daylightBias); /* DaylightBias */

	return TRUE;
}

/**
 * Write client time zone information (TS_TIME_ZONE_INFORMATION).\n
 * @msdn{cc240477}
 * @param s stream
 * @param settings settings
 */

void rdp_write_client_time_zone(wStream* s, rdpSettings* settings)
{
	WCHAR* standardName = NULL;
	WCHAR* daylightName = NULL;
	int standardNameLength;
	int daylightNameLength;
	TIME_ZONE_INFO* clientTimeZone;

	clientTimeZone = settings->ClientTimeZone;
	freerdp_time_zone_detect(clientTimeZone);

	standardNameLength = ConvertToUnicode(CP_UTF8, 0, clientTimeZone->standardName, -1, &standardName, 0) * 2;
	daylightNameLength = ConvertToUnicode(CP_UTF8, 0, clientTimeZone->daylightName, -1, &daylightName, 0) * 2;

	if (standardNameLength > 62)
		standardNameLength = 62;

	if (daylightNameLength > 62)
		daylightNameLength = 62;

    /* Bias */
 	stream_write_UINT32(s, clientTimeZone->bias);

	/* standardName (64 bytes) */
	stream_write(s, standardName, standardNameLength);
	stream_write_zero(s, 64 - standardNameLength);

    /* StandardDate */
    rdp_write_system_time(s, &clientTimeZone->standardDate);

	DEBUG_TIMEZONE("bias=%d stdName='%s' dlName='%s'", clientTimeZone->bias, clientTimeZone->standardName, clientTimeZone->daylightName);

	/* Note that StandardBias is ignored if no valid standardDate is provided. */
    /* StandardBias */
	stream_write_UINT32(s, clientTimeZone->standardBias);
	DEBUG_TIMEZONE("StandardBias=%d", clientTimeZone->standardBias);

	/* daylightName (64 bytes) */
    stream_write(s, daylightName, daylightNameLength);
    stream_write_zero(s, 64 - daylightNameLength);

     /* DaylightDate */
    rdp_write_system_time(s, &clientTimeZone->daylightDate);

	/* Note that DaylightBias is ignored if no valid daylightDate is provided. */
    /* DaylightBias */
	stream_write_UINT32(s, clientTimeZone->daylightBias);
	DEBUG_TIMEZONE("DaylightBias=%d", clientTimeZone->daylightBias);

	free(standardName);
	free(daylightName);
}
