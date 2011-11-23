/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_alsa_plugin rdpsndAlsaPlugin;
struct rdpsnd_alsa_plugin
{
	rdpsndDevicePlugin device;

	char* device_name;
	snd_pcm_t* out_handle;
	uint32 source_rate;
	uint32 actual_rate;
	snd_pcm_format_t format;
	uint32 source_channels;
	uint32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int block_size;
	int latency;
	ADPCM adpcm;
};

static void rdpsnd_alsa_set_params(rdpsndAlsaPlugin* alsa)
{
	snd_pcm_hw_params_t* hw_params;
	snd_pcm_sw_params_t* sw_params;
	int error;
	snd_pcm_uframes_t frames;
	snd_pcm_uframes_t start_threshold;

	snd_pcm_drop(alsa->out_handle);

	error = snd_pcm_hw_params_malloc(&hw_params);
	if (error < 0)
	{
		DEBUG_WARN("snd_pcm_hw_params_malloc failed");
		return;
	}
	snd_pcm_hw_params_any(alsa->out_handle, hw_params);
	snd_pcm_hw_params_set_access(alsa->out_handle, hw_params,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(alsa->out_handle, hw_params,
		alsa->format);
	snd_pcm_hw_params_set_rate_near(alsa->out_handle, hw_params,
		&alsa->actual_rate, NULL);
	snd_pcm_hw_params_set_channels_near(alsa->out_handle, hw_params,
		&alsa->actual_channels);
	if (alsa->latency < 0)
		frames = alsa->actual_rate * 4; /* Default to 4-second buffer */
	else
		frames = alsa->latency * alsa->actual_rate * 2 / 1000; /* Double of the latency */
	if (frames < alsa->actual_rate / 2)
		frames = alsa->actual_rate / 2; /* Minimum 0.5-second buffer */
	snd_pcm_hw_params_set_buffer_size_near(alsa->out_handle, hw_params,
		&frames);
	snd_pcm_hw_params(alsa->out_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);

	error = snd_pcm_sw_params_malloc(&sw_params);
	if (error < 0)
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
		(int)frames, (double)frames / 2.0 / (double)alsa->actual_rate);
	if ((alsa->actual_rate != alsa->source_rate) ||
		(alsa->actual_channels != alsa->source_channels))
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

static void rdpsnd_alsa_open(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	int error;

	if (alsa->out_handle != 0)
		return;

	DEBUG_SVC("opening");

	error = snd_pcm_open(&alsa->out_handle, alsa->device_name,
		SND_PCM_STREAM_PLAYBACK, 0);
	if (error < 0)
	{
		DEBUG_WARN("snd_pcm_open failed");
	}
	else
	{
		memset(&alsa->adpcm, 0, sizeof(ADPCM));
		rdpsnd_alsa_set_format(device, format, latency);
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
}

static void rdpsnd_alsa_free(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	rdpsnd_alsa_close(device);
	xfree(alsa->device_name);
	xfree(alsa);
}

static boolean rdpsnd_alsa_format_supported(rdpsndDevicePlugin* device, rdpsndFormat* format)
{
	switch (format->wFormatTag)
	{
		case 1: /* PCM */
			if (format->cbSize == 0 &&
				format->nSamplesPerSec <= 48000 &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return true;
			}
			break;

		case 0x11: /* IMA ADPCM */
			if (format->nSamplesPerSec <= 48000 &&
				format->wBitsPerSample == 4 &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return true;
			}
			break;
	}
	return false;
}

static void rdpsnd_alsa_set_volume(rdpsndDevicePlugin* device, uint32 value)
{
}

static void rdpsnd_alsa_play(rdpsndDevicePlugin* device, uint8* data, int size)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;
	uint8* decoded_data;
	int decoded_size;
	uint8* src;
	uint8* resampled_data;
	int len;
	int error;
	int frames;
	int rbytes_per_frame;
	int sbytes_per_frame;
	uint8* pindex;
	uint8* end;

	if (alsa->out_handle == 0)
		return;

	if (alsa->wformat == 0x11)
	{
		decoded_data = dsp_decode_ima_adpcm(&alsa->adpcm,
			data, size, alsa->source_channels, alsa->block_size, &decoded_size);
		size = decoded_size;
		src = decoded_data;
	}
	else
	{
		decoded_data = NULL;
		src = data;
	}

	sbytes_per_frame = alsa->source_channels * alsa->bytes_per_channel;
	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	if ((size % sbytes_per_frame) != 0)
	{
		DEBUG_WARN("error len mod");
		return;
	}

	if ((alsa->source_rate == alsa->actual_rate) &&
		(alsa->source_channels == alsa->actual_channels))
	{
		resampled_data = NULL;
	}
	else
	{
		resampled_data = dsp_resample(src, alsa->bytes_per_channel,
			alsa->source_channels, alsa->source_rate, size / sbytes_per_frame,
			alsa->actual_channels, alsa->actual_rate, &frames);
		DEBUG_SVC("resampled %d frames at %d to %d frames at %d",
			size / sbytes_per_frame, alsa->source_rate, frames, alsa->actual_rate);
		size = frames * rbytes_per_frame;
		src = resampled_data;
	}

	pindex = src;
	end = pindex + size;
	while (pindex < end)
	{
		len = end - pindex;
		frames = len / rbytes_per_frame;
		error = snd_pcm_writei(alsa->out_handle, pindex, frames);
		if (error == -EPIPE)
		{
			snd_pcm_recover(alsa->out_handle, error, 0);
			error = 0;
		}
		else if (error < 0)
		{
			DEBUG_WARN("error %d", error);
			snd_pcm_close(alsa->out_handle);
			alsa->out_handle = 0;
			rdpsnd_alsa_open(device, NULL, alsa->latency);
			break;
		}
		pindex += error * rbytes_per_frame;
	}

	if (resampled_data)
		xfree(resampled_data);
	if (decoded_data)
		xfree(decoded_data);
}

static void rdpsnd_alsa_start(rdpsndDevicePlugin* device)
{
	rdpsndAlsaPlugin* alsa = (rdpsndAlsaPlugin*)device;

	if (alsa->out_handle == 0)
		return;

	snd_pcm_start(alsa->out_handle);
}

int FreeRDPRdpsndDeviceEntry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	rdpsndAlsaPlugin* alsa;
	RDP_PLUGIN_DATA* data;

	alsa = xnew(rdpsndAlsaPlugin);

	alsa->device.Open = rdpsnd_alsa_open;
	alsa->device.FormatSupported = rdpsnd_alsa_format_supported;
	alsa->device.SetFormat = rdpsnd_alsa_set_format;
	alsa->device.SetVolume = rdpsnd_alsa_set_volume;
	alsa->device.Play = rdpsnd_alsa_play;
	alsa->device.Start = rdpsnd_alsa_start;
	alsa->device.Close = rdpsnd_alsa_close;
	alsa->device.Free = rdpsnd_alsa_free;

	data = pEntryPoints->plugin_data;
	if (data && strcmp((char*)data->data[0], "alsa") == 0)
	{
		alsa->device_name = xstrdup((char*)data->data[1]);
	}
	if (alsa->device_name == NULL)
	{
		alsa->device_name = xstrdup("default");
	}
	alsa->out_handle = 0;
	alsa->source_rate = 22050;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->source_channels = 2;
	alsa->actual_channels = 2;
	alsa->bytes_per_channel = 2;

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)alsa);

	return 0;
}

