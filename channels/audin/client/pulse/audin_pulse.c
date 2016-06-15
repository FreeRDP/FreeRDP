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
#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"

typedef struct _AudinPulseDevice
{
	IAudinDevice iface;

	char* device_name;
	UINT32 frames_per_packet;
	pa_threaded_mainloop* mainloop;
	pa_context* context;
	pa_sample_spec sample_spec;
	pa_stream* stream;
	int format;
	int block_size;

	FREERDP_DSP_CONTEXT* dsp_context;

	int bytes_per_frame;
	BYTE* buffer;
	int buffer_frames;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_connect(IAudinDevice* device)
{
	pa_context_state_t state;
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;

	if (pa_context_connect(pulse->context, NULL, 0, NULL))
	{
		WLog_ERR(TAG, "pa_context_connect failed (%d)",
				 pa_context_errno(pulse->context));
		return ERROR_INTERNAL_ERROR;
	}
	pa_threaded_mainloop_lock(pulse->mainloop);
	if (pa_threaded_mainloop_start(pulse->mainloop) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_ERR(TAG, "pa_threaded_mainloop_start failed (%d)",
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
			WLog_ERR(TAG, "bad context state (%d)",
					 pa_context_errno(pulse->context));
			pa_context_disconnect(pulse->context);
			return ERROR_INVALID_STATE;
		}
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_threaded_mainloop_unlock(pulse->mainloop);
	DEBUG_DVC("connected");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_free(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

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
	freerdp_dsp_context_free(pulse->dsp_context);
	free(pulse);

	return CHANNEL_RC_OK;
}

static BOOL audin_pulse_format_supported(IAudinDevice* device, audinFormat* format)
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
				return TRUE;
			}
			break;

		case 6: /* A-LAW */
		case 7: /* U-LAW */
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= PA_RATE_MAX) &&
				(format->wBitsPerSample == 8) &&
				(format->nChannels >= 1 && format->nChannels <= PA_CHANNELS_MAX))
			{
				return TRUE;
			}
			break;

		case 0x11: /* IMA ADPCM */
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
{
	int bs;
	pa_sample_spec sample_spec = { 0 };
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;

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
	return CHANNEL_RC_OK;
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
	const void* data;
	const BYTE* src;
	int encoded_size;
	BYTE* encoded_data;
	AudinPulseDevice* pulse = (AudinPulseDevice*) userdata;
	UINT error = CHANNEL_RC_OK;

	/* There is a race condition here where we may receive this callback
	 * before the buffer has been set up in the main code.  It's probably
	 * possible to fix with additional locking, but it's easier just to
	 * ignore input until the buffer is ready.
	 */
	if (pulse->buffer == NULL)
	{
		/* WLog_ERR(TAG,  "%s: ignoring input, pulse buffer not ready.\n", __func__); */
		return;
	}

	pa_stream_peek(stream, &data, &length);
	frames = length / pulse->bytes_per_frame;

	DEBUG_DVC("length %d frames %d", (int) length, frames);

	src = (const BYTE*) data;
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
				if (!pulse->dsp_context->encode_ima_adpcm(pulse->dsp_context,
					pulse->buffer, pulse->buffer_frames * pulse->bytes_per_frame,
					pulse->sample_spec.channels, pulse->block_size))
				{
					error = ERROR_INTERNAL_ERROR;
					break;
				}
				encoded_data = pulse->dsp_context->adpcm_buffer;
				encoded_size = pulse->dsp_context->adpcm_size;
			}
			else
			{
				encoded_data = pulse->buffer;
				encoded_size = pulse->buffer_frames * pulse->bytes_per_frame;
			}

			DEBUG_DVC("encoded %d [%d] to %d [%X]",
				pulse->buffer_frames, pulse->bytes_per_frame, encoded_size,
				pulse->format);
			error = pulse->receive(encoded_data, encoded_size, pulse->user_data);
			pulse->buffer_frames = 0;
			if (!error)
				break;
		}
		src += cframes * pulse->bytes_per_frame;
		frames -= cframes;
	}

	pa_stream_drop(stream);

	if (error && pulse->rdpcontext)
		setChannelError(pulse->rdpcontext, error, "audin_oss_thread_func reported an error");
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_close(IAudinDevice* device)
{
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context || !pulse->stream)
		return ERROR_INVALID_PARAMETER;

	pa_threaded_mainloop_lock(pulse->mainloop);
	pa_stream_disconnect(pulse->stream);
	pa_stream_unref(pulse->stream);
	pulse->stream = NULL;
	pa_threaded_mainloop_unlock(pulse->mainloop);

	pulse->receive = NULL;
	pulse->user_data = NULL;
	if (pulse->buffer)
	{
		free(pulse->buffer);
		pulse->buffer = NULL;
		pulse->buffer_frames = 0;
	}
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
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	if (!pulse->context)
		return ERROR_INVALID_PARAMETER;
	if (!pulse->sample_spec.rate || pulse->stream)
		return ERROR_INVALID_PARAMETER;

	pulse->buffer = NULL;
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
		return pa_context_errno(pulse->context);
	}
	pulse->bytes_per_frame = pa_frame_size(&pulse->sample_spec);
	pa_stream_set_state_callback(pulse->stream,
		audin_pulse_stream_state_callback, pulse);
	pa_stream_set_read_callback(pulse->stream,
		audin_pulse_stream_request_callback, pulse);
	buffer_attr.maxlength = (UINT32) -1;
	buffer_attr.tlength = (UINT32) -1;
	buffer_attr.prebuf = (UINT32) -1;
	buffer_attr.minreq = (UINT32) -1;
	/* 500ms latency */
	buffer_attr.fragsize = pa_usec_to_bytes(500000, &pulse->sample_spec);
	if (pa_stream_connect_record(pulse->stream,
		pulse->device_name,
		&buffer_attr, PA_STREAM_ADJUST_LATENCY) < 0)
	{
		pa_threaded_mainloop_unlock(pulse->mainloop);
		WLog_ERR(TAG, "pa_stream_connect_playback failed (%d)",
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
			WLog_ERR(TAG, "bad stream state (%d)",
					 pa_context_errno(pulse->context));
			pa_threaded_mainloop_unlock(pulse->mainloop);
			return pa_context_errno(pulse->context);
		}
		pa_threaded_mainloop_wait(pulse->mainloop);
	}
	pa_threaded_mainloop_unlock(pulse->mainloop);
	freerdp_dsp_context_reset_adpcm(pulse->dsp_context);
	pulse->buffer = calloc(1, pulse->bytes_per_frame * pulse->frames_per_packet);
	if (!pulse->buffer) {
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	pulse->buffer_frames = 0;
	DEBUG_DVC("connected");
	return CHANNEL_RC_OK;
}

static COMMAND_LINE_ARGUMENT_A audin_pulse_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_pulse_parse_addin_args(AudinPulseDevice* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinPulseDevice* pulse = (AudinPulseDevice*) device;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv, audin_pulse_args, flags, pulse, NULL, NULL);

	arg = audin_pulse_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			pulse->device_name = _strdup(arg->Value);
			if (!pulse->device_name)
			{
				WLog_ERR(TAG, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_audin_client_subsystem_entry	pulse_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry	FREERDP_API freerdp_audin_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinPulseDevice* pulse;
	UINT error;

	pulse = (AudinPulseDevice*) calloc(1, sizeof(AudinPulseDevice));
	if (!pulse)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	pulse->iface.Open = audin_pulse_open;
	pulse->iface.FormatSupported = audin_pulse_format_supported;
	pulse->iface.SetFormat = audin_pulse_set_format;
	pulse->iface.Close = audin_pulse_close;
	pulse->iface.Free = audin_pulse_free;
	pulse->rdpcontext = pEntryPoints->rdpcontext;

	args = pEntryPoints->args;

	if ((error = audin_pulse_parse_addin_args(pulse, args)))
	{
		WLog_ERR(TAG, "audin_pulse_parse_addin_args failed with error %lu!", error);
		goto error_out;
	}

	pulse->dsp_context = freerdp_dsp_context_new();
	if (!pulse->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	pulse->mainloop = pa_threaded_mainloop_new();

	if (!pulse->mainloop)
	{
		WLog_ERR(TAG, "pa_threaded_mainloop_new failed");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	pulse->context = pa_context_new(pa_threaded_mainloop_get_api(pulse->mainloop), "freerdp");

	if (!pulse->context)
	{
		WLog_ERR(TAG, "pa_context_new failed");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	pa_context_set_state_callback(pulse->context, audin_pulse_context_state_callback, pulse);

	if ((error = audin_pulse_connect((IAudinDevice*) pulse)))
	{
		WLog_ERR(TAG, "audin_pulse_connect failed");
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) pulse)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %lu!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	audin_pulse_free((IAudinDevice*)pulse);
	return error;
}

