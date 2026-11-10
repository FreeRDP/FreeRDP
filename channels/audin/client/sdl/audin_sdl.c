/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - SDL3 backend
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

#include <freerdp/freerdp.h>
#include <freerdp/addin.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/client/audin.h>

#include <SDL3/SDL.h>

#include "audin_main.h"

#define AUDIN_SDL_MIN_RATE 8000
#define AUDIN_SDL_MAX_RATE 192000

typedef struct
{
	IAudinDevice iface;

	char* device_name;
	SDL_AudioStream* stream;
	AUDIO_FORMAT format;
	size_t packet_bytes;
	BYTE* packet;
	BOOL failed;

	AudinReceive receive;
	void* user_data;
	rdpContext* rdpcontext;
	wLog* log;
} AudinSdlDevice;

/* Shared by the SDL audio channels: SDL_InitSubSystem is not thread safe. */
#define AUDIN_SDL_LOCK_PROPERTY "FreeRDP.SDL.audio.lock"

static void SDLCALL audin_sdl_destroy_lock(WINPR_ATTR_UNUSED void* userdata, void* value)
{
	SDL_DestroyMutex(value);
}

WINPR_ATTR_NODISCARD
static SDL_Mutex* audin_sdl_audio_lock(void)
{
	const SDL_PropertiesID props = SDL_GetGlobalProperties();
	if (!props || !SDL_LockProperties(props))
		return nullptr;

	SDL_Mutex* lock = SDL_GetPointerProperty(props, AUDIN_SDL_LOCK_PROPERTY, nullptr);
	if (!lock)
	{
		lock = SDL_CreateMutex();
		/* On failure the cleanup callback already destroyed the mutex. */
		if (lock && !SDL_SetPointerPropertyWithCleanup(props, AUDIN_SDL_LOCK_PROPERTY, lock,
		                                               audin_sdl_destroy_lock, nullptr))
			lock = nullptr;
	}
	SDL_UnlockProperties(props);
	return lock;
}

WINPR_ATTR_NODISCARD
static BOOL audin_sdl_init_audio(void)
{
	SDL_Mutex* lock = audin_sdl_audio_lock();
	if (!lock)
		return FALSE;

	SDL_LockMutex(lock);
	const BOOL rc = SDL_InitSubSystem(SDL_INIT_AUDIO);
	SDL_UnlockMutex(lock);
	return rc;
}

static void audin_sdl_quit_audio(wLog* log)
{
	SDL_Mutex* lock = audin_sdl_audio_lock();
	if (!lock)
		WLog_Print(log, WLOG_WARN, "quitting SDL audio without the lock: %s", SDL_GetError());

	SDL_LockMutex(lock);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	SDL_UnlockMutex(lock);
}

WINPR_ATTR_NODISCARD
static BOOL audin_sdl_pcm_valid(const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(format);

	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->wBitsPerSample == 16) &&
	       (format->nSamplesPerSec >= AUDIN_SDL_MIN_RATE) &&
	       (format->nSamplesPerSec <= AUDIN_SDL_MAX_RATE) &&
	       ((format->nChannels == 1) || (format->nChannels == 2));
}

WINPR_ATTR_NODISCARD
static SDL_AudioDeviceID audin_sdl_find_device(AudinSdlDevice* sdl)
{
	WINPR_ASSERT(sdl);

	SDL_AudioDeviceID id = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
	if (!sdl->device_name)
		return id;

	int count = 0;
	SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
	if (!devices)
	{
		WLog_Print(sdl->log, WLOG_WARN, "SDL_GetAudioRecordingDevices: %s", SDL_GetError());
		return id;
	}

	BOOL found = FALSE;
	for (int x = 0; x < count; x++)
	{
		const char* cur = SDL_GetAudioDeviceName(devices[x]);
		if (cur && (strcmp(cur, sdl->device_name) == 0))
		{
			id = devices[x];
			found = TRUE;
			break;
		}
	}
	SDL_free(devices);

	if (!found)
		WLog_Print(sdl->log, WLOG_WARN, "recording device '%s' not found, using the default device",
		           sdl->device_name);
	return id;
}

/* Runs on the SDL audio thread. */
static void SDLCALL audin_sdl_recorded(void* userdata, SDL_AudioStream* stream,
                                       WINPR_ATTR_UNUSED int additional_amount,
                                       WINPR_ATTR_UNUSED int total_amount)
{
	AudinSdlDevice* sdl = userdata;
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(sdl->receive);
	WINPR_ASSERT(sdl->packet);

	const int len = WINPR_ASSERTING_INT_CAST(int, sdl->packet_bytes);
	while (!sdl->failed && (SDL_GetAudioStreamAvailable(stream) >= len))
	{
		const int got = SDL_GetAudioStreamData(stream, sdl->packet, len);
		if (got != len)
		{
			if (got < 0)
				WLog_Print(sdl->log, WLOG_WARN, "SDL_GetAudioStreamData: %s", SDL_GetError());
			break;
		}

		const UINT error =
		    sdl->receive(&sdl->format, sdl->packet, sdl->packet_bytes, sdl->user_data);
		if (error != CHANNEL_RC_OK)
		{
			sdl->failed = TRUE;
			if (sdl->rdpcontext)
				setChannelError(sdl->rdpcontext, error, "audin_sdl receive reported an error");
		}
	}

	/* Drop input after a receive error. */
	if (sdl->failed && !SDL_ClearAudioStream(stream))
		WLog_Print(sdl->log, WLOG_WARN, "SDL_ClearAudioStream: %s", SDL_GetError());
}

WINPR_ATTR_NODISCARD
static BOOL audin_sdl_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	WINPR_ASSERT(device);
	WINPR_ASSERT(format);

	return audin_sdl_pcm_valid(format);
}

static void audin_sdl_stop(AudinSdlDevice* sdl)
{
	WINPR_ASSERT(sdl);

	/* Waits for a running callback. */
	SDL_DestroyAudioStream(sdl->stream);
	sdl->stream = nullptr;
	sdl->receive = nullptr;
	sdl->user_data = nullptr;
}

WINPR_ATTR_NODISCARD
static UINT audin_sdl_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                 UINT32 FramesPerPacket)
{
	AudinSdlDevice* sdl = (AudinSdlDevice*)device;
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(format);

	if (!audin_sdl_pcm_valid(format))
		return ERROR_UNSUPPORTED_TYPE;

	/* Server provided, keep it between 10 ms and 1 s. */
	const size_t bytesPerFrame = 2ULL * format->nChannels;
	const size_t frames =
	    MAX(MIN(FramesPerPacket, format->nSamplesPerSec), format->nSamplesPerSec / 100);

	/* The callback uses the packet buffer. */
	audin_sdl_stop(sdl);

	BYTE* packet = realloc(sdl->packet, frames * bytesPerFrame);
	if (!packet)
		return CHANNEL_RC_NO_MEMORY;

	sdl->packet = packet;
	sdl->packet_bytes = frames * bytesPerFrame;
	sdl->format = *format;
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT audin_sdl_close(IAudinDevice* device)
{
	AudinSdlDevice* sdl = (AudinSdlDevice*)device;
	WINPR_ASSERT(sdl);

	audin_sdl_stop(sdl);
	return CHANNEL_RC_OK;
}

/* One packet per device period: with a longer period (1024 frames on PipeWire) the packets
 * leave in bursts, and Windows loses some of the AAC encoded audio. */
WINPR_ATTR_NODISCARD
static SDL_AudioStream* audin_sdl_open_stream(AudinSdlDevice* sdl, const SDL_AudioSpec* spec)
{
	WINPR_ASSERT(sdl);
	WINPR_ASSERT(spec);

	const char* prev = SDL_GetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
	char* saved = prev ? _strdup(prev) : nullptr;

	char frames[32] = WINPR_C_ARRAY_INIT;
	const size_t packetFrames = sdl->packet_bytes / (2ULL * sdl->format.nChannels);
	if ((_snprintf(frames, sizeof(frames), "%" PRIuz, packetFrames) < 0) ||
	    !SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames))
		WLog_Print(sdl->log, WLOG_DEBUG, "device period not set: %s", SDL_GetError());

	SDL_AudioStream* stream =
	    SDL_OpenAudioDeviceStream(audin_sdl_find_device(sdl), spec, audin_sdl_recorded, sdl);

	const bool restored = saved ? SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, saved)
	                            : SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
	if (!restored)
		WLog_Print(sdl->log, WLOG_DEBUG, "device period hint not restored");
	free(saved);
	return stream;
}

WINPR_ATTR_NODISCARD
static UINT audin_sdl_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinSdlDevice* sdl = (AudinSdlDevice*)device;
	WINPR_ASSERT(sdl);

	if (!receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	if (sdl->stream)
		return CHANNEL_RC_OK;

	if (!sdl->packet)
	{
		WLog_Print(sdl->log, WLOG_ERROR, "SetFormat must run with a valid PCM format first");
		return ERROR_INVALID_PARAMETER;
	}

	const SDL_AudioSpec spec = {
		.format = SDL_AUDIO_S16LE,
		.channels = sdl->format.nChannels,
		.freq = WINPR_ASSERTING_INT_CAST(int, sdl->format.nSamplesPerSec),
	};

	sdl->receive = receive;
	sdl->user_data = user_data;
	sdl->failed = FALSE;

	sdl->stream = audin_sdl_open_stream(sdl, &spec);
	if (!sdl->stream)
	{
		WLog_Print(sdl->log, WLOG_ERROR, "SDL_OpenAudioDeviceStream: %s", SDL_GetError());
		goto fail;
	}

	if (!SDL_ResumeAudioStreamDevice(sdl->stream))
	{
		WLog_Print(sdl->log, WLOG_ERROR, "SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
		goto fail;
	}

	return CHANNEL_RC_OK;

fail:
	audin_sdl_stop(sdl);
	return ERROR_INTERNAL_ERROR;
}

WINPR_ATTR_NODISCARD
static UINT audin_sdl_free(IAudinDevice* device)
{
	AudinSdlDevice* sdl = (AudinSdlDevice*)device;
	if (!sdl)
		return ERROR_INVALID_PARAMETER;

	audin_sdl_stop(sdl);
	audin_sdl_quit_audio(sdl->log);
	free(sdl->packet);
	free(sdl->device_name);
	free(sdl);
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT audin_sdl_parse_addin_args(AudinSdlDevice* sdl, const ADDIN_ARGV* args)
{
	COMMAND_LINE_ARGUMENT_A audin_sdl_args[] = {
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", nullptr, nullptr, -1, nullptr,
		  "audio device name" },
		{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
	};
	const DWORD flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	WINPR_ASSERT(sdl);
	WINPR_ASSERT(args);

	const int status = CommandLineParseArgumentsA(args->argc, args->argv, audin_sdl_args, flags,
	                                              sdl, nullptr, nullptr);
	if (status < 0)
		return ERROR_INVALID_DATA;

	const COMMAND_LINE_ARGUMENT_A* arg = audin_sdl_args;
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

FREERDP_ENTRY_POINT(UINT VCAPITYPE sdl_freerdp_audin_client_subsystem_entry(
    PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->pRegisterAudinDevice);

	AudinSdlDevice* sdl = calloc(1, sizeof(AudinSdlDevice));
	if (!sdl)
		return CHANNEL_RC_NO_MEMORY;

	sdl->log = WLog_Get(TAG);
	if (!audin_sdl_init_audio())
	{
		WLog_Print(sdl->log, WLOG_ERROR, "SDL_InitSubSystem(SDL_INIT_AUDIO): %s", SDL_GetError());
		free(sdl);
		return ERROR_INTERNAL_ERROR;
	}

	sdl->iface.Open = audin_sdl_open;
	sdl->iface.FormatSupported = audin_sdl_format_supported;
	sdl->iface.SetFormat = audin_sdl_set_format;
	sdl->iface.Close = audin_sdl_close;
	sdl->iface.Free = audin_sdl_free;
	sdl->rdpcontext = pEntryPoints->rdpcontext;

	UINT error = CHANNEL_RC_OK;
	if (pEntryPoints->args)
	{
		error = audin_sdl_parse_addin_args(sdl, pEntryPoints->args);
		if (error != CHANNEL_RC_OK)
			WLog_Print(sdl->log, WLOG_ERROR, "error parsing arguments");
	}

	if (error == CHANNEL_RC_OK)
	{
		error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, &sdl->iface);
		if (error != CHANNEL_RC_OK)
			WLog_Print(sdl->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32,
			           error);
	}

	wLog* log = sdl->log;
	if ((error != CHANNEL_RC_OK) && (audin_sdl_free(&sdl->iface) != CHANNEL_RC_OK))
		WLog_Print(log, WLOG_WARN, "audin_sdl_free failed");
	return error;
}
