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

/* JavaScript side of h264_webcodecs.c, linked with --js-library. Runs on the main browser
 * thread. */

/* Copies frame into the I420 planes t (y, u, v, ys, cs); resolves with 1, or -4 for an
 * unsupported pixel format. */
function h264_wc_copy(frame, t)
{
	const w = frame.visibleRect.width, h = frame.visibleRect.height;
	const cw = (w + 1) >> 1, ch = (h + 1) >> 1;
	if (frame.format === "I420")
	{
		const view = HEAPU8.subarray(t.y, t.v + t.cs * ch);
		return frame
		    .copyTo(view, {
			    layout: [
				    { offset: 0, stride: t.ys }, { offset: t.u - t.y, stride: t.cs },
				    { offset: t.v - t.y, stride: t.cs }
			    ]
		    })
		    .then(() => 1);
	}
	if (frame.format !== "NV12")
		return Promise.resolve(-4);
	/* hardware decoders return NV12: split the interleaved chroma plane */
	const tmp = new Uint8Array(frame.allocationSize());
	return frame.copyTo(tmp).then((layout) => {
		const heap = HEAPU8;
		for (let r = 0; r < h; r++)
		{
			const s = layout[0].offset + r * layout[0].stride;
			heap.set(tmp.subarray(s, s + w), t.y + r * t.ys);
		}
		for (let r = 0; r < ch; r++)
		{
			const s = layout[1].offset + r * layout[1].stride, du = t.u + r * t.cs,
			      dv = t.v + r * t.cs;
			for (let x = 0; x < cw; x++)
			{
				heap[du + x] = tmp[s + 2 * x];
				heap[dv + x] = tmp[s + 2 * x + 1];
			}
		}
		return 1;
	});
}

/* Opens a decoder slot once the browser has confirmed it can decode H.264 at all. */
function h264_wc_js_open(ctx, handle)
{
	const reg = (Module.freerdpH264 =
	                 Module.freerdpH264 || { next: 1, decoders: new Map(), supported: null });
	const finish = (ok) => {
		reg.supported = ok;
		HEAP32[handle >>> 2] = 0;
		if (ok)
		{
			const h = reg.next++;
			reg.decoders.set(h, { decoder: null, pending: null, frame: null, ts: 0 });
			HEAP32[handle >>> 2] = h;
		}
		_freerdp_h264_wc_finish(ctx);
	};
	if (reg.supported !== null)
		return finish(reg.supported);
	if (typeof VideoDecoder !== "function" || typeof EncodedVideoChunk !== "function")
		return finish(false);
	/* Constrained Baseline 3.1: any H.264 capable browser decodes it */
	VideoDecoder.isConfigSupported({ codec: "avc1.42e01f" })
	    .then((s) => finish(!!s.supported), () => finish(false));
}

function h264_wc_js_close(handle)
{
	const reg = Module.freerdpH264;
	const d = reg?.decoders.get(handle);
	if (!d)
		return;
	try
	{
		d.decoder?.close();
	}
	catch (e)
	{
	}
	d.frame?.close();
	reg.decoders.delete(handle);
}

/* Stores the result in *rc and completes asynchronously through _freerdp_h264_wc_finish. */
function h264_wc_js_decode(ctx, handle, src, size, key, codec, y, u, v, ys, cs, maxWidth, maxHeight,
                           dim, rc)
{
	size >>>= 0;
	ys >>>= 0;
	cs >>>= 0;
	maxWidth >>>= 0;
	maxHeight >>>= 0;
	const finish = (r) => {
		HEAP32[rc >>> 2] = r;
		_freerdp_h264_wc_finish(ctx);
	};
	const reg = Module.freerdpH264;
	const d = reg?.decoders.get(handle);
	if (!d)
		return finish(-1);

	if (!d.decoder || d.decoder.state === "closed")
	{
		if (!key)
			return finish(0); /* WebCodecs must start with a key frame: drop until one arrives */
		d.decoder = new VideoDecoder({
			output: (frame) => {
			    const p = d.pending;
			    d.pending = null;
			    if (!p)
			    {
				    frame.close();
				    return;
			    }
			    clearTimeout(p.timer);
			    const w = frame.visibleRect.width, h = frame.visibleRect.height;
			    HEAP32[p.dim >>> 2] = w;
			    HEAP32[(p.dim >>> 2) + 1] = h;
			    if (w > p.maxWidth || h > p.maxHeight)
			    {
				    d.frame?.close();
				    d.frame = frame; /* kept for h264_wc_js_copy once the planes are larger */
				    return p.finish(2);
			    }
			    Promise.resolve()
			        .then(() => h264_wc_copy(frame, p))
			        .then(
			            (r) => {
				            frame.close();
				            p.finish(r);
			            },
			            (e) => {
				            frame.close();
				            console.error("[h264-webcodecs] copyTo", e);
				            p.finish(-3);
			            });
			},
			error: (e) => {
			    console.error("[h264-webcodecs] decoder error", e);
			    const p = d.pending;
			    d.pending = null;
			    if (p)
			    {
				    clearTimeout(p.timer);
				    p.finish(-2);
			    }
			}
		});
		try
		{
			/* no description: the stream is Annex B with in-band SPS/PPS */
			d.decoder.configure({
				codec: UTF8ToString(codec),
				optimizeForLatency: true,
				hardwareAcceleration: "no-preference"
			});
		}
		catch (e)
		{
			console.error("[h264-webcodecs] configure", e);
			d.decoder = null;
			return finish(-2);
		}
	}

	d.pending = {
		y,
		u,
		v,
		ys,
		cs,
		maxWidth,
		maxHeight,
		dim,
		finish,
		/* low-latency streams release each frame at once; never hang the worker if not */
		timer: setTimeout(
		    () => {
		        if (d.pending)
		        {
			        d.pending = null;
			        finish(0);
		        }
		    },
		    1000)
	};
	try
	{
		const data = HEAPU8.slice(src, src + size); /* EncodedVideoChunk copies; avoid SAB views */
		d.decoder.decode(
		    new EncodedVideoChunk({ type: key ? "key" : "delta", timestamp: d.ts++, data }));
	}
	catch (e)
	{
		console.error("[h264-webcodecs] decode", e);
		clearTimeout(d.pending.timer);
		d.pending = null;
		finish(-2);
	}
}

/* Copies the frame kept by h264_wc_js_decode after the planes were enlarged. */
function h264_wc_js_copy(ctx, handle, y, u, v, ys, cs, rc)
{
	ys >>>= 0;
	cs >>>= 0;
	const finish = (r) => {
		HEAP32[rc >>> 2] = r;
		_freerdp_h264_wc_finish(ctx);
	};
	const reg = Module.freerdpH264;
	const d = reg?.decoders.get(handle);
	const frame = d?.frame;
	if (!frame)
		return finish(-1);
	d.frame = null;
	Promise.resolve()
	    .then(() => h264_wc_copy(frame, { y, u, v, ys, cs }))
	    .then(
	        (r) => {
		        frame.close();
		        finish(r);
	        },
	        (e) => {
		        frame.close();
		        console.error("[h264-webcodecs] copyTo", e);
		        finish(-3);
	        });
}

addToLibrary({
	$h264_wc_copy: h264_wc_copy,
	h264_wc_js_open,
	h264_wc_js_open__sig: "vpp",
	h264_wc_js_open__deps: ["freerdp_h264_wc_finish"],
	h264_wc_js_close,
	h264_wc_js_close__sig: "vi",
	h264_wc_js_decode,
	h264_wc_js_decode__sig: "vpipiippppiiiipp",
	h264_wc_js_decode__deps: ["$h264_wc_copy", "$UTF8ToString", "freerdp_h264_wc_finish"],
	h264_wc_js_copy,
	h264_wc_js_copy__sig: "vpipppiip",
	h264_wc_js_copy__deps: ["$h264_wc_copy", "freerdp_h264_wc_finish"],
});
