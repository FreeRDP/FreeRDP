/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - OpenSL ES implementation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/rdpsnd.h>

#include <SLES/OpenSLES.h>

#include "opensl_io.h"
#include "audin_main.h"

typedef struct _AudinOpenSLESDevice
{
	IAudinDevice iface;

	char* device_name;
	OPENSL_STREAM *stream;

	UINT32 frames_per_packet;
	UINT32 target_rate;
	UINT32 actual_rate;
	UINT32 target_channels;
	UINT32 actual_channels;
	int bytes_per_channel;
	int wformat;
	int format;
	int block_size;

	FREERDP_DSP_CONTEXT* dsp_context;

	HANDLE thread;
	HANDLE stopEvent;

	void* user_data;
} AudinOpenSLESDevice;

static BOOL audin_opensles_thread_receive(AudinOpenSLESDevice* opensles,
		void* src, int count)
{
	int frames;
	int cframes;
	int ret = 0;
	int encoded_size;
	BYTE* encoded_data;
	int rbytes_per_frame;
	int tbytes_per_frame;

	rbytes_per_frame = opensles->actual_channels * opensles->bytes_per_channel;
	tbytes_per_frame = opensles->target_channels * opensles->bytes_per_channel;

	if ((opensles->target_rate == opensles->actual_rate) &&
		(opensles->target_channels == opensles->actual_channels))
	{
		frames = size / rbytes_per_frame;
	}
	else
	{
		opensles->dsp_context->resample(opensles->dsp_context, src,
				opensles->bytes_per_channel,	opensles->actual_channels,
				opensles->actual_rate, size / rbytes_per_frame,
				opensles->target_channels, opensles->target_rate);
		frames = opensles->dsp_context->resampled_frames;
		
		DEBUG_DVC("resampled %d frames at %d to %d frames at %d",
			size / rbytes_per_frame, opensles->actual_rate,
			frames, opensles->target_rate);
		
		size = frames * tbytes_per_frame;
		src = opensles->dsp_context->resampled_buffer;
	}

	while (frames > 0)
	{
		if (WaitForSingleObject(opensles->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		cframes = opensles->frames_per_packet - opensles->buffer_frames;

		if (cframes > frames)
			cframes = frames;

		CopyMemory(opensles->buffer + opensles->buffer_frames * tbytes_per_frame,
				src, cframes * tbytes_per_frame);

		opensles->buffer_frames += cframes;

		if (opensles->buffer_frames >= opensles->frames_per_packet)
		{
			if (opensles->wformat == WAVE_FORMAT_DVI_ADPCM)
			{
				opensles->dsp_context->encode_ima_adpcm(opensles->dsp_context,
					opensles->buffer, opensles->buffer_frames * tbytes_per_frame,
					opensles->target_channels, opensles->block_size);

				encoded_data = opensles->dsp_context->adpcm_buffer;
				encoded_size = opensles->dsp_context->adpcm_size;

				DEBUG_DVC("encoded %d to %d",
					opensles->buffer_frames * tbytes_per_frame, encoded_size);
			}
			else
			{
				encoded_data = opensles->buffer;
				encoded_size = opensles->buffer_frames * tbytes_per_frame;
			}

			if (WaitForSingleObject(opensles->stopEvent, 0) == WAIT_OBJECT_0)
				break;
			else
				ret = opensles->receive(encoded_data, encoded_size,
						opensles->user_data);

			opensles->buffer_frames = 0;

			if (!ret)
				break;
		}

		src += cframes * tbytes_per_frame;
		frames -= cframes;
	}

	return (ret) ? TRUE : FALSE;
}

static void* audin_opensles_thread_func(void* arg)
{
	float* buffer;
	int rbytes_per_frame;
	int tbytes_per_frame;
	snd_pcm_t* capture_handle = NULL;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) arg;

	DEBUG_SND("opensles=%p", opensles);

	buffer = (BYTE*) calloc(sizeof(float), opensles->frames_per_packet);
	ZeroMemory(buffer, opensles->frames_per_packet);
	freerdp_dsp_context_reset_adpcm(opensles->dsp_context);

	while (!(WaitForSingleObject(opensles->stopEvent, 0) == WAIT_OBJECT_0))
	{
		int rc = android_AudioIn(opensles->stream, buffer,
				opensles->frames_per_packet);
		if (rc < 0)
		{
			DEBUG_WARN("snd_pcm_readi (%s)", snd_strerror(error));
			break;
		}

		if (!audin_opensles_thread_receive(opensles, buffer, rc * sizeof(float)))
			break;
	}

	free(buffer);

	DEBUG_DVC("thread shutdown.");

	ExitThread(0);
	return NULL;
}

static void audin_opensles_free(IAudinDevice* device)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p", device);

	freerdp_dsp_context_free(opensles->dsp_context);

	free(opensles->device_name);
	free(opensles);
}

static BOOL audin_opensles_format_supported(IAudinDevice* device, audinFormat* format)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;
	SLResult rc;
	
	DEBUG_DVC("device=%p, format=%p", device, format);

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

static void audin_opensles_set_format(IAudinDevice* device,
		audinFormat* format, UINT32 FramesPerPacket)
{
	int bs;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, format=%p, FramesPerPacket=%d",
			device, format, FramesPerPacket);

	opensles->target_rate = format->nSamplesPerSec;
	opensles->actual_rate = format->nSamplesPerSec;
	opensles->target_channels = format->nChannels;
	opensles->actual_channels = format->nChannels;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			switch (format->wBitsPerSample)
			{
				case 8:
					opensles->format = SND_PCM_FORMAT_S8;
					opensles->bytes_per_channel = 1;
					break;
				case 16:
					opensles->format = SND_PCM_FORMAT_S16_LE;
					opensles->bytes_per_channel = 2;
					break;
			}
			break;

		case WAVE_FORMAT_DVI_ADPCM:
			opensles->format = SND_PCM_FORMAT_S16_LE;
			opensles->bytes_per_channel = 2;
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			opensles->frames_per_packet =
				(opensles->frames_per_packet * format->nChannels * 2 /
				bs + 1) * bs / (format->nChannels * 2);
			DEBUG_DVC("aligned FramesPerPacket=%d",
				opensles->frames_per_packet);
			break;
	}

	opensles->wformat = format->wFormatTag;
	opensles->block_size = format->nBlockAlign;
}

static int audin_opensles_open(IAudinDevice* device, AudinReceive receive,
		void* user_data)
{
	int status = 0;
	int rbytes_per_frame;
	int tbytes_per_frame;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, receive=%d, user_data=%p", device, receive, user_data);

	opensles->stream = android_OpenAudioDevice(opensles->target_rate,
			opensles->target_channels, 0, opensles->frames_per_packet);

	opensles->receive = receive;
	opensles->user_data = user_data;

	rbytes_per_frame = opensles->actual_channels * opensles->bytes_per_channel;
	tbytes_per_frame = opensles->target_channels * opensles->bytes_per_channel;
	opensles->buffer =
		(BYTE*) malloc(tbytes_per_frame * opensles->frames_per_packet);
	ZeroMemory(opensles->buffer,
			tbytes_per_frame * opensles->frames_per_packet);
	opensles->buffer_frames = 0;
	
	opensles->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	opensles->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) audin_opensles_thread_func,
			opensles, 0, NULL);
}

static void audin_opensles_close(IAudinDevice* device)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p", device);

	SetEvent(opensles->stopEvent);
	WaitForSingleObject(opensles->thread, INFINITE);
	CloseHandle(opensles->stopEvent);
	CloseHandle(opensles->thread);

	android_CloseAudioDevice(opensles->stream);

	opensles->stopEvent = NULL;
	opensles->thread = NULL;
	opensles->receive = NULL;
	opensles->user_data = NULL;
	opsnsles->stream = NULL;
}

static const COMMAND_LINE_ARGUMENT_A audin_opensles_args[] =
{
	{ "audio-dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static int audin_opensles_parse_addin_args(AudinOpenSLESDevice* device,
		ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, args=%p", device, args);

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			audin_opensles_args, flags, opensles, NULL, NULL);
	if (status < 0)
		return status;

	arg = audin_opensles_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "audio-dev")
		{
			opensles->device_name = _strdup(arg->Value);
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return status;
}

#ifdef STATIC_CHANNELS
#define freerdp_audin_client_subsystem_entry \
	opensles_freerdp_audin_client_subsystem_entry
#endif

int freerdp_audin_client_subsystem_entry(
		PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinOpenSLESDevice* opensles;

	DEBUG_DVC("pEntryPoints=%p", pEntryPoints);

	opensles = (AudinOpenSLESDevice*) malloc(sizeof(AudinOpenSLESDevice));
	ZeroMemory(opensles, sizeof(AudinOpenSLESDevice));

	opensles->iface.Open = audin_opensles_open;
	opensles->iface.FormatSupported = audin_opensles_format_supported;
	opensles->iface.SetFormat = audin_opensles_set_format;
	opensles->iface.Close = audin_opensles_close;
	opensles->iface.Free = audin_opensles_free;

	args = pEntryPoints->args;

	audin_opensles_parse_addin_args(opensles, args);

	if (!opensles->device_name)
		opensles->device_name = _strdup("default");

	opensles->frames_per_packet = 128;
	opensles->target_rate = 22050;
	opensles->actual_rate = 22050;
	opensles->format = SND_PCM_FORMAT_S16_LE;
	opensles->target_channels = 2;
	opensles->actual_channels = 2;
	opensles->bytes_per_channel = 2;

	opensles->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin,
			(IAudinDevice*) opensles);

	return 0;
}
