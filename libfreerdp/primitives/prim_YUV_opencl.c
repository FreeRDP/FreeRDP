/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Optimized YUV/RGB conversion operations using openCL
 *
 * Copyright 2019 David Fort <contact@hardening-consulting.com>
 * Copyright 2019 Rangee Gmbh
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

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include "prim_internal.h"

#if defined(WITH_OPENCL)
#ifdef __APPLE__
#include "OpenCL/opencl.h"
#else
#include <CL/cl.h>
#endif
#endif

#define TAG FREERDP_TAG("primitives")

typedef struct
{
	BOOL support;
	cl_platform_id platformId;
	cl_device_id deviceId;
	cl_context context;
	cl_command_queue commandQueue;
	cl_program program;
} primitives_opencl_context;

static primitives_opencl_context* primitives_get_opencl_context(void);

static pstatus_t opencl_YUVToRGB(const char* kernelName, const BYTE* const pSrc[3],
                                 const UINT32 srcStep[3], BYTE* pDst, UINT32 dstStep,
                                 const prim_size_t* roi)
{
	cl_int ret;
	cl_uint i;
	cl_mem objs[3] = { NULL, NULL, NULL };
	cl_mem destObj;
	cl_kernel kernel;
	size_t indexes[2];
	const char* sourceNames[] = { "Y", "U", "V" };
	primitives_opencl_context* cl = primitives_get_opencl_context();

	kernel = clCreateKernel(cl->program, kernelName, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create kernel %s", kernelName);
		return -1;
	}

	for (i = 0; i < 3; i++)
	{
		objs[i] = clCreateBuffer(cl->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
		                         srcStep[i] * roi->height, (char*)pSrc[i], &ret);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to create %sobj", sourceNames[i]);
			goto error_objs;
		}
	}

	destObj = clCreateBuffer(cl->context, CL_MEM_WRITE_ONLY, dstStep * roi->height, NULL, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to create dest obj");
		goto error_objs;
	}

	/* push source + stride arguments*/
	for (i = 0; i < 3; i++)
	{
		ret = clSetKernelArg(kernel, i * 2, sizeof(cl_mem), &objs[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg for %sobj", sourceNames[i]);
			goto error_set_args;
		}

		ret = clSetKernelArg(kernel, i * 2 + 1, sizeof(cl_int), &srcStep[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg stride for %sobj", sourceNames[i]);
			goto error_set_args;
		}
	}

	ret = clSetKernelArg(kernel, 6, sizeof(cl_mem), &destObj);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg destObj");
		goto error_set_args;
	}

	ret = clSetKernelArg(kernel, 7, sizeof(cl_int), &dstStep);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg dstStep");
		goto error_set_args;
	}

	indexes[0] = roi->width;
	indexes[1] = roi->height;
	ret = clEnqueueNDRangeKernel(cl->commandQueue, kernel, 2, NULL, indexes, NULL, 0, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to enqueue call kernel");
		goto error_set_args;
	}

	/* Transfer result to host */
	ret = clEnqueueReadBuffer(cl->commandQueue, destObj, CL_TRUE, 0, roi->height * dstStep, pDst, 0,
	                          NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to read back buffer");
		goto error_set_args;
	}

	/* cleanup things */
	clReleaseMemObject(destObj);
	for (i = 0; i < 3; i++)
		if (objs[i])
			clReleaseMemObject(objs[i]);
	clReleaseKernel(kernel);

	return PRIMITIVES_SUCCESS;

error_set_args:
	clReleaseMemObject(destObj);
error_objs:
	for (i = 0; i < 3; i++)
	{
		if (objs[i])
			clReleaseMemObject(objs[i]);
	}
	clReleaseKernel(kernel);
	return -1;
}

static primitives_opencl_context openclContext;

static primitives_opencl_context* primitives_get_opencl_context(void)
{
	return &openclContext;
}

static pstatus_t primitives_uninit_opencl(void)
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

static const char* openclProgram =
#include "primitives.cl"
    ;

static BOOL primitives_init_opencl_context(primitives_opencl_context* cl)
{
	cl_platform_id* platform_ids = NULL;
	cl_uint ndevices, nplatforms, i;
	cl_kernel kernel;
	cl_int ret;

	BOOL gotGPU = FALSE;
	size_t programLen;

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

		ret = clGetPlatformInfo(platform_ids[i], CL_PLATFORM_NAME, sizeof(platformName),
		                        platformName, NULL);
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
			WLog_ERR(TAG, "openCL: unable to create context for platform %s, device %s",
			         platformName, deviceName);
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

	programLen = strlen(openclProgram);
	cl->program =
	    clCreateProgramWithSource(cl->context, 1, (const char**)&openclProgram, &programLen, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create program");
		goto out_program_create;
	}

	ret = clBuildProgram(cl->program, 1, &cl->deviceId, NULL, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		size_t length;
		char buffer[2048];
		ret = clGetProgramBuildInfo(cl->program, cl->deviceId, CL_PROGRAM_BUILD_LOG, sizeof(buffer),
		                            buffer, &length);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG,
			         "openCL: building program failed but unable to retrieve buildLog, error=%d",
			         ret);
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
out_program_create:
	clReleaseCommandQueue(cl->commandQueue);
	clReleaseContext(cl->context);
	clReleaseDevice(cl->deviceId);
	return FALSE;
}

static pstatus_t opencl_YUV420ToRGB_8u_P3AC4R(const BYTE* const pSrc[3], const UINT32 srcStep[3],
                                              BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* roi)
{
	const char* kernel_name;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			kernel_name = "yuv420_to_bgra_1b";
			break;
		case PIXEL_FORMAT_XRGB32:
		case PIXEL_FORMAT_ARGB32:
			kernel_name = "yuv420_to_argb_1b";
			break;
		default:
		{
			primitives_t* p = primitives_get_by_type(PRIMITIVES_ONLY_CPU);
			if (!p)
				return -1;
			return p->YUV420ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
		}
	}

	return opencl_YUVToRGB(kernel_name, pSrc, srcStep, pDst, dstStep, roi);
}

static pstatus_t opencl_YUV444ToRGB_8u_P3AC4R(const BYTE* const pSrc[3], const UINT32 srcStep[3],
                                              BYTE* pDst, UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* roi)
{
	const char* kernel_name;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			kernel_name = "yuv444_to_bgra_1b";
			break;
		case PIXEL_FORMAT_XRGB32:
		case PIXEL_FORMAT_ARGB32:
			kernel_name = "yuv444_to_argb_1b";
			break;
		default:
		{
			primitives_t* p = primitives_get_by_type(PRIMITIVES_ONLY_CPU);
			if (!p)
				return -1;
			return p->YUV444ToRGB_8u_P3AC4R(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
		}
	}

	return opencl_YUVToRGB(kernel_name, pSrc, srcStep, pDst, dstStep, roi);
}

BOOL primitives_init_opencl(primitives_t* prims)
{
	primitives_t* p = primitives_get_by_type(PRIMITIVES_ONLY_CPU);
	if (!prims || !p)
		return FALSE;
	*prims = *p;

	if (!primitives_init_opencl_context(&openclContext))
		return FALSE;

	prims->YUV420ToRGB_8u_P3AC4R = opencl_YUV420ToRGB_8u_P3AC4R;
	prims->YUV444ToRGB_8u_P3AC4R = opencl_YUV444ToRGB_8u_P3AC4R;
	prims->flags |= PRIM_FLAGS_HAVE_EXTGPU;
	prims->uninit = primitives_uninit_opencl;
	return TRUE;
}
