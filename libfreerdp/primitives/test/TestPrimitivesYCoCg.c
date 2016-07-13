/* test_YCoCg.c
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#include <winpr/sysinfo.h>
#include "prim_test.h"

/* ------------------------------------------------------------------------- */
static BOOL test_YCoCgRToRGB_8u_AC4R_func(void)
{
	BOOL result = TRUE;
	pstatus_t status;
	INT32 ALIGN(out_sse[4098]), ALIGN(out_sse_inv[4098]);
	INT32 ALIGN(in[4098]);
	INT32 ALIGN(out_c[4098]), ALIGN(out_c_inv[4098]);

	UINT32 i, x;
	const UINT32 formats[] = {
		PIXEL_FORMAT_ARGB32,
		PIXEL_FORMAT_ABGR32,
		PIXEL_FORMAT_RGBA32,
		PIXEL_FORMAT_RGBX32,
		PIXEL_FORMAT_BGRA32,
		PIXEL_FORMAT_BGRX32
	};

	winpr_RAND((BYTE*)in, sizeof(in));

	for (x=0; x<sizeof(formats)/sizeof(formats[0]); x++)
	{
		UINT32 format = formats[x];

		status = generic->YCoCgToRGB_8u_AC4R(
				 (const BYTE*)(in + 1), 63 * 4,
				 (BYTE*) out_c, format, 63 * 4, 63, 61, 2, TRUE);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;
		status = generic->YCoCgToRGB_8u_AC4R(
				 (const BYTE*)(in + 1), 63 * 4,
				 (BYTE*) out_c_inv, format, 63 * 4, 63, 61, 2, TRUE);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;

		status = optimized->YCoCgToRGB_8u_AC4R(
				 (const BYTE*)(in + 1), 63 * 4,
				 (BYTE*) out_sse, format, 63 * 4, 63, 61, 2, TRUE);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;
		status = optimized->YCoCgToRGB_8u_AC4R(
				 (const BYTE*)(in + 1), 63 * 4,
				 (BYTE*) out_sse_inv, format, 63 * 4, 63, 61, 2, TRUE);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;

		for (i = 0; i < 63 * 61; ++i)
		{
			if (out_c[i] != out_sse[i])
			{
				printf("optimized->YCoCgRToRGB FAIL[%d]: 0x%08x -> C 0x%08x vs optimized 0x%08x\n", i,
				       in[i + 1], out_c[i], out_sse[i]);
				result = FALSE;
			}
		}

		for (i = 0; i < 63 * 61; ++i)
		{
			if (out_c_inv[i] != out_sse_inv[i])
			{
				printf("optimized->YCoCgRToRGB inverted FAIL[%d]: 0x%08x -> C 0x%08x vs optimized 0x%08x\n",
				       i,
				       in[i + 1], out_c_inv[i], out_sse_inv[i]);
				result = FALSE;
			}
		}
	}
	return result;
}

static int test_YCoCgRToRGB_8u_AC4R_speed(void)
{
	INT32 ALIGN(in[4096]);
	INT32 ALIGN(out[4096]);

	winpr_RAND((BYTE*)in, sizeof(in));

	if (!speed_test("YCoCgToRGB_8u_AC4R", "aligned", g_Iterations,
			(speed_test_fkt)generic->YCoCgToRGB_8u_AC4R,
			(speed_test_fkt)optimized->YCoCgToRGB_8u_AC4R,
			in, 64 * 4, out, 64 * 4, 64, 64, 2, FALSE, FALSE))
		return FALSE;

	return TRUE;
}

int TestPrimitivesYCoCg(int argc, char* argv[])
{
	prim_test_setup(FALSE);

	if (!test_YCoCgRToRGB_8u_AC4R_func())
		return 1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_YCoCgRToRGB_8u_AC4R_speed())
			return 1;
	}

	return 0;
}
