/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/constants.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

struct rdpsnd_plugin
{
	rdpSvcPlugin plugin;

	LIST* data_out_list;

	BYTE cBlockNo;
	rdpsndFormat* supported_formats;
	int n_supported_formats;
	int current_format;

	BOOL expectingWave;
	BYTE waveData[4];
	UINT16 waveDataSize;
	UINT32 wTimeStamp; /* server timestamp */
	UINT32 wave_timestamp; /* client timestamp */

	BOOL is_open;
	UINT32 close_timestamp;

	UINT16 fixed_format;
	UINT16 fixed_channel;
	UINT32 fixed_rate;
	int latency;

	char* subsystem;
	char* device_name;

	/* Device plugin */
	rdpsndDevicePlugin* device;
};

struct data_out_item
{
	STREAM* data_out;
	UINT32 out_timestamp;
};

/* get time in milliseconds */
static UINT32 get_mstime(void)
{
#ifndef _WIN32
	struct timeval tp;
	gettimeofday(&tp, 0);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
#else
	FILETIME ft;
	UINT64 time64 = 0;

	GetSystemTimeAsFileTime(&ft);
	
	time64 |= ft.dwHighDateTime;
	time64 <<= 32;
	time64 |= ft.dwLowDateTime;
	time64 /= 10000;
	
	/* fix epoch? */

	return (UINT32) time64;
#endif
}

/* process the linked list of data that has queued to be sent */
static void rdpsnd_process_interval(rdpSvcPlugin* plugin)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	struct data_out_item* item;
	UINT32 cur_time;

	while (list_size(rdpsnd->data_out_list) > 0)
	{
		item = (struct data_out_item*) list_peek(rdpsnd->data_out_list);

		cur_time = get_mstime();

		if (!item || cur_time <= item->out_timestamp)
			break;

		item = (struct data_out_item*) list_dequeue(rdpsnd->data_out_list);
		svc_plugin_send(plugin, item->data_out);
		free(item);

		DEBUG_SVC("processed data_out");
	}

	if (rdpsnd->is_open && rdpsnd->close_timestamp > 0)
	{
		cur_time = get_mstime();

		if (cur_time > rdpsnd->close_timestamp)
		{
			if (rdpsnd->device)
				IFCALL(rdpsnd->device->Close, rdpsnd->device);
			rdpsnd->is_open = FALSE;
			rdpsnd->close_timestamp = 0;

			DEBUG_SVC("processed close");
		}
	}

	if (list_size(rdpsnd->data_out_list) == 0 && !rdpsnd->is_open)
	{
		rdpsnd->plugin.interval_ms = 0;
	}
}

static void rdpsnd_free_supported_formats(rdpsndPlugin* rdpsnd)
{
	UINT16 i;

	for (i = 0; i < rdpsnd->n_supported_formats; i++)
		free(rdpsnd->supported_formats[i].data);
	free(rdpsnd->supported_formats);

	rdpsnd->supported_formats = NULL;
	rdpsnd->n_supported_formats = 0;
}

/* receives a list of server supported formats and returns a list
   of client supported formats */
static void rdpsnd_process_message_formats(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	UINT16 wNumberOfFormats;
	UINT16 nFormat;
	UINT16 wVersion;
	STREAM* data_out;
	rdpsndFormat* out_formats;
	UINT16 n_out_formats;
	rdpsndFormat* format;
	BYTE* format_mark;
	BYTE* data_mark;
	int pos;

	rdpsnd_free_supported_formats(rdpsnd);

	stream_seek_UINT32(data_in); /* dwFlags */
	stream_seek_UINT32(data_in); /* dwVolume */
	stream_seek_UINT32(data_in); /* dwPitch */
	stream_seek_UINT16(data_in); /* wDGramPort */
	stream_read_UINT16(data_in, wNumberOfFormats);
	stream_read_BYTE(data_in, rdpsnd->cBlockNo); /* cLastBlockConfirmed */
	stream_read_UINT16(data_in, wVersion);
	stream_seek_BYTE(data_in); /* bPad */

	DEBUG_SVC("wNumberOfFormats %d wVersion %d", wNumberOfFormats, wVersion);
	if (wNumberOfFormats < 1)
	{
		DEBUG_WARN("wNumberOfFormats is 0");
		return;
	}

	out_formats = (rdpsndFormat*) malloc(wNumberOfFormats * sizeof(rdpsndFormat));
	ZeroMemory(out_formats, wNumberOfFormats * sizeof(rdpsndFormat));
	n_out_formats = 0;

	data_out = stream_new(24);
	stream_write_BYTE(data_out, SNDC_FORMATS); /* msgType */
	stream_write_BYTE(data_out, 0); /* bPad */
	stream_seek_UINT16(data_out); /* BodySize */
	stream_write_UINT32(data_out, TSSNDCAPS_ALIVE | TSSNDCAPS_VOLUME); /* dwFlags */
	stream_write_UINT32(data_out, 0xFFFFFFFF); /* dwVolume */
	stream_write_UINT32(data_out, 0); /* dwPitch */
	stream_write_UINT16_be(data_out, 0); /* wDGramPort */
	stream_seek_UINT16(data_out); /* wNumberOfFormats */
	stream_write_BYTE(data_out, 0); /* cLastBlockConfirmed */
	stream_write_UINT16(data_out, 6); /* wVersion */
	stream_write_BYTE(data_out, 0); /* bPad */

	for (nFormat = 0; nFormat < wNumberOfFormats; nFormat++)
	{
		stream_get_mark(data_in, format_mark);
		format = &out_formats[n_out_formats];
		stream_read_UINT16(data_in, format->wFormatTag);
		stream_read_UINT16(data_in, format->nChannels);
		stream_read_UINT32(data_in, format->nSamplesPerSec);
		stream_seek_UINT32(data_in); /* nAvgBytesPerSec */
		stream_read_UINT16(data_in, format->nBlockAlign);
		stream_read_UINT16(data_in, format->wBitsPerSample);
		stream_read_UINT16(data_in, format->cbSize);
		stream_get_mark(data_in, data_mark);
		stream_seek(data_in, format->cbSize);
		format->data = NULL;

		DEBUG_SVC("wFormatTag=%d nChannels=%d nSamplesPerSec=%d nBlockAlign=%d wBitsPerSample=%d",
			format->wFormatTag, format->nChannels, format->nSamplesPerSec,
			format->nBlockAlign, format->wBitsPerSample);

		if (rdpsnd->fixed_format > 0 && rdpsnd->fixed_format != format->wFormatTag)
			continue;
		if (rdpsnd->fixed_channel > 0 && rdpsnd->fixed_channel != format->nChannels)
			continue;
		if (rdpsnd->fixed_rate > 0 && rdpsnd->fixed_rate != format->nSamplesPerSec)
			continue;
		if (rdpsnd->device && rdpsnd->device->FormatSupported(rdpsnd->device, format))
		{
			DEBUG_SVC("format supported.");

			stream_check_size(data_out, 18 + format->cbSize);
			stream_write(data_out, format_mark, 18 + format->cbSize);
			if (format->cbSize > 0)
			{
				format->data = malloc(format->cbSize);
				memcpy(format->data, data_mark, format->cbSize);
			}
			n_out_formats++;
		}
	}

	rdpsnd->n_supported_formats = n_out_formats;
	if (n_out_formats > 0)
	{
		rdpsnd->supported_formats = out_formats;
	}
	else
	{
		free(out_formats);
		DEBUG_WARN("no formats supported");
	}

	pos = stream_get_pos(data_out);
	stream_set_pos(data_out, 2);
	stream_write_UINT16(data_out, pos - 4);
	stream_set_pos(data_out, 18);
	stream_write_UINT16(data_out, n_out_formats);
	stream_set_pos(data_out, pos);

	svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);

	if (wVersion >= 6)
	{
		data_out = stream_new(8);
		stream_write_BYTE(data_out, SNDC_QUALITYMODE); /* msgType */
		stream_write_BYTE(data_out, 0); /* bPad */
		stream_write_UINT16(data_out, 4); /* BodySize */
		stream_write_UINT16(data_out, HIGH_QUALITY); /* wQualityMode */
		stream_write_UINT16(data_out, 0); /* Reserved */

		svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);
	}
}

/* server is getting a feel of the round trip time */
static void rdpsnd_process_message_training(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	UINT16 wTimeStamp;
	UINT16 wPackSize;
	STREAM* data_out;

	stream_read_UINT16(data_in, wTimeStamp);
	stream_read_UINT16(data_in, wPackSize);

	data_out = stream_new(8);
	stream_write_BYTE(data_out, SNDC_TRAINING); /* msgType */
	stream_write_BYTE(data_out, 0); /* bPad */
	stream_write_UINT16(data_out, 4); /* BodySize */
	stream_write_UINT16(data_out, wTimeStamp);
	stream_write_UINT16(data_out, wPackSize);

	svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);
}

static void rdpsnd_process_message_wave_info(rdpsndPlugin* rdpsnd, STREAM* data_in, UINT16 BodySize)
{
	UINT16 wFormatNo;

	stream_read_UINT16(data_in, rdpsnd->wTimeStamp);
	stream_read_UINT16(data_in, wFormatNo);
	stream_read_BYTE(data_in, rdpsnd->cBlockNo);
	stream_seek(data_in, 3); /* bPad */
	stream_read(data_in, rdpsnd->waveData, 4);
	rdpsnd->waveDataSize = BodySize - 8;
	rdpsnd->wave_timestamp = get_mstime();
	rdpsnd->expectingWave = TRUE;

	DEBUG_SVC("waveDataSize %d wFormatNo %d", rdpsnd->waveDataSize, wFormatNo);

	rdpsnd->close_timestamp = 0;

	if (!rdpsnd->is_open)
	{
		rdpsnd->current_format = wFormatNo;
		rdpsnd->is_open = TRUE;

		if (rdpsnd->device)
		{
			IFCALL(rdpsnd->device->Open, rdpsnd->device, &rdpsnd->supported_formats[wFormatNo],
				rdpsnd->latency);
		}
	}
	else if (wFormatNo != rdpsnd->current_format)
	{
		rdpsnd->current_format = wFormatNo;

		if (rdpsnd->device)
		{
			IFCALL(rdpsnd->device->SetFormat, rdpsnd->device, &rdpsnd->supported_formats[wFormatNo],
				rdpsnd->latency);
		}
	}
}

/* header is not removed from data in this function */
static void rdpsnd_process_message_wave(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	UINT16 wTimeStamp;
	UINT32 delay_ms;
	UINT32 process_ms;
	struct data_out_item* item;

	rdpsnd->expectingWave = 0;

	memcpy(stream_get_head(data_in), rdpsnd->waveData, 4);

	if (stream_get_size(data_in) != rdpsnd->waveDataSize)
	{
		DEBUG_WARN("size error");
		return;
	}

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Play, rdpsnd->device, stream_get_head(data_in), stream_get_size(data_in));
	}

	process_ms = get_mstime() - rdpsnd->wave_timestamp;
	delay_ms = 250;
	wTimeStamp = rdpsnd->wTimeStamp + delay_ms;

	DEBUG_SVC("data_size %d delay_ms %u process_ms %u",
		stream_get_size(data_in), delay_ms, process_ms);

	item = (struct data_out_item*) malloc(sizeof(struct data_out_item));
	ZeroMemory(item, sizeof(struct data_out_item));

	item->data_out = stream_new(8);
	stream_write_BYTE(item->data_out, SNDC_WAVECONFIRM);
	stream_write_BYTE(item->data_out, 0);
	stream_write_UINT16(item->data_out, 4);
	stream_write_UINT16(item->data_out, wTimeStamp);
	stream_write_BYTE(item->data_out, rdpsnd->cBlockNo); /* cConfirmedBlockNo */
	stream_write_BYTE(item->data_out, 0); /* bPad */
	item->out_timestamp = rdpsnd->wave_timestamp + delay_ms;

	list_enqueue(rdpsnd->data_out_list, item);
	rdpsnd->plugin.interval_ms = 10;
}

static void rdpsnd_process_message_close(rdpsndPlugin* rdpsnd)
{
	DEBUG_SVC("server closes.");

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Start, rdpsnd->device);
	}

	rdpsnd->close_timestamp = get_mstime() + 2000;
	rdpsnd->plugin.interval_ms = 10;
}

static void rdpsnd_process_message_setvolume(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	UINT32 dwVolume;

	stream_read_UINT32(data_in, dwVolume);
	DEBUG_SVC("dwVolume 0x%X", dwVolume);

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->SetVolume, rdpsnd->device, dwVolume);
	}
}

static void rdpsnd_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	BYTE msgType;
	UINT16 BodySize;

	if (rdpsnd->expectingWave)
	{
		rdpsnd_process_message_wave(rdpsnd, data_in);
		stream_free(data_in);
		return;
	}

	stream_read_BYTE(data_in, msgType); /* msgType */
	stream_seek_BYTE(data_in); /* bPad */
	stream_read_UINT16(data_in, BodySize);

	DEBUG_SVC("msgType %d BodySize %d", msgType, BodySize);

	switch (msgType)
	{
		case SNDC_FORMATS:
			rdpsnd_process_message_formats(rdpsnd, data_in);
			break;

		case SNDC_TRAINING:
			rdpsnd_process_message_training(rdpsnd, data_in);
			break;

		case SNDC_WAVE:
			rdpsnd_process_message_wave_info(rdpsnd, data_in, BodySize);
			break;

		case SNDC_CLOSE:
			rdpsnd_process_message_close(rdpsnd);
			break;

		case SNDC_SETVOLUME:
			rdpsnd_process_message_setvolume(rdpsnd, data_in);
			break;

		default:
			DEBUG_WARN("unknown msgType %d", msgType);
			break;
	}

	stream_free(data_in);
}

static void rdpsnd_register_device_plugin(rdpsndPlugin* rdpsnd, rdpsndDevicePlugin* device)
{
	if (rdpsnd->device)
	{
		DEBUG_WARN("existing device, abort.");
		return;
	}
	rdpsnd->device = device;
}

static BOOL rdpsnd_load_device_plugin(rdpsndPlugin* rdpsnd, const char* name, ADDIN_ARGV* args)
{
	PFREERDP_RDPSND_DEVICE_ENTRY entry;
	FREERDP_RDPSND_DEVICE_ENTRY_POINTS entryPoints;

	entry = (PFREERDP_RDPSND_DEVICE_ENTRY) freerdp_load_channel_addin_entry("rdpsnd", (LPSTR) name, NULL, 0);

	if (entry == NULL)
		return FALSE;

	entryPoints.rdpsnd = rdpsnd;
	entryPoints.pRegisterRdpsndDevice = rdpsnd_register_device_plugin;
	entryPoints.args = args;

	if (entry(&entryPoints) != 0)
	{
		DEBUG_WARN("%s entry returns error.", name);
		return FALSE;
	}

	return TRUE;
}

void rdpsnd_set_subsystem(rdpsndPlugin* rdpsnd, char* subsystem)
{
	if (rdpsnd->subsystem)
		free(rdpsnd->subsystem);

	rdpsnd->subsystem = _strdup(subsystem);
}

void rdpsnd_set_device_name(rdpsndPlugin* rdpsnd, char* device_name)
{
	if (rdpsnd->device_name)
		free(rdpsnd->device_name);

	rdpsnd->device_name = _strdup(device_name);
}

COMMAND_LINE_ARGUMENT_A rdpsnd_args[] =
{
	{ "sys", COMMAND_LINE_VALUE_REQUIRED, "<subsystem>", NULL, NULL, -1, NULL, "subsystem" },
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "device" },
	{ "format", COMMAND_LINE_VALUE_REQUIRED, "<format>", NULL, NULL, -1, NULL, "format" },
	{ "rate", COMMAND_LINE_VALUE_REQUIRED, "<rate>", NULL, NULL, -1, NULL, "rate" },
	{ "channel", COMMAND_LINE_VALUE_REQUIRED, "<channel>", NULL, NULL, -1, NULL, "channel" },
	{ "latency", COMMAND_LINE_VALUE_REQUIRED, "<latency>", NULL, NULL, -1, NULL, "latency" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void rdpsnd_process_addin_args(rdpsndPlugin* rdpsnd, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			rdpsnd_args, flags, rdpsnd, NULL, NULL);

	arg = rdpsnd_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "sys")
		{
			rdpsnd_set_subsystem(rdpsnd, arg->Value);
		}
		CommandLineSwitchCase(arg, "dev")
		{
			rdpsnd_set_device_name(rdpsnd, arg->Value);
		}
		CommandLineSwitchCase(arg, "format")
		{
			rdpsnd->fixed_format = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "rate")
		{
			rdpsnd->fixed_rate = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "channel")
		{
			rdpsnd->fixed_channel = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "latency")
		{
			rdpsnd->latency = atoi(arg->Value);
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

static void rdpsnd_process_connect(rdpSvcPlugin* plugin)
{
	ADDIN_ARGV* args;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) plugin;

	DEBUG_SVC("connecting");

	plugin->interval_callback = rdpsnd_process_interval;

	rdpsnd->data_out_list = list_new();
	rdpsnd->latency = -1;

	args = (ADDIN_ARGV*) plugin->channel_entry_points.pExtendedData;

	if (args)
		rdpsnd_process_addin_args(rdpsnd, args);

	if (rdpsnd->subsystem)
	{
		if (strcmp(rdpsnd->subsystem, "fake") == 0)
			return;

		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "pulse");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "alsa");
		rdpsnd_set_device_name(rdpsnd, "default");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "macaudio");
		rdpsnd_set_device_name(rdpsnd, "default");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

	if (rdpsnd->device == NULL)
	{
		DEBUG_WARN("no sound device.");
	}
}

static void rdpsnd_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	freerdp_event_free(event);
}

static void rdpsnd_process_terminate(rdpSvcPlugin* plugin)
{
	struct data_out_item* item;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) plugin;

	if (rdpsnd->device)
		IFCALL(rdpsnd->device->Free, rdpsnd->device);

	while ((item = list_dequeue(rdpsnd->data_out_list)) != NULL)
	{
		stream_free(item->data_out);
		free(item);
	}
	list_free(rdpsnd->data_out_list);

	if (rdpsnd->subsystem)
		free(rdpsnd->subsystem);

	if (rdpsnd->device_name)
		free(rdpsnd->device_name);

	rdpsnd_free_supported_formats(rdpsnd);

	free(plugin);
}

/* rdpsnd is always built-in */
#define VirtualChannelEntry	rdpsnd_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	rdpsndPlugin* _p;

	_p = (rdpsndPlugin*) malloc(sizeof(rdpsndPlugin));
	ZeroMemory(_p, sizeof(rdpsndPlugin));

	_p->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP;

	strcpy(_p->plugin.channel_def.name, "rdpsnd");

	_p->plugin.connect_callback = rdpsnd_process_connect;
	_p->plugin.receive_callback = rdpsnd_process_receive;
	_p->plugin.event_callback = rdpsnd_process_event;
	_p->plugin.terminate_callback = rdpsnd_process_terminate;

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
