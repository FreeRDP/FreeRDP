/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <freerdp/types.h>

#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;
} rdpsndFakePlugin;

static BOOL rdpsnd_fake_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format, UINT32 latency)
{
	return TRUE;
}

static void rdpsnd_fake_close(rdpsndDevicePlugin* device)
{
}

static BOOL rdpsnd_fake_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	return TRUE;
}

static void rdpsnd_fake_free(rdpsndDevicePlugin* device)
{
	rdpsndFakePlugin* fake = (rdpsndFakePlugin*)device;

	if (!fake)
		return;

	free(fake);
}

static BOOL rdpsnd_fake_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	return TRUE;
}

static UINT rdpsnd_fake_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_fake_parse_addin_args(rdpsndFakePlugin* fake, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	COMMAND_LINE_ARGUMENT_A rdpsnd_fake_args[] = { { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, rdpsnd_fake_args, flags, fake, NULL,
	                                    NULL);

	if (status < 0)
		return ERROR_INVALID_DATA;

	arg = rdpsnd_fake_args;

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
UINT fake_freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	rdpsndFakePlugin* fake;
	UINT ret = CHANNEL_RC_OK;
	fake = (rdpsndFakePlugin*)calloc(1, sizeof(rdpsndFakePlugin));

	if (!fake)
		return CHANNEL_RC_NO_MEMORY;

	fake->device.Open = rdpsnd_fake_open;
	fake->device.FormatSupported = rdpsnd_fake_format_supported;
	fake->device.SetVolume = rdpsnd_fake_set_volume;
	fake->device.Play = rdpsnd_fake_play;
	fake->device.Close = rdpsnd_fake_close;
	fake->device.Free = rdpsnd_fake_free;
	args = pEntryPoints->args;

	if (args->argc > 1)
	{
		ret = rdpsnd_fake_parse_addin_args(fake, args);

		if (ret != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "error parsing arguments");
			goto error;
		}
	}

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, &fake->device);
	return ret;
error:
	rdpsnd_fake_free(&fake->device);
	return ret;
}
