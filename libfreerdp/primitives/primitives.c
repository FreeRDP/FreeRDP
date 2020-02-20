/* primitives.c
 * This code queries processor features and calls the init/deinit routines.
 * vi:ts=4 sw=4
 *
 * Copyright 2011 Martin Fleisz <martin.fleisz@thincast.com>
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Copyright 2019 David Fort <contact@hardening-consulting.com>
 *
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

#include <string.h>
#include <stdlib.h>

#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/crypto.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

#define TAG FREERDP_TAG("primitives")

/* hints to know which kind of primitives to use */
static primitive_hints primitivesHints = PRIMITIVES_AUTODETECT;
static BOOL primitives_init_optimized(primitives_t* prims);

void primitives_set_hints(primitive_hints hints)
{
	primitivesHints = hints;
}

primitive_hints primitives_get_hints(void)
{
	return primitivesHints;
}

/* Singleton pointer used throughout the program when requested. */
static primitives_t pPrimitivesGeneric = { 0 };
static INIT_ONCE generic_primitives_InitOnce = INIT_ONCE_STATIC_INIT;

#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
static primitives_t pPrimitivesCpu = { 0 };
static INIT_ONCE cpu_primitives_InitOnce = INIT_ONCE_STATIC_INIT;

#endif
#if defined(WITH_OPENCL)
static primitives_t pPrimitivesGpu = { 0 };
static INIT_ONCE gpu_primitives_InitOnce = INIT_ONCE_STATIC_INIT;

#endif

static INIT_ONCE auto_primitives_InitOnce = INIT_ONCE_STATIC_INIT;

static primitives_t pPrimitives = { 0 };

/* ------------------------------------------------------------------------- */
static BOOL primitives_init_generic(primitives_t* prims)
{
	primitives_init_add(prims);
	primitives_init_andor(prims);
	primitives_init_alphaComp(prims);
	primitives_init_copy(prims);
	primitives_init_set(prims);
	primitives_init_shift(prims);
	primitives_init_sign(prims);
	primitives_init_colors(prims);
	primitives_init_YCoCg(prims);
	primitives_init_YUV(prims);
	prims->uninit = NULL;
	return TRUE;
}

static BOOL CALLBACK primitives_init_generic_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);
	return primitives_init_generic(&pPrimitivesGeneric);
}

static BOOL primitives_init_optimized(primitives_t* prims)
{
	primitives_init_generic(prims);

#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
	primitives_init_add_opt(prims);
	primitives_init_andor_opt(prims);
	primitives_init_alphaComp_opt(prims);
	primitives_init_copy_opt(prims);
	primitives_init_set_opt(prims);
	primitives_init_shift_opt(prims);
	primitives_init_sign_opt(prims);
	primitives_init_colors_opt(prims);
	primitives_init_YCoCg_opt(prims);
	primitives_init_YUV_opt(prims);
	prims->flags |= PRIM_FLAGS_HAVE_EXTCPU;
#endif
	return TRUE;
}

typedef struct
{
	BYTE* channels[3];
	UINT32 steps[3];
	prim_size_t roi;
	BYTE* outputBuffer;
	UINT32 outputStride;
	UINT32 testedFormat;
} primitives_YUV_benchmark;

static void primitives_YUV_benchmark_free(primitives_YUV_benchmark* bench)
{
	int i;
	if (!bench)
		return;

	free(bench->outputBuffer);

	for (i = 0; i < 3; i++)
		free(bench->channels[i]);
	memset(bench, 0, sizeof(primitives_YUV_benchmark));
}

static primitives_YUV_benchmark* primitives_YUV_benchmark_init(primitives_YUV_benchmark* ret)
{
	int i;
	prim_size_t* roi;
	if (!ret)
		return NULL;

	memset(ret, 0, sizeof(primitives_YUV_benchmark));
	roi = &ret->roi;
	roi->width = 1024;
	roi->height = 768;
	ret->outputStride = roi->width * 4;
	ret->testedFormat = PIXEL_FORMAT_BGRA32;

	ret->outputBuffer = malloc(ret->outputStride * roi->height);
	if (!ret->outputBuffer)
		goto fail;

	for (i = 0; i < 3; i++)
	{
		BYTE* buf = ret->channels[i] = malloc(roi->width * roi->height);
		if (!buf)
			goto fail;

		winpr_RAND(buf, roi->width * roi->height);
		ret->steps[i] = roi->width;
	}

	return ret;

fail:
	primitives_YUV_benchmark_free(ret);
	return ret;
}

static BOOL primitives_YUV_benchmark_run(primitives_YUV_benchmark* bench, primitives_t* prims,
                                         UINT64 runTime, UINT32* computations)
{
	ULONGLONG dueDate;
	const BYTE* channels[3];
	size_t i;
	pstatus_t status;

	*computations = 0;

	for (i = 0; i < 3; i++)
		channels[i] = bench->channels[i];

	/* do a first dry run to initialize cache and such */
	status = prims->YUV420ToRGB_8u_P3AC4R(channels, bench->steps, bench->outputBuffer,
	                                      bench->outputStride, bench->testedFormat, &bench->roi);
	if (status != PRIMITIVES_SUCCESS)
		return FALSE;

	/* let's run the benchmark */
	dueDate = GetTickCount64() + runTime;
	while (GetTickCount64() < dueDate)
	{
		pstatus_t status =
		    prims->YUV420ToRGB_8u_P3AC4R(channels, bench->steps, bench->outputBuffer,
		                                 bench->outputStride, bench->testedFormat, &bench->roi);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;
		*computations = *computations + 1;
	}
	return TRUE;
}

static BOOL primitives_autodetect_best(primitives_t* prims)
{
	size_t x;
	BOOL ret = FALSE;
	UINT64 benchDuration = 150; /* 150 ms */
	struct prim_benchmark
	{
		const char* name;
		primitives_t* prims;
		UINT32 flags;
		UINT32 count;
	};

	struct prim_benchmark testcases[] =
	{
		{ "generic", NULL, PRIMITIVES_PURE_SOFT, 0 },
#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
		{ "optimized", NULL, PRIMITIVES_ONLY_CPU, 0 },
#endif
#if defined(WITH_OPENCL)
		{ "opencl", NULL, PRIMITIVES_ONLY_GPU, 0 },
#endif
	};
	const struct prim_benchmark* best = NULL;

	primitives_YUV_benchmark bench;
	primitives_YUV_benchmark* yuvBench = primitives_YUV_benchmark_init(&bench);
	if (!yuvBench)
		return FALSE;

	WLog_DBG(TAG, "primitives benchmark result:");
	for (x = 0; x < ARRAYSIZE(testcases); x++)
	{
		struct prim_benchmark* cur = &testcases[x];
		cur->prims = primitives_get_by_type(cur->flags);
		if (!cur->prims)
		{
			WLog_WARN(TAG, "Failed to initialize %s primitives", cur->name);
			continue;
		}
		if (!primitives_YUV_benchmark_run(yuvBench, cur->prims, benchDuration, &cur->count))
		{
			WLog_WARN(TAG, "error running %s YUV bench", cur->name);
			continue;
		}

		WLog_DBG(TAG, " * %s= %" PRIu32, cur->name, cur->count);
		if (!best || (best->count < cur->count))
			best = cur;
	}

	if (!best)
	{
		WLog_ERR(TAG, "No primitives to test, aborting.");
		goto out;
	}
	/* finally compute the results */
	*prims = *best->prims;

	WLog_INFO(TAG, "primitives autodetect, using %s", best->name);
	ret = TRUE;
out:
	if (!ret)
		*prims = pPrimitivesGeneric;
	primitives_YUV_benchmark_free(yuvBench);
	return ret;
}

#if defined(WITH_OPENCL)
static BOOL CALLBACK primitives_init_gpu_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);

	if (!primitives_init_opencl(&pPrimitivesGpu))
		return FALSE;

	return TRUE;
}
#endif

#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
static BOOL CALLBACK primitives_init_cpu_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);

	if (!primitives_init_optimized(&pPrimitivesCpu))
		return FALSE;

	return TRUE;
}
#endif

static BOOL CALLBACK primitives_auto_init_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);

	return primitives_init(&pPrimitives, primitivesHints);
}

BOOL primitives_init(primitives_t* p, primitive_hints hints)
{
	switch (hints)
	{
		case PRIMITIVES_AUTODETECT:
			return primitives_autodetect_best(p);
		case PRIMITIVES_PURE_SOFT:
			*p = pPrimitivesGeneric;
			return TRUE;
		case PRIMITIVES_ONLY_CPU:
#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
			*p = pPrimitivesCpu;
			return TRUE;
#endif
		case PRIMITIVES_ONLY_GPU:
#if defined(WITH_OPENCL)
			*p = pPrimitivesGpu;
			return TRUE;
#endif
		default:
			WLog_ERR(TAG, "unknown hint %d", hints);
			return FALSE;
	}
}

void primitives_uninit(void)
{
#if defined(WITH_OPENCL)
	if (pPrimitivesGpu.uninit)
		pPrimitivesGpu.uninit();
#endif
#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
	if (pPrimitivesCpu.uninit)
		pPrimitivesCpu.uninit();
#endif
	if (pPrimitivesGeneric.uninit)
		pPrimitivesGeneric.uninit();
}

/* ------------------------------------------------------------------------- */
static void setup(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic_cb, NULL, NULL);
#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
	InitOnceExecuteOnce(&cpu_primitives_InitOnce, primitives_init_cpu_cb, NULL, NULL);
#endif
#if defined(WITH_OPENCL)
	InitOnceExecuteOnce(&gpu_primitives_InitOnce, primitives_init_gpu_cb, NULL, NULL);
#endif
	InitOnceExecuteOnce(&auto_primitives_InitOnce, primitives_auto_init_cb, NULL, NULL);
}

primitives_t* primitives_get(void)
{
	setup();
	return &pPrimitives;
}

primitives_t* primitives_get_generic(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic_cb, NULL, NULL);
	return &pPrimitivesGeneric;
}

primitives_t* primitives_get_by_type(DWORD type)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic_cb, NULL, NULL);

	switch (type)
	{
		case PRIMITIVES_ONLY_GPU:
#if defined(WITH_OPENCL)
			if (!InitOnceExecuteOnce(&gpu_primitives_InitOnce, primitives_init_gpu_cb, NULL, NULL))
				return NULL;
			return &pPrimitivesGpu;
#endif
		case PRIMITIVES_ONLY_CPU:
#if defined(HAVE_CPU_OPTIMIZED_PRIMITIVES)
			if (!InitOnceExecuteOnce(&cpu_primitives_InitOnce, primitives_init_cpu_cb, NULL, NULL))
				return NULL;
			return &pPrimitivesCpu;
#endif
		case PRIMITIVES_PURE_SOFT:
		default:
			return &pPrimitivesGeneric;
	}
}

DWORD primitives_flags(primitives_t* p)
{
	return p->flags;
}
