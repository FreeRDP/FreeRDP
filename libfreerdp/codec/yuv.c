#include <winpr/sysinfo.h>
#include <winpr/assert.h>
#include <winpr/pool.h>

#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/yuv.h>

#define TAG FREERDP_TAG("codec")

#define TILE_SIZE 64

typedef struct
{
	YUV_CONTEXT* context;
	const BYTE* pYUVData[3];
	UINT32 iStride[3];
	DWORD DstFormat;
	BYTE* dest;
	UINT32 nDstStep;
	RECTANGLE_16 rect;
} YUV_PROCESS_WORK_PARAM;

typedef struct
{
	YUV_CONTEXT* context;
	const BYTE* pYUVData[3];
	UINT32 iStride[3];
	BYTE* pYUVDstData[3];
	UINT32 iDstStride[3];
	RECTANGLE_16 rect;
	BYTE type;
} YUV_COMBINE_WORK_PARAM;

typedef struct
{
	YUV_CONTEXT* context;
	const BYTE* pSrcData;

	DWORD SrcFormat;
	UINT32 nSrcStep;
	RECTANGLE_16 rect;
	BYTE version;

	BYTE* pYUVLumaData[3];
	BYTE* pYUVChromaData[3];
	UINT32 iStride[3];
} YUV_ENCODE_WORK_PARAM;

struct S_YUV_CONTEXT
{
	UINT32 width, height;
	BOOL useThreads;
	BOOL encoder;
	UINT32 nthreads;
	UINT32 heightStep;

	PTP_POOL threadPool;
	TP_CALLBACK_ENVIRON ThreadPoolEnv;

	UINT32 work_object_count;
	PTP_WORK* work_objects;
	YUV_ENCODE_WORK_PARAM* work_enc_params;
	YUV_PROCESS_WORK_PARAM* work_dec_params;
	YUV_COMBINE_WORK_PARAM* work_combined_params;
};

static INLINE BOOL avc420_yuv_to_rgb(const BYTE* pYUVData[3], const UINT32 iStride[3],
                                     const RECTANGLE_16* rect, UINT32 nDstStep, BYTE* pDstData,
                                     DWORD DstFormat)
{
	primitives_t* prims = primitives_get();
	prim_size_t roi;
	const BYTE* pYUVPoint[3];

	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(rect);
	WINPR_ASSERT(pDstData);

	const INT32 width = rect->right - rect->left;
	const INT32 height = rect->bottom - rect->top;
	BYTE* pDstPoint =
	    pDstData + rect->top * nDstStep + rect->left * FreeRDPGetBytesPerPixel(DstFormat);

	pYUVPoint[0] = pYUVData[0] + rect->top * iStride[0] + rect->left;
	pYUVPoint[1] = pYUVData[1] + rect->top / 2 * iStride[1] + rect->left / 2;
	pYUVPoint[2] = pYUVData[2] + rect->top / 2 * iStride[2] + rect->left / 2;

	roi.width = width;
	roi.height = height;

	if (prims->YUV420ToRGB_8u_P3AC4R(pYUVPoint, iStride, pDstPoint, nDstStep, DstFormat, &roi) !=
	    PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static INLINE BOOL avc444_yuv_to_rgb(const BYTE* pYUVData[3], const UINT32 iStride[3],
                                     const RECTANGLE_16* rect, UINT32 nDstStep, BYTE* pDstData,
                                     DWORD DstFormat)
{
	primitives_t* prims = primitives_get();
	prim_size_t roi;
	const BYTE* pYUVPoint[3];

	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(rect);
	WINPR_ASSERT(pDstData);

	const INT32 width = rect->right - rect->left;
	const INT32 height = rect->bottom - rect->top;
	BYTE* pDstPoint =
	    pDstData + rect->top * nDstStep + rect->left * FreeRDPGetBytesPerPixel(DstFormat);

	pYUVPoint[0] = pYUVData[0] + rect->top * iStride[0] + rect->left;
	pYUVPoint[1] = pYUVData[1] + rect->top * iStride[1] + rect->left;
	pYUVPoint[2] = pYUVData[2] + rect->top * iStride[2] + rect->left;

	roi.width = width;
	roi.height = height;

	if (prims->YUV444ToRGB_8u_P3AC4R(pYUVPoint, iStride, pDstPoint, nDstStep, DstFormat, &roi) !=
	    PRIMITIVES_SUCCESS)
		return FALSE;

	return TRUE;
}

static void CALLBACK yuv420_process_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                  PTP_WORK work)
{
	YUV_PROCESS_WORK_PARAM* param = (YUV_PROCESS_WORK_PARAM*)context;
	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);
	WINPR_ASSERT(param);

	if (!avc420_yuv_to_rgb(param->pYUVData, param->iStride, &param->rect, param->nDstStep,
	                       param->dest, param->DstFormat))
		WLog_WARN(TAG, "avc420_yuv_to_rgb failed");
}

static void CALLBACK yuv444_process_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                  PTP_WORK work)
{
	YUV_PROCESS_WORK_PARAM* param = (YUV_PROCESS_WORK_PARAM*)context;
	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);
	WINPR_ASSERT(param);

	if (!avc444_yuv_to_rgb(param->pYUVData, param->iStride, &param->rect, param->nDstStep,
	                       param->dest, param->DstFormat))
		WLog_WARN(TAG, "avc444_yuv_to_rgb failed");
}

BOOL yuv_context_reset(YUV_CONTEXT* context, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(context);

	context->width = width;
	context->height = height;
	context->heightStep = (height / context->nthreads);

	if (context->useThreads)
	{
		const UINT32 pw = (width + TILE_SIZE - width % TILE_SIZE) / TILE_SIZE;
		const UINT32 ph = (height + TILE_SIZE - height % TILE_SIZE) / TILE_SIZE;

		/* We´ve calculated the amount of workers for 64x64 tiles, but the decoder
		 * might get 16x16 tiles mixed in. */
		const UINT32 count = pw * ph * 16;

		context->work_object_count = 0;
		if (context->encoder)
		{
			free(context->work_enc_params);
			context->work_enc_params = calloc(count, sizeof(YUV_ENCODE_WORK_PARAM));
			if (!context->work_enc_params)
				return FALSE;
		}
		else
		{
			free(context->work_dec_params);
			context->work_dec_params = calloc(count, sizeof(YUV_PROCESS_WORK_PARAM));
			if (!context->work_dec_params)
				return FALSE;

			free(context->work_combined_params);
			context->work_combined_params = calloc(count, sizeof(YUV_COMBINE_WORK_PARAM));
			if (!context->work_combined_params)
				return FALSE;
		}

		free(context->work_objects);
		context->work_objects = calloc(count, sizeof(PTP_WORK));
		if (!context->work_objects)
			return FALSE;

		context->work_object_count = count;
	}
	return TRUE;
}

YUV_CONTEXT* yuv_context_new(BOOL encoder, UINT32 ThreadingFlags)
{
	SYSTEM_INFO sysInfos;
	YUV_CONTEXT* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	/** do it here to avoid a race condition between threads */
	primitives_get();

	ret->encoder = encoder;
	ret->nthreads = 1;
	if (!(ThreadingFlags & THREADING_FLAGS_DISABLE_THREADS))
	{
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
	}

	return ret;

error_threadpool:
	yuv_context_free(ret);
	return NULL;
}

void yuv_context_free(YUV_CONTEXT* context)
{
	if (!context)
		return;
	if (context->useThreads)
	{
		if (context->threadPool)
			CloseThreadpool(context->threadPool);
		DestroyThreadpoolEnvironment(&context->ThreadPoolEnv);
		free(context->work_objects);
		free(context->work_combined_params);
		free(context->work_enc_params);
		free(context->work_dec_params);
	}
	free(context);
}

static INLINE YUV_PROCESS_WORK_PARAM pool_decode_param(const RECTANGLE_16* rect,
                                                       YUV_CONTEXT* context,
                                                       const BYTE* pYUVData[3],
                                                       const UINT32 iStride[3], UINT32 DstFormat,
                                                       BYTE* dest, UINT32 nDstStep)
{
	YUV_PROCESS_WORK_PARAM current = { 0 };

	WINPR_ASSERT(rect);
	WINPR_ASSERT(context);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(dest);

	current.context = context;
	current.DstFormat = DstFormat;
	current.pYUVData[0] = pYUVData[0];
	current.pYUVData[1] = pYUVData[1];
	current.pYUVData[2] = pYUVData[2];
	current.iStride[0] = iStride[0];
	current.iStride[1] = iStride[1];
	current.iStride[2] = iStride[2];
	current.nDstStep = nDstStep;
	current.dest = dest;
	current.rect = *rect;
	return current;
}

static BOOL submit_object(PTP_WORK* work_object, PTP_WORK_CALLBACK cb, const void* param,
                          YUV_CONTEXT* context)
{
	union
	{
		const void* cpv;
		void* pv;
	} cnv;

	cnv.cpv = param;

	if (!work_object || !param || !context)
		return FALSE;

	*work_object = CreateThreadpoolWork(cb, cnv.pv, &context->ThreadPoolEnv);
	if (!*work_object)
		return FALSE;

	SubmitThreadpoolWork(*work_object);
	return TRUE;
}

static void free_objects(PTP_WORK* work_objects, UINT32 waitCount)
{
	UINT32 i;

	WINPR_ASSERT(work_objects || (waitCount == 0));

	for (i = 0; i < waitCount; i++)
	{
		PTP_WORK cur = work_objects[i];
		work_objects[i] = NULL;

		if (!cur)
			continue;

		WaitForThreadpoolWorkCallbacks(cur, FALSE);
		CloseThreadpoolWork(cur);
	}
}

static BOOL intersects(UINT32 pos, const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	UINT32 x;

	WINPR_ASSERT(regionRects || (numRegionRects == 0));

	for (x = pos + 1; x < numRegionRects; x++)
	{
		const RECTANGLE_16* what = &regionRects[pos];
		const RECTANGLE_16* rect = &regionRects[x];

		if (rectangles_intersects(what, rect))
		{
			WLog_WARN(TAG, "YUV decoder: intersecting rectangles, aborting");
			return TRUE;
		}
	}

	return FALSE;
}

static BOOL pool_decode(YUV_CONTEXT* context, PTP_WORK_CALLBACK cb, const BYTE* pYUVData[3],
                        const UINT32 iStride[3], UINT32 yuvHeight, UINT32 DstFormat, BYTE* dest,
                        UINT32 nDstStep, const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	BOOL rc = FALSE;
	UINT32 x;
	UINT32 waitCount = 0;
	primitives_t* prims = primitives_get();

	WINPR_ASSERT(context);
	WINPR_ASSERT(cb);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(dest);
	WINPR_ASSERT(regionRects || (numRegionRects == 0));

	if (context->encoder)
	{
		WLog_ERR(TAG, "YUV context set up for encoding, can not decode with it, aborting");
		return FALSE;
	}

	if (!context->useThreads || (primitives_flags(prims) & PRIM_FLAGS_HAVE_EXTGPU))
	{
		for (UINT32 y = 0; y < numRegionRects; y++)
		{
			const RECTANGLE_16* rect = &regionRects[y];
			YUV_PROCESS_WORK_PARAM current =
			    pool_decode_param(rect, context, pYUVData, iStride, DstFormat, dest, nDstStep);
			cb(NULL, &current, NULL);
		}
		return TRUE;
	}

	/* case where we use threads */
	for (x = 0; x < numRegionRects; x++)
	{
		const RECTANGLE_16* rect = &regionRects[x];
		RECTANGLE_16 r = *rect;

		if (intersects(x, regionRects, numRegionRects))
			continue;

		while (r.left < r.right)
		{
			RECTANGLE_16 y = r;
			y.right = MIN(r.right, r.left + TILE_SIZE);

			while (y.top < y.bottom)
			{
				RECTANGLE_16 z = y;
				YUV_PROCESS_WORK_PARAM* cur;

				if (context->work_object_count <= waitCount)
				{
					WLog_ERR(TAG,
					         "YUV decoder: invalid number of tiles, only support %" PRIu32
					         ", got %" PRIu32,
					         context->work_object_count, waitCount);
					goto fail;
				}

				cur = &context->work_dec_params[waitCount];
				z.bottom = MIN(z.bottom, z.top + TILE_SIZE);
				if (rectangle_is_empty(&z))
					continue;
				*cur = pool_decode_param(&z, context, pYUVData, iStride, DstFormat, dest, nDstStep);
				if (!submit_object(&context->work_objects[waitCount], cb, cur, context))
					goto fail;
				waitCount++;
				y.top += TILE_SIZE;
			}

			r.left += TILE_SIZE;
		}
	}
	rc = TRUE;
fail:
	free_objects(context->work_objects, waitCount);
	return rc;
}

static INLINE BOOL check_rect(const YUV_CONTEXT* yuv, const RECTANGLE_16* rect, UINT32 nDstWidth,
                              UINT32 nDstHeight)
{
	WINPR_ASSERT(yuv);
	WINPR_ASSERT(rect);

	/* Check, if the output rectangle is valid in decoded h264 frame. */
	if ((rect->right > yuv->width) || (rect->left > yuv->width))
		return FALSE;

	if ((rect->top > yuv->height) || (rect->bottom > yuv->height))
		return FALSE;

	/* Check, if the output rectangle is valid in destination buffer. */
	if ((rect->right > nDstWidth) || (rect->left > nDstWidth))
		return FALSE;

	if ((rect->bottom > nDstHeight) || (rect->top > nDstHeight))
		return FALSE;

	return TRUE;
}

static void CALLBACK yuv444_combine_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                  PTP_WORK work)
{
	YUV_COMBINE_WORK_PARAM* param = (YUV_COMBINE_WORK_PARAM*)context;
	primitives_t* prims = primitives_get();

	WINPR_ASSERT(param);
	YUV_CONTEXT* yuv = param->context;
	WINPR_ASSERT(yuv);

	const RECTANGLE_16* rect = &param->rect;
	WINPR_ASSERT(rect);

	const UINT32 alignedWidth = yuv->width + ((yuv->width % 16 != 0) ? 16 - yuv->width % 16 : 0);
	const UINT32 alignedHeight =
	    yuv->height + ((yuv->height % 16 != 0) ? 16 - yuv->height % 16 : 0);

	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);

	if (!check_rect(param->context, rect, yuv->width, yuv->height))
		return;

	if (prims->YUV420CombineToYUV444(param->type, param->pYUVData, param->iStride, alignedWidth,
	                                 alignedHeight, param->pYUVDstData, param->iDstStride,
	                                 rect) != PRIMITIVES_SUCCESS)
		WLog_WARN(TAG, "YUV420CombineToYUV444 failed");
}

static INLINE YUV_COMBINE_WORK_PARAM pool_decode_rect_param(
    const RECTANGLE_16* rect, YUV_CONTEXT* context, BYTE type, const BYTE* pYUVData[3],
    const UINT32 iStride[3], UINT32 yuvHeight, BYTE* pYUVDstData[3], const UINT32 iDstStride[3])
{
	YUV_COMBINE_WORK_PARAM current = { 0 };

	WINPR_ASSERT(rect);
	WINPR_ASSERT(context);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(pYUVDstData);
	WINPR_ASSERT(iDstStride);

	current.context = context;
	current.pYUVData[0] = pYUVData[0];
	current.pYUVData[1] = pYUVData[1];
	current.pYUVData[2] = pYUVData[2];
	current.pYUVDstData[0] = pYUVDstData[0];
	current.pYUVDstData[1] = pYUVDstData[1];
	current.pYUVDstData[2] = pYUVDstData[2];
	current.iStride[0] = iStride[0];
	current.iStride[1] = iStride[1];
	current.iStride[2] = iStride[2];
	current.iDstStride[0] = iDstStride[0];
	current.iDstStride[1] = iDstStride[1];
	current.iDstStride[2] = iDstStride[2];
	current.type = type;
	current.rect = *rect;
	return current;
}

static BOOL pool_decode_rect(YUV_CONTEXT* context, BYTE type, const BYTE* pYUVData[3],
                             const UINT32 iStride[3], UINT32 yuvHeight, BYTE* pYUVDstData[3],
                             const UINT32 iDstStride[3], const RECTANGLE_16* regionRects,
                             UINT32 numRegionRects)
{
	BOOL rc = FALSE;
	UINT32 y;
	UINT32 waitCount = 0;
	PTP_WORK_CALLBACK cb = yuv444_combine_work_callback;
	primitives_t* prims = primitives_get();

	WINPR_ASSERT(context);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(pYUVDstData);
	WINPR_ASSERT(iDstStride);
	WINPR_ASSERT(regionRects || (numRegionRects == 0));

	if (!context->useThreads || (primitives_flags(prims) & PRIM_FLAGS_HAVE_EXTGPU))
	{
		for (y = 0; y < numRegionRects; y++)
		{
			YUV_COMBINE_WORK_PARAM current =
			    pool_decode_rect_param(&regionRects[y], context, type, pYUVData, iStride, yuvHeight,
			                           pYUVDstData, iDstStride);
			cb(NULL, &current, NULL);
		}
		return TRUE;
	}

	/* case where we use threads */
	for (waitCount = 0; waitCount < numRegionRects; waitCount++)
	{
		YUV_COMBINE_WORK_PARAM* current;

		if (context->work_object_count <= waitCount)
		{
			WLog_ERR(TAG,
			         "YUV rect decoder: invalid number of tiles, only support %" PRIu32
			         ", got %" PRIu32,
			         context->work_object_count, waitCount);
			goto fail;
		}
		current = &context->work_combined_params[waitCount];
		*current = pool_decode_rect_param(&regionRects[waitCount], context, type, pYUVData, iStride,
		                                  yuvHeight, pYUVDstData, iDstStride);

		if (!submit_object(&context->work_objects[waitCount], cb, current, context))
			goto fail;
	}

	rc = TRUE;
fail:
	free_objects(context->work_objects, waitCount);
	return rc;
}

BOOL yuv444_context_decode(YUV_CONTEXT* context, BYTE type, const BYTE* pYUVData[3],
                           const UINT32 iStride[3], UINT32 yuvHeight, BYTE* pYUVDstData[3],
                           const UINT32 iDstStride[3], DWORD DstFormat, BYTE* dest, UINT32 nDstStep,
                           const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	const BYTE* pYUVCDstData[3];

	WINPR_ASSERT(context);
	WINPR_ASSERT(pYUVData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(pYUVDstData);
	WINPR_ASSERT(iDstStride);
	WINPR_ASSERT(dest);
	WINPR_ASSERT(regionRects || (numRegionRects == 0));

	if (context->encoder)
	{
		WLog_ERR(TAG, "YUV context set up for encoding, can not decode with it, aborting");
		return FALSE;
	}
	if (!pool_decode_rect(context, type, pYUVData, iStride, yuvHeight, pYUVDstData, iDstStride,
	                      regionRects, numRegionRects))
		return FALSE;

	pYUVCDstData[0] = pYUVDstData[0];
	pYUVCDstData[1] = pYUVDstData[1];
	pYUVCDstData[2] = pYUVDstData[2];
	return pool_decode(context, yuv444_process_work_callback, pYUVCDstData, iDstStride, yuvHeight,
	                   DstFormat, dest, nDstStep, regionRects, numRegionRects);
}

BOOL yuv420_context_decode(YUV_CONTEXT* context, const BYTE* pYUVData[3], const UINT32 iStride[3],
                           UINT32 yuvHeight, DWORD DstFormat, BYTE* dest, UINT32 nDstStep,
                           const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	return pool_decode(context, yuv420_process_work_callback, pYUVData, iStride, yuvHeight,
	                   DstFormat, dest, nDstStep, regionRects, numRegionRects);
}

static void CALLBACK yuv420_encode_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                 PTP_WORK work)
{
	prim_size_t roi;
	YUV_ENCODE_WORK_PARAM* param = (YUV_ENCODE_WORK_PARAM*)context;
	primitives_t* prims = primitives_get();
	BYTE* pYUVData[3];
	const BYTE* src;

	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);
	WINPR_ASSERT(param);

	roi.width = param->rect.right - param->rect.left;
	roi.height = param->rect.bottom - param->rect.top;
	src = param->pSrcData + param->nSrcStep * param->rect.top +
	      param->rect.left * FreeRDPGetBytesPerPixel(param->SrcFormat);
	pYUVData[0] = param->pYUVLumaData[0] + param->rect.top * param->iStride[0] + param->rect.left;
	pYUVData[1] =
	    param->pYUVLumaData[1] + param->rect.top / 2 * param->iStride[1] + param->rect.left / 2;
	pYUVData[2] =
	    param->pYUVLumaData[2] + param->rect.top / 2 * param->iStride[2] + param->rect.left / 2;

	if (prims->RGBToYUV420_8u_P3AC4R(src, param->SrcFormat, param->nSrcStep, pYUVData,
	                                 param->iStride, &roi) != PRIMITIVES_SUCCESS)
	{
		WLog_ERR(TAG, "error when decoding lines");
	}
}

static void CALLBACK yuv444v1_encode_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                   PTP_WORK work)
{
	prim_size_t roi;
	YUV_ENCODE_WORK_PARAM* param = (YUV_ENCODE_WORK_PARAM*)context;
	primitives_t* prims = primitives_get();
	BYTE* pYUVLumaData[3];
	BYTE* pYUVChromaData[3];
	const BYTE* src;

	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);
	WINPR_ASSERT(param);

	roi.width = param->rect.right - param->rect.left;
	roi.height = param->rect.bottom - param->rect.top;
	src = param->pSrcData + param->nSrcStep * param->rect.top +
	      param->rect.left * FreeRDPGetBytesPerPixel(param->SrcFormat);
	pYUVLumaData[0] =
	    param->pYUVLumaData[0] + param->rect.top * param->iStride[0] + param->rect.left;
	pYUVLumaData[1] =
	    param->pYUVLumaData[1] + param->rect.top / 2 * param->iStride[1] + param->rect.left / 2;
	pYUVLumaData[2] =
	    param->pYUVLumaData[2] + param->rect.top / 2 * param->iStride[2] + param->rect.left / 2;
	pYUVChromaData[0] =
	    param->pYUVChromaData[0] + param->rect.top * param->iStride[0] + param->rect.left;
	pYUVChromaData[1] =
	    param->pYUVChromaData[1] + param->rect.top / 2 * param->iStride[1] + param->rect.left / 2;
	pYUVChromaData[2] =
	    param->pYUVChromaData[2] + param->rect.top / 2 * param->iStride[2] + param->rect.left / 2;
	if (prims->RGBToAVC444YUV(src, param->SrcFormat, param->nSrcStep, pYUVLumaData, param->iStride,
	                          pYUVChromaData, param->iStride, &roi) != PRIMITIVES_SUCCESS)
	{
		WLog_ERR(TAG, "error when decoding lines");
	}
}

static void CALLBACK yuv444v2_encode_work_callback(PTP_CALLBACK_INSTANCE instance, void* context,
                                                   PTP_WORK work)
{
	prim_size_t roi;
	YUV_ENCODE_WORK_PARAM* param = (YUV_ENCODE_WORK_PARAM*)context;
	primitives_t* prims = primitives_get();
	BYTE* pYUVLumaData[3];
	BYTE* pYUVChromaData[3];
	const BYTE* src;

	WINPR_UNUSED(instance);
	WINPR_UNUSED(work);
	WINPR_ASSERT(param);

	roi.width = param->rect.right - param->rect.left;
	roi.height = param->rect.bottom - param->rect.top;
	src = param->pSrcData + param->nSrcStep * param->rect.top +
	      param->rect.left * FreeRDPGetBytesPerPixel(param->SrcFormat);
	pYUVLumaData[0] =
	    param->pYUVLumaData[0] + param->rect.top * param->iStride[0] + param->rect.left;
	pYUVLumaData[1] =
	    param->pYUVLumaData[1] + param->rect.top / 2 * param->iStride[1] + param->rect.left / 2;
	pYUVLumaData[2] =
	    param->pYUVLumaData[2] + param->rect.top / 2 * param->iStride[2] + param->rect.left / 2;
	pYUVChromaData[0] =
	    param->pYUVChromaData[0] + param->rect.top * param->iStride[0] + param->rect.left;
	pYUVChromaData[1] =
	    param->pYUVChromaData[1] + param->rect.top / 2 * param->iStride[1] + param->rect.left / 2;
	pYUVChromaData[2] =
	    param->pYUVChromaData[2] + param->rect.top / 2 * param->iStride[2] + param->rect.left / 2;
	if (prims->RGBToAVC444YUVv2(src, param->SrcFormat, param->nSrcStep, pYUVLumaData,
	                            param->iStride, pYUVChromaData, param->iStride,
	                            &roi) != PRIMITIVES_SUCCESS)
	{
		WLog_ERR(TAG, "error when decoding lines");
	}
}

static INLINE YUV_ENCODE_WORK_PARAM pool_encode_fill(const RECTANGLE_16* rect, YUV_CONTEXT* context,
                                                     const BYTE* pSrcData, UINT32 nSrcStep,
                                                     UINT32 SrcFormat, const UINT32 iStride[],
                                                     BYTE* pYUVLumaData[], BYTE* pYUVChromaData[])
{
	YUV_ENCODE_WORK_PARAM current = { 0 };

	WINPR_ASSERT(rect);
	WINPR_ASSERT(context);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(pYUVLumaData);

	current.context = context;
	current.pSrcData = pSrcData;
	current.SrcFormat = SrcFormat;
	current.nSrcStep = nSrcStep;
	current.pYUVLumaData[0] = pYUVLumaData[0];
	current.pYUVLumaData[1] = pYUVLumaData[1];
	current.pYUVLumaData[2] = pYUVLumaData[2];
	if (pYUVChromaData)
	{
		current.pYUVChromaData[0] = pYUVChromaData[0];
		current.pYUVChromaData[1] = pYUVChromaData[1];
		current.pYUVChromaData[2] = pYUVChromaData[2];
	}
	current.iStride[0] = iStride[0];
	current.iStride[1] = iStride[1];
	current.iStride[2] = iStride[2];

	current.rect = *rect;

	return current;
}

static BOOL pool_encode(YUV_CONTEXT* context, PTP_WORK_CALLBACK cb, const BYTE* pSrcData,
                        UINT32 nSrcStep, UINT32 SrcFormat, const UINT32 iStride[],
                        BYTE* pYUVLumaData[], BYTE* pYUVChromaData[],
                        const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	BOOL rc = FALSE;
	primitives_t* prims = primitives_get();
	UINT32 x, y;
	UINT32 waitCount = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(cb);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(iStride);
	WINPR_ASSERT(regionRects || (numRegionRects == 0));

	if (!context->encoder)
	{

		WLog_ERR(TAG, "YUV context set up for decoding, can not encode with it, aborting");
		return FALSE;
	}

	if (!context->useThreads || (primitives_flags(prims) & PRIM_FLAGS_HAVE_EXTGPU))
	{
		for (x = 0; x < numRegionRects; x++)
		{
			YUV_ENCODE_WORK_PARAM current =
			    pool_encode_fill(&regionRects[x], context, pSrcData, nSrcStep, SrcFormat, iStride,
			                     pYUVLumaData, pYUVChromaData);
			cb(NULL, &current, NULL);
		}
		return TRUE;
	}

	/* case where we use threads */
	for (x = 0; x < numRegionRects; x++)
	{
		const RECTANGLE_16* rect = &regionRects[x];
		const UINT32 height = rect->bottom - rect->top;
		const UINT32 steps = (height + context->heightStep / 2) / context->heightStep;

		waitCount += steps;
	}

	for (x = 0; x < numRegionRects; x++)
	{
		const RECTANGLE_16* rect = &regionRects[x];
		const UINT32 height = rect->bottom - rect->top;
		const UINT32 steps = (height + context->heightStep / 2) / context->heightStep;

		for (y = 0; y < steps; y++)
		{
			RECTANGLE_16 r = *rect;
			YUV_ENCODE_WORK_PARAM* current;

			if (context->work_object_count <= waitCount)
			{
				WLog_ERR(TAG,
				         "YUV encoder: invalid number of tiles, only support %" PRIu32
				         ", got %" PRIu32,
				         context->work_object_count, waitCount);
				goto fail;
			}

			current = &context->work_enc_params[waitCount];
			r.top += y * context->heightStep;
			*current = pool_encode_fill(&r, context, pSrcData, nSrcStep, SrcFormat, iStride,
			                            pYUVLumaData, pYUVChromaData);
			if (!submit_object(&context->work_objects[waitCount], cb, current, context))
				goto fail;
			waitCount++;
		}
	}

	rc = TRUE;
fail:
	free_objects(context->work_objects, waitCount);
	return rc;
}

BOOL yuv420_context_encode(YUV_CONTEXT* context, const BYTE* pSrcData, UINT32 nSrcStep,
                           UINT32 SrcFormat, const UINT32 iStride[3], BYTE* pYUVData[3],
                           const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	if (!context || !pSrcData || !iStride || !pYUVData || !regionRects)
		return FALSE;

	return pool_encode(context, yuv420_encode_work_callback, pSrcData, nSrcStep, SrcFormat, iStride,
	                   pYUVData, NULL, regionRects, numRegionRects);
}

BOOL yuv444_context_encode(YUV_CONTEXT* context, BYTE version, const BYTE* pSrcData,
                           UINT32 nSrcStep, UINT32 SrcFormat, const UINT32 iStride[3],
                           BYTE* pYUVLumaData[3], BYTE* pYUVChromaData[3],
                           const RECTANGLE_16* regionRects, UINT32 numRegionRects)
{
	PTP_WORK_CALLBACK cb;
	switch (version)
	{
		case 1:
			cb = yuv444v1_encode_work_callback;
			break;
		case 2:
			cb = yuv444v2_encode_work_callback;
			break;
		default:
			return FALSE;
	}

	return pool_encode(context, cb, pSrcData, nSrcStep, SrcFormat, iStride, pYUVLumaData,
	                   pYUVChromaData, regionRects, numRegionRects);
}
