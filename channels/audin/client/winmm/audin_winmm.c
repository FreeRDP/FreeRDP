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
#include <freerdp/addin.h>
#include <freerdp/client/audin.h>

#include "audin_main.h"

typedef struct _AudinWinmmDevice
{
	IAudinDevice iface;

	char* device_name;
	AudinReceive receive;
	void* user_data;
	HANDLE thread;
	HANDLE stopEvent;
	HWAVEIN hWaveIn;
	PWAVEFORMATEX *ppwfx;
	PWAVEFORMATEX pwfx_cur;
	UINT32 ppwfx_size;
	UINT32 cFormats;
	UINT32 frames_per_packet;
	rdpContext* rdpcontext;
} AudinWinmmDevice;

static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,
						DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) dwInstance;
	PWAVEHDR pWaveHdr;
	UINT error = CHANNEL_RC_OK;
	MMRESULT mmResult;

	switch(uMsg)
	{
		case WIM_CLOSE:
			break;

		case WIM_DATA:
			pWaveHdr = (WAVEHDR *)dwParam1;
			if (WHDR_DONE == (WHDR_DONE & pWaveHdr->dwFlags))
			{
				if (pWaveHdr->dwBytesRecorded
					&& !(WaitForSingleObject(winmm->stopEvent, 0) == WAIT_OBJECT_0))
				{
					if ((error = winmm->receive(pWaveHdr->lpData, pWaveHdr->dwBytesRecorded, winmm->user_data)))
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

static DWORD audin_winmm_thread_func(void* arg)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) arg;
	char *buffer;
	int size, i;
	WAVEHDR waveHdr[4];
	DWORD status;
	MMRESULT rc;

	if (!winmm->hWaveIn)
	{
		if (MMSYSERR_NOERROR != waveInOpen(&winmm->hWaveIn, WAVE_MAPPER, winmm->pwfx_cur,
			(DWORD_PTR)waveInProc, (DWORD_PTR)winmm, CALLBACK_FUNCTION))
		{
			if (winmm->rdpcontext)
				setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
			return 0;
		}
	}

	size = (winmm->pwfx_cur->wBitsPerSample * winmm->pwfx_cur->nChannels * winmm->frames_per_packet + 7) / 8;
	for (i = 0; i < 4; i++)
	{
		buffer = (char *) malloc(size);
		if (!buffer)
			return CHANNEL_RC_NO_MEMORY;
		waveHdr[i].dwBufferLength = size;
		waveHdr[i].dwFlags = 0;
		waveHdr[i].lpData = buffer;

		rc = waveInPrepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));
		if (MMSYSERR_NOERROR != rc)
		{
			DEBUG_DVC("waveInPrepareHeader failed. %d", rc);
			if (winmm->rdpcontext)
				setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
		}

		rc = waveInAddBuffer(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));
		if (MMSYSERR_NOERROR != rc)
		{
			DEBUG_DVC("waveInAddBuffer failed. %d", rc);
			if (winmm->rdpcontext)
				setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
		}
	}

	rc = waveInStart(winmm->hWaveIn);
	if (MMSYSERR_NOERROR != rc)
	{
		DEBUG_DVC("waveInStart failed. %d", rc);
		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
	}

	status = WaitForSingleObject(winmm->stopEvent, INFINITE);

	if (status == WAIT_FAILED)
{
		DEBUG_DVC("WaitForSingleObject failed.");
		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
	}

	rc = waveInReset(winmm->hWaveIn);
	if (MMSYSERR_NOERROR != rc)
	{
		DEBUG_DVC("waveInReset failed. %d", rc);
		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
	}

	for (i = 0; i < 4; i++)
	{
		rc = waveInUnprepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i]));
		if (MMSYSERR_NOERROR != rc)
		{
			DEBUG_DVC("waveInUnprepareHeader failed. %d", rc);
			if (winmm->rdpcontext)
				setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
		}
		free(waveHdr[i].lpData);
	}

	rc = waveInClose(winmm->hWaveIn);
	if (MMSYSERR_NOERROR != rc)
	{
		DEBUG_DVC("waveInClose failed. %d", rc);
		if (winmm->rdpcontext)
			setChannelError(winmm->rdpcontext, ERROR_INTERNAL_ERROR, "audin_winmm_thread_func reported an error");
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
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

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
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	SetEvent(winmm->stopEvent);

	status = WaitForSingleObject(winmm->thread, INFINITE);

    if (status == WAIT_FAILED)
    {
        error = GetLastError();
        WLog_ERR(TAG, "WaitForSingleObject failed with error %lu!", error);
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
static UINT audin_winmm_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
{
	UINT32 i;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	winmm->frames_per_packet = FramesPerPacket;

	for (i = 0; i < winmm->cFormats; i++)
	{
		if (winmm->ppwfx[i]->wFormatTag == format->wFormatTag
			&& winmm->ppwfx[i]->nChannels == format->nChannels
			&& winmm->ppwfx[i]->wBitsPerSample == format->wBitsPerSample)
		{
			winmm->pwfx_cur = winmm->ppwfx[i];
			break;
		}
	}
	return CHANNEL_RC_OK;
}

static BOOL audin_winmm_format_supported(IAudinDevice* device, audinFormat* format)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;
	PWAVEFORMATEX pwfx;
	BYTE *data;

	pwfx = (PWAVEFORMATEX)malloc(sizeof(WAVEFORMATEX) + format->cbSize);
	if (!pwfx)
		return FALSE;
	pwfx->cbSize = format->cbSize;
	pwfx->wFormatTag = format->wFormatTag;
	pwfx->nChannels = format->nChannels;
	pwfx->nSamplesPerSec = format->nSamplesPerSec;
	pwfx->nBlockAlign = format->nBlockAlign;
	pwfx->wBitsPerSample = format->wBitsPerSample;
	data = (BYTE *)pwfx + sizeof(WAVEFORMATEX);

	memcpy(data, format->data, format->cbSize);

	if (pwfx->wFormatTag == WAVE_FORMAT_PCM)
	{
		pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->nBlockAlign;
		if (MMSYSERR_NOERROR == waveInOpen(NULL, WAVE_MAPPER, pwfx, 0, 0, WAVE_FORMAT_QUERY))
		{
			if (winmm->cFormats >= winmm->ppwfx_size)
			{
				PWAVEFORMATEX *tmp_ppwfx;
				tmp_ppwfx = realloc(winmm->ppwfx, sizeof(PWAVEFORMATEX) * winmm->ppwfx_size * 2);
				if (!tmp_ppwfx)
					return FALSE;

				winmm->ppwfx_size *= 2;
				winmm->ppwfx = tmp_ppwfx;
			}
			winmm->ppwfx[winmm->cFormats++] = pwfx;
	
			return TRUE;
		}
	}

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
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	winmm->receive = receive;
	winmm->user_data = user_data;

	if (!(winmm->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(winmm->thread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE) audin_winmm_thread_func, winmm, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(winmm->stopEvent);
		winmm->stopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

static COMMAND_LINE_ARGUMENT_A audin_winmm_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_winmm_parse_addin_args(AudinWinmmDevice* device, ADDIN_ARGV* args)
{
	int status;
	DWORD flags;
	COMMAND_LINE_ARGUMENT_A* arg;
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;

	status = CommandLineParseArgumentsA(args->argc, (const char**) args->argv, audin_winmm_args, flags, winmm, NULL, NULL);

	arg = audin_winmm_args;

	do
	{
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "dev")
		{
			winmm->device_name = _strdup(arg->Value);
			if (!winmm->device_name)
			{
				WLog_ERR(TAG, "_strdup failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

	return CHANNEL_RC_OK;
}

#ifdef STATIC_CHANNELS
#define freerdp_audin_client_subsystem_entry	winmm_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry	FREERDP_API freerdp_audin_client_subsystem_entry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinWinmmDevice* winmm;
	UINT error;

	winmm = (AudinWinmmDevice*) calloc(1, sizeof(AudinWinmmDevice));
	if (!winmm)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	winmm->iface.Open = audin_winmm_open;
	winmm->iface.FormatSupported = audin_winmm_format_supported;
	winmm->iface.SetFormat = audin_winmm_set_format;
	winmm->iface.Close = audin_winmm_close;
	winmm->iface.Free = audin_winmm_free;
	winmm->rdpcontext = pEntryPoints->rdpcontext;

	args = pEntryPoints->args;

	if ((error = audin_winmm_parse_addin_args(winmm, args)))
	{
		WLog_ERR(TAG, "audin_winmm_parse_addin_args failed with error %d!", error);
		goto error_out;
	}

	if (!winmm->device_name)
	{
		winmm->device_name = _strdup("default");
		if (!winmm->device_name)
		{
			WLog_ERR(TAG, "_strdup failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}
	}

	winmm->ppwfx_size = 10;
	winmm->ppwfx = malloc(sizeof(PWAVEFORMATEX) * winmm->ppwfx_size);
	if (!winmm->ppwfx)
	{
		WLog_ERR(TAG, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) winmm)))
	{
		WLog_ERR(TAG, "RegisterAudinDevice failed with error %d!", error);
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	free(winmm->ppwfx);
	free(winmm->device_name);
	free(winmm);
	return error;
}
