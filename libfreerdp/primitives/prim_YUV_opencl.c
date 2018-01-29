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


#define TAG FREERDP_TAG("primitives")



static pstatus_t opencl_YUV420ToRGB(const char *kernelName, const BYTE* pSrc[3], const UINT32 srcStep[3],
	BYTE* pDst, UINT32 dstStep, const prim_size_t* roi)
{
	cl_int ret;
	int i;
	cl_mem objs[3] = {NULL, NULL, NULL};
	cl_mem destObj;
	cl_kernel kernel;
	size_t indexes[2];
	const char *sourceNames[] = {"Y", "U", "V"};
	primitives_opencl_context *cl = primitives_get_opencl_context();

	kernel = clCreateKernel(cl->program, kernelName, &ret);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "openCL: unable to create kernel %s", kernelName);
		return -1;
	}

	for (i = 0; i < 3; i++)
	{
		objs[i] = clCreateBuffer(cl->context, CL_MEM_READ_ONLY, srcStep[i] * roi->height, NULL, &ret);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to create %sobj", sourceNames[i]);
			goto error_objs;
		}

		ret = clEnqueueWriteBuffer(cl->commandQueue, objs[i], CL_TRUE, 0, srcStep[i] * roi->height,
				pSrc[i], 0, NULL, NULL);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to enqueue write command for %sobj", sourceNames[i]);
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
		ret = clSetKernelArg(kernel, i * 2, sizeof(cl_mem), (void *)&objs[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg for %sobj", sourceNames[i]);
			goto error_set_args;
		}

		ret = clSetKernelArg(kernel, i * 2 + 1, sizeof(cl_int), (void *)&srcStep[i]);
		if (ret != CL_SUCCESS)
		{
			WLog_ERR(TAG, "unable to set arg stride for %sobj", sourceNames[i]);
			goto error_set_args;
		}
	}

	ret = clSetKernelArg(kernel, 6, sizeof(cl_mem), (void *)&destObj);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg destObj");
		goto error_set_args;
	}

	ret = clSetKernelArg(kernel, 7, sizeof(cl_int), (void *)&dstStep);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to set arg dstStep");
		goto error_set_args;
	}

	indexes[0] = roi->width;
	indexes[1] = roi->height;
	ret = clEnqueueNDRangeKernel(cl->commandQueue, kernel, 2, NULL, indexes, NULL,
			0, NULL, NULL);
	if (ret != CL_SUCCESS)
	{
		WLog_ERR(TAG, "unable to enqueue call kernel");
		goto error_set_args;
	}

	/* Transfer result to host */
	ret = clEnqueueReadBuffer(cl->commandQueue, destObj, CL_TRUE, 0, roi->height * dstStep, pDst, 0, NULL, NULL);
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


static pstatus_t opencl_YUV420ToRGB_8u_P3AC4R(const BYTE* pSrc[3], const UINT32 srcStep[3],
    BYTE* pDst, UINT32 dstStep, UINT32 DstFormat, const prim_size_t* roi)
{
	const char *kernel_name;

	switch(DstFormat)
	{
	case PIXEL_FORMAT_BGRA32:
	case PIXEL_FORMAT_BGRX32:
		kernel_name = "yuv420_to_bgra_1b";
		break;
	case PIXEL_FORMAT_XRGB32:
	case PIXEL_FORMAT_ARGB32:
		kernel_name = "yuv420_to_argb_1b";
		break;
	default: {
		primitives_opencl_context *cl = primitives_get_opencl_context();
		return cl->YUV420ToRGB_backup(pSrc, srcStep, pDst, dstStep, DstFormat, roi);
	}
	}

	return opencl_YUV420ToRGB(kernel_name, pSrc, srcStep, pDst, dstStep, roi);
}

void primitives_init_YUV_opencl(primitives_t* prims)
{
	prims->YUV420ToRGB_8u_P3AC4R = opencl_YUV420ToRGB_8u_P3AC4R;

}


