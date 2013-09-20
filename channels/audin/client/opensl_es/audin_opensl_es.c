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

typedef struct _AudinOpenSL_ESDevice
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
	int block_size;

	FREERDP_DSP_CONTEXT* dsp_context;

	HANDLE thread;
	HANDLE stopEvent;

	void* user_data;
} AudinOpenSL_ESDevice;

static BOOL audin_opensl_es_thread_receive(AudinOpenSL_ESDevice* opensl_es,
		void* src, int count)
{
	int frames;
	int cframes;
	int ret = 0;
	int encoded_size;
	BYTE* encoded_data;
	int rbytes_per_frame;
	int tbytes_per_frame;

	rbytes_per_frame = opensl_es->actual_channels * opensl_es->bytes_per_channel;
	tbytes_per_frame = opensl_es->target_channels * opensl_es->bytes_per_channel;

	if ((opensl_es->target_rate == opensl_es->actual_rate) &&
		(opensl_es->target_channels == opensl_es->actual_channels))
	{
		frames = size / rbytes_per_frame;
	}
	else
	{
		opensl_es->dsp_context->resample(opensl_es->dsp_context, src,
				opensl_es->bytes_per_channel,	opensl_es->actual_channels,
				opensl_es->actual_rate, size / rbytes_per_frame,
				opensl_es->target_channels, opensl_es->target_rate);
		frames = opensl_es->dsp_context->resampled_frames;
		
		DEBUG_DVC("resampled %d frames at %d to %d frames at %d",
			size / rbytes_per_frame, opensl_es->actual_rate,
			frames, opensl_es->target_rate);
		
		size = frames * tbytes_per_frame;
		src = opensl_es->dsp_context->resampled_buffer;
	}

	while (frames > 0)
	{
		if (WaitForSingleObject(opensl_es->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		cframes = opensl_es->frames_per_packet - opensl_es->buffer_frames;

		if (cframes > frames)
			cframes = frames;

		CopyMemory(opensl_es->buffer + opensl_es->buffer_frames * tbytes_per_frame,
				src, cframes * tbytes_per_frame);

		opensl_es->buffer_frames += cframes;

		if (opensl_es->buffer_frames >= opensl_es->frames_per_packet)
		{
			if (opensl_es->wformat == WAVE_FORMAT_DVI_ADPCM)
			{
				opensl_es->dsp_context->encode_ima_adpcm(opensl_es->dsp_context,
					opensl_es->buffer, opensl_es->buffer_frames * tbytes_per_frame,
					opensl_es->target_channels, opensl_es->block_size);

				encoded_data = opensl_es->dsp_context->adpcm_buffer;
				encoded_size = opensl_es->dsp_context->adpcm_size;

				DEBUG_DVC("encoded %d to %d",
					opensl_es->buffer_frames * tbytes_per_frame, encoded_size);
			}
			else
			{
				encoded_data = opensl_es->buffer;
				encoded_size = opensl_es->buffer_frames * tbytes_per_frame;
			}

			if (WaitForSingleObject(opensl_es->stopEvent, 0) == WAIT_OBJECT_0)
				break;
			else
				ret = opensl_es->receive(encoded_data, encoded_size,
						opensl_es->user_data);

			opensl_es->buffer_frames = 0;

			if (!ret)
				break;
		}

		src += cframes * tbytes_per_frame;
		frames -= cframes;
	}

	return (ret) ? TRUE : FALSE;
}

static void* audin_opensl_es_thread_func(void* arg)
{
	float* buffer;
	int rbytes_per_frame;
	int tbytes_per_frame;
	snd_pcm_t* capture_handle = NULL;
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) arg;

	DEBUG_DVC("opensl_es=%p", opensl_es);

	buffer = (BYTE*) calloc(sizeof(float), opensl_es->frames_per_packet);
	ZeroMemory(buffer, opensl_es->frames_per_packet);
	freerdp_dsp_context_reset_adpcm(opensl_es->dsp_context);

	while (!(WaitForSingleObject(opensl_es->stopEvent, 0) == WAIT_OBJECT_0))
	{
		int rc = android_AudioIn(opensl_es->stream, buffer,
				opensl_es->frames_per_packet);
		if (rc < 0)
		{
			DEBUG_WARN("snd_pcm_readi (%s)", snd_strerror(error));
			break;
		}

		if (!audin_opensl_es_thread_receive(opensl_es, buffer, rc * sizeof(float)))
			break;
	}

	free(buffer);

	DEBUG_DVC("thread shutdown.");

	ExitThread(0);
	return NULL;
}

static void audin_opensl_es_free(IAudinDevice* device)
{
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;

	DEBUG_DVC("device=%p", device);

	freerdp_dsp_context_free(opensl_es->dsp_context);

	free(opensl_es->device_name);
	free(opensl_es);
}

static BOOL audin_opensl_es_format_supported(IAudinDevice* device, audinFormat* format)
{
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;
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

static void audin_opensl_es_set_format(IAudinDevice* device,
		audinFormat* format, UINT32 FramesPerPacket)
{
	int bs;
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;

	DEBUG_DVC("device=%p, format=%p, FramesPerPacket=%d",
			device, format, FramesPerPacket);

	opensl_es->target_rate = format->nSamplesPerSec;
	opensl_es->actual_rate = format->nSamplesPerSec;
	opensl_es->target_channels = format->nChannels;
	opensl_es->actual_channels = format->nChannels;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			switch (format->wBitsPerSample)
			{
				case 8:
					opensl_es->format = SND_PCM_FORMAT_S8;
					opensl_es->bytes_per_channel = 1;
					break;
				case 16:
					opensl_es->format = SND_PCM_FORMAT_S16_LE;
					opensl_es->bytes_per_channel = 2;
					break;
			}
			break;

		case WAVE_FORMAT_DVI_ADPCM:
			opensl_es->format = SND_PCM_FORMAT_S16_LE;
			opensl_es->bytes_per_channel = 2;
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			opensl_es->frames_per_packet =
				(opensl_es->frames_per_packet * format->nChannels * 2 /
				bs + 1) * bs / (format->nChannels * 2);
			DEBUG_DVC("aligned FramesPerPacket=%d",
				opensl_es->frames_per_packet);
			break;
	}

	opensl_es->wformat = format->wFormatTag;
	opensl_es->block_size = format->nBlockAlign;
}

static int audin_opensl_es_open(IAudinDevice* device, AudinReceive receive,
		void* user_data)
{
	int status = 0;
	int rbytes_per_frame;
	int tbytes_per_frame;
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;

	DEBUG_DVC("device=%p, receive=%d, user_data=%p", device, receive, user_data);

	opensl_es->stream = android_OpenAudioDevice(opensl_es->target_rate,
			opensl_es->target_channels, 0, opensl_es->frames_per_packet);

	opensl_es->receive = receive;
	opensl_es->user_data = user_data;

	rbytes_per_frame = opensl_es->actual_channels * opensl_es->bytes_per_channel;
	tbytes_per_frame = opensl_es->target_channels * opensl_es->bytes_per_channel;
	opensl_es->buffer =
		(BYTE*) malloc(tbytes_per_frame * opensl_es->frames_per_packet);
	ZeroMemory(opensl_es->buffer,
			tbytes_per_frame * opensl_es->frames_per_packet);
	opensl_es->buffer_frames = 0;
	
	opensl_es->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	opensl_es->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) audin_opensl_es_thread_func,
			opensl_es, 0, NULL);
}

static void audin_opensl_es_close(IAudinDevice* device)
{
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;

	DEBUG_DVC("device=%p", device);

	SetEvent(opensl_es->stopEvent);
	WaitForSingleObject(opensl_es->thread, INFINITE);
	CloseHandle(opensl_es->stopEvent);
	CloseHandle(opensl_es->thread);

	android_CloseAudioDevice(opensl_es->stream);

	opensl_es->stopEvent = NULL;
	opensl_es->thread = NULL;
	opensl_es->receive = NULL;
	opensl_es->user_data = NULL;
	opsnsl_es->stream = NULL;
}

static const COMMAND_LINE_ARGUMENT_A audin_opensl_es_args[] =
{
	{ "audio-dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static int audin_opensl_es_parse_addin_args(AudinOpenSL_ESDevice* device,
		ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinOpenSL_ESDevice* opensl_es = (AudinOpenSL_ESDevice*) device;

	DEBUG_DVC("device=%p, args=%p", device, args);

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			audin_opensl_es_args, flags, opensl_es, NULL, NULL);
	if (status < 0)
		return status;

	arg = audin_opensl_es_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "audio-dev")
		{
			opensl_es->device_name = _strdup(arg->Value);
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return status;
}

#ifdef STATIC_CHANNELS
#define freerdp_audin_client_subsystem_entry \
	opensl_es_freerdp_audin_client_subsystem_entry
#endif

int freerdp_audin_client_subsystem_entry(
		PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinOpenSL_ESDevice* opensl_es;

	DEBUG_DVC("pEntryPoints=%p", pEntryPoints);

	opensl_es = (AudinOpenSL_ESDevice*) malloc(sizeof(AudinOpenSL_ESDevice));
	ZeroMemory(opensl_es, sizeof(AudinOpenSL_ESDevice));

	opensl_es->iface.Open = audin_opensl_es_open;
	opensl_es->iface.FormatSupported = audin_opensl_es_format_supported;
	opensl_es->iface.SetFormat = audin_opensl_es_set_format;
	opensl_es->iface.Close = audin_opensl_es_close;
	opensl_es->iface.Free = audin_opensl_es_free;

	args = pEntryPoints->args;

	audin_opensl_es_parse_addin_args(opensl_es, args);

	if (!opensl_es->device_name)
		opensl_es->device_name = _strdup("default");

	opensl_es->frames_per_packet = 128;
	opensl_es->target_rate = 22050;
	opensl_es->actual_rate = 22050;
	opensl_es->format = SND_PCM_FORMAT_S16_LE;
	opensl_es->target_channels = 2;
	opensl_es->actual_channels = 2;
	opensl_es->bytes_per_channel = 2;

	opensl_es->dsp_context = freerdp_dsp_context_new();

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin,
			(IAudinDevice*) opensl_es);

	return 0;
}
