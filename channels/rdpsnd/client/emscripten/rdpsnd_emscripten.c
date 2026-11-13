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

/*
 * Play() runs on the audio channel thread. It copies the PCM samples and hands them to the
 * main browser thread asynchronously, so it never waits for the busy UI thread. The main thread
 * converts them to float and schedules them back to back with AudioBufferSourceNode.start(),
 * and keeps the amount of queued audio (ms) in shared memory, which Play() reports to the server
 * as latency. Browsers only allow audio after a user gesture: a page that creates and resumes
 * Module.freerdpAudioContext in its connect click handler gets sound at once. Otherwise the backend
 * creates the context itself, and samples are dropped until the user interacts with the page.
 */

#include <freerdp/config.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/types.h>
#include <freerdp/codec/audio.h>

#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;
	UINT32 channels;
	UINT32 rate;
	UINT32 volume;
	volatile int queued_ms; /* written by the main thread */
	AUDIO_FORMAT asked;     /* the last format passed to FormatSupported() */
} rdpsndEmscriptenPlugin;

/*
 * The format the server sends on the wire, for the web page's Info panel:
 * [0] wFormatTag (WAVE_FORMAT_UNKNOWN = no audio yet), [1] nSamplesPerSec, [2] nChannels.
 *
 * Open() only sees the PCM format the device plays: for Opus, AAC or ADPCM rdpsnd opens the
 * device with a PCM copy of the wire format and decodes with freerdp_dsp. The wire format is
 * what rdpsnd asked FormatSupported() about right before Open() (rdpsnd_ensure_device_is_open),
 * and the PCM copy keeps its rate and channels. Open() takes the asked format only when rate
 * and channels match, and otherwise publishes the format it opened with.
 */
static uint32_t web_audio_format[3];

EMSCRIPTEN_KEEPALIVE uint32_t* freerdp_web_audio_format(void);
EMSCRIPTEN_KEEPALIVE uint32_t* freerdp_web_audio_format(void)
{
	return web_audio_format;
}

static void web_audio_format_set(UINT16 tag, UINT32 rate, UINT16 channels)
{
	__atomic_store_n(&web_audio_format[1], rate, __ATOMIC_RELAXED);
	__atomic_store_n(&web_audio_format[2], channels, __ATOMIC_RELAXED);
	__atomic_store_n(&web_audio_format[0], tag, __ATOMIC_RELEASE);
}

typedef struct
{
	rdpsndEmscriptenPlugin* plugin;
	BYTE* data;
	size_t size;
	UINT32 channels; /* the format at Play(): Open() may change it before the job runs */
	UINT32 rate;
} play_job;

/* in rdpsnd_emscripten.js */
extern void rdpsnd_web_play(BYTE* data, int size, int channels, int rate, int volume,
                            int* queued_ms);

static void play_on_main(void* arg)
{
	play_job* job = arg;
	WINPR_ASSERT(job);

	rdpsndEmscriptenPlugin* p = job->plugin;
	WINPR_ASSERT(p);

	rdpsnd_web_play(job->data, (int)job->size, (int)job->channels, (int)job->rate,
	                (int)(p->volume & 0xffff), (int*)&p->queued_ms);
	free(job->data);
	free(job);
}

WINPR_ATTR_NODISCARD
static BOOL pcm_playable(const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(format);

	/* Web Audio takes float samples at any rate: accept 16 bit PCM, mono or stereo */
	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->wBitsPerSample == 16) &&
	       (format->nChannels >= 1) && (format->nChannels <= 2) && (format->nSamplesPerSec > 0);
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_emscripten_format_supported(rdpsndDevicePlugin* device,
                                               const AUDIO_FORMAT* format)
{
	rdpsndEmscriptenPlugin* p = (rdpsndEmscriptenPlugin*)device;
	WINPR_ASSERT(p);
	WINPR_ASSERT(format);

	p->asked = *format;
	p->asked.data = nullptr;
	return pcm_playable(format);
}

static BOOL rdpsnd_emscripten_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                   WINPR_ATTR_UNUSED UINT32 latency)
{
	rdpsndEmscriptenPlugin* p = (rdpsndEmscriptenPlugin*)device;
	WINPR_ASSERT(p);

	if (!format || !pcm_playable(format))
		return FALSE;
	p->channels = format->nChannels;
	p->rate = format->nSamplesPerSec;

	const AUDIO_FORMAT* wire = format;
	if ((p->asked.wFormatTag != WAVE_FORMAT_UNKNOWN) &&
	    (p->asked.nSamplesPerSec == format->nSamplesPerSec) &&
	    (p->asked.nChannels == format->nChannels))
		wire = &p->asked;
	web_audio_format_set(wire->wFormatTag, wire->nSamplesPerSec, wire->nChannels);
	return TRUE;
}

static void rdpsnd_emscripten_close(WINPR_ATTR_UNUSED rdpsndDevicePlugin* device)
{
}

static BOOL rdpsnd_emscripten_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndEmscriptenPlugin* p = (rdpsndEmscriptenPlugin*)device;
	WINPR_ASSERT(p);

	p->volume = value;
	return TRUE;
}

static UINT32 rdpsnd_emscripten_get_volume(rdpsndDevicePlugin* device)
{
	const rdpsndEmscriptenPlugin* p = (const rdpsndEmscriptenPlugin*)device;
	WINPR_ASSERT(p);

	return p->volume;
}

static UINT rdpsnd_emscripten_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	rdpsndEmscriptenPlugin* p = (rdpsndEmscriptenPlugin*)device;
	WINPR_ASSERT(p);

	if (!data || size == 0 || p->channels == 0)
		return 0;

	play_job* job = calloc(1, sizeof(play_job));
	if (!job)
		return 0;
	job->data = malloc(size);
	if (!job->data)
	{
		free(job);
		return 0;
	}
	memcpy(job->data, data, size);
	job->plugin = p;
	job->size = size;
	job->channels = p->channels;
	job->rate = p->rate;

	if (emscripten_is_main_browser_thread())
		play_on_main(job);
	else if (!emscripten_proxy_async(emscripten_proxy_get_system_queue(),
	                                 emscripten_main_runtime_thread_id(), play_on_main, job))
	{
		free(job->data);
		free(job);
		return 0;
	}
	/* latency reported to the server: what is already queued in the browser */
	return (UINT)p->queued_ms;
}

static void drain_on_main(WINPR_ATTR_UNUSED void* arg)
{
}

static void rdpsnd_emscripten_free(rdpsndDevicePlugin* device)
{
	/* Jobs queued by Play() still use the plugin. The main thread runs its queue in order, so
	 * once a synchronous no-op went through, all of them are done. */
	if (emscripten_is_main_browser_thread())
		emscripten_proxy_execute_queue(emscripten_proxy_get_system_queue());
	else
		(void)emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
		                            emscripten_main_runtime_thread_id(), drain_on_main, nullptr);
	free(device);
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE emscripten_freerdp_rdpsnd_client_subsystem_entry(
    PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);

	rdpsndEmscriptenPlugin* p = calloc(1, sizeof(rdpsndEmscriptenPlugin));
	if (!p)
		return CHANNEL_RC_NO_MEMORY;

	p->volume = UINT32_MAX;
	web_audio_format_set(WAVE_FORMAT_UNKNOWN, 0, 0); /* a new session: no audio yet */
	p->device.Open = rdpsnd_emscripten_open;
	p->device.FormatSupported = rdpsnd_emscripten_format_supported;
	p->device.GetVolume = rdpsnd_emscripten_get_volume;
	p->device.SetVolume = rdpsnd_emscripten_set_volume;
	p->device.Play = rdpsnd_emscripten_play;
	p->device.Close = rdpsnd_emscripten_close;
	p->device.Free = rdpsnd_emscripten_free;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, &p->device);
	return CHANNEL_RC_OK;
}
