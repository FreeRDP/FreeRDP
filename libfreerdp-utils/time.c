/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Time Utils
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

#include <winpr/windows.h>
#include <freerdp/utils/time.h>

uint64 freerdp_windows_gmtime()
{
	time_t unix_time;
	uint64 windows_time;

	gmtime(&unix_time);
	windows_time = freerdp_get_windows_time_from_unix_time(unix_time);

	return windows_time;
}

uint64 freerdp_get_windows_time_from_unix_time(time_t unix_time)
{
	uint64 windows_time;
	windows_time = (unix_time * 10000000) + 621355968000000000ULL;
	return windows_time;
}

time_t freerdp_get_unix_time_from_windows_time(uint64 windows_time)
{
	time_t unix_time;
	unix_time = (windows_time - 621355968000000000ULL) / 10000000;
	return unix_time;
}

time_t freerdp_get_unix_time_from_generalized_time(const char* generalized_time)
{
	int Y, m, d;
	int H, M, S;
	struct tm gt;
	time_t unix_time = 0;

	/*
	 * GeneralizedTime:
	 *
	 * 12th November 1997 at 15:30:10,5 PM
	 *
	 * "19971112153010.5Z"
	 * "19971112173010.5+0200"
	 */

	memset(&gt, 0, sizeof(struct tm));
	sscanf(generalized_time, "%4d%2d%2d%2d%2d%2d", &Y, &m, &d, &H, &M, &S);

	gt.tm_year = Y - 1900; /* Year since 1900 */
	gt.tm_mon = m; /* Months since January */
	gt.tm_mday = d; /* Day of the month */
	gt.tm_hour = H; /* Hour since midnight */
	gt.tm_min = M; /* Minutes after the hour */
	gt.tm_sec = S; /* Seconds after the minute */

	unix_time = mktime(&gt);

	return unix_time;
}
