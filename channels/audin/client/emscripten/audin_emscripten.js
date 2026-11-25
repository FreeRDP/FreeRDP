/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel: Web Audio backend for Emscripten builds
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

/* JavaScript side of audin_emscripten.c, linked with --js-library. Runs on the main browser
 * thread. */
function audin_web_start(id, capture, samples, channels, rate)
{
	const all = (Module.freerdpAudin = Module.freerdpAudin || {});
	const st = { stopped: false, stream: null, ctx: null, closed: null };
	const shutdown = () => {
		if (st.stream)
			st.stream.getTracks().forEach((t) => t.stop());
		st.stream = null;
		if (st.ctx)
			st.closed = st.ctx.close();
		st.ctx = null;
		return st.closed;
	};
	st.stop = () => {
		st.stopped = true;
		delete all[id];
		const release = () => _audin_emscripten_release(capture);
		const closed = shutdown();
		if (closed)
			closed.then(release, release);
		else
			release();
	};
	all[id] = st;

	(async () => {
		try
		{
			/* voice processing by default; a page can override single constraints */
			const audio = {
				channelCount: channels,
				echoCancellation: true,
				noiseSuppression: true,
				autoGainControl: true,
				...Module.freerdpAudinConstraints
			};
			const stream = await navigator.mediaDevices.getUserMedia({ audio: audio });
			if (st.stopped)
			{
				stream.getTracks().forEach((t) => t.stop());
				return;
			}
			st.stream = stream;
			/* capture at the negotiated rate so the browser resamples; not every browser can
			 * connect a microphone to a context of another rate, then the worklet resamples */
			let ctx, source;
			try
			{
				ctx = st.ctx = new AudioContext({ sampleRate: rate });
				source = ctx.createMediaStreamSource(stream);
			}
			catch (e)
			{
				if (ctx)
					ctx.close().catch(() => {});
				ctx = st.ctx = new AudioContext();
				source = ctx.createMediaStreamSource(stream);
			}
			await ctx.audioWorklet.addModule(Module.freerdpAudinWorkletUrl ||
			                                 locateFile("audin_emscripten_worklet.js"));
			if (st.stopped)
				return;
			const node = new AudioWorkletNode(ctx, "freerdp-audin-capture", {
				numberOfInputs: 1,
				numberOfOutputs: 1,
				outputChannelCount: [1],
				processorOptions: {
					memory: HEAPU8.buffer,
					capture: capture,
					samples: samples,
					channels: channels,
					rate: rate
				}
			});
			source.connect(node);
			/* the output is silent; connected so that the browser keeps pulling the node */
			node.connect(ctx.destination);
			if (ctx.state !== "running")
			{
				const resume = () => ctx.resume().catch(() => {});
				resume();
				for (const ev of ["pointerdown", "keydown"])
					window.addEventListener(ev, resume, { once: true, capture: true });
			}
		}
		catch (e)
		{
			if (st.stopped)
				return;
			err("[audin] microphone capture failed: " + e);
			Atomics.store(HEAP32, capture >>> 2, 0);
			Atomics.notify(HEAP32, (capture >>> 2) + 1);
			shutdown();
			if (typeof Module.onFreerdpMicrophoneError === "function")
				Module.onFreerdpMicrophoneError(e);
		}
	})();
}

function audin_web_stop(id, capture)
{
	const st = Module.freerdpAudin && Module.freerdpAudin[id];
	if (st)
		st.stop();
	else
		_audin_emscripten_release(capture);
}

/* No __deps on audin_emscripten_release: it is EMSCRIPTEN_KEEPALIVE, and a dependency would make
 * it internal to the library, so Module would no longer export it to the page. */
addToLibrary({
	audin_web_start,
	audin_web_start__sig: "vipiii",
	audin_web_stop,
	audin_web_stop__sig: "vip",
});
