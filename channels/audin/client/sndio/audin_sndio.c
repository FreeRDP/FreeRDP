/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - sndio implementation
 *
 * Copyright (c) 2015 Rozhuk Ivan <rozhuk.im@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2020 Ingo Feinerer <feinerer@logic.at>
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

#include <sndio.h>

#include <winpr/cmdline.h>

#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

typedef struct
{
	IAudinDevice device;

	HANDLE thread;
	HANDLE stopEvent;

	AUDIO_FORMAT format;
	UINT32 FramesPerPacket;

	AudinReceive receive;
	void* user_data;

	rdpContext* rdpcontext;
} AudinSndioDevice;

static BOOL audin_sndio_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	if (device == NULL || format == NULL)
		return FALSE;

	return (format->wFormatTag == WAVE_FORMAT_PCM);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_sndio_set_format(IAudinDevice* device, AUDIO_FORMAT* format,
                                   UINT32 FramesPerPacket)
{
	AudinSndioDevice* sndio = (AudinSndioDevice*)device;

	if (device == NULL || format == NULL)
		return ERROR_INVALID_PARAMETER;

	if (format->wFormatTag != WAVE_FORMAT_PCM)
		return ERROR_INTERNAL_ERROR;

	sndio->format = *format;
	sndio->FramesPerPacket = FramesPerPacket;

	return CHANNEL_RC_OK;
}

static void* audin_sndio_thread_func(void* arg)
{
	struct sio_hdl* hdl;
	struct sio_par par;
	BYTE* buffer = NULL;
	size_t n, nbytes;
	AudinSndioDevice* sndio = (AudinSndioDevice*)arg;
	UINT error = 0;
	DWORD status;

	if (arg == NULL)
	{
		error = ERROR_INVALID_PARAMETER;
		goto err_out;
	}

	hdl = sio_open(SIO_DEVANY, SIO_REC, 0);
	if (hdl == NULL)
	{
		WLog_ERR(TAG, "could not open audio device");
		error = ERROR_INTERNAL_ERROR;
		goto err_out;
	}

	sio_initpar(&par);
	par.bits = sndio->format.wBitsPerSample;
	par.rchan = sndio->format.nChannels;
	par.rate = sndio->format.nSamplesPerSec;
	if (!sio_setpar(hdl, &par))
	{
		WLog_ERR(TAG, "could not set audio parameters");
		error = ERROR_INTERNAL_ERROR;
		goto err_out;
	}
	if (!sio_getpar(hdl, &par))
	{
		WLog_ERR(TAG, "could not get audio parameters");
		error = ERROR_INTERNAL_ERROR;
		goto err_out;
	}

	if (!sio_start(hdl))
	{
		WLog_ERR(TAG, "could not start audio device");
		error = ERROR_INTERNAL_ERROR;
		goto err_out;
	}

	nbytes =
	    (sndio->FramesPerPacket * sndio->format.nChannels * (sndio->format.wBitsPerSample / 8));
	buffer = (BYTE*)calloc((nbytes + sizeof(void*)), sizeof(BYTE));

	if (buffer == NULL)
	{
		error = ERROR_NOT_ENOUGH_MEMORY;
		goto err_out;
	}

	while (1)
	{
		status = WaitForSingleObject(sndio->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto err_out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		n = sio_read(hdl, buffer, nbytes);

		if (n == 0)
		{
			WLog_ERR(TAG, "could not read");
			continue;
		}

		if (n < nbytes)
			continue;

		if ((error = sndio->receive(&sndio->format, buffer, nbytes, sndio->user_data)))
		{
			WLog_ERR(TAG, "sndio->receive failed with error %" PRIu32 "", error);
			break;
		}
	}

err_out:
	if (error && sndio->rdpcontext)
		setChannelError(sndio->rdpcontext, error, "audin_sndio_thread_func reported an error");

	if (hdl != NULL)
	{
		WLog_INFO(TAG, "sio_close");
		sio_stop(hdl);
		sio_close(hdl);
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
static UINT audin_sndio_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinSndioDevice* sndio = (AudinSndioDevice*)device;
	sndio->receive = receive;
	sndio->user_data = user_data;

	if (!(sndio->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(sndio->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)audin_sndio_thread_func,
	                                   sndio, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed");
		CloseHandle(sndio->stopEvent);
		sndio->stopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_sndio_close(IAudinDevice* device)
{
	UINT error;
	AudinSndioDevice* sndio = (AudinSndioDevice*)device;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if (sndio->stopEvent != NULL)
	{
		SetEvent(sndio->stopEvent);

		if (WaitForSingleObject(sndio->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(sndio->stopEvent);
		sndio->stopEvent = NULL;
		CloseHandle(sndio->thread);
		sndio->thread = NULL;
	}

	sndio->receive = NULL;
	sndio->user_data = NULL;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_sndio_free(IAudinDevice* device)
{
	AudinSndioDevice* sndio = (AudinSndioDevice*)device;
	int error;

	if (device == NULL)
		return ERROR_INVALID_PARAMETER;

	if ((error = audin_sndio_close(device)))
	{
		WLog_ERR(TAG, "audin_sndio_close failed with error code %d", error);
	}

	free(sndio);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_sndio_parse_addin_args(AudinSndioDevice* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinSndioDevice* sndio = (AudinSndioDevice*)device;
	COMMAND_LINE_ARGUMENT_A audin_sndio_args[] = { { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, (const char**)args->argv, audin_sndio_args,
	                                    flags, sndio, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_PARAMETER;

	arg = audin_sndio_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT sndio_freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinSndioDevice* sndio;
	UINT ret = CHANNEL_RC_OK;
	sndio = (AudinSndioDevice*)calloc(1, sizeof(AudinSndioDevice));

	if (sndio == NULL)
		return CHANNEL_RC_NO_MEMORY;

	sndio->device.Open = audin_sndio_open;
	sndio->device.FormatSupported = audin_sndio_format_supported;
	sndio->device.SetFormat = audin_sndio_set_format;
	sndio->device.Close = audin_sndio_close;
	sndio->device.Free = audin_sndio_free;
	sndio->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if (args->argc > 1)
	{
		ret = audin_sndio_parse_addin_args(sndio, args);

		if (ret != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "error parsing arguments");
			goto error;
		}
	}

	if ((ret = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*)sndio)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %" PRIu32 "", ret);
		goto error;
	}

	return ret;
error:
	audin_sndio_free(&sndio->device);
	return ret;
}
