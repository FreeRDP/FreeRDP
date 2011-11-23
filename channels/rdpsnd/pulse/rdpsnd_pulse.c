/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_pulse_plugin rdpsndPulsePlugin;
struct rdpsnd_pulse_plugin
{
	rdpsndDevicePlugin device;

	char* device_name;
	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_sample_spec sample_spec;
	pa_stream *stream;
	int format;
	int block_size;
	int latency;
	ADPCM adpcm;
};

static void rdpsnd_pulse_context_state_callback(pa_context* context, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
	pa_context_state_t state;

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

static boolean rdpsnd_pulse_connect(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;
	pa_context_state_t state;

	if (!pulse->context)
		return false;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		DEBUG_WARN("pa_context_connect failed (%d)", pa_context_errno(pulse->context));
		return false;
	}
	pa_threaded_mainloop_lock(pulse->mainloop);
	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_threaded_mainloop_start failed (%d)", pa_context_errno(pulse->context));
		return false;
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
		return true;
	}
	else
	{
		pa_context_disconnect(pulse->context);
		return false;
	}
}

static void rdpsnd_pulse_stream_success_callback(pa_stream* stream, int success, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;

	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void rdpsnd_pulse_wait_for_operation(rdpsndPulsePlugin* pulse, pa_operation* operation)
{
	if (operation == NULL)
		return;
	while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
	{
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_operation_unref(operation);
}

static void rdpsnd_pulse_stream_state_callback(pa_stream* stream, void* userdata)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;
	pa_stream_state_t state;

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
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)userdata;

	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void rdpsnd_pulse_close(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context || !pulse->stream)
		return;

	pa_threaded_mainloop_lock(pulse->mainloop);
	rdpsnd_pulse_wait_for_operation(pulse,
		pa_stream_drain(pulse->stream, rdpsnd_pulse_stream_success_callback, pulse));
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;
	pa_threaded_mainloop_unlock(pulse->mainloop);
}

static void rdpsnd_pulse_set_format_spec(rdpsndPulsePlugin* pulse, rdpsndFormat* format)
{
	pa_sample_spec sample_spec = { 0 };

	if (!pulse->context)
		return;

	sample_spec.rate = format->nSamplesPerSec;
	sample_spec.channels = format->nChannels;
	switch (format->wFormatTag)
	{
		case 1: /* PCM */
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

		case 6: /* A-LAW */
			sample_spec.format = PA_SAMPLE_ALAW;
			break;

		case 7: /* U-LAW */
			sample_spec.format = PA_SAMPLE_ULAW;
			break;

		case 0x11: /* IMA ADPCM */
			sample_spec.format = PA_SAMPLE_S16LE;
			break;
	}

	pulse->sample_spec = sample_spec;
	pulse->format = format->wFormatTag;
	pulse->block_size = format->nBlockAlign;
}

static void rdpsnd_pulse_open(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;
	pa_stream_state_t state;
	pa_stream_flags_t flags;
	pa_buffer_attr buffer_attr = { 0 };
	char ss[PA_SAMPLE_SPEC_SNPRINT_MAX];

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
	pulse->stream = pa_stream_new(pulse->context, "freerdp",
		&pulse->sample_spec, NULL);
	if (!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_stream_new failed (%d)",
			pa_context_errno(pulse->context));
		return;
	}

	/* install essential callbacks */
	pa_stream_set_state_callback(pulse->stream,
		rdpsnd_pulse_stream_state_callback, pulse);
	pa_stream_set_write_callback(pulse->stream,
		rdpsnd_pulse_stream_request_callback, pulse);

	flags = PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE;
	if (pulse->latency > 0)
	{
		buffer_attr.maxlength = pa_usec_to_bytes(pulse->latency * 2 * 1000, &pulse->sample_spec);
		buffer_attr.tlength = pa_usec_to_bytes(pulse->latency * 1000, &pulse->sample_spec);
		buffer_attr.prebuf = (uint32_t) -1;
		buffer_attr.minreq = (uint32_t) -1;
		buffer_attr.fragsize = (uint32_t) -1;
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
		memset(&pulse->adpcm, 0, sizeof(ADPCM));
		DEBUG_SVC("connected");
	}
	else
	{
		rdpsnd_pulse_close(device);
	}
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
	xfree(pulse->device_name);
	xfree(pulse);
}

static boolean rdpsnd_pulse_format_supported(rdpsndDevicePlugin* device, rdpsndFormat* format)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->context)
		return false;

	switch (format->wFormatTag)
	{
		case 1: /* PCM */
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return true;
			}
			break;

		case 6: /* A-LAW */
		case 7: /* U-LAW */
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 8) &&
				(format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return true;
			}
			break;

		case 0x11: /* IMA ADPCM */
			if ((format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 4) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return true;
			}
			break;
	}
	return false;
}

static void rdpsnd_pulse_set_format(rdpsndDevicePlugin* device, rdpsndFormat* format, int latency)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

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

static void rdpsnd_pulse_set_volume(rdpsndDevicePlugin* device, uint32 value)
{
}

static void rdpsnd_pulse_play(rdpsndDevicePlugin* device, uint8* data, int size)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;
	int len;
	int ret;
	uint8* decoded_data;
	uint8* src;
	int decoded_size;

	if (!pulse->stream)
		return;

	if (pulse->format == 0x11)
	{
		decoded_data = dsp_decode_ima_adpcm(&pulse->adpcm,
			data, size, pulse->sample_spec.channels, pulse->block_size, &decoded_size);
		size = decoded_size;
		src = decoded_data;
	}
	else
	{
		decoded_data = NULL;
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

	if (decoded_data)
		xfree(decoded_data);
}

static void rdpsnd_pulse_start(rdpsndDevicePlugin* device)
{
	rdpsndPulsePlugin* pulse = (rdpsndPulsePlugin*)device;

	if (!pulse->stream)
		return;

	pa_stream_trigger(pulse->stream, NULL, NULL);
}

int FreeRDPRdpsndDeviceEntry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	rdpsndPulsePlugin* pulse;
	RDP_PLUGIN_DATA* data;

	pulse = xnew(rdpsndPulsePlugin);

	pulse->device.Open = rdpsnd_pulse_open;
	pulse->device.FormatSupported = rdpsnd_pulse_format_supported;
	pulse->device.SetFormat = rdpsnd_pulse_set_format;
	pulse->device.SetVolume = rdpsnd_pulse_set_volume;
	pulse->device.Play = rdpsnd_pulse_play;
	pulse->device.Start = rdpsnd_pulse_start;
	pulse->device.Close = rdpsnd_pulse_close;
	pulse->device.Free = rdpsnd_pulse_free;

	data = pEntryPoints->plugin_data;
	if (data && strcmp((char*)data->data[0], "pulse") == 0)
	{
		if(strlen((char*)data->data[1]) > 0) 
			pulse->device_name = xstrdup((char*)data->data[1]);
		else
			pulse->device_name = NULL;
	}

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

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)pulse);

	return 0;
}

