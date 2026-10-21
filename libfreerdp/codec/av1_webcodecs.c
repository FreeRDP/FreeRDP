/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV1 decoding with the browser's WebCodecs API (Emscripten builds)
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
 * freerdp_av1_decompress() runs on the graphics channel thread (a Web Worker) and needs the
 * picture back synchronously, while VideoDecoder delivers frames asynchronously on the event
 * loop of the thread that owns it. The worker therefore proxies each job to the main browser
 * thread with emscripten_proxy_sync_with_ctx() and sleeps until the main thread has decoded
 * the frame, copied it into the destination surface and called emscripten_proxy_finish().
 *
 * VideoFrame.copyTo() converts to the surface's RGB layout directly, so the browser (GPU or
 * its native decoder) does both the decoding and the colour conversion, honouring the colour
 * space signalled in the bitstream.
 */

#include <freerdp/config.h>

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <winpr/assert.h>
#include <freerdp/codec/color.h>

#include "av1_webcodecs.h"

typedef struct
{
	int handle;
	const BYTE* src;
	UINT32 size;
	BYTE* dst;
	const char* format;
	UINT32 stride;
	UINT32 width;
	UINT32 height;
	int key;
	int profile;
	int rc; /* 1: picture written, 0: no picture, <0: error */
} av1_wc_job;

/* clang-format off */
EM_JS(int, av1_wc_js_available, (void), {
	return (typeof VideoDecoder === "function" && typeof EncodedVideoChunk === "function") ? 1 : 0;
});

EM_JS(int, av1_wc_js_open, (void), {
	const reg = (Module.freerdpAv1 = Module.freerdpAv1 || { next : 1, decoders : new Map() });
	const handle = reg.next++;
	reg.decoders.set(handle, { decoder : null, profile : -1, pending : null });
	return handle;
});

EM_JS(void, av1_wc_js_close, (int handle), {
	const reg = Module.freerdpAv1;
	const d = reg?.decoders.get(handle);
	if (!d)
		return;
	try { d.decoder?.close(); } catch (e) {}
	reg.decoders.delete(handle);
});

/* Runs on the main browser thread; stores the result in *rc and completes asynchronously
 * through _freerdp_av1_wc_finish. */
EM_JS(void, av1_wc_js_decode, (void* ctx, int handle, const BYTE* src, UINT32 size, BYTE* dst,
                               const char* formatName, UINT32 stride, UINT32 width, UINT32 height,
                               int key, int profile, int* rc), {
	src >>>= 0; size >>>= 0; dst >>>= 0; stride >>>= 0; width >>>= 0; height >>>= 0;
	const format = UTF8ToString(formatName);
	const finish = (r) => { HEAP32[rc >> 2] = r; _freerdp_av1_wc_finish(ctx); };
	const d = Module.freerdpAv1?.decoders.get(handle);
	if (!d)
		return finish(-1);

	/* (re)configure on key frames that carry a different profile (4:2:0 = 0, 4:4:4 = 1) */
	if (!d.decoder || d.decoder.state === "closed" || (key && profile >= 0 && profile !== d.profile)) {
		if (!key)
			return finish(0); /* WebCodecs must start with a key frame: drop until one arrives */
		try { d.decoder?.close(); } catch (e) {}
		d.profile = profile;
		d.decoder = new VideoDecoder({
			output : (frame) => {
				const p = d.pending;
				d.pending = null;
				if (!p) { frame.close(); return; }
				clearTimeout(p.timer);
				const w = Math.min(frame.displayWidth, p.width), h = Math.min(frame.displayHeight, p.height);
				const view = HEAPU8.subarray(p.dst, p.dst + p.stride * h);
				frame.copyTo(view, { rect : { x : 0, y : 0, width : w, height : h },
				                     layout : [ { offset : 0, stride : p.stride } ], format : p.format })
				    .then(() => { frame.close(); p.finish(1); },
				          (e) => { frame.close(); console.error("[av1-webcodecs] copyTo", e); p.finish(-3); });
			},
			error : (e) => {
				console.error("[av1-webcodecs] decoder error", e);
				const p = d.pending;
				d.pending = null;
				if (p) { clearTimeout(p.timer); p.finish(-2); }
			}
		});
		d.decoder.configure({ codec : profile === 1 ? "av01.1.13M.08" : "av01.0.13M.08",
		                      optimizeForLatency : true, hardwareAcceleration : "no-preference" });
	}

	d.pending = { dst, format, stride, width, height, finish,
		/* low-latency streams release each frame at once; never hang the worker if not */
		timer : setTimeout(() => { if (d.pending) { d.pending = null; finish(0); } }, 1000) };
	try {
		const data = HEAPU8.slice(src, src + size); /* EncodedVideoChunk copies; avoid SAB views */
		d.decoder.decode(new EncodedVideoChunk({ type : key ? "key" : "delta", timestamp : 0, data }));
	} catch (e) {
		console.error("[av1-webcodecs] decode", e);
		clearTimeout(d.pending.timer);
		d.pending = null;
		finish(-2);
	}
});
/* clang-format on */

EMSCRIPTEN_KEEPALIVE void freerdp_av1_wc_finish(em_proxying_ctx* ctx)
{
	emscripten_proxy_finish(ctx);
}

static void main_available(void* arg)
{
	*(int*)arg = av1_wc_js_available();
}

static void main_open(void* arg)
{
	*(int*)arg = av1_wc_js_open();
}

static void main_close(void* arg)
{
	av1_wc_js_close(*(int*)arg);
}

static void main_decode(em_proxying_ctx* ctx, void* arg)
{
	av1_wc_job* job = arg;
	WINPR_ASSERT(job);

	av1_wc_js_decode(ctx, job->handle, job->src, job->size, job->dst, job->format, job->stride,
	                 job->width, job->height, job->key, job->profile, &job->rc);
}

static BOOL on_main(void (*fn)(void*), void* arg)
{
	WINPR_ASSERT(fn);

	if (emscripten_is_main_browser_thread())
	{
		fn(arg);
		return TRUE;
	}
	return emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
	                             emscripten_main_runtime_thread_id(), fn, arg) == 1;
}

int av1_wc_open(void)
{
	int available = 0;
	if (!on_main(main_available, &available) || !available)
		return 0;
	int handle = 0;
	if (!on_main(main_open, &handle))
		return 0;
	return handle;
}

void av1_wc_close(int handle)
{
	if (handle > 0)
		(void)on_main(main_close, &handle);
}

/* AV1 low overhead bitstream: a key frame carries a sequence header OBU (type 1), whose
 * first 3 bits are seq_profile. Returns the profile, or -1 if there is no sequence header. */
static int sequence_header_profile(const BYTE* p, UINT32 size)
{
	UINT32 pos = 0;
	while (pos < size)
	{
		const BYTE header = p[pos];
		const int type = (header >> 3) & 0x0f;
		const int has_extension = (header >> 2) & 1;
		const int has_size = (header >> 1) & 1;
		pos += 1 + (UINT32)has_extension;
		if (!has_size)
			return (type == 1 && pos < size) ? (p[pos] >> 5) : -1;
		UINT64 len = 0;
		for (int i = 0; i < 8 && pos < size; i++)
		{
			const BYTE b = p[pos++];
			len |= (UINT64)(b & 0x7f) << (7 * i);
			if (!(b & 0x80))
				break;
		}
		if (type == 1)
			return (pos < size) ? (p[pos] >> 5) : -1;
		if (len > size - pos)
			return -1;
		pos += (UINT32)len;
	}
	return -1;
}

static const char* copy_format(DWORD DstFormat)
{
	switch (DstFormat)
	{
		case PIXEL_FORMAT_BGRA32:
			return "BGRA";
		case PIXEL_FORMAT_BGRX32:
			return "BGRX";
		case PIXEL_FORMAT_RGBA32:
			return "RGBA";
		case PIXEL_FORMAT_RGBX32:
			return "RGBX";
		default:
			return nullptr;
	}
}

INT32 av1_wc_decode(int handle, const BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData,
                    DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight)
{
	const char* format = copy_format(DstFormat);
	if (!format)
		return AV1_WC_UNSUPPORTED; /* copyTo() only writes 32 bit RGB: let dav1d handle it */
	if (emscripten_is_main_browser_thread())
		return -1; /* the main thread cannot block for its own asynchronous decode */

	const int profile = sequence_header_profile(pSrcData, SrcSize);
	av1_wc_job job = { .handle = handle,
		               .src = pSrcData,
		               .size = SrcSize,
		               .dst = pDstData,
		               .format = format,
		               .stride = nDstStep,
		               .width = nDstWidth,
		               .height = nDstHeight,
		               .key = profile >= 0,
		               .profile = profile,
		               .rc = -1 };
	if (emscripten_proxy_sync_with_ctx(emscripten_proxy_get_system_queue(),
	                                   emscripten_main_runtime_thread_id(), main_decode, &job) != 1)
		return -1;
	return job.rc;
}
