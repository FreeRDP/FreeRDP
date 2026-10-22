/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel: Web Audio backend for Emscripten builds
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

/* JavaScript side of rdpsnd_emscripten.c, linked with --js-library. Runs on the main browser
 * thread. */
function rdpsnd_web_play(data, size, channels, rate, volume, queued_ms)
{
	const ctx = (Module.freerdpAudioContext = Module.freerdpAudioContext || new AudioContext());
	if (ctx.state === "suspended")
		ctx.resume().catch(() => {});
	if (ctx.state !== "running")
	{
		/* currentTime does not advance: scheduling now would play everything at once later */
		HEAP32[queued_ms >>> 2] = 0;
		return;
	}
	const a = (Module.freerdpAudio = Module.freerdpAudio || { next: 0, gain: null });
	if (!a.gain)
	{
		a.gain = ctx.createGain();
		a.gain.connect(ctx.destination);
	}
	a.gain.gain.value = volume / 0xffff;

	const frames = (size / 2 / channels) | 0;
	if (frames <= 0)
		return;
	const pcm = new Int16Array(HEAPU8.slice(data, data + frames * channels * 2).buffer);
	const buffer = ctx.createBuffer(channels, frames, rate);
	for (let c = 0; c < channels; c++)
	{
		const out = buffer.getChannelData(c);
		for (let i = 0; i < frames; i++)
			out[i] = pcm[i * channels + c] / 32768;
	}
	const now = ctx.currentTime;
	/* Jitter buffer: after an underrun (or at start) begin 120 ms ahead, enough for internet
	 * jitter; if more than 500 ms piled up, drop the backlog so audio never lags further. */
	if (a.next < now || a.next > now + 0.5)
		a.next = now + 0.12;
	const src = ctx.createBufferSource();
	src.buffer = buffer;
	src.connect(a.gain);
	src.start(a.next);
	a.next += frames / rate;
	HEAP32[queued_ms >>> 2] = Math.round((a.next - now) * 1000);
}

addToLibrary({
	rdpsnd_web_play,
	rdpsnd_web_play__sig: "vpiiiip",
});
