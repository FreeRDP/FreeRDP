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
	const a =
	    (Module.freerdpAudio = Module.freerdpAudio || { next: 0, gain: null, sources: new Set() });
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
	/* Jitter buffer: after an underrun (or at start) begin startMs ahead, enough for internet
	 * jitter; if more than maxMs piled up, drop the backlog so audio never lags further. A page
	 * can tune both with Module.freerdpAudioLatency = { startMs, maxMs }. */
	const lat = Module.freerdpAudioLatency || {};
	const start = (lat.startMs > 0 ? lat.startMs : 120) / 1000;
	const max = Math.max((lat.maxMs > 0 ? lat.maxMs : 500) / 1000, start);
	const now = ctx.currentTime;
	if (a.next > now + max)
	{
		/* the dropped backlog is still scheduled and would play over the new audio */
		for (const s of a.sources)
			s.stop();
		a.sources.clear();
	}
	if (a.next < now || a.next > now + max)
		a.next = now + start;
	const src = ctx.createBufferSource();
	src.buffer = buffer;
	src.connect(a.gain);
	src.onended = () => a.sources.delete(src);
	a.sources.add(src);
	src.start(a.next);
	a.next += frames / rate;
	HEAP32[queued_ms >>> 2] = Math.round((a.next - now) * 1000);
}

addToLibrary({
	rdpsnd_web_play,
	rdpsnd_web_play__sig: "vpiiiip",
});
