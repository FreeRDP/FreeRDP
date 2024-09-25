/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2024 Thincast Technologies GmbH
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include <winpr/crypto.h>
#include <freerdp/utils/encoded_types.h>

#define MIN(x, y) ((x) < (y)) ? (x) : (y)
#define MAX(x, y) ((x) > (y)) ? (x) : (y)

static BOOL test_signed_integer_read_write_equal(INT32 value)
{
	INT32 rvalue = 0;
	BYTE buffer[32] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	if (!freerdp_write_four_byte_signed_integer(s, value))
	{
		(void)fprintf(stderr, "[%s(%" PRId32 ")] failed to write to stream\n", __func__, value);
		return FALSE;
	}
	if (!Stream_SetPosition(s, 0))
	{
		(void)fprintf(stderr, "[%s(%" PRId32 ")] failed to reset stream position\n", __func__,
		              value);
		return FALSE;
	}
	if (!freerdp_read_four_byte_signed_integer(s, &rvalue))
	{
		(void)fprintf(stderr, "[%s(%" PRId32 ")] failed to read from stream\n", __func__, value);
		return FALSE;
	}
	if (value != rvalue)
	{
		(void)fprintf(stderr, "[%s(%" PRId32 ")] read invalid value %" PRId32 " from stream\n",
		              __func__, value, rvalue);
		return FALSE;
	}
	return TRUE;
}

static BOOL test_signed_integer_write_oor(INT32 value)
{
	BYTE buffer[32] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	if (freerdp_write_four_byte_signed_integer(s, value))
	{
		(void)fprintf(stderr,
		              "[%s(%" PRId32 ")] out of range value not detected and written to stream\n",
		              __func__, value);
		return FALSE;
	}
	return TRUE;
}

static BOOL test_signed_integers(void)
{
	const INT32 outofrange[] = { FREERDP_FOUR_BYTE_SIGNED_INT_MAX + 1,
		                         FREERDP_FOUR_BYTE_SIGNED_INT_MIN - 1, INT32_MAX, INT32_MIN };
	const INT32 limits[] = { 1, 0, -1, FREERDP_FOUR_BYTE_SIGNED_INT_MAX,
		                     FREERDP_FOUR_BYTE_SIGNED_INT_MIN };

	for (size_t x = 0; x < ARRAYSIZE(limits); x++)
	{
		if (!test_signed_integer_read_write_equal(limits[x]))
			return FALSE;
	}
	for (size_t x = 0; x < ARRAYSIZE(outofrange); x++)
	{
		if (!test_signed_integer_write_oor(outofrange[x]))
			return FALSE;
	}
	for (size_t x = 0; x < 100000; x++)
	{
		INT32 val = 0;
		winpr_RAND(&val, sizeof(val));
		val = MAX(val, 0);
		val = MIN(val, FREERDP_FOUR_BYTE_SIGNED_INT_MAX);

		const INT32 nval = -val;
		if (!test_signed_integer_read_write_equal(val))
			return FALSE;
		if (!test_signed_integer_read_write_equal(nval))
			return FALSE;
	}
	return TRUE;
}

static BOOL test_float_read_write_equal(double value)
{
	BYTE exp = 0;
	double rvalue = FP_NAN;
	BYTE buffer[32] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	if (!freerdp_write_four_byte_float(s, value))
	{
		(void)fprintf(stderr, "[%s(%lf)] failed to write to stream\n", __func__, value);
		return FALSE;
	}
	if (!Stream_SetPosition(s, 0))
	{
		(void)fprintf(stderr, "[%s(%lf)] failed to reset stream position\n", __func__, value);
		return FALSE;
	}
	if (!freerdp_read_four_byte_float_exp(s, &rvalue, &exp))
	{
		(void)fprintf(stderr, "[%s(%lf)] failed to read from stream\n", __func__, value);
		return FALSE;
	}
	const double diff = fabs(value - rvalue);
	const double expdiff = diff * pow(10, exp);
	if (expdiff >= 1.0)
	{
		(void)fprintf(stderr, "[%s(%lf)] read invalid value %lf from stream\n", __func__, value,
		              rvalue);
		return FALSE;
	}
	return TRUE;
}

static BOOL test_floag_write_oor(double value)
{
	BYTE buffer[32] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	WINPR_ASSERT(s);

	if (freerdp_write_four_byte_float(s, value))
	{
		(void)fprintf(stderr, "[%s(%lf)] out of range value not detected and written to stream\n",
		              __func__, value);
		return FALSE;
	}
	return TRUE;
}

static double get(void)
{
	double val = NAN;
	do
	{
		winpr_RAND(&val, sizeof(val));
	} while ((val < 0.0) || (val > FREERDP_FOUR_BYTE_FLOAT_MAX) || isnan(val) || isinf(val));
	return val;
}

static BOOL test_floats(void)
{
	const double outofrange[] = { FREERDP_FOUR_BYTE_FLOAT_MAX + 1, FREERDP_FOUR_BYTE_FLOAT_MIN - 1,
		                          DBL_MAX, -DBL_MAX };
	const double limits[] = { 100045.26129238126,         1, 0, -1, FREERDP_FOUR_BYTE_FLOAT_MAX,
		                      FREERDP_FOUR_BYTE_FLOAT_MIN };

	for (size_t x = 0; x < ARRAYSIZE(limits); x++)
	{
		if (!test_float_read_write_equal(limits[x]))
			return FALSE;
	}
	for (size_t x = 0; x < ARRAYSIZE(outofrange); x++)
	{
		if (!test_floag_write_oor(outofrange[x]))
			return FALSE;
	}
	for (size_t x = 0; x < 100000; x++)
	{
		double val = get();

		const double nval = -val;
		if (!test_float_read_write_equal(val))
			return FALSE;
		if (!test_float_read_write_equal(nval))
			return FALSE;
	}
	return TRUE;
}

int TestEncodedTypes(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_signed_integers())
		return -1;
	if (!test_floats())
		return -1;
	return 0;
}
