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

#ifndef FREERDP_UTILS_STOPWATCH_H
#define FREERDP_UTILS_STOPWATCH_H

#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct
{
	UINT64 start;
	UINT64 end;
	UINT64 elapsed;
	UINT32 count;
} STOPWATCH;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API STOPWATCH* stopwatch_create(void);
	FREERDP_API void stopwatch_free(STOPWATCH* stopwatch);

	FREERDP_API void stopwatch_start(STOPWATCH* stopwatch);
	FREERDP_API void stopwatch_stop(STOPWATCH* stopwatch);
	FREERDP_API void stopwatch_reset(STOPWATCH* stopwatch);

	FREERDP_API double stopwatch_get_elapsed_time_in_seconds(STOPWATCH* stopwatch);
	FREERDP_API void stopwatch_get_elapsed_time_in_useconds(STOPWATCH* stopwatch, UINT32* sec,
	                                                        UINT32* usec);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_STOPWATCH_H */
