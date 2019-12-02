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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <freerdp/utils/stopwatch.h>

#ifdef _WIN32
LARGE_INTEGER stopwatch_freq = { 0, 0 };
#else
#include <sys/time.h>
#endif

static void stopwatch_set_time(UINT64* usecs)
{
#ifdef _WIN32
	LARGE_INTEGER perfcount;
	QueryPerformanceCounter(&perfcount);
	*usecs = (perfcount.QuadPart * 1000000) / stopwatch_freq.QuadPart;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*usecs = tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

STOPWATCH* stopwatch_create(void)
{
	STOPWATCH* sw;
#ifdef _WIN32
	if (stopwatch_freq.QuadPart == 0)
	{
		QueryPerformanceFrequency(&stopwatch_freq);
	}
#endif

	sw = (STOPWATCH*)malloc(sizeof(STOPWATCH));
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
	return (stopwatch->elapsed / 1000000.0);
}

void stopwatch_get_elapsed_time_in_useconds(STOPWATCH* stopwatch, UINT32* sec, UINT32* usec)
{
	*sec = (UINT32)stopwatch->elapsed / 1000000;
	*usec = (UINT32)stopwatch->elapsed % 1000000;
}
