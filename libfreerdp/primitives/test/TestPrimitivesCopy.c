/* test_copy.c
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

#include <winpr/sysinfo.h>
#include "prim_test.h"

#define COPY_TESTSIZE (256 * 2 + 16 * 2 + 15 + 15)

/* ------------------------------------------------------------------------- */
static BOOL test_copy8u_func(void)
{
	primitives_t* prims = primitives_get();
	BYTE ALIGN(data[COPY_TESTSIZE + 15]) = { 0 };
	int i, soff;
	BYTE ALIGN(dest[COPY_TESTSIZE + 15]) = { 0 };
	winpr_RAND(data, sizeof(data));

	for (soff = 0; soff < 16; ++soff)
	{
		int doff;

		for (doff = 0; doff < 16; ++doff)
		{
			int length;

			for (length = 1; length <= COPY_TESTSIZE - doff; ++length)
			{
				memset(dest, 0, sizeof(dest));

				if (prims->copy_8u(data + soff, dest + doff, length) != PRIMITIVES_SUCCESS)
					return FALSE;

				for (i = 0; i < length; ++i)
				{
					if (dest[i + doff] != data[i + soff])
					{
						printf("COPY8U FAIL: off=%d len=%d, dest[%d]=0x%02" PRIx8 ""
						       "data[%d]=0x%02" PRIx8 "\n",
						       doff, length, i + doff, dest[i + doff], i + soff, data[i + soff]);
						return FALSE;
					}
				}
			}
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_copy8u_speed(void)
{
	BYTE ALIGN(src[MAX_TEST_SIZE + 4]);
	BYTE ALIGN(dst[MAX_TEST_SIZE + 4]);

	if (!speed_test("copy_8u", "aligned", g_Iterations, (speed_test_fkt)generic->copy_8u,
	                (speed_test_fkt)optimized->copy_8u, src, dst, MAX_TEST_SIZE))
		return FALSE;

	if (!speed_test("copy_8u", "unaligned", g_Iterations, (speed_test_fkt)generic->copy_8u,
	                (speed_test_fkt)optimized->copy_8u, src + 1, dst + 1, MAX_TEST_SIZE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesCopy(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	prim_test_setup(FALSE);

	if (!test_copy8u_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_copy8u_speed())
			return 1;
	}

	return 0;
}
