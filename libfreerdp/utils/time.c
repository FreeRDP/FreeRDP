/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/windows.h>

#include <freerdp/utils/time.h>

UINT64 freerdp_windows_gmtime()
{
	time_t unix_time;
	UINT64 windows_time;

	time(&unix_time);
	windows_time = freerdp_get_windows_time_from_unix_time(unix_time);

	return windows_time;
}

UINT64 freerdp_get_windows_time_from_unix_time(time_t unix_time)
{
	UINT64 windows_time;
	windows_time = ((UINT64)unix_time * 10000000) + 621355968000000000ULL;
	return windows_time;
}

time_t freerdp_get_unix_time_from_windows_time(UINT64 windows_time)
{
	time_t unix_time;
	unix_time = (windows_time - 621355968000000000ULL) / 10000000;
	return unix_time;
}
