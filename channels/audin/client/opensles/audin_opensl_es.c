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
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/rdpsnd.h>

#include <SLES/OpenSLES.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"
#include "opensl_io.h"

typedef struct _AudinOpenSLESDevice
{
	IAudinDevice iface;

	char* device_name;
	OPENSL_STREAM *stream;

	UINT32 frames_per_packet;
	UINT32 rate;
	UINT32 channels;

	UINT32 bytes_per_channel;

	UINT32 format;
	UINT32 block_size;

	FREERDP_DSP_CONTEXT* dsp_context;
	AudinReceive receive;

	HANDLE thread;
	HANDLE stopEvent;

	void* user_data;

	rdpContext* rdpcontext;
} AudinOpenSLESDevice;

static void* audin_opensles_thread_func(void* arg)
{
	union
	{
		void *v;
		short* s;
		BYTE *b;
	} buffer;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) arg;
	const size_t raw_size = opensles->frames_per_packet * opensles->bytes_per_channel;
	int rc = CHANNEL_RC_OK;
	UINT error = CHANNEL_RC_OK;
    DWORD status;

	DEBUG_DVC("opensles=%p", opensles);
	
	assert(opensles);
	assert(opensles->frames_per_packet > 0);
	assert(opensles->dsp_context);
	assert(opensles->stopEvent);
	assert(opensles->stream);

	buffer.v = calloc(1, raw_size);
	if (!buffer.v)
	{
        error = CHANNEL_RC_NO_MEMORY;
		WLog_ERR(TAG, "calloc failed!");
		if (opensles->rdpcontext)
			setChannelError(opensles->rdpcontext, CHANNEL_RC_NO_MEMORY, "audin_opensles_thread_func reported an error");
        goto out;
	}

	freerdp_dsp_context_reset_adpcm(opensles->dsp_context);

	while (1)
	{

        status = WaitForSingleObject(opensles->stopEvent, 0);

        if (status == WAIT_FAILED)
        {
            error = GetLastError();
            WLog_ERR(TAG, "WaitForSingleObject failed with error %lu!", error);
            break;
        }

        if (status == WAIT_OBJECT_0)
            break;

        size_t encoded_size;
		void *encoded_data;

		rc = android_RecIn(opensles->stream, buffer.s, raw_size);
		if (rc < 0)
		{
			WLog_ERR(TAG, "android_RecIn %lu", rc);
			continue;
		}

		assert(rc == raw_size);
		if (opensles->format == WAVE_FORMAT_ADPCM)
		{
			if (!opensles->dsp_context->encode_ms_adpcm(opensles->dsp_context,
				buffer.b, rc, opensles->channels, opensles->block_size))
			{
				error = ERROR_INTERNAL_ERROR;
				break;
			}

			encoded_data = opensles->dsp_context->adpcm_buffer;
			encoded_size = opensles->dsp_context->adpcm_size;
		}
		else if (opensles->format == WAVE_FORMAT_DVI_ADPCM)
		{
			if (!opensles->dsp_context->encode_ima_adpcm(opensles->dsp_context,
				buffer.b, rc,
				opensles->channels, opensles->block_size))
			{
				error = ERROR_INTERNAL_ERROR;
				break;
			}

			encoded_data = opensles->dsp_context->adpcm_buffer;
			encoded_size = opensles->dsp_context->adpcm_size;
		}
		else
		{
			encoded_data = buffer.v;
			encoded_size = rc;
		}

		error = opensles->receive(encoded_data, encoded_size, opensles->user_data);
		if (error)
			break;
	}

	free(buffer.v);
out:
	DEBUG_DVC("thread shutdown.");

	if (error && opensles->rdpcontext)
		setChannelError(opensles->rdpcontext, error, "audin_opensles_thread_func reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_free(IAudinDevice* device)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p", device);

	/* The function may have been called out of order,
	 * ignore duplicate requests. */
	if (!opensles)
		return CHANNEL_RC_OK;

	assert(opensles);
	assert(opensles->dsp_context);
	assert(!opensles->stream);

	freerdp_dsp_context_free(opensles->dsp_context);

	free(opensles->device_name);

	free(opensles);

	return CHANNEL_RC_OK;
}

static BOOL audin_opensles_format_supported(IAudinDevice* device, audinFormat* format)
{
#ifdef WITH_DEBUG_DVC
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;
#endif
	
	DEBUG_DVC("device=%p, format=%p", opensles, format);

	assert(format);

	switch (format->wFormatTag)
	{
		case WAVE_FORMAT_PCM: /* PCM */
			if (format->cbSize == 0 &&
				(format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 8 || format->wBitsPerSample == 16) &&
				(format->nChannels >= 1 && format->nChannels <= 2))
			{
				return TRUE;
			}
			break;
			/* TODO: Deactivated format, does not work, find out why */
//		case WAVE_FORMAT_ADPCM: /* IMA ADPCM */
		case WAVE_FORMAT_DVI_ADPCM: 
			if ((format->nSamplesPerSec <= 48000) &&
				(format->wBitsPerSample == 4) &&
				(format->nChannels == 1 || format->nChannels == 2))
			{
				return TRUE;
			}
			break;
		default:
			DEBUG_DVC("Encoding '%s' [%08X] not supported",
				rdpsnd_get_audio_tag_string(format->wFormatTag),
				format->wFormatTag); 
			break;
	}

	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_set_format(IAudinDevice* device,
		audinFormat* format, UINT32 FramesPerPacket)
{
	int bs;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, format=%p, FramesPerPacket=%d",
			device, format, FramesPerPacket);

	assert(format);

	/* The function may have been called out of order, ignore
	 * requests before the device is available. */
	if (!opensles)
		return CHANNEL_RC_OK;

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
			}
			break;
		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			opensles->bytes_per_channel = 2;
			bs = (format->nBlockAlign - 4 * format->nChannels) * 4;
	
			opensles->frames_per_packet =
				(FramesPerPacket * format->nChannels * 2 /
				bs + 1) * bs / (format->nChannels * 2);
			break;
		case WAVE_FORMAT_ALAW:
		case WAVE_FORMAT_MULAW:
			opensles->frames_per_packet = FramesPerPacket;
			break;

		default:
			WLog_ERR(TAG, "Encoding '%d' [%08X] not supported",
					 (format->wFormatTag),
					 format->wFormatTag);
			return ERROR_UNSUPPORTED_TYPE;
	}

	opensles->rate = format->nSamplesPerSec;
	opensles->channels = format->nChannels;

	opensles->format = format->wFormatTag;
	opensles->block_size = format->nBlockAlign;

	DEBUG_DVC("aligned frames_per_packet=%d, block_size=%d",
			opensles->frames_per_packet, opensles->block_size);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_open(IAudinDevice* device, AudinReceive receive,
		void* user_data)
{
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, receive=%d, user_data=%p", device, receive, user_data);

	assert(opensles);

	/* The function may have been called out of order,
	 * ignore duplicate open requests. */
	if(opensles->stream)
		return CHANNEL_RC_OK;

	if(!(opensles->stream = android_OpenRecDevice(
			opensles->device_name,
			opensles->rate,
			opensles->channels,
			opensles->frames_per_packet,
			opensles->bytes_per_channel * 8)))
	{
		WLog_ERR(TAG, "android_OpenRecDevice failed!");
		return ERROR_INTERNAL_ERROR;
	}

	opensles->receive = receive;
	opensles->user_data = user_data;

	if (!(opensles->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto error_out;
	}
	if (!(opensles->thread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE) audin_opensles_thread_func,
		opensles, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto error_out;
	}
	return CHANNEL_RC_OK;
error_out:
	android_CloseRecDevice(opensles->stream);
	opensles->stream = NULL;
	CloseHandle(opensles->stopEvent);
	opensles->stopEvent = NULL;
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_close(IAudinDevice* device)
{
    UINT error;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p", device);
	
	assert(opensles);

	/* The function may have been called out of order,
	 * ignore duplicate requests. */
	if (!opensles->stopEvent)
	{
		WLog_ERR(TAG, "[ERROR] function called without matching open.");
		return ERROR_REQUEST_OUT_OF_SEQUENCE;
	}

	assert(opensles->stopEvent);
	assert(opensles->thread);
	assert(opensles->stream);

	SetEvent(opensles->stopEvent);
	if (WaitForSingleObject(opensles->thread, INFINITE) == WAIT_FAILED)
    {
        error = GetLastError();
        WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", error);
        return error;
    }
	CloseHandle(opensles->stopEvent);
	CloseHandle(opensles->thread);

	android_CloseRecDevice(opensles->stream);

	opensles->stopEvent = NULL;
	opensles->thread = NULL;
	opensles->receive = NULL;
	opensles->user_data = NULL;
	opensles->stream = NULL;

	return CHANNEL_RC_OK;
}

static COMMAND_LINE_ARGUMENT_A audin_opensles_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_opensles_parse_addin_args(AudinOpenSLESDevice* device,
		ADDIN_ARGV* args)
{
	UINT status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinOpenSLESDevice* opensles = (AudinOpenSLESDevice*) device;

	DEBUG_DVC("device=%p, args=%p", device, args);

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			audin_opensles_args, flags, opensles, NULL, NULL);
	if (status < 0)
		return status;

	arg = audin_opensles_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			opensles->device_name = _strdup(arg->Value);
			if (!opensles->device_name)
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
#define freerdp_audin_client_subsystem_entry \
	opensles_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry \
	FREERDP_API freerdp_audin_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_audin_client_subsystem_entry(
		PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinOpenSLESDevice* opensles;
	UINT error;

	DEBUG_DVC("pEntryPoints=%p", pEntryPoints);

	opensles = (AudinOpenSLESDevice*) calloc(1, sizeof(AudinOpenSLESDevice));
	if (!opensles)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	opensles->iface.Open = audin_opensles_open;
	opensles->iface.FormatSupported = audin_opensles_format_supported;
	opensles->iface.SetFormat = audin_opensles_set_format;
	opensles->iface.Close = audin_opensles_close;
	opensles->iface.Free = audin_opensles_free;
	opensles->rdpcontext = pEntryPoints->rdpcontext;

	args = pEntryPoints->args;

	if ((error = audin_opensles_parse_addin_args(opensles, args)))
	{
		WLog_ERR(TAG, "audin_opensles_parse_addin_args failed with errorcode %d!", error);
		goto error_out;
	}

	opensles->dsp_context = freerdp_dsp_context_new();
	if (!opensles->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) opensles)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %d!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	freerdp_dsp_context_free(opensles->dsp_context);
	free(opensles);
	return error;
}
