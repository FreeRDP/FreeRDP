/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - PulseAudio implementation
 *
 * Copyright 2010-2011 Vic Lee
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/wlog.h>

#include <pulse/pulseaudio.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/codec/audio.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"

typedef struct
{
	IAudinDevice iface;

	char* device_name;
	UINT32 frames_per_packet;
	pa_threaded_mainloop* mainloop;
	pa_context* context;
	pa_sample_spec sample_spec;
	pa_stream* stream;
	AUDIO_FORMAT format;

	size_t bytes_per_frame;
	size_t buffer_frames;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
	wLog* log;
} AudinPulseDevice;

static const char* pulse_context_state_string(pa_context_state_t state)
{
	switch (state)
	{
		case PA_CONTEXT_UNCONNECTED:
			return "PA_CONTEXT_UNCONNECTED";
		case PA_CONTEXT_CONNECTING:
			return "PA_CONTEXT_CONNECTING";
		case PA_CONTEXT_AUTHORIZING:
			return "PA_CONTEXT_AUTHORIZING";
		case PA_CONTEXT_SETTING_NAME:
			return "PA_CONTEXT_SETTING_NAME";
		case PA_CONTEXT_READY:
			return "PA_CONTEXT_READY";
		case PA_CONTEXT_FAILED:
			return "PA_CONTEXT_FAILED";
		case PA_CONTEXT_TERMINATED:
			return "PA_CONTEXT_TERMINATED";
		default:
			return "UNKNOWN";
	}
}

static const char* pulse_stream_state_string(pa_stream_state_t state)
{
	switch (state)
	{
		case PA_STREAM_UNCONNECTED:
			return "PA_STREAM_UNCONNECTED";
		case PA_STREAM_CREATING:
			return "PA_STREAM_CREATING";
		case PA_STREAM_READY:
			return "PA_STREAM_READY";
		case PA_STREAM_FAILED:
			return "PA_STREAM_FAILED";
		case PA_STREAM_TERMINATED:
			return "PA_STREAM_TERMINATED";
		default:
			return "UNKNOWN";
	}
}

static void audin_pulse_context_state_callback(pa_context* context, void* userdata)
{
	pa_context_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*)userdata;
	state = pa_context_get_state(context);

	WLog_Print(pulse->log, WLOG_DEBUG, "context state %s", pulse_context_state_string(state));
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_connect(IAudinDevice* device)
{
	pa_context_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		WLog_Print(pulse->log, WLOG_ERROR, "pa_context_connect failed (%d)",
		           pa_context_errno(pulse->context));
		return ERROR_INTERNAL_ERROR;
	}

	pa_threaded_mainloop_lock(pulse->mainloop);

	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_Print(pulse->log, WLOG_ERROR, "pa_threaded_mainloop_start failed (%d)",
		           pa_context_errno(pulse->context));
		return ERROR_INTERNAL_ERROR;
	}

	for (;;)
	{
		state = pa_context_get_state(pulse->context);

		if (state == PA_CONTEXT_READY)
			break;

		if (!PA_CONTEXT_IS_GOOD(state))
		{
			WLog_Print(pulse->log, WLOG_ERROR, "bad context state (%s: %d)",
			           pulse_context_state_string(state), pa_context_errno(pulse->context));
			pa_context_disconnect(pulse->context);
			return ERROR_INVALID_STATE;
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);
	WLog_Print(pulse->log, WLOG_DEBUG, "connected");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_free(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse)
		return ERROR_INVALID_PARAMETER;

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

	free(pulse);
	return CHANNEL_RC_OK;
}

static BOOL audin_pulse_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse || !format)
		return FALSE;

	if (!pulse->context)
		return 0;

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

		default:
			return FALSE;
	}

	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                   UINT32 FramesPerPacket)
{
	pa_sample_spec sample_spec = { 0 };
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse || !format)
		return ERROR_INVALID_PARAMETER;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;

	if (FramesPerPacket > 0)
		pulse->frames_per_packet = FramesPerPacket;

	sample_spec.rate = format->nSamplesPerSec;
	sample_spec.channels = format->nChannels;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM: /* PCM */
			switch (format->wBitsPerSample)
			{
				case 8:
					sample_spec.format = PA_SAMPLE_U8;
					break;

				case 16:
					sample_spec.format = PA_SAMPLE_S16LE;
					break;

				default:
					return ERROR_INTERNAL_ERROR;
			}

			break;

		case WAVE_FORMAT_ALAW: /* A-LAW */
			sample_spec.format = PA_SAMPLE_ALAW;
			break;

		case WAVE_FORMAT_MULAW: /* U-LAW */
			sample_spec.format = PA_SAMPLE_ULAW;
			break;

		default:
			return ERROR_INTERNAL_ERROR;
	}

	pulse->sample_spec = sample_spec;
	pulse->format = *format;
	return CHANNEL_RC_OK;
}

static void audin_pulse_stream_state_callback(pa_stream* stream, void* userdata)
{
	pa_stream_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*)userdata;
	state = pa_stream_get_state(stream);

	WLog_Print(pulse->log, WLOG_DEBUG, "stream state %s", pulse_stream_state_string(state));
	switch (state)
	{
		case PA_STREAM_READY:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_STREAM_FAILED:
		case PA_STREAM_TERMINATED:
			pa_threaded_mainloop_signal(pulse->mainloop, 0);
			break;

		case PA_STREAM_UNCONNECTED:
		case PA_STREAM_CREATING:
		default:
			break;
	}
}

static void audin_pulse_stream_request_callback(pa_stream* stream, size_t length, void* userdata)
{
	const void* data;
	AudinPulseDevice* pulse = (AudinPulseDevice*)userdata;
	UINT error = CHANNEL_RC_OK;
	pa_stream_peek(stream, &data, &length);
	error =
	    IFCALLRESULT(CHANNEL_RC_OK, pulse->receive, &pulse->format, data, length, pulse->user_data);
	pa_stream_drop(stream);

	if (error && pulse->rdpcontext)
		setChannelError(pulse->rdpcontext, error, "audin_pulse_thread_func reported an error");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_close(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse)
		return ERROR_INVALID_PARAMETER;

	if (pulse->stream)
	{
		pa_threaded_mainloop_lock(pulse->mainloop);
		pa_stream_disconnect(pulse->stream);
		pa_stream_unref(pulse->stream);
		pulse->stream = NULL;
		pa_threaded_mainloop_unlock(pulse->mainloop);
	}

	pulse->receive = NULL;
	pulse->user_data = NULL;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	pa_stream_state_t state;
	pa_buffer_attr buffer_attr = { 0 };
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;

	if (!pulse || !receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;

	if (!pulse->sample_spec.rate || pulse->stream)
		return ERROR_INVALID_PARAMETER;

	pulse->receive = receive;
	pulse->user_data = user_data;
	pa_threaded_mainloop_lock(pulse->mainloop);
	pulse->stream = pa_stream_new(pulse->context, "freerdp_audin", &pulse->sample_spec, NULL);

	if (!pulse->stream)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_Print(pulse->log, WLOG_DEBUG, "pa_stream_new failed (%d)",
		           pa_context_errno(pulse->context));
		return pa_context_errno(pulse->context);
	}

	pulse->bytes_per_frame = pa_frame_size(&pulse->sample_spec);
	pa_stream_set_state_callback(pulse->stream, audin_pulse_stream_state_callback, pulse);
	pa_stream_set_read_callback(pulse->stream, audin_pulse_stream_request_callback, pulse);
	buffer_attr.maxlength = (UINT32)-1;
	buffer_attr.tlength = (UINT32)-1;
	buffer_attr.prebuf = (UINT32)-1;
	buffer_attr.minreq = (UINT32)-1;
	/* 500ms latency */
	buffer_attr.fragsize = pulse->bytes_per_frame * pulse->frames_per_packet;

	if (buffer_attr.fragsize % pulse->format.nBlockAlign)
		buffer_attr.fragsize +=
		    pulse->format.nBlockAlign - buffer_attr.fragsize % pulse->format.nBlockAlign;

	if (pa_stream_connect_record(pulse->stream, pulse->device_name, &buffer_attr,
	                             PA_STREAM_ADJUST_LATENCY) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_Print(pulse->log, WLOG_ERROR, "pa_stream_connect_playback failed (%d)",
		           pa_context_errno(pulse->context));
		return pa_context_errno(pulse->context);
	}

	for (;;)
	{
		state = pa_stream_get_state(pulse->stream);

		if (state == PA_STREAM_READY)
			break;

		if (!PA_STREAM_IS_GOOD(state))
		{
			audin_pulse_close(device);
			WLog_Print(pulse->log, WLOG_ERROR, "bad stream state (%s: %d)",
			           pulse_stream_state_string(state), pa_context_errno(pulse->context));
			pa_threaded_mainloop_unlock(pulse->mainloop);
			return pa_context_errno(pulse->context);
		}

		pa_threaded_mainloop_wait(pulse->mainloop);
	}

	pa_threaded_mainloop_unlock(pulse->mainloop);
	pulse->buffer_frames = 0;
	WLog_Print(pulse->log, WLOG_DEBUG, "connected");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_parse_addin_args(AudinPulseDevice* device, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	AudinPulseDevice* pulse = (AudinPulseDevice*)device;
	COMMAND_LINE_ARGUMENT_A audin_pulse_args[] = { { "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		                                             NULL, NULL, -1, NULL, "audio device name" },
		                                           { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };

	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, audin_pulse_args, flags, pulse,
	                                    NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_PARAMETER;

	arg = audin_pulse_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			pulse->device_name = _strdup(arg->Value);

			if (!pulse->device_name)
			{
				WLog_Print(pulse->log, WLOG_ERROR, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT pulse_freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	AudinPulseDevice* pulse;
	UINT error;
	pulse = (AudinPulseDevice*)calloc(1, sizeof(AudinPulseDevice));

	if (!pulse)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	pulse->log = WLog_Get(TAG);
	pulse->iface.Open = audin_pulse_open;
	pulse->iface.FormatSupported = audin_pulse_format_supported;
	pulse->iface.SetFormat = audin_pulse_set_format;
	pulse->iface.Close = audin_pulse_close;
	pulse->iface.Free = audin_pulse_free;
	pulse->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if ((error = audin_pulse_parse_addin_args(pulse, args)))
	{
		WLog_Print(pulse->log, WLOG_ERROR,
		           "audin_pulse_parse_addin_args failed with error %" PRIu32 "!", error);
		goto error_out;
	}

	pulse->mainloop = pa_threaded_mainloop_new();

	if (!pulse->mainloop)
	{
		WLog_Print(pulse->log, WLOG_ERROR, "pa_threaded_mainloop_new failed");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");

	if (!pulse->context)
	{
		WLog_Print(pulse->log, WLOG_ERROR, "pa_context_new failed");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	pa_context_set_state_callback(pulse->context, audin_pulse_context_state_callback, pulse);

	if ((error = audin_pulse_connect(&pulse->iface)))
	{
		WLog_Print(pulse->log, WLOG_ERROR, "audin_pulse_connect failed");
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, &pulse->iface)))
	{
		WLog_Print(pulse->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	audin_pulse_free(&pulse->iface);
	return error;
}
