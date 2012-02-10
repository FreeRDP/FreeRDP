/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/load_plugin.h>

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
	uint16 fixed_format;
	uint16 fixed_channel;	
	uint32 fixed_rate;

	/* Device interface */
	IAudinDevice* device;
};

static int audin_process_version(IWTSVirtualChannelCallback* pChannelCallback, STREAM* s)
{
	int error;
	STREAM* out;
	uint32 Version;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	stream_read_uint32(s, Version);

	DEBUG_DVC("Version=%d", Version);

	out = stream_new(5);
	stream_write_uint8(out, MSG_SNDIN_VERSION);
	stream_write_uint32(out, Version);
	error = callback->channel->Write(callback->channel, stream_get_length(s), stream_get_head(s), NULL);
	stream_free(out);

	return error;
}

static int audin_send_incoming_data_pdu(IWTSVirtualChannelCallback* pChannelCallback)
{
	uint8 out_data[1];
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out_data[0] = MSG_SNDIN_DATA_INCOMING;
	return callback->channel->Write(callback->channel, 1, out_data, NULL);
}

static int audin_process_formats(IWTSVirtualChannelCallback* pChannelCallback, STREAM* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;
	uint32 i;
	uint8* fm;
	int error;
	STREAM* out;
	uint32 NumFormats;
	audinFormat format;
	uint32 cbSizeFormatsPacket;

	stream_read_uint32(s, NumFormats);
	DEBUG_DVC("NumFormats %d", NumFormats);
	if ((NumFormats < 1) || (NumFormats > 1000))
	{
		DEBUG_WARN("bad NumFormats %d", NumFormats);
		return 1;
	}
	stream_seek_uint32(s); /* cbSizeFormatsPacket */

	callback->formats = (audinFormat*) xzalloc(NumFormats * sizeof(audinFormat));

	out = stream_new(9);
	stream_seek(out, 9);

	/* SoundFormats (variable) */
	for (i = 0; i < NumFormats; i++)
	{
		stream_get_mark(s, fm);
		stream_read_uint16(s, format.wFormatTag);
		stream_read_uint16(s, format.nChannels);
		stream_read_uint32(s, format.nSamplesPerSec);
		stream_seek_uint32(s); /* nAvgBytesPerSec */
		stream_read_uint16(s, format.nBlockAlign);
		stream_read_uint16(s, format.wBitsPerSample);
		stream_read_uint16(s, format.cbSize);
		format.data = stream_get_tail(s);
		stream_seek(s, format.cbSize);
		
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
			stream_check_size(out, 18 + format.cbSize);
			stream_write(out, fm, 18 + format.cbSize);
		}
	}

	audin_send_incoming_data_pdu(pChannelCallback);

	cbSizeFormatsPacket = stream_get_pos(out);
	stream_set_pos(out, 0);

	stream_write_uint8(out, MSG_SNDIN_FORMATS); /* Header (1 byte) */
	stream_write_uint32(out, callback->formats_count); /* NumFormats (4 bytes) */
	stream_write_uint32(out, cbSizeFormatsPacket); /* cbSizeFormatsPacket (4 bytes) */

	error = callback->channel->Write(callback->channel, cbSizeFormatsPacket, stream_get_head(out), NULL);
	stream_free(out);

	return error;
}

static int audin_send_format_change_pdu(IWTSVirtualChannelCallback* pChannelCallback, uint32 NewFormat)
{
	int error;
	STREAM* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out = stream_new(5);
	stream_write_uint8(out, MSG_SNDIN_FORMATCHANGE);
	stream_write_uint32(out, NewFormat);
	error = callback->channel->Write(callback->channel, 5, stream_get_head(out), NULL);
	stream_free(out);

	return error;
}

static int audin_send_open_reply_pdu(IWTSVirtualChannelCallback* pChannelCallback, uint32 Result)
{
	int error;
	STREAM* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;

	out = stream_new(5);
	stream_write_uint8(out, MSG_SNDIN_OPEN_REPLY);
	stream_write_uint32(out, Result);
	error = callback->channel->Write(callback->channel, 5, stream_get_head(out), NULL);
	stream_free(out);

	return error;
}

static boolean audin_receive_wave_data(uint8* data, int size, void* user_data)
{
	int error;
	STREAM* out;
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) user_data;

	error = audin_send_incoming_data_pdu((IWTSVirtualChannelCallback*) callback);

	if (error != 0)
		return false;

	out = stream_new(size + 1);
	stream_write_uint8(out, MSG_SNDIN_DATA);
	stream_write(out, data, size);
	error = callback->channel->Write(callback->channel, stream_get_length(out), stream_get_head(out), NULL);
	stream_free(out);

	return (error == 0 ? true : false);
}

static int audin_process_open(IWTSVirtualChannelCallback* pChannelCallback, STREAM* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;
	audinFormat* format;
	uint32 initialFormat;
	uint32 FramesPerPacket;

	stream_read_uint32(s, FramesPerPacket);
	stream_read_uint32(s, initialFormat);

	DEBUG_DVC("FramesPerPacket=%d initialFormat=%d",
		FramesPerPacket, initialFormat);

	if (initialFormat >= callback->formats_count)
	{
		DEBUG_WARN("invalid format index %d (total %d)",
			initialFormat, callback->formats_count);
		return 1;
	}

	format = &callback->formats[initialFormat];
	if (audin->device)
	{
		IFCALL(audin->device->SetFormat, audin->device, format, FramesPerPacket);
		IFCALL(audin->device->Open, audin->device, audin_receive_wave_data, callback);
	}

	audin_send_format_change_pdu(pChannelCallback, initialFormat);
	audin_send_open_reply_pdu(pChannelCallback, 0);

	return 0;
}

static int audin_process_format_change(IWTSVirtualChannelCallback* pChannelCallback, STREAM* s)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN * audin = (AUDIN_PLUGIN*) callback->plugin;
	uint32 NewFormat;
	audinFormat* format;

	stream_read_uint32(s, NewFormat);

	DEBUG_DVC("NewFormat=%d", NewFormat);

	if (NewFormat >= callback->formats_count)
	{
		DEBUG_WARN("invalid format index %d (total %d)",
			NewFormat, callback->formats_count);
		return 1;
	}

	format = &callback->formats[NewFormat];

	if (audin->device)
	{
		IFCALL(audin->device->Close, audin->device);
		IFCALL(audin->device->SetFormat, audin->device, format, 0);
		IFCALL(audin->device->Open, audin->device, audin_receive_wave_data, callback);
	}

	audin_send_format_change_pdu(pChannelCallback, NewFormat);

	return 0;
}

static int audin_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, uint32 cbSize, uint8* pBuffer)
{
	int error;
	STREAM* s;
	uint8 MessageId;

	s = stream_new(0);
	stream_attach(s, pBuffer, cbSize);

	stream_read_uint8(s, MessageId);

	DEBUG_DVC("MessageId=0x%x", MessageId);

	switch (MessageId)
	{
		case MSG_SNDIN_VERSION:
			error = audin_process_version(pChannelCallback, s);
			break;

		case MSG_SNDIN_FORMATS:
			error = audin_process_formats(pChannelCallback, s);
			break;

		case MSG_SNDIN_OPEN:
			error = audin_process_open(pChannelCallback, s);
			break;

		case MSG_SNDIN_FORMATCHANGE:
			error = audin_process_format_change(pChannelCallback, s);
			break;

		default:
			DEBUG_WARN("unknown MessageId=0x%x", MessageId);
			error = 1;
			break;
	}

	stream_detach(s);
	stream_free(s);

	return error;
}

static int audin_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	AUDIN_CHANNEL_CALLBACK* callback = (AUDIN_CHANNEL_CALLBACK*) pChannelCallback;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) callback->plugin;

	DEBUG_DVC("");

	if (audin->device)
		IFCALL(audin->device->Close, audin->device);

	xfree(callback->formats);
	xfree(callback);

	return 0;
}

static int audin_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, uint8* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	AUDIN_CHANNEL_CALLBACK* callback;
	AUDIN_LISTENER_CALLBACK* listener_callback = (AUDIN_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = xnew(AUDIN_CHANNEL_CALLBACK);

	callback->iface.OnDataReceived = audin_on_data_received;
	callback->iface.OnClose = audin_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int audin_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	audin->listener_callback = xnew(AUDIN_LISTENER_CALLBACK);

	audin->listener_callback->iface.OnNewChannelConnection = audin_on_new_channel_connection;
	audin->listener_callback->plugin = pPlugin;
	audin->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "AUDIO_INPUT", 0,
		(IWTSListenerCallback*) audin->listener_callback, NULL);
}

static int audin_plugin_terminated(IWTSPlugin* pPlugin)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	if (audin->device)
	{
		IFCALL(audin->device->Close, audin->device);
		IFCALL(audin->device->Free, audin->device);
		audin->device = NULL;
	}

	xfree(audin->listener_callback);
	xfree(audin);

	return 0;
}

static void audin_register_device_plugin(IWTSPlugin* pPlugin, IAudinDevice* device)
{
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;

	if (audin->device)
	{
		DEBUG_WARN("existing device, abort.");
		return;
	}

	DEBUG_DVC("device registered.");

	audin->device = device;
}

static boolean audin_load_device_plugin(IWTSPlugin* pPlugin, const char* name, RDP_PLUGIN_DATA* data)
{
	char* fullname;
	PFREERDP_AUDIN_DEVICE_ENTRY entry;
	FREERDP_AUDIN_DEVICE_ENTRY_POINTS entryPoints;

	if (strrchr(name, '.') != NULL)
	{
		entry = (PFREERDP_AUDIN_DEVICE_ENTRY) freerdp_load_plugin(name, AUDIN_DEVICE_EXPORT_FUNC_NAME);
	}
	else
	{
		fullname = xzalloc(strlen(name) + 8);
		strcpy(fullname, "audin_");
		strcat(fullname, name);
		entry = (PFREERDP_AUDIN_DEVICE_ENTRY) freerdp_load_plugin(fullname, AUDIN_DEVICE_EXPORT_FUNC_NAME);
		xfree(fullname);
	}

	if (entry == NULL)
		return false;

	entryPoints.plugin = pPlugin;
	entryPoints.pRegisterAudinDevice = audin_register_device_plugin;
	entryPoints.plugin_data = data;

	if (entry(&entryPoints) != 0)
	{
		DEBUG_WARN("%s entry returns error.", name);
		return false;
	}

	return true;
}

static boolean audin_process_plugin_data(IWTSPlugin* pPlugin, RDP_PLUGIN_DATA* data)
{
	boolean ret;
	AUDIN_PLUGIN* audin = (AUDIN_PLUGIN*) pPlugin;
	RDP_PLUGIN_DATA default_data[2] = { { 0 }, { 0 } };

	if (data->data[0] && (strcmp((char*)data->data[0], "audin") == 0 || strstr((char*) data->data[0], "/audin.") != NULL))
	{
		if (data->data[1] && strcmp((char*)data->data[1], "format") == 0)
		{
			audin->fixed_format = atoi(data->data[2]);
			return true;
		}
		else if (data->data[1] && strcmp((char*)data->data[1], "rate") == 0)
		{
			audin->fixed_rate = atoi(data->data[2]);
			return true;
		}
		else if (data->data[1] && strcmp((char*)data->data[1], "channel") == 0)
		{
			audin->fixed_channel = atoi(data->data[2]);
			return true;
		}
		else if (data->data[1] && ((char*)data->data[1])[0])
		{
			return audin_load_device_plugin(pPlugin, (char*) data->data[1], data);
		}
		else
		{
			default_data[0].size = sizeof(RDP_PLUGIN_DATA);
			default_data[0].data[0] = "audin";
			default_data[0].data[1] = "pulse";
			default_data[0].data[2] = "";

			ret = audin_load_device_plugin(pPlugin, "pulse", default_data);

			if (!ret)
			{
				default_data[0].size = sizeof(RDP_PLUGIN_DATA);
				default_data[0].data[0] = "audin";
				default_data[0].data[1] = "alsa";
				default_data[0].data[2] = "default";
				ret = audin_load_device_plugin(pPlugin, "alsa", default_data);
			}

			return ret;
		}
	}

	return true;
}

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	AUDIN_PLUGIN* audin;

	audin = (AUDIN_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "audin");

	if (audin == NULL)
	{
		audin = xnew(AUDIN_PLUGIN);

		audin->iface.Initialize = audin_plugin_initialize;
		audin->iface.Connected = NULL;
		audin->iface.Disconnected = NULL;
		audin->iface.Terminated = audin_plugin_terminated;
		error = pEntryPoints->RegisterPlugin(pEntryPoints, "audin", (IWTSPlugin*) audin);
	}

	if (error == 0)
		audin_process_plugin_data((IWTSPlugin*) audin, pEntryPoints->GetPluginData(pEntryPoints));

	return error;
}

