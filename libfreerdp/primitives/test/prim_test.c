/* prim_test.c
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
 * permissions and limitations under the License.
 */

#include <freerdp/config.h>

#include "prim_test.h"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <winpr/sysinfo.h>
#include <winpr/platform.h>
#include <winpr/crypto.h>

primitives_t* generic = NULL;
primitives_t* optimized = NULL;
BOOL g_TestPrimitivesPerformance = FALSE;
UINT32 g_Iterations = 1000;

int test_sizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };

/* ------------------------------------------------------------------------- */

#ifdef _WIN32
float _delta_time(const struct timespec* t0, const struct timespec* t1)
{
	return 0.0f;
}
#else
float _delta_time(const struct timespec* t0, const struct timespec* t1)
{
	INT64 secs = (INT64)(t1->tv_sec) - (INT64)(t0->tv_sec);
	long nsecs = t1->tv_nsec - t0->tv_nsec;
	double retval;

	if (nsecs < 0)
	{
		--secs;
		nsecs += 1000000000;
	}

	retval = (double)secs + (double)nsecs / (double)1000000000.0;
	return (retval < 0.0) ? 0.0 : (float)retval;
}
#endif

/* ------------------------------------------------------------------------- */
void _floatprint(float t, char* output)
{
	/* I don't want to link against -lm, so avoid log,exp,... */
	float f = 10.0;
	int i;

	while (t > f)
		f *= 10.0;

	f /= 1000.0;
	i = ((int)(t / f + 0.5f)) * (int)f;

	if (t < 0.0f)
		sprintf(output, "%f", t);
	else if (i == 0)
		sprintf(output, "%d", (int)(t + 0.5f));
	else if (t < 1e+3f)
		sprintf(output, "%3d", i);
	else if (t < 1e+6f)
		sprintf(output, "%3d,%03d", i / 1000, i % 1000);
	else if (t < 1e+9f)
		sprintf(output, "%3d,%03d,000", i / 1000000, (i % 1000000) / 1000);
	else if (t < 1e+12f)
		sprintf(output, "%3d,%03d,000,000", i / 1000000000, (i % 1000000000) / 1000000);
	else
		sprintf(output, "%f", t);
}

void prim_test_setup(BOOL performance)
{
	generic = primitives_get_generic();
	optimized = primitives_get();
	g_TestPrimitivesPerformance = performance;
}

BOOL speed_test(const char* name, const char* dsc, UINT32 iterations, pstatus_t (*fkt_generic)(),
                pstatus_t (*optimised)(), ...)
{
	UINT32 i;

	if (!name || !generic || !optimised || (iterations == 0))
		return FALSE;

	for (i = 0; i < iterations; i++)
	{
	}

	return TRUE;
}
