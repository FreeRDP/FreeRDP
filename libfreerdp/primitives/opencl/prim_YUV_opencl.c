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

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>
#include "prim_internal.h"

#if defined(WITH_OPENCL)
#ifdef __APPLE__
#include "OpenCL/opencl.h"
#else
#include <CL/cl.h>
#endif
#include "primitives-opencl-program.h"

#include <freerdp/log.h>
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

typedef struct
{
	primitives_opencl_context* cl;
	cl_kernel kernel;
	cl_mem srcObjs[3];
	cl_mem dstObj;
	prim_size_t roi;
	size_t dstStep;
} primitives_cl_kernel;

static primitives_opencl_context* primitives_get_opencl_context(void);

static void cl_kernel_free(primitives_cl_kernel* kernel)
{
	if (!kernel)
		return;

	if (kernel->dstObj)
		clReleaseMemObject(kernel->dstObj);

	for (size_t i = 0; i < ARRAYSIZE(kernel->srcObjs); i++)
	{
		cl_mem obj = kernel->srcObjs[i];
		kernel->srcObjs[i] = NULL;
		if (obj)
			clReleaseMemObject(obj);
	}

	if (kernel->kernel)
		clReleaseKernel(kernel->kernel);

	free(kernel);
}

static primitives_cl_kernel* cl_kernel_new(const char* kernelName, const prim_size_t* roi)
{
	WINPR_ASSERT(kernelName);
	WINPR_ASSERT(roi);

	primitives_cl_kernel* kernel = calloc(1, sizeof(primitives_cl_kernel));
	if (!kernel)
		goto fail;

	kernel->roi = *roi;
	kernel->cl = primitives_get_opencl_context();
	if (!kernel->cl)
		goto fail;

	cl_int ret = CL_INVALID_VALUE;
	kernel->kernel = clCreateKernel(kernel->cl->program, kernelName, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create kernel %s", kernelName);
		goto fail;
	}

	return kernel;
fail:
	cl_kernel_free(kernel);
	return NULL;
}

static BOOL cl_kernel_set_sources(primitives_cl_kernel* ctx, const BYTE* WINPR_RESTRICT pSrc[3],
                                  const UINT32 srcStep[3])
{
	const char* sourceNames[] = { "Y", "U", "V" };

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(pSrc);
	WINPR_ASSERT(srcStep);

	for (cl_uint i = 0; i < ARRAYSIZE(ctx->srcObjs); i++)
	{
		cl_int ret = CL_INVALID_VALUE;
		const BYTE* csrc = pSrc[i];
		void* WINPR_RESTRICT src = WINPR_CAST_CONST_PTR_AWAY(csrc, void* WINPR_RESTRICT);
		ctx->srcObjs[i] = clCreateBuffer(ctx->cl->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
		                                 1ull * srcStep[i] * ctx->roi.height, src, &ret);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to create %sobj", sourceNames[i]);
			return FALSE;
		}

		ret = clSetKernelArg(ctx->kernel, i * 2, sizeof(cl_mem), &ctx->srcObjs[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg for %sobj", sourceNames[i]);
			return FALSE;
		}

		ret = clSetKernelArg(ctx->kernel, i * 2 + 1, sizeof(cl_uint), &srcStep[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg stride for %sobj", sourceNames[i]);
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL cl_kernel_set_destination(primitives_cl_kernel* ctx, UINT32 dstStep)
{

	WINPR_ASSERT(ctx);

	ctx->dstStep = dstStep;
	cl_int ret = CL_INVALID_VALUE;
	ctx->dstObj = clCreateBuffer(ctx->cl->context, CL_MEM_WRITE_ONLY,
	                             1ull * dstStep * ctx->roi.height, NULL, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to create dest obj");
		return FALSE;
	}

	ret = clSetKernelArg(ctx->kernel, 6, sizeof(cl_mem), &ctx->dstObj);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg destObj");
		return FALSE;
	}

	ret = clSetKernelArg(ctx->kernel, 7, sizeof(cl_uint), &dstStep);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg dstStep");
		return FALSE;
	}

	return TRUE;
}

static BOOL cl_kernel_process(primitives_cl_kernel* ctx, BYTE* pDst)
{
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(pDst);

	size_t indexes[2] = { 0 };
	indexes[0] = ctx->roi.width;
	indexes[1] = ctx->roi.height;

	cl_int ret = clEnqueueNDRangeKernel(ctx->cl->commandQueue, ctx->kernel, 2, NULL, indexes, NULL,
	                                    0, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to enqueue call kernel");
		return FALSE;
	}

	/* Transfer result to host */
	ret = clEnqueueReadBuffer(ctx->cl->commandQueue, ctx->dstObj, CL_TRUE, 0,
	                          ctx->roi.height * ctx->dstStep, pDst, 0, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to read back buffer");
		return FALSE;
	}

	return TRUE;
}

static pstatus_t opencl_YUVToRGB(const char* kernelName, const BYTE* WINPR_RESTRICT pSrc[3],
                                 const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
                                 const prim_size_t* WINPR_RESTRICT roi)
{
	pstatus_t res = -1;

	primitives_cl_kernel* ctx = cl_kernel_new(kernelName, roi);
	if (!ctx)
		goto fail;

	if (!cl_kernel_set_sources(ctx, pSrc, srcStep))
		goto fail;

	if (!cl_kernel_set_destination(ctx, dstStep))
		goto fail;

	if (!cl_kernel_process(ctx, pDst))
		goto fail;

	res = PRIMITIVES_SUCCESS;

fail:
	cl_kernel_free(ctx);
	return res;
}

static primitives_opencl_context openclContext = { 0 };

static primitives_opencl_context* primitives_get_opencl_context(void)
{
	return &openclContext;
}

static void cl_context_free(primitives_opencl_context* ctx)
{
	if (!ctx)
		return;
	clReleaseProgram(ctx->program);
	clReleaseCommandQueue(ctx->commandQueue);
	clReleaseContext(ctx->context);
	clReleaseDevice(ctx->deviceId);
	ctx->support = FALSE;
}

static pstatus_t primitives_uninit_opencl(void)
{
	if (!openclContext.support)
		return PRIMITIVES_SUCCESS;

	cl_context_free(&openclContext);
	return PRIMITIVES_SUCCESS;
}

static BOOL primitives_init_opencl_context(primitives_opencl_context* WINPR_RESTRICT prims)
{
	cl_uint ndevices = 0;
	cl_uint nplatforms = 0;
	cl_kernel kernel = NULL;

	BOOL gotGPU = FALSE;
	size_t programLen = 0;

	cl_int ret = clGetPlatformIDs(0, NULL, &nplatforms);
	if (ret != CL_SUCCESS || nplatforms < 1)
		return FALSE;

	cl_platform_id* platform_ids = (cl_platform_id*)calloc(nplatforms, sizeof(cl_platform_id));
	if (!platform_ids)
		return FALSE;

	ret = clGetPlatformIDs(nplatforms, platform_ids, &nplatforms);
	if (ret != CL_SUCCESS)
	{
		free((void*)platform_ids);
		return FALSE;
	}

	for (cl_uint i = 0; (i < nplatforms) && !gotGPU; i++)
	{
		cl_device_id device_id = NULL;
		cl_context context = NULL;
		char platformName[1000] = { 0 };
		char deviceName[1000] = { 0 };

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

#if defined(CL_VERSION_2_0)
		prims->commandQueue = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret);
#else
		prims->commandQueue = clCreateCommandQueue(context, device_id, 0, &ret);
#endif
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "openCL: unable to create command queue");
			clReleaseContext(context);
			clReleaseDevice(device_id);
			continue;
		}

		WLog_INFO(TAG, "openCL: using platform=%s device=%s", platformName, deviceName);

		prims->platformId = platform_ids[i];
		prims->deviceId = device_id;
		prims->context = context;
		gotGPU = TRUE;
	}

	free((void*)platform_ids);

	if (!gotGPU)
	{
		WLog_ERR(TAG, "openCL: no GPU found");
		return FALSE;
	}

	programLen = strnlen(openclProgram, sizeof(openclProgram));
	const char* ptr = openclProgram;
	prims->program = clCreateProgramWithSource(prims->context, 1, &ptr, &programLen, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create program");
		goto fail;
	}

	ret = clBuildProgram(prims->program, 1, &prims->deviceId, NULL, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		size_t length = 0;
		char buffer[2048];
		ret = clGetProgramBuildInfo(prims->program, prims->deviceId, CL_PROGRAM_BUILD_LOG,
		                            sizeof(buffer), buffer, &length);
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
		goto fail;
	}

	kernel = clCreateKernel(prims->program, "yuv420_to_bgra_1b", &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create yuv420_to_bgra_1b kernel");
		goto fail;
	}
	clReleaseKernel(kernel);

	prims->support = TRUE;
	return TRUE;

fail:
	cl_context_free(prims);
	return FALSE;
}

static pstatus_t opencl_YUV420ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                              const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                              UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* WINPR_RESTRICT roi)
{
	const char* kernel_name = NULL;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_ABGR32:
			kernel_name = "yuv420_to_abgr_1b";
			break;
		case PIXEL_FORMAT_XBGR32:
			kernel_name = "yuv420_to_xbgr_1b";
			break;
		case PIXEL_FORMAT_RGBX32:
			kernel_name = "yuv420_to_rgba_1b";
			break;
		case PIXEL_FORMAT_RGBA32:
			kernel_name = "yuv420_to_rgbx_1b";
			break;
		case PIXEL_FORMAT_BGRA32:
			kernel_name = "yuv420_to_bgra_1b";
			break;
		case PIXEL_FORMAT_BGRX32:
			kernel_name = "yuv420_to_bgrx_1b";
			break;
		case PIXEL_FORMAT_XRGB32:
			kernel_name = "yuv420_to_xrgb_1b";
			break;
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

static pstatus_t opencl_YUV444ToRGB_8u_P3AC4R(const BYTE* WINPR_RESTRICT pSrc[3],
                                              const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
                                              UINT32 dstStep, UINT32 DstFormat,
                                              const prim_size_t* WINPR_RESTRICT roi)
{
	const char* kernel_name = NULL;

	switch (DstFormat)
	{
		case PIXEL_FORMAT_ABGR32:
			kernel_name = "yuv444_to_abgr_1b";
			break;
		case PIXEL_FORMAT_XBGR32:
			kernel_name = "yuv444_to_xbgr_1b";
			break;
		case PIXEL_FORMAT_RGBX32:
			kernel_name = "yuv444_to_rgba_1b";
			break;
		case PIXEL_FORMAT_RGBA32:
			kernel_name = "yuv444_to_rgbx_1b";
			break;
		case PIXEL_FORMAT_BGRA32:
			kernel_name = "yuv444_to_bgra_1b";
			break;
		case PIXEL_FORMAT_BGRX32:
			kernel_name = "yuv444_to_bgrx_1b";
			break;
		case PIXEL_FORMAT_XRGB32:
			kernel_name = "yuv444_to_xrgb_1b";
			break;
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
#endif
