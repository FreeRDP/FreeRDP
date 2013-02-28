/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2011 Vic Lee
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

#include <pulse/pulseaudio.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_pulse_plugin rdpsndPulsePlugin;

struct rdpsnd_pulse_plugin
{
	rdpsndDevicePlugin device;

	char* device_name;
	pa_threaded_mainloop* mainloop;
	pa_context* context;
	pa_sample_spec sample_spec;
	pa_stream* stream;
	int format;
	int block_size;
	int latency;

	FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_pulse_context_state_callback(pa_context* context, void* userdata)
{
	pa_context_state_t state;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) userdata;

	state = pa_context_get_state(context);

	switch (state)
	{
		case PA_CONTEXT_READY:
			DEBUG_SVC("PA_CONTEXT_READY");
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			DEBUG_SVC("PA_CONTEXT_FAILED/PA_CONTEXT_TERMINATED %d", (int)state);
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		default:
			DEBUG_SVC("state %d", (int)state);
			break;
	}
}

static BOOL rdpsnd_pulse_connect(rdpsndDevicePlugin* device)
{
	pa_context_state_t state;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->context)
		return FALSE;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		DEBUG_WARN("pa_context_connect failed (%d)", pa_context_errno(pulse->context));
		return FALSE;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);

	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_threaded_mainloop_start failed (%d)", pa_context_errno(pulse->context));
		return FALSE;
	}

	for (;;)
	{
		state = pa_context_get_state(pulse->context);

		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state))
		{
			DEBUG_WARN("bad context state (%d)", pa_context_errno(pulse->context));
			break;
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);

	if (state == PA_CONTEXT_READY)
	{
		DEBUG_SVC("connected");
		return TRUE;
	}
	else
	{
		pa_context_disconnect(pulse->context);
		return FALSE;
	}
}

static void rdpsnd_pulse_stream_success_callback(pa_stream* stream, int success, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) userdata;

	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void rdpsnd_pulse_wait_for_operation(rdpsndPulsePlugin* pulse, pa_operation* operation)
{
	if (!operation)
		return;

	while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
	{
		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_operation_unref(operation);
}

static void rdpsnd_pulse_stream_state_callback(pa_stream* stream, void* userdata)
{
	pa_stream_state_t state;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) userdata;

	state = pa_stream_get_state(stream);

	switch (state)
	{
		case PA_STREAM_READY:
			DEBUG_SVC("PA_STREAM_READY");
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			DEBUG_SVC("state %d", (int)state);
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		default:
			DEBUG_SVC("state %d", (int)state);
			break;
	}
}

static void rdpsnd_pulse_stream_request_callback(pa_stream* stream, size_t length, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) userdata;

	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void rdpsnd_pulse_close(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->context || !pulse->stream)
		return;

	pa_threaded_mainloop_lock(pulse->mainloop);

	rdpsnd_pulse_wait_for_operation(pulse, pa_stream_drain(pulse->stream, rdpsnd_pulse_stream_success_callback, pulse));
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;

	pa_threaded_mainloop_unlock(pulse->mainloop);
}

static void rdpsnd_pulse_set_format_spec(rdpsndPulsePlugin* pulse, AUDIO_FORMAT* format)
{
	pa_sample_spec sample_spec = { 0 };

	if (!pulse->context)
		return;

	sample_spec.rate = format->nSamplesPerSec;
	sample_spec.channels = format->nChannels;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			switch (format->wBitsPerSample)
			{
				case 8:
					sample_spec.format = PA_SAMPLE_U8;
					break;
				case 16:
					sample_spec.format = PA_SAMPLE_S16LE;
					break;
			}
			break;

		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			sample_spec.format = PA_SAMPLE_S16LE;
			break;

		case WAVE_FORMAT_ALAW:
			sample_spec.format = PA_SAMPLE_ALAW;
			break;

		case WAVE_FORMAT_MULAW:
			sample_spec.format = PA_SAMPLE_ULAW;
			break;

		case WAVE_FORMAT_GSM610:
			break;
	}

	pulse->sample_spec = sample_spec;
	pulse->format = format->wFormatTag;
	pulse->block_size = format->nBlockAlign;
}

static void rdpsnd_pulse_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	pa_stream_state_t state;
	pa_stream_flags_t flags;
	pa_buffer_attr buffer_attr = { 0 };
	char ss[PA_SAMPLE_SPEC_SNPRINT_MAX];
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->context || pulse->stream)
	{
		DEBUG_WARN("pulse stream has been created.");
		return;
	}

	rdpsnd_pulse_set_format_spec(pulse, format);
	pulse->latency = latency;

	if (pa_sample_spec_valid(&pulse->sample_spec) == 0)
	{
		pa_sample_spec_snprint(ss, sizeof(ss), &pulse->sample_spec);
		DEBUG_WARN("Invalid sample spec %s", ss);
		return;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);

	pulse->stream = pa_stream_new(pulse->context, "freerdp", &pulse->sample_spec, NULL);

	if (!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_stream_new failed (%d)", pa_context_errno(pulse->context));
		return;
	}

	/* register essential callbacks */
	pa_stream_set_state_callback(pulse->stream, rdpsnd_pulse_stream_state_callback, pulse);
	pa_stream_set_write_callback(pulse->stream, rdpsnd_pulse_stream_request_callback, pulse);

	flags = PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;

	if (pulse->latency > 0)
	{
		buffer_attr.maxlength = pa_usec_to_bytes(pulse->latency * 2 * 1000, &pulse->sample_spec);
		buffer_attr.tlength = pa_usec_to_bytes(pulse->latency * 1000, &pulse->sample_spec);
		buffer_attr.prebuf = (UINT32) -1;
		buffer_attr.minreq = (UINT32) -1;
		buffer_attr.fragsize = (UINT32) -1;
		flags |= PA_STREAM_ADJUST_LATENCY;
	}

	if (pa_stream_connect_playback(pulse->stream,
		pulse->device_name, pulse->latency > 0 ? &buffer_attr : NULL, flags, NULL, NULL) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_stream_connect_playback failed (%d)",
			pa_context_errno(pulse->context));
		return;
	}

	for (;;)
	{
		state = pa_stream_get_state(pulse->stream);

		if (state == PA_STREAM_READY)
			break;

		if (!PA_STREAM_IS_GOOD(state))
		{
			DEBUG_WARN("bad stream state (%d)",
				pa_context_errno(pulse->context));
			break;
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);

	if (state == PA_STREAM_READY)
	{
		freerdp_dsp_context_reset_adpcm(pulse->dsp_context);
		DEBUG_SVC("connected");
	}
	else
	{
		rdpsnd_pulse_close(device);
	}
}

static void rdpsnd_pulse_free(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse)
		return;

	rdpsnd_pulse_close(device);

	if (pulse->mainloop)
	{
		pa_threaded_mainloop_stop(pulse->mainloop);
	}

	if (pulse->context)
	{
		pa_context_disconnect(pulse->context);
		pa_context_unref(pulse->context);
		pulse->context = NULL;
	}

	if (pulse->mainloop)
	{
		pa_threaded_mainloop_free(pulse->mainloop);
		pulse->mainloop = NULL;
	}

	free(pulse->device_name);
	freerdp_dsp_context_free(pulse->dsp_context);
	free(pulse);
}

static BOOL rdpsnd_pulse_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context)
		return FALSE;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return TRUE;
			}
			break;

		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 8) &&
				(format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return TRUE;
			}
			break;

		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			if ((format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 4) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;
	}
	return FALSE;
}

static void rdpsnd_pulse_set_format(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (pulse->stream)
	{
		pa_threaded_mainloop_lock(pulse->mainloop);
		pa_stream_disconnect(pulse->stream);
		pa_stream_unref(pulse->stream);
		pulse->stream = NULL;
		pa_threaded_mainloop_unlock(pulse->mainloop);
	}

	rdpsnd_pulse_open(device, format, latency);
}

static void rdpsnd_pulse_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	pa_cvolume cv;
	pa_volume_t left;
	pa_volume_t right;
	pa_operation* operation;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->context || !pulse->stream)
		return;

	left = (pa_volume_t) (value & 0xFFFF);
	right = (pa_volume_t) ((value >> 16) & 0xFFFF);

	pa_cvolume_init(&cv);
	cv.channels = 2;
	cv.values[0] = PA_VOLUME_MUTED + (left * (PA_VOLUME_NORM - PA_VOLUME_MUTED)) / 0xFFFF;
	cv.values[1] = PA_VOLUME_MUTED + (right * (PA_VOLUME_NORM - PA_VOLUME_MUTED)) / 0xFFFF;

	pa_threaded_mainloop_lock(pulse->mainloop);

	operation = pa_context_set_sink_input_volume(pulse->context, pa_stream_get_index(pulse->stream), &cv, NULL, NULL);

	if (operation)
		pa_operation_unref(operation);

	pa_threaded_mainloop_unlock(pulse->mainloop);
}

static void rdpsnd_pulse_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	int len;
	int ret;
	BYTE* src;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->stream)
		return;

	if (pulse->format == WAVE_FORMAT_ADPCM)
	{
		pulse->dsp_context->decode_ms_adpcm(pulse->dsp_context,
			data, size, pulse->sample_spec.channels, pulse->block_size);

		size = pulse->dsp_context->adpcm_size;
		src = pulse->dsp_context->adpcm_buffer;
	}
	else if (pulse->format == WAVE_FORMAT_DVI_ADPCM)
	{
		pulse->dsp_context->decode_ima_adpcm(pulse->dsp_context,
			data, size, pulse->sample_spec.channels, pulse->block_size);

		size = pulse->dsp_context->adpcm_size;
		src = pulse->dsp_context->adpcm_buffer;
	}
	else
	{
		src = data;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);

	while (size > 0)
	{
		while ((len = pa_stream_writable_size(pulse->stream)) == 0)
		{
			pa_threaded_mainloop_wait(pulse->mainloop);
		}

		if (len < 0)
			break;

		if (len > size)
			len = size;

		ret = pa_stream_write(pulse->stream, src, len, NULL, 0LL, PA_SEEK_RELATIVE);

		if (ret < 0)
		{
			DEBUG_WARN("pa_stream_write failed (%d)",
				pa_context_errno(pulse->context));
			break;
		}

		src += len;
		size -= len;
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);
}

static void rdpsnd_pulse_start(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	if (!pulse->stream)
		return;

	pa_stream_trigger(pulse->stream, NULL, NULL);
}

COMMAND_LINE_ARGUMENT_A rdpsnd_pulse_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void rdpsnd_pulse_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*) device;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			rdpsnd_pulse_args, flags, pulse, NULL, NULL);

	arg = rdpsnd_pulse_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			pulse->device_name = _strdup(arg->Value);
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	pulse_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndPulsePlugin* pulse;

	pulse = (rdpsndPulsePlugin*) malloc(sizeof(rdpsndPulsePlugin));
	ZeroMemory(pulse, sizeof(rdpsndPulsePlugin));

	pulse->device.Open = rdpsnd_pulse_open;
	pulse->device.FormatSupported = rdpsnd_pulse_format_supported;
	pulse->device.SetFormat = rdpsnd_pulse_set_format;
	pulse->device.SetVolume = rdpsnd_pulse_set_volume;
	pulse->device.Play = rdpsnd_pulse_play;
	pulse->device.Start = rdpsnd_pulse_start;
	pulse->device.Close = rdpsnd_pulse_close;
	pulse->device.Free = rdpsnd_pulse_free;

	args = pEntryPoints->args;
	rdpsnd_pulse_parse_addin_args((rdpsndDevicePlugin*) pulse, args);

	pulse->dsp_context = freerdp_dsp_context_new();

	pulse->mainloop = pa_threaded_mainloop_new();

	if (!pulse->mainloop)
	{
		DEBUG_WARN("pa_threaded_mainloop_new failed");
		rdpsnd_pulse_free((rdpsndDevicePlugin*)pulse);
		return 1;
	}

	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");

	if (!pulse->context)
	{
		DEBUG_WARN("pa_context_new failed");
		rdpsnd_pulse_free((rdpsndDevicePlugin*)pulse);
		return 1;
	}

	pa_context_set_state_callback(pulse->context, rdpsnd_pulse_context_state_callback, pulse);

	if (!rdpsnd_pulse_connect((rdpsndDevicePlugin*)pulse))
	{
		DEBUG_WARN("rdpsnd_pulse_connect failed");
		rdpsnd_pulse_free((rdpsndDevicePlugin*)pulse);
		return 1;
	}

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) pulse);

	return 0;
}
