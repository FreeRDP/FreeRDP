/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>

#include "rdpsnd_common.h"
#include "rdpsnd_main.h"

struct rdpsnd_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntryPoints;

	HANDLE thread;
	wStreamPool* pool;
	wStream* data_in;

	void* InitHandle;
	DWORD OpenHandle;

	wLog* log;
	HANDLE stopEvent;

	BYTE cBlockNo;
	UINT16 wQualityMode;
	UINT16 wCurrentFormatNo;

	AUDIO_FORMAT* ServerFormats;
	UINT16 NumberOfServerFormats;

	AUDIO_FORMAT* ClientFormats;
	UINT16 NumberOfClientFormats;

	BOOL attached;
	BOOL connected;

	BOOL expectingWave;
	BYTE waveData[4];
	UINT16 waveDataSize;
	UINT32 wTimeStamp;
	UINT32 wArrivalTime;

	UINT32 latency;
	BOOL isOpen;
	AUDIO_FORMAT* fixed_format;

	char* subsystem;
	char* device_name;

	/* Device plugin */
	rdpsndDevicePlugin* device;
	rdpContext* rdpcontext;

	wQueue* queue;
	FREERDP_DSP_CONTEXT* dsp_context;
};

static void rdpsnd_recv_close_pdu(rdpsndPlugin* rdpsnd);
static void rdpsnd_virtual_channel_event_terminated(rdpsndPlugin* rdpsnd);

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_virtual_channel_write(rdpsndPlugin* rdpsnd, wStream* s);

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_send_quality_mode_pdu(rdpsndPlugin* rdpsnd)
{
	wStream* pdu;
	pdu = Stream_New(NULL, 8);

	if (!pdu)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(pdu, SNDC_QUALITYMODE); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, 4); /* BodySize */
	Stream_Write_UINT16(pdu, rdpsnd->wQualityMode); /* wQualityMode */
	Stream_Write_UINT16(pdu, 0); /* Reserved */
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "QualityMode: %"PRIu16"", rdpsnd->wQualityMode);
	return rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

static void rdpsnd_select_supported_audio_formats(rdpsndPlugin* rdpsnd)
{
	UINT16 index;
	audio_formats_free(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	rdpsnd->NumberOfClientFormats = 0;
	rdpsnd->ClientFormats = NULL;

	if (!rdpsnd->NumberOfServerFormats)
		return;

	rdpsnd->ClientFormats = audio_formats_new(rdpsnd->NumberOfServerFormats);

	if (!rdpsnd->ClientFormats)
		return;

	for (index = 0; index < rdpsnd->NumberOfServerFormats; index++)
	{
		const AUDIO_FORMAT* serverFormat = &rdpsnd->ServerFormats[index];

		if (!audio_format_compatible(rdpsnd->fixed_format, serverFormat))
			continue;

		if (freerdp_dsp_supports_format(serverFormat, FALSE) ||
		    rdpsnd->device->FormatSupported(rdpsnd->device, serverFormat))
		{
			AUDIO_FORMAT* clientFormat = &rdpsnd->ClientFormats[rdpsnd->NumberOfClientFormats++];
			audio_format_copy(serverFormat, clientFormat);
		}
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_send_client_audio_formats(rdpsndPlugin* rdpsnd)
{
	UINT16 index;
	wStream* pdu;
	UINT16 length;
	UINT32 dwVolume;
	UINT16 wNumberOfFormats;
	dwVolume = IFCALLRESULT(0, rdpsnd->device->GetVolume, rdpsnd->device);
	wNumberOfFormats = rdpsnd->NumberOfClientFormats;
	length = 4 + 20;

	for (index = 0; index < wNumberOfFormats; index++)
		length += (18 + rdpsnd->ClientFormats[index].cbSize);

	pdu = Stream_New(NULL, length);

	if (!pdu)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(pdu, SNDC_FORMATS); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, length - 4); /* BodySize */
	Stream_Write_UINT32(pdu, TSSNDCAPS_ALIVE | TSSNDCAPS_VOLUME); /* dwFlags */
	Stream_Write_UINT32(pdu, dwVolume); /* dwVolume */
	Stream_Write_UINT32(pdu, 0); /* dwPitch */
	Stream_Write_UINT16(pdu, 0); /* wDGramPort */
	Stream_Write_UINT16(pdu, wNumberOfFormats); /* wNumberOfFormats */
	Stream_Write_UINT8(pdu, 0); /* cLastBlockConfirmed */
	Stream_Write_UINT16(pdu, CHANNEL_VERSION_WIN_MAX); /* wVersion */
	Stream_Write_UINT8(pdu, 0); /* bPad */

	for (index = 0; index < wNumberOfFormats; index++)
	{
		const AUDIO_FORMAT* clientFormat = &rdpsnd->ClientFormats[index];

		if (!audio_format_write(pdu, clientFormat))
		{
			Stream_Free(pdu, TRUE);
			return ERROR_INTERNAL_ERROR;
		}
	}

	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Client Audio Formats");
	return rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_server_audio_formats_pdu(rdpsndPlugin* rdpsnd,
        wStream* s)
{
	UINT16 index;
	UINT16 wVersion;
	UINT16 wNumberOfFormats;
	UINT ret = ERROR_BAD_LENGTH;
	audio_formats_free(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	rdpsnd->NumberOfServerFormats = 0;
	rdpsnd->ServerFormats = NULL;

	if (Stream_GetRemainingLength(s) < 30)
		return ERROR_BAD_LENGTH;

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

	if (Stream_GetRemainingLength(s) / 14 < wNumberOfFormats)
		return ERROR_BAD_LENGTH;

	rdpsnd->ServerFormats = audio_formats_new(wNumberOfFormats);

	if (!rdpsnd->ServerFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (index = 0; index < wNumberOfFormats; index++)
	{
		AUDIO_FORMAT* format = &rdpsnd->ServerFormats[index];

		if (!audio_format_read(s, format))
			goto out_fail;
	}

	rdpsnd_select_supported_audio_formats(rdpsnd);
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Server Audio Formats");
	ret = rdpsnd_send_client_audio_formats(rdpsnd);

	if (ret == CHANNEL_RC_OK)
	{
		if (wVersion >= CHANNEL_VERSION_WIN_7)
			ret = rdpsnd_send_quality_mode_pdu(rdpsnd);
	}

	return ret;
out_fail:
	audio_formats_free(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	rdpsnd->ServerFormats = NULL;
	rdpsnd->NumberOfServerFormats = 0;
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_send_training_confirm_pdu(rdpsndPlugin* rdpsnd,
        UINT16 wTimeStamp, UINT16 wPackSize)
{
	wStream* pdu;
	pdu = Stream_New(NULL, 8);

	if (!pdu)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(pdu, SNDC_TRAINING); /* msgType */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	Stream_Write_UINT16(pdu, 4); /* BodySize */
	Stream_Write_UINT16(pdu, wTimeStamp);
	Stream_Write_UINT16(pdu, wPackSize);
	WLog_Print(rdpsnd->log, WLOG_DEBUG,
	           "Training Response: wTimeStamp: %"PRIu16" wPackSize: %"PRIu16"",
	           wTimeStamp, wPackSize);
	return rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_training_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT16 wTimeStamp;
	UINT16 wPackSize;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_BAD_LENGTH;

	Stream_Read_UINT16(s, wTimeStamp);
	Stream_Read_UINT16(s, wPackSize);
	WLog_Print(rdpsnd->log, WLOG_DEBUG,
	           "Training Request: wTimeStamp: %"PRIu16" wPackSize: %"PRIu16"",
	           wTimeStamp, wPackSize);
	return rdpsnd_send_training_confirm_pdu(rdpsnd, wTimeStamp, wPackSize);
}

static BOOL rdpsnd_ensure_device_is_open(rdpsndPlugin* rdpsnd, UINT32 wFormatNo,
        const AUDIO_FORMAT* format)
{
	if (!rdpsnd)
		return FALSE;

	if (!rdpsnd->isOpen || (wFormatNo != rdpsnd->wCurrentFormatNo))
	{
		BOOL rc;
		BOOL supported;
		AUDIO_FORMAT deviceFormat = *format;
		rdpsnd_recv_close_pdu(rdpsnd);
		supported = IFCALLRESULT(FALSE, rdpsnd->device->FormatSupported, rdpsnd->device, format);

		if (!supported)
		{
			deviceFormat.wFormatTag = WAVE_FORMAT_PCM;
			deviceFormat.wBitsPerSample = 16;
			deviceFormat.cbSize = 0;
		}

		WLog_Print(rdpsnd->log, WLOG_DEBUG, "Opening device with format %s [backend %s]",
		           audio_format_get_tag_string(format->wFormatTag),
		           audio_format_get_tag_string(deviceFormat.wFormatTag));
		rc = IFCALLRESULT(FALSE, rdpsnd->device->Open, rdpsnd->device, &deviceFormat, rdpsnd->latency);

		if (!rc)
			return FALSE;

		if (!supported)
		{
			if (!freerdp_dsp_context_reset(rdpsnd->dsp_context, format))
				return FALSE;
		}

		rdpsnd->isOpen = TRUE;
		rdpsnd->wCurrentFormatNo = wFormatNo;
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_wave_info_pdu(rdpsndPlugin* rdpsnd, wStream* s,
                                      UINT16 BodySize)
{
	UINT16 wFormatNo;
	const AUDIO_FORMAT* format;

	if (Stream_GetRemainingLength(s) < 12)
		return ERROR_BAD_LENGTH;

	rdpsnd->wArrivalTime = GetTickCount();
	Stream_Read_UINT16(s, rdpsnd->wTimeStamp);
	Stream_Read_UINT16(s, wFormatNo);

	if (wFormatNo >= rdpsnd->NumberOfClientFormats)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, rdpsnd->cBlockNo);
	Stream_Seek(s, 3); /* bPad */
	Stream_Read(s, rdpsnd->waveData, 4);
	rdpsnd->waveDataSize = BodySize - 8;
	format = &rdpsnd->ClientFormats[wFormatNo];
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "WaveInfo: cBlockNo: %"PRIu8" wFormatNo: %"PRIu16" [%s]",
	           rdpsnd->cBlockNo, wFormatNo, audio_format_get_tag_string(format->wFormatTag));

	if (!rdpsnd_ensure_device_is_open(rdpsnd, wFormatNo, format))
		return ERROR_INTERNAL_ERROR;

	rdpsnd->expectingWave = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_send_wave_confirm_pdu(rdpsndPlugin* rdpsnd,
        UINT16 wTimeStamp, BYTE cConfirmedBlockNo)
{
	wStream* pdu;
	pdu = Stream_New(NULL, 8);

	if (!pdu)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT8(pdu, SNDC_WAVECONFIRM);
	Stream_Write_UINT8(pdu, 0);
	Stream_Write_UINT16(pdu, 4);
	Stream_Write_UINT16(pdu, wTimeStamp);
	Stream_Write_UINT8(pdu, cConfirmedBlockNo); /* cConfirmedBlockNo */
	Stream_Write_UINT8(pdu, 0); /* bPad */
	return rdpsnd_virtual_channel_write(rdpsnd, pdu);
}

static UINT rdpsnd_treat_wave(rdpsndPlugin* rdpsnd, wStream* s, size_t size)
{
	BYTE* data;
	AUDIO_FORMAT* format;
	DWORD end;
	DWORD diffMS;
	UINT latency = 0;

	if (Stream_GetRemainingLength(s) < size)
		return ERROR_BAD_LENGTH;

	data = Stream_Pointer(s);
	format = &rdpsnd->ClientFormats[rdpsnd->wCurrentFormatNo];
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Wave: cBlockNo: %"PRIu8" wTimeStamp: %"PRIu16", size: %"PRIdz,
	           rdpsnd->cBlockNo, rdpsnd->wTimeStamp, size);

	if (rdpsnd->device && rdpsnd->attached)
	{
		UINT status = CHANNEL_RC_OK;
		wStream* pcmData = StreamPool_Take(rdpsnd->pool, 4096);

		if (rdpsnd->device->FormatSupported(rdpsnd->device, format))
			latency = IFCALLRESULT(0, rdpsnd->device->Play, rdpsnd->device, data, size);
		else if (freerdp_dsp_decode(rdpsnd->dsp_context, format, data, size, pcmData))
		{
			Stream_SealLength(pcmData);
			latency = IFCALLRESULT(0, rdpsnd->device->Play, rdpsnd->device, Stream_Buffer(pcmData),
			                       Stream_Length(pcmData));
		}
		else
			status = ERROR_INTERNAL_ERROR;

		StreamPool_Return(rdpsnd->pool, pcmData);

		if (status != CHANNEL_RC_OK)
			return status;
	}

	end = GetTickCount();
	diffMS = end - rdpsnd->wArrivalTime + latency;
	return rdpsnd_send_wave_confirm_pdu(rdpsnd, rdpsnd->wTimeStamp + diffMS, rdpsnd->cBlockNo);
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_wave_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	rdpsnd->expectingWave = FALSE;

	/**
	 * The Wave PDU is a special case: it is always sent after a Wave Info PDU,
	 * and we do not process its header. Instead, the header is pad that needs
	 * to be filled with the first four bytes of the audio sample data sent as
	 * part of the preceding Wave Info PDU.
	 */
	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	CopyMemory(Stream_Buffer(s), rdpsnd->waveData, 4);
	return rdpsnd_treat_wave(rdpsnd, s, rdpsnd->waveDataSize);
}

static UINT rdpsnd_recv_wave2_pdu(rdpsndPlugin* rdpsnd, wStream* s, UINT16 BodySize)
{
	UINT16 wFormatNo;
	AUDIO_FORMAT* format;
	UINT32 dwAudioTimeStamp;

	if (Stream_GetRemainingLength(s) < 12)
		return ERROR_BAD_LENGTH;

	Stream_Read_UINT16(s, rdpsnd->wTimeStamp);
	Stream_Read_UINT16(s, wFormatNo);
	Stream_Read_UINT8(s, rdpsnd->cBlockNo);
	Stream_Seek(s, 3); /* bPad */
	Stream_Read_UINT32(s, dwAudioTimeStamp);
	rdpsnd->waveDataSize = BodySize - 12;
	format = &rdpsnd->ClientFormats[wFormatNo];
	rdpsnd->wArrivalTime = GetTickCount();
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Wave2PDU: cBlockNo: %"PRIu8" wFormatNo: %"PRIu16", align=%hu",
	           rdpsnd->cBlockNo, wFormatNo, format->nBlockAlign);

	if (!rdpsnd_ensure_device_is_open(rdpsnd, wFormatNo, format))
		return ERROR_INTERNAL_ERROR;

	return rdpsnd_treat_wave(rdpsnd, s, rdpsnd->waveDataSize);
}

static void rdpsnd_recv_close_pdu(rdpsndPlugin* rdpsnd)
{
	if (rdpsnd->isOpen)
	{
		WLog_Print(rdpsnd->log, WLOG_DEBUG, "Closing device");
		IFCALL(rdpsnd->device->Close, rdpsnd->device);
		rdpsnd->isOpen = FALSE;
	}
	else
		WLog_Print(rdpsnd->log, WLOG_DEBUG, "Device already closed");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_volume_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	BOOL rc;
	UINT32 dwVolume;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_BAD_LENGTH;

	Stream_Read_UINT32(s, dwVolume);
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Volume: 0x%08"PRIX32"", dwVolume);
	rc = IFCALLRESULT(FALSE, rdpsnd->device->SetVolume, rdpsnd->device, dwVolume);

	if (!rc)
	{
		WLog_ERR(TAG, "error setting volume");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	BYTE msgType;
	UINT16 BodySize;
	UINT status = CHANNEL_RC_OK;

	if (rdpsnd->expectingWave)
	{
		status = rdpsnd_recv_wave_pdu(rdpsnd, s);
		goto out;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		status = ERROR_BAD_LENGTH;
		goto out;
	}

	Stream_Read_UINT8(s, msgType); /* msgType */
	Stream_Seek_UINT8(s); /* bPad */
	Stream_Read_UINT16(s, BodySize);

	switch (msgType)
	{
		case SNDC_FORMATS:
			status = rdpsnd_recv_server_audio_formats_pdu(rdpsnd, s);
			break;

		case SNDC_TRAINING:
			status = rdpsnd_recv_training_pdu(rdpsnd, s);
			break;

		case SNDC_WAVE:
			status = rdpsnd_recv_wave_info_pdu(rdpsnd, s, BodySize);
			break;

		case SNDC_CLOSE:
			rdpsnd_recv_close_pdu(rdpsnd);
			break;

		case SNDC_SETVOLUME:
			status = rdpsnd_recv_volume_pdu(rdpsnd, s);
			break;

		case SNDC_WAVE2:
			status = rdpsnd_recv_wave2_pdu(rdpsnd, s, BodySize);
			break;

		default:
			WLog_ERR(TAG, "unknown msgType %"PRIu8"", msgType);
			break;
	}

out:
	StreamPool_Return(rdpsnd->pool, s);
	return status;
}

static void rdpsnd_register_device_plugin(rdpsndPlugin* rdpsnd,
        rdpsndDevicePlugin* device)
{
	if (rdpsnd->device)
	{
		WLog_ERR(TAG, "existing device, abort.");
		return;
	}

	rdpsnd->device = device;
	device->rdpsnd = rdpsnd;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_load_device_plugin(rdpsndPlugin* rdpsnd, const char* name,
                                      ADDIN_ARGV* args)
{
	PFREERDP_RDPSND_DEVICE_ENTRY entry;
	FREERDP_RDPSND_DEVICE_ENTRY_POINTS entryPoints;
	UINT error;
	entry = (PFREERDP_RDPSND_DEVICE_ENTRY)
	        freerdp_load_channel_addin_entry("rdpsnd", (LPSTR) name, NULL, 0);

	if (!entry)
		return ERROR_INTERNAL_ERROR;

	entryPoints.rdpsnd = rdpsnd;
	entryPoints.pRegisterRdpsndDevice = rdpsnd_register_device_plugin;
	entryPoints.args = args;

	if ((error = entry(&entryPoints)))
		WLog_ERR(TAG, "%s entry returns error %"PRIu32"", name, error);

	WLog_INFO(TAG, "Loaded %s backend for rdpsnd", name);
	return error;
}

static BOOL rdpsnd_set_subsystem(rdpsndPlugin* rdpsnd, const char* subsystem)
{
	free(rdpsnd->subsystem);
	rdpsnd->subsystem = _strdup(subsystem);
	return (rdpsnd->subsystem != NULL);
}

BOOL rdpsnd_set_device_name(rdpsndPlugin* rdpsnd, const char* device_name)
{
	free(rdpsnd->device_name);
	rdpsnd->device_name = _strdup(device_name);
	return (rdpsnd->device_name != NULL);
}

static COMMAND_LINE_ARGUMENT_A rdpsnd_args[] =
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_process_addin_args(rdpsndPlugin* rdpsnd, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	rdpsnd->wQualityMode = HIGH_QUALITY; /* default quality mode */

	if (args->argc > 1)
	{
		flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON;
		status = CommandLineParseArgumentsA(args->argc, args->argv,
		                                    rdpsnd_args, flags, rdpsnd, NULL, NULL);

		if (status < 0)
			return CHANNEL_RC_INITIALIZATION_ERROR;

		arg = rdpsnd_args;
		errno = 0;

		do
		{
			if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
				continue;

			CommandLineSwitchStart(arg)
			CommandLineSwitchCase(arg, "sys")
			{
				if (!rdpsnd_set_subsystem(rdpsnd, arg->Value))
					return CHANNEL_RC_NO_MEMORY;
			}
			CommandLineSwitchCase(arg, "dev")
			{
				if (!rdpsnd_set_device_name(rdpsnd, arg->Value))
					return CHANNEL_RC_NO_MEMORY;
			}
			CommandLineSwitchCase(arg, "format")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > UINT16_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->fixed_format->wFormatTag = val;
			}
			CommandLineSwitchCase(arg, "rate")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > UINT32_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->fixed_format->nSamplesPerSec = val;
			}
			CommandLineSwitchCase(arg, "channel")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > UINT16_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->fixed_format->nChannels = val;
			}
			CommandLineSwitchCase(arg, "latency")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > INT32_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->latency = val;
			}
			CommandLineSwitchCase(arg, "quality")
			{
				long wQualityMode = DYNAMIC_QUALITY;

				if (_stricmp(arg->Value, "dynamic") == 0)
					wQualityMode = DYNAMIC_QUALITY;
				else if (_stricmp(arg->Value, "medium") == 0)
					wQualityMode = MEDIUM_QUALITY;
				else if (_stricmp(arg->Value, "high") == 0)
					wQualityMode = HIGH_QUALITY;
				else
				{
					wQualityMode = strtol(arg->Value, NULL, 0);

					if (errno != 0)
						return CHANNEL_RC_INITIALIZATION_ERROR;
				}

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

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_process_connect(rdpsndPlugin* rdpsnd)
{
	const struct
	{
		const char* subsystem;
		const char* device;
	}
	backends[] =
	{
#if defined(WITH_IOSAUDIO)
		{"ios", ""},
#endif
#if defined(WITH_OPENSLES)
		{"opensles", ""},
#endif
#if defined(WITH_PULSE)
		{"pulse", ""},
#endif
#if defined(WITH_ALSA)
		{"alsa", "default"},
#endif
#if defined(WITH_OSS)
		{"oss", ""},
#endif
#if defined(WITH_MACAUDIO)
		{"mac", "default"},
#endif
#if defined(WITH_WINMM)
		{ "winmm", ""},
#endif
		{ "fake", "" }
	};
	ADDIN_ARGV* args;
	UINT status = ERROR_INTERNAL_ERROR;
	rdpsnd->latency = 0;
	args = (ADDIN_ARGV*) rdpsnd->channelEntryPoints.pExtendedData;

	if (args)
	{
		status = rdpsnd_process_addin_args(rdpsnd, args);

		if (status != CHANNEL_RC_OK)
			return status;
	}

	if (rdpsnd->subsystem)
	{
		if ((status = rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args)))
		{
			WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
			         rdpsnd->subsystem, status);
			return status;
		}
	}
	else
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(backends); x++)
		{
			const char* subsystem_name = backends[x].subsystem;
			const char* device_name = backends[x].device;

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);

			if (!rdpsnd->device)
				continue;

			if (!rdpsnd_set_subsystem(rdpsnd, subsystem_name) ||
			    !rdpsnd_set_device_name(rdpsnd, device_name))
				return CHANNEL_RC_NO_MEMORY;

			break;
		}

		if (!rdpsnd->device || status)
			return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	return CHANNEL_RC_OK;
}

static void rdpsnd_process_disconnect(rdpsndPlugin* rdpsnd)
{
	rdpsnd_recv_close_pdu(rdpsnd);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpsnd_virtual_channel_write(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT status = CHANNEL_RC_BAD_INIT_HANDLE;

	if (rdpsnd)
	{
		status = rdpsnd->channelEntryPoints.pVirtualChannelWriteEx(rdpsnd->InitHandle, rdpsnd->OpenHandle,
		         Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "pVirtualChannelWriteEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_virtual_channel_event_data_received(rdpsndPlugin* plugin,
        void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
		return CHANNEL_RC_OK;

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (!plugin->data_in)
			plugin->data_in = StreamPool_Take(plugin->pool, totalLength);

		Stream_SetPosition(plugin->data_in, 0);
	}

	if (!Stream_EnsureRemainingCapacity(plugin->data_in, dataLength))
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write(plugin->data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		Stream_SealLength(plugin->data_in);
		Stream_SetPosition(plugin->data_in, 0);

		if (!Queue_Enqueue(plugin->queue, plugin->data_in))
		{
			WLog_ERR(TAG,  "Queue_Enqueue failed!");
			return ERROR_INTERNAL_ERROR;
		}

		plugin->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE rdpsnd_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event,
        LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) lpUserParam;

	if (!rdpsnd || (rdpsnd->OpenHandle != openHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = rdpsnd_virtual_channel_event_data_received(rdpsnd, pData,
			             dataLength, totalLength, dataFlags)))
				WLog_ERR(TAG,
				         "rdpsnd_virtual_channel_event_data_received failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && rdpsnd->rdpcontext)
		setChannelError(rdpsnd->rdpcontext, error,
		                "rdpsnd_virtual_channel_open_event_ex reported an error");
}

static DWORD WINAPI rdpsnd_virtual_channel_client_thread(LPVOID arg)
{
	BOOL running = TRUE;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;
	DWORD error = CHANNEL_RC_OK;
	HANDLE events[2];

	if ((error = rdpsnd_process_connect(rdpsnd)))
	{
		WLog_ERR(TAG, "error connecting sound channel");
		goto out;
	}

	events[1] = rdpsnd->stopEvent;
	events[0] = Queue_Event(rdpsnd->queue);

	do
	{
		const DWORD status = WaitForMultipleObjects(ARRAYSIZE(events), events, FALSE, INFINITE);

		switch (status)
		{
			case WAIT_OBJECT_0:
				{
					wStream* s = Queue_Dequeue(rdpsnd->queue);
					error = rdpsnd_recv_pdu(rdpsnd, s);
				}
				break;

			case WAIT_OBJECT_0 + 1:
				running = FALSE;
				break;

			default:
				error = status;
				break;
		}
	}
	while ((error == CHANNEL_RC_OK) && running);

out:

	if (error && rdpsnd->rdpcontext)
		setChannelError(rdpsnd->rdpcontext, error,
		                "rdpsnd_virtual_channel_client_thread reported an error");

	rdpsnd_process_disconnect(rdpsnd);
	ExitThread((DWORD)error);
	return error;
}

/* Called during cleanup.
 * All streams still in the queue have been removed
 * from the streampool and nead cleanup. */
static void rdpsnd_queue_free(void* data)
{
	wStream* s = (wStream*)data;
	Stream_Free(s, TRUE);
}

static UINT rdpsnd_virtual_channel_event_initialized(rdpsndPlugin* rdpsnd,
        LPVOID pData, UINT32 dataLength)
{
	rdpsnd->stopEvent = CreateEventA(NULL, TRUE, FALSE, "rdpsnd->stopEvent");

	if (!rdpsnd->stopEvent)
		goto fail;

	return CHANNEL_RC_OK;
fail:
	return ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_virtual_channel_event_connected(rdpsndPlugin* rdpsnd,
        LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	status = rdpsnd->channelEntryPoints.pVirtualChannelOpenEx(rdpsnd->InitHandle,
	         &rdpsnd->OpenHandle, rdpsnd->channelDef.name,
	         rdpsnd_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpenEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	rdpsnd->dsp_context = freerdp_dsp_context_new(FALSE);

	if (!rdpsnd->dsp_context)
		goto fail;

	rdpsnd->queue = Queue_New(TRUE, 32, 2);

	if (!rdpsnd->queue)
		goto fail;

	rdpsnd->queue->object.fnObjectFree = rdpsnd_queue_free;
	rdpsnd->pool = StreamPool_New(TRUE, 4096);

	if (!rdpsnd->pool)
		goto fail;

	ResetEvent(rdpsnd->stopEvent);
	rdpsnd->thread = CreateThread(NULL, 0,
	                              rdpsnd_virtual_channel_client_thread, (void*) rdpsnd,
	                              0, NULL);

	if (!rdpsnd->thread)
		goto fail;

	return CHANNEL_RC_OK;
fail:
	freerdp_dsp_context_free(rdpsnd->dsp_context);
	StreamPool_Free(rdpsnd->pool);
	Queue_Free(rdpsnd->queue);

	if (rdpsnd->stopEvent)
		CloseHandle(rdpsnd->stopEvent);

	if (rdpsnd->thread)
		CloseHandle(rdpsnd->thread);

	return CHANNEL_RC_NO_MEMORY;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_virtual_channel_event_disconnected(rdpsndPlugin* rdpsnd)
{
	UINT error;

	if (rdpsnd->OpenHandle == 0)
		return CHANNEL_RC_OK;

	SetEvent(rdpsnd->stopEvent);

	if (WaitForSingleObject(rdpsnd->thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
		return error;
	}

	CloseHandle(rdpsnd->thread);
	error = rdpsnd->channelEntryPoints.pVirtualChannelCloseEx(rdpsnd->InitHandle, rdpsnd->OpenHandle);

	if (CHANNEL_RC_OK != error)
	{
		WLog_ERR(TAG, "pVirtualChannelCloseEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(error), error);
		return error;
	}

	rdpsnd->OpenHandle = 0;
	freerdp_dsp_context_free(rdpsnd->dsp_context);
	StreamPool_Return(rdpsnd->pool, rdpsnd->data_in);
	StreamPool_Free(rdpsnd->pool);
	Queue_Free(rdpsnd->queue);
	audio_formats_free(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	rdpsnd->NumberOfClientFormats = 0;
	rdpsnd->ClientFormats = NULL;
	audio_formats_free(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
	rdpsnd->NumberOfServerFormats = 0;
	rdpsnd->ServerFormats = NULL;

	if (rdpsnd->device)
	{
		IFCALL(rdpsnd->device->Free, rdpsnd->device);
		rdpsnd->device = NULL;
	}

	return CHANNEL_RC_OK;
}

static void rdpsnd_virtual_channel_event_terminated(rdpsndPlugin* rdpsnd)
{
	if (rdpsnd)
	{
		audio_formats_free(rdpsnd->fixed_format, 1);
		free(rdpsnd->subsystem);
		free(rdpsnd->device_name);
		CloseHandle(rdpsnd->stopEvent);
		rdpsnd->InitHandle = 0;
	}

	free(rdpsnd);
}

static VOID VCAPITYPE rdpsnd_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	rdpsndPlugin* plugin = (rdpsndPlugin*) lpUserParam;

	if (!plugin || (plugin->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG,  "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_INITIALIZED:
			if ((error = rdpsnd_virtual_channel_event_initialized(plugin, pData, dataLength)))
				WLog_ERR(TAG, "rdpsnd_virtual_channel_event_initialized failed with error %"PRIu32"!",
				         error);

			break;

		case CHANNEL_EVENT_CONNECTED:
			if ((error = rdpsnd_virtual_channel_event_connected(plugin, pData, dataLength)))
				WLog_ERR(TAG, "rdpsnd_virtual_channel_event_connected failed with error %"PRIu32"!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = rdpsnd_virtual_channel_event_disconnected(plugin)))
				WLog_ERR(TAG,
				         "rdpsnd_virtual_channel_event_disconnected failed with error %"PRIu32"!", error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			rdpsnd_virtual_channel_event_terminated(plugin);
			break;

		case CHANNEL_EVENT_ATTACHED:
			plugin->attached = TRUE;
			break;

		case CHANNEL_EVENT_DETACHED:
			plugin->attached = FALSE;
			break;

		default:
			break;
	}

	if (error && plugin && plugin->rdpcontext)
		setChannelError(plugin->rdpcontext, error,
		                "rdpsnd_virtual_channel_init_event reported an error");
}

/* rdpsnd is always built-in */
#define VirtualChannelEntryEx	rdpsnd_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	rdpsndPlugin* rdpsnd;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;

	if (!pEntryPoints)
		return FALSE;

	rdpsnd = (rdpsndPlugin*) calloc(1, sizeof(rdpsndPlugin));

	if (!rdpsnd)
		return FALSE;

	rdpsnd->attached = TRUE;
	rdpsnd->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED |
	    CHANNEL_OPTION_ENCRYPT_RDP;
	sprintf_s(rdpsnd->channelDef.name, ARRAYSIZE(rdpsnd->channelDef.name), "rdpsnd");
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		rdpsnd->rdpcontext = pEntryPointsEx->context;
	}

	rdpsnd->fixed_format = audio_format_new();

	if (!rdpsnd->fixed_format)
	{
		free(rdpsnd);
		return FALSE;
	}

	rdpsnd->log = WLog_Get("com.freerdp.channels.rdpsnd.client");
	CopyMemory(&(rdpsnd->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	rdpsnd->InitHandle = pInitHandle;
	rc = rdpsnd->channelEntryPoints.pVirtualChannelInitEx(rdpsnd, NULL, pInitHandle,
	        &rdpsnd->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	        rdpsnd_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInitEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		free(rdpsnd);
		return FALSE;
	}

	return TRUE;
}
