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
#include <winpr/synch.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/constants.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/signal.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

#define TIME_DELAY_MS	250

struct rdpsnd_plugin
{
	rdpSvcPlugin plugin;

	wMessagePipe* MsgPipe;
	HANDLE thread;

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

static void* rdpsnd_schedule_thread(void* arg)
{
	STREAM* data;
	wMessage message;
	UINT16 wTimeDiff;
	UINT16 wTimeStamp;
	UINT16 wCurrentTime;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(rdpsnd->MsgPipe->Out))
			break;

		if (!MessageQueue_Peek(rdpsnd->MsgPipe->Out, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		wTimeStamp = (UINT16) (size_t) message.lParam;
		wCurrentTime = (UINT16) GetTickCount();

		//printf("wTimeStamp: %d wCurrentTime: %d\n", wTimeStamp, wCurrentTime);

		if (wCurrentTime <= wTimeStamp)
		{
			wTimeDiff = wTimeStamp - wCurrentTime;
			//printf("Sleeping %d ms\n", wTimeDiff);
			Sleep(wTimeDiff);
		}

		data = (STREAM*) message.wParam;
		svc_plugin_send((rdpSvcPlugin*) rdpsnd, data);
		DEBUG_SVC("processed output data");
	}

#if 0
	if (rdpsnd->is_open && (rdpsnd->close_timestamp > 0))
	{
		if (GetTickCount() > rdpsnd->close_timestamp)
		{
			if (rdpsnd->device)
				IFCALL(rdpsnd->device->Close, rdpsnd->device);

			rdpsnd->is_open = FALSE;
			rdpsnd->close_timestamp = 0;

			DEBUG_SVC("processed close");
		}
	}
#endif

	return NULL;
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

void rdpsnd_send_quality_mode_pdu(rdpsndPlugin* rdpsnd)
{
	STREAM* pdu;

	pdu = stream_new(8);
	stream_write_BYTE(pdu, SNDC_QUALITYMODE); /* msgType */
	stream_write_BYTE(pdu, 0); /* bPad */
	stream_write_UINT16(pdu, 4); /* BodySize */
	stream_write_UINT16(pdu, HIGH_QUALITY); /* wQualityMode */
	stream_write_UINT16(pdu, 0); /* Reserved */

	svc_plugin_send((rdpSvcPlugin*) rdpsnd, pdu);
}

static void rdpsnd_recv_formats_pdu(rdpsndPlugin* rdpsnd, STREAM* s)
{
	int pos;
	STREAM* pdu;
	UINT32 dwVolume;
	UINT16 dwVolumeLeft;
	UINT16 dwVolumeRight;
	UINT16 wNumberOfFormats;
	UINT16 nFormat;
	UINT16 wVersion;
	BYTE* data_mark;
	BYTE* format_mark;
	rdpsndFormat* format;
	UINT16 n_out_formats;
	rdpsndFormat* out_formats;

	rdpsnd_free_supported_formats(rdpsnd);

	stream_seek_UINT32(s); /* dwFlags */
	stream_seek_UINT32(s); /* dwVolume */
	stream_seek_UINT32(s); /* dwPitch */
	stream_seek_UINT16(s); /* wDGramPort */
	stream_read_UINT16(s, wNumberOfFormats);
	stream_read_BYTE(s, rdpsnd->cBlockNo); /* cLastBlockConfirmed */
	stream_read_UINT16(s, wVersion);
	stream_seek_BYTE(s); /* bPad */

	DEBUG_SVC("wNumberOfFormats %d wVersion %d", wNumberOfFormats, wVersion);

	if (wNumberOfFormats < 1)
	{
		DEBUG_WARN("wNumberOfFormats is 0");
		return;
	}

	out_formats = (rdpsndFormat*) malloc(wNumberOfFormats * sizeof(rdpsndFormat));
	ZeroMemory(out_formats, wNumberOfFormats * sizeof(rdpsndFormat));
	n_out_formats = 0;

	dwVolumeLeft = (0xFFFF / 2); /* 50% ? */
	dwVolumeRight = (0xFFFF / 2); /* 50% ? */
	dwVolume = (dwVolumeLeft << 16) | dwVolumeRight;

	pdu = stream_new(24);
	stream_write_BYTE(pdu, SNDC_FORMATS); /* msgType */
	stream_write_BYTE(pdu, 0); /* bPad */
	stream_seek_UINT16(pdu); /* BodySize */
	stream_write_UINT32(pdu, TSSNDCAPS_ALIVE | TSSNDCAPS_VOLUME); /* dwFlags */
	stream_write_UINT32(pdu, dwVolume); /* dwVolume */
	stream_write_UINT32(pdu, 0); /* dwPitch */
	stream_write_UINT16_be(pdu, 0); /* wDGramPort */
	stream_seek_UINT16(pdu); /* wNumberOfFormats */
	stream_write_BYTE(pdu, 0); /* cLastBlockConfirmed */
	stream_write_UINT16(pdu, 6); /* wVersion */
	stream_write_BYTE(pdu, 0); /* bPad */

	for (nFormat = 0; nFormat < wNumberOfFormats; nFormat++)
	{
		stream_get_mark(s, format_mark);
		format = &out_formats[n_out_formats];
		stream_read_UINT16(s, format->wFormatTag);
		stream_read_UINT16(s, format->nChannels);
		stream_read_UINT32(s, format->nSamplesPerSec);
		stream_seek_UINT32(s); /* nAvgBytesPerSec */
		stream_read_UINT16(s, format->nBlockAlign);
		stream_read_UINT16(s, format->wBitsPerSample);
		stream_read_UINT16(s, format->cbSize);
		stream_get_mark(s, data_mark);
		stream_seek(s, format->cbSize);
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

			stream_check_size(pdu, 18 + format->cbSize);
			stream_write(pdu, format_mark, 18 + format->cbSize);

			if (format->cbSize > 0)
			{
				format->data = malloc(format->cbSize);
				CopyMemory(format->data, data_mark, format->cbSize);
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

	pos = stream_get_pos(pdu);
	stream_set_pos(pdu, 2);
	stream_write_UINT16(pdu, pos - 4);
	stream_set_pos(pdu, 18);
	stream_write_UINT16(pdu, n_out_formats);
	stream_set_pos(pdu, pos);

	svc_plugin_send((rdpSvcPlugin*) rdpsnd, pdu);

	if (wVersion >= 6)
		rdpsnd_send_quality_mode_pdu(rdpsnd);
}

void rdpsnd_send_training_confirm_pdu(rdpsndPlugin* rdpsnd, UINT16 wTimeStamp, UINT16 wPackSize)
{
	STREAM* pdu;

	pdu = stream_new(8);
	stream_write_BYTE(pdu, SNDC_TRAINING); /* msgType */
	stream_write_BYTE(pdu, 0); /* bPad */
	stream_write_UINT16(pdu, 4); /* BodySize */
	stream_write_UINT16(pdu, wTimeStamp);
	stream_write_UINT16(pdu, wPackSize);

	svc_plugin_send((rdpSvcPlugin*) rdpsnd, pdu);
}

static void rdpsnd_recv_training_pdu(rdpsndPlugin* rdpsnd, STREAM* s)
{
	UINT16 wTimeStamp;
	UINT16 wPackSize;

	stream_read_UINT16(s, wTimeStamp);
	stream_read_UINT16(s, wPackSize);

	rdpsnd_send_training_confirm_pdu(rdpsnd, wTimeStamp, wPackSize);
}

static void rdpsnd_recv_wave_info_pdu(rdpsndPlugin* rdpsnd, STREAM* data_in, UINT16 BodySize)
{
	UINT16 wFormatNo;

	stream_read_UINT16(data_in, rdpsnd->wTimeStamp);
	stream_read_UINT16(data_in, wFormatNo);
	stream_read_BYTE(data_in, rdpsnd->cBlockNo);
	stream_seek(data_in, 3); /* bPad */
	stream_read(data_in, rdpsnd->waveData, 4);

	rdpsnd->waveDataSize = BodySize - 8;
	rdpsnd->wave_timestamp = GetTickCount();
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

void rdpsnd_send_wave_confirm_pdu(rdpsndPlugin* rdpsnd, UINT16 wTimeStamp, BYTE cConfirmedBlockNo)
{
	STREAM* pdu;

	pdu = stream_new(8);
	stream_write_BYTE(pdu, SNDC_WAVECONFIRM);
	stream_write_BYTE(pdu, 0);
	stream_write_UINT16(pdu, 4);
	stream_write_UINT16(pdu, wTimeStamp);
	stream_write_BYTE(pdu, cConfirmedBlockNo); /* cConfirmedBlockNo */
	stream_write_BYTE(pdu, 0); /* bPad */

	svc_plugin_send((rdpSvcPlugin*) rdpsnd, pdu);
}

void rdpsnd_device_send_wave_confirm_pdu(rdpsndDevicePlugin* device, UINT16 wTimeStamp, BYTE cConfirmedBlockNo)
{
	rdpsnd_send_wave_confirm_pdu(device->rdpsnd, wTimeStamp, cConfirmedBlockNo);
}

static void rdpsnd_recv_wave_pdu(rdpsndPlugin* rdpsnd, STREAM* s)
{
	STREAM* data;
	UINT16 wTimeStamp;

	rdpsnd->expectingWave = FALSE;

	/**
	 * The Wave PDU is a special case: it is always sent after a Wave Info PDU,
	 * and we do not process its header. Instead, the header is pad that needs
	 * to be filled with the first four bytes of the audio sample data sent as
	 * part of the preceding Wave Info PDU.
	 */

	CopyMemory(stream_get_head(s), rdpsnd->waveData, 4);

	if (stream_get_size(s) != rdpsnd->waveDataSize)
	{
		DEBUG_WARN("size error");
		return;
	}

	if (rdpsnd->device)
	{
		if (rdpsnd->device->WavePlay)
		{
			IFCALL(rdpsnd->device->WavePlay, rdpsnd->device, rdpsnd->wTimeStamp,
					rdpsnd->current_format, rdpsnd->cBlockNo, stream_get_head(s), stream_get_size(s));
		}
		else
		{
			IFCALL(rdpsnd->device->Play, rdpsnd->device, stream_get_head(s), stream_get_size(s));
		}
	}

	wTimeStamp = rdpsnd->wTimeStamp + TIME_DELAY_MS;

	data = stream_new(8);
	stream_write_BYTE(data, SNDC_WAVECONFIRM);
	stream_write_BYTE(data, 0);
	stream_write_UINT16(data, 4);
	stream_write_UINT16(data, wTimeStamp);
	stream_write_BYTE(data, rdpsnd->cBlockNo); /* cConfirmedBlockNo */
	stream_write_BYTE(data, 0); /* bPad */

	wTimeStamp = rdpsnd->wave_timestamp + TIME_DELAY_MS;
	MessageQueue_Post(rdpsnd->MsgPipe->Out, NULL, 0, (void*) data, (void*) (size_t) wTimeStamp);
}

static void rdpsnd_recv_close_pdu(rdpsndPlugin* rdpsnd)
{
	DEBUG_SVC("server closes.");

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Start, rdpsnd->device);
	}

	rdpsnd->close_timestamp = GetTickCount() + 2000;
}

static void rdpsnd_recv_volume_pdu(rdpsndPlugin* rdpsnd, STREAM* data_in)
{
	UINT32 dwVolume;

	stream_read_UINT32(data_in, dwVolume);
	DEBUG_SVC("dwVolume 0x%X", dwVolume);

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->SetVolume, rdpsnd->device, dwVolume);
	}
}

static void rdpsnd_recv_pdu(rdpSvcPlugin* plugin, STREAM* data_in)
{
	BYTE msgType;
	UINT16 BodySize;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) plugin;

	if (rdpsnd->expectingWave)
	{
		rdpsnd_recv_wave_pdu(rdpsnd, data_in);
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
			rdpsnd_recv_formats_pdu(rdpsnd, data_in);
			break;

		case SNDC_TRAINING:
			rdpsnd_recv_training_pdu(rdpsnd, data_in);
			break;

		case SNDC_WAVE:
			rdpsnd_recv_wave_info_pdu(rdpsnd, data_in, BodySize);
			break;

		case SNDC_CLOSE:
			rdpsnd_recv_close_pdu(rdpsnd);
			break;

		case SNDC_SETVOLUME:
			rdpsnd_recv_volume_pdu(rdpsnd, data_in);
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
	device->rdpsnd = rdpsnd;

	device->WaveConfirm = rdpsnd_device_send_wave_confirm_pdu;
}

static BOOL rdpsnd_load_device_plugin(rdpsndPlugin* rdpsnd, const char* name, ADDIN_ARGV* args)
{
	PFREERDP_RDPSND_DEVICE_ENTRY entry;
	FREERDP_RDPSND_DEVICE_ENTRY_POINTS entryPoints;

	entry = (PFREERDP_RDPSND_DEVICE_ENTRY) freerdp_load_channel_addin_entry("rdpsnd", (LPSTR) name, NULL, 0);

	if (!entry)
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

	rdpsnd->latency = -1;
	rdpsnd->MsgPipe = MessagePipe_New();
	rdpsnd->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) rdpsnd_schedule_thread, (void*) plugin, 0, NULL);

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

	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "winmm");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

	if (!rdpsnd->device)
	{
		DEBUG_WARN("no sound device.");
		return;
	}
}

static void rdpsnd_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	freerdp_event_free(event);
}

static void rdpsnd_process_terminate(rdpSvcPlugin* plugin)
{
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) plugin;

	if (rdpsnd->device)
		IFCALL(rdpsnd->device->Free, rdpsnd->device);

	MessagePipe_Free(rdpsnd->MsgPipe);

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
	_p->plugin.receive_callback = rdpsnd_recv_pdu;
	_p->plugin.event_callback = rdpsnd_process_event;
	_p->plugin.terminate_callback = rdpsnd_process_terminate;

#ifndef _WIN32
	{
		sigset_t mask;
		sigemptyset(&mask);
		sigaddset(&mask, SIGIO);
		pthread_sigmask(SIG_BLOCK, &mask, NULL);
	}
#endif

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
