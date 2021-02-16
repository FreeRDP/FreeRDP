/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <pulse/pulseaudio.h>

#include <freerdp/types.h>
#include <freerdp/codec/dsp.h>

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
	UINT32 latency;
	UINT32 volume;
};

static BOOL rdpsnd_pulse_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format);

static void rdpsnd_pulse_get_sink_info(pa_context* c, const pa_sink_info* i, int eol,
                                       void* userdata)
{
	uint8_t x;
	UINT16 dwVolumeLeft = ((50 * 0xFFFF) / 100); /* 50% */
	;
	UINT16 dwVolumeRight = ((50 * 0xFFFF) / 100); /* 50% */
	;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;

	if (!pulse || !c || !i)
		return;

	for (x = 0; x < i->volume.channels; x++)
	{
		pa_volume_t volume = i->volume.values[x];

		if (volume >= PA_VOLUME_NORM)
			volume = PA_VOLUME_NORM - 1;

		switch (x)
		{
			case 0:
				dwVolumeLeft = (UINT16)volume;
				break;

			case 1:
				dwVolumeRight = (UINT16)volume;
				break;

			default:
				break;
		}
	}

	pulse->volume = ((UINT32)dwVolumeLeft << 16U) | dwVolumeRight;
}

static void rdpsnd_pulse_context_state_callback(pa_context* context, void* userdata)
{
	pa_context_state_t state;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
	state = pa_context_get_state(context);

	switch (state)
	{
		case PA_CONTEXT_READY:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		default:
			break;
	}
}

static BOOL rdpsnd_pulse_connect(rdpsndDevicePlugin* device)
{
	pa_operation* o;
	pa_context_state_t state;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context)
		return FALSE;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		return FALSE;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);

	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		return FALSE;
	}

	for (;;)
	{
		state = pa_context_get_state(pulse->context);

		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state))
		{
			break;
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	o = pa_context_get_sink_info_by_index(pulse->context, 0, rdpsnd_pulse_get_sink_info, pulse);

	if (o)
		pa_operation_unref(o);

	pa_threaded_mainloop_unlock(pulse->mainloop);

	if (state == PA_CONTEXT_READY)
	{
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
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
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
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
	state = pa_stream_get_state(stream);

	switch (state)
	{
		case PA_STREAM_READY:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		default:
			break;
	}
}

static void rdpsnd_pulse_stream_request_callback(pa_stream* stream, size_t length, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void rdpsnd_pulse_close(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context || !pulse->stream)
		return;

	pa_threaded_mainloop_lock(pulse->mainloop);
	rdpsnd_pulse_wait_for_operation(
	    pulse, pa_stream_drain(pulse->stream, rdpsnd_pulse_stream_success_callback, pulse));
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;
	pa_threaded_mainloop_unlock(pulse->mainloop);
}

static BOOL rdpsnd_pulse_set_format_spec(rdpsndPulsePlugin* pulse, const AUDIO_FORMAT* format)
{
	pa_sample_spec sample_spec = { 0 };

	if (!pulse->context)
		return FALSE;

	if (!rdpsnd_pulse_format_supported(&pulse->device, format))
		return FALSE;

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

				default:
					return FALSE;
			}

			break;

		case WAVE_FORMAT_ALAW:
			sample_spec.format = PA_SAMPLE_ALAW;
			break;

		case WAVE_FORMAT_MULAW:
			sample_spec.format = PA_SAMPLE_ULAW;
			break;

		default:
			return FALSE;
	}

	pulse->sample_spec = sample_spec;
	return TRUE;
}

static BOOL rdpsnd_pulse_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                              UINT32 latency)
{
	pa_stream_state_t state;
	pa_stream_flags_t flags;
	pa_buffer_attr buffer_attr = { 0 };
	char ss[PA_SAMPLE_SPEC_SNPRINT_MAX];
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context || pulse->stream)
		return TRUE;

	if (!rdpsnd_pulse_set_format_spec(pulse, format))
		return FALSE;

	pulse->latency = latency;

	if (pa_sample_spec_valid(&pulse->sample_spec) == 0)
	{
		pa_sample_spec_snprint(ss, sizeof(ss), &pulse->sample_spec);
		return TRUE;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);
	pulse->stream = pa_stream_new(pulse->context, "freerdp", &pulse->sample_spec, NULL);

	if (!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		return FALSE;
	}

	/* register essential callbacks */
	pa_stream_set_state_callback(pulse->stream, rdpsnd_pulse_stream_state_callback, pulse);
	pa_stream_set_write_callback(pulse->stream, rdpsnd_pulse_stream_request_callback, pulse);
	flags = PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;

	if (pulse->latency > 0)
	{
		buffer_attr.maxlength = pa_usec_to_bytes(pulse->latency * 2 * 1000, &pulse->sample_spec);
		buffer_attr.tlength = pa_usec_to_bytes(pulse->latency * 1000, &pulse->sample_spec);
		buffer_attr.prebuf = (UINT32)-1;
		buffer_attr.minreq = (UINT32)-1;
		buffer_attr.fragsize = (UINT32)-1;
		flags |= PA_STREAM_ADJUST_LATENCY;
	}

	if (pa_stream_connect_playback(pulse->stream, pulse->device_name,
	                               pulse->latency > 0 ? &buffer_attr : NULL, flags, NULL, NULL) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		return TRUE;
	}

	for (;;)
	{
		state = pa_stream_get_state(pulse->stream);

		if (state == PA_STREAM_READY)
			break;

		if (!PA_STREAM_IS_GOOD(state))
		{
			break;
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);

	if (state == PA_STREAM_READY)
		return TRUE;

	rdpsnd_pulse_close(device);
	return FALSE;
}

static void rdpsnd_pulse_free(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

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
	free(pulse);
}

static BOOL rdpsnd_pulse_default_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* desired,
                                        AUDIO_FORMAT* defaultFormat)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;
	if (!pulse || !defaultFormat)
		return FALSE;

	*defaultFormat = *desired;
	defaultFormat->data = NULL;
	defaultFormat->cbSize = 0;
	defaultFormat->wFormatTag = WAVE_FORMAT_PCM;
	if ((defaultFormat->nChannels < 1) || (defaultFormat->nChannels > PA_CHANNELS_MAX))
		defaultFormat->nChannels = 2;
	if ((defaultFormat->nSamplesPerSec < 1) || (defaultFormat->nSamplesPerSec > PA_RATE_MAX))
		defaultFormat->nSamplesPerSec = 44100;
	if ((defaultFormat->wBitsPerSample != 8) && (defaultFormat->wBitsPerSample != 16))
		defaultFormat->wBitsPerSample = 16;

	defaultFormat->nBlockAlign = defaultFormat->nChannels * defaultFormat->wBitsPerSample / 8;
	defaultFormat->nAvgBytesPerSec = defaultFormat->nBlockAlign * defaultFormat->nSamplesPerSec;
	return TRUE;
}

BOOL rdpsnd_pulse_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 && (format->nSamplesPerSec <= PA_RATE_MAX) &&
			    (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			    (format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return TRUE;
			}

			break;

		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
			if (format->cbSize == 0 && (format->nSamplesPerSec <= PA_RATE_MAX) &&
			    (format->wBitsPerSample == 8) &&
			    (format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return TRUE;
			}

			break;
	}

	return FALSE;
}

static UINT32 rdpsnd_pulse_get_volume(rdpsndDevicePlugin* device)
{
	pa_operation* o;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse)
		return 0;

	if (!pulse->context || !pulse->mainloop)
		return 0;

	pa_threaded_mainloop_lock(pulse->mainloop);
	o = pa_context_get_sink_info_by_index(pulse->context, 0, rdpsnd_pulse_get_sink_info, pulse);
	pa_operation_unref(o);
	pa_threaded_mainloop_unlock(pulse->mainloop);
	return pulse->volume;
}

static BOOL rdpsnd_pulse_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	pa_cvolume cv;
	pa_volume_t left;
	pa_volume_t right;
	pa_operation* operation;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context || !pulse->stream)
		return FALSE;

	left = (pa_volume_t)(value & 0xFFFF);
	right = (pa_volume_t)((value >> 16) & 0xFFFF);
	pa_cvolume_init(&cv);
	cv.channels = 2;
	cv.values[0] = PA_VOLUME_MUTED + (left * (PA_VOLUME_NORM - PA_VOLUME_MUTED)) / 0xFFFF;
	cv.values[1] = PA_VOLUME_MUTED + (right * (PA_VOLUME_NORM - PA_VOLUME_MUTED)) / 0xFFFF;
	pa_threaded_mainloop_lock(pulse->mainloop);
	operation = pa_context_set_sink_input_volume(pulse->context, pa_stream_get_index(pulse->stream),
	                                             &cv, NULL, NULL);

	if (operation)
		pa_operation_unref(operation);

	pa_threaded_mainloop_unlock(pulse->mainloop);
	return TRUE;
}

static UINT rdpsnd_pulse_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	size_t length;
	int status;
	pa_usec_t latency;
	int negative;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->stream || !data)
		return 0;

	pa_threaded_mainloop_lock(pulse->mainloop);

	while (size > 0)
	{
		while ((length = pa_stream_writable_size(pulse->stream)) == 0)
			pa_threaded_mainloop_wait(pulse->mainloop);

		if (length == (size_t)-1)
			break;

		if (length > size)
			length = size;

		status = pa_stream_write(pulse->stream, data, length, NULL, 0LL, PA_SEEK_RELATIVE);

		if (status < 0)
		{
			break;
		}

		data += length;
		size -= length;
	}

	if (pa_stream_get_latency(pulse->stream, &latency, &negative) != 0)
		latency = 0;

	pa_threaded_mainloop_unlock(pulse->mainloop);
	return latency / 1000;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_pulse_parse_addin_args(rdpsndDevicePlugin* device, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;
	COMMAND_LINE_ARGUMENT_A rdpsnd_pulse_args[] = { { "dev", COMMAND_LINE_VALUE_REQUIRED,
		                                              "<device>", NULL, NULL, -1, NULL, "device" },
		                                            { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, rdpsnd_pulse_args, flags, pulse,
	                                    NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_DATA;

	arg = rdpsnd_pulse_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			pulse->device_name = _strdup(arg->Value);

			if (!pulse->device_name)
				return ERROR_OUTOFMEMORY;
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry pulse_freerdp_rdpsnd_client_subsystem_entry
#else
#define freerdp_rdpsnd_client_subsystem_entry FREERDP_API freerdp_rdpsnd_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	rdpsndPulsePlugin* pulse;
	UINT ret;
	pulse = (rdpsndPulsePlugin*)calloc(1, sizeof(rdpsndPulsePlugin));

	if (!pulse)
		return CHANNEL_RC_NO_MEMORY;

	pulse->device.Open = rdpsnd_pulse_open;
	pulse->device.FormatSupported = rdpsnd_pulse_format_supported;
	pulse->device.GetVolume = rdpsnd_pulse_get_volume;
	pulse->device.SetVolume = rdpsnd_pulse_set_volume;
	pulse->device.Play = rdpsnd_pulse_play;
	pulse->device.Close = rdpsnd_pulse_close;
	pulse->device.Free = rdpsnd_pulse_free;
	pulse->device.DefaultFormat = rdpsnd_pulse_default_format;
	args = pEntryPoints->args;

	if (args->argc > 1)
	{
		ret = rdpsnd_pulse_parse_addin_args(&pulse->device, args);

		if (ret != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "error parsing arguments");
			goto error;
		}
	}

	ret = CHANNEL_RC_NO_MEMORY;
	pulse->mainloop = pa_threaded_mainloop_new();

	if (!pulse->mainloop)
		goto error;

	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");

	if (!pulse->context)
		goto error;

	pa_context_set_state_callback(pulse->context, rdpsnd_pulse_context_state_callback, pulse);
	ret = ERROR_INVALID_OPERATION;

	if (!rdpsnd_pulse_connect((rdpsndDevicePlugin*)pulse))
		goto error;

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)pulse);
	return CHANNEL_RC_OK;
error:
	rdpsnd_pulse_free((rdpsndDevicePlugin*)pulse);
	return ret;
}
