/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <signal.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/constants.h>
#include <freerdp/channels/log.h>
#include <freerdp/utils/signal.h>

#include "rdpsnd_main.h"

#define TIME_DELAY_MS	65

struct rdpsnd_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP channelEntryPoints;

	HANDLE thread;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	wMessagePipe* MsgPipe;

	wLog* log;
	HANDLE stopEvent;
	HANDLE ScheduleThread;

	BYTE cBlockNo;
	UINT16 wQualityMode;
	int wCurrentFormatNo;

	AUDIO_FORMAT* ServerFormats;
	UINT16 NumberOfServerFormats;

	AUDIO_FORMAT* ClientFormats;
	UINT16 NumberOfClientFormats;

	BOOL expectingWave;
	BYTE waveData[4];
	UINT16 waveDataSize;
	UINT32 wTimeStamp;

	int latency;
	BOOL isOpen;
	UINT16 fixedFormat;
	UINT16 fixedChannel;
	UINT32 fixedRate;

	char* subsystem;
	char* device_name;

	/* Device plugin */
	rdpsndDevicePlugin* device;
};

static void rdpsnd_confirm_wave(rdpsndPlugin* rdpsnd, RDPSND_WAVE* wave);

static void* rdpsnd_schedule_thread(void* arg)
{
	wMessage message;
	UINT16 wTimeDiff;
	UINT16 wTimeStamp;
	UINT16 wCurrentTime;
	RDPSND_WAVE* wave;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;
	HANDLE events[2];

	events[0] = MessageQueue_Event(rdpsnd->MsgPipe->Out);
	events[1] = rdpsnd->stopEvent;

	while (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0)
	{
		if (!MessageQueue_Peek(rdpsnd->MsgPipe->Out, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		wave = (RDPSND_WAVE*) message.wParam;
		wCurrentTime = (UINT16) GetTickCount();
		wTimeStamp = wave->wLocalTimeB;

		if (wCurrentTime <= wTimeStamp)
		{
			wTimeDiff = wTimeStamp - wCurrentTime;
			Sleep(wTimeDiff);
		}

		rdpsnd_confirm_wave(rdpsnd, wave);

		message.wParam = NULL;
		free(wave);
	}

	ExitThread(0);
	return NULL;
}

void rdpsnd_send_quality_mode_pdu(rdpsndPlugin* rdpsnd)
{
	wStream* pdu;

	pdu = Stream_New(NULL, 8);
	Stream_Write_UINT8(pdu, SNDC_QUALITYMODE); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, 4); /* BodySize */
	Stream_Write_UINT16(pdu, rdpsnd->wQualityMode); /* wQualityMode */
	Stream_Write_UINT16(pdu, 0); /* Reserved */

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "QualityMode: %d", rdpsnd->wQualityMode);

	rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

static void rdpsnd_select_supported_audio_formats(rdpsndPlugin* rdpsnd)
{
	int index;
	AUDIO_FORMAT* serverFormat;
	AUDIO_FORMAT* clientFormat;

	rdpsnd_free_audio_formats(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	rdpsnd->NumberOfClientFormats = 0;
	rdpsnd->ClientFormats = NULL;

	if (!rdpsnd->NumberOfServerFormats)
		return;

	rdpsnd->ClientFormats = (AUDIO_FORMAT*) malloc(sizeof(AUDIO_FORMAT) * rdpsnd->NumberOfServerFormats);
	for (index = 0; index < (int) rdpsnd->NumberOfServerFormats; index++)
	{
		serverFormat = &rdpsnd->ServerFormats[index];

		if (rdpsnd->fixedFormat > 0 && (rdpsnd->fixedFormat != serverFormat->wFormatTag))
			continue;

		if (rdpsnd->fixedChannel > 0 && (rdpsnd->fixedChannel != serverFormat->nChannels))
			continue;

		if (rdpsnd->fixedRate > 0 && (rdpsnd->fixedRate != serverFormat->nSamplesPerSec))
			continue;

		if (rdpsnd->device && rdpsnd->device->FormatSupported(rdpsnd->device, serverFormat))
		{
			clientFormat = &rdpsnd->ClientFormats[rdpsnd->NumberOfClientFormats++];

			CopyMemory(clientFormat, serverFormat, sizeof(AUDIO_FORMAT));
			clientFormat->cbSize = 0;

			if (serverFormat->cbSize > 0)
			{
				clientFormat->data = (BYTE*) malloc(serverFormat->cbSize);
				CopyMemory(clientFormat->data, serverFormat->data, serverFormat->cbSize);
				clientFormat->cbSize = serverFormat->cbSize;
			}
		}
	}

#if 0
	WLog_ERR(TAG,  "Server ");
	rdpsnd_print_audio_formats(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	WLog_ERR(TAG,  "");
	WLog_ERR(TAG,  "Client ");
	rdpsnd_print_audio_formats(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	WLog_ERR(TAG,  "");
#endif
}

void rdpsnd_send_client_audio_formats(rdpsndPlugin* rdpsnd)
{
	int index;
	wStream* pdu;
	UINT16 length;
	UINT32 dwVolume;
	UINT16 dwVolumeLeft;
	UINT16 dwVolumeRight;
	UINT16 wNumberOfFormats;
	AUDIO_FORMAT* clientFormat;

	dwVolumeLeft = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolumeRight = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolume = (dwVolumeLeft << 16) | dwVolumeRight;

	if (rdpsnd->device)
	{
		if (rdpsnd->device->GetVolume)
			dwVolume = rdpsnd->device->GetVolume(rdpsnd->device);
	}

	wNumberOfFormats = rdpsnd->NumberOfClientFormats;

	length = 4 + 20;

	for (index = 0; index < (int) wNumberOfFormats; index++)
		length += (18 + rdpsnd->ClientFormats[index].cbSize);

	pdu = Stream_New(NULL, length);

	Stream_Write_UINT8(pdu, SNDC_FORMATS); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, length - 4); /* BodySize */

	Stream_Write_UINT32(pdu, TSSNDCAPS_ALIVE | TSSNDCAPS_VOLUME); /* dwFlags */
	Stream_Write_UINT32(pdu, dwVolume); /* dwVolume */
	Stream_Write_UINT32(pdu, 0); /* dwPitch */
	Stream_Write_UINT16(pdu, 0); /* wDGramPort */
	Stream_Write_UINT16(pdu, wNumberOfFormats); /* wNumberOfFormats */
	Stream_Write_UINT8(pdu, 0); /* cLastBlockConfirmed */
	Stream_Write_UINT16(pdu, 6); /* wVersion */
	Stream_Write_UINT8(pdu, 0); /* bPad */

	for (index = 0; index < (int) wNumberOfFormats; index++)
	{
		clientFormat = &rdpsnd->ClientFormats[index];

		Stream_Write_UINT16(pdu, clientFormat->wFormatTag);
		Stream_Write_UINT16(pdu, clientFormat->nChannels);
		Stream_Write_UINT32(pdu, clientFormat->nSamplesPerSec);
		Stream_Write_UINT32(pdu, clientFormat->nAvgBytesPerSec);
		Stream_Write_UINT16(pdu, clientFormat->nBlockAlign);
		Stream_Write_UINT16(pdu, clientFormat->wBitsPerSample);
		Stream_Write_UINT16(pdu, clientFormat->cbSize);

		if (clientFormat->cbSize > 0)
			Stream_Write(pdu, clientFormat->data, clientFormat->cbSize);
	}

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Client Audio Formats");

	rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

void rdpsnd_recv_server_audio_formats_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	int index;
	UINT16 wVersion;
	AUDIO_FORMAT* format;
	UINT16 wNumberOfFormats;

	rdpsnd_free_audio_formats(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	rdpsnd->NumberOfServerFormats = 0;
	rdpsnd->ServerFormats = NULL;

	/* http://msdn.microsoft.com/en-us/library/cc240956.aspx */
	Stream_Seek_UINT32(s); /* dwFlags */
	Stream_Seek_UINT32(s); /* dwVolume */
	Stream_Seek_UINT32(s); /* dwPitch */
	Stream_Seek_UINT16(s); /* wDGramPort */
	Stream_Read_UINT16(s, wNumberOfFormats);
	Stream_Read_UINT8(s, rdpsnd->cBlockNo); /* cLastBlockConfirmed */
	Stream_Read_UINT16(s, wVersion); /* wVersion */
	Stream_Seek_UINT8(s); /* bPad */

	rdpsnd->NumberOfServerFormats = wNumberOfFormats;
	rdpsnd->ServerFormats = (AUDIO_FORMAT*) malloc(sizeof(AUDIO_FORMAT) * wNumberOfFormats);

	for (index = 0; index < (int) wNumberOfFormats; index++)
	{
		format = &rdpsnd->ServerFormats[index];

		Stream_Read_UINT16(s, format->wFormatTag); /* wFormatTag */
		Stream_Read_UINT16(s, format->nChannels); /* nChannels */
		Stream_Read_UINT32(s, format->nSamplesPerSec); /* nSamplesPerSec */
		Stream_Read_UINT32(s, format->nAvgBytesPerSec); /* nAvgBytesPerSec */
		Stream_Read_UINT16(s, format->nBlockAlign); /* nBlockAlign */
		Stream_Read_UINT16(s, format->wBitsPerSample); /* wBitsPerSample */
		Stream_Read_UINT16(s, format->cbSize); /* cbSize */

		if (format->cbSize > 0)
		{
			format->data = (BYTE*) malloc(format->cbSize);
			Stream_Read(s, format->data, format->cbSize);
		}
		else
		{
			format->data = NULL;
		}
	}

	rdpsnd_select_supported_audio_formats(rdpsnd);

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Server Audio Formats");

	rdpsnd_send_client_audio_formats(rdpsnd);

	if (wVersion >= 6)
		rdpsnd_send_quality_mode_pdu(rdpsnd);
}

void rdpsnd_send_training_confirm_pdu(rdpsndPlugin* rdpsnd, UINT16 wTimeStamp, UINT16 wPackSize)
{
	wStream* pdu;

	pdu = Stream_New(NULL, 8);
	Stream_Write_UINT8(pdu, SNDC_TRAINING); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, 4); /* BodySize */
	Stream_Write_UINT16(pdu, wTimeStamp);
	Stream_Write_UINT16(pdu, wPackSize);

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Training Response: wTimeStamp: %d wPackSize: %d",
			wTimeStamp, wPackSize);

	rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

static void rdpsnd_recv_training_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT16 wTimeStamp;
	UINT16 wPackSize;

	Stream_Read_UINT16(s, wTimeStamp);
	Stream_Read_UINT16(s, wPackSize);

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Training Request: wTimeStamp: %d wPackSize: %d",
			wTimeStamp, wPackSize);

	rdpsnd_send_training_confirm_pdu(rdpsnd, wTimeStamp, wPackSize);
}

static void rdpsnd_recv_wave_info_pdu(rdpsndPlugin* rdpsnd, wStream* s, UINT16 BodySize)
{
	UINT16 wFormatNo;
	AUDIO_FORMAT* format;

	rdpsnd->expectingWave = TRUE;

	Stream_Read_UINT16(s, rdpsnd->wTimeStamp);
	Stream_Read_UINT16(s, wFormatNo);
	Stream_Read_UINT8(s, rdpsnd->cBlockNo);
	Stream_Seek(s, 3); /* bPad */
	Stream_Read(s, rdpsnd->waveData, 4);

	rdpsnd->waveDataSize = BodySize - 8;

	format = &rdpsnd->ClientFormats[wFormatNo];

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "WaveInfo: cBlockNo: %d wFormatNo: %d",
			rdpsnd->cBlockNo, wFormatNo);

	if (!rdpsnd->isOpen)
	{
		rdpsnd->isOpen = TRUE;
		rdpsnd->wCurrentFormatNo = wFormatNo;

		//rdpsnd_print_audio_format(format);

		if (rdpsnd->device)
		{
			IFCALL(rdpsnd->device->Open, rdpsnd->device, format, rdpsnd->latency);
		}
	}
	else if (wFormatNo != rdpsnd->wCurrentFormatNo)
	{
		rdpsnd->wCurrentFormatNo = wFormatNo;

		if (rdpsnd->device)
		{
			IFCALL(rdpsnd->device->SetFormat, rdpsnd->device, format, rdpsnd->latency);
		}
	}
}

void rdpsnd_send_wave_confirm_pdu(rdpsndPlugin* rdpsnd, UINT16 wTimeStamp, BYTE cConfirmedBlockNo)
{
	wStream* pdu;

	pdu = Stream_New(NULL, 8);
	Stream_Write_UINT8(pdu, SNDC_WAVECONFIRM);
	Stream_Write_UINT8(pdu, 0);
	Stream_Write_UINT16(pdu, 4);
	Stream_Write_UINT16(pdu, wTimeStamp);
	Stream_Write_UINT8(pdu, cConfirmedBlockNo); /* cConfirmedBlockNo */
	Stream_Write_UINT8(pdu, 0); /* bPad */

	rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

void rdpsnd_confirm_wave(rdpsndPlugin* rdpsnd, RDPSND_WAVE* wave)
{
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "WaveConfirm: cBlockNo: %d wTimeStamp: %d wTimeDiff: %d",
			wave->cBlockNo, wave->wTimeStampB, wave->wTimeStampB - wave->wTimeStampA);

	rdpsnd_send_wave_confirm_pdu(rdpsnd, wave->wTimeStampB, wave->cBlockNo);
}

static void rdpsnd_device_send_wave_confirm_pdu(rdpsndDevicePlugin* device, RDPSND_WAVE* wave)
{
	if (device->DisableConfirmThread)
		rdpsnd_confirm_wave(device->rdpsnd, wave);
	else
		MessageQueue_Post(device->rdpsnd->MsgPipe->Out, NULL, 0, (void*) wave, NULL);
}

static void rdpsnd_recv_wave_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	int size;
	BYTE* data;
	RDPSND_WAVE* wave;
	AUDIO_FORMAT* format;

	rdpsnd->expectingWave = FALSE;

	/**
	 * The Wave PDU is a special case: it is always sent after a Wave Info PDU,
	 * and we do not process its header. Instead, the header is pad that needs
	 * to be filled with the first four bytes of the audio sample data sent as
	 * part of the preceding Wave Info PDU.
	 */

	CopyMemory(Stream_Buffer(s), rdpsnd->waveData, 4);

	data = Stream_Buffer(s);
	size = (int) Stream_Capacity(s);

	wave = (RDPSND_WAVE*) malloc(sizeof(RDPSND_WAVE));

	wave->wLocalTimeA = GetTickCount();
	wave->wTimeStampA = rdpsnd->wTimeStamp;
	wave->wFormatNo = rdpsnd->wCurrentFormatNo;
	wave->cBlockNo = rdpsnd->cBlockNo;

	wave->data = data;
	wave->length = size;
	wave->AutoConfirm = TRUE;

	format = &rdpsnd->ClientFormats[rdpsnd->wCurrentFormatNo];
	wave->wAudioLength = rdpsnd_compute_audio_time_length(format, size);

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Wave: cBlockNo: %d wTimeStamp: %d",
			wave->cBlockNo, wave->wTimeStampA);

	if (!rdpsnd->device)
	{
		wave->wLocalTimeB = wave->wLocalTimeA;
		wave->wTimeStampB = wave->wTimeStampA;
		rdpsnd_confirm_wave(rdpsnd, wave);
		free(wave);
		return;
	}

	if (rdpsnd->device->WaveDecode)
	{
		IFCALL(rdpsnd->device->WaveDecode, rdpsnd->device, wave);
	}

	if (rdpsnd->device->WavePlay)
	{
		IFCALL(rdpsnd->device->WavePlay, rdpsnd->device, wave);
	}
	else
	{
		IFCALL(rdpsnd->device->Play, rdpsnd->device, data, size);
	}

	if (!rdpsnd->device->WavePlay)
	{
		wave->wTimeStampB = rdpsnd->wTimeStamp + wave->wAudioLength + TIME_DELAY_MS;
		wave->wLocalTimeB = wave->wLocalTimeA + wave->wAudioLength + TIME_DELAY_MS;
	}

	if (wave->AutoConfirm)
		rdpsnd->device->WaveConfirm(rdpsnd->device, wave);
}

static void rdpsnd_recv_close_pdu(rdpsndPlugin* rdpsnd)
{
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Close");

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Close, rdpsnd->device);
	}

	rdpsnd->isOpen = FALSE;
}

static void rdpsnd_recv_volume_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT32 dwVolume;

	Stream_Read_UINT32(s, dwVolume);

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Volume: 0x%04X", dwVolume);

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->SetVolume, rdpsnd->device, dwVolume);
	}
}

static void rdpsnd_recv_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	BYTE msgType;
	UINT16 BodySize;

	if (rdpsnd->expectingWave)
	{
		rdpsnd_recv_wave_pdu(rdpsnd, s);
		Stream_Free(s, TRUE);
		return;
	}

	Stream_Read_UINT8(s, msgType); /* msgType */
	Stream_Seek_UINT8(s); /* bPad */
	Stream_Read_UINT16(s, BodySize);

	//WLog_ERR(TAG,  "msgType %d BodySize %d", msgType, BodySize);

	switch (msgType)
	{
		case SNDC_FORMATS:
			rdpsnd_recv_server_audio_formats_pdu(rdpsnd, s);
			break;

		case SNDC_TRAINING:
			rdpsnd_recv_training_pdu(rdpsnd, s);
			break;

		case SNDC_WAVE:
			rdpsnd_recv_wave_info_pdu(rdpsnd, s, BodySize);
			break;

		case SNDC_CLOSE:
			rdpsnd_recv_close_pdu(rdpsnd);
			break;

		case SNDC_SETVOLUME:
			rdpsnd_recv_volume_pdu(rdpsnd, s);
			break;

		default:
			WLog_ERR(TAG, "unknown msgType %d", msgType);
			break;
	}

	Stream_Free(s, TRUE);
}

static void rdpsnd_register_device_plugin(rdpsndPlugin* rdpsnd, rdpsndDevicePlugin* device)
{
	if (rdpsnd->device)
	{
		WLog_ERR(TAG, "existing device, abort.");
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
		WLog_ERR(TAG, "%s entry returns error.", name);
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
	{ "quality", COMMAND_LINE_VALUE_REQUIRED, "<quality mode>", NULL, NULL, -1, NULL, "quality mode" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void rdpsnd_process_addin_args(rdpsndPlugin* rdpsnd, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;

	rdpsnd->wQualityMode = HIGH_QUALITY; /* default quality mode */

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
			rdpsnd_args, flags, rdpsnd, NULL, NULL);

	if (status < 0)
		return;

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
			rdpsnd->fixedFormat = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "rate")
		{
			rdpsnd->fixedRate = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "channel")
		{
			rdpsnd->fixedChannel = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "latency")
		{
			rdpsnd->latency = atoi(arg->Value);
		}
		CommandLineSwitchCase(arg, "quality")
		{
			int wQualityMode = DYNAMIC_QUALITY;

			if (_stricmp(arg->Value, "dynamic") == 0)
				wQualityMode = DYNAMIC_QUALITY;
			else if (_stricmp(arg->Value, "medium") == 0)
				wQualityMode = MEDIUM_QUALITY;
			else if (_stricmp(arg->Value, "high") == 0)
				wQualityMode = HIGH_QUALITY;
			else
				wQualityMode = atoi(arg->Value);

			if ((wQualityMode < 0) || (wQualityMode > 2))
				wQualityMode = DYNAMIC_QUALITY;

			rdpsnd->wQualityMode = (UINT16) wQualityMode;
		}
		CommandLineSwitchDefault(arg)
		{

		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

static void rdpsnd_process_connect(rdpsndPlugin* rdpsnd)
{
	ADDIN_ARGV* args;

	rdpsnd->latency = -1;

	args = (ADDIN_ARGV*) rdpsnd->channelEntryPoints.pExtendedData;

	if (args)
		rdpsnd_process_addin_args(rdpsnd, args);

	if (rdpsnd->subsystem)
	{
		if (strcmp(rdpsnd->subsystem, "fake") == 0)
		{
			return;
		}

		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}

#if defined(WITH_IOSAUDIO)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "ios");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

#if defined(WITH_OPENSLES)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "opensles");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

#if defined(WITH_PULSE)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "pulse");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

#if defined(WITH_ALSA)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "alsa");
		rdpsnd_set_device_name(rdpsnd, "default");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

#if defined(WITH_MACAUDIO)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "mac");
		rdpsnd_set_device_name(rdpsnd, "default");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

#if defined(WITH_WINMM)
	if (!rdpsnd->device)
	{
		rdpsnd_set_subsystem(rdpsnd, "winmm");
		rdpsnd_set_device_name(rdpsnd, "");
		rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args);
	}
#endif

	if (!rdpsnd->device)
	{
		WLog_ERR(TAG, "no sound device.");
		return;
	}

	if (!rdpsnd->device->DisableConfirmThread)
	{
		rdpsnd->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		rdpsnd->ScheduleThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rdpsnd_schedule_thread,
			(void*) rdpsnd, 0, NULL);
	}
}

static void rdpsnd_process_disconnect(rdpsndPlugin* rdpsnd)
{
	if (rdpsnd->ScheduleThread)
	{
		SetEvent(rdpsnd->stopEvent);
		WaitForSingleObject(rdpsnd->ScheduleThread, INFINITE);
		CloseHandle(rdpsnd->ScheduleThread);
		CloseHandle(rdpsnd->stopEvent);
	}
}

/****************************************************************************************/


static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

void rdpsnd_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* rdpsnd_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void rdpsnd_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

void rdpsnd_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* rdpsnd_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void rdpsnd_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

int rdpsnd_virtual_channel_write(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT32 status = 0;

	if (!rdpsnd)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = rdpsnd->channelEntryPoints.pVirtualChannelWrite(rdpsnd->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	return status;
}

static void rdpsnd_virtual_channel_event_data_received(rdpsndPlugin* plugin,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* s;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (plugin->data_in != NULL)
			Stream_Free(plugin->data_in, TRUE);

		plugin->data_in = Stream_New(NULL, totalLength);
	}

	s = plugin->data_in;
	Stream_EnsureRemainingCapacity(s, (int) dataLength);
	Stream_Write(s, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(s) != Stream_GetPosition(s))
		{
			WLog_ERR(TAG,  "rdpsnd_virtual_channel_event_data_received: read error");
		}

		plugin->data_in = NULL;
		Stream_SealLength(s);
		Stream_SetPosition(s, 0);

		MessageQueue_Post(plugin->MsgPipe->In, NULL, 0, (void*) s, NULL);
	}
}

static VOID VCAPITYPE rdpsnd_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	rdpsndPlugin* rdpsnd;

	rdpsnd = (rdpsndPlugin*) rdpsnd_get_open_handle_data(openHandle);

	if (!rdpsnd)
	{
		WLog_ERR(TAG,  "rdpsnd_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			rdpsnd_virtual_channel_event_data_received(rdpsnd, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* rdpsnd_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;

	rdpsnd_process_connect(rdpsnd);

	while (1)
	{
		if (!MessageQueue_Wait(rdpsnd->MsgPipe->In))
			break;

		if (MessageQueue_Peek(rdpsnd->MsgPipe->In, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				rdpsnd_recv_pdu(rdpsnd, data);
			}
		}
	}

	rdpsnd_process_disconnect(rdpsnd);

	ExitThread(0);
	return NULL;
}

static void rdpsnd_virtual_channel_event_connected(rdpsndPlugin* plugin, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = plugin->channelEntryPoints.pVirtualChannelOpen(plugin->InitHandle,
		&plugin->OpenHandle, plugin->channelDef.name, rdpsnd_virtual_channel_open_event);

	rdpsnd_add_open_handle_data(plugin->OpenHandle, plugin);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	plugin->MsgPipe = MessagePipe_New();

	plugin->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rdpsnd_virtual_channel_client_thread, (void*) plugin, 0, NULL);
}

static void rdpsnd_virtual_channel_event_disconnected(rdpsndPlugin* rdpsnd)
{
	UINT rc;

	MessagePipe_PostQuit(rdpsnd->MsgPipe, 0);
	WaitForSingleObject(rdpsnd->thread, INFINITE);

	CloseHandle(rdpsnd->thread);
	rdpsnd->thread = NULL;

	rc = rdpsnd->channelEntryPoints.pVirtualChannelClose(rdpsnd->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	}

	if (rdpsnd->data_in)
	{
		Stream_Free(rdpsnd->data_in, TRUE);
		rdpsnd->data_in = NULL;
	}

	MessagePipe_Free(rdpsnd->MsgPipe);
	rdpsnd->MsgPipe = NULL;

	rdpsnd_free_audio_formats(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	rdpsnd->NumberOfClientFormats = 0;
	rdpsnd->ClientFormats = NULL;

	rdpsnd_free_audio_formats(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	rdpsnd->NumberOfServerFormats = 0;
	rdpsnd->ServerFormats = NULL;

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Free, rdpsnd->device);
		rdpsnd->device = NULL;
	}

	if (rdpsnd->subsystem)
	{
		free(rdpsnd->subsystem);
		rdpsnd->subsystem = NULL;
	}

	if (rdpsnd->device_name)
	{
		free(rdpsnd->device_name);
		rdpsnd->device_name = NULL;
	}

	rdpsnd_remove_open_handle_data(rdpsnd->OpenHandle);
}

static void rdpsnd_virtual_channel_event_terminated(rdpsndPlugin* rdpsnd)
{
	rdpsnd_remove_init_handle_data(rdpsnd->InitHandle);

	free(rdpsnd);
}

static VOID VCAPITYPE rdpsnd_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	rdpsndPlugin* plugin;

	plugin = (rdpsndPlugin*) rdpsnd_get_init_handle_data(pInitHandle);

	if (!plugin)
	{
		WLog_ERR(TAG,  "rdpsnd_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			rdpsnd_virtual_channel_event_connected(plugin, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			rdpsnd_virtual_channel_event_disconnected(plugin);
			break;

		case CHANNEL_EVENT_TERMINATED:
			rdpsnd_virtual_channel_event_terminated(plugin);
			break;
	}
}

/* rdpsnd is always built-in */
#define VirtualChannelEntry	rdpsnd_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;

	rdpsndPlugin* rdpsnd;

	rdpsnd = (rdpsndPlugin*) calloc(1, sizeof(rdpsndPlugin));

	if (rdpsnd)
	{
#if !defined(_WIN32) && !defined(ANDROID)
		{
			sigset_t mask;
			sigemptyset(&mask);
			sigaddset(&mask, SIGIO);
			pthread_sigmask(SIG_BLOCK, &mask, NULL);
		}
#endif

		rdpsnd->channelDef.options =
				CHANNEL_OPTION_INITIALIZED |
				CHANNEL_OPTION_ENCRYPT_RDP;

		strcpy(rdpsnd->channelDef.name, "rdpsnd");

		CopyMemory(&(rdpsnd->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

		rdpsnd->log = WLog_Get("com.freerdp.channels.rdpsnd.client");
		
		rc = rdpsnd->channelEntryPoints.pVirtualChannelInit(&rdpsnd->InitHandle,
			&rdpsnd->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, rdpsnd_virtual_channel_init_event);
		if (CHANNEL_RC_OK != rc)
		{
			WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
					 WTSErrorToString(rc), rc);
			free(rdpsnd);
			return -1;
		}

		rdpsnd_add_init_handle_data(rdpsnd->InitHandle, (void*) rdpsnd);
	}

	return 1;
}
