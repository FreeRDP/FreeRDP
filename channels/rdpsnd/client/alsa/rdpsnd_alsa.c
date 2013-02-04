/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <alsa/asoundlib.h>

#include <freerdp/types.h>
#include <freerdp/utils/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_alsa_plugin rdpsndAlsaPlugin;

struct rdpsnd_alsa_plugin
{
	rdpsndDevicePlugin device;

	char* device_name;
	snd_pcm_t* out_handle;
	snd_mixer_t* mixer_handle;
	UINT32 source_rate;
	UINT32 actual_rate;
	snd_pcm_format_t format;
	UINT32 source_channels;
	UINT32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int block_size;
	int latency;

	FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_alsa_set_params(rdpsndAlsaPlugin* alsa)
{
	int status;
	snd_pcm_hw_params_t* hw_params;
	snd_pcm_sw_params_t* sw_params;
	snd_pcm_uframes_t frames;
	snd_pcm_uframes_t start_threshold;

	snd_pcm_drop(alsa->out_handle);

	status = snd_pcm_hw_params_malloc(&hw_params);

	if (status < 0)
	{
		DEBUG_WARN("snd_pcm_hw_params_malloc failed");
		return;
	}

	snd_pcm_hw_params_any(alsa->out_handle, hw_params);
	snd_pcm_hw_params_set_access(alsa->out_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(alsa->out_handle, hw_params, alsa->format);
	snd_pcm_hw_params_set_rate_near(alsa->out_handle, hw_params, &alsa->actual_rate, NULL);
	snd_pcm_hw_params_set_channels_near(alsa->out_handle, hw_params, &alsa->actual_channels);

	if (alsa->latency < 0)
		frames = alsa->actual_rate * 4 / 10; /* Default to 400ms buffer */
	else
		frames = alsa->latency * alsa->actual_rate * 2 / 1000; /* Double of the latency */

	if (frames < alsa->actual_rate / 2)
		frames = alsa->actual_rate / 2; /* Minimum 0.5-second buffer */

	snd_pcm_hw_params_set_buffer_size_near(alsa->out_handle, hw_params, &frames);
	snd_pcm_hw_params(alsa->out_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);

	status = snd_pcm_sw_params_malloc(&sw_params);

	if (status < 0)
	{
		DEBUG_WARN("snd_pcm_sw_params_malloc failed");
		return;
	}

	snd_pcm_sw_params_current(alsa->out_handle, sw_params);

	if (alsa->latency == 0)
		start_threshold = 0;
	else
		start_threshold = frames / 2;

	snd_pcm_sw_params_set_start_threshold(alsa->out_handle, sw_params, start_threshold);
	snd_pcm_sw_params(alsa->out_handle, sw_params);
	snd_pcm_sw_params_free(sw_params);

	snd_pcm_prepare(alsa->out_handle);

	DEBUG_SVC("hardware buffer %d frames, playback buffer %.2g seconds",
		(int) frames, (double) frames / 2.0 / (double) alsa->actual_rate);

	if ((alsa->actual_rate != alsa->source_rate) || (alsa->actual_channels != alsa->source_channels))
	{
		DEBUG_SVC("actual rate %d / channel %d is different from source rate %d / channel %d, resampling required.",
			alsa->actual_rate, alsa->actual_channels, alsa->source_rate, alsa->source_channels);
	}
}

static void rdpsnd_alsa_set_format(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (format != NULL)
	{
		alsa->source_rate = format->nSamplesPerSec;
		alsa->actual_rate = format->nSamplesPerSec;
		alsa->source_channels = format->nChannels;
		alsa->actual_channels = format->nChannels;

		switch (format->wFormatTag)
		{
			case 1: /* PCM */
				switch (format->wBitsPerSample)
				{
					case 8:
						alsa->format = SND_PCM_FORMAT_S8;
						alsa->bytes_per_channel = 1;
						break;
					case 16:
						alsa->format = SND_PCM_FORMAT_S16_LE;
						alsa->bytes_per_channel = 2;
						break;
				}
				break;

			case 2: /* MS ADPCM */
			case 0x11: /* IMA ADPCM */
				alsa->format = SND_PCM_FORMAT_S16_LE;
				alsa->bytes_per_channel = 2;
				break;
		}

		alsa->wformat = format->wFormatTag;
		alsa->block_size = format->nBlockAlign;
	}

	alsa->latency = latency;

	rdpsnd_alsa_set_params(alsa);
}

static void rdpsnd_alsa_open_mixer(rdpsndAlsaPlugin* alsa)
{
	int status;
	snd_mixer_t* handle;

	status = snd_mixer_open(&handle, 0);

	if (status < 0)
	{
		DEBUG_WARN("snd_mixer_open failed");
		return;
	}

	status = snd_mixer_attach(handle, alsa->device_name);

	if (status < 0)
	{
		DEBUG_WARN("snd_mixer_attach failed");
		snd_mixer_close(handle);
		return;
	}

	status = snd_mixer_selem_register(handle, NULL, NULL);

	if (status < 0)
	{
		DEBUG_WARN("snd_mixer_selem_register failed");
		snd_mixer_close(handle);
		return;
	}

	status = snd_mixer_load(handle);

	if (status < 0)
	{
		DEBUG_WARN("snd_mixer_load failed");
		snd_mixer_close(handle);
		return;
	}

	alsa->mixer_handle = handle;
}

static void rdpsnd_alsa_open(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	int status;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*) device;

	if (alsa->out_handle != 0)
		return;

	DEBUG_SVC("opening");

	status = snd_pcm_open(&alsa->out_handle, alsa->device_name, SND_PCM_STREAM_PLAYBACK, 0);

	if (status < 0)
	{
		DEBUG_WARN("snd_pcm_open failed");
	}
	else
	{
		freerdp_dsp_context_reset_adpcm(alsa->dsp_context);
		rdpsnd_alsa_set_format(device, format, latency);
		rdpsnd_alsa_open_mixer(alsa);
	}
}

static void rdpsnd_alsa_close(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (alsa->out_handle != 0)
	{
		DEBUG_SVC("close");
		snd_pcm_drain(alsa->out_handle);
		snd_pcm_close(alsa->out_handle);
		alsa->out_handle = 0;
	}

	if (alsa->mixer_handle)
	{
		snd_mixer_close(alsa->mixer_handle);
		alsa->mixer_handle = NULL;
	}
}

static void rdpsnd_alsa_free(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	rdpsnd_alsa_close(device);
	free(alsa->device_name);
	freerdp_dsp_context_free(alsa->dsp_context);
	free(alsa);
}

static BOOL rdpsnd_alsa_format_supported(rdpsndDevicePlugin* device, rdpsndFormat* format)
{
	switch (format->wFormatTag)
	{
		case 1: /* PCM */
			if (format->cbSize == 0 &&
				format->nSamplesPerSec <= 48000 &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;

		case 2: /* MS ADPCM */
		case 0x11: /* IMA ADPCM */
			if (format->nSamplesPerSec <= 48000 &&
				format->wBitsPerSample == 4 &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;
	}
	return FALSE;
}

static void rdpsnd_alsa_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	long left;
	long right;
	long volume_min;
	long volume_max;
	long volume_left;
	long volume_right;
	snd_mixer_elem_t* elem;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*) device;

	if (!alsa->mixer_handle)
		return;

	left = (value & 0xFFFF);
	right = ((value >> 16) & 0xFFFF);
	
	for (elem = snd_mixer_first_elem(alsa->mixer_handle); elem; elem = snd_mixer_elem_next(elem))
	{
		if (snd_mixer_selem_has_playback_volume(elem))
		{
			snd_mixer_selem_get_playback_volume_range(elem, &volume_min, &volume_max);
			volume_left = volume_min + (left * (volume_max - volume_min)) / 0xFFFF;
			volume_right = volume_min + (right * (volume_max - volume_min)) / 0xFFFF;
			snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, volume_left);
			snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, volume_right);
		}
	}
}

static void rdpsnd_alsa_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	BYTE* src;
	int len;
	int status;
	int frames;
	int rbytes_per_frame;
	int sbytes_per_frame;
	BYTE* pindex;
	BYTE* end;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*) device;

	if (alsa->out_handle == 0)
		return;

	if (alsa->wformat == 2)
	{
		alsa->dsp_context->decode_ms_adpcm(alsa->dsp_context,
			data, size, alsa->source_channels, alsa->block_size);
		size = alsa->dsp_context->adpcm_size;
		src = alsa->dsp_context->adpcm_buffer;
	}
	else if (alsa->wformat == 0x11)
	{
		alsa->dsp_context->decode_ima_adpcm(alsa->dsp_context,
			data, size, alsa->source_channels, alsa->block_size);
		size = alsa->dsp_context->adpcm_size;
		src = alsa->dsp_context->adpcm_buffer;
	}
	else
	{
		src = data;
	}

	sbytes_per_frame = alsa->source_channels * alsa->bytes_per_channel;
	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;

	if ((size % sbytes_per_frame) != 0)
	{
		DEBUG_WARN("error len mod");
		return;
	}

	if ((alsa->source_rate == alsa->actual_rate) && (alsa->source_channels == alsa->actual_channels))
	{
	}
	else
	{
		alsa->dsp_context->resample(alsa->dsp_context, src, alsa->bytes_per_channel,
			alsa->source_channels, alsa->source_rate, size / sbytes_per_frame,
			alsa->actual_channels, alsa->actual_rate);
		frames = alsa->dsp_context->resampled_frames;
		DEBUG_SVC("resampled %d frames at %d to %d frames at %d",
			size / sbytes_per_frame, alsa->source_rate, frames, alsa->actual_rate);
		size = frames * rbytes_per_frame;
		src = alsa->dsp_context->resampled_buffer;
	}

	pindex = src;
	end = pindex + size;

	while (pindex < end)
	{
		len = end - pindex;
		frames = len / rbytes_per_frame;
		status = snd_pcm_writei(alsa->out_handle, pindex, frames);

		if (status == -EPIPE)
		{
			snd_pcm_recover(alsa->out_handle, status, 0);
			status = 0;
		}
		else if (status < 0)
		{
			DEBUG_WARN("status %d", status);
			snd_pcm_close(alsa->out_handle);
			alsa->out_handle = 0;
			rdpsnd_alsa_open(device, NULL, alsa->latency);
			break;
		}

		pindex += status * rbytes_per_frame;
	}
}

static void rdpsnd_alsa_start(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*) device;

	if (alsa->out_handle == 0)
		return;

	snd_pcm_start(alsa->out_handle);
}

COMMAND_LINE_ARGUMENT_A rdpsnd_alsa_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void rdpsnd_alsa_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*) device;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv, rdpsnd_alsa_args, flags, alsa, NULL, NULL);

	arg = rdpsnd_alsa_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			alsa->device_name = _strdup(arg->Value);
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	alsa_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndAlsaPlugin* alsa;

	alsa = (rdpsndAlsaPlugin*) malloc(sizeof(rdpsndAlsaPlugin));
	ZeroMemory(alsa, sizeof(rdpsndAlsaPlugin));

	alsa->device.Open = rdpsnd_alsa_open;
	alsa->device.FormatSupported = rdpsnd_alsa_format_supported;
	alsa->device.SetFormat = rdpsnd_alsa_set_format;
	alsa->device.SetVolume = rdpsnd_alsa_set_volume;
	alsa->device.Play = rdpsnd_alsa_play;
	alsa->device.Start = rdpsnd_alsa_start;
	alsa->device.Close = rdpsnd_alsa_close;
	alsa->device.Free = rdpsnd_alsa_free;

	args = pEntryPoints->args;
	rdpsnd_alsa_parse_addin_args((rdpsndDevicePlugin*) alsa, args);

	if (!alsa->device_name)
		alsa->device_name = _strdup("default");

	alsa->out_handle = 0;
	alsa->source_rate = 22050;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->source_channels = 2;
	alsa->actual_channels = 2;
	alsa->bytes_per_channel = 2;

	alsa->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) alsa);

	return 0;
}
