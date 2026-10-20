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

/*
 * The main browser thread asks for the microphone (getUserMedia) and feeds it through an
 * AudioWorklet. The worklet runs on the browser's audio thread: it converts every block to 16 bit
 * PCM at the negotiated rate and channel count and appends it to a ring buffer in the shared wasm
 * memory, then wakes the write position as a futex. A capture thread here waits on that futex and
 * hands packets of FramesPerPacket frames to audin, which sends them as they are or encodes them
 * (Opus, ...) for the server. Captured audio never waits for the busy main thread.
 *
 * Close() stops the worklet and the capture thread, then gives the ring buffer to the main thread,
 * which frees it once the AudioContext is closed and the worklet can no longer write to it.
 * Browsers only allow the microphone in a secure context after the user agreed, and only run an
 * AudioContext after the user interacted with the page. Echo cancellation, noise suppression and
 * automatic gain are on, as for a call; a page can set Module.freerdpAudinConstraints to other
 * MediaTrackConstraints, e.g. to record music unprocessed.
 */

#include <freerdp/config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/freerdp.h>
#include <freerdp/codec/audio.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"

/* about 1.4 s at 48 kHz stereo; must be a power of two */
#define AUDIN_WEB_RING_SAMPLES (1u << 17)

typedef struct
{
	volatile int32_t active;     /* 0: the worklet stops writing */
	volatile uint32_t write_pos; /* samples written so far (wraps), woken as a futex */
	int16_t ring[AUDIN_WEB_RING_SAMPLES];
} audin_web_capture;

typedef struct
{
	IAudinDevice iface;

	AUDIO_FORMAT format;
	UINT32 frames_per_packet;

	AudinReceive receive;
	void* user_data;

	audin_web_capture* capture;
	int capture_id;
	int16_t* packet;
	HANDLE thread;
	volatile int32_t stop;

	rdpContext* rdpcontext;
	wLog* log;
} AudinEmscriptenDevice;

typedef struct
{
	int id;
	audin_web_capture* capture;
	int channels;
	int rate;
} audin_web_job;

void audin_emscripten_release(void* capture);

/* Called by the main thread when the AudioContext of a capture is closed. */
EMSCRIPTEN_KEEPALIVE void audin_emscripten_release(void* capture)
{
	free(capture);
}

/* clang-format off */
EM_JS(void, audin_web_start, (int id, audin_web_capture* capture, int samples, int channels, int rate), {
	capture >>>= 0;
	const all = (Module.freerdpAudin = Module.freerdpAudin || {});
	const st = { stopped : false, stream : null, ctx : null };
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
	const worklet = "(" + (function() {
		class FreeRDPAudinCapture extends AudioWorkletProcessor {
			constructor(options) {
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
			read(input, i, out) {
				/* map the microphone channels to the negotiated ones */
				if (this.channels === 1 && input.length > 1)
					out[0] = (input[0][i] + input[1][i]) / 2;
				else
					for (let c = 0; c < this.channels; c++)
						out[c] = input[Math.min(c, input.length - 1)][i];
			}
			process(inputs) {
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
				while (t < n - 1) {
					const i = Math.floor(t);
					const f = t - i;
					if (i !== prev) {
						if (i < 0)
							a.set(this.last);
						else
							this.read(input, i, a);
						this.read(input, i + 1, cur);
						prev = i;
					}
					for (let c = 0; c < ch; c++) {
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
	}).toString() + ")();";

	(async () => {
		try {
			/* voice processing by default; a page can ask for other constraints */
			const audio = Module.freerdpAudinConstraints || {
				channelCount : channels, echoCancellation : true, noiseSuppression : true,
				autoGainControl : true
			};
			const stream = await navigator.mediaDevices.getUserMedia({ audio : audio });
			if (st.stopped) {
				stream.getTracks().forEach((t) => t.stop());
				return;
			}
			st.stream = stream;
			const ctx = (st.ctx = new AudioContext());
			if (!all.workletUrl)
				all.workletUrl = URL.createObjectURL(new Blob([worklet], { type : "text/javascript" }));
			await ctx.audioWorklet.addModule(all.workletUrl);
			if (st.stopped)
				return;
			const node = new AudioWorkletNode(ctx, "freerdp-audin-capture", {
				numberOfInputs : 1, numberOfOutputs : 1, outputChannelCount : [1],
				processorOptions : { memory : HEAPU8.buffer, capture : capture, samples : samples,
				                     channels : channels, rate : rate }
			});
			ctx.createMediaStreamSource(stream).connect(node);
			/* the output is silent; connected so that the browser keeps pulling the node */
			node.connect(ctx.destination);
			if (ctx.state !== "running") {
				const resume = () => ctx.resume().catch(() => {});
				resume();
				for (const ev of ["pointerdown", "keydown"])
					window.addEventListener(ev, resume, { once : true, capture : true });
			}
		} catch (e) {
			err("[audin] microphone capture failed: " + e);
		}
	})();
});

EM_JS(void, audin_web_stop, (int id, audin_web_capture* capture), {
	capture >>>= 0;
	const st = Module.freerdpAudin && Module.freerdpAudin[id];
	if (st)
		st.stop();
	else
		_audin_emscripten_release(capture);
});
/* clang-format on */

static void start_on_main(void* arg)
{
	audin_web_job* job = arg;
	audin_web_start(job->id, job->capture, (int)AUDIN_WEB_RING_SAMPLES, job->channels, job->rate);
	free(job);
}

static void stop_on_main(void* arg)
{
	audin_web_job* job = arg;
	audin_web_stop(job->id, job->capture);
	free(job);
}

static BOOL run_on_main(void (*fn)(void*), audin_web_job* job)
{
	if (emscripten_is_main_browser_thread())
	{
		fn(job);
		return TRUE;
	}
	return emscripten_proxy_async(emscripten_proxy_get_system_queue(),
	                              emscripten_main_runtime_thread_id(), fn, job);
}

static DWORD WINAPI audin_emscripten_thread(LPVOID arg)
{
	AudinEmscriptenDevice* dev = arg;
	audin_web_capture* capture = dev->capture;
	const uint32_t packet = dev->frames_per_packet * dev->format.nChannels;
	const size_t bytes = packet * sizeof(int16_t);
	uint32_t read_pos = 0;

	while (!__atomic_load_n(&dev->stop, __ATOMIC_ACQUIRE))
	{
		const uint32_t written = __atomic_load_n(&capture->write_pos, __ATOMIC_ACQUIRE);
		const uint32_t available = written - read_pos;

		if (available > AUDIN_WEB_RING_SAMPLES - packet)
		{
			/* the worklet overtook us: skip to the newest half of the ring */
			WLog_Print(dev->log, WLOG_WARN,
			           "microphone capture overrun, dropping %" PRIu32 " samples",
			           available - AUDIN_WEB_RING_SAMPLES / 2);
			read_pos = written - AUDIN_WEB_RING_SAMPLES / 2;
			continue;
		}

		if (available < packet)
		{
			(void)emscripten_futex_wait(&capture->write_pos, written, 100.0);
			continue;
		}

		for (uint32_t x = 0; x < packet; x++)
			dev->packet[x] = capture->ring[(read_pos + x) & (AUDIN_WEB_RING_SAMPLES - 1)];
		read_pos += packet;

		const UINT error =
		    dev->receive(&dev->format, (const BYTE*)dev->packet, bytes, dev->user_data);
		if (error)
		{
			if (dev->rdpcontext)
				setChannelError(dev->rdpcontext, error,
				                "audin_emscripten_thread reported an error");
			break;
		}
	}

	ExitThread(0);
	return 0;
}

static BOOL audin_emscripten_format_supported(WINPR_ATTR_UNUSED IAudinDevice* device,
                                              const AUDIO_FORMAT* format)
{
	/* the worklet converts to any rate: 16 bit PCM, mono or stereo */
	if (!format)
		return FALSE;
	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->wBitsPerSample == 16) &&
	       (format->nChannels >= 1) && (format->nChannels <= 2) &&
	       (format->nSamplesPerSec >= 8000) && (format->nSamplesPerSec <= 192000);
}

static UINT audin_emscripten_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                        UINT32 FramesPerPacket)
{
	AudinEmscriptenDevice* dev = (AudinEmscriptenDevice*)device;

	if (!dev || !audin_emscripten_format_supported(device, format))
		return ERROR_INVALID_PARAMETER;

	dev->format = *format;
	dev->format.cbSize = 0;
	dev->format.data = nullptr;

	/* default to 20 ms, and keep a packet well inside the ring */
	UINT32 frames = FramesPerPacket ? FramesPerPacket : format->nSamplesPerSec / 50;
	const UINT32 max_frames = AUDIN_WEB_RING_SAMPLES / 4 / format->nChannels;
	if (frames > max_frames)
		frames = max_frames;
	dev->frames_per_packet = frames;
	return CHANNEL_RC_OK;
}

static UINT audin_emscripten_close(IAudinDevice* device)
{
	AudinEmscriptenDevice* dev = (AudinEmscriptenDevice*)device;

	if (!dev)
		return ERROR_INVALID_PARAMETER;
	if (!dev->capture)
		return CHANNEL_RC_OK;

	__atomic_store_n(&dev->capture->active, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&dev->stop, 1, __ATOMIC_RELEASE);
	(void)emscripten_futex_wake(&dev->capture->write_pos, INT32_MAX);

	if (dev->thread)
	{
		(void)WaitForSingleObject(dev->thread, INFINITE);
		(void)CloseHandle(dev->thread);
		dev->thread = nullptr;
	}

	/* the main thread frees the ring once the worklet can no longer write to it */
	audin_web_job* job = calloc(1, sizeof(audin_web_job));
	if (job)
	{
		job->id = dev->capture_id;
		job->capture = dev->capture;
	}
	if (!job || !run_on_main(stop_on_main, job))
	{
		WLog_Print(dev->log, WLOG_ERROR, "could not stop the microphone capture, leaking it");
		free(job);
	}

	dev->capture = nullptr;
	free(dev->packet);
	dev->packet = nullptr;
	dev->receive = nullptr;
	dev->user_data = nullptr;
	return CHANNEL_RC_OK;
}

static UINT audin_emscripten_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	static int next_id = 0;
	AudinEmscriptenDevice* dev = (AudinEmscriptenDevice*)device;

	if (!dev || !receive || (dev->frames_per_packet == 0))
		return ERROR_INVALID_PARAMETER;

	UINT error = audin_emscripten_close(device);
	if (error)
		return error;

	dev->packet = calloc(1ull * dev->frames_per_packet * dev->format.nChannels, sizeof(int16_t));
	dev->capture = calloc(1, sizeof(audin_web_capture));
	audin_web_job* job = calloc(1, sizeof(audin_web_job));
	if (!dev->packet || !dev->capture || !job)
	{
		free(job);
		error = CHANNEL_RC_NO_MEMORY;
		goto fail;
	}

	dev->capture->active = 1;
	dev->capture_id = __atomic_add_fetch(&next_id, 1, __ATOMIC_RELAXED);
	dev->receive = receive;
	dev->user_data = user_data;
	__atomic_store_n(&dev->stop, 0, __ATOMIC_RELEASE);

	dev->thread = CreateThread(nullptr, 0, audin_emscripten_thread, dev, 0, nullptr);
	if (!dev->thread)
	{
		free(job);
		error = ERROR_INTERNAL_ERROR;
		goto fail;
	}

	job->id = dev->capture_id;
	job->capture = dev->capture;
	job->channels = dev->format.nChannels;
	job->rate = (int)dev->format.nSamplesPerSec;
	if (!run_on_main(start_on_main, job))
	{
		free(job);
		(void)audin_emscripten_close(device);
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;

fail:
	free(dev->packet);
	dev->packet = nullptr;
	free(dev->capture);
	dev->capture = nullptr;
	return error;
}

static UINT audin_emscripten_free(IAudinDevice* device)
{
	AudinEmscriptenDevice* dev = (AudinEmscriptenDevice*)device;

	if (!dev)
		return ERROR_INVALID_PARAMETER;

	const UINT error = audin_emscripten_close(device);
	free(dev);
	return error;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE emscripten_freerdp_audin_client_subsystem_entry(
    PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints))
{
	AudinEmscriptenDevice* dev = calloc(1, sizeof(AudinEmscriptenDevice));
	if (!dev)
		return CHANNEL_RC_NO_MEMORY;

	dev->log = WLog_Get(TAG);
	dev->rdpcontext = pEntryPoints->rdpcontext;
	dev->iface.Open = audin_emscripten_open;
	dev->iface.FormatSupported = audin_emscripten_format_supported;
	dev->iface.SetFormat = audin_emscripten_set_format;
	dev->iface.Close = audin_emscripten_close;
	dev->iface.Free = audin_emscripten_free;

	const UINT error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, &dev->iface);
	if (error)
	{
		WLog_Print(dev->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		free(dev);
	}
	return error;
}
