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

STOPWATCH* stopwatch_create()
{
	STOPWATCH* sw;

	sw = (STOPWATCH*) malloc(sizeof(STOPWATCH));
	stopwatch_reset(sw);

	return sw;
}

void stopwatch_free(STOPWATCH* stopwatch)
{
	free(stopwatch);
}

void stopwatch_start(STOPWATCH* stopwatch)
{
	stopwatch->start = clock();
	stopwatch->count++;
}

void stopwatch_stop(STOPWATCH* stopwatch)
{
	stopwatch->end = clock();
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
	return ((double) stopwatch->elapsed) / CLOCKS_PER_SEC;
}

void stopwatch_get_elapsed_time_in_useconds(STOPWATCH* stopwatch, UINT32* sec, UINT32* usec)
{
	double uelapsed;
	double clocks_per_usec;

	*sec = ((UINT32) stopwatch->elapsed) / CLOCKS_PER_SEC;
	uelapsed = stopwatch->elapsed - ((double)(*sec) * CLOCKS_PER_SEC);

	clocks_per_usec = (CLOCKS_PER_SEC / 1000000);

	if (clocks_per_usec > 0.0)
		*usec = (UINT32)(uelapsed / clocks_per_usec);
	else
		*usec = 0;
}

