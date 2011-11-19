/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef _WIN32
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

#define SNDC_CLOSE         1
#define SNDC_WAVE          2
#define SNDC_SETVOLUME     3
#define SNDC_SETPITCH      4
#define SNDC_WAVECONFIRM   5
#define SNDC_TRAINING      6
#define SNDC_FORMATS       7
#define SNDC_CRYPTKEY      8
#define SNDC_WAVEENCRYPT   9
#define SNDC_UDPWAVE       10
#define SNDC_UDPWAVELAST   11
#define SNDC_QUALITYMODE   12

#define TSSNDCAPS_ALIVE  1
#define TSSNDCAPS_VOLUME 2
#define TSSNDCAPS_PITCH  4

#define DYNAMIC_QUALITY  0x0000
#define MEDIUM_QUALITY   0x0001
#define HIGH_QUALITY     0x0002

struct rdpsnd_plugin
{
	rdpSvcPlugin plugin;

	LIST* data_out_list;

	uint8 cBlockNo;
	rdpsndFormat* supported_formats;
	int n_supported_formats;
	int current_format;

	boolean expectingWave;
	uint8 waveData[4];
	uint16 waveDataSize;
	uint32 wTimeStamp; /* server timestamp */
	uint32 wave_timestamp; /* client timestamp */

	boolean is_open;
	uint32 close_timestamp;

	uint16 fixed_format;
	uint16 fixed_channel;
	uint32 fixed_rate;
	int latency;

	/* Device plugin */
	rdpsndDevicePlugin* device;
};

struct data_out_item
{
	STREAM* data_out;
	uint32 out_timestamp;
};

/* get time in milliseconds */
static uint32 get_mstime(void)
{
	struct timeval tp;

	gettimeofday(&tp, 0);
	return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

/* process the linked list of data that has queued to be sent */
static void rdpsnd_process_interval(rdpSvcPlugin* plugin)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	struct data_out_item* item;
	uint32 cur_time;

	while (rdpsnd->data_out_list->head)
	{
		item = (struct data_out_item*)rdpsnd->data_out_list->head->data;
		cur_time = get_mstime();
		if (cur_time <= item->out_timestamp)
			break;

		item = (struct data_out_item*)list_dequeue(rdpsnd->data_out_list);
		svc_plugin_send(plugin, item->data_out);
		xfree(item);

		DEBUG_SVC("processed data_out");
	}

	if (rdpsnd->is_open && rdpsnd->close_timestamp > 0)
	{
		cur_time = get_mstime();
		if (cur_time > rdpsnd->close_timestamp)
		{
			if (rdpsnd->device)
				IFCALL(rdpsnd->device->Close, rdpsnd->device);
			rdpsnd->is_open = false;
			rdpsnd->close_timestamp = 0;

			DEBUG_SVC("processed close");
		}
	}

	if (rdpsnd->data_out_list->head == NULL && !rdpsnd->is_open)
	{
		rdpsnd->plugin.interval_ms = 0;
	}
}

static void rdpsnd_free_supported_formats(rdpsndPlugin* rdpsnd)
{
	uint16 i;

	for (i = 0; i < rdpsnd->n_supported_formats; i++)
		xfree(rdpsnd->supported_formats[i].data);
	xfree(rdpsnd->supported_formats);

	rdpsnd->supported_formats = NULL;
	rdpsnd->n_supported_formats = 0;
}

/* receives a list of server supported formats and returns a list
   of client supported formats */
static void rdpsnd_process_message_formats(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	uint16 wNumberOfFormats;
	uint16 nFormat;
	uint16 wVersion;
	STREAM* data_out;
	rdpsndFormat* out_formats;
	uint16 n_out_formats;
	rdpsndFormat* format;
	uint8* format_mark;
	uint8* data_mark;
	int pos;

	rdpsnd_free_supported_formats(rdpsnd);

	stream_seek_uint32(data_in); /* dwFlags */
	stream_seek_uint32(data_in); /* dwVolume */
	stream_seek_uint32(data_in); /* dwPitch */
	stream_seek_uint16(data_in); /* wDGramPort */
	stream_read_uint16(data_in, wNumberOfFormats);
	stream_read_uint8(data_in, rdpsnd->cBlockNo); /* cLastBlockConfirmed */
	stream_read_uint16(data_in, wVersion);
	stream_seek_uint8(data_in); /* bPad */

	DEBUG_SVC("wNumberOfFormats %d wVersion %d", wNumberOfFormats, wVersion);
	if (wNumberOfFormats < 1)
	{
		DEBUG_WARN("wNumberOfFormats is 0");
		return;
	}

	out_formats = (rdpsndFormat*)xzalloc(wNumberOfFormats * sizeof(rdpsndFormat));
	n_out_formats = 0;

	data_out = stream_new(24);
	stream_write_uint8(data_out, SNDC_FORMATS); /* msgType */
	stream_write_uint8(data_out, 0); /* bPad */
	stream_seek_uint16(data_out); /* BodySize */
	stream_write_uint32(data_out, TSSNDCAPS_ALIVE); /* dwFlags */
	stream_write_uint32(data_out, 0); /* dwVolume */
	stream_write_uint32(data_out, 0); /* dwPitch */
	stream_write_uint16_be(data_out, 0); /* wDGramPort */
	stream_seek_uint16(data_out); /* wNumberOfFormats */
	stream_write_uint8(data_out, 0); /* cLastBlockConfirmed */
	stream_write_uint16(data_out, 6); /* wVersion */
	stream_write_uint8(data_out, 0); /* bPad */

	for (nFormat = 0; nFormat < wNumberOfFormats; nFormat++)
	{
		stream_get_mark(data_in, format_mark);
		format = &out_formats[n_out_formats];
		stream_read_uint16(data_in, format->wFormatTag);
		stream_read_uint16(data_in, format->nChannels);
		stream_read_uint32(data_in, format->nSamplesPerSec);
		stream_seek_uint32(data_in); /* nAvgBytesPerSec */
		stream_read_uint16(data_in, format->nBlockAlign);
		stream_read_uint16(data_in, format->wBitsPerSample);
		stream_read_uint16(data_in, format->cbSize);
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
				format->data = xmalloc(format->cbSize);
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
		xfree(out_formats);
		DEBUG_WARN("no formats supported");
	}

	pos = stream_get_pos(data_out);
	stream_set_pos(data_out, 2);
	stream_write_uint16(data_out, pos - 4);
	stream_set_pos(data_out, 18);
	stream_write_uint16(data_out, n_out_formats);
	stream_set_pos(data_out, pos);

	svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);

	if (wVersion >= 6)
	{
		data_out = stream_new(8);
		stream_write_uint8(data_out, SNDC_QUALITYMODE); /* msgType */
		stream_write_uint8(data_out, 0); /* bPad */
		stream_write_uint16(data_out, 4); /* BodySize */
		stream_write_uint16(data_out, HIGH_QUALITY); /* wQualityMode */
		stream_write_uint16(data_out, 0); /* Reserved */

		svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);
	}
}

/* server is getting a feel of the round trip time */
static void rdpsnd_process_message_training(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	uint16 wTimeStamp;
	uint16 wPackSize;
	STREAM* data_out;

	stream_read_uint16(data_in, wTimeStamp);
	stream_read_uint16(data_in, wPackSize);

	data_out = stream_new(8);
	stream_write_uint8(data_out, SNDC_TRAINING); /* msgType */
	stream_write_uint8(data_out, 0); /* bPad */
	stream_write_uint16(data_out, 4); /* BodySize */
	stream_write_uint16(data_out, wTimeStamp);
	stream_write_uint16(data_out, wPackSize);

	svc_plugin_send((rdpSvcPlugin*)rdpsnd, data_out);
}

static void rdpsnd_process_message_wave_info(rdpsndPlugin* rdpsnd, STREAM* data_in, uint16 BodySize)
{
	uint16 wFormatNo;

	stream_read_uint16(data_in, rdpsnd->wTimeStamp);
	stream_read_uint16(data_in, wFormatNo);
	stream_read_uint8(data_in, rdpsnd->cBlockNo);
	stream_seek(data_in, 3); /* bPad */
	stream_read(data_in, rdpsnd->waveData, 4);
	rdpsnd->waveDataSize = BodySize - 8;
	rdpsnd->wave_timestamp = get_mstime();
	rdpsnd->expectingWave = true;

	DEBUG_SVC("waveDataSize %d wFormatNo %d", rdpsnd->waveDataSize, wFormatNo);

	rdpsnd->close_timestamp = 0;
	if (!rdpsnd->is_open)
	{
		rdpsnd->current_format = wFormatNo;
		rdpsnd->is_open = true;
		if (rdpsnd->device)
			IFCALL(rdpsnd->device->Open, rdpsnd->device, &rdpsnd->supported_formats[wFormatNo],
				rdpsnd->latency);
	}
	else if (wFormatNo != rdpsnd->current_format)
	{
		rdpsnd->current_format = wFormatNo;
		if (rdpsnd->device)
			IFCALL(rdpsnd->device->SetFormat, rdpsnd->device, &rdpsnd->supported_formats[wFormatNo],
				rdpsnd->latency);
	}
}

/* header is not removed from data in this function */
static void rdpsnd_process_message_wave(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	uint16 wTimeStamp;
	uint32 delay_ms;
	uint32 process_ms;
	struct data_out_item* item;

	rdpsnd->expectingWave = 0;
	memcpy(stream_get_head(data_in), rdpsnd->waveData, 4);
	if (stream_get_size(data_in) != rdpsnd->waveDataSize)
	{
		DEBUG_WARN("size error");
		return;
	}
	if (rdpsnd->device)
		IFCALL(rdpsnd->device->Play, rdpsnd->device, stream_get_head(data_in), stream_get_size(data_in));

	process_ms = get_mstime() - rdpsnd->wave_timestamp;
	delay_ms = 250;
	wTimeStamp = rdpsnd->wTimeStamp + delay_ms;

	DEBUG_SVC("data_size %d delay_ms %u process_ms %u",
		stream_get_size(data_in), delay_ms, process_ms);

	item = xnew(struct data_out_item);
	item->data_out = stream_new(8);
	stream_write_uint8(item->data_out, SNDC_WAVECONFIRM);
	stream_write_uint8(item->data_out, 0);
	stream_write_uint16(item->data_out, 4);
	stream_write_uint16(item->data_out, wTimeStamp);
	stream_write_uint8(item->data_out, rdpsnd->cBlockNo); /* cConfirmedBlockNo */
	stream_write_uint8(item->data_out, 0); /* bPad */
	item->out_timestamp = rdpsnd->wave_timestamp + delay_ms;

	list_enqueue(rdpsnd->data_out_list, item);
	rdpsnd->plugin.interval_ms = 10;
}

static void rdpsnd_process_message_close(rdpsndPlugin* rdpsnd)
{
	DEBUG_SVC("server closes.");
	if (rdpsnd->device)
		IFCALL(rdpsnd->device->Start, rdpsnd->device);
	rdpsnd->close_timestamp = get_mstime() + 2000;
	rdpsnd->plugin.interval_ms = 10;
}

static void rdpsnd_process_message_setvolume(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	uint32 dwVolume;

	stream_read_uint32(data_in, dwVolume);
	DEBUG_SVC("dwVolume 0x%X", dwVolume);
	if (rdpsnd->device)
		IFCALL(rdpsnd->device->SetVolume, rdpsnd->device, dwVolume);
}

static void rdpsnd_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	uint8 msgType;
	uint16 BodySize;

	if (rdpsnd->expectingWave)
	{
		rdpsnd_process_message_wave(rdpsnd, data_in);
		return;
	}

	stream_read_uint8(data_in, msgType); /* msgType */
	stream_seek_uint8(data_in); /* bPad */
	stream_read_uint16(data_in, BodySize);

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

static boolean rdpsnd_load_device_plugin(rdpsndPlugin* rdpsnd, const char* name, RDP_PLUGIN_DATA* data)
{
	FREERDP_RDPSND_DEVICE_ENTRY_POINTS entryPoints;
	PFREERDP_RDPSND_DEVICE_ENTRY entry;
	char* fullname;

	if (strrchr(name, '.') != NULL)
		entry = (PFREERDP_RDPSND_DEVICE_ENTRY)freerdp_load_plugin(name, RDPSND_DEVICE_EXPORT_FUNC_NAME);
	else
	{
		fullname = xzalloc(strlen(name) + 8);
		strcpy(fullname, "rdpsnd_");
		strcat(fullname, name);
		entry = (PFREERDP_RDPSND_DEVICE_ENTRY)freerdp_load_plugin(fullname, RDPSND_DEVICE_EXPORT_FUNC_NAME);
		xfree(fullname);
	}
	if (entry == NULL)
	{
		return false;
	}

	entryPoints.rdpsnd = rdpsnd;
	entryPoints.pRegisterRdpsndDevice = rdpsnd_register_device_plugin;
	entryPoints.plugin_data = data;
	if (entry(&entryPoints) != 0)
	{
		DEBUG_WARN("%s entry returns error.", name);
		return false;
	}
	return true;
}

static void rdpsnd_process_plugin_data(rdpsndPlugin* rdpsnd, RDP_PLUGIN_DATA* data)
{
	if (strcmp((char*)data->data[0], "format") == 0)
	{
		rdpsnd->fixed_format = atoi(data->data[1]);
	}
	else if (strcmp((char*)data->data[0], "rate") == 0)
	{
		rdpsnd->fixed_rate = atoi(data->data[1]);
	}
	else if (strcmp((char*)data->data[0], "channel") == 0)
	{
		rdpsnd->fixed_channel = atoi(data->data[1]);
	}
	else if (strcmp((char*)data->data[0], "latency") == 0)
	{
		rdpsnd->latency = atoi(data->data[1]);
	}
	else
	{
		rdpsnd_load_device_plugin(rdpsnd, (char*)data->data[0], data);
	}
}

static void rdpsnd_process_connect(rdpSvcPlugin* plugin)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	RDP_PLUGIN_DATA* data;
	RDP_PLUGIN_DATA default_data[2] = { { 0 }, { 0 } };

	DEBUG_SVC("connecting");

	plugin->interval_callback = rdpsnd_process_interval;

	rdpsnd->data_out_list = list_new();
	rdpsnd->latency = -1;

	data = (RDP_PLUGIN_DATA*)plugin->channel_entry_points.pExtendedData;
	while (data && data->size > 0)
	{
		rdpsnd_process_plugin_data(rdpsnd, data);
		data = (RDP_PLUGIN_DATA*) (((void*) data) + data->size);
	}

	if (rdpsnd->device == NULL)
	{
		default_data[0].size = sizeof(RDP_PLUGIN_DATA);
		default_data[0].data[0] = "pulse";
		default_data[0].data[1] = "";
		if (!rdpsnd_load_device_plugin(rdpsnd, "pulse", default_data))
		{
			default_data[0].data[0] = "alsa";
			default_data[0].data[1] = "default";
			rdpsnd_load_device_plugin(rdpsnd, "alsa", default_data);
		}
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
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*)plugin;
	struct data_out_item* item;

	if (rdpsnd->device)
		IFCALL(rdpsnd->device->Free, rdpsnd->device);

	while ((item = list_dequeue(rdpsnd->data_out_list)) != NULL)
	{
		stream_free(item->data_out);
		xfree(item);
	}
	list_free(rdpsnd->data_out_list);

	rdpsnd_free_supported_formats(rdpsnd);

	xfree(plugin);
}

DEFINE_SVC_PLUGIN(rdpsnd, "rdpsnd",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)

