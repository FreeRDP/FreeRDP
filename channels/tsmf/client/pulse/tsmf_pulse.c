/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel - PulseAudio Device
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

#include <pulse/pulseaudio.h>

#include "tsmf_audio.h"

typedef struct _TSMFPulseAudioDevice
{
	ITSMFAudioDevice iface;

	char device[32];
	pa_threaded_mainloop *mainloop;
	pa_context *context;
	pa_sample_spec sample_spec;
	pa_stream *stream;
} TSMFPulseAudioDevice;

static void tsmf_pulse_context_state_callback(pa_context *context, void *userdata)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) userdata;
	pa_context_state_t state;
	state = pa_context_get_state(context);
	switch(state)
	{
		case PA_CONTEXT_READY:
			DEBUG_TSMF("PA_CONTEXT_READY");
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;
		case PA_CONTEXT_FAILED:
		case PA_CONTEXT_TERMINATED:
			DEBUG_TSMF("state %d", (int)state);
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;
		default:
			DEBUG_TSMF("state %d", (int)state);
			break;
	}
}

static BOOL tsmf_pulse_connect(TSMFPulseAudioDevice *pulse)
{
	pa_context_state_t state;
	if(!pulse->context)
		return FALSE;
	if(pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		WLog_ERR(TAG, "pa_context_connect failed (%d)",
				   pa_context_errno(pulse->context));
		return FALSE;
	}
	pa_threaded_mainloop_lock(pulse->mainloop);
	if(pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_ERR(TAG, "pa_threaded_mainloop_start failed (%d)",
				   pa_context_errno(pulse->context));
		return FALSE;
	}
	for(;;)
	{
		state = pa_context_get_state(pulse->context);
		if(state == PA_CONTEXT_READY)
			break;
		if(!PA_CONTEXT_IS_GOOD(state))
		{
			DEBUG_TSMF("bad context state (%d)",
					   pa_context_errno(pulse->context));
			break;
		}
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_threaded_mainloop_unlock(pulse->mainloop);
	if(state == PA_CONTEXT_READY)
	{
		DEBUG_TSMF("connected");
		return TRUE;
	}
	else
	{
		pa_context_disconnect(pulse->context);
		return FALSE;
	}
}

static BOOL tsmf_pulse_open(ITSMFAudioDevice *audio, const char *device)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	if(device)
	{
		strcpy(pulse->device, device);
	}
	pulse->mainloop = pa_threaded_mainloop_new();
	if(!pulse->mainloop)
	{
		WLog_ERR(TAG, "pa_threaded_mainloop_new failed");
		return FALSE;
	}
	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");
	if(!pulse->context)
	{
		WLog_ERR(TAG, "pa_context_new failed");
		return FALSE;
	}
	pa_context_set_state_callback(pulse->context, tsmf_pulse_context_state_callback, pulse);
	if(tsmf_pulse_connect(pulse))
	{
		WLog_ERR(TAG, "tsmf_pulse_connect failed");
		return FALSE;
	}
	DEBUG_TSMF("open device %s", pulse->device);
	return TRUE;
}

static void tsmf_pulse_stream_success_callback(pa_stream *stream, int success, void *userdata)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) userdata;
	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static void tsmf_pulse_wait_for_operation(TSMFPulseAudioDevice *pulse, pa_operation *operation)
{
	if(operation == NULL)
		return;
	while(pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
	{
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_operation_unref(operation);
}

static void tsmf_pulse_stream_state_callback(pa_stream *stream, void *userdata)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) userdata;
	pa_stream_state_t state;
	state = pa_stream_get_state(stream);
	switch(state)
	{
		case PA_STREAM_READY:
			DEBUG_TSMF("PA_STREAM_READY");
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;
		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			DEBUG_TSMF("state %d", (int)state);
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;
		default:
			DEBUG_TSMF("state %d", (int)state);
			break;
	}
}

static void tsmf_pulse_stream_request_callback(pa_stream *stream, size_t length, void *userdata)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) userdata;
	DEBUG_TSMF("%d", (int) length);
	pa_threaded_mainloop_signal(pulse->mainloop, 0);
}

static BOOL tsmf_pulse_close_stream(TSMFPulseAudioDevice *pulse)
{
	if(!pulse->context || !pulse->stream)
		return FALSE;
	DEBUG_TSMF("");
	pa_threaded_mainloop_lock(pulse->mainloop);
	pa_stream_set_write_callback(pulse->stream, NULL, NULL);
	tsmf_pulse_wait_for_operation(pulse,
								  pa_stream_drain(pulse->stream, tsmf_pulse_stream_success_callback, pulse));
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;
	pa_threaded_mainloop_unlock(pulse->mainloop);
	return TRUE;
}

static BOOL tsmf_pulse_open_stream(TSMFPulseAudioDevice *pulse)
{
	pa_stream_state_t state;
	pa_buffer_attr buffer_attr = { 0 };
	if(!pulse->context)
		return FALSE;
	DEBUG_TSMF("");
	pa_threaded_mainloop_lock(pulse->mainloop);
	pulse->stream = pa_stream_new(pulse->context, "freerdp",
								  &pulse->sample_spec, NULL);
	if(!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_ERR(TAG, "pa_stream_new failed (%d)",
				   pa_context_errno(pulse->context));
		return FALSE;
	}
	pa_stream_set_state_callback(pulse->stream,
								 tsmf_pulse_stream_state_callback, pulse);
	pa_stream_set_write_callback(pulse->stream,
								 tsmf_pulse_stream_request_callback, pulse);
	buffer_attr.maxlength = pa_usec_to_bytes(500000, &pulse->sample_spec);
	buffer_attr.tlength = pa_usec_to_bytes(250000, &pulse->sample_spec);
	buffer_attr.prebuf = (UINT32) -1;
	buffer_attr.minreq = (UINT32) -1;
	buffer_attr.fragsize = (UINT32) -1;
	if(pa_stream_connect_playback(pulse->stream,
								  pulse->device[0] ? pulse->device : NULL, &buffer_attr,
								  PA_STREAM_ADJUST_LATENCY | PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE,
								  NULL, NULL) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_ERR(TAG, "pa_stream_connect_playback failed (%d)",
				   pa_context_errno(pulse->context));
		return FALSE;
	}
	for(;;)
	{
		state = pa_stream_get_state(pulse->stream);
		if(state == PA_STREAM_READY)
			break;
		if(!PA_STREAM_IS_GOOD(state))
		{
			WLog_ERR(TAG, "bad stream state (%d)",
					   pa_context_errno(pulse->context));
			break;
		}
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_threaded_mainloop_unlock(pulse->mainloop);
	if(state == PA_STREAM_READY)
	{
		DEBUG_TSMF("connected");
		return TRUE;
	}
	else
	{
		tsmf_pulse_close_stream(pulse);
		return FALSE;
	}
}

static BOOL tsmf_pulse_set_format(ITSMFAudioDevice *audio,
								  UINT32 sample_rate, UINT32 channels, UINT32 bits_per_sample)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	DEBUG_TSMF("sample_rate %d channels %d bits_per_sample %d",
			   sample_rate, channels, bits_per_sample);
	pulse->sample_spec.rate = sample_rate;
	pulse->sample_spec.channels = channels;
	pulse->sample_spec.format = PA_SAMPLE_S16LE;
	return tsmf_pulse_open_stream(pulse);
}

static BOOL tsmf_pulse_play(ITSMFAudioDevice *audio, BYTE *data, UINT32 data_size)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	BYTE *src;
	int len;
	int ret;
	DEBUG_TSMF("data_size %d", data_size);
	if(pulse->stream)
	{
		pa_threaded_mainloop_lock(pulse->mainloop);
		src = data;
		while(data_size > 0)
		{
			while((len = pa_stream_writable_size(pulse->stream)) == 0)
			{
				DEBUG_TSMF("waiting");
				pa_threaded_mainloop_wait(pulse->mainloop);
			}
			if(len < 0)
				break;
			if(len > data_size)
				len = data_size;
			ret = pa_stream_write(pulse->stream, src, len, NULL, 0LL, PA_SEEK_RELATIVE);
			if(ret < 0)
			{
				DEBUG_TSMF("pa_stream_write failed (%d)",
						   pa_context_errno(pulse->context));
				break;
			}
			src += len;
			data_size -= len;
		}
		pa_threaded_mainloop_unlock(pulse->mainloop);
	}
	free(data);
	return TRUE;
}

static UINT64 tsmf_pulse_get_latency(ITSMFAudioDevice *audio)
{
	pa_usec_t usec;
	UINT64 latency = 0;
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	if(pulse->stream && pa_stream_get_latency(pulse->stream, &usec, NULL) == 0)
	{
		latency = ((UINT64)usec) * 10LL;
	}
	return latency;
}

static BOOL tsmf_pulse_flush(ITSMFAudioDevice *audio)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	pa_threaded_mainloop_lock(pulse->mainloop);
	tsmf_pulse_wait_for_operation(pulse,
								  pa_stream_flush(pulse->stream, tsmf_pulse_stream_success_callback, pulse));
	pa_threaded_mainloop_unlock(pulse->mainloop);
	return TRUE;
}

static void tsmf_pulse_free(ITSMFAudioDevice *audio)
{
	TSMFPulseAudioDevice *pulse = (TSMFPulseAudioDevice *) audio;
	DEBUG_TSMF("");
	tsmf_pulse_close_stream(pulse);
	if(pulse->mainloop)
	{
		pa_threaded_mainloop_stop(pulse->mainloop);
	}
	if(pulse->context)
	{
		pa_context_disconnect(pulse->context);
		pa_context_unref(pulse->context);
		pulse->context = NULL;
	}
	if(pulse->mainloop)
	{
		pa_threaded_mainloop_free(pulse->mainloop);
		pulse->mainloop = NULL;
	}
	free(pulse);
}

#ifdef STATIC_CHANNELS
ITSMFAudioDevice *pulse_freerdp_tsmf_client_audio_subsystem_entry(void)
#else
FREERDP_API ITSMFAudioDevice *freerdp_tsmf_client_audio_subsystem_entry(void)
#endif
{
	TSMFPulseAudioDevice *pulse;

	pulse = (TSMFPulseAudioDevice *) calloc(1, sizeof(TSMFPulseAudioDevice));
	if (!pulse)
		return NULL;
	pulse->iface.Open = tsmf_pulse_open;
	pulse->iface.SetFormat = tsmf_pulse_set_format;
	pulse->iface.Play = tsmf_pulse_play;
	pulse->iface.GetLatency = tsmf_pulse_get_latency;
	pulse->iface.Flush = tsmf_pulse_flush;
	pulse->iface.Free = tsmf_pulse_free;
	return (ITSMFAudioDevice *) pulse;
}

