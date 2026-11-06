/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel - SDL3 backend
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

#include <stdlib.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/synch.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include <SDL3/SDL.h>

#include "rdpsnd_main.h"

#define RDPSND_SDL_DEFAULT_LATENCY_MS 100
#define RDPSND_SDL_MAX_LATENCY_MS 1000
/* Queued audio beyond the latency, same slack as rdpsnd_detect_overrun(). */
#define RDPSND_SDL_OVERRUN_SLACK_MS 200

#define RDPSND_SDL_MIN_RATE 8000
#define RDPSND_SDL_MAX_RATE 192000

typedef struct
{
	rdpsndDevicePlugin device;

	char* device_name;
	SDL_AudioStream* stream;
	SDL_AudioSpec spec;
	size_t frame_size;
	UINT32 latency;
	UINT32 volume;

	/* Play and Close run on different threads. */
	CRITICAL_SECTION lock;
} rdpsndSdlPlugin;

/* Shared by the SDL audio channels: SDL_InitSubSystem is not thread safe. */
#define RDPSND_SDL_LOCK_PROPERTY "FreeRDP.SDL.audio.lock"

static void SDLCALL rdpsnd_sdl_destroy_lock(WINPR_ATTR_UNUSED void* userdata, void* value)
{
	SDL_DestroyMutex(value);
}

WINPR_ATTR_NODISCARD
static SDL_Mutex* rdpsnd_sdl_audio_lock(void)
{
	const SDL_PropertiesID props = SDL_GetGlobalProperties();
	if (!props || !SDL_LockProperties(props))
		return nullptr;

	SDL_Mutex* lock = SDL_GetPointerProperty(props, RDPSND_SDL_LOCK_PROPERTY, nullptr);
	if (!lock)
	{
		lock = SDL_CreateMutex();
		/* On failure the cleanup callback already destroyed the mutex. */
		if (lock && !SDL_SetPointerPropertyWithCleanup(props, RDPSND_SDL_LOCK_PROPERTY, lock,
		                                               rdpsnd_sdl_destroy_lock, nullptr))
			lock = nullptr;
	}
	SDL_UnlockProperties(props);
	return lock;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_init_audio(void)
{
	SDL_Mutex* lock = rdpsnd_sdl_audio_lock();
	if (!lock)
		return FALSE;

	SDL_LockMutex(lock);
	const BOOL rc = SDL_InitSubSystem(SDL_INIT_AUDIO);
	SDL_UnlockMutex(lock);
	return rc;
}

static void rdpsnd_sdl_quit_audio(void)
{
	SDL_Mutex* lock = rdpsnd_sdl_audio_lock();
	if (!lock)
		WLog_WARN(TAG, "quitting SDL audio without the lock: %s", SDL_GetError());

	SDL_LockMutex(lock);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	SDL_UnlockMutex(lock);
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_format_to_spec(const AUDIO_FORMAT* format, SDL_AudioSpec* spec)
{
	WINPR_ASSERT(format);
	WINPR_ASSERT(spec);

	if ((format->wFormatTag != WAVE_FORMAT_PCM) || (format->nChannels < 1) ||
	    (format->nChannels > 8) || (format->nSamplesPerSec < RDPSND_SDL_MIN_RATE) ||
	    (format->nSamplesPerSec > RDPSND_SDL_MAX_RATE))
		return FALSE;

	switch (format->wBitsPerSample)
	{
		case 8:
			spec->format = SDL_AUDIO_U8;
			break;
		case 16:
			spec->format = SDL_AUDIO_S16LE;
			break;
		default:
			return FALSE;
	}

	spec->channels = format->nChannels;
	spec->freq = WINPR_ASSERTING_INT_CAST(int, format->nSamplesPerSec);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static SDL_AudioDeviceID rdpsnd_sdl_find_device(const char* name)
{
	SDL_AudioDeviceID id = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
	if (!name)
		return id;

	int count = 0;
	SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
	if (!devices)
	{
		WLog_WARN(TAG, "SDL_GetAudioPlaybackDevices: %s", SDL_GetError());
		return id;
	}

	BOOL found = FALSE;
	for (int x = 0; x < count; x++)
	{
		const char* cur = SDL_GetAudioDeviceName(devices[x]);
		if (cur && (strcmp(cur, name) == 0))
		{
			id = devices[x];
			found = TRUE;
			break;
		}
	}
	SDL_free(devices);

	if (!found)
		WLog_WARN(TAG, "playback device '%s' not found, using the default device", name);
	return id;
}

static void rdpsnd_sdl_apply_volume(rdpsndSdlPlugin* sdl)
{
	WINPR_ASSERT(sdl);

	if (!sdl->stream)
		return;

	/* SDL has one gain per stream, use the louder channel. */
	const UINT32 left = sdl->volume & 0xFFFFU;
	const UINT32 right = (sdl->volume >> 16) & 0xFFFFU;
	const float gain = (float)MAX(left, right) / (float)0xFFFFU;
	if (!SDL_SetAudioStreamGain(sdl->stream, gain))
		WLog_WARN(TAG, "SDL_SetAudioStreamGain: %s", SDL_GetError());
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(device);
	WINPR_ASSERT(format);

	SDL_AudioSpec spec = WINPR_C_ARRAY_INIT;
	return rdpsnd_sdl_format_to_spec(format, &spec);
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_default_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* desired,
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

	if ((defaultFormat->nChannels < 1) || (defaultFormat->nChannels > 8))
		defaultFormat->nChannels = 2;
	if ((defaultFormat->nSamplesPerSec < RDPSND_SDL_MIN_RATE) ||
	    (defaultFormat->nSamplesPerSec > RDPSND_SDL_MAX_RATE))
		defaultFormat->nSamplesPerSec = 48000;

	defaultFormat->nBlockAlign = defaultFormat->nChannels * defaultFormat->wBitsPerSample / 8;
	defaultFormat->nAvgBytesPerSec = defaultFormat->nBlockAlign * defaultFormat->nSamplesPerSec;
	return TRUE;
}

static void rdpsnd_sdl_close(rdpsndDevicePlugin* device)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	WINPR_ASSERT(sdl);

	EnterCriticalSection(&sdl->lock);
	SDL_DestroyAudioStream(sdl->stream);
	sdl->stream = nullptr;
	LeaveCriticalSection(&sdl->lock);
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format, UINT32 latency)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(format);

	SDL_AudioSpec spec = WINPR_C_ARRAY_INIT;
	if (!rdpsnd_sdl_format_to_spec(format, &spec))
	{
		WLog_ERR(TAG, "unsupported format %s", audio_format_get_tag_string(format->wFormatTag));
		return FALSE;
	}

	EnterCriticalSection(&sdl->lock);
	SDL_DestroyAudioStream(sdl->stream);
	sdl->latency =
	    (latency > 0) ? MIN(latency, RDPSND_SDL_MAX_LATENCY_MS) : RDPSND_SDL_DEFAULT_LATENCY_MS;
	sdl->spec = spec;
	sdl->frame_size = WINPR_ASSERTING_INT_CAST(size_t, SDL_AUDIO_FRAMESIZE(spec));
	sdl->stream = SDL_OpenAudioDeviceStream(rdpsnd_sdl_find_device(sdl->device_name), &spec,
	                                        nullptr, nullptr);

	BOOL rc = FALSE;
	if (!sdl->stream)
		WLog_ERR(TAG, "SDL_OpenAudioDeviceStream: %s", SDL_GetError());
	else if (!SDL_ResumeAudioStreamDevice(sdl->stream))
		WLog_ERR(TAG, "SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
	else
	{
		rdpsnd_sdl_apply_volume(sdl);
		rc = TRUE;
	}

	if (!rc)
	{
		SDL_DestroyAudioStream(sdl->stream);
		sdl->stream = nullptr;
	}
	LeaveCriticalSection(&sdl->lock);
	return rc;
}

static void rdpsnd_sdl_free(rdpsndDevicePlugin* device)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	if (!sdl)
		return;

	rdpsnd_sdl_close(device);
	rdpsnd_sdl_quit_audio();
	DeleteCriticalSection(&sdl->lock);
	free(sdl->device_name);
	free(sdl);
}

WINPR_ATTR_NODISCARD
static UINT32 rdpsnd_sdl_get_volume(rdpsndDevicePlugin* device)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	WINPR_ASSERT(sdl);

	return sdl->volume;
}

WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	WINPR_ASSERT(sdl);

	EnterCriticalSection(&sdl->lock);
	sdl->volume = value;
	rdpsnd_sdl_apply_volume(sdl);
	LeaveCriticalSection(&sdl->lock);
	return TRUE;
}

/* Silence in front of a sound that starts from an empty stream. */
WINPR_ATTR_NODISCARD
static BOOL rdpsnd_sdl_put_silence(rdpsndSdlPlugin* sdl)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(sdl->stream);

	BYTE chunk[4096];
	const size_t frameSize = sdl->frame_size;
	if (frameSize == 0)
		return FALSE;

	const size_t chunkSize = sizeof(chunk) / frameSize * frameSize;
	memset(chunk, SDL_GetSilenceValueForFormat(sdl->spec.format), sizeof(chunk));

	size_t left =
	    1ULL * sdl->latency * WINPR_ASSERTING_INT_CAST(size_t, sdl->spec.freq) / 1000 * frameSize;
	while (left > 0)
	{
		const size_t len = MIN(left, chunkSize);
		if (!SDL_PutAudioStreamData(sdl->stream, chunk, WINPR_ASSERTING_INT_CAST(int, len)))
		{
			WLog_WARN(TAG, "SDL_PutAudioStreamData: %s", SDL_GetError());
			return FALSE;
		}
		left -= len;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static UINT rdpsnd_sdl_bytes_to_ms(const rdpsndSdlPlugin* sdl, size_t bytes)
{
	WINPR_ASSERT(sdl);

	const UINT64 ms = 1000ULL * bytes / sdl->frame_size / (UINT64)sdl->spec.freq;
	return WINPR_ASSERTING_INT_CAST(UINT, ms);
}

/* Called with the lock held. */
WINPR_ATTR_NODISCARD
static UINT rdpsnd_sdl_queue(rdpsndSdlPlugin* sdl, const BYTE* data, size_t size)
{
	WINPR_ASSERT(sdl);

	const size_t frameSize = sdl->frame_size;
	if (!sdl->stream || (frameSize == 0) || (sdl->spec.freq <= 0))
		return 0;

	/* SDL rejects a buffer that ends in a partial frame. */
	size -= size % frameSize;
	if ((size == 0) || (size > INT32_MAX))
		return 0;

	/* SDL holds a few frames back for resampling, a drained stream is not always empty. */
	const int before = SDL_GetAudioStreamQueued(sdl->stream);
	const size_t drained = frameSize * WINPR_ASSERTING_INT_CAST(size_t, sdl->spec.freq) / 100;
	if ((before >= 0) && ((size_t)before < drained) && !rdpsnd_sdl_put_silence(sdl))
		return 0;

	/* A burst after a server stall would delay the sound for the rest of the session. */
	const UINT queuedMs = (before > 0) ? rdpsnd_sdl_bytes_to_ms(sdl, (size_t)before) : 0;
	const UINT sizeMs = rdpsnd_sdl_bytes_to_ms(sdl, size);
	if (queuedMs > sizeMs + sdl->latency + RDPSND_SDL_OVERRUN_SLACK_MS)
	{
		WLog_DBG(TAG, "%" PRIu32 " ms queued, dropping %" PRIu32 " ms", queuedMs, sizeMs);
		return queuedMs;
	}

	if (!SDL_PutAudioStreamData(sdl->stream, data, WINPR_ASSERTING_INT_CAST(int, size)))
	{
		WLog_ERR(TAG, "SDL_PutAudioStreamData: %s", SDL_GetError());
		return 0;
	}

	const int queued = SDL_GetAudioStreamQueued(sdl->stream);
	if (queued <= 0)
		return 0;

	return rdpsnd_sdl_bytes_to_ms(sdl, (size_t)queued);
}

WINPR_ATTR_NODISCARD
static UINT rdpsnd_sdl_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	rdpsndSdlPlugin* sdl = (rdpsndSdlPlugin*)device;
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(data);

	EnterCriticalSection(&sdl->lock);
	const UINT latency = rdpsnd_sdl_queue(sdl, data, size);
	LeaveCriticalSection(&sdl->lock);
	return latency;
}

WINPR_ATTR_NODISCARD
static UINT rdpsnd_sdl_parse_addin_args(rdpsndSdlPlugin* sdl, const ADDIN_ARGV* args)
{
	COMMAND_LINE_ARGUMENT_A rdpsnd_sdl_args[] = {
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", nullptr, nullptr, -1, nullptr, "device" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
	};
	const DWORD flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(args);

	const int status = CommandLineParseArgumentsA(args->argc, args->argv, rdpsnd_sdl_args, flags,
	                                              sdl, nullptr, nullptr);
	if (status < 0)
		return ERROR_INVALID_DATA;

	const COMMAND_LINE_ARGUMENT_A* arg = rdpsnd_sdl_args;
	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			free(sdl->device_name);
			sdl->device_name = _strdup(arg->Value);
			if (!sdl->device_name)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != nullptr);

	return CHANNEL_RC_OK;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE sdl_freerdp_rdpsnd_client_subsystem_entry(
    PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->pRegisterRdpsndDevice);

	rdpsndSdlPlugin* sdl = calloc(1, sizeof(rdpsndSdlPlugin));
	if (!sdl)
		return CHANNEL_RC_NO_MEMORY;

	if (!rdpsnd_sdl_init_audio())
	{
		WLog_ERR(TAG, "SDL_InitSubSystem(SDL_INIT_AUDIO): %s", SDL_GetError());
		free(sdl);
		return ERROR_INTERNAL_ERROR;
	}

	InitializeCriticalSection(&sdl->lock);
	sdl->device.Open = rdpsnd_sdl_open;
	sdl->device.FormatSupported = rdpsnd_sdl_format_supported;
	sdl->device.GetVolume = rdpsnd_sdl_get_volume;
	sdl->device.SetVolume = rdpsnd_sdl_set_volume;
	sdl->device.Play = rdpsnd_sdl_play;
	sdl->device.Close = rdpsnd_sdl_close;
	sdl->device.Free = rdpsnd_sdl_free;
	sdl->device.DefaultFormat = rdpsnd_sdl_default_format;
	sdl->volume = 0xFFFFFFFF;

	if (pEntryPoints->args)
	{
		const UINT rc = rdpsnd_sdl_parse_addin_args(sdl, pEntryPoints->args);
		if (rc != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "error parsing arguments");
			rdpsnd_sdl_free(&sdl->device);
			return rc;
		}
	}

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, &sdl->device);
	return CHANNEL_RC_OK;
}
