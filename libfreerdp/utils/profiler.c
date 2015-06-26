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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <freerdp/utils/profiler.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("utils")

PROFILER* profiler_create(char* name)
{
	PROFILER* profiler;

	profiler = (PROFILER*) malloc(sizeof(PROFILER));
	if (!profiler)
		return NULL;
	
	profiler->name = name;
	profiler->stopwatch = stopwatch_create();
	if (!profiler->stopwatch)
	{
		free(profiler);
		return NULL;
	}

	return profiler;
}

void profiler_free(PROFILER* profiler)
{	
	stopwatch_free(profiler->stopwatch);
	
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

void profiler_print_header()
{
	WLog_INFO(TAG,  "                                             |-----------------------|");
	WLog_INFO(TAG,  "                PROFILER                     |    elapsed seconds    |");
	WLog_INFO(TAG,  "|--------------------------------------------|-----------------------|");
	WLog_INFO(TAG,  "| code section                  | iterations |     total |      avg. |");
	WLog_INFO(TAG,  "|-------------------------------|------------|-----------|-----------|");
}

void profiler_print(PROFILER* profiler)
{
	double elapsed_sec = stopwatch_get_elapsed_time_in_seconds(profiler->stopwatch);
	double avg_sec = elapsed_sec / (double) profiler->stopwatch->count;
	WLog_INFO(TAG,  "| %-30.30s| %10du | %9f | %9f |",
			  profiler->name, profiler->stopwatch->count, elapsed_sec, avg_sec);
}

void profiler_print_footer()
{
	WLog_INFO(TAG,  "|--------------------------------------------------------------------|");
}
