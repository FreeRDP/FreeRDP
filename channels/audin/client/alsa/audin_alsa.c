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
#include <winpr/wlog.h>

#include <alsa/asoundlib.h>

#include <freerdp/addin.h>
#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

typedef struct _AudinALSADevice
{
	IAudinDevice iface;

	char* device_name;
	UINT32 frames_per_packet;
	AUDIO_FORMAT aformat;

	HANDLE thread;
	HANDLE stopEvent;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
	wLog* log;
	int bytes_per_frame;
} AudinALSADevice;

static snd_pcm_format_t audin_alsa_format(UINT32 wFormatTag, UINT32 bitPerChannel)
{
	switch (wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			switch (bitPerChannel)
			{
				case 16:
					return SND_PCM_FORMAT_S16_LE;

				case 8:
					return SND_PCM_FORMAT_S8;

				default:
					return SND_PCM_FORMAT_UNKNOWN;
			}

		case WAVE_FORMAT_ALAW:
			return SND_PCM_FORMAT_A_LAW;

		case WAVE_FORMAT_MULAW:
			return SND_PCM_FORMAT_MU_LAW;

		default:
			return SND_PCM_FORMAT_UNKNOWN;
	}
}

static BOOL audin_alsa_set_params(AudinALSADevice* alsa, snd_pcm_t* capture_handle)
{
	int error;
	SSIZE_T s;
	UINT32 channels = alsa->aformat.nChannels;
	snd_pcm_hw_params_t* hw_params;
	snd_pcm_format_t format =
	    audin_alsa_format(alsa->aformat.wFormatTag, alsa->aformat.wBitsPerSample);

	if ((error = snd_pcm_hw_params_malloc(&hw_params)) < 0)
	{
		WLog_Print(alsa->log, WLOG_ERROR, "snd_pcm_hw_params_malloc (%s)", snd_strerror(error));
		return FALSE;
	}

	snd_pcm_hw_params_any(capture_handle, hw_params);
	snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(capture_handle, hw_params, format);
	snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &alsa->aformat.nSamplesPerSec, NULL);
	snd_pcm_hw_params_set_channels_near(capture_handle, hw_params, &channels);
	snd_pcm_hw_params(capture_handle, hw_params);
	snd_pcm_hw_params_free(hw_params);
	snd_pcm_prepare(capture_handle);
	if (channels > UINT16_MAX)
		return FALSE;
	s = snd_pcm_format_size(format, 1);
	if ((s < 0) || (s > UINT16_MAX))
		return FALSE;
	alsa->aformat.nChannels = (UINT16)channels;
	alsa->bytes_per_frame = (size_t)s * channels;
	return TRUE;
}

static DWORD WINAPI audin_alsa_thread_func(LPVOID arg)
{
	long error;
	BYTE* buffer;
	snd_pcm_t* capture_handle = NULL;
	AudinALSADevice* alsa = (AudinALSADevice*)arg;
	DWORD status;
	WLog_Print(alsa->log, WLOG_DEBUG, "in");

	if ((error = snd_pcm_open(&capture_handle, alsa->device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0)
	{
		WLog_Print(alsa->log, WLOG_ERROR, "snd_pcm_open (%s)", snd_strerror(error));
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto out;
	}

	if (!audin_alsa_set_params(alsa, capture_handle))
	{
		WLog_Print(alsa->log, WLOG_ERROR, "audin_alsa_set_params failed");
		goto out;
	}

	buffer =
	    (BYTE*)calloc(alsa->frames_per_packet + alsa->aformat.nBlockAlign, alsa->bytes_per_frame);

	if (!buffer)
	{
		WLog_Print(alsa->log, WLOG_ERROR, "calloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	while (1)
	{
		size_t frames = alsa->frames_per_packet;
		status = WaitForSingleObject(alsa->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(alsa->log, WLOG_ERROR, "WaitForSingleObject failed with error %ld!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		error = snd_pcm_readi(capture_handle, buffer, frames);

		if (error == 0)
			continue;

		if (error == -EPIPE)
		{
			snd_pcm_recover(capture_handle, error, 0);
			continue;
		}
		else if (error < 0)
		{
			WLog_Print(alsa->log, WLOG_ERROR, "snd_pcm_readi (%s)", snd_strerror(error));
			break;
		}

		error =
		    alsa->receive(&alsa->aformat, buffer, error * alsa->bytes_per_frame, alsa->user_data);

		if (error)
		{
			WLog_Print(alsa->log, WLOG_ERROR, "audin_alsa_thread_receive failed with error %ld",
			           error);
			break;
		}
	}

	free(buffer);

	if (capture_handle)
		snd_pcm_close(capture_handle);

out:
	WLog_Print(alsa->log, WLOG_DEBUG, "out");

	if (error && alsa->rdpcontext)
		setChannelError(alsa->rdpcontext, error, "audin_alsa_thread_func reported an error");

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
	AudinALSADevice* alsa = (AudinALSADevice*)device;

	if (alsa)
		free(alsa->device_name);

	free(alsa);
	return CHANNEL_RC_OK;
}

static BOOL audin_alsa_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	if (!device || !format)
		return FALSE;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			if (format->cbSize == 0 && (format->nSamplesPerSec <= 48000) &&
			    (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			    (format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}

			break;

		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
			return TRUE;

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
static UINT audin_alsa_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                  UINT32 FramesPerPacket)
{
	AudinALSADevice* alsa = (AudinALSADevice*)device;

	if (!alsa || !format)
		return ERROR_INVALID_PARAMETER;

	alsa->aformat = *format;
	alsa->frames_per_packet = FramesPerPacket;

	if (audin_alsa_format(format->wFormatTag, format->wBitsPerSample) == SND_PCM_FORMAT_UNKNOWN)
		return ERROR_INTERNAL_ERROR;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinALSADevice* alsa = (AudinALSADevice*)device;

	if (!device || !receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	alsa->receive = receive;
	alsa->user_data = user_data;

	if (!(alsa->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_Print(alsa->log, WLOG_ERROR, "CreateEvent failed!");
		goto error_out;
	}

	if (!(alsa->thread = CreateThread(NULL, 0, audin_alsa_thread_func, alsa, 0, NULL)))
	{
		WLog_Print(alsa->log, WLOG_ERROR, "CreateThread failed!");
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
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
	AudinALSADevice* alsa = (AudinALSADevice*)device;

	if (!alsa)
		return ERROR_INVALID_PARAMETER;

	if (alsa->stopEvent)
	{
		SetEvent(alsa->stopEvent);

		if (WaitForSingleObject(alsa->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(alsa->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "",
			           error);
			return error;
		}

		CloseHandle(alsa->stopEvent);
		alsa->stopEvent = NULL;
		CloseHandle(alsa->thread);
		alsa->thread = NULL;
	}

	alsa->receive = NULL;
	alsa->user_data = NULL;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_alsa_parse_addin_args(AudinALSADevice* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinALSADevice* alsa = (AudinALSADevice*)device;
	COMMAND_LINE_ARGUMENT_A audin_alsa_args[] = { { "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		                                            NULL, NULL, -1, NULL, "audio device name" },
		                                          { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, audin_alsa_args, flags, alsa, NULL,
	                                    NULL);

	if (status < 0)
		return ERROR_INVALID_PARAMETER;

	arg = audin_alsa_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			alsa->device_name = _strdup(arg->Value);

			if (!alsa->device_name)
			{
				WLog_Print(alsa->log, WLOG_ERROR, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_audin_client_subsystem_entry alsa_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry FREERDP_API freerdp_audin_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinALSADevice* alsa;
	UINT error;
	alsa = (AudinALSADevice*)calloc(1, sizeof(AudinALSADevice));

	if (!alsa)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	alsa->log = WLog_Get(TAG);
	alsa->iface.Open = audin_alsa_open;
	alsa->iface.FormatSupported = audin_alsa_format_supported;
	alsa->iface.SetFormat = audin_alsa_set_format;
	alsa->iface.Close = audin_alsa_close;
	alsa->iface.Free = audin_alsa_free;
	alsa->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if ((error = audin_alsa_parse_addin_args(alsa, args)))
	{
		WLog_Print(alsa->log, WLOG_ERROR,
		           "audin_alsa_parse_addin_args failed with errorcode %" PRIu32 "!", error);
		goto error_out;
	}

	if (!alsa->device_name)
	{
		alsa->device_name = _strdup("default");

		if (!alsa->device_name)
		{
			WLog_Print(alsa->log, WLOG_ERROR, "_strdup failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}
	}

	alsa->frames_per_packet = 128;
	alsa->aformat.nChannels = 2;
	alsa->aformat.wBitsPerSample = 16;
	alsa->aformat.wFormatTag = WAVE_FORMAT_PCM;
	alsa->aformat.nSamplesPerSec = 44100;

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*)alsa)))
	{
		WLog_Print(alsa->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(alsa->device_name);
	free(alsa);
	return error;
}
