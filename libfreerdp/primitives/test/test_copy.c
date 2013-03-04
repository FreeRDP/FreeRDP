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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include "prim_test.h"

static const int MEMCPY_PRETEST_ITERATIONS = 1000000;
static const int TEST_TIME = 1.0;  // seconds
#define COPY_TESTSIZE (256*2+16*2+15+15)
#if 0
extern pstatus_t sse3_copy_8u(const BYTE *pSrc, BYTE *pDst, int len);
#endif

/* ------------------------------------------------------------------------- */
int test_copy8u_func(void)
{
	primitives_t *prims = primitives_get();
	BYTE ALIGN(data[COPY_TESTSIZE+15]);
	int i, soff;
	int failed = 0;
	char testStr[256];
	BYTE ALIGN(dest[COPY_TESTSIZE+15]);

	testStr[0] = '\0';
	get_random_data(data, sizeof(data));

	strcat(testStr, " ptr");
	for (soff=0; soff<16; ++soff)
	{
		int doff;
		for (doff=0; doff<16; ++doff)
		{
			int length;
			for (length=1; length<=COPY_TESTSIZE-doff; ++length)
			{
				memset(dest, 0, sizeof(dest));
				prims->copy_8u(data+soff, dest+doff, length);
				for (i=0; i<length; ++i)
				{
					if (dest[i+doff] != data[i+soff])
					{
						printf("COPY8U FAIL: off=%d len=%d, dest[%d]=0x%02x"
							"data[%d]=0x%02x\n",
							doff, length, i+doff, dest[i+doff],
							i+soff, data[i+soff]);
						failed = 1;
					}
				}
			}
		}
	}
	if (!failed) printf("All copy8 tests passed (%s).\n", testStr);
	return (failed > 0) ? FAILURE : SUCCESS;
}

/* ------------------------------------------------------------------------- */
STD_SPEED_TEST(copy8u_speed_test, BYTE, BYTE, dst=dst,
	TRUE, memcpy(dst, src1, size),
	FALSE, PRIM_NOP, 0, FALSE,
	TRUE, ippsCopy_8u(src1, dst, size));

int test_copy8u_speed(void)
{
	BYTE ALIGN(src[MAX_TEST_SIZE+4]);
	BYTE ALIGN(dst[MAX_TEST_SIZE+4]);
	copy8u_speed_test("copy8u", "aligned", src, NULL, 0, dst,
		test_sizes, NUM_TEST_SIZES, MEMCPY_PRETEST_ITERATIONS, TEST_TIME);
	copy8u_speed_test("copy8u", "unaligned", src+1, NULL, 0, dst,
		test_sizes, NUM_TEST_SIZES, MEMCPY_PRETEST_ITERATIONS, TEST_TIME);
	return SUCCESS;
}
