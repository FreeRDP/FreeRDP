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

/* JavaScript side of av1_webcodecs.c, linked with --js-library. Runs on the main browser thread. */
function av1_wc_js_available()
{
	return typeof VideoDecoder === "function" && typeof EncodedVideoChunk === "function" ? 1 : 0;
}

function av1_wc_js_open()
{
	const reg = (Module.freerdpAv1 = Module.freerdpAv1 || { next: 1, decoders: new Map() });
	const handle = reg.next++;
	reg.decoders.set(handle, { decoder: null, profile: -1, pending: null });
	return handle;
}

function av1_wc_js_close(handle)
{
	const reg = Module.freerdpAv1;
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
	reg.decoders.delete(handle);
}

/* Stores the result in *rc and completes asynchronously through _freerdp_av1_wc_finish. */
function av1_wc_js_decode(ctx, handle, src, size, dst, formatName, stride, width, height, key,
                          profile, rc)
{
	size >>>= 0;
	stride >>>= 0;
	width >>>= 0;
	height >>>= 0;
	const format = UTF8ToString(formatName);
	const finish = (r) => {
		HEAP32[rc >>> 2] = r;
		_freerdp_av1_wc_finish(ctx);
	};
	const d = Module.freerdpAv1?.decoders.get(handle);
	if (!d)
		return finish(-1);

	/* (re)configure on key frames that carry a different profile (4:2:0 = 0, 4:4:4 = 1) */
	if (!d.decoder || d.decoder.state === "closed" ||
	    (key && profile >= 0 && profile !== d.profile))
	{
		if (!key)
			return finish(0); /* WebCodecs must start with a key frame: drop until one arrives */
		try
		{
			d.decoder?.close();
		}
		catch (e)
		{
		}
		d.profile = profile;
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
			    const w = Math.min(frame.displayWidth, p.width);
			    const h = Math.min(frame.displayHeight, p.height);
			    const view = HEAPU8.subarray(p.dst, p.dst + p.stride * h);
			    frame
			        .copyTo(view, {
				        rect: { x: 0, y: 0, width: w, height: h },
				        layout: [{ offset: 0, stride: p.stride }],
				        format: p.format,
			        })
			        .then(
			            () => {
				            frame.close();
				            p.finish(1);
			            },
			            (e) => {
				            frame.close();
				            console.error("[av1-webcodecs] copyTo", e);
				            p.finish(-3);
			            });
			},
			error: (e) => {
			    console.error("[av1-webcodecs] decoder error", e);
			    const p = d.pending;
			    d.pending = null;
			    if (p)
			    {
				    clearTimeout(p.timer);
				    p.finish(-2);
			    }
			},
		});
		d.decoder.configure({
			codec: profile === 1 ? "av01.1.13M.08" : "av01.0.13M.08",
			optimizeForLatency: true,
			hardwareAcceleration: "no-preference",
		});
	}

	d.pending = {
		dst,
		format,
		stride,
		width,
		height,
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
		    1000),
	};
	try
	{
		const data = HEAPU8.slice(src, src + size); /* EncodedVideoChunk copies; avoid SAB views */
		d.decoder.decode(
		    new EncodedVideoChunk({ type: key ? "key" : "delta", timestamp: 0, data }));
	}
	catch (e)
	{
		console.error("[av1-webcodecs] decode", e);
		clearTimeout(d.pending.timer);
		d.pending = null;
		finish(-2);
	}
}

addToLibrary({
	av1_wc_js_available,
	av1_wc_js_available__sig: "i",
	av1_wc_js_open,
	av1_wc_js_open__sig: "i",
	av1_wc_js_close,
	av1_wc_js_close__sig: "vi",
	av1_wc_js_decode,
	av1_wc_js_decode__sig: "vpipippiiiiip",
	av1_wc_js_decode__deps: ["$UTF8ToString", "freerdp_av1_wc_finish"],
});
