/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Stopwatch Utils
 *
 * Copyright 2011 Stephen Erisman
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

#ifndef __UTILS_STOPWATCH_H
#define __UTILS_STOPWATCH_H

#include <time.h>
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>

struct _STOPWATCH
{
	clock_t start;
	clock_t end;
	double elapsed;
	clock_t count;
};
typedef struct _STOPWATCH STOPWATCH;

FREERDP_API STOPWATCH* stopwatch_create();
FREERDP_API void stopwatch_free(STOPWATCH* stopwatch);

FREERDP_API void stopwatch_start(STOPWATCH* stopwatch);
FREERDP_API void stopwatch_stop(STOPWATCH* stopwatch);
FREERDP_API void stopwatch_reset(STOPWATCH* stopwatch);

FREERDP_API double stopwatch_get_elapsed_time_in_seconds(STOPWATCH* stopwatch);
FREERDP_API void stopwatch_get_elapsed_time_in_useconds(STOPWATCH* stopwatch, uint32* sec, uint32* usec);

#endif /* __UTILS_STOPWATCH_H */
