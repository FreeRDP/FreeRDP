/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - ALSA implementation
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

	rdpContext* rdpcontext;
} AudinALSADevice;

static BOOL audin_alsa_set_params(AudinALSADevice* alsa,
                                  snd_pcm_t* capture_handle)
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
	snd_pcm_hw_params_set_access(capture_handle, hw_params,
	                             SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(capture_handle, hw_params, alsa->format);
	snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &alsa->actual_rate,
	                                NULL);
	snd_pcm_hw_params_set_channels_near(capture_handle, hw_params,
	                                    &alsa->actual_channels);
	snd_pcm_hw_params(capture_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);
	snd_pcm_prepare(capture_handle);

	if ((alsa->actual_rate != alsa->target_rate) ||
	    (alsa->actual_channels != alsa->target_channels))
	{
		DEBUG_DVC("actual rate %"PRIu32" / channel %"PRIu32" is "
		          "different from target rate %"PRIu32" / channel %"PRIu32", resampling required.",
		          alsa->actual_rate, alsa->actual_channels,
		          alsa->target_rate, alsa->target_channels);
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_thread_receive(AudinALSADevice* alsa, BYTE* src,
                                      int size)
{
	int frames;
	int cframes;
	UINT ret = CHANNEL_RC_OK;
	int encoded_size;
	BYTE* encoded_data;
	int rbytes_per_frame;
	int tbytes_per_frame;
	int status;
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
		DEBUG_DVC("resampled %d frames at %"PRIu32" to %d frames at %"PRIu32"",
		          size / rbytes_per_frame, alsa->actual_rate, frames, alsa->target_rate);
		src = alsa->dsp_context->resampled_buffer;
	}

	while (frames > 0)
	{
		status = WaitForSingleObject(alsa->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			ret = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", ret);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		cframes = alsa->frames_per_packet - alsa->buffer_frames;

		if (cframes > frames)
			cframes = frames;

		CopyMemory(alsa->buffer + alsa->buffer_frames * tbytes_per_frame, src,
		           cframes * tbytes_per_frame);
		alsa->buffer_frames += cframes;

		if (alsa->buffer_frames >= alsa->frames_per_packet)
		{
			if (alsa->wformat == WAVE_FORMAT_DVI_ADPCM)
			{
				if (!alsa->dsp_context->encode_ima_adpcm(alsa->dsp_context,
				        alsa->buffer, alsa->buffer_frames * tbytes_per_frame,
				        alsa->target_channels, alsa->block_size))
				{
					ret = ERROR_INTERNAL_ERROR;
					break;
				}

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

			status = WaitForSingleObject(alsa->stopEvent, 0);

			if (status == WAIT_FAILED)
			{
				ret = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", ret);
				break;
			}

			if (status == WAIT_OBJECT_0)
				break;
			else
			{
				DEBUG_DVC("encoded %d [%d] to %d [%X]", alsa->buffer_frames,
				          tbytes_per_frame, encoded_size,
				          alsa->wformat);
				ret = alsa->receive(encoded_data, encoded_size, alsa->user_data);
			}

			alsa->buffer_frames = 0;

			if (ret != CHANNEL_RC_OK)
				break;
		}

		src += cframes * tbytes_per_frame;
		frames -= cframes;
	}

	return ret;
}

static DWORD WINAPI audin_alsa_thread_func(LPVOID arg)
{
	long error;
	BYTE* buffer;
	int rbytes_per_frame;
	snd_pcm_t* capture_handle = NULL;
	AudinALSADevice* alsa = (AudinALSADevice*) arg;
	DWORD status;
	DEBUG_DVC("in");
	rbytes_per_frame = alsa->actual_channels * alsa->bytes_per_channel;
	buffer = (BYTE*) calloc(alsa->frames_per_packet, rbytes_per_frame);

	if (!buffer)
	{
		WLog_ERR(TAG, "calloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	freerdp_dsp_context_reset_adpcm(alsa->dsp_context);

	if ((error = snd_pcm_open(&capture_handle, alsa->device_name,
	                          SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		WLog_ERR(TAG, "snd_pcm_open (%s)", snd_strerror(error));
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto out;
	}

	if (!audin_alsa_set_params(alsa, capture_handle))
	{
		WLog_ERR(TAG, "audin_alsa_set_params failed");
		goto out;
	}

	while (1)
	{
		status = WaitForSingleObject(alsa->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %ld!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

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

		if ((error = audin_alsa_thread_receive(alsa, buffer, error * rbytes_per_frame)))
		{
			WLog_ERR(TAG, "audin_alsa_thread_receive failed with error %ld", error);
			break;
		}
	}

	free(buffer);

	if (capture_handle)
		snd_pcm_close(capture_handle);

out:
	DEBUG_DVC("out");

	if (error && alsa->rdpcontext)
		setChannelError(alsa->rdpcontext, error,
		                "audin_alsa_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_free(IAudinDevice* device)
{
	AudinALSADevice* alsa = (AudinALSADevice*) device;
	freerdp_dsp_context_free(alsa->dsp_context);
	free(alsa->device_name);
	free(alsa);
	return CHANNEL_RC_OK;
}

static BOOL audin_alsa_format_supported(IAudinDevice* device,
                                        audinFormat* format)
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_set_format(IAudinDevice* device, audinFormat* format,
                                  UINT32 FramesPerPacket)
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
			DEBUG_DVC("aligned FramesPerPacket=%"PRIu32"",
			          alsa->frames_per_packet);
			break;
	}

	alsa->wformat = format->wFormatTag;
	alsa->block_size = format->nBlockAlign;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_open(IAudinDevice* device, AudinReceive receive,
                            void* user_data)
{
	int tbytes_per_frame;
	AudinALSADevice* alsa = (AudinALSADevice*) device;
	alsa->receive = receive;
	alsa->user_data = user_data;
	tbytes_per_frame = alsa->target_channels * alsa->bytes_per_channel;
	alsa->buffer = (BYTE*) calloc(alsa->frames_per_packet, tbytes_per_frame);

	if (!alsa->buffer)
	{
		WLog_ERR(TAG, "calloc failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	alsa->buffer_frames = 0;

	if (!(alsa->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto error_out;
	}

	if (!(alsa->thread = CreateThread(NULL, 0,
									  audin_alsa_thread_func, alsa, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(alsa->buffer);
	alsa->buffer = NULL;
	CloseHandle(alsa->stopEvent);
	alsa->stopEvent = NULL;
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_close(IAudinDevice* device)
{
	UINT error = CHANNEL_RC_OK;
	AudinALSADevice* alsa = (AudinALSADevice*) device;

	if (alsa->stopEvent)
	{
		SetEvent(alsa->stopEvent);

		if (WaitForSingleObject(alsa->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			return error;
		}

		CloseHandle(alsa->stopEvent);
		alsa->stopEvent = NULL;
		CloseHandle(alsa->thread);
		alsa->thread = NULL;
	}

	free(alsa->buffer);
	alsa->buffer = NULL;
	alsa->receive = NULL;
	alsa->user_data = NULL;
	return error;
}

COMMAND_LINE_ARGUMENT_A audin_alsa_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_parse_addin_args(AudinALSADevice* device,
                                        ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinALSADevice* alsa = (AudinALSADevice*) device;
	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON |
	        COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
	                                    audin_alsa_args, flags, alsa, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_PARAMETER;

	arg = audin_alsa_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "dev")
		{
			alsa->device_name = _strdup(arg->Value);

			if (!alsa->device_name)
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
#define freerdp_audin_client_subsystem_entry	alsa_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry	FREERDP_API freerdp_audin_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS
        pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinALSADevice* alsa;
	UINT error;
	alsa = (AudinALSADevice*) calloc(1, sizeof(AudinALSADevice));

	if (!alsa)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	alsa->iface.Open = audin_alsa_open;
	alsa->iface.FormatSupported = audin_alsa_format_supported;
	alsa->iface.SetFormat = audin_alsa_set_format;
	alsa->iface.Close = audin_alsa_close;
	alsa->iface.Free = audin_alsa_free;
	alsa->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if ((error = audin_alsa_parse_addin_args(alsa, args)))
	{
		WLog_ERR(TAG, "audin_alsa_parse_addin_args failed with errorcode %"PRIu32"!", error);
		goto error_out;
	}

	if (!alsa->device_name)
	{
		alsa->device_name = _strdup("default");

		if (!alsa->device_name)
		{
			WLog_ERR(TAG, "_strdup failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}
	}

	alsa->frames_per_packet = 128;
	alsa->target_rate = 22050;
	alsa->actual_rate = 22050;
	alsa->format = SND_PCM_FORMAT_S16_LE;
	alsa->target_channels = 2;
	alsa->actual_channels = 2;
	alsa->bytes_per_channel = 2;
	alsa->dsp_context = freerdp_dsp_context_new();

	if (!alsa->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin,
	             (IAudinDevice*) alsa)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %"PRIu32"!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	freerdp_dsp_context_free(alsa->dsp_context);
	free(alsa->device_name);
	free(alsa);
	return error;
}
