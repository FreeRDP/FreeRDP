/* measure.h
 * Macros to help with performance measurement.
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.  Algorithms used by
 * this code may be covered by patents by HP, Microsoft, or other parties.
 *
 * MEASURE_LOOP_START("measurement", 2000)
 *   code to be measured
 * MEASURE_LOOP_STOP
 *   buffer flush and such
 * MEASURE_SHOW_RESULTS
 *
 * Define GOOGLE_PROFILER if you want gperftools included.
 */

#ifdef _GNUC_
# pragma once
#endif

#ifndef __MEASURE_H_INCLUDED__
#define __MEASURE_H_INCLUDED__

#include <time.h>

#ifndef _WIN32
#include <sys/param.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GOOGLE_PROFILER
#include <gperftools/profiler.h>
#define PROFILER_START(_prefix_)  \
    do {  \
		char _path[PATH_MAX];  \
		sprintf(_path, "./%s.prof", (_prefix_));  \
		ProfilerStart(_path);  \
    } while (0);
# define PROFILER_STOP  \
    do {  \
		ProfilerStop(); \
    } while (0);
#else
#define PROFILER_START(_prefix_)
#define PROFILER_STOP
#endif // GOOGLE_PROFILER

extern float _delta_time(const struct timespec *t0, const struct timespec *t1);
extern void _floatprint(float t, char *output);

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif // !CLOCK_MONOTONIC_RAW

#define MEASURE_LOOP_START(_prefix_, _count_)  \
{   struct timespec _start, _stop;  \
    char *_prefix;  \
    int _count = (_count_);  \
	int _loop; \
	float _delta; \
	char _str1[32], _str2[32]; \
    _prefix = strdup(_prefix_);  \
	_str1[0] = '\0'; _str2[0] = '\0'; \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_start);  \
    PROFILER_START(_prefix);  \
    _loop = (_count);  \
    do {

#define MEASURE_LOOP_STOP  \
    } while (--_loop);

#define MEASURE_GET_RESULTS(_result_)  \
    PROFILER_STOP;  \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_stop);  \
    _delta = _delta_time(&_start, &_stop);  \
    (_result_) = (float) _count / _delta;  \
    free(_prefix);  \
}

#define MEASURE_SHOW_RESULTS(_result_)  \
    PROFILER_STOP; \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_stop); \
    _delta = _delta_time(&_start, &_stop); \
    (_result_) = (float) _count / _delta; \
    _floatprint((float) _count / _delta, _str1); \
    printf("%s: %9d iterations in %5.1f seconds = %s/s \n",	\
	_prefix, _count, _delta, _str1); \
    free(_prefix); \
}

#define MEASURE_SHOW_RESULTS_SCALED(_scale_, _label_) \
    PROFILER_STOP; \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_stop); \
    _delta = _delta_time(&_start, &_stop); \
    _floatprint((float) _count / _delta, _str1); \
    _floatprint((float) _count / _delta * (_scale_), _str2); \
    printf("%s: %9d iterations in %5.1f seconds = %s/s = %s%s \n",	\
	_prefix, _count, _delta, _str1, _str2, _label_); \
    free(_prefix); \
}

#define MEASURE_TIMED(_label_, _init_iter_, _test_time_, _result_, _call_)  \
{   float _r;  \
    MEASURE_LOOP_START(_label_, _init_iter_);  \
    _call_;  \
    MEASURE_LOOP_STOP;  \
    MEASURE_GET_RESULTS(_r);  \
    MEASURE_LOOP_START(_label_, _r * _test_time_);  \
    _call_;  \
    MEASURE_LOOP_STOP;  \
    MEASURE_SHOW_RESULTS(_result_);  \
}

#endif // __MEASURE_H_INCLUDED__
