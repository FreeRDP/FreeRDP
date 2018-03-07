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

#include "rdpsnd_main.h"

#define TIME_DELAY_MS	65

struct rdpsnd_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntryPoints;

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

	BOOL attached;

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
	rdpContext* rdpcontext;
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_confirm_wave(rdpsndPlugin* rdpsnd, RDPSND_WAVE* wave);

static DWORD WINAPI rdpsnd_schedule_thread(LPVOID arg)
{
	wMessage message;
	UINT16 wTimeDiff;
	UINT16 wTimeStamp;
	UINT16 wCurrentTime;
	RDPSND_WAVE* wave;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;
	HANDLE events[2];
	UINT error = CHANNEL_RC_OK;
	DWORD status;
	events[0] = MessageQueue_Event(rdpsnd->MsgPipe->Out);
	events[1] = rdpsnd->stopEvent;

	while (1)
	{
		status = WaitForMultipleObjects(2, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"!", error);
			break;
		}

		status = WaitForSingleObject(rdpsnd->stopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(events[0], 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
			break;
		}

		if (!MessageQueue_Peek(rdpsnd->MsgPipe->Out, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

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

		if ((error = rdpsnd_confirm_wave(rdpsnd, wave)))
		{
			WLog_ERR(TAG, "error confirming wave");
			break;
		}

		message.wParam = NULL;
		free(wave);
	}

	if (error && rdpsnd->rdpcontext)
		setChannelError(rdpsnd->rdpcontext, error,
		                "rdpsnd_schedule_thread reported an error");

	ExitThread(error);
	return error;
}

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
	int index;
	AUDIO_FORMAT* serverFormat;
	AUDIO_FORMAT* clientFormat;
	rdpsnd_free_audio_formats(rdpsnd->ClientFormats, rdpsnd->NumberOfClientFormats);
	rdpsnd->NumberOfClientFormats = 0;
	rdpsnd->ClientFormats = NULL;

	if (!rdpsnd->NumberOfServerFormats)
		return;

	rdpsnd->ClientFormats = (AUDIO_FORMAT*) calloc(
	                            rdpsnd->NumberOfServerFormats,
	                            sizeof(AUDIO_FORMAT));

	if (!rdpsnd->ClientFormats)
		return;

	for (index = 0; index < (int) rdpsnd->NumberOfServerFormats; index++)
	{
		serverFormat = &rdpsnd->ServerFormats[index];

		if (rdpsnd->fixedFormat > 0
		    && (rdpsnd->fixedFormat != serverFormat->wFormatTag))
			continue;

		if (rdpsnd->fixedChannel > 0
		    && (rdpsnd->fixedChannel != serverFormat->nChannels))
			continue;

		if (rdpsnd->fixedRate > 0
		    && (rdpsnd->fixedRate != serverFormat->nSamplesPerSec))
			continue;

		if (rdpsnd->device
		    && rdpsnd->device->FormatSupported(rdpsnd->device, serverFormat))
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
	rdpsnd_print_audio_formats(rdpsnd->ServerFormats,
	                           rdpsnd->NumberOfServerFormats);
	WLog_ERR(TAG,  "");
	WLog_ERR(TAG,  "Client ");
	rdpsnd_print_audio_formats(rdpsnd->ClientFormats,
	                           rdpsnd->NumberOfClientFormats);
	WLog_ERR(TAG,  "");
#endif
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_send_client_audio_formats(rdpsndPlugin* rdpsnd)
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
	int index;
	UINT16 wVersion;
	AUDIO_FORMAT* format;
	UINT16 wNumberOfFormats;
	UINT ret = ERROR_BAD_LENGTH;
	rdpsnd_free_audio_formats(rdpsnd->ServerFormats, rdpsnd->NumberOfServerFormats);
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

	rdpsnd->ServerFormats = (AUDIO_FORMAT*) calloc(wNumberOfFormats,
	                        sizeof(AUDIO_FORMAT));

	if (!rdpsnd->ServerFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (index = 0; index < (int) wNumberOfFormats; index++)
	{
		format = &rdpsnd->ServerFormats[index];

		if (Stream_GetRemainingLength(s) < 14)
			goto out_fail;

		Stream_Read_UINT16(s, format->wFormatTag); /* wFormatTag */
		Stream_Read_UINT16(s, format->nChannels); /* nChannels */
		Stream_Read_UINT32(s, format->nSamplesPerSec); /* nSamplesPerSec */
		Stream_Read_UINT32(s, format->nAvgBytesPerSec); /* nAvgBytesPerSec */
		Stream_Read_UINT16(s, format->nBlockAlign); /* nBlockAlign */
		Stream_Read_UINT16(s, format->wBitsPerSample); /* wBitsPerSample */
		Stream_Read_UINT16(s, format->cbSize); /* cbSize */

		if (format->cbSize > 0)
		{
			if (Stream_GetRemainingLength(s) < format->cbSize)
				goto out_fail;

			format->data = (BYTE*) malloc(format->cbSize);

			if (!format->data)
			{
				ret = CHANNEL_RC_NO_MEMORY;
				goto out_fail;
			}

			Stream_Read(s, format->data, format->cbSize);
		}
	}

	rdpsnd_select_supported_audio_formats(rdpsnd);
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Server Audio Formats");
	ret = rdpsnd_send_client_audio_formats(rdpsnd);

	if (ret == CHANNEL_RC_OK)
	{
		if (wVersion >= 6)
			ret = rdpsnd_send_quality_mode_pdu(rdpsnd);
	}

	return ret;
out_fail:

	for (index = 0; index < (int) wNumberOfFormats; index++)
		free(format->data);

	free(rdpsnd->ServerFormats);
	rdpsnd->ServerFormats = NULL;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_wave_info_pdu(rdpsndPlugin* rdpsnd, wStream* s,
                                      UINT16 BodySize)
{
	UINT16 wFormatNo;
	AUDIO_FORMAT* format;
	rdpsnd->expectingWave = TRUE;

	if (Stream_GetRemainingLength(s) < 12)
		return ERROR_BAD_LENGTH;

	Stream_Read_UINT16(s, rdpsnd->wTimeStamp);
	Stream_Read_UINT16(s, wFormatNo);
	Stream_Read_UINT8(s, rdpsnd->cBlockNo);
	Stream_Seek(s, 3); /* bPad */
	Stream_Read(s, rdpsnd->waveData, 4);
	rdpsnd->waveDataSize = BodySize - 8;
	format = &rdpsnd->ClientFormats[wFormatNo];
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "WaveInfo: cBlockNo: %"PRIu8" wFormatNo: %"PRIu16"",
	           rdpsnd->cBlockNo, wFormatNo);

	if (!rdpsnd->isOpen)
	{
		rdpsnd->isOpen = TRUE;
		rdpsnd->wCurrentFormatNo = wFormatNo;

		//rdpsnd_print_audio_format(format);

		if (rdpsnd->device && rdpsnd->device->Open &&
		    !rdpsnd->device->Open(rdpsnd->device, format, rdpsnd->latency))
		{
			return CHANNEL_RC_INITIALIZATION_ERROR;
		}
	}
	else if (wFormatNo != rdpsnd->wCurrentFormatNo)
	{
		rdpsnd->wCurrentFormatNo = wFormatNo;

		if (rdpsnd->device)
		{
			if (rdpsnd->device->SetFormat
			    && !rdpsnd->device->SetFormat(rdpsnd->device, format, rdpsnd->latency))
				return CHANNEL_RC_INITIALIZATION_ERROR;
		}
	}

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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_confirm_wave(rdpsndPlugin* rdpsnd, RDPSND_WAVE* wave)
{
	WLog_Print(rdpsnd->log, WLOG_DEBUG,
	           "WaveConfirm: cBlockNo: %"PRIu8" wTimeStamp: %"PRIu16" wTimeDiff: %d",
	           wave->cBlockNo, wave->wTimeStampB, wave->wTimeStampB - wave->wTimeStampA);
	return rdpsnd_send_wave_confirm_pdu(rdpsnd, wave->wTimeStampB, wave->cBlockNo);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_device_send_wave_confirm_pdu(rdpsndDevicePlugin* device,
        RDPSND_WAVE* wave)
{
	if (device->DisableConfirmThread)
		return rdpsnd_confirm_wave(device->rdpsnd, wave);

	if (!MessageQueue_Post(device->rdpsnd->MsgPipe->Out, NULL, 0, (void*) wave,
	                       NULL))
	{
		WLog_ERR(TAG, "MessageQueue_Post failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_wave_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	int size;
	BYTE* data;
	RDPSND_WAVE* wave;
	AUDIO_FORMAT* format;
	UINT status;
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
	wave = (RDPSND_WAVE*) calloc(1, sizeof(RDPSND_WAVE));

	if (!wave)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	wave->wLocalTimeA = GetTickCount();
	wave->wTimeStampA = rdpsnd->wTimeStamp;
	wave->wFormatNo = rdpsnd->wCurrentFormatNo;
	wave->cBlockNo = rdpsnd->cBlockNo;
	wave->data = data;
	wave->length = size;
	wave->AutoConfirm = TRUE;
	format = &rdpsnd->ClientFormats[rdpsnd->wCurrentFormatNo];
	wave->wAudioLength = rdpsnd_compute_audio_time_length(format, size);
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Wave: cBlockNo: %"PRIu8" wTimeStamp: %"PRIu16"",
	           wave->cBlockNo, wave->wTimeStampA);

	if (!rdpsnd->device)
	{
		wave->wLocalTimeB = wave->wLocalTimeA;
		wave->wTimeStampB = wave->wTimeStampA;
		status = rdpsnd_confirm_wave(rdpsnd, wave);
		free(wave);
		return status;
	}

	if (rdpsnd->device->WaveDecode
	    && !rdpsnd->device->WaveDecode(rdpsnd->device, wave))
	{
		free(wave);
		return CHANNEL_RC_NO_MEMORY;
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

	status = CHANNEL_RC_OK;

	if (wave->AutoConfirm)
		status = rdpsnd->device->WaveConfirm(rdpsnd->device, wave);

	return status;
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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_recv_volume_pdu(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT32 dwVolume;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_BAD_LENGTH;

	Stream_Read_UINT32(s, dwVolume);
	WLog_Print(rdpsnd->log, WLOG_DEBUG, "Volume: 0x%08"PRIX32"", dwVolume);

	if (rdpsnd->device && rdpsnd->device->SetVolume &&
	    !rdpsnd->device->SetVolume(rdpsnd->device, dwVolume))
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

	if (!rdpsnd->attached)
		goto out;

	//WLog_ERR(TAG,  "msgType %"PRIu8" BodySize %"PRIu16"", msgType, BodySize);

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

		default:
			WLog_ERR(TAG, "unknown msgType %"PRIu8"", msgType);
			break;
	}

out:
	Stream_Free(s, TRUE);
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
	device->WaveConfirm = rdpsnd_device_send_wave_confirm_pdu;
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
		status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv,
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

				rdpsnd->fixedFormat = val;
			}
			CommandLineSwitchCase(arg, "rate")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > UINT32_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->fixedRate = val;
			}
			CommandLineSwitchCase(arg, "channel")
			{
				unsigned long val = strtoul(arg->Value, NULL, 0);

				if ((errno != 0) || (val > UINT16_MAX))
					return CHANNEL_RC_INITIALIZATION_ERROR;

				rdpsnd->fixedChannel = val;
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
	ADDIN_ARGV* args;
	UINT status = ERROR_INTERNAL_ERROR;
	char* subsystem_name = NULL, *device_name = NULL;
	rdpsnd->latency = -1;
	args = (ADDIN_ARGV*) rdpsnd->channelEntryPoints.pExtendedData;

	if (args)
	{
		status = rdpsnd_process_addin_args(rdpsnd, args);

		if (status != CHANNEL_RC_OK)
			return status;
	}

	if (rdpsnd->subsystem)
	{
		if (strcmp(rdpsnd->subsystem, "fake") == 0)
			return CHANNEL_RC_OK;

		if ((status = rdpsnd_load_device_plugin(rdpsnd, rdpsnd->subsystem, args)))
		{
			WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
			         rdpsnd->subsystem, status);
			return status;
		}
	}
	else
	{
#if defined(WITH_IOSAUDIO)

		if (!rdpsnd->device)
		{
			subsystem_name = "ios";
			device_name = "";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_OPENSLES)

		if (!rdpsnd->device)
		{
			subsystem_name = "opensles";
			device_name = "";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_PULSE)

		if (!rdpsnd->device)
		{
			subsystem_name = "pulse";
			device_name = "";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_ALSA)

		if (!rdpsnd->device)
		{
			subsystem_name = "alsa";
			device_name = "default";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_OSS)

		if (!rdpsnd->device)
		{
			subsystem_name = "oss";
			device_name = "";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_MACAUDIO)

		if (!rdpsnd->device)
		{
			subsystem_name = "mac";
			device_name = "default";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif
#if defined(WITH_WINMM)

		if (!rdpsnd->device)
		{
			subsystem_name = "winmm";
			device_name = "";

			if ((status = rdpsnd_load_device_plugin(rdpsnd, subsystem_name, args)))
				WLog_ERR(TAG, "unable to load the %s subsystem plugin because of error %"PRIu32"",
				         subsystem_name, status);
		}

#endif

		if (status)
			return status;

		if (rdpsnd->device)
		{
			if (!rdpsnd_set_subsystem(rdpsnd, subsystem_name)
			    || !rdpsnd_set_device_name(rdpsnd, device_name))
				return CHANNEL_RC_NO_MEMORY;
		}
	}

	if (!rdpsnd->device)
	{
		WLog_ERR(TAG, "no sound device.");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	if (!rdpsnd->device->DisableConfirmThread)
	{
		rdpsnd->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (!rdpsnd->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return CHANNEL_RC_INITIALIZATION_ERROR;
		}

		rdpsnd->ScheduleThread = CreateThread(NULL, 0,
											  rdpsnd_schedule_thread,
		                                      (void*) rdpsnd, 0, NULL);

		if (!rdpsnd->ScheduleThread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			return CHANNEL_RC_INITIALIZATION_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static void rdpsnd_process_disconnect(rdpsndPlugin* rdpsnd)
{
	if (rdpsnd->ScheduleThread)
	{
		SetEvent(rdpsnd->stopEvent);

		if (WaitForSingleObject(rdpsnd->ScheduleThread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", GetLastError());
			return;
		}

		CloseHandle(rdpsnd->ScheduleThread);
		CloseHandle(rdpsnd->stopEvent);
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpsnd_virtual_channel_write(rdpsndPlugin* rdpsnd, wStream* s)
{
	UINT status;

	if (!rdpsnd)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
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
	wStream* s;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (plugin->data_in != NULL)
			Stream_Free(plugin->data_in, TRUE);

		plugin->data_in = Stream_New(NULL, totalLength);

		if (!plugin->data_in)
		{
			WLog_ERR(TAG,  "Stream_New failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	s = plugin->data_in;

	if (!Stream_EnsureRemainingCapacity(s, (int) dataLength))
	{
		WLog_ERR(TAG,  "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(s, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(s) != Stream_GetPosition(s))
		{
			WLog_ERR(TAG,  "rdpsnd_virtual_channel_event_data_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		plugin->data_in = NULL;
		Stream_SealLength(s);
		Stream_SetPosition(s, 0);

		if (!MessageQueue_Post(plugin->MsgPipe->In, NULL, 0, (void*) s, NULL))
		{
			WLog_ERR(TAG,  "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
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
			Stream_Free((wStream*) pData, TRUE);
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
	wStream* data;
	wMessage message;
	rdpsndPlugin* rdpsnd = (rdpsndPlugin*) arg;
	UINT error;

	if ((error = rdpsnd_process_connect(rdpsnd)))
	{
		WLog_ERR(TAG, "error connecting sound channel");
		goto out;
	}

	while (1)
	{
		if (!MessageQueue_Wait(rdpsnd->MsgPipe->In))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(rdpsnd->MsgPipe->In, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;

			if ((error = rdpsnd_recv_pdu(rdpsnd, data)))
			{
				WLog_ERR(TAG, "error treating sound channel message");
				break;
			}
		}
	}

out:

	if (error && rdpsnd->rdpcontext)
		setChannelError(rdpsnd->rdpcontext, error,
		                "rdpsnd_virtual_channel_client_thread reported an error");

	rdpsnd_process_disconnect(rdpsnd);
	ExitThread(error);
	return error;
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

	rdpsnd->MsgPipe = MessagePipe_New();

	if (!rdpsnd->MsgPipe)
	{
		WLog_ERR(TAG, "unable to create message pipe");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpsnd->thread = CreateThread(NULL, 0,
								  rdpsnd_virtual_channel_client_thread, (void*) rdpsnd,
	                              0, NULL);

	if (!rdpsnd->thread)
	{
		WLog_ERR(TAG, "unable to create thread");
		MessagePipe_Free(rdpsnd->MsgPipe);
		rdpsnd->MsgPipe = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpsnd_virtual_channel_event_disconnected(rdpsndPlugin* rdpsnd)
{
	UINT error;
	MessagePipe_PostQuit(rdpsnd->MsgPipe, 0);

	if (WaitForSingleObject(rdpsnd->thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
		return error;
	}

	CloseHandle(rdpsnd->thread);
	rdpsnd->thread = NULL;
	error = rdpsnd->channelEntryPoints.pVirtualChannelCloseEx(rdpsnd->InitHandle, rdpsnd->OpenHandle);

	if (CHANNEL_RC_OK != error)
	{
		WLog_ERR(TAG, "pVirtualChannelCloseEx failed with %s [%08"PRIX32"]",
		         WTSErrorToString(error), error);
		return error;
	}

	rdpsnd->OpenHandle = 0;

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

	return CHANNEL_RC_OK;
}

static void rdpsnd_virtual_channel_event_terminated(rdpsndPlugin* rdpsnd)
{
	rdpsnd->InitHandle = 0;
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
	{
		return FALSE;
	}

	rdpsnd = (rdpsndPlugin*) calloc(1, sizeof(rdpsndPlugin));

	if (!rdpsnd)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	rdpsnd->attached = TRUE;
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
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		rdpsnd->rdpcontext = pEntryPointsEx->context;
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
