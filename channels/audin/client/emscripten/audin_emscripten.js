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
	const st = { stopped: false, stream: null, ctx: null };
	st.stop = () => {
		st.stopped = true;
		delete all[id];
		if (st.stream)
			st.stream.getTracks().forEach((t) => t.stop());
		const release = () => _audin_emscripten_release(capture);
		if (st.ctx)
			st.ctx.close().then(release, release);
		else
			release();
	};
	all[id] = st;

	/* The worklet code: runs on the audio thread, in its own global scope. */
	function worklet_main()
	{
		class FreeRDPAudinCapture extends AudioWorkletProcessor
		{
			constructor(options)
			{
				super();
				const o = options.processorOptions;
				this.words = new Int32Array(o.memory, o.capture, 2);
				this.ring = new Int16Array(o.memory, o.capture + 8, o.samples);
				this.mask = o.samples - 1;
				this.channels = o.channels;
				this.step = sampleRate / o.rate; /* input frames per output frame */
				this.t = 0;                      /* next output position in the input block */
				this.last = new Float32Array(o.channels);
				this.frame = new Float32Array(o.channels);
				this.before = new Float32Array(o.channels);
			}
			read(input, i, out)
			{
				/* map the microphone channels to the negotiated ones */
				if (this.channels === 1 && input.length > 1)
					out[0] = (input[0][i] + input[1][i]) / 2;
				else
					for (let c = 0; c < this.channels; c++)
						out[c] = input[Math.min(c, input.length - 1)][i];
			}
			process(inputs)
			{
				if (Atomics.load(this.words, 0) === 0)
					return false;
				const input = inputs[0];
				if (!input || input.length === 0)
					return true;
				const n = input[0].length;
				const ch = this.channels;
				const cur = this.frame;
				const a = this.before;
				let pos = Atomics.load(this.words, 1);
				/* linear interpolation between this.last (position -1) and the block */
				let t = this.t;
				let prev = -2;
				while (t < n - 1)
				{
					const i = Math.floor(t);
					const f = t - i;
					if (i !== prev)
					{
						if (i < 0)
							a.set(this.last);
						else
							this.read(input, i, a);
						this.read(input, i + 1, cur);
						prev = i;
					}
					for (let c = 0; c < ch; c++)
					{
						let v = a[c] + (cur[c] - a[c]) * f;
						v = v < -1 ? -1 : v > 1 ? 1 : v;
						this.ring[pos & this.mask] = v < 0 ? v * 32768 : v * 32767;
						pos++;
					}
					t += this.step;
				}
				this.t = t - n;
				this.read(input, n - 1, this.last);
				Atomics.store(this.words, 1, pos | 0);
				Atomics.notify(this.words, 1);
				return true;
			}
		}
		registerProcessor("freerdp-audin-capture", FreeRDPAudinCapture);
	}
	const worklet = "(" + worklet_main.toString() + ")();";

	(async () => {
		try
		{
			/* voice processing by default; a page can ask for other constraints */
			const audio = Module.freerdpAudinConstraints || {
				channelCount: channels,
				echoCancellation: true,
				noiseSuppression: true,
				autoGainControl: true
			};
			const stream = await navigator.mediaDevices.getUserMedia({ audio: audio });
			if (st.stopped)
			{
				stream.getTracks().forEach((t) => t.stop());
				return;
			}
			st.stream = stream;
			const ctx = (st.ctx = new AudioContext());
			if (!all.workletUrl)
				all.workletUrl =
				    URL.createObjectURL(new Blob([worklet], { type: "text/javascript" }));
			await ctx.audioWorklet.addModule(all.workletUrl);
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
			ctx.createMediaStreamSource(stream).connect(node);
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
			err("[audin] microphone capture failed: " + e);
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
