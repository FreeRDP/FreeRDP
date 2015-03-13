/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - WinMM implementation
 *
 * Copyright 2013 Zhang Zhaolong <zhangzl2013@126.com>
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
} AudinWinmmDevice;

static void CALLBACK waveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD_PTR dwInstance,
						DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) dwInstance;
	PWAVEHDR pWaveHdr;

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
					winmm->receive(pWaveHdr->lpData, pWaveHdr->dwBytesRecorded, winmm->user_data);
					waveInAddBuffer(hWaveIn, pWaveHdr, sizeof(WAVEHDR));
				}
			}
			break;

		case WIM_OPEN:
			break;

		default:
			break;
	}
}

static DWORD audin_winmm_thread_func(void* arg)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) arg;
	char *buffer;
	int size, i;
	WAVEHDR waveHdr[4];

	if (!winmm->hWaveIn)
	{
		if (MMSYSERR_NOERROR != waveInOpen(&winmm->hWaveIn, WAVE_MAPPER, winmm->pwfx_cur,
			(DWORD_PTR)waveInProc, (DWORD_PTR)winmm, CALLBACK_FUNCTION))
		{
			return 0;
		}
	}

	size = (winmm->pwfx_cur->wBitsPerSample * winmm->pwfx_cur->nChannels * winmm->frames_per_packet + 7) / 8;
	for (i = 0; i < 4; i++)
	{
		buffer = (char *) malloc(size);
		waveHdr[i].dwBufferLength = size;
		waveHdr[i].dwFlags = 0;
		waveHdr[i].lpData = buffer;
		if (MMSYSERR_NOERROR != waveInPrepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i])))
		{
			DEBUG_DVC("waveInPrepareHeader failed.");
		}
		if (MMSYSERR_NOERROR != waveInAddBuffer(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i])))
		{
			DEBUG_DVC("waveInAddBuffer failed.");
		}
	}
	waveInStart(winmm->hWaveIn);

	WaitForSingleObject(winmm->stopEvent, INFINITE);

	waveInStop(winmm->hWaveIn);

	for (i = 0; i < 4; i++)
	{
		if (MMSYSERR_NOERROR != waveInUnprepareHeader(winmm->hWaveIn, &waveHdr[i], sizeof(waveHdr[i])))
		{
			DEBUG_DVC("waveInUnprepareHeader failed.");
		}
		free(waveHdr[i].lpData);
	}

	waveInClose(winmm->hWaveIn);
	winmm->hWaveIn = NULL;

	return 0;
}

static void audin_winmm_free(IAudinDevice* device)
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
}

static void audin_winmm_close(IAudinDevice* device)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	DEBUG_DVC("");

	SetEvent(winmm->stopEvent);

	WaitForSingleObject(winmm->thread, INFINITE);

	CloseHandle(winmm->thread);
	CloseHandle(winmm->stopEvent);

	winmm->thread = NULL;
	winmm->stopEvent = NULL;
	winmm->receive = NULL;
	winmm->user_data = NULL;
}

static void audin_winmm_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
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
}

static BOOL audin_winmm_format_supported(IAudinDevice* device, audinFormat* format)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;
	PWAVEFORMATEX pwfx;
	BYTE *data;

	pwfx = (PWAVEFORMATEX)malloc(sizeof(WAVEFORMATEX) + format->cbSize);
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
					return 0;

				winmm->ppwfx_size *= 2;
				winmm->ppwfx = tmp_ppwfx;
			}
			winmm->ppwfx[winmm->cFormats++] = pwfx;
	
			return 1;
		}
	}

	free(pwfx);
	return 0;
}

static void audin_winmm_open(IAudinDevice* device, AudinReceive receive, void* user_data)
{
	AudinWinmmDevice* winmm = (AudinWinmmDevice*) device;

	DEBUG_DVC("");

	winmm->receive = receive;
	winmm->user_data = user_data;

	winmm->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	winmm->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) audin_winmm_thread_func, winmm, 0, NULL);
}

static COMMAND_LINE_ARGUMENT_A audin_winmm_args[] =
{
	{ "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void audin_winmm_parse_addin_args(AudinWinmmDevice* device, ADDIN_ARGV* args)
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
		}

		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

#ifdef STATIC_CHANNELS
#define freerdp_audin_client_subsystem_entry	winmm_freerdp_audin_client_subsystem_entry
#endif

int freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	AudinWinmmDevice* winmm;

	winmm = (AudinWinmmDevice*) malloc(sizeof(AudinWinmmDevice));
	ZeroMemory(winmm, sizeof(AudinWinmmDevice));

	winmm->iface.Open = audin_winmm_open;
	winmm->iface.FormatSupported = audin_winmm_format_supported;
	winmm->iface.SetFormat = audin_winmm_set_format;
	winmm->iface.Close = audin_winmm_close;
	winmm->iface.Free = audin_winmm_free;

	args = pEntryPoints->args;

	audin_winmm_parse_addin_args(winmm, args);

	if (!winmm->device_name)
		winmm->device_name = _strdup("default");

	winmm->ppwfx_size = 10;
	winmm->ppwfx = malloc(sizeof(PWAVEFORMATEX) * winmm->ppwfx_size);

	pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) winmm);

	return 0;
}
