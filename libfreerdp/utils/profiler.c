/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Profiler Utils
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

#include <freerdp/utils/profiler.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("utils")

struct S_PROFILER
{
	char* name;
	STOPWATCH* stopwatch;
};

PROFILER* profiler_create(const char* name)
{
	PROFILER* profiler = (PROFILER*)calloc(1, sizeof(PROFILER));

	if (!profiler)
		return NULL;

	profiler->name = _strdup(name);
	profiler->stopwatch = stopwatch_create();

	if (!profiler->name || !profiler->stopwatch)
		goto fail;

	return profiler;
fail:
	profiler_free(profiler);
	return NULL;
}

void profiler_free(PROFILER* profiler)
{
	if (profiler)
	{
		free(profiler->name);
		stopwatch_free(profiler->stopwatch);
	}

	free(profiler);
}

void profiler_enter(PROFILER* profiler)
{
	stopwatch_start(profiler->stopwatch);
}

void profiler_exit(PROFILER* profiler)
{
	stopwatch_stop(profiler->stopwatch);
}

void profiler_print_header(void)
{
	WLog_INFO(TAG,
	          "-------------------------------+------------+-------------+-----------+-------");
	WLog_INFO(TAG,
	          "PROFILER NAME                  |      COUNT |       TOTAL |       AVG |    IPS");
	WLog_INFO(TAG,
	          "-------------------------------+------------+-------------+-----------+-------");
}

void profiler_print(PROFILER* profiler)
{
	double s = stopwatch_get_elapsed_time_in_seconds(profiler->stopwatch);
	double avg = profiler->stopwatch->count == 0 ? 0 : s / profiler->stopwatch->count;
	WLog_INFO(TAG, "%-30s | %10u | %10.4fs | %8.6fs | %6.0f", profiler->name,
	          profiler->stopwatch->count, s, avg, profiler->stopwatch->count / s);
}

void profiler_print_footer(void)
{
	WLog_INFO(TAG,
	          "-------------------------------+------------+-------------+-----------+-------");
}
