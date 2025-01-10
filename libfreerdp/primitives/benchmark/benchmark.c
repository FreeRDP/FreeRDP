/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * primitives benchmarking tool
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <stdio.h>

#include <winpr/crypto.h>
#include <winpr/sysinfo.h>
#include <freerdp/primitives.h>

typedef struct
{
	BYTE* channels[3];
	UINT32 steps[3];
	prim_size_t roi;
	BYTE* outputBuffer;
	BYTE* outputChannels[3];
	BYTE* rgbBuffer;
	UINT32 outputStride;
	UINT32 testedFormat;
} primitives_YUV_benchmark;

static void primitives_YUV_benchmark_free(primitives_YUV_benchmark* bench)
{
	if (!bench)
		return;

	free(bench->outputBuffer);
	free(bench->rgbBuffer);

	for (size_t i = 0; i < 3; i++)
	{
		free(bench->outputChannels[i]);
		free(bench->channels[i]);
	}

	const primitives_YUV_benchmark empty = { 0 };
	*bench = empty;
}

static primitives_YUV_benchmark primitives_YUV_benchmark_init(void)
{
	primitives_YUV_benchmark ret = { 0 };
	ret.roi.width = 3840 * 4;
	ret.roi.height = 2160 * 4;
	ret.outputStride = ret.roi.width * 4;
	ret.testedFormat = PIXEL_FORMAT_BGRA32;

	ret.outputBuffer = calloc(ret.outputStride, ret.roi.height);
	if (!ret.outputBuffer)
		goto fail;
	ret.rgbBuffer = calloc(ret.outputStride, ret.roi.height);
	if (!ret.rgbBuffer)
		goto fail;
	winpr_RAND(ret.rgbBuffer, 1ULL * ret.outputStride * ret.roi.height);

	for (size_t i = 0; i < 3; i++)
	{
		ret.channels[i] = calloc(ret.roi.width, ret.roi.height);
		ret.outputChannels[i] = calloc(ret.roi.width, ret.roi.height);
		if (!ret.channels[i] || !ret.outputChannels[i])
			goto fail;

		winpr_RAND(ret.channels[i], 1ull * ret.roi.width * ret.roi.height);
		ret.steps[i] = ret.roi.width;
	}

	return ret;

fail:
	primitives_YUV_benchmark_free(&ret);
	return ret;
}

static const char* print_time(UINT64 t, char* buffer, size_t size)
{
	(void)_snprintf(buffer, size, "%u.%03u.%03u.%03u", (unsigned)(t / 1000000000ull),
	                (unsigned)((t / 1000000ull) % 1000), (unsigned)((t / 1000ull) % 1000),
	                (unsigned)((t) % 1000));
	return buffer;
}

static BOOL primitives_YUV420_benchmark_run(primitives_YUV_benchmark* bench, primitives_t* prims)
{
	const BYTE* channels[3] = { 0 };

	for (size_t i = 0; i < 3; i++)
		channels[i] = bench->channels[i];

	for (size_t x = 0; x < 10; x++)
	{
		const UINT64 start = winpr_GetTickCount64NS();
		pstatus_t status =
		    prims->YUV420ToRGB_8u_P3AC4R(channels, bench->steps, bench->outputBuffer,
		                                 bench->outputStride, bench->testedFormat, &bench->roi);
		const UINT64 end = winpr_GetTickCount64NS();
		if (status != PRIMITIVES_SUCCESS)
		{
			(void)fprintf(stderr, "Running YUV420ToRGB_8u_P3AC4R failed\n");
			return FALSE;
		}
		const UINT64 diff = end - start;
		char buffer[32] = { 0 };
		printf("[%" PRIuz "] YUV420ToRGB_8u_P3AC4R %" PRIu32 "x%" PRIu32 " took %sns\n", x,
		       bench->roi.width, bench->roi.height, print_time(diff, buffer, sizeof(buffer)));
	}

	return TRUE;
}

static BOOL primitives_YUV444_benchmark_run(primitives_YUV_benchmark* bench, primitives_t* prims)
{
	const BYTE* channels[3] = { 0 };

	for (size_t i = 0; i < 3; i++)
		channels[i] = bench->channels[i];

	for (size_t x = 0; x < 10; x++)
	{
		const UINT64 start = winpr_GetTickCount64NS();
		pstatus_t status =
		    prims->YUV444ToRGB_8u_P3AC4R(channels, bench->steps, bench->outputBuffer,
		                                 bench->outputStride, bench->testedFormat, &bench->roi);
		const UINT64 end = winpr_GetTickCount64NS();
		if (status != PRIMITIVES_SUCCESS)
		{
			(void)fprintf(stderr, "Running YUV444ToRGB_8u_P3AC4R failed\n");
			return FALSE;
		}
		const UINT64 diff = end - start;
		char buffer[32] = { 0 };
		printf("[%" PRIuz "] YUV444ToRGB_8u_P3AC4R %" PRIu32 "x%" PRIu32 " took %sns\n", x,
		       bench->roi.width, bench->roi.height, print_time(diff, buffer, sizeof(buffer)));
	}

	return TRUE;
}

static BOOL primitives_RGB2420_benchmark_run(primitives_YUV_benchmark* bench, primitives_t* prims)
{
	for (size_t x = 0; x < 10; x++)
	{
		const UINT64 start = winpr_GetTickCount64NS();
		pstatus_t status =
		    prims->RGBToYUV420_8u_P3AC4R(bench->rgbBuffer, bench->testedFormat, bench->outputStride,
		                                 bench->outputChannels, bench->steps, &bench->roi);
		const UINT64 end = winpr_GetTickCount64NS();
		if (status != PRIMITIVES_SUCCESS)
		{
			(void)fprintf(stderr, "Running RGBToYUV420_8u_P3AC4R failed\n");
			return FALSE;
		}
		const UINT64 diff = end - start;
		char buffer[32] = { 0 };
		printf("[%" PRIuz "] RGBToYUV420_8u_P3AC4R %" PRIu32 "x%" PRIu32 " took %sns\n", x,
		       bench->roi.width, bench->roi.height, print_time(diff, buffer, sizeof(buffer)));
	}

	return TRUE;
}

static BOOL primitives_RGB2444_benchmark_run(primitives_YUV_benchmark* bench, primitives_t* prims)
{
	for (size_t x = 0; x < 10; x++)
	{
		const UINT64 start = winpr_GetTickCount64NS();
		pstatus_t status =
		    prims->RGBToYUV444_8u_P3AC4R(bench->rgbBuffer, bench->testedFormat, bench->outputStride,
		                                 bench->outputChannels, bench->steps, &bench->roi);
		const UINT64 end = winpr_GetTickCount64NS();
		if (status != PRIMITIVES_SUCCESS)
		{
			(void)fprintf(stderr, "Running RGBToYUV444_8u_P3AC4R failed\n");
			return FALSE;
		}
		const UINT64 diff = end - start;
		char buffer[32] = { 0 };
		printf("[%" PRIuz "] RGBToYUV444_8u_P3AC4R %" PRIu32 "x%" PRIu32 " took %sns\n", x,
		       bench->roi.width, bench->roi.height, print_time(diff, buffer, sizeof(buffer)));
	}

	return TRUE;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	primitives_YUV_benchmark bench = primitives_YUV_benchmark_init();

	for (primitive_hints hint = PRIMITIVES_PURE_SOFT; hint < PRIMITIVES_AUTODETECT; hint++)
	{
		const char* hintstr = primtives_hint_str(hint);
		primitives_t* prim = primitives_get_by_type(hint);
		if (!prim)
		{
			(void)fprintf(stderr, "failed to get primitives: %s\n", hintstr);
			goto fail;
		}

		printf("Running YUV420 -> RGB benchmark on %s implementation:\n", hintstr);
		if (!primitives_YUV420_benchmark_run(&bench, prim))
		{
			(void)fprintf(stderr, "YUV420 -> RGB benchmark failed\n");
			goto fail;
		}
		printf("\n");

		printf("Running RGB -> YUV420 benchmark on %s implementation:\n", hintstr);
		if (!primitives_RGB2420_benchmark_run(&bench, prim))
		{
			(void)fprintf(stderr, "RGB -> YUV420 benchmark failed\n");
			goto fail;
		}
		printf("\n");

		printf("Running YUV444 -> RGB benchmark on %s implementation:\n", hintstr);
		if (!primitives_YUV444_benchmark_run(&bench, prim))
		{
			(void)fprintf(stderr, "YUV444 -> RGB benchmark failed\n");
			goto fail;
		}
		printf("\n");

		printf("Running RGB -> YUV444 benchmark on %s implementation:\n", hintstr);
		if (!primitives_RGB2444_benchmark_run(&bench, prim))
		{
			(void)fprintf(stderr, "RGB -> YUV444 benchmark failed\n");
			goto fail;
		}
		printf("\n");
	}
fail:
	primitives_YUV_benchmark_free(&bench);
	return 0;
}
