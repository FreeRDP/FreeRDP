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

static primitives_t pPrimitives = { 0 };
static INIT_ONCE primitives_InitOnce = INIT_ONCE_STATIC_INIT;

/* ------------------------------------------------------------------------- */
static BOOL primitives_init_generic(primitives_t *prims)
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

static BOOL primitives_init_optimized(primitives_t *prims)
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

typedef struct {
	BYTE *channels[3];
	UINT32 steps[3];
	prim_size_t roi;
	BYTE *outputBuffer;
	UINT32 outputStride;
	UINT32 testedFormat;
} primitives_YUV_benchmark;

static primitives_YUV_benchmark* primitives_YUV_benchmark_init(void)
{
	int i;
	primitives_YUV_benchmark *ret = calloc(1, sizeof(*ret));
	prim_size_t *roi;
	if (!ret)
		return NULL;

	roi = &ret->roi;
	roi->width = 1024;
	roi->height = 768;

	ret->outputStride = roi->width *4;
	ret->testedFormat = PIXEL_FORMAT_BGRA32;

	ret->outputBuffer = malloc(roi->width * roi->height * 4);
	if (!ret->outputBuffer)
		goto error_output;

	for (i = 0; i < 3; i++)
	{
		BYTE *buf = ret->channels[i] = malloc(roi->width * roi->height);
		if (!buf)
			goto error_channels;

		winpr_RAND(buf, roi->width * roi->height);
		ret->steps[i] = roi->width;
	}

	return ret;

error_channels:
	for(i = 0; i < 3; i++)
		free(ret->channels[i]);
error_output:
	free(ret);
	return NULL;
}

static void primitives_YUV_benchmark_free(primitives_YUV_benchmark **pbench)
{
	int i;
	primitives_YUV_benchmark *bench;
	if (!*pbench)
		return;
	bench = *pbench;

	free(bench->outputBuffer);

	for (i = 0; i < 3; i++)
		free(bench->channels[i]);

	free(bench);
	*pbench = NULL;
}

static BOOL primitives_YUV_benchmark_run(primitives_YUV_benchmark *bench, primitives_t *prims,
		UINT64 runTime,	UINT32 *computations)
{
	ULONGLONG dueDate = GetTickCount64() + runTime;
	const BYTE *channels[3];
	int i;

	*computations = 0;

	for (i = 0; i < 3; i++)
		channels[i] = bench->channels[i];

	while (GetTickCount64() < dueDate)
	{
		pstatus_t status = prims->YUV420ToRGB_8u_P3AC4R(channels, bench->steps, bench->outputBuffer,
				bench->outputStride, bench->testedFormat, &bench->roi);
		if (status != PRIMITIVES_SUCCESS)
			return FALSE;
		*computations = *computations + 1;
	}
	return TRUE;
}

static BOOL primitives_autodetect_best(primitives_t *prims)
{
	BOOL ret = FALSE;
	UINT64 benchDuration = 150; // 100 ms
	UINT32 genericCount = 0, optimizedCount = 0, openclCount = 0;
	UINT32 bestCount;
	primitives_t *genericPrims = primitives_get_generic();
	primitives_t optimizedPrims;
#if defined(WITH_OPENCL)
	primitives_t openclPrims;
#endif
	const char *primName = "generic";

	primitives_YUV_benchmark *yuvBench = primitives_YUV_benchmark_init();
	if (!yuvBench)
		return FALSE;

	if (!primitives_YUV_benchmark_run(yuvBench, genericPrims, benchDuration, &genericCount))
	{
		WLog_ERR(TAG, "error running generic YUV bench");
		goto out;
	}

	if (!primitives_init_optimized(&optimizedPrims))
	{
		WLog_ERR(TAG, "error initializing CPU optimized primitives");
		goto out;
	}

	if(optimizedPrims.flags & PRIM_FLAGS_HAVE_EXTCPU) /* run the test only if we really have optimizations */
	{
		if (!primitives_YUV_benchmark_run(yuvBench, &optimizedPrims, benchDuration, &optimizedCount))
		{
			WLog_ERR(TAG, "error running optimized YUV bench");
			goto out;
		}
	}

#if defined(WITH_OPENCL)
	if (primitives_init_opencl(&openclPrims))
	{
		if (!primitives_YUV_benchmark_run(yuvBench, &openclPrims, benchDuration, &openclCount))
		{
			WLog_ERR(TAG, "error running opencl YUV bench");
			goto out;
		}
	}
#endif

	/* finally compute the results */
	bestCount = genericCount;
	*prims = *genericPrims;

	if (bestCount < optimizedCount)
	{
		bestCount = optimizedCount;
		*prims = optimizedPrims;
		primName = "optimized";
	}

#if defined(WITH_OPENCL)
	if (bestCount < openclCount)
	{
		bestCount = openclCount;
		*prims = openclPrims;
		primName = "openCL";
	}
#endif

	WLog_DBG(TAG, "benchmark result: generic=%d optimized=%d openCL=%d", genericCount, optimizedCount, openclCount);
	WLog_INFO(TAG, "primitives autodetect, using %s", primName);
	ret = TRUE;
out:
	primitives_YUV_benchmark_free(&yuvBench);
	return ret;
}

static BOOL CALLBACK primitives_init_cb(PINIT_ONCE once, PVOID param, PVOID* context)
{
	WINPR_UNUSED(once);
	WINPR_UNUSED(param);
	WINPR_UNUSED(context);

	return primitives_init(&pPrimitives, primitivesHints);
}


#if defined(WITH_OPENCL)
static primitives_opencl_context openclContext;

primitives_opencl_context *primitives_get_opencl_context(void)
{
	return &openclContext;
}

pstatus_t primitives_uninit_opencl(void)
{
	if (!openclContext.support)
		return PRIMITIVES_SUCCESS;

	clReleaseProgram(openclContext.program);
	clReleaseCommandQueue(openclContext.commandQueue);
	clReleaseContext(openclContext.context);
	clReleaseDevice(openclContext.deviceId);
	openclContext.support = FALSE;
	return PRIMITIVES_SUCCESS;
}

BOOL primitives_init_opencl_context(primitives_opencl_context *cl)
{
	cl_platform_id *platform_ids = NULL;
	cl_uint ndevices, nplatforms, i;
	cl_kernel kernel;
	cl_int ret;
	char sourcePath[1000];
	primitives_t optimized;

	BOOL gotGPU = FALSE;
	FILE *f;
	size_t programLen;
	char *programSource;

	if (!primitives_init_optimized(&optimized))
		return FALSE;
	cl->YUV420ToRGB_backup = optimized.YUV420ToRGB_8u_P3AC4R;

	ret = clGetPlatformIDs(0, NULL, &nplatforms);
	if (ret != CL_SUCCESS || nplatforms < 1)
		return FALSE;

	platform_ids = calloc(nplatforms, sizeof(*platform_ids));
	if (!platform_ids)
		return FALSE;

	ret = clGetPlatformIDs(nplatforms, platform_ids, &nplatforms);
	if (ret != CL_SUCCESS)
	{
		free(platform_ids);
		return FALSE;
	}

	for (i = 0; (i < nplatforms) && !gotGPU; i++)
	{
		cl_device_id device_id;
		cl_context context;
		char platformName[1000];
		char deviceName[1000];

		ret = clGetPlatformInfo(platform_ids[i], CL_PLATFORM_NAME, sizeof(platformName), platformName, NULL);
		if (ret != CL_SUCCESS)
			continue;

		ret = clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_GPU, 1, &device_id, &ndevices);
		if (ret != CL_SUCCESS)
			continue;

		ret = clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "openCL: unable get device name for platform %s", platformName);
			clReleaseDevice(device_id);
			continue;
		}

		context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "openCL: unable to create context for platform %s, device %s", platformName, deviceName);
			clReleaseDevice(device_id);
			continue;
		}

		cl->commandQueue = clCreateCommandQueue(context, device_id, 0, &ret);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "openCL: unable to create command queue");
			clReleaseContext(context);
			clReleaseDevice(device_id);
			continue;
		}

		WLog_INFO(TAG, "openCL: using platform=%s device=%s", platformName, deviceName);

		cl->platformId = platform_ids[i];
		cl->deviceId = device_id;
		cl->context = context;
		gotGPU = TRUE;
	}

	free(platform_ids);

	if (!gotGPU)
	{
		WLog_ERR(TAG, "openCL: no GPU found");
		return FALSE;
	}

	snprintf(sourcePath, sizeof(sourcePath), "%s/primitives.cl", OPENCL_SOURCE_PATH);

	f = fopen(sourcePath, "r");
	if (!f)
	{
		WLog_ERR(TAG, "openCL: unable to open source file %s", sourcePath);
		goto error_source_file;
	}

	fseek(f, 0, SEEK_END);
	programLen = ftell(f);
	fseek(f, 0, SEEK_SET);

	programSource = malloc(programLen);
	if (!programSource) {
		WLog_ERR(TAG, "openCL: unable to allocate memory(%d bytes) for source file %s",
				programLen, sourcePath);
		fclose(f);
		goto error_source_file;
	}

	if (fread(programSource, programLen, 1, f) <= 0)
	{
		WLog_ERR(TAG, "openCL: unable to read openCL program in %s", sourcePath);
		free(programSource);
		fclose(f);
		goto error_source_file;
	}
	fclose(f);

	cl->program = clCreateProgramWithSource(cl->context, 1, (const char **)&programSource,
			&programLen, &ret);
	if (ret != CL_SUCCESS) {
		WLog_ERR(TAG, "openCL: unable to create command queue");
		goto out_program_create;
	}
	free(programSource);

	ret = clBuildProgram(cl->program, 1, &cl->deviceId, NULL, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		size_t length;
		char buffer[2048];
		ret = clGetProgramBuildInfo(cl->program, cl->deviceId, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &length);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "openCL: building program failed but unable to retrieve buildLog, error=%d", ret);
		}
		else
		{
			WLog_ERR(TAG, "openCL: unable to build program, errorLog=%s", buffer);
		}
		goto out_program_build;
	}

	kernel = clCreateKernel(cl->program, "yuv420_to_bgra_1b", &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create yuv420_to_bgra_1b kernel");
		goto out_program_build;
	}
	clReleaseKernel(kernel);

	cl->support = TRUE;
	return TRUE;

out_program_build:
	clReleaseProgram(cl->program);
error_source_file:
out_program_create:
	clReleaseCommandQueue(cl->commandQueue);
	clReleaseContext(cl->context);
	clReleaseDevice(cl->deviceId);
	return FALSE;
}

BOOL primitives_init_opencl(primitives_t* prims)
{
	if (!primitives_init_opencl_context(&openclContext))
		return FALSE;

	primitives_init_optimized(prims);
	primitives_init_YUV_opencl(prims);
	prims->flags |= PRIM_FLAGS_HAVE_EXTGPU;
	prims->uninit = primitives_uninit_opencl;
	return TRUE;
}

#endif

BOOL primitives_init(primitives_t *p, primitive_hints hints)
{
	switch(hints)
	{
	case PRIMITIVES_PURE_SOFT:
		return primitives_init_generic(p);
	case PRIMITIVES_ONLY_CPU:
		return primitives_init_optimized(p);
	case PRIMITIVES_AUTODETECT:
		return primitives_autodetect_best(p);
	default:
		WLog_ERR(TAG, "unknown hint %d", hints);
		return FALSE;
	}
}

void primitives_uninit() {
	if (pPrimitives.uninit)
		pPrimitives.uninit();
}

/* ------------------------------------------------------------------------- */
primitives_t* primitives_get(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic_cb, NULL, NULL);
#if defined(HAVE_OPTIMIZED_PRIMITIVES)
	InitOnceExecuteOnce(&primitives_InitOnce, primitives_init_cb, NULL, NULL);
	return &pPrimitives;
#else
	return &pPrimitivesGeneric;
#endif
}

primitives_t* primitives_get_generic(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic_cb, NULL, NULL);
	return &pPrimitivesGeneric;
}

DWORD primitives_flags(primitives_t *p)
{
	return p->flags;
}
