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
#include <math.h>
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

float measure_delta_time(UINT64 t0, UINT64 t1)
{
	INT64 diff = (INT64)(t1 - t0);
	double retval = diff / 1000000000.0;
	return (retval < 0.0) ? 0.0f : (float)retval;
}

/* ------------------------------------------------------------------------- */
void measure_floatprint(float t, char* output)
{
	/* I don't want to link against -lm, so avoid log,exp,... */
	float f = 10.0;
	int i = 0;

	while (t > f)
		f *= 10.0;

	f /= 1000.0;
	i = ((int)(t / f + 0.5f)) * (int)f;

	if (t < 0.0f)
		(void)sprintf(output, "%f", t);
	else if (i == 0)
		(void)sprintf(output, "%d", (int)(t + 0.5f));
	else if (t < 1e+3f)
		(void)sprintf(output, "%3d", i);
	else if (t < 1e+6f)
		(void)sprintf(output, "%3d,%03d", i / 1000, i % 1000);
	else if (t < 1e+9f)
		(void)sprintf(output, "%3d,%03d,000", i / 1000000, (i % 1000000) / 1000);
	else if (t < 1e+12f)
		(void)sprintf(output, "%3d,%03d,000,000", i / 1000000000, (i % 1000000000) / 1000000);
	else
		(void)sprintf(output, "%f", t);
}

void prim_test_setup(BOOL performance)
{
	generic = primitives_get_generic();
	optimized = primitives_get();
	g_TestPrimitivesPerformance = performance;
}

BOOL speed_test(const char* name, const char* dsc, UINT32 iterations, speed_test_fkt generic,
                speed_test_fkt optimized, ...)
{
	if (!name || !generic || !optimized || (iterations == 0))
		return FALSE;

	for (UINT32 i = 0; i < iterations; i++)
	{
	}

	return TRUE;
}
