/* primtest.h
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
 */

#ifndef FREERDP_LIB_PRIMTEST_H
#define FREERDP_LIB_PRIMTEST_H

#include <winpr/crt.h>
#include <winpr/spec.h>
#include <winpr/wtypes.h>
#include <winpr/platform.h>
#include <winpr/crypto.h>

#include <freerdp/primitives.h>

#include "measure.h"

#ifdef WITH_IPP
#include <ipps.h>
#include <ippi.h>
#endif

#ifdef _WIN32
#define ALIGN(x) x
#else
#define ALIGN(x) x DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT)
#endif

#define ABS(_x_) ((_x_) < 0 ? (-(_x_)) : (_x_))
#define MAX_TEST_SIZE 4096

extern int test_sizes[];
#define NUM_TEST_SIZES 10

extern BOOL g_TestPrimitivesPerformance;
extern UINT32 g_Iterations;

extern primitives_t* generic;
extern primitives_t* optimized;

void prim_test_setup(BOOL performance);

typedef pstatus_t (*speed_test_fkt)();

BOOL speed_test(const char* name, const char* dsc, UINT32 iterations, speed_test_fkt generic,
                speed_test_fkt optimized, ...);

#endif /* FREERDP_LIB_PRIMTEST_H */
