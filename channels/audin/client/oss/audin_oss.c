/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - OSS implementation
 *
 * Copyright (c) 2015 Rozhuk Ivan <rozhuk.im@gmail.com>
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
#include <winpr/string.h>
#include <winpr/thread.h>
#include <winpr/cmdline.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#if defined(__OpenBSD__)
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif
#include <sys/ioctl.h>

#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

typedef struct _AudinOSSDevice
{
	IAudinDevice iface;

	FREERDP_DSP_CONTEXT* dsp_context;

	HANDLE thread;
	HANDLE stopEvent;

	audinFormat format;
	UINT32 FramesPerPacket;
	int dev_unit;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
} AudinOSSDevice;

#define OSS_LOG_ERR(_text, _error) \
	if (_error != 0) \
		WLog_ERR(TAG, "%s: %i - %s\n", _text, _error, strerror(_error));


static int audin_oss_get_format(audinFormat* format)
{
	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:

			switch (format->wBitsPerSample)
			{
				case 8:
					return AFMT_S8;
				case 16:
					return AFMT_S16_LE;
			}

			break;
		case WAVE_FORMAT_ALAW:
			return AFMT_A_LAW;
#if 0 /* This does not work on my desktop. */
		case WAVE_FORMAT_MULAW:
			return AFMT_MU_LAW;
#endif
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			return AFMT_S16_LE;
	}

	return 0;
}

static BOOL audin_oss_format_supported(IAudinDevice* device, audinFormat* format)
{
	int req_fmt = 0;

	if (device == NULL || format == NULL)
		return FALSE;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:

			if (format->cbSize != 0 ||
					format->nSamplesPerSec > 48000 ||
					(format->wBitsPerSample != 8 && format->wBitsPerSample != 16) ||
					(format->nChannels != 1 && format->nChannels != 2))
				return FALSE;

			break;
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:

			if (format->nSamplesPerSec > 48000 ||
					format->wBitsPerSample != 4 ||
					(format->nChannels != 1 && format->nChannels != 2))
				return FALSE;

			break;
	}

	req_fmt = audin_oss_get_format(format);

	if (req_fmt == 0)
		return FALSE;

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_oss_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
{
	AudinOSSDevice* oss = (AudinOSSDevice*)device;

	if (device == NULL || format == NULL)
		return ERROR_INVALID_PARAMETER;

	oss->FramesPerPacket = FramesPerPacket;
	CopyMemory(&(oss->format), format, sizeof(audinFormat));

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			oss->FramesPerPacket *= 4; /* Compression ratio. */
			oss->format.wBitsPerSample *= 4;
			break;
	}
	return CHANNEL_RC_OK;
}

static void* audin_oss_thread_func(void* arg)
{
	char dev_name[PATH_MAX] = "/dev/dsp";
	char mixer_name[PATH_MAX] = "/dev/mixer";
	int pcm_handle = -1, mixer_handle;
	BYTE* buffer = NULL, *encoded_data = NULL;
	int tmp, buffer_size, encoded_size;
	AudinOSSDevice* oss = (AudinOSSDevice*)arg;
	UINT error = 0;
	DWORD status;

	if (arg == NULL)
	{
		error = ERROR_INVALID_PARAMETER;
		goto err_out;
	}

	if (oss->dev_unit != -1)
	{
		sprintf_s(dev_name, (PATH_MAX - 1), "/dev/dsp%i", oss->dev_unit);
		sprintf_s(mixer_name, PATH_MAX - 1, "/dev/mixer%i", oss->dev_unit);
	}

	WLog_INFO(TAG, "open: %s", dev_name);

	if ((pcm_handle = open(dev_name, O_RDONLY)) < 0)
	{
		OSS_LOG_ERR("sound dev open failed", errno);
		error = ERROR_INTERNAL_ERROR;
		goto err_out;
	}

	/* Set rec volume to 100%. */
	if ((mixer_handle = open(mixer_name, O_RDWR)) < 0)
	{
		OSS_LOG_ERR("mixer open failed, not critical", errno);
	}
	else
	{
		tmp = (100 | (100 << 8));

		if (ioctl(mixer_handle, MIXER_WRITE(SOUND_MIXER_MIC), &tmp) == -1)
			OSS_LOG_ERR("WRITE_MIXER - SOUND_MIXER_MIC, not critical", errno);

		tmp = (100 | (100 << 8));

		if (ioctl(mixer_handle, MIXER_WRITE(SOUND_MIXER_RECLEV), &tmp) == -1)
			OSS_LOG_ERR("WRITE_MIXER - SOUND_MIXER_RECLEV, not critical", errno);

		close(mixer_handle);
	}

#if 0 /* FreeBSD OSS implementation at this moment (2015.03) does not set PCM_CAP_INPUT flag. */
	tmp = 0;

	if (ioctl(pcm_handle, SNDCTL_DSP_GETCAPS, &tmp) == -1)
	{
		OSS_LOG_ERR("SNDCTL_DSP_GETCAPS failed, try ignory", errno);
	}
	else if ((tmp & PCM_CAP_INPUT) == 0)
	{
		OSS_LOG_ERR("Device does not supports playback", EOPNOTSUPP);
		goto err_out;
	}

#endif
	/* Set format. */
	tmp = audin_oss_get_format(&oss->format);

	if (ioctl(pcm_handle, SNDCTL_DSP_SETFMT, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SETFMT failed", errno);

	tmp = oss->format.nChannels;

	if (ioctl(pcm_handle, SNDCTL_DSP_CHANNELS, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_CHANNELS failed", errno);

	tmp = oss->format.nSamplesPerSec;

	if (ioctl(pcm_handle, SNDCTL_DSP_SPEED, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SPEED failed", errno);

	tmp = oss->format.nBlockAlign;

	if (ioctl(pcm_handle, SNDCTL_DSP_SETFRAGMENT, &tmp) == -1)
		OSS_LOG_ERR("SNDCTL_DSP_SETFRAGMENT failed", errno);

	buffer_size = (oss->FramesPerPacket * oss->format.nChannels * (oss->format.wBitsPerSample / 8));
	buffer = (BYTE*)calloc((buffer_size + sizeof(void*)), sizeof(BYTE));

	if (NULL == buffer)
	{
		OSS_LOG_ERR("malloc() fail", errno);
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto err_out;
	}

	freerdp_dsp_context_reset_adpcm(oss->dsp_context);

	while (1)
	{
		status = WaitForSingleObject(oss->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", error);
			goto err_out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		tmp = read(pcm_handle, buffer, buffer_size);

		/* Error happen. */
		if (tmp < 0)
		{
			OSS_LOG_ERR("read() error", errno);
			continue;
		}

		if (tmp < buffer_size) /* Not enouth data. */
			continue;

		/* Process. */
		switch (oss->format.wFormatTag) {
		case WAVE_FORMAT_ADPCM:
			if (!oss->dsp_context->encode_ms_adpcm(oss->dsp_context,
			    buffer, buffer_size, oss->format.nChannels, oss->format.nBlockAlign))
			{
				error = ERROR_INTERNAL_ERROR;
				goto err_out;
			}
			encoded_data = oss->dsp_context->adpcm_buffer;
			encoded_size = oss->dsp_context->adpcm_size;
			break;
		case WAVE_FORMAT_DVI_ADPCM:
			if (!oss->dsp_context->encode_ima_adpcm(oss->dsp_context,
			    buffer, buffer_size, oss->format.nChannels, oss->format.nBlockAlign))
			{
				error = ERROR_INTERNAL_ERROR;
				goto err_out;
			}
			encoded_data = oss->dsp_context->adpcm_buffer;
			encoded_size = oss->dsp_context->adpcm_size;
			break;
		default:
			encoded_data = buffer;
			encoded_size = buffer_size;
			break;
		}
		if ((error = oss->receive(encoded_data, encoded_size, oss->user_data)))
		{
			WLog_ERR(TAG, "oss->receive failed with error %lu", error);
			break;
		}

	}

err_out:

	if (error && oss->rdpcontext)
		setChannelError(oss->rdpcontext, error, "audin_oss_thread_func reported an error");

	if (pcm_handle != -1)
	{
		WLog_INFO(TAG, "close: %s", dev_name);
		close(pcm_handle);
	}

	free(buffer);
	ExitThread(0);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_oss_open(IAudinDevice *device, AudinReceive receive, void *user_data) {
	AudinOSSDevice *oss = (AudinOSSDevice*)device;

	oss->receive = receive;
	oss->user_data = user_data;

	if (!(oss->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}
	if (!(oss->thread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)audin_oss_thread_func, oss, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(oss->stopEvent);
		oss->stopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_oss_close(IAudinDevice *device)
{
	UINT error;
	AudinOSSDevice *oss = (AudinOSSDevice*)device;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if (oss->stopEvent != NULL)
	{
		SetEvent(oss->stopEvent);
		if (WaitForSingleObject(oss->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", error);
			return error;
		}
		CloseHandle(oss->stopEvent);
		oss->stopEvent = NULL;
		CloseHandle(oss->thread);
		oss->thread = NULL;
	}

	oss->receive = NULL;
	oss->user_data = NULL;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_oss_free(IAudinDevice* device)
{
	AudinOSSDevice *oss = (AudinOSSDevice*)device;

	int error;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if ((error = audin_oss_close(device)))
	{
		WLog_ERR(TAG, "audin_oss_close failed with error code %d!", error);
	}
	freerdp_dsp_context_free(oss->dsp_context);
	free(oss);

	return CHANNEL_RC_OK;
}

COMMAND_LINE_ARGUMENT_A audin_oss_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_oss_parse_addin_args(AudinOSSDevice *device, ADDIN_ARGV *args)
{
	int status;
	char* str_num, *eptr;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinOSSDevice* oss = (AudinOSSDevice*)device;
	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, (const char**)args->argv, audin_oss_args, flags, oss, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_PARAMETER;

	arg = audin_oss_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)
		CommandLineSwitchCase(arg, "dev")
		{
			str_num = _strdup(arg->Value);
			if (!str_num)
			{
				WLog_ERR(TAG, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
			oss->dev_unit = strtol(str_num, &eptr, 10);

			if (oss->dev_unit < 0 || *eptr != '\0')
				oss->dev_unit = -1;

			free(str_num);
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
	return CHANNEL_RC_OK;
}

#ifdef STATIC_CHANNELS
#define freerdp_audin_client_subsystem_entry	oss_freerdp_audin_client_subsystem_entry
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
	ADDIN_ARGV *args;
	AudinOSSDevice *oss;
	UINT error;

	oss = (AudinOSSDevice*)calloc(1, sizeof(AudinOSSDevice));
	if (!oss)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	oss->iface.Open = audin_oss_open;
	oss->iface.FormatSupported = audin_oss_format_supported;
	oss->iface.SetFormat = audin_oss_set_format;
	oss->iface.Close = audin_oss_close;
	oss->iface.Free = audin_oss_free;
	oss->rdpcontext = pEntryPoints->rdpcontext;

	oss->dev_unit = -1;
	args = pEntryPoints->args;

	if ((error = audin_oss_parse_addin_args(oss, args)))
	{
		WLog_ERR(TAG, "audin_oss_parse_addin_args failed with errorcode %lu!", error);
		goto error_out;
	}

	oss->dsp_context = freerdp_dsp_context_new();
	if (!oss->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) oss)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %lu!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	freerdp_dsp_context_free(oss->dsp_context);
	free(oss);
	return error;

}
