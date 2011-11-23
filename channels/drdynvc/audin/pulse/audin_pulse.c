/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Input Redirection Virtual Channel - PulseAudio implementation
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
#include <pulse/pulseaudio.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/dsp.h>

#include "audin_main.h"

typedef struct _AudinPulseDevice
{
	IAudinDevice iface;

	char device_name[32];
	uint32 frames_per_packet;
	pa_threaded_mainloop* mainloop;
	pa_context* context;
	pa_sample_spec sample_spec;
	pa_stream* stream;
	int format;
	int block_size;
	ADPCM adpcm;

	int bytes_per_frame;
	uint8* buffer;
	int buffer_frames;

	AudinReceive receive;
	void* user_data;
} AudinPulseDevice;

static void audin_pulse_context_state_callback(pa_context* context, void* userdata)
{
	pa_context_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*) userdata;

	state = pa_context_get_state(context);
	switch (state)
	{
		case PA_CONTEXT_READY:
			DEBUG_DVC("PA_CONTEXT_READY");
			pa_threaded_mainloop_signal (pulse->mainloop, 0);
			break;

		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			DEBUG_DVC("state %d", (int)state);
			pa_threaded_mainloop_signal (pulse->mainloop, 0);
			break;

		default:
			DEBUG_DVC("state %d", (int)state);
			break;
	}
}

static boolean audin_pulse_connect(IAudinDevice* device)
{
	pa_context_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return false;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		DEBUG_WARN("pa_context_connect failed (%d)",
			pa_context_errno(pulse->context));
		return false;
	}
	pa_threaded_mainloop_lock(pulse->mainloop);
	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_WARN("pa_threaded_mainloop_start failed (%d)",
			pa_context_errno(pulse->context));
		return false;
	}
	for (;;)
	{
		state = pa_context_get_state(pulse->context);
		if (state == PA_CONTEXT_READY)
			break;
		if (!PA_CONTEXT_IS_GOOD(state))
		{
			DEBUG_WARN("bad context state (%d)",
				pa_context_errno(pulse->context));
			break;
		}
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_threaded_mainloop_unlock(pulse->mainloop);
	if (state == PA_CONTEXT_READY)
	{
		DEBUG_DVC("connected");
		return true;
	}
	else
	{
		pa_context_disconnect(pulse->context);
		return false;
	}
}

static void audin_pulse_free(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	DEBUG_DVC("");

	if (!pulse)
		return;
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
	xfree(pulse);
}

static boolean audin_pulse_format_supported(IAudinDevice* device, audinFormat* format)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return 0;

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

static void audin_pulse_set_format(IAudinDevice* device, audinFormat* format, uint32 FramesPerPacket)
{
	int bs;
	pa_sample_spec sample_spec = { 0 };
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return;

	if (FramesPerPacket > 0)
	{
		pulse->frames_per_packet = FramesPerPacket;
	}

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
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
			pulse->frames_per_packet = (pulse->frames_per_packet * format->nChannels * 2 /
				bs + 1) * bs / (format->nChannels * 2);
			DEBUG_DVC("aligned FramesPerPacket=%d",
				pulse->frames_per_packet);
			break;
	}

	pulse->sample_spec = sample_spec;
	pulse->format = format->wFormatTag;
	pulse->block_size = format->nBlockAlign;
}

static void audin_pulse_stream_state_callback(pa_stream* stream, void* userdata)
{
	pa_stream_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*) userdata;

	state = pa_stream_get_state(stream);
	switch (state)
	{
		case PA_STREAM_READY:
			DEBUG_DVC("PA_STREAM_READY");
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			DEBUG_DVC("state %d", (int)state);
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		default:
			DEBUG_DVC("state %d", (int)state);
			break;
	}
}

static void audin_pulse_stream_request_callback(pa_stream* stream, size_t length, void* userdata)
{
	int frames;
	int cframes;
	boolean ret;
	const void* data;
	const uint8* src;
	int encoded_size;
	uint8* encoded_data;
	AudinPulseDevice* pulse = (AudinPulseDevice*) userdata;

	pa_stream_peek(stream, &data, &length);
	frames = length / pulse->bytes_per_frame;

	DEBUG_DVC("length %d frames %d", (int) length, frames);

	src = (const uint8*) data;
	while (frames > 0)
	{
		cframes = pulse->frames_per_packet - pulse->buffer_frames;
		if (cframes > frames)
			cframes = frames;
		memcpy(pulse->buffer + pulse->buffer_frames * pulse->bytes_per_frame,
			src, cframes * pulse->bytes_per_frame);
		pulse->buffer_frames += cframes;
		if (pulse->buffer_frames >= pulse->frames_per_packet)
		{
			if (pulse->format == 0x11)
			{
				encoded_data = dsp_encode_ima_adpcm(&pulse->adpcm,
					pulse->buffer, pulse->buffer_frames * pulse->bytes_per_frame,
					pulse->sample_spec.channels, pulse->block_size, &encoded_size);
				DEBUG_DVC("encoded %d to %d",
					pulse->buffer_frames * pulse->bytes_per_frame, encoded_size);
			}
			else
			{
				encoded_data = pulse->buffer;
				encoded_size = pulse->buffer_frames * pulse->bytes_per_frame;
			}

			ret = pulse->receive(encoded_data, encoded_size, pulse->user_data);
			pulse->buffer_frames = 0;
			if (encoded_data != pulse->buffer)
				xfree(encoded_data);
			if (!ret)
				break;
		}
		src += cframes * pulse->bytes_per_frame;
		frames -= cframes;
	}

	pa_stream_drop(stream);
}


static void audin_pulse_close(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context || !pulse->stream)
		return;

	DEBUG_DVC("");

	pa_threaded_mainloop_lock(pulse->mainloop);
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;
	pa_threaded_mainloop_unlock(pulse->mainloop);

	pulse->receive = NULL;
	pulse->user_data = NULL;
	if (pulse->buffer)
	{
		xfree(pulse->buffer);
		pulse->buffer = NULL;
		pulse->buffer_frames = 0;
	}
}

static void audin_pulse_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	pa_stream_state_t state;
	pa_buffer_attr buffer_attr = { 0 };
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return;
	if (!pulse->sample_spec.rate || pulse->stream)
		return;

	DEBUG_DVC("");

	pulse->receive = receive;
	pulse->user_data = user_data;

	pa_threaded_mainloop_lock(pulse->mainloop);
	pulse->stream = pa_stream_new(pulse->context, "freerdp_audin",
		&pulse->sample_spec, NULL);
	if (!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		DEBUG_DVC("pa_stream_new failed (%d)",
			pa_context_errno(pulse->context));
		return;
	}
	pulse->bytes_per_frame = pa_frame_size(&pulse->sample_spec);
	pa_stream_set_state_callback(pulse->stream,
		audin_pulse_stream_state_callback, pulse);
	pa_stream_set_read_callback(pulse->stream,
		audin_pulse_stream_request_callback, pulse);
	buffer_attr.maxlength = (uint32_t) -1;
	buffer_attr.tlength = (uint32_t) -1;
	buffer_attr.prebuf = (uint32_t) -1;
	buffer_attr.minreq = (uint32_t) -1;
	/* 500ms latency */
	buffer_attr.fragsize = pa_usec_to_bytes(500000, &pulse->sample_spec);
	if (pa_stream_connect_record(pulse->stream,
		pulse->device_name[0] ? pulse->device_name : NULL,
		&buffer_attr, PA_STREAM_ADJUST_LATENCY) < 0)
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
		pulse->buffer = xzalloc(pulse->bytes_per_frame * pulse->frames_per_packet);
		pulse->buffer_frames = 0;
		DEBUG_DVC("connected");
	}
	else
	{
		audin_pulse_close(device);
	}
}

int FreeRDPAudinDeviceEntry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	AudinPulseDevice* pulse;
	RDP_PLUGIN_DATA * data;

	pulse = xnew(AudinPulseDevice);

	pulse->iface.Open = audin_pulse_open;
	pulse->iface.FormatSupported = audin_pulse_format_supported;
	pulse->iface.SetFormat = audin_pulse_set_format;
	pulse->iface.Close = audin_pulse_close;
	pulse->iface.Free = audin_pulse_free;

	data = pEntryPoints->plugin_data;
	if (data && data->data[0] && strcmp(data->data[0], "audin") == 0 &&
		data->data[1] && strcmp(data->data[1], "pulse") == 0)
	{
		strncpy(pulse->device_name, (char*)data->data[2], sizeof(pulse->device_name));
	}

	pulse->mainloop = pa_threaded_mainloop_new();
	if (!pulse->mainloop)
	{
		DEBUG_WARN("pa_threaded_mainloop_new failed");
		audin_pulse_free((IAudinDevice*) pulse);
		return 1;
	}
	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");
	if (!pulse->context)
	{
		DEBUG_WARN("pa_context_new failed");
		audin_pulse_free((IAudinDevice*) pulse);
		return 1;
	}
	pa_context_set_state_callback(pulse->context, audin_pulse_context_state_callback, pulse);
	if (!audin_pulse_connect((IAudinDevice*) pulse))
	{
		audin_pulse_free((IAudinDevice*) pulse);
		return 1;
	}

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) pulse);

	return 0;
}

