/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2012 Jay Sorg
 * Copyright 2010-2012 Vic Lee
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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>
#include <winpr/sysinfo.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include "rdpsnd_main.h"

typedef struct
{
	rdpsndDevicePlugin device;

	HWAVEOUT hWaveOut;
	WAVEFORMATEX format;
	UINT32 volume;
	wLog* log;
	UINT32 latency;
	HANDLE hThread;
	DWORD threadId;
	CRITICAL_SECTION cs;
} rdpsndWinmmPlugin;

static BOOL rdpsnd_winmm_convert_format(const AUDIO_FORMAT* in, WAVEFORMATEX* out)
{
	if (!in || !out)
		return FALSE;

	ZeroMemory(out, sizeof(WAVEFORMATEX));
	out->wFormatTag = WAVE_FORMAT_PCM;
	out->nChannels = in->nChannels;
	out->nSamplesPerSec = in->nSamplesPerSec;

	switch (in->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			out->wBitsPerSample = in->wBitsPerSample;
			break;

		default:
			return FALSE;
	}

	out->nBlockAlign = out->nChannels * out->wBitsPerSample / 8;
	out->nAvgBytesPerSec = out->nSamplesPerSec * out->nBlockAlign;
	return TRUE;
}

static BOOL rdpsnd_winmm_set_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                    UINT32 latency)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	winmm->latency = latency;
	if (!rdpsnd_winmm_convert_format(format, &winmm->format))
		return FALSE;

	return TRUE;
}

static DWORD WINAPI waveOutProc(LPVOID lpParameter)
{
	MSG msg;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)lpParameter;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == MM_WOM_CLOSE)
		{
			/* device was closed - exit thread */
			break;
		}
		else if (msg.message == MM_WOM_DONE)
		{
			/* free buffer */
			LPWAVEHDR waveHdr = (LPWAVEHDR)msg.lParam;
			EnterCriticalSection(&winmm->cs);
			waveOutUnprepareHeader((HWAVEOUT)msg.wParam, waveHdr, sizeof(WAVEHDR));
			LeaveCriticalSection(&winmm->cs);
			free(waveHdr->lpData);
			free(waveHdr);
		}
	}

	return 0;
}

static BOOL rdpsnd_winmm_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                              UINT32 latency)
{
	MMRESULT mmResult;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (winmm->hWaveOut)
		return TRUE;

	if (!rdpsnd_winmm_set_format(device, format, latency))
		return FALSE;

	winmm->hThread = CreateThread(NULL, 0, waveOutProc, winmm, 0, &winmm->threadId);
	if (!winmm->hThread)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "CreateThread failed: %" PRIu32 "", GetLastError());
		return FALSE;
	}

	mmResult = waveOutOpen(&winmm->hWaveOut, WAVE_MAPPER, &winmm->format,
	                       (DWORD_PTR)winmm->threadId, 0, CALLBACK_THREAD);

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutOpen failed: %" PRIu32 "", mmResult);
		return FALSE;
	}

	mmResult = waveOutSetVolume(winmm->hWaveOut, winmm->volume);

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutSetVolume failed: %" PRIu32 "", mmResult);
		return FALSE;
	}

	return TRUE;
}

static void rdpsnd_winmm_close(rdpsndDevicePlugin* device)
{
	MMRESULT mmResult;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (winmm->hWaveOut)
	{
		EnterCriticalSection(&winmm->cs);

		mmResult = waveOutReset(winmm->hWaveOut);
		if (mmResult != MMSYSERR_NOERROR)
			WLog_Print(winmm->log, WLOG_ERROR, "waveOutReset failure: %" PRIu32 "", mmResult);

		mmResult = waveOutClose(winmm->hWaveOut);
		if (mmResult != MMSYSERR_NOERROR)
			WLog_Print(winmm->log, WLOG_ERROR, "waveOutClose failure: %" PRIu32 "", mmResult);

		LeaveCriticalSection(&winmm->cs);

		winmm->hWaveOut = NULL;
	}

	if (winmm->hThread)
	{
		WaitForSingleObject(winmm->hThread, INFINITE);
		CloseHandle(winmm->hThread);
		winmm->hThread = NULL;
	}
}

static void rdpsnd_winmm_free(rdpsndDevicePlugin* device)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (winmm)
	{
		rdpsnd_winmm_close(device);
		DeleteCriticalSection(&winmm->cs);
		free(winmm);
	}
}

static BOOL rdpsnd_winmm_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	MMRESULT result;
	WAVEFORMATEX out;

	WINPR_UNUSED(device);
	if (rdpsnd_winmm_convert_format(format, &out))
	{
		result = waveOutOpen(NULL, WAVE_MAPPER, &out, 0, 0, WAVE_FORMAT_QUERY);

		if (result == MMSYSERR_NOERROR)
			return TRUE;
	}

	return FALSE;
}

static UINT32 rdpsnd_winmm_get_volume(rdpsndDevicePlugin* device)
{
	MMRESULT mmResult;
	DWORD dwVolume = UINT32_MAX;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (!winmm->hWaveOut)
		return dwVolume;

	mmResult = waveOutGetVolume(winmm->hWaveOut, &dwVolume);
	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutGetVolume failure: %" PRIu32 "", mmResult);
		dwVolume = UINT32_MAX;
	}
	return dwVolume;
}

static BOOL rdpsnd_winmm_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	MMRESULT mmResult;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;
	winmm->volume = value;

	if (!winmm->hWaveOut)
		return TRUE;

	mmResult = waveOutSetVolume(winmm->hWaveOut, value);
	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutGetVolume failure: %" PRIu32 "", mmResult);
		return FALSE;
	}
	return TRUE;
}

static UINT rdpsnd_winmm_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	MMRESULT mmResult;
	LPWAVEHDR lpWaveHdr;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (!winmm->hWaveOut)
		return 0;

	if (size > UINT32_MAX)
		return 0;

	lpWaveHdr = (LPWAVEHDR)calloc(1, sizeof(WAVEHDR));
	if (!lpWaveHdr)
		return 0;

	lpWaveHdr->dwFlags = 0;
	lpWaveHdr->dwLoops = 0;
	lpWaveHdr->lpData = malloc(size);
	if (!lpWaveHdr->lpData)
		goto fail;
	memcpy(lpWaveHdr->lpData, data, size);
	lpWaveHdr->dwBufferLength = (DWORD)size;

	EnterCriticalSection(&winmm->cs);

	mmResult = waveOutPrepareHeader(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutPrepareHeader failure: %" PRIu32 "", mmResult);
		goto failCS;
	}

	mmResult = waveOutWrite(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));
	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_Print(winmm->log, WLOG_ERROR, "waveOutWrite failure: %" PRIu32 "", mmResult);
		waveOutUnprepareHeader(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));
		goto failCS;
	}

	LeaveCriticalSection(&winmm->cs);
	return winmm->latency;
failCS:
	LeaveCriticalSection(&winmm->cs);
fail:
	if (lpWaveHdr)
		free(lpWaveHdr->lpData);
	free(lpWaveHdr);
	return 0;
}

static void rdpsnd_winmm_parse_addin_args(rdpsndDevicePlugin* device, const ADDIN_ARGV* args)
{
	WINPR_UNUSED(device);
	WINPR_UNUSED(args);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT winmm_freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	const ADDIN_ARGV* args;
	rdpsndWinmmPlugin* winmm;

	if (waveOutGetNumDevs() == 0)
	{
		WLog_Print(WLog_Get(TAG), WLOG_ERROR, "No sound playback device available!");
		return ERROR_DEVICE_NOT_AVAILABLE;
	}

	winmm = (rdpsndWinmmPlugin*)calloc(1, sizeof(rdpsndWinmmPlugin));
	if (!winmm)
		return CHANNEL_RC_NO_MEMORY;

	winmm->device.Open = rdpsnd_winmm_open;
	winmm->device.FormatSupported = rdpsnd_winmm_format_supported;
	winmm->device.GetVolume = rdpsnd_winmm_get_volume;
	winmm->device.SetVolume = rdpsnd_winmm_set_volume;
	winmm->device.Play = rdpsnd_winmm_play;
	winmm->device.Close = rdpsnd_winmm_close;
	winmm->device.Free = rdpsnd_winmm_free;
	winmm->log = WLog_Get(TAG);
	InitializeCriticalSection(&winmm->cs);

	args = pEntryPoints->args;
	rdpsnd_winmm_parse_addin_args(&winmm->device, args);
	winmm->volume = 0xFFFFFFFF;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*)winmm);
	return CHANNEL_RC_OK;
}
