/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Input Redirection Virtual Channel - ALSA implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/dsp.h>

#include "audin_main.h"

typedef struct _AudinALSADevice
{
	IAudinDevice iface;

	char device_name[32];
	uint32 frames_per_packet;
	uint32 target_rate;
	uint32 actual_rate;
	snd_pcm_format_t format;
	uint32 target_channels;
	uint32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int block_size;
	ADPCM adpcm;

	freerdp_thread* thread;

	uint8* buffer;
	int buffer_frames;

	AudinReceive receive;
	void* user_data;
} AudinALSADevice;

static boolean audin_alsa_set_params(AudinALSADevice* alsa, snd_pcm_t* capture_handle)
{
	int error;
	snd_pcm_hw_params_t* hw_params;

	if ((error = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		DEBUG_WARN("snd_pcm_hw_params_malloc (%s)",
			 snd_strerror(error));
		return false;
	}
	snd_pcm_hw_params_any(capture_handle, hw_params);
	snd_pcm_hw_params_set_access(capture_handle, hw_params,
		SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(capture_handle, hw_params,
		alsa->format);
	snd_pcm_hw_params_set_rate_near(capture_handle, hw_params,
		&alsa->actual_rate, NULL);
	snd_pcm_hw_params_set_channels_near(capture_handle, hw_params,
		&alsa->actual_channels);
	snd_pcm_hw_params(capture_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);
	snd_pcm_prepare(capture_handle);

	if ((alsa->actual_rate != alsa->target_rate) ||
		(alsa->actual_channels != alsa->target_channels))
	{
		DEBUG_DVC("actual rate %d / channel %d is "
			"different from target rate %d / channel %d, resampling required.",
			alsa->actual_rate, alsa->actual_channels,
			alsa->target_rate, alsa->target_channels);
	}
	return true;
}

static boolean audin_alsa_thread_receive(AudinALSADevice* alsa, uint8* src, int size)
{
	int frames;
	int cframes;
	int ret = 0;
	int encoded_size;
	uint8* encoded_data;
	int rbytes_per_frame;
	int tbytes_per_frame;
	uint8* resampled_data;

	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;

	if ((alsa->target_rate == alsa->actual_rate) &&
		(alsa->target_channels == alsa->actual_channels))
	{
		resampled_data = NULL;
		frames = size / rbytes_per_frame;
	}
	else
	{
		resampled_data = dsp_resample(src, alsa->bytes_per_channel,
			alsa->actual_channels, alsa->actual_rate, size / rbytes_per_frame,
			alsa->target_channels, alsa->target_rate, &frames);
		DEBUG_DVC("resampled %d frames at %d to %d frames at %d",
			size / rbytes_per_frame, alsa->actual_rate, frames, alsa->target_rate);
		size = frames * tbytes_per_frame;
		src = resampled_data;
	}

	while (frames > 0)
	{
		if (freerdp_thread_is_stopped(alsa->thread))
			break;

		cframes = alsa->frames_per_packet - alsa->buffer_frames;
		if (cframes > frames)
			cframes = frames;
		memcpy(alsa->buffer + alsa->buffer_frames * tbytes_per_frame,
			src, cframes * tbytes_per_frame);
		alsa->buffer_frames += cframes;
		if (alsa->buffer_frames >= alsa->frames_per_packet)
		{
			if (alsa->wformat == 0x11)
			{
				encoded_data = dsp_encode_ima_adpcm(&alsa->adpcm,
					alsa->buffer, alsa->buffer_frames * tbytes_per_frame,
					alsa->target_channels, alsa->block_size, &encoded_size);
				DEBUG_DVC("encoded %d to %d",
					alsa->buffer_frames * tbytes_per_frame, encoded_size);
			}
			else
			{
				encoded_data = alsa->buffer;
				encoded_size = alsa->buffer_frames * tbytes_per_frame;
			}

			if (freerdp_thread_is_stopped(alsa->thread))
			{
				ret = 0;
				frames = 0;
			}
			else
				ret = alsa->receive(encoded_data, encoded_size, alsa->user_data);
			alsa->buffer_frames = 0;
			if (encoded_data != alsa->buffer)
				xfree(encoded_data);
			if (!ret)
				break;
		}
		src += cframes * tbytes_per_frame;
		frames -= cframes;
	}

	if (resampled_data)
		xfree(resampled_data);

	return ret;
}

static void* audin_alsa_thread_func(void* arg)
{
	int error;
	uint8* buffer;
	int rbytes_per_frame;
	int tbytes_per_frame;
	snd_pcm_t* capture_handle = NULL;
	AudinALSADevice* alsa = (AudinALSADevice*) arg;

	DEBUG_DVC("in");

	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;
	alsa->buffer = (uint8*) xzalloc(tbytes_per_frame * alsa->frames_per_packet);
	alsa->buffer_frames = 0;
	buffer = (uint8*) xzalloc(rbytes_per_frame * alsa->frames_per_packet);
	memset(&alsa->adpcm, 0, sizeof(ADPCM));
	do
	{
		if ((error = snd_pcm_open(&capture_handle, alsa->device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0)
		{
			DEBUG_WARN("snd_pcm_open (%s)", snd_strerror(error));
			break;
		}
		if (!audin_alsa_set_params(alsa, capture_handle))
		{
			break;
		}

		while (!freerdp_thread_is_stopped(alsa->thread))
		{
			error = snd_pcm_readi(capture_handle, buffer, alsa->frames_per_packet);
			if (error == -EPIPE)
			{
				snd_pcm_recover(capture_handle, error, 0);
				continue;
			}
			else if (error < 0)
			{
				DEBUG_WARN("snd_pcm_readi (%s)", snd_strerror(error));
				break;
			}
			if (!audin_alsa_thread_receive(alsa, buffer, error * rbytes_per_frame))
				break;
		}
	} while (0);

	xfree(buffer);
	xfree(alsa->buffer);
	alsa->buffer = NULL;
	if (capture_handle)
		snd_pcm_close(capture_handle);

	freerdp_thread_quit(alsa->thread);

	DEBUG_DVC("out");

	return NULL;
}

static void audin_alsa_free(IAudinDevice* device)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	freerdp_thread_free(alsa->thread);
	xfree(alsa);
}

static boolean audin_alsa_format_supported(IAudinDevice* device, audinFormat* format)
{
	switch (format->wFormatTag)
	{
		case 1: /* PCM */
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return true;
			}
			break;

		case 0x11: /* IMA ADPCM */
			if ((format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 4) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return true;
			}
			break;
	}
	return false;
}

static void audin_alsa_set_format(IAudinDevice* device, audinFormat* format, uint32 FramesPerPacket)
{
	int bs;
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	alsa->target_rate = format->nSamplesPerSec;
	alsa->actual_rate = format->nSamplesPerSec;
	alsa->target_channels = format->nChannels;
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
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			alsa->frames_per_packet = (alsa->frames_per_packet * format->nChannels * 2 /
				bs + 1) * bs / (format->nChannels * 2);
			DEBUG_DVC("aligned FramesPerPacket=%d",
				alsa->frames_per_packet);
			break;
	}
	alsa->wformat = format->wFormatTag;
	alsa->block_size = format->nBlockAlign;
}

static void audin_alsa_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	DEBUG_DVC("");

	alsa->receive = receive;
	alsa->user_data = user_data;

	freerdp_thread_start(alsa->thread, audin_alsa_thread_func, alsa);
}

static void audin_alsa_close(IAudinDevice* device)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	DEBUG_DVC("");

	freerdp_thread_stop(alsa->thread);

	alsa->receive = NULL;
	alsa->user_data = NULL;
}

int FreeRDPAudinDeviceEntry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	AudinALSADevice* alsa;
	RDP_PLUGIN_DATA* data;

	alsa = xnew(AudinALSADevice);

	alsa->iface.Open = audin_alsa_open;
	alsa->iface.FormatSupported = audin_alsa_format_supported;
	alsa->iface.SetFormat = audin_alsa_set_format;
	alsa->iface.Close = audin_alsa_close;
	alsa->iface.Free = audin_alsa_free;

	data = pEntryPoints->plugin_data;
	if (data && data->data[0] && strcmp(data->data[0], "audin") == 0 &&
		data->data[1] && strcmp(data->data[1], "alsa") == 0)
	{
		if (data[2].size)
			strncpy(alsa->device_name, (char*)data->data[2], sizeof(alsa->device_name));
	}
	if (alsa->device_name[0] == '\0')
	{
		strcpy(alsa->device_name, "default");
	}

	alsa->frames_per_packet = 128;
	alsa->target_rate = 22050;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->target_channels = 2;
	alsa->actual_channels = 2;
	alsa->bytes_per_channel = 2;
	alsa->thread = freerdp_thread_new();

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) alsa);

	return 0;
}

