/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2013 Armin Novak <armin.novak@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/utils/debug.h>

#include "opensl_io.h"
#include "rdpsnd_main.h"

typedef struct rdpsnd_opensles_plugin rdpsndopenslesPlugin;

struct rdpsnd_opensles_plugin
{
	rdpsndDevicePlugin device;

	int latency;
	int wformat;
	int block_size;
	char* device_name;

	OPENSL_STREAM *stream;

	UINT32 volume;

	UINT32 rate;
	UINT32 channels;
	int format;
	FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_opensles_set_volume(rdpsndDevicePlugin* device,
		UINT32 volume);

static int rdpsnd_opensles_set_params(rdpsndopenslesPlugin* opensles)
{
	DEBUG_SND("opensles=%p", opensles);

	return 0;
}

static void rdpsnd_opensles_set_format(rdpsndDevicePlugin* device,
		AUDIO_FORMAT* format, int latency)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p format=%p, latency=%d", opensles, format, latency);

	if (format)
	{
		opensles->rate = format->nSamplesPerSec;
		opensles->channels = format->nChannels;

		switch (format->wFormatTag)
		{
			case WAVE_FORMAT_PCM:
				switch (format->wBitsPerSample)
				{
					case 4:
						opensles->format = WAVE_FORMAT_ADPCM;
						break;

					case 8:
						opensles->format = WAVE_FORMAT_PCM;
						break;

					case 16:
						opensles->format = WAVE_FORMAT_ADPCM;
						break;
				}
				break;

			case WAVE_FORMAT_ADPCM:
			case WAVE_FORMAT_DVI_ADPCM:
				opensles->format = WAVE_FORMAT_ADPCM;
				break;
		}

		opensles->wformat = format->wFormatTag;
		opensles->block_size = format->nBlockAlign;
	}

	opensles->latency = latency;

	rdpsnd_opensles_set_params(opensles);
}

static void rdpsnd_opensles_open(rdpsndDevicePlugin* device,
		AUDIO_FORMAT* format, int latency)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p format=%p, latency=%d", opensles, format, latency);
	if (opensles->stream)
		return;

	opensles->stream = android_OpenAudioDevice(
		opensles->rate, 0, opensles->channels, opensles->rate * 100);
	if (!opensles->stream)
		DEBUG_WARN("android_OpenAudioDevice failed");
	else
		rdpsnd_opensles_set_volume(device, opensles->volume);
}

static void rdpsnd_opensles_close(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p", opensles);
	if (!opensles->stream)
		return;

	android_CloseAudioDevice(opensles->stream);
}

static void rdpsnd_opensles_free(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p", opensles);

	free(opensles->device_name);

	freerdp_dsp_context_free(opensles->dsp_context);

	free(opensles);
}

static BOOL rdpsnd_opensles_format_supported(rdpsndDevicePlugin* device,
		AUDIO_FORMAT* format)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p, format=%p", opensles, format);
	
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 &&
				format->nSamplesPerSec <= 48000 &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;

		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			/*
			if (format->nSamplesPerSec <= 48000 &&
				format->wBitsPerSample == 4 &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			*/
			break;

		case WAVE_FORMAT_ALAW:
			break;

		case WAVE_FORMAT_MULAW:
			break;

		case WAVE_FORMAT_GSM610:
			break;
	}

	return FALSE;
}

static UINT32 rdpsnd_opensles_get_volume(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p", opensles);

	if (opensles->stream)
		return android_GetOutputVolume(opensles->stream);
	else
		return opensles->volume;
}

static void rdpsnd_opensles_set_volume(rdpsndDevicePlugin* device,
		UINT32 value)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;
	
	DEBUG_SND("opensles=%p, value=%d", opensles, value);

	opensles->volume = value;
	if (opensles->stream)
		android_SetOutputVolume(opensles->stream, value);
}

static void rdpsnd_opensles_play(rdpsndDevicePlugin* device,
		BYTE *data, int size)
{
	BYTE* src;
	int len;
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;
	
	DEBUG_SND("opensles=%p, data=%p, size=%d", opensles, data, size);
	assert(opensles);
	if (!opensles->stream)
		return;

	if (opensles->format == WAVE_FORMAT_ADPCM)
	{
		opensles->dsp_context->decode_ms_adpcm(opensles->dsp_context,
				data, size, opensles->channels, opensles->block_size);

		size = opensles->dsp_context->adpcm_size;
		src = opensles->dsp_context->adpcm_buffer;
	}
	else if (opensles->format == WAVE_FORMAT_DVI_ADPCM)
	{
		opensles->dsp_context->decode_ima_adpcm(opensles->dsp_context,
				data, size, opensles->channels, opensles->block_size);
		
		size = opensles->dsp_context->adpcm_size;
		src = opensles->dsp_context->adpcm_buffer;
	}
	else
	{   
		src = data;
	} 

	len = size;
	while (size > 0)
	{
		int ret;

		if (len < 0)
			break;
		
		if (len > size)
			len = size;

		DEBUG_SND("len=%d, src=%p", len, src);
		ret = android_AudioOut(opensles->stream, (short*)src, len / 2);
		if (ret < 0)
		{
			DEBUG_WARN("android_AudioOut failed (%d)", ret);
			break;
		}
	
		DEBUG_SND("foobar XXXXXXXXXXXX opensles=%p, data=%p, size=%d", opensles, data, size);
		
		src += len;
		size -= len;
	}
}

static void rdpsnd_opensles_start(rdpsndDevicePlugin* device)
{
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p", opensles);
}

static COMMAND_LINE_ARGUMENT_A rdpsnd_opensles_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		NULL, NULL, -1, NULL, "device" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static int rdpsnd_opensles_parse_addin_args(rdpsndDevicePlugin* device,
		ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndopenslesPlugin* opensles = (rdpsndopenslesPlugin*) device;

	DEBUG_SND("opensles=%p, args=%p", opensles, args);

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			rdpsnd_opensles_args, flags, opensles, NULL, NULL);
	if (status < 0)
		return status;

	arg = rdpsnd_opensles_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			opensles->device_name = _strdup(arg->Value);
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return status;
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry \
	opensles_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(
		PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndopenslesPlugin* opensles;

	DEBUG_SND("pEntryPoints=%p", pEntryPoints);

	opensles = (rdpsndopenslesPlugin*) malloc(sizeof(rdpsndopenslesPlugin));
	ZeroMemory(opensles, sizeof(rdpsndopenslesPlugin));

	opensles->device.Open = rdpsnd_opensles_open;
	opensles->device.FormatSupported = rdpsnd_opensles_format_supported;
	opensles->device.SetFormat = rdpsnd_opensles_set_format;
	opensles->device.GetVolume = rdpsnd_opensles_get_volume;
	opensles->device.SetVolume = rdpsnd_opensles_set_volume;
	opensles->device.Start = rdpsnd_opensles_start;
	opensles->device.Play = rdpsnd_opensles_play;
	opensles->device.Close = rdpsnd_opensles_close;
	opensles->device.Free = rdpsnd_opensles_free;

	args = pEntryPoints->args;
	rdpsnd_opensles_parse_addin_args((rdpsndDevicePlugin*) opensles, args);

	if (!opensles->device_name)
		opensles->device_name = _strdup("default");

	opensles->rate = 22050;
	opensles->channels = 2;
	opensles->format = WAVE_FORMAT_ADPCM;

	opensles->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd,
			(rdpsndDevicePlugin*) opensles);

	DEBUG_SND("success");
	return 0;
}
