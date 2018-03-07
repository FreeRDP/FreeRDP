#include <winpr/sysinfo.h>
#include <winpr/pool.h>

#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/yuv.h>

#define TAG FREERDP_TAG("codec")

struct _YUV_CONTEXT
{
	UINT32 width, height;
	BOOL useThreads;
	UINT32 nthreads;
	UINT32 heightStep;

	PTP_POOL threadPool;
	TP_CALLBACK_ENVIRON ThreadPoolEnv;
};


struct _YUV_PROCESS_WORK_PARAM
{
	YUV_CONTEXT* context;
	const BYTE* pYUVData[3];
	UINT32 iStride[3];
	DWORD DstFormat;
	BYTE *dest;
	UINT32 nDstStep;
	UINT32 y;
	UINT32 height;
};
typedef struct _YUV_PROCESS_WORK_PARAM YUV_PROCESS_WORK_PARAM;

static void CALLBACK yuv_process_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
		PTP_WORK work)
{
	prim_size_t roi;
	YUV_PROCESS_WORK_PARAM* param = (YUV_PROCESS_WORK_PARAM*)context;
	primitives_t* prims = primitives_get();

	roi.width = param->context->width;
	roi.height = param->height;
	if( prims->YUV420ToRGB_8u_P3AC4R(param->pYUVData, param->iStride, param->dest, param->nDstStep,
				        param->DstFormat, &roi) != PRIMITIVES_SUCCESS)
	{
		WLog_ERR(TAG, "error when decoding lines");
	}
}


void yuv_context_reset(YUV_CONTEXT* context, UINT32 width, UINT32 height)
{
	context->width = width;
	context->height = height;
	context->heightStep = (height / context->nthreads);
}


YUV_CONTEXT* yuv_context_new(BOOL encoder)
{
	SYSTEM_INFO sysInfos;
	YUV_CONTEXT* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	/** do it here to avoid a race condition between threads */
	primitives_get();

	GetNativeSystemInfo(&sysInfos);
	ret->useThreads = (sysInfos.dwNumberOfProcessors > 1);
	if (ret->useThreads)
	{
		ret->nthreads = sysInfos.dwNumberOfProcessors;
		ret->threadPool = CreateThreadpool(NULL);
		if (!ret->threadPool)
		{
			goto error_threadpool;
		}

		InitializeThreadpoolEnvironment(&ret->ThreadPoolEnv);
		SetThreadpoolCallbackPool(&ret->ThreadPoolEnv, ret->threadPool);
	}
	else
	{
		ret->nthreads = 1;
	}

	return ret;

error_threadpool:
	free(ret);
	return NULL;
}


void yuv_context_free(YUV_CONTEXT* context)
{
	if (context->useThreads)
	{
		CloseThreadpool(context->threadPool);
		DestroyThreadpoolEnvironment(&context->ThreadPoolEnv);
	}
	free(context);
}


BOOL yuv_context_decode(YUV_CONTEXT* context, const BYTE* pYUVData[3], UINT32 iStride[3],
						DWORD DstFormat, BYTE *dest, UINT32 nDstStep)
{
	UINT32 y, nobjects, i;
	PTP_WORK *work_objects = NULL;
	YUV_PROCESS_WORK_PARAM *params;
	int waitCount = 0;
	BOOL ret = TRUE;

	if (!context->useThreads)
	{
		primitives_t* prims = primitives_get();
		prim_size_t roi;
		roi.width = context->width;
		roi.height = context->height;
		return prims->YUV420ToRGB_8u_P3AC4R(pYUVData, iStride, dest, nDstStep,
					        DstFormat, &roi) == PRIMITIVES_SUCCESS;
	}

	/* case where we use threads */
	nobjects = (context->height + context->heightStep - 1) / context->heightStep;
	work_objects = (PTP_WORK *)calloc(nobjects, sizeof(PTP_WORK));
	if (!work_objects)
	{
		return FALSE;
	}

	params = (YUV_PROCESS_WORK_PARAM *)calloc(nobjects, sizeof(*params));
	if (!params)
	{
		free(work_objects);
		return FALSE;
	}

	for (i = 0, y = 0; i < nobjects; i++, y += context->heightStep, waitCount++)
	{
		params[i].context = context;
		params[i].DstFormat = DstFormat;
		params[i].pYUVData[0] = pYUVData[0] + (y * iStride[0]);
		params[i].pYUVData[1] = pYUVData[1] + ((y / 2) * iStride[1]);
		params[i].pYUVData[2] = pYUVData[2] + ((y / 2) * iStride[2]);

		params[i].iStride[0] = iStride[0];
		params[i].iStride[1] = iStride[1];
		params[i].iStride[2] = iStride[2];

		params[i].nDstStep = nDstStep;
		params[i].dest = dest + (nDstStep * y);
		params[i].y = y;
		if (y + context->heightStep <= context->height)
			params[i].height = context->heightStep;
		else
			params[i].height = context->height % context->heightStep;

		work_objects[i] = CreateThreadpoolWork(yuv_process_work_callback,
					                        (void*) &params[i], &context->ThreadPoolEnv);
		if (!work_objects[i])
		{
			ret = FALSE;
			break;
		}
		SubmitThreadpoolWork(work_objects[i]);
	}

	for (i = 0; i < waitCount; i++)
	{
		WaitForThreadpoolWorkCallbacks(work_objects[i], FALSE);
		CloseThreadpoolWork(work_objects[i]);
	}

	free(work_objects);
	free(params);

	return ret;
}
