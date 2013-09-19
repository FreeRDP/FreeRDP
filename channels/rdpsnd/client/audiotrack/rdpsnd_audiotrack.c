/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Felix Long 
 * Copyright 2013 Armin Novak
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

#include <audiotrack.h>

#include <freerdp/types.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/codec/dsp.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_audiotrack_plugin rdpsndAudioTrackPlugin;
struct rdpsnd_audiotrack_plugin
{
	rdpsndDevicePlugin device;

	AUDIO_DRIVER_HANDLE out_handle;

	UINT32 source_rate;
	UINT32 actual_rate;
	int format;
	UINT32 source_channels;
	UINT32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int block_size;
	int latency;

	FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_audiotrack_set_format(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;

	if (format != NULL)
	{
		audiotrack->source_rate = format->nSamplesPerSec;
		audiotrack->actual_rate = format->nSamplesPerSec;
		audiotrack->source_channels = format->nChannels;
		audiotrack->actual_channels = format->nChannels;
		switch (format->wFormatTag)
		{
			case 1: /* PCM */
				switch (format->wBitsPerSample)
				{
					case 8:
						audiotrack->format = PCM_8_BIT;
						audiotrack->bytes_per_channel = 1;
						break;
					case 16:
						audiotrack->format = PCM_16_BIT;
						audiotrack->bytes_per_channel = 2;
						break;
				}
				break;

			case 2: /* MS ADPCM */
			case 0x11: /* IMA ADPCM */
				audiotrack->format = PCM_16_BIT;
				audiotrack->bytes_per_channel = 2;
				break;
		}
		audiotrack->wformat = format->wFormatTag;
		audiotrack->block_size = format->nBlockAlign;
	}
	audiotrack->latency = latency;
        
    freerdp_android_at_set(audiotrack->out_handle,
                           MUSIC, 
                           audiotrack->actual_rate, 
                           audiotrack->format, 
                           audiotrack->actual_channels == 2 ? CHANNEL_OUT_STEREO : CHANNEL_OUT_MONO);
}

static void rdpsnd_audiotrack_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;
	int error;

	if (audiotrack->out_handle != 0)
		return;

	DEBUG_SVC("opening");

    error = freerdp_android_at_open(&audiotrack->out_handle);

	if (error < 0)
	{
		DEBUG_WARN("freerdp_android_at_open failed");
	}
	else
	{
		freerdp_dsp_context_reset_adpcm(audiotrack->dsp_context);
		rdpsnd_audiotrack_set_format(device, format, latency);
	}
}

static void rdpsnd_audiotrack_close(rdpsndDevicePlugin* device)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;

	if (audiotrack->out_handle != 0)
	{
		DEBUG_SVC("close");
		freerdp_android_at_close(audiotrack->out_handle);
		audiotrack->out_handle = 0;
	}
}

static void rdpsnd_audiotrack_free(rdpsndDevicePlugin* device)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;

	rdpsnd_audiotrack_close(device);
	freerdp_dsp_context_free(audiotrack->dsp_context);
	free(audiotrack);
}

static BOOL rdpsnd_audiotrack_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
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

static void rdpsnd_audiotrack_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;
	float left;
	float right;
    
	if (audiotrack->out_handle == 0)
		return;
    
	left = ((value & 0xFFFF) * 1.0) / 0xFFFF;
	right = (((value >> 16) & 0xFFFF) * 1.0) / 0xFFFF;
	freerdp_android_at_set_volume(audiotrack->out_handle, left, right);
}

static void rdpsnd_audiotrack_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;
	BYTE* src;
	int len;
	int error;
	int frames;
	int rbytes_per_frame;
	int sbytes_per_frame;
	BYTE* pindex;
	BYTE* end;

	if (audiotrack->out_handle == 0)
		return;

	if (audiotrack->wformat == 2)
	{
		audiotrack->dsp_context->decode_ms_adpcm(audiotrack->dsp_context,
			data, size, audiotrack->source_channels, audiotrack->block_size);
		size = audiotrack->dsp_context->adpcm_size;
		src = audiotrack->dsp_context->adpcm_buffer;
	}
	else if (audiotrack->wformat == 0x11)
	{
		audiotrack->dsp_context->decode_ima_adpcm(audiotrack->dsp_context,
			data, size, audiotrack->source_channels, audiotrack->block_size);
		size = audiotrack->dsp_context->adpcm_size;
		src = audiotrack->dsp_context->adpcm_buffer;
	}
	else
	{
		src = data;
	}

	sbytes_per_frame = audiotrack->source_channels * audiotrack->bytes_per_channel;
	rbytes_per_frame = audiotrack->actual_channels * audiotrack->bytes_per_channel;
	if ((size % sbytes_per_frame) != 0)
	{
		DEBUG_WARN("error len mod");
		return;
	}

	if ((audiotrack->source_rate == audiotrack->actual_rate) &&
		(audiotrack->source_channels == audiotrack->actual_channels))
	{
	}
	else
	{
		audiotrack->dsp_context->resample(audiotrack->dsp_context, src, audiotrack->bytes_per_channel,
			audiotrack->source_channels, audiotrack->source_rate, size / sbytes_per_frame,
			audiotrack->actual_channels, audiotrack->actual_rate);
		frames = audiotrack->dsp_context->resampled_frames;
		DEBUG_SVC("resampled %d frames at %d to %d frames at %d",
			size / sbytes_per_frame, audiotrack->source_rate, frames, audiotrack->actual_rate);
		size = frames * rbytes_per_frame;
		src = audiotrack->dsp_context->resampled_buffer;
	}

	pindex = src;
	end = pindex + size;
	while (pindex < end)
	{
		len = end - pindex;
		
		error = freerdp_android_at_write(audiotrack->out_handle, pindex, len);
		if (error < 0)
		{
			DEBUG_WARN("error %d", error);
			freerdp_android_at_close(audiotrack->out_handle);
			audiotrack->out_handle = 0;
			rdpsnd_audiotrack_open(device, NULL, audiotrack->latency);
			break;
		}
		pindex += error;
	}
}

static void rdpsnd_audiotrack_start(rdpsndDevicePlugin* device)
{
	rdpsndAudioTrackPlugin* audiotrack = (rdpsndAudioTrackPlugin*)device;

	if (audiotrack->out_handle == 0)
		return;

	freerdp_android_at_start(audiotrack->out_handle);
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	audiotrack_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	rdpsndAudioTrackPlugin* audiotrack;

    freerdp_android_at_init_library();
	audiotrack = malloc(sizeof(rdpsndAudioTrackPlugin));

	audiotrack->device.Open = rdpsnd_audiotrack_open;
	audiotrack->device.FormatSupported = rdpsnd_audiotrack_format_supported;
	audiotrack->device.SetFormat = rdpsnd_audiotrack_set_format;
	audiotrack->device.SetVolume = rdpsnd_audiotrack_set_volume;
	audiotrack->device.Play = rdpsnd_audiotrack_play;
	audiotrack->device.Start = rdpsnd_audiotrack_start;
	audiotrack->device.Close = rdpsnd_audiotrack_close;
	audiotrack->device.Free = rdpsnd_audiotrack_free;

	audiotrack->out_handle = 0;
	audiotrack->source_rate = 22050;
	audiotrack->actual_rate = 22050;
	audiotrack->format = PCM_16_BIT;
	audiotrack->source_channels = 2;
	audiotrack->actual_channels = 2;
	audiotrack->bytes_per_channel = 2;

	audiotrack->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)audiotrack);

	return 0;
}

