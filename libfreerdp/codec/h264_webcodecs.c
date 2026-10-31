/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 decoding with the browser's WebCodecs API (Emscripten builds)
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

/*
 * The H.264 decompress callback runs on the graphics channel thread (a Web Worker) and must
 * return the picture synchronously, while VideoDecoder delivers frames asynchronously on the
 * event loop of the thread that owns it. The worker therefore proxies each job to the main
 * browser thread with emscripten_proxy_sync_with_ctx() and sleeps until the main thread has
 * decoded the frame, copied it into the I420 planes and called emscripten_proxy_finish().
 *
 * The planes keep the decoded sample values (no colour conversion in the browser), so
 * h264.c converts them and combines the AVC444 main and auxiliary pictures as it does for
 * every other backend.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <winpr/assert.h>
#include <winpr/string.h>
#include <freerdp/log.h>

#include "h264.h"

#define H264_WC_NEEDS_BUFFER 2 /* picture decoded but larger than the planes */

typedef struct
{
	int handle;
	BYTE* params; /* last SPS/PPS NAL units, each with a 4 byte start code */
	size_t paramsSize;
	BYTE* yuv; /* Y, U and V planes in one allocation */
	UINT32 width;
	UINT32 height;
	UINT32 stride[3];
	BYTE* plane[3];
} H264_CONTEXT_WEBCODECS;

typedef struct
{
	int handle;
	const BYTE* src;
	UINT32 size;
	int key;
	const char* codec;
	BYTE* const* plane;
	const UINT32* stride;
	UINT32 maxWidth;
	UINT32 maxHeight;
	UINT32 dim[2]; /* decoded width and height */
	int rc;        /* 1: picture copied, 0: no picture, 2: planes too small, <0: error */
} h264_wc_job;

/* in h264_webcodecs.js */
extern void h264_wc_js_open(void* ctx, int* handle);
extern void h264_wc_js_close(int handle);
extern void h264_wc_js_decode(void* ctx, int handle, const BYTE* src, UINT32 size, int key,
                              const char* codec, BYTE* y, BYTE* u, BYTE* v, UINT32 ys, UINT32 cs,
                              UINT32 maxWidth, UINT32 maxHeight, UINT32* dim, int* rc);
extern void h264_wc_js_copy(void* ctx, int handle, BYTE* y, BYTE* u, BYTE* v, UINT32 ys, UINT32 cs,
                            int* rc);

EMSCRIPTEN_KEEPALIVE void freerdp_h264_wc_finish(em_proxying_ctx* ctx)
{
	emscripten_proxy_finish(ctx);
}

static void main_open(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_js_open(ctx, arg);
}

static void main_close(void* arg)
{
	h264_wc_js_close(*(int*)arg);
}

static void main_decode(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_job* job = arg;
	h264_wc_js_decode(ctx, job->handle, job->src, job->size, job->key, job->codec, job->plane[0],
	                  job->plane[1], job->plane[2], job->stride[0], job->stride[1], job->maxWidth,
	                  job->maxHeight, job->dim, &job->rc);
}

static void main_copy(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_job* job = arg;
	h264_wc_js_copy(ctx, job->handle, job->plane[0], job->plane[1], job->plane[2], job->stride[0],
	                job->stride[1], &job->rc);
}

WINPR_ATTR_NODISCARD
static BOOL on_main(void (*fn)(em_proxying_ctx*, void*), void* arg)
{
	return emscripten_proxy_sync_with_ctx(emscripten_proxy_get_system_queue(),
	                                      emscripten_main_runtime_thread_id(), fn, arg) == 1;
}

/* Finds the next Annex B NAL unit; trailing zero bytes belong to the next start code. */
WINPR_ATTR_NODISCARD
static BOOL next_nal(const BYTE* data, size_t size, size_t* offset, const BYTE** nal,
                     size_t* length)
{
	size_t pos = *offset;
	if ((size < 3) || (pos == SIZE_MAX - size))
		return FALSE;
	while ((pos <= size - 3ull) &&
	       !((data[pos] == 0) && (data[pos + 1] == 0) && (data[pos + 2] == 1)))
		pos++;
	if (pos >= size - 3ull)
		return FALSE;

	const size_t start = pos + 3;
	pos = start;
	while ((pos <= size - 3ull) &&
	       !((data[pos] == 0) && (data[pos + 1] == 0) && (data[pos + 2] == 1)))
		pos++;
	size_t end = (pos <= size - 3ull) ? pos : size;
	while ((end > start) && (data[end - 1] == 0))
		end--;
	*offset = end;
	*nal = &data[start];
	*length = end - start;
	return TRUE;
}

/* Remembers the parameter sets: they may arrive separately from the picture (MS-RDPEVOR),
 * and a decoder that is created again after an error needs them with the next IDR picture. */
WINPR_ATTR_NODISCARD
static BOOL save_parameters(H264_CONTEXT_WEBCODECS* sys, const BYTE* data, size_t size, BOOL hasSps)
{
	size_t total = hasSps ? 0 : sys->paramsSize;
	size_t offset = 0;
	const BYTE* nal = nullptr;
	size_t length = 0;
	while (next_nal(data, size, &offset, &nal, &length))
	{
		if (length == 0)
			continue;
		const BYTE type = nal[0] & 0x1F;
		if ((type == 7) && (length < 4))
			continue;
		if ((type == 7) || (type == 8))
			total += 4 + length;
	}

	BYTE* params = malloc(total);
	if (!params)
		return FALSE;
	size_t pos = 0;
	if (!hasSps && (sys->paramsSize > 0))
	{
		memcpy(params, sys->params, sys->paramsSize);
		pos = sys->paramsSize;
	}
	offset = 0;
	while (next_nal(data, size, &offset, &nal, &length))
	{
		if (length == 0)
			continue;
		const BYTE type = nal[0] & 0x1F;
		if ((type == 7) && (length < 4))
			continue;
		if ((type != 7) && (type != 8))
			continue;
		const BYTE startCode[] = { 0, 0, 0, 1 };
		memcpy(&params[pos], startCode, sizeof(startCode));
		memcpy(&params[pos + 4], nal, length);
		pos += 4 + length;
	}
	WINPR_ASSERT(pos == total);
	free(sys->params);
	sys->params = params;
	sys->paramsSize = total;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL ensure_planes(H264_CONTEXT_WEBCODECS* sys, UINT32 width, UINT32 height)
{
	if (sys->yuv && (width <= sys->width) && (height <= sys->height))
		return TRUE;
	width = MAX(width, sys->width);
	height = MAX(height, sys->height);
	if ((width == 0) || (height == 0) || (width > 16384) || (height > 16384))
		return FALSE;

	/* same padding as the encoder planes in h264.c; relaxes bounds checks in the converters */
	const UINT32 ystride = width + 32 - width % 16;
	const UINT32 cstride = (ystride + 1) / 2;
	const size_t lines = height + 32 - height % 16;
	const size_t ysize = (size_t)ystride * lines;
	const size_t csize = (size_t)cstride * (lines / 2);
	BYTE* yuv = winpr_aligned_calloc(ysize + 2 * csize, 1, 16);
	if (!yuv)
		return FALSE;
	winpr_aligned_free(sys->yuv);
	sys->yuv = yuv;
	sys->width = width;
	sys->height = height;
	sys->stride[0] = ystride;
	sys->stride[1] = cstride;
	sys->stride[2] = cstride;
	/* h264_wc_js_copy expects the planes in this order, in one buffer */
	sys->plane[0] = yuv;
	sys->plane[1] = &yuv[ysize];
	sys->plane[2] = &yuv[ysize + csize];
	return TRUE;
}

WINPR_ATTR_NODISCARD
static int webcodecs_decompress(H264_CONTEXT* WINPR_RESTRICT h264,
                                const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData || (SrcSize == 0));

	H264_CONTEXT_WEBCODECS* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	if (emscripten_is_main_browser_thread())
		return -1; /* the main thread cannot block for its own asynchronous decode */

	BOOL hasSlice = FALSE;
	BOOL hasIdr = FALSE;
	BOOL hasSps = FALSE;
	BOOL hasPps = FALSE;
	size_t offset = 0;
	const BYTE* nal = nullptr;
	size_t length = 0;
	while (next_nal(pSrcData, SrcSize, &offset, &nal, &length))
	{
		if (length == 0)
			continue;
		const BYTE type = nal[0] & 0x1F;
		hasSlice |= (type == 1) || (type == 5);
		hasIdr |= (type == 5);
		hasSps |= (type == 7);
		hasPps |= (type == 8);
	}
	if ((hasSps || hasPps) && !save_parameters(sys, pSrcData, SrcSize, hasSps))
		return -1;
	if (!hasSlice)
		return 0; /* parameter sets only, no picture */

	/* avc1.PPCCLL from profile_idc, constraint flags and level_idc of the first SPS */
	char codec[16] = "avc1.42e01f";
	if ((sys->paramsSize > 7) && ((sys->params[4] & 0x1F) == 7))
		(void)_snprintf(codec, sizeof(codec), "avc1.%02x%02x%02x", sys->params[5], sys->params[6],
		                sys->params[7]);

	/* an IDR picture without in-band SPS/PPS gets the saved ones */
	const BYTE* src = pSrcData;
	UINT32 size = SrcSize;
	BYTE* combined = nullptr;
	if (hasIdr && !hasSps && (sys->paramsSize > 0))
	{
		if (sys->paramsSize > UINT32_MAX - SrcSize)
			return -1;
		size = (UINT32)sys->paramsSize + SrcSize;
		combined = malloc(size);
		if (!combined)
			return -1;
		memcpy(combined, sys->params, sys->paramsSize);
		memcpy(&combined[sys->paramsSize], pSrcData, SrcSize);
		src = combined;
	}

	int rc = -1;
	h264_wc_job job = { .handle = sys->handle,
		                .src = src,
		                .size = size,
		                .key = hasIdr,
		                .codec = codec,
		                .plane = sys->plane,
		                .stride = sys->stride,
		                .rc = -1 };
	if (!ensure_planes(sys, MAX(h264->width, 16), MAX(h264->height, 16)))
		goto fail;
	job.maxWidth = sys->width;
	job.maxHeight = sys->height;
	if (!on_main(main_decode, &job))
		goto fail;
	if (job.rc == H264_WC_NEEDS_BUFFER)
	{
		job.rc = -1;
		if (!ensure_planes(sys, job.dim[0], job.dim[1]) || !on_main(main_copy, &job))
			goto fail;
	}
	rc = job.rc;
	if (rc < 0)
		WLog_Print(h264->log, WLOG_WARN, "WebCodecs H.264 decode failed (%d)", rc);
	if (rc != 1)
		goto fail;

	for (size_t x = 0; x < 3; x++)
	{
		h264->pYUVData[x] = sys->plane[x];
		h264->iStride[x] = sys->stride[x];
	}
	h264->YUVWidth = job.dim[0];
	h264->YUVHeight = job.dim[1];
fail:
	free(combined);
	return rc;
}

static void webcodecs_uninit(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_WEBCODECS* sys = h264->pSystemData;
	if (!sys)
		return;
	if ((sys->handle > 0) && !emscripten_is_main_browser_thread())
		(void)emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
		                            emscripten_main_runtime_thread_id(), main_close, &sys->handle);
	else if (sys->handle > 0)
		main_close(&sys->handle);
	for (size_t x = 0; x < 3; x++)
		h264->pYUVData[x] = nullptr;
	winpr_aligned_free(sys->yuv);
	free(sys->params);
	free(sys);
	h264->pSystemData = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL webcodecs_init(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	/* decoder only; the main thread cannot wait for the asynchronous VideoDecoder */
	if (h264->Compressor || emscripten_is_main_browser_thread())
		return FALSE;

	H264_CONTEXT_WEBCODECS* sys = calloc(1, sizeof(*sys));
	if (!sys)
		return FALSE;
	if (!on_main(main_open, &sys->handle) || (sys->handle <= 0))
	{
		WLog_Print(h264->log, WLOG_DEBUG, "the browser cannot decode H.264 with WebCodecs");
		free(sys);
		return FALSE;
	}
	h264->pSystemData = sys;
	return TRUE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_WebCodecs = { "WebCodecs", webcodecs_init,
	                                                   webcodecs_uninit, webcodecs_decompress,
	                                                   nullptr };
