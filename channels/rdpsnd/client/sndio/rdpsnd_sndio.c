/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <freerdp/types.h>

#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;

	struct sio_hdl* hdl;
	struct sio_par par;
} rdpsndSndioPlugin;

static BOOL rdpsnd_sndio_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;

	if (device == NULL || format == NULL)
		return FALSE;

	if (sndio->hdl != NULL)
		return TRUE;

	sndio->hdl = sio_open(SIO_DEVANY, SIO_PLAY, 0);
	if (sndio->hdl == NULL)
	{
		WLog_ERR(TAG, "could not open audio device");
		return FALSE;
	}

	sio_initpar(&sndio->par);
	sndio->par.bits = format->wBitsPerSample;
	sndio->par.pchan = format->nChannels;
	sndio->par.rate = format->nSamplesPerSec;
	if (!sio_setpar(sndio->hdl, &sndio->par))
	{
		WLog_ERR(TAG, "could not set audio parameters");
		return FALSE;
	}
	if (!sio_getpar(sndio->hdl, &sndio->par))
	{
		WLog_ERR(TAG, "could not get audio parameters");
		return FALSE;
	}

	if (!sio_start(sndio->hdl))
	{
		WLog_ERR(TAG, "could not start audio device");
		return FALSE;
	}

	return TRUE;
}

static void rdpsnd_sndio_close(rdpsndDevicePlugin* device)
{
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;

	if (device == NULL)
		return;

	if (sndio->hdl != NULL)
	{
		sio_stop(sndio->hdl);
		sio_close(sndio->hdl);
		sndio->hdl = NULL;
	}
}

static BOOL rdpsnd_sndio_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;

	if (device == NULL || sndio->hdl == NULL)
		return FALSE;

	/*
	 * Low-order word contains the left-channel volume setting.
	 * We ignore the right-channel volume setting in the high-order word.
	 */
	return sio_setvol(sndio->hdl, ((value & 0xFFFF) * SIO_MAXVOL) / 0xFFFF);
}

static void rdpsnd_sndio_free(rdpsndDevicePlugin* device)
{
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;

	if (device == NULL)
		return;

	rdpsnd_sndio_close(device);
	free(sndio);
}

static BOOL rdpsnd_sndio_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
{
	if (format == NULL)
		return FALSE;

	return (format->wFormatTag == WAVE_FORMAT_PCM);
}

static void rdpsnd_sndio_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;

	if (device == NULL || sndio->hdl == NULL)
		return;

	sio_write(sndio->hdl, data, size);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_sndio_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsndSndioPlugin* sndio = (rdpsndSndioPlugin*)device;
	COMMAND_LINE_ARGUMENT_A rdpsnd_sndio_args[] = { { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, (const char**)args->argv, rdpsnd_sndio_args,
	                                    flags, sndio, NULL, NULL);

	if (status < 0)
		return ERROR_INVALID_DATA;

	arg = rdpsnd_sndio_args;

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
UINT sndio_freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndSndioPlugin* sndio;
	UINT ret = CHANNEL_RC_OK;
	sndio = (rdpsndSndioPlugin*)calloc(1, sizeof(rdpsndSndioPlugin));

	if (sndio == NULL)
		return CHANNEL_RC_NO_MEMORY;

	sndio->device.Open = rdpsnd_sndio_open;
	sndio->device.FormatSupported = rdpsnd_sndio_format_supported;
	sndio->device.SetVolume = rdpsnd_sndio_set_volume;
	sndio->device.Play = rdpsnd_sndio_play;
	sndio->device.Close = rdpsnd_sndio_close;
	sndio->device.Free = rdpsnd_sndio_free;
	args = pEntryPoints->args;

	if (args->argc > 1)
	{
		ret = rdpsnd_sndio_parse_addin_args((rdpsndDevicePlugin*)sndio, args);

		if (ret != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "error parsing arguments");
			goto error;
		}
	}

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, &sndio->device);
	return ret;
error:
	rdpsnd_sndio_free(&sndio->device);
	return ret;
}
