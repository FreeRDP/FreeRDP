/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>

#include <winpr/sysinfo.h>
#include <freerdp/utils/stopwatch.h>

static void stopwatch_set_time(UINT64* usecs)
{
	const UINT64 ns = winpr_GetTickCount64NS();
	*usecs = WINPR_TIME_NS_TO_US(ns);
}

STOPWATCH* stopwatch_create(void)
{
	STOPWATCH* sw = (STOPWATCH*)calloc(1, sizeof(STOPWATCH));
	if (!sw)
		return NULL;
	stopwatch_reset(sw);

	return sw;
}

void stopwatch_free(STOPWATCH* stopwatch)
{
	free(stopwatch);
}

void stopwatch_start(STOPWATCH* stopwatch)
{
	stopwatch_set_time(&stopwatch->start);
	stopwatch->count++;
}

void stopwatch_stop(STOPWATCH* stopwatch)
{
	stopwatch_set_time(&stopwatch->end);
	stopwatch->elapsed += (stopwatch->end - stopwatch->start);
}

void stopwatch_reset(STOPWATCH* stopwatch)
{
	stopwatch->start = 0;
	stopwatch->end = 0;
	stopwatch->elapsed = 0;
	stopwatch->count = 0;
}

double stopwatch_get_elapsed_time_in_seconds(STOPWATCH* stopwatch)
{
	const long double ld = stopwatch->elapsed / 1000000.0L;
	return (double)ld;
}

void stopwatch_get_elapsed_time_in_useconds(STOPWATCH* stopwatch, UINT32* sec, UINT32* usec)
{
	*sec = (UINT32)stopwatch->elapsed / 1000000;
	*usec = (UINT32)stopwatch->elapsed % 1000000;
}
