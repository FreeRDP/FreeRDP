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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Windows.h>
#include <MMSystem.h>

#include <winpr/crt.h>
#include <winpr/cmdline.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_winmm_plugin rdpsndWinmmPlugin;

struct rdpsnd_winmm_plugin
{
	rdpsndDevicePlugin device;

	HWAVEOUT hWaveOut;
	WAVEFORMATEX format;
	UINT32 volume;
	HANDLE next;
};

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
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (!rdpsnd_winmm_convert_format(format, &winmm->format))
		return FALSE;

	return TRUE;
}

static void CALLBACK rdpsnd_winmm_callback_function(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance,
        DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	LPWAVEHDR lpWaveHdr;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) dwInstance;

	switch (uMsg)
	{
		case MM_WOM_OPEN:
			WLog_DBG(TAG,  "MM_WOM_OPEN");
			break;

		case MM_WOM_CLOSE:
			WLog_DBG(TAG,  "MM_WOM_CLOSE");
			SetEvent(winmm->next);
			break;

		case MM_WOM_DONE:
			WLog_DBG(TAG, "MM_WOM_DONE");
			lpWaveHdr = (LPWAVEHDR) dwParam1;
			free(lpWaveHdr);
			SetEvent(winmm->next);
			break;

		default:
			WLog_DBG(TAG, "UNKNOWN [0x%08"PRIx32"]", uMsg);
			break;
	}
}

static BOOL rdpsnd_winmm_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                              UINT32 latency)
{
	MMRESULT mmResult;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (winmm->hWaveOut)
		return TRUE;

	if (!rdpsnd_winmm_set_format(device, format, latency))
		return FALSE;

	mmResult = waveOutOpen(&winmm->hWaveOut, WAVE_MAPPER, &winmm->format,
	                       (DWORD_PTR) rdpsnd_winmm_callback_function, (DWORD_PTR) winmm, CALLBACK_FUNCTION);

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_ERR(TAG, "waveOutOpen failed: %"PRIu32"", mmResult);
		return FALSE;
	}

	mmResult = waveOutSetVolume(winmm->hWaveOut, winmm->volume);

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_ERR(TAG, "waveOutSetVolume failed: %"PRIu32"", mmResult);
		return FALSE;
	}

	return TRUE;
}

static void rdpsnd_winmm_close(rdpsndDevicePlugin* device)
{
	MMRESULT mmResult;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (winmm->hWaveOut)
	{
		mmResult = waveOutReset(winmm->hWaveOut);
		mmResult = waveOutClose(winmm->hWaveOut);

		if (mmResult != MMSYSERR_NOERROR)
		{
			WLog_ERR(TAG,  "waveOutClose failure: %"PRIu32"", mmResult);
		}

		winmm->hWaveOut = NULL;
	}
}

static void rdpsnd_winmm_free(rdpsndDevicePlugin* device)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (winmm)
	{
		rdpsnd_winmm_close(device);
		CloseHandle(winmm->next);
		free(winmm);
	}
}

static BOOL rdpsnd_winmm_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	MMRESULT result;
	WAVEFORMATEX out;

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
	DWORD dwVolume;
	UINT16 dwVolumeLeft;
	UINT16 dwVolumeRight;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;
	dwVolumeLeft = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolumeRight = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolume = (dwVolumeLeft << 16) | dwVolumeRight;

	if (!winmm->hWaveOut)
		return dwVolume;

	waveOutGetVolume(winmm->hWaveOut, &dwVolume);
	return dwVolume;
}

static BOOL rdpsnd_winmm_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;
	winmm->volume = value;

	if (!winmm->hWaveOut)
		return TRUE;

	return (waveOutSetVolume(winmm->hWaveOut, value) == MMSYSERR_NOERROR);
}

static void rdpsnd_winmm_start(rdpsndDevicePlugin* device)
{
	//rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;
}

static UINT rdpsnd_winmm_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	MMRESULT mmResult;
	LPWAVEHDR lpWaveHdr;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (!winmm->hWaveOut)
		return 0;

	lpWaveHdr = (LPWAVEHDR) malloc(sizeof(WAVEHDR));

	if (!lpWaveHdr)
		return 0;

	ZeroMemory(lpWaveHdr, sizeof(WAVEHDR));
	lpWaveHdr->dwFlags = 0;
	lpWaveHdr->dwLoops = 0;
	lpWaveHdr->lpData = (LPSTR) data;
	lpWaveHdr->dwBufferLength = size;
	lpWaveHdr->dwUser = NULL;
	lpWaveHdr->lpNext = NULL;
	mmResult = waveOutPrepareHeader(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_ERR(TAG,  "waveOutPrepareHeader failure: %"PRIu32"", mmResult);
		return 0;
	}

	mmResult = waveOutWrite(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));

	if (mmResult != MMSYSERR_NOERROR)
	{
		WLog_ERR(TAG,  "waveOutWrite failure: %"PRIu32"", mmResult);
		waveOutUnprepareHeader(winmm->hWaveOut, lpWaveHdr, sizeof(WAVEHDR));
		free(lpWaveHdr);
		return 0;
	}

	WaitForSingleObject(winmm->next, INFINITE);
	return 10; /* TODO: Get real latencry in [ms] */
}

static void rdpsnd_winmm_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
}

#ifdef BUILTIN_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	winmm_freerdp_rdpsnd_client_subsystem_entry
#else
#define freerdp_rdpsnd_client_subsystem_entry	FREERDP_API freerdp_rdpsnd_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndWinmmPlugin* winmm;
	winmm = (rdpsndWinmmPlugin*) calloc(1, sizeof(rdpsndWinmmPlugin));

	if (!winmm)
		return CHANNEL_RC_NO_MEMORY;

	winmm->device.Open = rdpsnd_winmm_open;
	winmm->device.FormatSupported = rdpsnd_winmm_format_supported;
	winmm->device.GetVolume = rdpsnd_winmm_get_volume;
	winmm->device.SetVolume = rdpsnd_winmm_set_volume;
	winmm->device.Start = rdpsnd_winmm_start;
	winmm->device.Play = rdpsnd_winmm_play;
	winmm->device.Close = rdpsnd_winmm_close;
	winmm->device.Free = rdpsnd_winmm_free;
	winmm->next = CreateEventA(NULL, FALSE, FALSE, "winmm-play-event");

	if (!winmm->next)
	{
		free(winmm);
		return CHANNEL_RC_NO_MEMORY;
	}

	args = pEntryPoints->args;
	rdpsnd_winmm_parse_addin_args((rdpsndDevicePlugin*) winmm, args);
	winmm->volume = 0xFFFFFFFF;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) winmm);
	return CHANNEL_RC_OK;
}
