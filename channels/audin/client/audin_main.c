/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
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

#include <winpr/stream.h>
#include <freerdp/freerdp.h>
#include "audin_main.h"

#define MSG_SNDIN_VERSION       0x01
#define MSG_SNDIN_FORMATS       0x02
#define MSG_SNDIN_OPEN          0x03
#define MSG_SNDIN_OPEN_REPLY    0x04
#define MSG_SNDIN_DATA_INCOMING 0x05
#define MSG_SNDIN_DATA          0x06
#define MSG_SNDIN_FORMATCHANGE  0x07

typedef struct _AUDIN_LISTENER_CALLBACK AUDIN_LISTENER_CALLBACK;
struct _AUDIN_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

typedef struct _AUDIN_CHANNEL_CALLBACK AUDIN_CHANNEL_CALLBACK;
struct _AUDIN_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;

	/**
	 * The supported format list sent back to the server, which needs to
	 * be stored as reference when the server sends the format index in
	 * Open PDU and Format Change PDU
	 */
	audinFormat* formats;
	int formats_count;
};

typedef struct _AUDIN_PLUGIN AUDIN_PLUGIN;
struct _AUDIN_PLUGIN
{
	IWTSPlugin iface;

	AUDIN_LISTENER_CALLBACK* listener_callback;

	/* Parsed plugin data */
	UINT16 fixed_format;
	UINT16 fixed_channel;
	UINT32 fixed_rate;
	char* subsystem;
	char* device_name;

	/* Device interface */
	IAudinDevice* device;

	rdpContext* rdpcontext;
};

static BOOL audin_process_addin_args(AUDIN_PLUGIN* audin, ADDIN_ARGV* args);

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_version(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	UINT error;
	wStream* out;
	UINT32 Version;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	Stream_Read_UINT32(s, Version);

	DEBUG_DVC("Version=%d", Version);

	out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_OUTOFMEMORY;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_VERSION);
	Stream_Write_UINT32(out, Version);
	error = callback->channel->Write(callback->channel, (UINT32) Stream_GetPosition(out), Stream_Buffer(out), NULL);
	Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_send_incoming_data_pdu(IWTSVirtualChannelCallback* pChannelCallback)
{
	BYTE out_data[1];
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out_data[0] = MSG_SNDIN_DATA_INCOMING;
	return callback->channel->Write(callback->channel, 1, out_data, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_formats(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;
	UINT32 i;
	BYTE* fm;
	UINT error;
	wStream* out;
	UINT32 NumFormats;
	audinFormat format;
	UINT32 cbSizeFormatsPacket;

	Stream_Read_UINT32(s, NumFormats);
	DEBUG_DVC("NumFormats %d", NumFormats);
	if ((NumFormats < 1) || (NumFormats > 1000))
	{
		WLog_ERR(TAG, "bad NumFormats %d", NumFormats);
		return ERROR_INVALID_DATA;
	}
	Stream_Seek_UINT32(s); /* cbSizeFormatsPacket */

	callback->formats = (audinFormat*) calloc(1, NumFormats * sizeof(audinFormat));
	if (!callback->formats)
	{
		WLog_ERR(TAG, "calloc failed!");
		return ERROR_INVALID_DATA;
	}

	out = Stream_New(NULL, 9);

	if (!out)
	{
		error = CHANNEL_RC_NO_MEMORY;
		WLog_ERR(TAG, "Stream_New failed!");
		goto out;
	}

	Stream_Seek(out, 9);

	/* SoundFormats (variable) */
	for (i = 0; i < NumFormats; i++)
	{
		Stream_GetPointer(s, fm);
		Stream_Read_UINT16(s, format.wFormatTag);
		Stream_Read_UINT16(s, format.nChannels);
		Stream_Read_UINT32(s, format.nSamplesPerSec);
		Stream_Seek_UINT32(s); /* nAvgBytesPerSec */
		Stream_Read_UINT16(s, format.nBlockAlign);
		Stream_Read_UINT16(s, format.wBitsPerSample);
		Stream_Read_UINT16(s, format.cbSize);
		format.data = Stream_Pointer(s);
		Stream_Seek(s, format.cbSize);

		DEBUG_DVC("wFormatTag=%d nChannels=%d nSamplesPerSec=%d "
			"nBlockAlign=%d wBitsPerSample=%d cbSize=%d",
			format.wFormatTag, format.nChannels, format.nSamplesPerSec,
			format.nBlockAlign, format.wBitsPerSample, format.cbSize);

		if (audin->fixed_format > 0 && audin->fixed_format != format.wFormatTag)
			continue;
		if (audin->fixed_channel > 0 && audin->fixed_channel != format.nChannels)
			continue;
		if (audin->fixed_rate > 0 && audin->fixed_rate != format.nSamplesPerSec)
			continue;
		if (audin->device && audin->device->FormatSupported(audin->device, &format))
		{
			DEBUG_DVC("format ok");

			/* Store the agreed format in the corresponding index */
			callback->formats[callback->formats_count++] = format;
			/* Put the format to output buffer */
			if (!Stream_EnsureRemainingCapacity(out, 18 + format.cbSize))
			{
				error = CHANNEL_RC_NO_MEMORY;
				WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
				goto out;
			}


			Stream_Write(out, fm, 18 + format.cbSize);
		}
	}

	if ((error = audin_send_incoming_data_pdu(pChannelCallback)))
	{
		WLog_ERR(TAG, "audin_send_incoming_data_pdu failed!");
		goto out;
	}

	cbSizeFormatsPacket = (UINT32) Stream_GetPosition(out);
	Stream_SetPosition(out, 0);

	Stream_Write_UINT8(out, MSG_SNDIN_FORMATS); /* Header (1 byte) */
	Stream_Write_UINT32(out, callback->formats_count); /* NumFormats (4 bytes) */
	Stream_Write_UINT32(out, cbSizeFormatsPacket); /* cbSizeFormatsPacket (4 bytes) */

	error = callback->channel->Write(callback->channel, cbSizeFormatsPacket, Stream_Buffer(out), NULL);
out:
	if (error != CHANNEL_RC_OK)
	{
		free(callback->formats);
		callback->formats = NULL;
	}
	Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_send_format_change_pdu(IWTSVirtualChannelCallback* pChannelCallback, UINT32 NewFormat)
{
	UINT error;
	wStream* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_OK;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_FORMATCHANGE);
	Stream_Write_UINT32(out, NewFormat);
	error = callback->channel->Write(callback->channel, 5, Stream_Buffer(out), NULL);
	Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_send_open_reply_pdu(IWTSVirtualChannelCallback* pChannelCallback, UINT32 Result)
{
	UINT error;
	wStream* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_OPEN_REPLY);
	Stream_Write_UINT32(out, Result);
	error = callback->channel->Write(callback->channel, 5, Stream_Buffer(out), NULL);
	Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_receive_wave_data(const BYTE* data, int size, void* user_data)
{
	UINT error;
	wStream* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) user_data;

	if ((error = audin_send_incoming_data_pdu((IWTSVirtualChannelCallback*) callback)))
	{
		WLog_ERR(TAG, "audin_send_incoming_data_pdu failed!");
		return error;
	}

	out = Stream_New(NULL, size + 1);

	if (!out)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_DATA);
	Stream_Write(out, data, size);
	error = callback->channel->Write(callback->channel, (UINT32) Stream_GetPosition(out), Stream_Buffer(out), NULL);
	Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_open(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;
	audinFormat* format;
	UINT32 initialFormat;
	UINT32 FramesPerPacket;
	UINT error = CHANNEL_RC_OK;

	Stream_Read_UINT32(s, FramesPerPacket);
	Stream_Read_UINT32(s, initialFormat);

	DEBUG_DVC("FramesPerPacket=%d initialFormat=%d",
		FramesPerPacket, initialFormat);

	if (initialFormat >= (UINT32) callback->formats_count)
	{
		WLog_ERR(TAG, "invalid format index %d (total %d)",
				 initialFormat, callback->formats_count);
		return ERROR_INVALID_DATA;
	}

	format = &callback->formats[initialFormat];
	if (audin->device)
	{
		IFCALLRET(audin->device->SetFormat, error, audin->device, format, FramesPerPacket);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "SetFormat failed with errorcode %lu", error);
			return error;
		}
		IFCALLRET(audin->device->Open, error, audin->device, audin_receive_wave_data, callback);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Open failed with errorcode %lu", error);
			return error;
		}
	}

	if ((error = audin_send_format_change_pdu(pChannelCallback, initialFormat)))
	{
		WLog_ERR(TAG, "audin_send_format_change_pdu failed!");
		return error;
	}

	if ((error = audin_send_open_reply_pdu(pChannelCallback, 0)))
		WLog_ERR(TAG, "audin_send_open_reply_pdu failed!");

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_format_change(IWTSVirtualChannelCallback* pChannelCallback, wStream* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN * audin = (AUDIN_PLUGIN*) callback->plugin;
	UINT32 NewFormat;
	audinFormat* format;
	UINT error = CHANNEL_RC_OK;

	Stream_Read_UINT32(s, NewFormat);

	DEBUG_DVC("NewFormat=%d", NewFormat);

	if (NewFormat >= (UINT32) callback->formats_count)
	{
		WLog_ERR(TAG, "invalid format index %d (total %d)",
				 NewFormat, callback->formats_count);
		return ERROR_INVALID_DATA;
	}

	format = &callback->formats[NewFormat];

	if (audin->device)
	{
		IFCALLRET(audin->device->Close, error, audin->device);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Close failed with errorcode %lu", error);
			return error;
		}
		IFCALLRET(audin->device->SetFormat, error, audin->device, format, 0);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "SetFormat failed with errorcode %lu", error);
			return error;
		}
		IFCALLRET(audin->device->Open, error, audin->device, audin_receive_wave_data, callback);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Open failed with errorcode %lu", error);
			return error;
		}
	}

	if ((error = audin_send_format_change_pdu(pChannelCallback, NewFormat)))
		WLog_ERR(TAG, "audin_send_format_change_pdu failed!");

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	UINT error;
	BYTE MessageId;

	Stream_Read_UINT8(data, MessageId);

	DEBUG_DVC("MessageId=0x%x", MessageId);

	switch (MessageId)
	{
		case MSG_SNDIN_VERSION:
			error = audin_process_version(pChannelCallback, data);
			break;

		case MSG_SNDIN_FORMATS:
			error = audin_process_formats(pChannelCallback, data);
			break;

		case MSG_SNDIN_OPEN:
			error = audin_process_open(pChannelCallback, data);
			break;

		case MSG_SNDIN_FORMATCHANGE:
			error = audin_process_format_change(pChannelCallback, data);
			break;

		default:
			WLog_ERR(TAG, "unknown MessageId=0x%x", MessageId);
			error = ERROR_INVALID_DATA;
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;
	UINT error = CHANNEL_RC_OK;

	DEBUG_DVC("...");

	if (audin->device)
	{
		IFCALLRET(audin->device->Close, error, audin->device);
		if (error != CHANNEL_RC_OK)
			WLog_ERR(TAG, "Close failed with errorcode %lu", error);
	}

	free(callback->formats);
	free(callback);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	AUDIN_CHANNEL_CALLBACK* callback;
	AUDIN_LISTENER_CALLBACK* listener_callback = (AUDIN_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("...");

	callback = (AUDIN_CHANNEL_CALLBACK*) calloc(1, sizeof(AUDIN_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = audin_on_data_received;
	callback->iface.OnClose = audin_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;

	DEBUG_DVC("...");

	audin->listener_callback = (AUDIN_LISTENER_CALLBACK*) calloc(1, sizeof(AUDIN_LISTENER_CALLBACK));
	if (!audin->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	audin->listener_callback->iface.OnNewChannelConnection = audin_on_new_channel_connection;
	audin->listener_callback->plugin = pPlugin;
	audin->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "AUDIO_INPUT", 0,
		(IWTSListenerCallback*) audin->listener_callback, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_plugin_terminated(IWTSPlugin* pPlugin)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;
	UINT error = CHANNEL_RC_OK;

	DEBUG_DVC("...");

	if (audin->device)
	{
		IFCALLRET(audin->device->Free, error, audin->device);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Free failed with errorcode %lu", error);
			// dont stop on error
		}
		audin->device = NULL;
	}

	free(audin->subsystem);
	audin->subsystem = NULL;

	free(audin->device_name);
	audin->device_name = NULL;

	free(audin->listener_callback);
	free(audin);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_register_device_plugin(IWTSPlugin* pPlugin, IAudinDevice* device)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;

	if (audin->device)
	{
		WLog_ERR(TAG, "existing device, abort.");
		return ERROR_ALREADY_EXISTS;
	}

	DEBUG_DVC("device registered.");

	audin->device = device;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_load_device_plugin(IWTSPlugin* pPlugin, const char* name, ADDIN_ARGV* args)
{
	PFREERDP_AUDIN_DEVICE_ENTRY entry;
	FREERDP_AUDIN_DEVICE_ENTRY_POINTS entryPoints;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;
	UINT error;

	if (!audin_process_addin_args(audin, args))
		return CHANNEL_RC_INITIALIZATION_ERROR;

	entry = (PFREERDP_AUDIN_DEVICE_ENTRY) freerdp_load_channel_addin_entry("audin", (LPSTR) name, NULL, 0);

	if (entry == NULL)
	{
		WLog_ERR(TAG, "freerdp_load_channel_addin_entry did not return any function pointers for %s ", name);
		return ERROR_INVALID_FUNCTION;
	}

	entryPoints.plugin = pPlugin;
	entryPoints.pRegisterAudinDevice = audin_register_device_plugin;
	entryPoints.args = args;
	entryPoints.rdpcontext = ((AUDIN_PLUGIN*)pPlugin)->rdpcontext;

	if ((error = entry(&entryPoints)))
	{
		WLog_ERR(TAG, "%s entry returned error %lu.", name, error);
		return error;
	}

	WLog_INFO(TAG, "Loaded %s backend for audin", name);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_set_subsystem(AUDIN_PLUGIN* audin, char* subsystem)
{
	free(audin->subsystem);
	audin->subsystem = _strdup(subsystem);
	if (!audin->subsystem)
	{
		WLog_ERR(TAG, "_strdup failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_set_device_name(AUDIN_PLUGIN* audin, char* device_name)
{
	free(audin->device_name);
	audin->device_name = _strdup(device_name);
	if (!audin->device_name)
	{
		WLog_ERR(TAG, "_strdup failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	return CHANNEL_RC_OK;
}

static COMMAND_LINE_ARGUMENT_A audin_args[] =
{
	{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
	{ "format", COMMAND_LINE_VALUE_REQUIRED, "<format>", NULL, NULL, -1, NULL, "format" },
	{ "rate", COMMAND_LINE_VALUE_REQUIRED, "<rate>", NULL, NULL, -1, NULL, "rate" },
	{ "channel", COMMAND_LINE_VALUE_REQUIRED, "<channel>", NULL, NULL, -1, NULL, "channel" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

BOOL audin_process_addin_args(AUDIN_PLUGIN* audin, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	UINT error;

	if (!args || args->argc == 1)
		return TRUE;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			audin_args, flags, audin, NULL, NULL);
	if (status != 0)
		return FALSE;

	arg = audin_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "sys")
		{
			if ((error = audin_set_subsystem(audin, arg->Value)))
			{
				WLog_ERR(TAG, "audin_set_subsystem failed with error %lu!", error);
				return FALSE;
			}
		}
		CommandLineSwitchCase(arg, "dev")
		{
			if ((error = audin_set_device_name(audin, arg->Value)))
			{
				WLog_ERR(TAG, "audin_set_device_name failed with error %lu!", error);
				return FALSE;
			}
		}
		CommandLineSwitchCase(arg, "format")
		{
			audin->fixed_format = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "rate")
		{
			audin->fixed_rate = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "channel")
		{
			audin->fixed_channel = atoi(arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return TRUE;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		audin_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	struct SubsystemEntry
	{
		char *subsystem;
		char *device;
	};

	UINT error = CHANNEL_RC_INITIALIZATION_ERROR;
	ADDIN_ARGV* args;
	AUDIN_PLUGIN* audin;
	struct SubsystemEntry entries[] =
	{
#if defined(WITH_PULSE)
		{"pulse", ""},
#endif
#if defined(WITH_OSS)
		{"oss", "default"},
#endif
#if defined(WITH_ALSA)
		{"alsa", "default"},
#endif
#if defined(WITH_OPENSLES)
		{"opensles", "default"},
#endif
#if defined(WITH_WINMM)
		{"winmm", "default"},
#endif
#if defined(WITH_MACAUDIO)
		{"mac", "default"},
#endif
		{NULL,NULL}
	};
	struct SubsystemEntry *entry = &entries[0];

	assert(pEntryPoints);
	assert(pEntryPoints->GetPlugin);

	audin = (AUDIN_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "audin");
	if (audin != NULL)
		return CHANNEL_RC_ALREADY_INITIALIZED;

	audin = (AUDIN_PLUGIN*) calloc(1, sizeof(AUDIN_PLUGIN));
	if (!audin)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	audin->iface.Initialize = audin_plugin_initialize;
	audin->iface.Connected = NULL;
	audin->iface.Disconnected = NULL;
	audin->iface.Terminated = audin_plugin_terminated;

	args = pEntryPoints->GetPluginData(pEntryPoints);
	audin->rdpcontext = ((freerdp*)((rdpSettings*) pEntryPoints->GetRdpSettings(pEntryPoints))->instance)->context;

	if (args)
	{
		if (!audin_process_addin_args(audin, args))
			goto out;
	}

	if (audin->subsystem)
	{
		if ((error = audin_load_device_plugin((IWTSPlugin*) audin, audin->subsystem, args)))
		{
			WLog_ERR(TAG, "audin_load_device_plugin %s failed with error %lu!",
					 audin->subsystem, error);
			goto out;
		}
	}
	else
	{
		while (entry && entry->subsystem && !audin->device)
		{
			if ((error = audin_set_subsystem(audin, entry->subsystem)))
			{
				WLog_ERR(TAG, "audin_set_subsystem for %s failed with error %lu!",
					 entry->subsystem, error);
			}
			else if ((error = audin_set_device_name(audin, entry->device)))
			{
				WLog_ERR(TAG, "audin_set_device_name for %s failed with error %lu!",
					 entry->subsystem, error);
			}
			else if ((error = audin_load_device_plugin((IWTSPlugin*) audin, audin->subsystem, args)))
			{
				WLog_ERR(TAG, "audin_load_device_plugin %s failed with error %lu!",
					 entry->subsystem, error);
			}

			entry++;
		}
	}

	if (audin->device == NULL)
		WLog_ERR(TAG, "no sound device.");

	error = pEntryPoints->RegisterPlugin(pEntryPoints, "audin", (IWTSPlugin*) audin);

out:
	if (error != CHANNEL_RC_OK)
		audin_plugin_terminated((IWTSPlugin*)audin);

	return error;
}
