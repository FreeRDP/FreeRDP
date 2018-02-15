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

#ifndef FREERDP_UTILS_PROFILER_H
#define FREERDP_UTILS_PROFILER_H

#include <freerdp/api.h>
#include <freerdp/utils/stopwatch.h>

struct _PROFILER
{
	char* name;
	STOPWATCH* stopwatch;
};
typedef struct _PROFILER PROFILER;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API PROFILER* profiler_create(char* name);
FREERDP_API void profiler_free(PROFILER* profiler);

FREERDP_API void profiler_enter(PROFILER* profiler);
FREERDP_API void profiler_exit(PROFILER* profiler);

FREERDP_API void profiler_print_header(void);
FREERDP_API void profiler_print(PROFILER* profiler);
FREERDP_API void profiler_print_footer(void);

#ifdef __cplusplus
}
#endif

#ifdef WITH_PROFILER
#define IF_PROFILER(then)			then
#define PROFILER_DEFINE(prof)		PROFILER* prof;
#define PROFILER_CREATE(prof,name)	prof = profiler_create(name);
#define PROFILER_FREE(prof)			profiler_free(prof);
#define PROFILER_ENTER(prof)		profiler_enter(prof);
#define PROFILER_EXIT(prof)			profiler_exit(prof);
#define PROFILER_PRINT_HEADER		profiler_print_header();
#define PROFILER_PRINT(prof)		profiler_print(prof);
#define PROFILER_PRINT_FOOTER		profiler_print_footer();
#else
#define IF_PROFILER(then)		do { } while (0)

#define PROFILER_DEFINE(prof)
#define PROFILER_CREATE(prof,name)	do { } while (0);
#define PROFILER_FREE(prof)		do { } while (0);
#define PROFILER_ENTER(prof)		do { } while (0);
#define PROFILER_EXIT(prof)		do { } while (0);
#define PROFILER_PRINT_HEADER		do { } while (0);
#define PROFILER_PRINT(prof)		do { } while (0);
#define PROFILER_PRINT_FOOTER		do { } while (0);
#endif

#endif /* FREERDP_UTILS_PROFILER_H */
