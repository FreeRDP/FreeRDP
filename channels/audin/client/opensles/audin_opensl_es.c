/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - OpenSL ES implementation
 *
 * Copyright 2013 Armin Novak <armin.novak@gmail.com>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/channels/rdpsnd.h>

#include <SLES/OpenSLES.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"
#include "opensl_io.h"

typedef struct _AudinOpenSLESDevice
{
	IAudinDevice iface;

	char* device_name;
	OPENSL_STREAM* stream;

	AUDIO_FORMAT format;
	UINT32 frames_per_packet;

	UINT32 bytes_per_channel;

	AudinReceive receive;

	void* user_data;

	rdpContext* rdpcontext;
	wLog* log;
} AudinOpenSLESDevice;

static UINT audin_opensles_close(IAudinDevice* device);

static void audin_receive(void* context, const void* data, size_t size)
{
	UINT error;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)context;

	if (!opensles || !data)
	{
		WLog_ERR(TAG, "[%s] Invalid arguments context=%p, data=%p", __FUNCTION__, opensles, data);
		return;
	}

	error = opensles->receive(&opensles->format, data, size, opensles->user_data);

	if (error && opensles->rdpcontext)
		setChannelError(opensles->rdpcontext, error, "audin_receive reported an error");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_free(IAudinDevice* device)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;

	if (!opensles)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p", (void*)device);

	free(opensles->device_name);
	free(opensles);
	return CHANNEL_RC_OK;
}

static BOOL audin_opensles_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;

	if (!opensles || !format)
		return FALSE;

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p, format=%p", (void*)opensles, (void*)format);
	assert(format);

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM: /* PCM */
			if (format->cbSize == 0 && (format->nSamplesPerSec <= 48000) &&
			    (format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
			    (format->nChannels >= 1 && format->nChannels <= 2))
			{
				return TRUE;
			}

			break;

		default:
			WLog_Print(opensles->log, WLOG_DEBUG, "Encoding '%s' [0x%04X" PRIX16 "] not supported",
			           audio_format_get_tag_string(format->wFormatTag), format->wFormatTag);
			break;
	}

	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                      UINT32 FramesPerPacket)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;

	if (!opensles || !format)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p, format=%p, FramesPerPacket=%" PRIu32 "",
	           (void*)device, (void*)format, FramesPerPacket);
	assert(format);

	opensles->format = *format;

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			opensles->frames_per_packet = FramesPerPacket;

			switch (format->wBitsPerSample)
			{
				case 4:
					opensles->bytes_per_channel = 1;
					break;

				case 8:
					opensles->bytes_per_channel = 1;
					break;

				case 16:
					opensles->bytes_per_channel = 2;
					break;

				default:
					return ERROR_UNSUPPORTED_TYPE;
			}

			break;

		default:
			WLog_Print(opensles->log, WLOG_ERROR,
			           "Encoding '%" PRIu16 "' [%04" PRIX16 "] not supported", format->wFormatTag,
			           format->wFormatTag);
			return ERROR_UNSUPPORTED_TYPE;
	}

	WLog_Print(opensles->log, WLOG_DEBUG, "frames_per_packet=%" PRIu32,
	           opensles->frames_per_packet);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;

	if (!opensles || !receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p, receive=%p, user_data=%p", (void*)device,
	           (void*)receive, (void*)user_data);

	if (opensles->stream)
		goto error_out;

	if (!(opensles->stream = android_OpenRecDevice(
	          opensles, audin_receive, opensles->format.nSamplesPerSec, opensles->format.nChannels,
	          opensles->frames_per_packet, opensles->format.wBitsPerSample)))
	{
		WLog_Print(opensles->log, WLOG_ERROR, "android_OpenRecDevice failed!");
		goto error_out;
	}

	opensles->receive = receive;
	opensles->user_data = user_data;
	return CHANNEL_RC_OK;
error_out:
	audin_opensles_close(opensles);
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT audin_opensles_close(IAudinDevice* device)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;

	if (!opensles)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p", (void*)device);
	android_CloseRecDevice(opensles->stream);
	opensles->receive = NULL;
	opensles->user_data = NULL;
	opensles->stream = NULL;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_parse_addin_args(AudinOpenSLESDevice* device, ADDIN_ARGV* args)
{
	UINT status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*)device;
	COMMAND_LINE_ARGUMENT_A audin_opensles_args[] = {
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL,
		  "audio device name" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	WLog_Print(opensles->log, WLOG_DEBUG, "device=%p, args=%p", (void*)device, (void*)args);
	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, audin_opensles_args, flags,
	                                    opensles, NULL, NULL);

	if (status < 0)
		return status;

	arg = audin_opensles_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			opensles->device_name = _strdup(arg->Value);

			if (!opensles->device_name)
			{
				WLog_Print(opensles->log, WLOG_ERROR, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_audin_client_subsystem_entry opensles_freerdp_audin_client_subsystem_entry
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
	AudinOpenSLESDevice* opensles;
	UINT error;
	opensles = (AudinOpenSLESDevice*)calloc(1, sizeof(AudinOpenSLESDevice));

	if (!opensles)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	opensles->log = WLog_Get(TAG);
	opensles->iface.Open = audin_opensles_open;
	opensles->iface.FormatSupported = audin_opensles_format_supported;
	opensles->iface.SetFormat = audin_opensles_set_format;
	opensles->iface.Close = audin_opensles_close;
	opensles->iface.Free = audin_opensles_free;
	opensles->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if ((error = audin_opensles_parse_addin_args(opensles, args)))
	{
		WLog_Print(opensles->log, WLOG_ERROR,
		           "audin_opensles_parse_addin_args failed with errorcode %" PRIu32 "!", error);
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*)opensles)))
	{
		WLog_Print(opensles->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(opensles);
	return error;
}
