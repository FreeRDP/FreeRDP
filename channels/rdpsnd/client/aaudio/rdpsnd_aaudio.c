/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel - AAudio backend
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/synch.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include <aaudio/AAudio.h>

#include "rdpsnd_main.h"

/* Buffer cushion (ms) used when the server requests no explicit latency. */
#define AAUDIO_DEFAULT_LATENCY_MS 100

/* Short write timeout (10ms) to avoid stalling virtual channel playback thread under asyncLock. */
#define AAUDIO_WRITE_TIMEOUT_NS (10LL * 1000LL * 1000LL)

#define AAUDIO_MAX_RATE 48000

typedef struct
{
	rdpsndDevicePlugin device;

	AAudioStream* stream;

	UINT32 rate;
	UINT32 channels;
	UINT32 latency;

	UINT32 volume;
	BOOL opened;

	short* gainbuf;
	size_t gainbuf_len;

	CRITICAL_SECTION lock;
} rdpsndAAudioPlugin;

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_pcm_valid(const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(format);

	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->cbSize == 0) &&
	       (format->wBitsPerSample == 16) && (format->nSamplesPerSec > 0) &&
	       (format->nSamplesPerSec <= AAUDIO_MAX_RATE) &&
	       ((format->nChannels == 1) || (format->nChannels == 2));
}

static void rdpsnd_aaudio_close_stream(rdpsndAAudioPlugin* aaudio)
{
	WINPR_ASSERT(aaudio);

	if (!aaudio->stream)
		return;

	aaudio_result_t rc = AAudioStream_requestStop(aaudio->stream);
	if (rc != AAUDIO_OK)
		WLog_WARN(TAG, "AAudioStream_requestStop: %s", AAudio_convertResultToText(rc));
	rc = AAudioStream_close(aaudio->stream);
	if (rc != AAUDIO_OK)
		WLog_WARN(TAG, "AAudioStream_close: %s", AAudio_convertResultToText(rc));
	aaudio->stream = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_open_stream(rdpsndAAudioPlugin* aaudio)
{
	WINPR_ASSERT(aaudio);

	AAudioStreamBuilder* builder = nullptr;

	if (aaudio->stream)
		return TRUE;

	aaudio_result_t rc = AAudio_createStreamBuilder(&builder);
	if (rc != AAUDIO_OK)
	{
		WLog_ERR(TAG, "AAudio_createStreamBuilder: %s", AAudio_convertResultToText(rc));
		return FALSE;
	}

	AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
	AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
	AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
	AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setSampleRate(builder, WINPR_ASSERTING_INT_CAST(int32_t, aaudio->rate));
	AAudioStreamBuilder_setChannelCount(builder,
	                                    WINPR_ASSERTING_INT_CAST(int32_t, aaudio->channels));
#if __ANDROID_API__ >= 28
	AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
	AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
#endif

	const UINT32 targetMs = (aaudio->latency > 0) ? aaudio->latency : AAUDIO_DEFAULT_LATENCY_MS;
	const UINT64 targetFrames = (UINT64)targetMs * aaudio->rate / 1000;
	const int32_t targetCapacity =
	    (targetFrames > INT32_MAX) ? INT32_MAX : WINPR_ASSERTING_INT_CAST(int32_t, targetFrames);
	AAudioStreamBuilder_setBufferCapacityInFrames(builder, targetCapacity);

	rc = AAudioStreamBuilder_openStream(builder, &aaudio->stream);
	AAudioStreamBuilder_delete(builder);

	if (rc != AAUDIO_OK)
	{
		WLog_ERR(TAG, "AAudioStreamBuilder_openStream: %s", AAudio_convertResultToText(rc));
		aaudio->stream = nullptr;
		return FALSE;
	}

	const int32_t actualRate = AAudioStream_getSampleRate(aaudio->stream);
	const int32_t actualChannels = AAudioStream_getChannelCount(aaudio->stream);
	const aaudio_format_t actualFormat = AAudioStream_getFormat(aaudio->stream);

	/* The frame size comes from the negotiated format and nothing converts here, so a stream
	 * with a different channel count or sample format would have AAudioStream_write read past
	 * the end of the caller's buffer. */
	if ((actualChannels < 0) || ((UINT32)actualChannels != aaudio->channels) ||
	    (actualFormat != AAUDIO_FORMAT_PCM_I16))
	{
		WLog_ERR(TAG, "AAudio opened as %dch/fmt%d, need %" PRIu32 "ch/I16", actualChannels,
		         (int)actualFormat, aaudio->channels);
		rdpsnd_aaudio_close_stream(aaudio);
		return FALSE;
	}

	if ((actualRate < 0) || ((UINT32)actualRate != aaudio->rate))
	{
		WLog_WARN(TAG, "AAudio rate adjusted: requested %" PRIu32 "Hz, got %dHz", aaudio->rate,
		          actualRate);
	}

	/* fixed buffer cushion */
	const int32_t burst = AAudioStream_getFramesPerBurst(aaudio->stream);
	const int32_t capacity = AAudioStream_getBufferCapacityInFrames(aaudio->stream);
	int32_t target = targetCapacity;

	if ((burst > 0) && (burst <= INT32_MAX / 2) && (target < burst * 2))
		target = burst * 2;
	if ((capacity > 0) && (target > capacity))
		target = capacity;

	const aaudio_result_t size = AAudioStream_setBufferSizeInFrames(aaudio->stream, target);
	if (size < 0)
		WLog_WARN(TAG, "AAudioStream_setBufferSizeInFrames(%d): %s", target,
		          AAudio_convertResultToText(size));

	rc = AAudioStream_requestStart(aaudio->stream);
	if (rc != AAUDIO_OK)
	{
		WLog_ERR(TAG, "AAudioStream_requestStart: %s", AAudio_convertResultToText(rc));
		rdpsnd_aaudio_close_stream(aaudio);
		return FALSE;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(device);
	WINPR_ASSERT(format);

	return rdpsnd_aaudio_pcm_valid(format);
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_default_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* desired,
                                         AUDIO_FORMAT* defaultFormat)
{
	WINPR_ASSERT(device);
	WINPR_ASSERT(defaultFormat);

	if (desired)
		*defaultFormat = *desired;

	defaultFormat->wFormatTag = WAVE_FORMAT_PCM;
	defaultFormat->wBitsPerSample = 16;
	defaultFormat->cbSize = 0;
	defaultFormat->data = nullptr;

	if ((defaultFormat->nChannels != 1) && (defaultFormat->nChannels != 2))
		defaultFormat->nChannels = 2;
	if ((defaultFormat->nSamplesPerSec == 0) || (defaultFormat->nSamplesPerSec > AAUDIO_MAX_RATE))
		defaultFormat->nSamplesPerSec = AAUDIO_MAX_RATE;

	defaultFormat->nBlockAlign = defaultFormat->nChannels * defaultFormat->wBitsPerSample / 8;
	defaultFormat->nAvgBytesPerSec = defaultFormat->nBlockAlign * defaultFormat->nSamplesPerSec;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                               UINT32 latency)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	WINPR_ASSERT(aaudio);

	if (format && !rdpsnd_aaudio_pcm_valid(format))
	{
		WLog_ERR(TAG, "unsupported format %s", audio_format_get_tag_string(format->wFormatTag));
		return FALSE;
	}

	EnterCriticalSection(&aaudio->lock);
	if (format)
	{
		/* A live stream from a previous format must not swallow the new one. */
		if (aaudio->stream &&
		    ((aaudio->rate != format->nSamplesPerSec) || (aaudio->channels != format->nChannels)))
			rdpsnd_aaudio_close_stream(aaudio);

		aaudio->rate = format->nSamplesPerSec;
		aaudio->channels = format->nChannels;
	}

	aaudio->latency = latency;
	const BOOL rc = rdpsnd_aaudio_open_stream(aaudio);
	aaudio->opened = rc;
	LeaveCriticalSection(&aaudio->lock);
	return rc;
}

static void rdpsnd_aaudio_close(rdpsndDevicePlugin* device)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	WINPR_ASSERT(aaudio);
	EnterCriticalSection(&aaudio->lock);
	aaudio->opened = FALSE;
	rdpsnd_aaudio_close_stream(aaudio);
	LeaveCriticalSection(&aaudio->lock);
}

static void rdpsnd_aaudio_free(rdpsndDevicePlugin* device)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	if (!aaudio)
		return;

	EnterCriticalSection(&aaudio->lock);
	aaudio->opened = FALSE;
	rdpsnd_aaudio_close_stream(aaudio);
	free(aaudio->gainbuf);
	aaudio->gainbuf = nullptr;
	aaudio->gainbuf_len = 0;
	LeaveCriticalSection(&aaudio->lock);

	DeleteCriticalSection(&aaudio->lock);
	free(aaudio);
}

WINPR_ATTR_NODISCARD
static UINT32 rdpsnd_aaudio_get_volume(rdpsndDevicePlugin* device)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	WINPR_ASSERT(aaudio);
	EnterCriticalSection(&aaudio->lock);
	const UINT32 volume = aaudio->volume;
	LeaveCriticalSection(&aaudio->lock);
	return volume;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_aaudio_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	WINPR_ASSERT(aaudio);
	EnterCriticalSection(&aaudio->lock);
	aaudio->volume = value;
	LeaveCriticalSection(&aaudio->lock);
	return TRUE;
}

/* NDK has no per-stream volume, so scale in software (into gainbuf). */
WINPR_ATTR_NODISCARD
static const short* rdpsnd_aaudio_apply_volume(rdpsndAAudioPlugin* aaudio, const short* src,
                                               size_t samples)
{
	WINPR_ASSERT(aaudio);
	WINPR_ASSERT(src);

	const UINT32 left = aaudio->volume & 0xFFFF;
	const UINT32 right = (aaudio->volume >> 16) & 0xFFFF;

	if ((left == 0xFFFF) && (right == 0xFFFF))
		return src;

	if (samples > SIZE_MAX / sizeof(short))
		return src;

	if (aaudio->gainbuf_len < samples)
	{
		short* tmp = (short*)realloc(aaudio->gainbuf, samples * sizeof(short));
		if (!tmp)
			return src;
		aaudio->gainbuf = tmp;
		aaudio->gainbuf_len = samples;
	}

	for (size_t i = 0; i < samples; i++)
	{
		const UINT32 gain = ((aaudio->channels == 2) && (i & 1)) ? right : left;
		aaudio->gainbuf[i] = (short)((int64_t)src[i] * gain / 0xFFFF);
	}

	return aaudio->gainbuf;
}

WINPR_ATTR_NODISCARD
static UINT rdpsnd_aaudio_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)device;
	WINPR_ASSERT(aaudio);

	if (!data || (size < 2))
		return 0;

	EnterCriticalSection(&aaudio->lock);

	if (!aaudio->opened)
	{
		/* Closed by rdpsnd; a wave PDU still in flight must not reopen the device. */
		LeaveCriticalSection(&aaudio->lock);
		return 0;
	}

	if (!aaudio->stream)
	{
		if (!rdpsnd_aaudio_open_stream(aaudio))
		{
			LeaveCriticalSection(&aaudio->lock);
			return 0;
		}
	}

	const size_t bytesPerFrame = aaudio->channels * sizeof(INT16);
	if (bytesPerFrame == 0)
	{
		LeaveCriticalSection(&aaudio->lock);
		return 0;
	}

	const short* samples =
	    rdpsnd_aaudio_apply_volume(aaudio, (const short*)data, size / sizeof(short));
	size_t frames = size / bytesPerFrame;
	const BYTE* pos = (const BYTE*)samples;

	while (frames > 0)
	{
		const int32_t chunk =
		    (frames > INT32_MAX) ? INT32_MAX : WINPR_ASSERTING_INT_CAST(int32_t, frames);
		aaudio_result_t written =
		    AAudioStream_write(aaudio->stream, pos, chunk, AAUDIO_WRITE_TIMEOUT_NS);

		if (written < 0)
		{
			/* A timeout is transient. Everything else (route change, audioserver restart,
			 * internal fault) leaves the stream unusable, so drop it and let the next play
			 * call reopen; keeping it would mute audio for the rest of the session. */
			if (written != AAUDIO_ERROR_TIMEOUT)
			{
				WLog_WARN(TAG, "AAudio stream unusable (%s), closing",
				          AAudio_convertResultToText(written));
				rdpsnd_aaudio_close_stream(aaudio);
			}
			else
			{
				WLog_ERR(TAG, "AAudioStream_write: %s", AAudio_convertResultToText(written));
			}

			LeaveCriticalSection(&aaudio->lock);
			return 0;
		}

		if ((written == 0) || (written > chunk))
			break;

		pos += (size_t)written * bytesPerFrame;
		frames -= (size_t)written;
	}

	if (aaudio->rate == 0)
	{
		LeaveCriticalSection(&aaudio->lock);
		return 0;
	}

	/* report accurate presentation latency (A/V sync) */
	int64_t framePosition = 0;
	int64_t timeNanoseconds = 0;
	int64_t queued = 0;

	if (AAudioStream_getTimestamp(aaudio->stream, CLOCK_MONOTONIC, &framePosition,
	                              &timeNanoseconds) == AAUDIO_OK)
	{
		struct timespec ts = { 0 };
		if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
		{
			const int64_t nowNanos = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
			int64_t elapsedNanos = nowNanos - timeNanoseconds;
			if (elapsedNanos < 0)
				elapsedNanos = 0;
			if (elapsedNanos > INT64_MAX / AAUDIO_MAX_RATE)
				elapsedNanos = INT64_MAX / AAUDIO_MAX_RATE;
			const int64_t framesPlayed = elapsedNanos * aaudio->rate / 1000000000LL;
			queued = AAudioStream_getFramesWritten(aaudio->stream) - (framePosition + framesPlayed);
		}
		else
		{
			const int64_t written = AAudioStream_getFramesWritten(aaudio->stream);
			const int64_t read = AAudioStream_getFramesRead(aaudio->stream);
			queued = written - read;
		}
	}
	else
	{
		const int64_t written = AAudioStream_getFramesWritten(aaudio->stream);
		const int64_t read = AAudioStream_getFramesRead(aaudio->stream);
		queued = written - read;
	}

	if (queued < 0)
		queued = 0;
	const UINT64 latencyMs = (UINT64)queued / aaudio->rate * 1000 +
	                         ((UINT64)queued % aaudio->rate) * 1000 / aaudio->rate;
	const UINT latency = (latencyMs > UINT32_MAX) ? UINT32_MAX : (UINT)latencyMs;
	LeaveCriticalSection(&aaudio->lock);
	return latency;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE aaudio_freerdp_rdpsnd_client_subsystem_entry(
    PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->pRegisterRdpsndDevice);

	rdpsndAAudioPlugin* aaudio = (rdpsndAAudioPlugin*)calloc(1, sizeof(rdpsndAAudioPlugin));
	if (!aaudio)
		return CHANNEL_RC_NO_MEMORY;

	InitializeCriticalSection(&aaudio->lock);

	aaudio->device.Open = rdpsnd_aaudio_open;
	aaudio->device.FormatSupported = rdpsnd_aaudio_format_supported;
	aaudio->device.GetVolume = rdpsnd_aaudio_get_volume;
	aaudio->device.SetVolume = rdpsnd_aaudio_set_volume;
	aaudio->device.Play = rdpsnd_aaudio_play;
	aaudio->device.Close = rdpsnd_aaudio_close;
	aaudio->device.Free = rdpsnd_aaudio_free;
	aaudio->device.DefaultFormat = rdpsnd_aaudio_default_format;

	aaudio->rate = 48000;
	aaudio->channels = 2;
	aaudio->volume = 0xFFFFFFFF;

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)aaudio);
	return CHANNEL_RC_OK;
}
