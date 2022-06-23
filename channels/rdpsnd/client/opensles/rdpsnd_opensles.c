/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2013 Armin Novak <armin.novak@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <stdbool.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include "opensl_io.h"
#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;

	UINT32 latency;
	int wformat;
	int block_size;
	char* device_name;

	OPENSL_STREAM* stream;

	UINT32 volume;

	UINT32 rate;
	UINT32 channels;
	int format;
} rdpsndopenslesPlugin;

static int rdpsnd_opensles_volume_to_millibel(unsigned short level, int max)
{
	const int min = SL_MILLIBEL_MIN;
	const int step = max - min;
	const int rc = (level * step / 0xFFFF) + min;
	DEBUG_SND("level=%hu, min=%d, max=%d, step=%d, result=%d", level, min, max, step, rc);
	return rc;
}

static unsigned short rdpsnd_opensles_millibel_to_volume(int millibel, int max)
{
	const int min = SL_MILLIBEL_MIN;
	const int range = max - min;
	const int rc = ((millibel - min) * 0xFFFF + range / 2 + 1) / range;
	DEBUG_SND("millibel=%d, min=%d, max=%d, range=%d, result=%d", millibel, min, max, range, rc);
	return rc;
}

static bool rdpsnd_opensles_check_handle(const rdpsndopenslesPlugin* hdl)
{
	bool rc = true;

	if (!hdl)
		rc = false;
	else
	{
		if (!hdl->stream)
			rc = false;
	}

	return rc;
}

static BOOL rdpsnd_opensles_set_volume(rdpsndDevicePlugin* device, UINT32 volume);

static int rdpsnd_opensles_set_params(rdpsndopenslesPlugin* opensles)
{
	DEBUG_SND("opensles=%p", (void*)opensles);

	if (!rdpsnd_opensles_check_handle(opensles))
		return 0;

	if (opensles->stream)
		android_CloseAudioDevice(opensles->stream);

	opensles->stream = android_OpenAudioDevice(opensles->rate, opensles->channels, 20);
	return 0;
}

static BOOL rdpsnd_opensles_set_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                       UINT32 latency)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	rdpsnd_opensles_check_handle(opensles);
	DEBUG_SND("opensles=%p format=%p, latency=%" PRIu32, (void*)opensles, (void*)format, latency);

	if (format)
	{
		DEBUG_SND("format=%" PRIu16 ", cbsize=%" PRIu16 ", samples=%" PRIu32 ", bits=%" PRIu16
		          ", channels=%" PRIu16 ", align=%" PRIu16 "",
		          format->wFormatTag, format->cbSize, format->nSamplesPerSec,
		          format->wBitsPerSample, format->nChannels, format->nBlockAlign);
		opensles->rate = format->nSamplesPerSec;
		opensles->channels = format->nChannels;
		opensles->format = format->wFormatTag;
		opensles->wformat = format->wFormatTag;
		opensles->block_size = format->nBlockAlign;
	}

	opensles->latency = latency;
	return (rdpsnd_opensles_set_params(opensles) == 0);
}

static BOOL rdpsnd_opensles_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                 UINT32 latency)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p format=%p, latency=%" PRIu32 ", rate=%" PRIu32 "", (void*)opensles,
	          (void*)format, latency, opensles->rate);

	if (rdpsnd_opensles_check_handle(opensles))
		return TRUE;

	opensles->stream = android_OpenAudioDevice(opensles->rate, opensles->channels, 20);
	WINPR_ASSERT(opensles->stream);

	if (!opensles->stream)
		WLog_ERR(TAG, "android_OpenAudioDevice failed");
	else
		rdpsnd_opensles_set_volume(device, opensles->volume);

	return rdpsnd_opensles_set_format(device, format, latency);
}

static void rdpsnd_opensles_close(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p", (void*)opensles);

	if (!rdpsnd_opensles_check_handle(opensles))
		return;

	android_CloseAudioDevice(opensles->stream);
	opensles->stream = NULL;
}

static void rdpsnd_opensles_free(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p", (void*)opensles);
	WINPR_ASSERT(opensles);
	WINPR_ASSERT(opensles->device_name);
	free(opensles->device_name);
	free(opensles);
}

static BOOL rdpsnd_opensles_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	DEBUG_SND("format=%" PRIu16 ", cbsize=%" PRIu16 ", samples=%" PRIu32 ", bits=%" PRIu16
	          ", channels=%" PRIu16 ", align=%" PRIu16 "",
	          format->wFormatTag, format->cbSize, format->nSamplesPerSec, format->wBitsPerSample,
	          format->nChannels, format->nBlockAlign);
	WINPR_ASSERT(device);
	WINPR_ASSERT(format);

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 && format->nSamplesPerSec <= 48000 &&
			    (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			    (format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}

			break;

		default:
			break;
	}

	return FALSE;
}

static UINT32 rdpsnd_opensles_get_volume(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p", (void*)opensles);
	WINPR_ASSERT(opensles);

	if (opensles->stream)
	{
		const int max = android_GetOutputVolumeMax(opensles->stream);
		const int rc = android_GetOutputVolume(opensles->stream);

		if (android_GetOutputMute(opensles->stream))
			opensles->volume = 0;
		else
		{
			const unsigned short vol = rdpsnd_opensles_millibel_to_volume(rc, max);
			opensles->volume = (vol << 16) | (vol & 0xFFFF);
		}
	}

	return opensles->volume;
}

static BOOL rdpsnd_opensles_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p, value=%" PRIu32 "", (void*)opensles, value);
	WINPR_ASSERT(opensles);
	opensles->volume = value;

	if (opensles->stream)
	{
		if (0 == opensles->volume)
			return android_SetOutputMute(opensles->stream, true);
		else
		{
			const int max = android_GetOutputVolumeMax(opensles->stream);
			const int vol = rdpsnd_opensles_volume_to_millibel(value & 0xFFFF, max);

			if (!android_SetOutputMute(opensles->stream, false))
				return FALSE;

			if (!android_SetOutputVolume(opensles->stream, vol))
				return FALSE;
		}
	}

	return TRUE;
}

static UINT rdpsnd_opensles_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	union
	{
		const BYTE* b;
		const short* s;
	} src;
	int ret;
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	DEBUG_SND("opensles=%p, data=%p, size=%d", (void*)opensles, (void*)data, size);

	if (!rdpsnd_opensles_check_handle(opensles))
		return 0;

	src.b = data;
	DEBUG_SND("size=%d, src=%p", size, (void*)src.b);
	WINPR_ASSERT(0 == size % 2);
	WINPR_ASSERT(size > 0);
	WINPR_ASSERT(src.b);
	ret = android_AudioOut(opensles->stream, src.s, size / 2);

	if (ret < 0)
		WLog_ERR(TAG, "android_AudioOut failed (%d)", ret);

	return 10; /* TODO: Get real latencry in [ms] */
}

static void rdpsnd_opensles_start(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	rdpsnd_opensles_check_handle(opensles);
	DEBUG_SND("opensles=%p", (void*)opensles);
}

static int rdpsnd_opensles_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*)device;
	COMMAND_LINE_ARGUMENT_A rdpsnd_opensles_args[] = {
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	WINPR_ASSERT(opensles);
	WINPR_ASSERT(args);
	DEBUG_SND("opensles=%p, args=%p", (void*)opensles, (void*)args);
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, rdpsnd_opensles_args, flags,
	                                    opensles, NULL, NULL);

	if (status < 0)
		return status;

	arg = rdpsnd_opensles_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			opensles->device_name = _strdup(arg->Value);

			if (!opensles->device_name)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT opensles_freerdp_rdpsnd_client_subsystem_entry(
    PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndopenslesPlugin* opensles;
	UINT error;
	DEBUG_SND("pEntryPoints=%p", (void*)pEntryPoints);
	opensles = (rdpsndopenslesPlugin*)calloc(1, sizeof(rdpsndopenslesPlugin));

	if (!opensles)
		return CHANNEL_RC_NO_MEMORY;

	opensles->device.Open = rdpsnd_opensles_open;
	opensles->device.FormatSupported = rdpsnd_opensles_format_supported;
	opensles->device.GetVolume = rdpsnd_opensles_get_volume;
	opensles->device.SetVolume = rdpsnd_opensles_set_volume;
	opensles->device.Start = rdpsnd_opensles_start;
	opensles->device.Play = rdpsnd_opensles_play;
	opensles->device.Close = rdpsnd_opensles_close;
	opensles->device.Free = rdpsnd_opensles_free;
	args = pEntryPoints->args;
	rdpsnd_opensles_parse_addin_args((rdpsndDevicePlugin*)opensles, args);

	if (!opensles->device_name)
	{
		opensles->device_name = _strdup("default");

		if (!opensles->device_name)
		{
			error = CHANNEL_RC_NO_MEMORY;
			goto outstrdup;
		}
	}

	opensles->rate = 44100;
	opensles->channels = 2;
	opensles->format = WAVE_FORMAT_ADPCM;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)opensles);
	DEBUG_SND("success");
	return CHANNEL_RC_OK;
outstrdup:
	free(opensles);
	return error;
}
