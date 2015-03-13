/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/cmdline.h>

#include <alsa/asoundlib.h>

#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

typedef struct _AudinALSADevice
{
	IAudinDevice iface;

	char* device_name;
	UINT32 frames_per_packet;
	UINT32 target_rate;
	UINT32 actual_rate;
	snd_pcm_format_t format;
	UINT32 target_channels;
	UINT32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int block_size;

	FREERDP_DSP_CONTEXT* dsp_context;

	HANDLE thread;
	HANDLE stopEvent;

	BYTE* buffer;
	int buffer_frames;

	AudinReceive receive;
	void* user_data;
} AudinALSADevice;

static BOOL audin_alsa_set_params(AudinALSADevice* alsa, snd_pcm_t* capture_handle)
{
	int error;
	snd_pcm_hw_params_t* hw_params;

	if ((error = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		WLog_ERR(TAG, "snd_pcm_hw_params_malloc (%s)",
				 snd_strerror(error));
		return FALSE;
	}

	snd_pcm_hw_params_any(capture_handle, hw_params);
	snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(capture_handle, hw_params, alsa->format);
	snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &alsa->actual_rate, NULL);
	snd_pcm_hw_params_set_channels_near(capture_handle, hw_params, &alsa->actual_channels);
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

	return TRUE;
}

static BOOL audin_alsa_thread_receive(AudinALSADevice* alsa, BYTE* src, int size)
{
	int frames;
	int cframes;
	int ret = 0;
	int encoded_size;
	BYTE* encoded_data;
	int rbytes_per_frame;
	int tbytes_per_frame;

	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;

	if ((alsa->target_rate == alsa->actual_rate) &&
		(alsa->target_channels == alsa->actual_channels))
	{
		frames = size / rbytes_per_frame;
	}
	else
	{
		alsa->dsp_context->resample(alsa->dsp_context, src, alsa->bytes_per_channel,
			alsa->actual_channels, alsa->actual_rate, size / rbytes_per_frame,
			alsa->target_channels, alsa->target_rate);
		frames = alsa->dsp_context->resampled_frames;
		DEBUG_DVC("resampled %d frames at %d to %d frames at %d",
			size / rbytes_per_frame, alsa->actual_rate, frames, alsa->target_rate);
		size = frames * tbytes_per_frame;
		src = alsa->dsp_context->resampled_buffer;
	}

	while (frames > 0)
	{
		if (WaitForSingleObject(alsa->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		cframes = alsa->frames_per_packet - alsa->buffer_frames;

		if (cframes > frames)
			cframes = frames;

		CopyMemory(alsa->buffer + alsa->buffer_frames * tbytes_per_frame, src, cframes * tbytes_per_frame);

		alsa->buffer_frames += cframes;

		if (alsa->buffer_frames >= alsa->frames_per_packet)
		{
			if (alsa->wformat == WAVE_FORMAT_DVI_ADPCM)
			{
				alsa->dsp_context->encode_ima_adpcm(alsa->dsp_context,
					alsa->buffer, alsa->buffer_frames * tbytes_per_frame,
					alsa->target_channels, alsa->block_size);

				encoded_data = alsa->dsp_context->adpcm_buffer;
				encoded_size = alsa->dsp_context->adpcm_size;

				DEBUG_DVC("encoded %d to %d",
					alsa->buffer_frames * tbytes_per_frame, encoded_size);
			}
			else
			{
				encoded_data = alsa->buffer;
				encoded_size = alsa->buffer_frames * tbytes_per_frame;
			}

			if (WaitForSingleObject(alsa->stopEvent, 0) == WAIT_OBJECT_0)
				break;
			else
			{
				DEBUG_DVC("encoded %d [%d] to %d [%X]", alsa->buffer_frames,
						tbytes_per_frame, encoded_size,
						alsa->wformat);
				ret = alsa->receive(encoded_data, encoded_size, alsa->user_data);
			}

			alsa->buffer_frames = 0;

			if (!ret)
				break;
		}

		src += cframes * tbytes_per_frame;
		frames -= cframes;
	}

	return (ret) ? TRUE : FALSE;
}

static void* audin_alsa_thread_func(void* arg)
{
	int error;
	BYTE* buffer;
	int rbytes_per_frame;
	int tbytes_per_frame;
	snd_pcm_t* capture_handle = NULL;
	AudinALSADevice* alsa = (AudinALSADevice*) arg;

	DEBUG_DVC("in");

	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;
	buffer = (BYTE*) malloc(rbytes_per_frame * alsa->frames_per_packet);
	ZeroMemory(buffer, rbytes_per_frame * alsa->frames_per_packet);
	freerdp_dsp_context_reset_adpcm(alsa->dsp_context);

	do
	{
		if ((error = snd_pcm_open(&capture_handle, alsa->device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0)
		{
			WLog_ERR(TAG, "snd_pcm_open (%s)", snd_strerror(error));
			break;
		}

		if (!audin_alsa_set_params(alsa, capture_handle))
		{
			break;
		}

		while (!(WaitForSingleObject(alsa->stopEvent, 0) == WAIT_OBJECT_0))
		{
			error = snd_pcm_readi(capture_handle, buffer, alsa->frames_per_packet);

			if (error == -EPIPE)
			{
				snd_pcm_recover(capture_handle, error, 0);
				continue;
			}
			else if (error < 0)
			{
				WLog_ERR(TAG, "snd_pcm_readi (%s)", snd_strerror(error));
				break;
			}

			if (!audin_alsa_thread_receive(alsa, buffer, error * rbytes_per_frame))
				break;
		}
	}
	while (0);

	free(buffer);

	if (capture_handle)
		snd_pcm_close(capture_handle);

	DEBUG_DVC("out");
	ExitThread(0);

	return NULL;
}

static void audin_alsa_free(IAudinDevice* device)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	freerdp_dsp_context_free(alsa->dsp_context);

	free(alsa->device_name);

	free(alsa);
}

static BOOL audin_alsa_format_supported(IAudinDevice* device, audinFormat* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;

		case WAVE_FORMAT_DVI_ADPCM:
			if ((format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 4) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;
	}

	return FALSE;
}

static void audin_alsa_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
{
	int bs;
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	alsa->target_rate = format->nSamplesPerSec;
	alsa->actual_rate = format->nSamplesPerSec;
	alsa->target_channels = format->nChannels;
	alsa->actual_channels = format->nChannels;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
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

		case WAVE_FORMAT_DVI_ADPCM:
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
	int rbytes_per_frame;
	int tbytes_per_frame;
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	DEBUG_DVC("");

	alsa->receive = receive;
	alsa->user_data = user_data;

	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;
	alsa->buffer = (BYTE*) malloc(tbytes_per_frame * alsa->frames_per_packet);
	ZeroMemory(alsa->buffer, tbytes_per_frame * alsa->frames_per_packet);
	alsa->buffer_frames = 0;
	
	alsa->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	alsa->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) audin_alsa_thread_func, alsa, 0, NULL);
}

static void audin_alsa_close(IAudinDevice* device)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	DEBUG_DVC("");

	if (alsa->stopEvent)
	{
		SetEvent(alsa->stopEvent);
		WaitForSingleObject(alsa->thread, INFINITE);

		CloseHandle(alsa->stopEvent);
		alsa->stopEvent = NULL;

		CloseHandle(alsa->thread);
		alsa->thread = NULL;
	}

	if (alsa->buffer)
		free(alsa->buffer);
	alsa->buffer = NULL;

	alsa->receive = NULL;
	alsa->user_data = NULL;
}

COMMAND_LINE_ARGUMENT_A audin_alsa_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void audin_alsa_parse_addin_args(AudinALSADevice* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv, audin_alsa_args, flags, alsa, NULL, NULL);

	arg = audin_alsa_args;

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
#define freerdp_audin_client_subsystem_entry	alsa_freerdp_audin_client_subsystem_entry
#endif

int freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinALSADevice* alsa;

	alsa = (AudinALSADevice*) malloc(sizeof(AudinALSADevice));
	ZeroMemory(alsa, sizeof(AudinALSADevice));

	alsa->iface.Open = audin_alsa_open;
	alsa->iface.FormatSupported = audin_alsa_format_supported;
	alsa->iface.SetFormat = audin_alsa_set_format;
	alsa->iface.Close = audin_alsa_close;
	alsa->iface.Free = audin_alsa_free;

	args = pEntryPoints->args;

	audin_alsa_parse_addin_args(alsa, args);

	if (!alsa->device_name)
		alsa->device_name = _strdup("default");

	alsa->frames_per_packet = 128;
	alsa->target_rate = 22050;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->target_channels = 2;
	alsa->actual_channels = 2;
	alsa->bytes_per_channel = 2;

	alsa->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) alsa);

	return 0;
}
