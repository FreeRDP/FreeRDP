/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
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

#include <windows.h>
#include <objbase.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/codec/audio.h>
#include <freerdp/codec/dsp.h>

#include "win_shadow.h"
#include "win_audio.h"
#include "win_wasapi.h"

#define TAG SERVER_TAG("shadow.win")

/* What the clients get: 48 kHz, 2 channels, 16 bits. The build has no
 * resampler (SOXR or FFmpeg) unless asked for one, so every format offered
 * has the rate and channels of the captured samples, and the same frame
 * size: rdpsnd sizes its buffers from the first one. Opus first, so that a
 * client that has it takes it (96 kbit/s instead of 1.5 Mbit/s for PCM). */
static const AUDIO_FORMAT win_audio_offered_formats[] = {
	{ WAVE_FORMAT_OPUS, WIN_WASAPI_CHANNELS, WIN_WASAPI_RATE, 12000, 4, 16, 0, nullptr },
	{ WAVE_FORMAT_PCM, WIN_WASAPI_CHANNELS, WIN_WASAPI_RATE, WIN_WASAPI_RATE * 4, 4, 16, 0,
	  nullptr },
};

/* The format of the samples in each message: it outlives every client,
 * since rdpsnd keeps the pointer as its source format */
static AUDIO_FORMAT win_audio_source_format = {
	WAVE_FORMAT_PCM, WIN_WASAPI_CHANNELS, WIN_WASAPI_RATE, WIN_WASAPI_RATE * 4, 4, 16, 0, nullptr
};

static void win_audio_msg_free(UINT32 id, SHADOW_MSG_OUT* msg)
{
	WINPR_UNUSED(id);

	SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* samples = (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)msg;
	if (samples)
		free(samples->buf);
	free(samples);
}

/* Sends what s holds to every client, as one message */
static void win_audio_post(winShadowSubsystem* subsystem, wStream* s)
{
	WINPR_ASSERT(subsystem);

	const size_t size = Stream_GetPosition(s);
	Stream_ResetPosition(s);
	if (size == 0)
		return;

	rdpShadowServer* server = subsystem->base.server;
	if (!server || !server->clients || (ArrayList_Count(server->clients) == 0))
		return;

	SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* msg =
	    (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)calloc(1, sizeof(SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES));
	if (!msg)
		return;

	msg->buf = malloc(size);
	if (!msg->buf)
	{
		free(msg);
		return;
	}

	memcpy(msg->buf, Stream_Buffer(s), size);
	msg->common.Free = win_audio_msg_free;
	msg->audio_format = &win_audio_source_format;
	msg->nFrames = size / WIN_WASAPI_FRAME_BYTES;
	msg->wTimestamp = (UINT16)(GetTickCount64() & 0xFFFF);
	/* the message is freed with its last reference, also when no client took it */
	(void)shadow_client_boardcast_msg(server, nullptr, SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID,
	                                  &msg->common, nullptr);
}

static DWORD WINAPI win_audio_thread(LPVOID arg)
{
	winShadowSubsystem* subsystem = (winShadowSubsystem*)arg;
	WINPR_ASSERT(subsystem);

	const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		WLog_ERR(TAG, "audio: CoInitializeEx failed (0x%08lX): no sound", hr);
		return 1;
	}

	wStream* s = Stream_New(nullptr, 1ull * WIN_WASAPI_RATE * WIN_WASAPI_FRAME_BYTES / 10);
	BOOL log = TRUE;

	while (s && (WaitForSingleObject(subsystem->audioStopEvent, 0) == WAIT_TIMEOUT))
	{
		winWasapiLoopback* loopback = win_wasapi_loopback_open(log);
		if (!loopback)
		{
			/* tried again every 2 s, quietly: the audio service may start later
			 * than we do, a sound card may be plugged in */
			log = FALSE;
			(void)WaitForSingleObject(subsystem->audioStopEvent, 2000);
			continue;
		}

		/* until stopped, or the endpoint went away: then the default one again */
		log = TRUE;
		while (WaitForSingleObject(subsystem->audioStopEvent, 10) == WAIT_TIMEOUT)
		{
			const BOOL rc = win_wasapi_loopback_read(loopback, s);
			win_audio_post(subsystem, s);
			if (!rc)
				break;
		}
		win_wasapi_loopback_close(loopback);
	}

	Stream_Free(s, TRUE);
	CoUninitialize();
	return 0;
}

BOOL win_shadow_audio_init(winShadowSubsystem* subsystem)
{
	WINPR_ASSERT(subsystem);

	AUDIO_FORMAT* formats = audio_formats_new(ARRAYSIZE(win_audio_offered_formats));
	if (!formats)
		return FALSE;

	size_t count = 0;
	for (size_t x = 0; x < ARRAYSIZE(win_audio_offered_formats); x++)
	{
		const AUDIO_FORMAT* format = &win_audio_offered_formats[x];
		if (freerdp_dsp_supports_format(format, TRUE))
			formats[count++] = *format;
	}

	subsystem->base.rdpsndFormats = formats;
	subsystem->base.nRdpsndFormats = count;
	return TRUE;
}

void win_shadow_audio_uninit(winShadowSubsystem* subsystem)
{
	WINPR_ASSERT(subsystem);

	win_shadow_audio_stop(subsystem);
	audio_formats_free(subsystem->base.rdpsndFormats, subsystem->base.nRdpsndFormats);
	subsystem->base.rdpsndFormats = nullptr;
	subsystem->base.nRdpsndFormats = 0;
}

BOOL win_shadow_audio_start(winShadowSubsystem* subsystem)
{
	WINPR_ASSERT(subsystem);

	if (subsystem->audioThread)
		return TRUE;

	/* without its formats rdpsnd would expect 44.1 kHz from us */
	if (!subsystem->base.rdpsndFormats)
		return FALSE;

	subsystem->audioStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!subsystem->audioStopEvent)
		return FALSE;

	subsystem->audioThread = CreateThread(nullptr, 0, win_audio_thread, subsystem, 0, nullptr);
	if (!subsystem->audioThread)
	{
		WLog_ERR(TAG, "audio: failed to create the capture thread");
		(void)CloseHandle(subsystem->audioStopEvent);
		subsystem->audioStopEvent = nullptr;
		return FALSE;
	}

	return TRUE;
}

void win_shadow_audio_stop(winShadowSubsystem* subsystem)
{
	WINPR_ASSERT(subsystem);

	if (!subsystem->audioThread)
		return;

	(void)SetEvent(subsystem->audioStopEvent);
	(void)WaitForSingleObject(subsystem->audioThread, INFINITE);
	(void)CloseHandle(subsystem->audioThread);
	(void)CloseHandle(subsystem->audioStopEvent);
	subsystem->audioThread = nullptr;
	subsystem->audioStopEvent = nullptr;
}
