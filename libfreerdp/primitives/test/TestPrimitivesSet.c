/* test_set.c
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

/* ------------------------------------------------------------------------- */
static BOOL check8(const BYTE* src, UINT32 length, UINT32 offset, BYTE value)
{
	UINT32 i;

	for (i = 0; i < length; ++i)
	{
		if (src[offset + i] != value)
		{
			printf("SET8U FAILED: off=%" PRIu32 " len=%" PRIu32 " dest[%" PRIu32 "]=0x%02" PRIx8
			       "\n",
			       offset, length, i + offset, src[i + offset]);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL test_set8u_func(void)
{
	pstatus_t status;
	UINT32 off;

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		BYTE dest[1024];

		memset(dest, 3, sizeof(dest));
		for (len = 1; len < 48 - off; ++len)
		{
			status = generic->set_8u(0xa5, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check8(dest, len, off, 0xa5))
				return FALSE;
		}
	}

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		BYTE dest[1024];

		memset(dest, 3, sizeof(dest));
		for (len = 1; len < 48 - off; ++len)
		{
			status = optimized->set_8u(0xa5, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check8(dest, len, off, 0xa5))
				return FALSE;
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_set8u_speed(void)
{
	BYTE dest[1024];
	BYTE value;
	UINT32 x;

	for (x = 0; x < 16; x++)
	{
		winpr_RAND(&value, sizeof(value));

		if (!speed_test("set_8u", "", g_Iterations, (speed_test_fkt)generic->set_8u,
		                (speed_test_fkt)optimized->set_8u, value, dest + x, x))
			return FALSE;
	}

	return TRUE;
}

static BOOL check32s(const INT32* src, UINT32 length, UINT32 offset, INT32 value)
{
	UINT32 i;

	for (i = 0; i < length; ++i)
	{
		if (src[offset + i] != value)
		{
			printf("SET8U FAILED: off=%" PRIu32 " len=%" PRIu32 " dest[%" PRIu32 "]=0x%08" PRIx32
			       "\n",
			       offset, length, i + offset, src[i + offset]);
			return FALSE;
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_set32s_func(void)
{
	pstatus_t status;
	UINT32 off;
	const INT32 value = -0x12345678;

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		INT32 dest[1024] = { 0 };

		for (len = 1; len < 48 - off; ++len)
		{
			status = generic->set_32s(value, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check32s(dest, len, off, value))
				return FALSE;
		}
	}

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		INT32 dest[1024] = { 0 };

		for (len = 1; len < 48 - off; ++len)
		{
			status = optimized->set_32s(value, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check32s(dest, len, off, value))
				return FALSE;
		}
	}

	return TRUE;
}

static BOOL check32u(const UINT32* src, UINT32 length, UINT32 offset, UINT32 value)
{
	UINT32 i;

	for (i = 0; i < length; ++i)
	{
		if (src[offset + i] != value)
		{
			printf("SET8U FAILED: off=%" PRIu32 " len=%" PRIu32 " dest[%" PRIu32 "]=0x%08" PRIx32
			       "\n",
			       offset, length, i + offset, src[i + offset]);
			return FALSE;
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_set32u_func(void)
{
	pstatus_t status;
	UINT32 off;
	const UINT32 value = 0xABCDEF12;

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		UINT32 dest[1024] = { 0 };

		for (len = 1; len < 48 - off; ++len)
		{
			status = generic->set_32u(value, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check32u(dest, len, off, value))
				return FALSE;
		}
	}

	for (off = 0; off < 16; ++off)
	{
		UINT32 len;
		UINT32 dest[1024] = { 0 };

		for (len = 1; len < 48 - off; ++len)
		{
			status = optimized->set_32u(value, dest + off, len);

			if (status != PRIMITIVES_SUCCESS)
				return FALSE;

			if (!check32u(dest, len, off, value))
				return FALSE;
		}
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_set32u_speed(void)
{
	UINT32 dest[1024];
	BYTE value;
	UINT32 x;

	for (x = 0; x < 16; x++)
	{
		winpr_RAND(&value, sizeof(value));

		if (!speed_test("set_32u", "", g_Iterations, (speed_test_fkt)generic->set_32u,
		                (speed_test_fkt)optimized->set_32u, value, dest + x, x))
			return FALSE;
	}

	return TRUE;
}

/* ------------------------------------------------------------------------- */
static BOOL test_set32s_speed(void)
{
	INT32 dest[1024];
	BYTE value;
	UINT32 x;

	for (x = 0; x < 16; x++)
	{
		winpr_RAND(&value, sizeof(value));

		if (!speed_test("set_32s", "", g_Iterations, (speed_test_fkt)generic->set_32s,
		                (speed_test_fkt)optimized->set_32s, value, dest + x, x))
			return FALSE;
	}

	return TRUE;
}

int TestPrimitivesSet(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	prim_test_setup(FALSE);

	if (!test_set8u_func())
		return -1;

	if (!test_set32s_func())
		return -1;

	if (!test_set32u_func())
		return -1;

	if (g_TestPrimitivesPerformance)
	{
		if (!test_set8u_speed())
			return -1;

		if (!test_set32s_speed())
			return -1;

		if (!test_set32u_speed())
			return -1;
	}

	return 0;
}
