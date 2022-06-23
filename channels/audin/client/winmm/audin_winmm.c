/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - WinMM implementation
 *
 * Copyright 2013 Zhang Zhaolong <zhangzl2013@126.com>
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <freerdp/addin.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"

/* fix missing definitions in mingw */
#ifndef WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE
#define WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE 0x0010
#endif

typedef struct
{
	IAudinDevice iface;

	char* device_name;
	AudinReceive receive;
	void* user_data;
	HANDLE thread;
	HANDLE stopEvent;
	HWAVEIN hWaveIn;
	PWAVEFORMATEX* ppwfx;
	PWAVEFORMATEX pwfx_cur;
	UINT32 ppwfx_size;
	UINT32 cFormats;
	UINT32 frames_per_packet;
	rdpContext* rdpcontext;
	wLog* log;
} AudinWinmmDevice;

static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,
                                DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)dwInstance;
	PWAVEHDR pWaveHdr;
	UINT error = CHANNEL_RC_OK;
	MMRESULT mmResult;

	switch (uMsg)
	{
		case WIM_CLOSE:
			break;

		case WIM_DATA:
			pWaveHdr = (WAVEHDR*)dwParam1;

			if (WHDR_DONE == (WHDR_DONE & pWaveHdr->dwFlags))
			{
				if (pWaveHdr->dwBytesRecorded &&
				    !(WaitForSingleObject(winmm->stopEvent, 0) == WAIT_OBJECT_0))
				{
					AUDIO_FORMAT format;
					format.cbSize = winmm->pwfx_cur->cbSize;
					format.nBlockAlign = winmm->pwfx_cur->nBlockAlign;
					format.nAvgBytesPerSec = winmm->pwfx_cur->nAvgBytesPerSec;
					format.nChannels = winmm->pwfx_cur->nChannels;
					format.nSamplesPerSec = winmm->pwfx_cur->nSamplesPerSec;
					format.wBitsPerSample = winmm->pwfx_cur->wBitsPerSample;
					format.wFormatTag = winmm->pwfx_cur->wFormatTag;

					if ((error = winmm->receive(&format, pWaveHdr->lpData,
					                            pWaveHdr->dwBytesRecorded, winmm->user_data)))
						break;

					mmResult = waveInAddBuffer(hWaveIn, pWaveHdr, sizeof(WAVEHDR));

					if (mmResult != MMSYSERR_NOERROR)
						error = ERROR_INTERNAL_ERROR;
				}
			}

			break;

		case WIM_OPEN:
			break;

		default:
			break;
	}

	if (error && winmm->rdpcontext)
		setChannelError(winmm->rdpcontext, error, "waveInProc reported an error");
}

static BOOL log_mmresult(AudinWinmmDevice* winmm, const char* what, MMRESULT result)
{
	if (result != MMSYSERR_NOERROR)
	{
		CHAR buffer[8192] = { 0 };
		CHAR msg[8192] = { 0 };
		CHAR cmsg[8192] = { 0 };
		waveInGetErrorTextA(result, buffer, sizeof(buffer));

		_snprintf(msg, sizeof(msg) - 1, "%s failed. %" PRIu32 " [%s]", what, result, buffer);
		_snprintf(cmsg, sizeof(cmsg) - 1, "audin_winmm_thread_func reported an error '%s'", msg);
		WLog_Print(winmm->log, WLOG_DEBUG, "%s", msg);
		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, cmsg);
		return FALSE;
	}
	return TRUE;
}

static BOOL test_format_supported(const PWAVEFORMATEX pwfx)
{
	MMRESULT rc;
	WAVEINCAPSA caps = { 0 };

	rc = waveInGetDevCapsA(WAVE_MAPPER, &caps, sizeof(caps));
	if (rc != MMSYSERR_NOERROR)
		return FALSE;

	switch (pwfx->nChannels)
	{
		case 1:
			if ((caps.dwFormats &
			     (WAVE_FORMAT_1M08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_4M08 | WAVE_FORMAT_96M08 |
			      WAVE_FORMAT_1M16 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M16 | WAVE_FORMAT_96M16)) == 0)
				return FALSE;
			break;
		case 2:
			if ((caps.dwFormats &
			     (WAVE_FORMAT_1S08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_96S08 |
			      WAVE_FORMAT_1S16 | WAVE_FORMAT_2S16 | WAVE_FORMAT_4S16 | WAVE_FORMAT_96S16)) == 0)
				return FALSE;
			break;
		default:
			return FALSE;
	}

	rc = waveInOpen(NULL, WAVE_MAPPER, pwfx, 0, 0,
	                WAVE_FORMAT_QUERY | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE);
	return (rc == MMSYSERR_NOERROR);
}

static DWORD WINAPI audin_winmm_thread_func(LPVOID arg)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)arg;
	char* buffer;
	int size, i;
	WAVEHDR waveHdr[4] = { 0 };
	DWORD status;
	MMRESULT rc;

	if (!winmm->hWaveIn)
	{
		MMRESULT rc;
		rc = waveInOpen(&winmm->hWaveIn, WAVE_MAPPER, winmm->pwfx_cur, (DWORD_PTR)waveInProc,
		                (DWORD_PTR)winmm,
		                CALLBACK_FUNCTION | WAVE_MAPPED_DEFAULT_COMMUNICATION_DEVICE);
		if (!log_mmresult(winmm, "waveInOpen", rc))
			return ERROR_INTERNAL_ERROR;
	}

	size =
	    (winmm->pwfx_cur->wBitsPerSample * winmm->pwfx_cur->nChannels * winmm->frames_per_packet +
	     7) /
	    8;

	for (i = 0; i < 4; i++)
	{
		buffer = (char*)malloc(size);

		if (!buffer)
			return CHANNEL_RC_NO_MEMORY;

		waveHdr[i].dwBufferLength = size;
		waveHdr[i].dwFlags = 0;
		waveHdr[i].lpData = buffer;
		rc = waveInPrepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));

		if (!log_mmresult(winmm, "waveInPrepareHeader", rc))
		{
		}

		rc = waveInAddBuffer(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));

		if (!log_mmresult(winmm, "waveInAddBuffer", rc))
		{
		}
	}

	rc = waveInStart(winmm->hWaveIn);

	if (!log_mmresult(winmm, "waveInStart", rc))
	{
	}

	status = WaitForSingleObject(winmm->stopEvent, INFINITE);

	if (status == WAIT_FAILED)
	{
		WLog_Print(winmm->log, WLOG_DEBUG, "WaitForSingleObject failed.");

		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR,
			                "audin_winmm_thread_func reported an error");
	}

	rc = waveInReset(winmm->hWaveIn);

	if (!log_mmresult(winmm, "waveInReset", rc))
	{
	}

	for (i = 0; i < 4; i++)
	{
		rc = waveInUnprepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));

		if (!log_mmresult(winmm, "waveInUnprepareHeader", rc))
		{
		}

		free(waveHdr[i].lpData);
	}

	rc = waveInClose(winmm->hWaveIn);

	if (!log_mmresult(winmm, "waveInClose", rc))
	{
	}

	winmm->hWaveIn = NULL;
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_free(IAudinDevice* device)
{
	UINT32 i;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;

	if (!winmm)
		return ERROR_INVALID_PARAMETER;

	for (i = 0; i < winmm->cFormats; i++)
	{
		free(winmm->ppwfx[i]);
	}

	free(winmm->ppwfx);
	free(winmm->device_name);
	free(winmm);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_close(IAudinDevice* device)
{
	DWORD status;
	UINT error = CHANNEL_RC_OK;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;

	if (!winmm)
		return ERROR_INVALID_PARAMETER;

	SetEvent(winmm->stopEvent);
	status = WaitForSingleObject(winmm->thread, INFINITE);

	if (status == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_Print(winmm->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	CloseHandle(winmm->thread);
	CloseHandle(winmm->stopEvent);
	winmm->thread = NULL;
	winmm->stopEvent = NULL;
	winmm->receive = NULL;
	winmm->user_data = NULL;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_set_format(IAudinDevice* device, const AUDIO_FORMAT* format,
                                   UINT32 FramesPerPacket)
{
	UINT32 i;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;

	if (!winmm || !format)
		return ERROR_INVALID_PARAMETER;

	winmm->frames_per_packet = FramesPerPacket;

	for (i = 0; i < winmm->cFormats; i++)
	{
		const PWAVEFORMATEX ppwfx = winmm->ppwfx[i];
		if ((ppwfx->wFormatTag == format->wFormatTag) && (ppwfx->nChannels == format->nChannels) &&
		    (ppwfx->wBitsPerSample == format->wBitsPerSample) &&
		    (ppwfx->nSamplesPerSec == format->nSamplesPerSec))
		{
			/* BUG: Many devices report to support stereo recording but fail here.
			 *      Ensure we always use mono. */
			if (ppwfx->nChannels > 1)
			{
				ppwfx->nChannels = 1;
			}

			if (ppwfx->nBlockAlign != 2)
			{
				ppwfx->nBlockAlign = 2;
				ppwfx->nAvgBytesPerSec = ppwfx->nSamplesPerSec * ppwfx->nBlockAlign;
			}

			if (!test_format_supported(ppwfx))
				return ERROR_INVALID_PARAMETER;
			winmm->pwfx_cur = ppwfx;
			return CHANNEL_RC_OK;
		}
	}

	return ERROR_INVALID_PARAMETER;
}

static BOOL audin_winmm_format_supported(IAudinDevice* device, const AUDIO_FORMAT* format)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;
	PWAVEFORMATEX pwfx;
	BYTE* data;

	if (!winmm || !format)
		return FALSE;

	if (format->wFormatTag != WAVE_FORMAT_PCM)
		return FALSE;

	if (format->nChannels != 1)
		return FALSE;

	pwfx = (PWAVEFORMATEX)malloc(sizeof(WAVEFORMATEX) + format->cbSize);

	if (!pwfx)
		return FALSE;

	pwfx->cbSize = format->cbSize;
	pwfx->wFormatTag = format->wFormatTag;
	pwfx->nChannels = format->nChannels;
	pwfx->nSamplesPerSec = format->nSamplesPerSec;
	pwfx->nBlockAlign = format->nBlockAlign;
	pwfx->wBitsPerSample = format->wBitsPerSample;
	data = (BYTE*)pwfx + sizeof(WAVEFORMATEX);
	memcpy(data, format->data, format->cbSize);

	pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->nBlockAlign;

	if (!test_format_supported(pwfx))
		goto fail;

	if (winmm->cFormats >= winmm->ppwfx_size)
	{
		PWAVEFORMATEX* tmp_ppwfx;
		tmp_ppwfx = realloc(winmm->ppwfx, sizeof(PWAVEFORMATEX) * winmm->ppwfx_size * 2);

		if (!tmp_ppwfx)
			goto fail;

		winmm->ppwfx_size *= 2;
		winmm->ppwfx = tmp_ppwfx;
	}

	winmm->ppwfx[winmm->cFormats++] = pwfx;
	return TRUE;

fail:
	free(pwfx);
	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;

	if (!winmm || !receive || !user_data)
		return ERROR_INVALID_PARAMETER;

	winmm->receive = receive;
	winmm->user_data = user_data;

	if (!(winmm->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_Print(winmm->log, WLOG_ERROR, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(winmm->thread = CreateThread(NULL, 0, audin_winmm_thread_func, winmm, 0, NULL)))
	{
		WLog_Print(winmm->log, WLOG_ERROR, "CreateThread failed!");
		CloseHandle(winmm->stopEvent);
		winmm->stopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_parse_addin_args(AudinWinmmDevice* device, const ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	const COMMAND_LINE_ARGUMENT_A* arg;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*)device;
	COMMAND_LINE_ARGUMENT_A audin_winmm_args[] = { { "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>",
		                                             NULL, NULL, -1, NULL, "audio device name" },
		                                           { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL } };

	flags =
	    COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
	status = CommandLineParseArgumentsA(args->argc, args->argv, audin_winmm_args, flags, winmm,
	                                    NULL, NULL);
	arg = audin_winmm_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg) CommandLineSwitchCase(arg, "dev")
		{
			winmm->device_name = _strdup(arg->Value);

			if (!winmm->device_name)
			{
				WLog_Print(winmm->log, WLOG_ERROR, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT winmm_freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	AudinWinmmDevice* winmm;
	UINT error;

	if (waveInGetNumDevs() == 0)
	{
		WLog_Print(WLog_Get(TAG), WLOG_ERROR, "No microphone available!");
		return ERROR_DEVICE_NOT_AVAILABLE;
	}

	winmm = (AudinWinmmDevice*)calloc(1, sizeof(AudinWinmmDevice));

	if (!winmm)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	winmm->log = WLog_Get(TAG);
	winmm->iface.Open = audin_winmm_open;
	winmm->iface.FormatSupported = audin_winmm_format_supported;
	winmm->iface.SetFormat = audin_winmm_set_format;
	winmm->iface.Close = audin_winmm_close;
	winmm->iface.Free = audin_winmm_free;
	winmm->rdpcontext = pEntryPoints->rdpcontext;
	args = pEntryPoints->args;

	if ((error = audin_winmm_parse_addin_args(winmm, args)))
	{
		WLog_Print(winmm->log, WLOG_ERROR,
		           "audin_winmm_parse_addin_args failed with error %" PRIu32 "!", error);
		goto error_out;
	}

	if (!winmm->device_name)
	{
		winmm->device_name = _strdup("default");

		if (!winmm->device_name)
		{
			WLog_Print(winmm->log, WLOG_ERROR, "_strdup failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}
	}

	winmm->ppwfx_size = 10;
	winmm->ppwfx = calloc(winmm->ppwfx_size, sizeof(PWAVEFORMATEX));

	if (!winmm->ppwfx)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, &winmm->iface)))
	{
		WLog_Print(winmm->log, WLOG_ERROR, "RegisterAudinDevice failed with error %" PRIu32 "!",
		           error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(winmm->ppwfx);
	free(winmm->device_name);
	free(winmm);
	return error;
}
