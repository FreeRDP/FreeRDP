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
