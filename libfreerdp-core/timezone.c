/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "timezone.h"

/**
 * Read SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_read_system_time(STREAM* s, SYSTEM_TIME* system_time)
{
	stream_read_uint16(s, system_time->wYear); /* wYear, must be set to 0 */
	stream_read_uint16(s, system_time->wMonth); /* wMonth */
	stream_read_uint16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	stream_read_uint16(s, system_time->wDay); /* wDay */
	stream_read_uint16(s, system_time->wHour); /* wHour */
	stream_read_uint16(s, system_time->wMinute); /* wMinute */
	stream_read_uint16(s, system_time->wSecond); /* wSecond */
	stream_read_uint16(s, system_time->wMilliseconds); /* wMilliseconds */
}

/**
 * Write SYSTEM_TIME structure (TS_SYSTEMTIME).\n
 * @msdn{cc240478}
 * @param s stream
 * @param system_time system time structure
 */

void rdp_write_system_time(STREAM* s, SYSTEM_TIME* system_time)
{
	stream_write_uint16(s, system_time->wYear); /* wYear, must be set to 0 */
	stream_write_uint16(s, system_time->wMonth); /* wMonth */
	stream_write_uint16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	stream_write_uint16(s, system_time->wDay); /* wDay */
	stream_write_uint16(s, system_time->wHour); /* wHour */
	stream_write_uint16(s, system_time->wMinute); /* wMinute */
	stream_write_uint16(s, system_time->wSecond); /* wSecond */
	stream_write_uint16(s, system_time->wMilliseconds); /* wMilliseconds */
}

/**
 * Read client time zone information (TS_TIME_ZONE_INFORMATION).\n
 * @msdn{cc240477}
 * @param s stream
 * @param settings settings
 */

boolean rdp_read_client_time_zone(STREAM* s, rdpSettings* settings)
{
	char* str;
	TIME_ZONE_INFO* clientTimeZone;

	if (stream_get_left(s) < 172)
		return false;

	clientTimeZone = settings->client_time_zone;

	stream_read_uint32(s, clientTimeZone->bias); /* Bias */

	/* standardName (64 bytes) */
	str = freerdp_uniconv_in(settings->uniconv, stream_get_tail(s), 64);
	stream_seek(s, 64);
	strncpy(clientTimeZone->standardName, str, sizeof(clientTimeZone->standardName));
	xfree(str);

	rdp_read_system_time(s, &clientTimeZone->standardDate); /* StandardDate */
	stream_read_uint32(s, clientTimeZone->standardBias); /* StandardBias */

	/* daylightName (64 bytes) */
	str = freerdp_uniconv_in(settings->uniconv, stream_get_tail(s), 64);
	stream_seek(s, 64);
	strncpy(clientTimeZone->daylightName, str, sizeof(clientTimeZone->daylightName));
	xfree(str);

	rdp_read_system_time(s, &clientTimeZone->daylightDate); /* DaylightDate */
	stream_read_uint32(s, clientTimeZone->daylightBias); /* DaylightBias */

	return true;
}

/**
 * Write client time zone information (TS_TIME_ZONE_INFORMATION).\n
 * @msdn{cc240477}
 * @param s stream
 * @param settings settings
 */

void rdp_write_client_time_zone(STREAM* s, rdpSettings* settings)
{
	size_t length;
	uint8* standardName;
	uint8* daylightName;
	size_t standardNameLength;
	size_t daylightNameLength;
	TIME_ZONE_INFO* clientTimeZone;

	clientTimeZone = settings->client_time_zone;
	freerdp_time_zone_detect(clientTimeZone);

	standardName = (uint8*) freerdp_uniconv_out(settings->uniconv, clientTimeZone->standardName, &length);
	standardNameLength = length;

	daylightName = (uint8*) freerdp_uniconv_out(settings->uniconv, clientTimeZone->daylightName, &length);
	daylightNameLength = length;

	if (standardNameLength > 62)
		standardNameLength = 62;

	if (daylightNameLength > 62)
		daylightNameLength = 62;

	stream_write_uint32(s, clientTimeZone->bias); /* Bias */

	/* standardName (64 bytes) */
	stream_write(s, standardName, standardNameLength);
	stream_write_zero(s, 64 - standardNameLength);

	rdp_write_system_time(s, &clientTimeZone->standardDate); /* StandardDate */
	stream_write_uint32(s, clientTimeZone->standardBias); /* StandardBias */

	/* daylightName (64 bytes) */
	stream_write(s, daylightName, daylightNameLength);
	stream_write_zero(s, 64 - daylightNameLength);

	rdp_write_system_time(s, &clientTimeZone->daylightDate); /* DaylightDate */
	stream_write_uint32(s, clientTimeZone->daylightBias); /* DaylightBias */

	xfree(standardName);
	xfree(daylightName);
}
