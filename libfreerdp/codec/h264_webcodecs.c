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
 *
 * Browsers return I420 or NV12 for 4:2:0 and I444 for 4:4:4, which are copied as they are.
 * Some (Firefox) return RGB frames instead, converted with the colour description of the
 * stream; h264_webcodecs.js turns those back into full resolution Y'CbCr planes, see
 * h264_wc_copy. The chroma planes are therefore always allocated at full resolution.
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
#define H264_WC_YUV444 3       /* picture copied as full resolution 4:4:4 planes */

typedef struct
{
	int handle;
	BOOL hevc;    /* H.265: NAL types and codec string differ, the rest is the same */
	BOOL hevc444; /* the HEVC stream is 4:4:4 */
	BYTE* params; /* last parameter set NAL units (H.264 SPS/PPS, HEVC VPS/SPS/PPS),
	               * each with a 4 byte start code */
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
	int gbr; /* RGB frames hold Y'CbCr samples labelled as GBR, see h264_wc_copy */
	UINT32 maxWidth;
	UINT32 maxHeight;
	UINT32 dim[2]; /* decoded width and height */
	int rc;        /* 1: 4:2:0 picture copied, 3: 4:4:4 picture copied, 0: no picture, 2: planes too
	                * small, <0: error */
} h264_wc_job;

/* in h264_webcodecs.js */
extern void h264_wc_js_open(void* ctx, int* handle, const char* probe);
extern void h264_wc_js_close(int handle);
extern void h264_wc_js_decode(void* ctx, int handle, const BYTE* src, UINT32 size, int key,
                              const char* codec, BYTE* y, BYTE* u, BYTE* v, UINT32 ys, UINT32 cs,
                              int gbr, UINT32 maxWidth, UINT32 maxHeight, UINT32* dim, int* rc);
extern void h264_wc_js_copy(void* ctx, int handle, BYTE* y, BYTE* u, BYTE* v, UINT32 ys, UINT32 cs,
                            int gbr, int* rc);

EMSCRIPTEN_KEEPALIVE void freerdp_h264_wc_finish(em_proxying_ctx* ctx)
{
	emscripten_proxy_finish(ctx);
}

typedef struct
{
	int handle;
	const char* probe; /* codec string the browser must support */
} h264_wc_open;

static void main_open(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_open* open = arg;
	h264_wc_js_open(ctx, &open->handle, open->probe);
}

static void main_close(void* arg)
{
	h264_wc_js_close(*(int*)arg);
}

static void main_decode(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_job* job = arg;
	h264_wc_js_decode(ctx, job->handle, job->src, job->size, job->key, job->codec, job->plane[0],
	                  job->plane[1], job->plane[2], job->stride[0], job->stride[1], job->gbr,
	                  job->maxWidth, job->maxHeight, job->dim, &job->rc);
}

static void main_copy(em_proxying_ctx* ctx, void* arg)
{
	h264_wc_job* job = arg;
	h264_wc_js_copy(ctx, job->handle, job->plane[0], job->plane[1], job->plane[2], job->stride[0],
	                job->stride[1], job->gbr, &job->rc);
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

/* H.264 and HEVC number their NAL unit types differently */
#define H264_NAL_SLICE 1
#define H264_NAL_IDR 5
#define H264_NAL_SPS 7
#define H264_NAL_PPS 8
#define HEVC_NAL_BLA_W_LP 16 /* 16 to 21 start a random access point (IRAP) */
#define HEVC_NAL_CRA 21
#define HEVC_NAL_VPS 32 /* 0 to 31 are slices (VCL) */
#define HEVC_NAL_SPS 33
#define HEVC_NAL_PPS 34

WINPR_ATTR_NODISCARD
static BYTE nal_type(const H264_CONTEXT_WEBCODECS* sys, const BYTE* nal)
{
	return sys->hevc ? (nal[0] >> 1) & 0x3F : nal[0] & 0x1F;
}

WINPR_ATTR_NODISCARD
static BOOL nal_is_slice(const H264_CONTEXT_WEBCODECS* sys, BYTE type)
{
	return sys->hevc ? (type < HEVC_NAL_VPS) : ((type == H264_NAL_SLICE) || (type == H264_NAL_IDR));
}

WINPR_ATTR_NODISCARD
static BOOL nal_is_key(const H264_CONTEXT_WEBCODECS* sys, BYTE type)
{
	return sys->hevc ? ((type >= HEVC_NAL_BLA_W_LP) && (type <= HEVC_NAL_CRA))
	                 : (type == H264_NAL_IDR);
}

/* a sequence parameter set starts a new set of saved parameters */
WINPR_ATTR_NODISCARD
static BOOL nal_is_sps(const H264_CONTEXT_WEBCODECS* sys, BYTE type)
{
	return sys->hevc ? ((type == HEVC_NAL_VPS) || (type == HEVC_NAL_SPS)) : (type == H264_NAL_SPS);
}

WINPR_ATTR_NODISCARD
static BOOL nal_is_parameter_set(const H264_CONTEXT_WEBCODECS* sys, BYTE type)
{
	return nal_is_sps(sys, type) || (type == (sys->hevc ? HEVC_NAL_PPS : H264_NAL_PPS));
}

/* parameter sets too short to be used are dropped (the H.264 SPS is read for the codec) */
WINPR_ATTR_NODISCARD
static BOOL nal_is_saved(const H264_CONTEXT_WEBCODECS* sys, BYTE type, size_t length)
{
	if (!nal_is_parameter_set(sys, type))
		return FALSE;
	if (sys->hevc)
		return length >= 3;
	return (type != H264_NAL_SPS) || (length >= 4);
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
		if (nal_is_saved(sys, nal_type(sys, nal), length))
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
		if (!nal_is_saved(sys, nal_type(sys, nal), length))
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

/* avc1.PPCCLL from profile_idc, constraint flags and level_idc of the first SPS */
static void codec_h264(const H264_CONTEXT_WEBCODECS* sys, char* codec, size_t size)
{
	(void)_snprintf(codec, size, "avc1.42e01f");
	if ((sys->paramsSize > 7) && ((sys->params[4] & 0x1F) == H264_NAL_SPS))
		(void)_snprintf(codec, size, "avc1.%02x%02x%02x", sys->params[5], sys->params[6],
		                sys->params[7]);
}

/* copies up to max bytes of the RBSP: the payload without emulation prevention bytes */
WINPR_ATTR_NODISCARD
static size_t nal_rbsp(const BYTE* data, size_t length, BYTE* rbsp, size_t max)
{
	size_t count = 0;
	size_t zeros = 0;
	for (size_t x = 0; (x < length) && (count < max); x++)
	{
		if ((zeros >= 2) && (data[x] == 3))
		{
			zeros = 0;
			continue;
		}
		zeros = (data[x] == 0) ? zeros + 1 : 0;
		rbsp[count++] = data[x];
	}
	return count;
}

/* hvc1.[A-C]<profile_idc>.<compatibility flags, bit reversed>.<L|H><level_idc>[.<constraint
 * flag bytes, trailing zeros omitted>] from the general profile_tier_level of the first SPS
 * (ISO/IEC 14496-15 annex E) */
static void codec_hevc(const H264_CONTEXT_WEBCODECS* sys, char* codec, size_t size)
{
	(void)_snprintf(codec, size, "hvc1.1.6.L93.B0"); /* Main profile, level 3.1 */

	size_t offset = 0;
	const BYTE* nal = nullptr;
	size_t length = 0;
	while (next_nal(sys->params, sys->paramsSize, &offset, &nal, &length))
	{
		if ((length < 2) || (nal_type(sys, nal) != HEVC_NAL_SPS))
			continue;

		/* after the 2 byte NAL header: sps_video_parameter_set_id, sps_max_sub_layers_minus1
		 * and sps_temporal_id_nesting_flag (1 byte), then profile space, tier and profile_idc
		 * (1), compatibility flags (4), constraint flags (6) and level_idc (1) */
		BYTE ptl[13] = WINPR_C_ARRAY_INIT;
		if (nal_rbsp(&nal[2], length - 2, ptl, sizeof(ptl)) < sizeof(ptl))
			return;

		const char* spaces[] = { "", "A", "B", "C" };
		const UINT32 flags = ((UINT32)ptl[2] << 24) | ((UINT32)ptl[3] << 16) |
		                     ((UINT32)ptl[4] << 8) | (UINT32)ptl[5];
		UINT32 reversed = 0;
		for (size_t x = 0; x < 32; x++)
			reversed |= ((flags >> x) & 1u) << (31 - x);

		char buffer[64] = WINPR_C_ARRAY_INIT;
		int len = _snprintf(buffer, sizeof(buffer), "hvc1.%s%u.%" PRIX32 ".%c%u",
		                    spaces[ptl[1] >> 6], (unsigned)(ptl[1] & 0x1F), reversed,
		                    (ptl[1] & 0x20) ? 'H' : 'L', (unsigned)ptl[12]);
		size_t constraints = 6;
		while ((constraints > 0) && (ptl[5 + constraints] == 0))
			constraints--;
		for (size_t x = 0; (x < constraints) && (len > 0) && ((size_t)len < sizeof(buffer)); x++)
			len += _snprintf(&buffer[len], sizeof(buffer) - (size_t)len, ".%02X",
			                 (unsigned)ptl[6 + x]);
		if ((len > 0) && ((size_t)len < size))
			(void)_snprintf(codec, size, "%s", buffer);
		return;
	}
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

	/* same padding as the encoder planes in h264.c; relaxes bounds checks in the converters.
	 * The chroma planes have full resolution for 4:4:4 pictures, 4:2:0 ones use part of them. */
	const UINT32 ystride = width + 32 - width % 16;
	const UINT32 cstride = ystride;
	const size_t lines = height + 32 - height % 16;
	const size_t ysize = (size_t)ystride * lines;
	const size_t csize = ysize;
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
		const BYTE type = nal_type(sys, nal);
		hasSlice |= nal_is_slice(sys, type);
		hasIdr |= nal_is_key(sys, type);
		hasSps |= nal_is_sps(sys, type);
		hasPps |= nal_is_parameter_set(sys, type);
	}
	if ((hasSps || hasPps) && !save_parameters(sys, pSrcData, SrcSize, hasSps))
		return -1;
	if (!hasSlice)
		return 0; /* parameter sets only, no picture */

	char codec[64] = WINPR_C_ARRAY_INIT;
	if (sys->hevc)
		codec_hevc(sys, codec, sizeof(codec));
	else
		codec_h264(sys, codec, sizeof(codec));

	/* a key picture without in-band parameter sets gets the saved ones */
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
		                .gbr = sys->hevc444,
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
		WLog_Print(h264->log, WLOG_WARN, "WebCodecs %s decode failed (%d)",
		           sys->hevc ? "HEVC" : "H.264", rc);
	if ((rc != 1) && (rc != H264_WC_YUV444))
		goto fail;
	h264->yuv444 = (rc == H264_WC_YUV444);
	rc = 1;

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
	sys->hevc = h264->hevc;
	sys->hevc444 = h264->hevc && h264->hevc444;

	/* H.264 Constrained Baseline 3.1 / HEVC Main 3.1: every decoder of the codec handles them.
	 * HEVC 4:4:4 needs a Main 4:4:4 (Range Extensions) decoder, which most browsers only have
	 * with a GPU that decodes it. */
	const char* probe = "avc1.42e01f";
	if (sys->hevc)
		probe = sys->hevc444 ? "hvc1.4.10.L120.9E.08" : "hvc1.1.6.L93.B0";
	h264_wc_open open = { .handle = 0, .probe = probe };
	if (!on_main(main_open, &open) || (open.handle <= 0))
	{
		WLog_Print(h264->log, WLOG_DEBUG, "the browser cannot decode %s with WebCodecs",
		           sys->hevc ? (sys->hevc444 ? "HEVC 4:4:4" : "HEVC") : "H.264");
		free(sys);
		return FALSE;
	}
	sys->handle = open.handle;
	h264->pSystemData = sys;
	return TRUE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_WebCodecs = { "WebCodecs", webcodecs_init,
	                                                   webcodecs_uninit, webcodecs_decompress,
	                                                   nullptr };
