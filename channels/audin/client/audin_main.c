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

#include <freerdp/config.h>

#include <errno.h>
#include <winpr/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/wlog.h>

#include <freerdp/addin.h>

#include <winpr/stream.h>
#include <freerdp/freerdp.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/audin.h>

#include "audin_main.h"

#define MSG_SNDIN_VERSION 0x01
#define MSG_SNDIN_FORMATS 0x02
#define MSG_SNDIN_OPEN 0x03
#define MSG_SNDIN_OPEN_REPLY 0x04
#define MSG_SNDIN_DATA_INCOMING 0x05
#define MSG_SNDIN_DATA 0x06
#define MSG_SNDIN_FORMATCHANGE 0x07

typedef struct
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
	AUDIO_FORMAT* formats;
	UINT32 formats_count;
} AUDIN_CHANNEL_CALLBACK;

typedef struct
{
	IWTSPlugin iface;

	GENERIC_LISTENER_CALLBACK* listener_callback;

	/* Parsed plugin data */
	AUDIO_FORMAT* fixed_format;
	char* subsystem;
	char* device_name;

	/* Device interface */
	IAudinDevice* device;

	rdpContext* rdpcontext;
	BOOL attached;
	wStream* data;
	AUDIO_FORMAT* format;
	UINT32 FramesPerPacket;

	FREERDP_DSP_CONTEXT* dsp_context;
	wLog* log;

	IWTSListener* listener;

	BOOL initialized;
} AUDIN_PLUGIN;

static BOOL audin_process_addin_args(AUDIN_PLUGIN* audin, const ADDIN_ARGV* args);

static UINT audin_channel_write_and_free(AUDIN_CHANNEL_CALLBACK* callback, wStream* out,
                                         BOOL freeStream)
{
	UINT error;

	if (!callback || !out)
		return ERROR_INVALID_PARAMETER;

	if (!callback->channel || !callback->channel->Write)
		return ERROR_INTERNAL_ERROR;

	Stream_SealLength(out);
	WINPR_ASSERT(Stream_Length(out) <= ULONG_MAX);
	error = callback->channel->Write(callback->channel, (ULONG)Stream_Length(out),
	                                 Stream_Buffer(out), NULL);

	if (freeStream)
		Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_version(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback, wStream* s)
{
	wStream* out;
	const UINT32 ClientVersion = 0x01;
	UINT32 ServerVersion;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, ServerVersion);
	WLog_Print(audin->log, WLOG_DEBUG, "ServerVersion=%" PRIu32 ", ClientVersion=%" PRIu32,
	           ServerVersion, ClientVersion);

	/* Do not answer server packet, we do not support the channel version. */
	if (ServerVersion != ClientVersion)
	{
		WLog_Print(audin->log, WLOG_WARN,
		           "Incompatible channel version server=%" PRIu32
		           ", client supports version=%" PRIu32,
		           ServerVersion, ClientVersion);
		return CHANNEL_RC_OK;
	}

	out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Stream_New failed!");
		return ERROR_OUTOFMEMORY;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_VERSION);
	Stream_Write_UINT32(out, ClientVersion);
	return audin_channel_write_and_free(callback, out, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_send_incoming_data_pdu(AUDIN_CHANNEL_CALLBACK* callback)
{
	BYTE out_data[1] = { MSG_SNDIN_DATA_INCOMING };

	if (!callback || !callback->channel || !callback->channel->Write)
		return ERROR_INTERNAL_ERROR;

	return callback->channel->Write(callback->channel, 1, out_data, NULL);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_formats(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 i;
	UINT error;
	wStream* out;
	UINT32 NumFormats;
	UINT32 cbSizeFormatsPacket;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, NumFormats);
	WLog_Print(audin->log, WLOG_DEBUG, "NumFormats %" PRIu32 "", NumFormats);

	if ((NumFormats < 1) || (NumFormats > 1000))
	{
		WLog_Print(audin->log, WLOG_ERROR, "bad NumFormats %" PRIu32 "", NumFormats);
		return ERROR_INVALID_DATA;
	}

	Stream_Seek_UINT32(s); /* cbSizeFormatsPacket */
	callback->formats = audio_formats_new(NumFormats);

	if (!callback->formats)
	{
		WLog_Print(audin->log, WLOG_ERROR, "calloc failed!");
		return ERROR_INVALID_DATA;
	}

	out = Stream_New(NULL, 9);

	if (!out)
	{
		error = CHANNEL_RC_NO_MEMORY;
		WLog_Print(audin->log, WLOG_ERROR, "Stream_New failed!");
		goto out;
	}

	Stream_Seek(out, 9);

	/* SoundFormats (variable) */
	for (i = 0; i < NumFormats; i++)
	{
		AUDIO_FORMAT format = { 0 };

		if (!audio_format_read(s, &format))
		{
			error = ERROR_INVALID_DATA;
			goto out;
		}

		audio_format_print(audin->log, WLOG_DEBUG, &format);

		if (!audio_format_compatible(audin->fixed_format, &format))
		{
			audio_format_free(&format);
			continue;
		}

		if (freerdp_dsp_supports_format(&format, TRUE) ||
		    audin->device->FormatSupported(audin->device, &format))
		{
			/* Store the agreed format in the corresponding index */
			callback->formats[callback->formats_count++] = format;

			if (!audio_format_write(out, &format))
			{
				error = CHANNEL_RC_NO_MEMORY;
				WLog_Print(audin->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
				goto out;
			}
		}
		else
		{
			audio_format_free(&format);
		}
	}

	if ((error = audin_send_incoming_data_pdu(callback)))
	{
		WLog_Print(audin->log, WLOG_ERROR, "audin_send_incoming_data_pdu failed!");
		goto out;
	}

	cbSizeFormatsPacket = (UINT32)Stream_GetPosition(out);
	Stream_SetPosition(out, 0);
	Stream_Write_UINT8(out, MSG_SNDIN_FORMATS);        /* Header (1 byte) */
	Stream_Write_UINT32(out, callback->formats_count); /* NumFormats (4 bytes) */
	Stream_Write_UINT32(out, cbSizeFormatsPacket);     /* cbSizeFormatsPacket (4 bytes) */
	Stream_SetPosition(out, cbSizeFormatsPacket);
	error = audin_channel_write_and_free(callback, out, FALSE);
out:

	if (error != CHANNEL_RC_OK)
	{
		audio_formats_free(callback->formats, NumFormats);
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
static UINT audin_send_format_change_pdu(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback,
                                         UINT32 NewFormat)
{
	wStream* out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_OK;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_FORMATCHANGE);
	Stream_Write_UINT32(out, NewFormat);
	return audin_channel_write_and_free(callback, out, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_send_open_reply_pdu(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback,
                                      UINT32 Result)
{
	wStream* out = Stream_New(NULL, 5);

	if (!out)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(out, MSG_SNDIN_OPEN_REPLY);
	Stream_Write_UINT32(out, Result);
	return audin_channel_write_and_free(callback, out, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_receive_wave_data(const AUDIO_FORMAT* format, const BYTE* data, size_t size,
                                    void* user_data)
{
	UINT error;
	BOOL compatible;
	AUDIN_PLUGIN* audin;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*)user_data;

	if (!callback)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	audin = (AUDIN_PLUGIN*)callback->plugin;

	if (!audin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!audin->attached)
		return CHANNEL_RC_OK;

	Stream_SetPosition(audin->data, 0);

	if (!Stream_EnsureRemainingCapacity(audin->data, 1))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT8(audin->data, MSG_SNDIN_DATA);

	compatible = audio_format_compatible(format, audin->format);
	if (compatible && audin->device->FormatSupported(audin->device, audin->format))
	{
		if (!Stream_EnsureRemainingCapacity(audin->data, size))
			return CHANNEL_RC_NO_MEMORY;

		Stream_Write(audin->data, data, size);
	}
	else
	{
		if (!freerdp_dsp_encode(audin->dsp_context, format, data, size, audin->data))
			return ERROR_INTERNAL_ERROR;
	}

	/* Did not encode anything, skip this, the codec is not ready for output. */
	if (Stream_GetPosition(audin->data) <= 1)
		return CHANNEL_RC_OK;

	audio_format_print(audin->log, WLOG_TRACE, audin->format);
	WLog_Print(audin->log, WLOG_TRACE, "[%" PRIdz "/%" PRIdz "]", size,
	           Stream_GetPosition(audin->data) - 1);

	if ((error = audin_send_incoming_data_pdu(callback)))
	{
		WLog_Print(audin->log, WLOG_ERROR, "audin_send_incoming_data_pdu failed!");
		return error;
	}

	return audin_channel_write_and_free(callback, audin->data, FALSE);
}

static BOOL audin_open_device(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback)
{
	UINT error = ERROR_INTERNAL_ERROR;
	BOOL supported;
	AUDIO_FORMAT format;

	if (!audin || !audin->device)
		return FALSE;

	format = *audin->format;
	supported = IFCALLRESULT(FALSE, audin->device->FormatSupported, audin->device, &format);
	WLog_Print(audin->log, WLOG_DEBUG, "microphone uses %s codec",
	           audio_format_get_tag_string(format.wFormatTag));

	if (!supported)
	{
		/* Default sample rates supported by most backends. */
		const UINT32 samplerates[] = { format.nSamplesPerSec, 96000, 48000, 44100, 22050 };
		BOOL test = FALSE;
		size_t x;

		format.wFormatTag = WAVE_FORMAT_PCM;
		format.wBitsPerSample = 16;
		format.cbSize = 0;
		for (x = 0; x < ARRAYSIZE(samplerates); x++)
		{
			size_t y;
			format.nSamplesPerSec = samplerates[x];
			for (y = audin->format->nChannels; y > 0; y--)
			{
				format.nChannels = y;
				format.nBlockAlign = 2 * format.nChannels;
				test = IFCALLRESULT(FALSE, audin->device->FormatSupported, audin->device, &format);
				if (test)
					break;
			}
			if (test)
				break;
		}
		if (!test)
			return FALSE;
	}

	IFCALLRET(audin->device->SetFormat, error, audin->device, &format, audin->FramesPerPacket);

	if (error != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "SetFormat failed with errorcode %" PRIu32 "", error);
		return FALSE;
	}

	if (!freerdp_dsp_context_reset(audin->dsp_context, audin->format, audin->FramesPerPacket))
		return FALSE;

	IFCALLRET(audin->device->Open, error, audin->device, audin_receive_wave_data, callback);

	if (error != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "Open failed with errorcode %" PRIu32 "", error);
		return FALSE;
	}

	return TRUE;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_open(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 initialFormat;
	UINT32 FramesPerPacket;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, FramesPerPacket);
	Stream_Read_UINT32(s, initialFormat);
	WLog_Print(audin->log, WLOG_DEBUG, "FramesPerPacket=%" PRIu32 " initialFormat=%" PRIu32 "",
	           FramesPerPacket, initialFormat);
	audin->FramesPerPacket = FramesPerPacket;

	if (initialFormat >= callback->formats_count)
	{
		WLog_Print(audin->log, WLOG_ERROR, "invalid format index %" PRIu32 " (total %d)",
		           initialFormat, callback->formats_count);
		return ERROR_INVALID_DATA;
	}

	audin->format = &callback->formats[initialFormat];

	if (!audin_open_device(audin, callback))
		return ERROR_INTERNAL_ERROR;

	if ((error = audin_send_format_change_pdu(audin, callback, initialFormat)))
	{
		WLog_Print(audin->log, WLOG_ERROR, "audin_send_format_change_pdu failed!");
		return error;
	}

	if ((error = audin_send_open_reply_pdu(audin, callback, 0)))
		WLog_Print(audin->log, WLOG_ERROR, "audin_send_open_reply_pdu failed!");

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_process_format_change(AUDIN_PLUGIN* audin, AUDIN_CHANNEL_CALLBACK* callback,
                                        wStream* s)
{
	UINT32 NewFormat;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, NewFormat);
	WLog_Print(audin->log, WLOG_DEBUG, "NewFormat=%" PRIu32 "", NewFormat);

	if (NewFormat >= callback->formats_count)
	{
		WLog_Print(audin->log, WLOG_ERROR, "invalid format index %" PRIu32 " (total %d)", NewFormat,
		           callback->formats_count);
		return ERROR_INVALID_DATA;
	}

	audin->format = &callback->formats[NewFormat];

	if (audin->device)
	{
		IFCALLRET(audin->device->Close, error, audin->device);

		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "Close failed with errorcode %" PRIu32 "", error);
			return error;
		}
	}

	if (!audin_open_device(audin, callback))
		return ERROR_INTERNAL_ERROR;

	if ((error = audin_send_format_change_pdu(audin, callback, NewFormat)))
		WLog_ERR(TAG, "audin_send_format_change_pdu failed!");

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	UINT error;
	BYTE MessageId;
	AUDIN_PLUGIN* audin;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*)pChannelCallback;

	if (!callback || !data)
		return ERROR_INVALID_PARAMETER;

	audin = (AUDIN_PLUGIN*)callback->plugin;

	if (!audin)
		return ERROR_INTERNAL_ERROR;

	if (Stream_GetRemainingCapacity(data) < 1)
		return ERROR_NO_DATA;

	Stream_Read_UINT8(data, MessageId);
	WLog_Print(audin->log, WLOG_DEBUG, "MessageId=0x%02" PRIx8 "", MessageId);

	switch (MessageId)
	{
		case MSG_SNDIN_VERSION:
			error = audin_process_version(audin, callback, data);
			break;

		case MSG_SNDIN_FORMATS:
			error = audin_process_formats(audin, callback, data);
			break;

		case MSG_SNDIN_OPEN:
			error = audin_process_open(audin, callback, data);
			break;

		case MSG_SNDIN_FORMATCHANGE:
			error = audin_process_format_change(audin, callback, data);
			break;

		default:
			WLog_Print(audin->log, WLOG_ERROR, "unknown MessageId=0x%02" PRIx8 "", MessageId);
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
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*)pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)callback->plugin;
	UINT error = CHANNEL_RC_OK;
	WLog_Print(audin->log, WLOG_TRACE, "...");

	if (audin->device)
	{
		IFCALLRET(audin->device->Close, error, audin->device);

		if (error != CHANNEL_RC_OK)
			WLog_Print(audin->log, WLOG_ERROR, "Close failed with errorcode %" PRIu32 "", error);
	}

	audin->format = NULL;
	audio_formats_free(callback->formats, callback->formats_count);
	free(callback);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                            IWTSVirtualChannel* pChannel, BYTE* Data,
                                            BOOL* pbAccept, IWTSVirtualChannelCallback** ppCallback)
{
	AUDIN_CHANNEL_CALLBACK* callback;
	AUDIN_PLUGIN* audin;
	GENERIC_LISTENER_CALLBACK* listener_callback = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;

	if (!listener_callback || !listener_callback->plugin)
		return ERROR_INTERNAL_ERROR;

	audin = (AUDIN_PLUGIN*)listener_callback->plugin;
	WLog_Print(audin->log, WLOG_TRACE, "...");
	callback = (AUDIN_CHANNEL_CALLBACK*)calloc(1, sizeof(AUDIN_CHANNEL_CALLBACK));

	if (!callback)
	{
		WLog_Print(audin->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = audin_on_data_received;
	callback->iface.OnClose = audin_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*)callback;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT rc;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;

	if (!audin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!pChannelMgr)
		return ERROR_INVALID_PARAMETER;

	if (audin->initialized)
	{
		WLog_ERR(TAG, "[%s] channel initialized twice, aborting", AUDIN_DVC_CHANNEL_NAME);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(audin->log, WLOG_TRACE, "...");
	audin->listener_callback =
	    (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));

	if (!audin->listener_callback)
	{
		WLog_Print(audin->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	audin->listener_callback->iface.OnNewChannelConnection = audin_on_new_channel_connection;
	audin->listener_callback->plugin = pPlugin;
	audin->listener_callback->channel_mgr = pChannelMgr;
	rc = pChannelMgr->CreateListener(pChannelMgr, AUDIN_DVC_CHANNEL_NAME, 0,
	                                 &audin->listener_callback->iface, &audin->listener);

	audin->initialized = rc == CHANNEL_RC_OK;
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_plugin_terminated(IWTSPlugin* pPlugin)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!audin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	WLog_Print(audin->log, WLOG_TRACE, "...");

	if (audin->listener_callback)
	{
		IWTSVirtualChannelManager* mgr = audin->listener_callback->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, audin->listener);
	}
	audio_formats_free(audin->fixed_format, 1);

	if (audin->device)
	{
		IFCALLRET(audin->device->Free, error, audin->device);

		if (error != CHANNEL_RC_OK)
		{
			WLog_Print(audin->log, WLOG_ERROR, "Free failed with errorcode %" PRIu32 "", error);
			// dont stop on error
		}

		audin->device = NULL;
	}

	freerdp_dsp_context_free(audin->dsp_context);
	Stream_Free(audin->data, TRUE);
	free(audin->subsystem);
	free(audin->device_name);
	free(audin->listener_callback);
	free(audin);
	return CHANNEL_RC_OK;
}

static UINT audin_plugin_attached(IWTSPlugin* pPlugin)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!audin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	audin->attached = TRUE;
	return error;
}

static UINT audin_plugin_detached(IWTSPlugin* pPlugin)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!audin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	audin->attached = FALSE;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_register_device_plugin(IWTSPlugin* pPlugin, IAudinDevice* device)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*)pPlugin;

	if (audin->device)
	{
		WLog_Print(audin->log, WLOG_ERROR, "existing device, abort.");
		return ERROR_ALREADY_EXISTS;
	}

	WLog_Print(audin->log, WLOG_DEBUG, "device registered.");
	audin->device = device;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_load_device_plugin(AUDIN_PLUGIN* audin, const char* name, const ADDIN_ARGV* args)
{
	FREERDP_AUDIN_DEVICE_ENTRY_POINTS entryPoints;
	UINT error;
	const PFREERDP_AUDIN_DEVICE_ENTRY entry =
	    (const PFREERDP_AUDIN_DEVICE_ENTRY)freerdp_load_channel_addin_entry("audin", name, NULL, 0);

	if (entry == NULL)
	{
		WLog_Print(audin->log, WLOG_ERROR,
		           "freerdp_load_channel_addin_entry did not return any function pointers for %s ",
		           name);
		return ERROR_INVALID_FUNCTION;
	}

	entryPoints.plugin = &audin->iface;
	entryPoints.pRegisterAudinDevice = audin_register_device_plugin;
	entryPoints.args = args;
	entryPoints.rdpcontext = audin->rdpcontext;

	if ((error = entry(&entryPoints)))
	{
		WLog_Print(audin->log, WLOG_ERROR, "%s entry returned error %" PRIu32 ".", name, error);
		return error;
	}

	WLog_Print(audin->log, WLOG_INFO, "Loaded %s backend for audin", name);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_set_subsystem(AUDIN_PLUGIN* audin, const char* subsystem)
{
	free(audin->subsystem);
	audin->subsystem = _strdup(subsystem);

	if (!audin->subsystem)
	{
		WLog_Print(audin->log, WLOG_ERROR, "_strdup failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_set_device_name(AUDIN_PLUGIN* audin, const char* device_name)
{
	free(audin->device_name);
	audin->device_name = _strdup(device_name);

	if (!audin->device_name)
	{
		WLog_Print(audin->log, WLOG_ERROR, "_strdup failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	return CHANNEL_RC_OK;
}

BOOL audin_process_addin_args(AUDIN_PLUGIN* audin, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	UINT error;
	COMMAND_LINE_ARGUMENT_A audin_args[] = {
		{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
		{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
		{ "format", COMMAND_LINE_VALUE_REQUIRED, "<format>", NULL, NULL, -1, NULL, "format" },
		{ "rate", COMMAND_LINE_VALUE_REQUIRED, "<rate>", NULL, NULL, -1, NULL, "rate" },
		{ "channel", COMMAND_LINE_VALUE_REQUIRED, "<channel>", NULL, NULL, -1, NULL, "channel" },
		{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
	};

	if (!args || args->argc == 1)
		return TRUE;

	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status =
	    CommandLineParseArgumentsA(args->argc, args->argv, audin_args, flags, audin, NULL, NULL);

	if (status != 0)
		return FALSE;

	arg = audin_args;
	errno = 0;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "sys")
		{
			if ((error = audin_set_subsystem(audin, arg->Value)))
			{
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_set_subsystem failed with error %" PRIu32 "!", error);
				return FALSE;
			}
		}
		CommandLineSwitchCase(arg, "dev")
		{
			if ((error = audin_set_device_name(audin, arg->Value)))
			{
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_set_device_name failed with error %" PRIu32 "!", error);
				return FALSE;
			}
		}
		CommandLineSwitchCase(arg, "format")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val > UINT16_MAX))
				return FALSE;

			audin->fixed_format->wFormatTag = val;
		}
		CommandLineSwitchCase(arg, "rate")
		{
			long val = strtol(arg->Value, NULL, 0);

			if ((errno != 0) || (val < INT32_MIN) || (val > INT32_MAX))
				return FALSE;

			audin->fixed_format->nSamplesPerSec = val;
		}
		CommandLineSwitchCase(arg, "channel")
		{
			unsigned long val = strtoul(arg->Value, NULL, 0);

			if ((errno != 0) || (val < UINT16_MAX))
				audin->fixed_format->nChannels = val;
		}
		CommandLineSwitchDefault(arg)
		{
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT audin_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	struct SubsystemEntry
	{
		char* subsystem;
		char* device;
	};
	UINT error = CHANNEL_RC_INITIALIZATION_ERROR;
	const ADDIN_ARGV* args;
	AUDIN_PLUGIN* audin;
	struct SubsystemEntry entries[] =
	{
#if defined(WITH_PULSE)
		{ "pulse", "" },
#endif
#if defined(WITH_OSS)
		{ "oss", "default" },
#endif
#if defined(WITH_ALSA)
		{ "alsa", "default" },
#endif
#if defined(WITH_OPENSLES)
		{ "opensles", "default" },
#endif
#if defined(WITH_WINMM)
		{ "winmm", "default" },
#endif
#if defined(WITH_MACAUDIO)
		{ "mac", "default" },
#endif
#if defined(WITH_IOSAUDIO)
		{ "ios", "default" },
#endif
#if defined(WITH_SNDIO)
		{ "sndio", "default" },
#endif
		{ NULL, NULL }
	};
	struct SubsystemEntry* entry = &entries[0];
	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->GetPlugin);
	audin = (AUDIN_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, "audin");

	if (audin != NULL)
		return CHANNEL_RC_ALREADY_INITIALIZED;

	audin = (AUDIN_PLUGIN*)calloc(1, sizeof(AUDIN_PLUGIN));

	if (!audin)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	audin->log = WLog_Get(TAG);
	audin->data = Stream_New(NULL, 4096);
	audin->fixed_format = audio_format_new();

	if (!audin->fixed_format)
		goto out;

	if (!audin->data)
		goto out;

	audin->dsp_context = freerdp_dsp_context_new(TRUE);

	if (!audin->dsp_context)
		goto out;

	audin->attached = TRUE;
	audin->iface.Initialize = audin_plugin_initialize;
	audin->iface.Connected = NULL;
	audin->iface.Disconnected = NULL;
	audin->iface.Terminated = audin_plugin_terminated;
	audin->iface.Attached = audin_plugin_attached;
	audin->iface.Detached = audin_plugin_detached;
	args = pEntryPoints->GetPluginData(pEntryPoints);
	audin->rdpcontext = pEntryPoints->GetRdpContext(pEntryPoints);

	if (args)
	{
		if (!audin_process_addin_args(audin, args))
			goto out;
	}

	if (audin->subsystem)
	{
		if ((error = audin_load_device_plugin(audin, audin->subsystem, args)))
		{
			WLog_Print(
			    audin->log, WLOG_ERROR,
			    "Unable to load microphone redirection subsystem %s because of error %" PRIu32 "",
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
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_set_subsystem for %s failed with error %" PRIu32 "!",
				           entry->subsystem, error);
			}
			else if ((error = audin_set_device_name(audin, entry->device)))
			{
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_set_device_name for %s failed with error %" PRIu32 "!",
				           entry->subsystem, error);
			}
			else if ((error = audin_load_device_plugin(audin, audin->subsystem, args)))
			{
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_load_device_plugin %s failed with error %" PRIu32 "!",
				           entry->subsystem, error);
			}

			entry++;
		}
	}

	if (audin->device == NULL)
	{
		/* If we have no audin device do not register plugin but still return OK or the client will
		 * just disconnect due to a missing microphone. */
		WLog_Print(audin->log, WLOG_ERROR, "No microphone device could be found.");
		error = CHANNEL_RC_OK;
		goto out;
	}

	error = pEntryPoints->RegisterPlugin(pEntryPoints, "audin", &audin->iface);
	if (error == CHANNEL_RC_OK)
		return error;

out:
	audin_plugin_terminated(&audin->iface);
	return error;
}
