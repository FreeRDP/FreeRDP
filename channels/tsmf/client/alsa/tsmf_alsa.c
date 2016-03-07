/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - ALSA Audio Device
 *
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
#include <unistd.h>

#include <winpr/crt.h>

#include <alsa/asoundlib.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>

#include "tsmf_audio.h"

typedef struct _TSMFALSAAudioDevice
{
	ITSMFAudioDevice iface;

	char device[32];
	snd_pcm_t *out_handle;
	UINT32 source_rate;
	UINT32 actual_rate;
	UINT32 source_channels;
	UINT32 actual_channels;
	UINT32 bytes_per_sample;

	FREERDP_DSP_CONTEXT *dsp_context;
} TSMFAlsaAudioDevice;

static BOOL tsmf_alsa_open_device(TSMFAlsaAudioDevice *alsa)
{
	int error;
	error = snd_pcm_open(&alsa->out_handle, alsa->device, SND_PCM_STREAM_PLAYBACK, 0);
	if(error < 0)
	{
		WLog_ERR(TAG, "failed to open device %s", alsa->device);
		return FALSE;
	}
	DEBUG_TSMF("open device %s", alsa->device);
	return TRUE;
}

static BOOL tsmf_alsa_open(ITSMFAudioDevice *audio, const char *device)
{
	TSMFAlsaAudioDevice *alsa = (TSMFAlsaAudioDevice *) audio;
	if(!device)
	{
		strncpy(alsa->device, "default", sizeof(alsa->device));
	}
	else
	{
		strncpy(alsa->device, device, sizeof(alsa->device));
	}
	return tsmf_alsa_open_device(alsa);
}

static BOOL tsmf_alsa_set_format(ITSMFAudioDevice *audio,
								 UINT32 sample_rate, UINT32 channels, UINT32 bits_per_sample)
{
	int error;
	snd_pcm_uframes_t frames;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	TSMFAlsaAudioDevice *alsa = (TSMFAlsaAudioDevice *) audio;
	if(!alsa->out_handle)
		return FALSE;
	snd_pcm_drop(alsa->out_handle);
	alsa->actual_rate = alsa->source_rate = sample_rate;
	alsa->actual_channels = alsa->source_channels = channels;
	alsa->bytes_per_sample = bits_per_sample / 8;
	error = snd_pcm_hw_params_malloc(&hw_params);
	if(error < 0)
	{
		WLog_ERR(TAG, "snd_pcm_hw_params_malloc failed");
		return FALSE;
	}
	snd_pcm_hw_params_any(alsa->out_handle, hw_params);
	snd_pcm_hw_params_set_access(alsa->out_handle, hw_params,
								 SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(alsa->out_handle, hw_params,
								 SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_rate_near(alsa->out_handle, hw_params,
									&alsa->actual_rate, NULL);
	snd_pcm_hw_params_set_channels_near(alsa->out_handle, hw_params,
										&alsa->actual_channels);
	frames = sample_rate;
	snd_pcm_hw_params_set_buffer_size_near(alsa->out_handle, hw_params,
										   &frames);
	snd_pcm_hw_params(alsa->out_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);
	error = snd_pcm_sw_params_malloc(&sw_params);
	if(error < 0)
	{
		WLog_ERR(TAG, "snd_pcm_sw_params_malloc");
		return FALSE;
	}
	snd_pcm_sw_params_current(alsa->out_handle, sw_params);
	snd_pcm_sw_params_set_start_threshold(alsa->out_handle, sw_params,
										  frames / 2);
	snd_pcm_sw_params(alsa->out_handle, sw_params);
	snd_pcm_sw_params_free(sw_params);
	snd_pcm_prepare(alsa->out_handle);
	DEBUG_TSMF("sample_rate %d channels %d bits_per_sample %d",
			   sample_rate, channels, bits_per_sample);
	DEBUG_TSMF("hardware buffer %d frames", (int)frames);
	if((alsa->actual_rate != alsa->source_rate) ||
			(alsa->actual_channels != alsa->source_channels))
	{
		DEBUG_TSMF("actual rate %d / channel %d is different "
				   "from source rate %d / channel %d, resampling required.",
				   alsa->actual_rate, alsa->actual_channels,
				   alsa->source_rate, alsa->source_channels);
	}
	return TRUE;
}

static BOOL tsmf_alsa_play(ITSMFAudioDevice *audio, BYTE *data, UINT32 data_size)
{
	int len;
	int error;
	int frames;
	BYTE *end;
	BYTE *src;
	BYTE *pindex;
	int rbytes_per_frame;
	int sbytes_per_frame;
	TSMFAlsaAudioDevice *alsa = (TSMFAlsaAudioDevice *) audio;
	DEBUG_TSMF("data_size %d", data_size);
	if(alsa->out_handle)
	{
		sbytes_per_frame = alsa->source_channels * alsa->bytes_per_sample;
		rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_sample;
		if((alsa->source_rate == alsa->actual_rate) &&
				(alsa->source_channels == alsa->actual_channels))
		{
			src = data;
		}
		else
		{
			alsa->dsp_context->resample(alsa->dsp_context, data, alsa->bytes_per_sample,
										alsa->source_channels, alsa->source_rate, data_size / sbytes_per_frame,
										alsa->actual_channels, alsa->actual_rate);
			frames = alsa->dsp_context->resampled_frames;
			DEBUG_TSMF("resampled %d frames at %d to %d frames at %d",
					   data_size / sbytes_per_frame, alsa->source_rate, frames, alsa->actual_rate);
			data_size = frames * rbytes_per_frame;
			src = alsa->dsp_context->resampled_buffer;
		}
		pindex = src;
		end = pindex + data_size;
		while(pindex < end)
		{
			len = end - pindex;
			frames = len / rbytes_per_frame;
			error = snd_pcm_writei(alsa->out_handle, pindex, frames);
			if(error == -EPIPE)
			{
				snd_pcm_recover(alsa->out_handle, error, 0);
				error = 0;
			}
			else if(error < 0)
			{
				DEBUG_TSMF("error len %d", error);
				snd_pcm_close(alsa->out_handle);
				alsa->out_handle = 0;
				tsmf_alsa_open_device(alsa);
				break;
			}
			DEBUG_TSMF("%d frames played.", error);
			if(error == 0)
				break;
			pindex += error * rbytes_per_frame;
		}
	}
	free(data);
	return TRUE;
}

static UINT64 tsmf_alsa_get_latency(ITSMFAudioDevice *audio)
{
	UINT64 latency = 0;
	snd_pcm_sframes_t frames = 0;
	TSMFAlsaAudioDevice *alsa = (TSMFAlsaAudioDevice *) audio;
	if(alsa->out_handle && alsa->actual_rate > 0 &&
			snd_pcm_delay(alsa->out_handle, &frames) == 0 &&
			frames > 0)
	{
		latency = ((UINT64)frames) * 10000000LL / (UINT64) alsa->actual_rate;
	}
	return latency;
}

static BOOL tsmf_alsa_flush(ITSMFAudioDevice *audio)
{
	return TRUE;
}

static void tsmf_alsa_free(ITSMFAudioDevice *audio)
{
	TSMFAlsaAudioDevice *alsa = (TSMFAlsaAudioDevice *) audio;
	DEBUG_TSMF("");
	if(alsa->out_handle)
	{
		snd_pcm_drain(alsa->out_handle);
		snd_pcm_close(alsa->out_handle);
	}
	freerdp_dsp_context_free(alsa->dsp_context);
	free(alsa);
}

#ifdef STATIC_CHANNELS
#define freerdp_tsmf_client_audio_subsystem_entry	alsa_freerdp_tsmf_client_audio_subsystem_entry
#else
#define freerdp_tsmf_client_audio_subsystem_entry	FREERDP_API freerdp_tsmf_client_audio_subsystem_entry
#endif

ITSMFAudioDevice *freerdp_tsmf_client_audio_subsystem_entry(void)
{
	TSMFAlsaAudioDevice *alsa;
	alsa = (TSMFAlsaAudioDevice *) malloc(sizeof(TSMFAlsaAudioDevice));
	ZeroMemory(alsa, sizeof(TSMFAlsaAudioDevice));
	alsa->iface.Open = tsmf_alsa_open;
	alsa->iface.SetFormat = tsmf_alsa_set_format;
	alsa->iface.Play = tsmf_alsa_play;
	alsa->iface.GetLatency = tsmf_alsa_get_latency;
	alsa->iface.Flush = tsmf_alsa_flush;
	alsa->iface.Free = tsmf_alsa_free;
	alsa->dsp_context = freerdp_dsp_context_new();
	return (ITSMFAudioDevice *) alsa;
}
