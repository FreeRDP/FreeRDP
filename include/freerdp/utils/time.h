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

#ifndef FREERDP_TIME_UTILS_H
#define FREERDP_TIME_UTILS_H

#define __USE_XOPEN
#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/api.h>
#include <freerdp/types.h>

FREERDP_API UINT64 freerdp_windows_gmtime();
FREERDP_API UINT64 freerdp_get_windows_time_from_unix_time(time_t unix_time);
FREERDP_API time_t freerdp_get_unix_time_from_windows_time(UINT64 windows_time);
FREERDP_API time_t freerdp_get_unix_time_from_generalized_time(const char* generalized_time);

#endif /* FREERDP_TIME_UTILS_H */
