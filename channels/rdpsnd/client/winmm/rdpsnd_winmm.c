/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel
 *
 * Copyright 2009-2012 Jay Sorg
 * Copyright 2010-2012 Vic Lee
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
#include <freerdp/codec/dsp.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpsnd_main.h"

typedef struct rdpsnd_winmm_datablock rdpsndWinmmDatablock;

struct rdpsnd_winmm_datablock
{
	WAVEHDR header;
	rdpsndWinmmDatablock* next;
};

typedef struct rdpsnd_winmm_plugin rdpsndWinmmPlugin;

struct rdpsnd_winmm_plugin
{
	rdpsndDevicePlugin device;

	HWAVEOUT out_handle;
	WAVEFORMATEX format;
	int wformat;
	int block_size;
	int latency;
	HANDLE event;
	rdpsndWinmmDatablock* datablock_head;

	FREERDP_DSP_CONTEXT* dsp_context;
};

static BOOL rdpsnd_winmm_convert_format(const AUDIO_FORMAT* in, WAVEFORMATEX* out)
{
	BOOL result = FALSE;

	ZeroMemory(out, sizeof(WAVEFORMATEX));
	out->wFormatTag = WAVE_FORMAT_PCM;
	out->nChannels = in->nChannels;
	out->nSamplesPerSec = in->nSamplesPerSec;

	switch (in->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			out->wBitsPerSample = in->wBitsPerSample;
			result = TRUE;
			break;

		case WAVE_FORMAT_ADPCM:
		case WAVE_FORMAT_DVI_ADPCM:
			out->wBitsPerSample = 16;
			result = TRUE;
			break;
	}

	out->nBlockAlign = out->nChannels * out->wBitsPerSample / 8;
	out->nAvgBytesPerSec = out->nSamplesPerSec * out->nBlockAlign;

	return result;
}

static void rdpsnd_winmm_clear_datablocks(rdpsndWinmmPlugin* winmm, BOOL drain)
{
	rdpsndWinmmDatablock* datablock;

	while ((datablock = winmm->datablock_head) != NULL)
	{
		if (!drain && (datablock->header.dwFlags & WHDR_DONE) == 0)
			break;

		while ((datablock->header.dwFlags & WHDR_DONE) == 0)
			WaitForSingleObject(winmm->event, INFINITE);
		
		winmm->datablock_head = datablock->next;
		
		free(datablock->header.lpData);
		free(datablock);
	}
}

static void rdpsnd_winmm_set_format(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (format)
	{
		rdpsnd_winmm_convert_format(format, &winmm->format);

		winmm->wformat = format->wFormatTag;
		winmm->block_size = format->nBlockAlign;
	}

	winmm->latency = latency;
}

static void rdpsnd_winmm_open(rdpsndDevicePlugin* device, AUDIO_FORMAT* format, int latency)
{
	MMRESULT result;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (winmm->out_handle != NULL)
		return;

	DEBUG_SVC("opening");

	rdpsnd_winmm_set_format(device, format, latency);
	freerdp_dsp_context_reset_adpcm(winmm->dsp_context);

	result = waveOutOpen(&winmm->out_handle, WAVE_MAPPER, &winmm->format, (DWORD_PTR) winmm->event, 0, CALLBACK_EVENT);

	if (result != MMSYSERR_NOERROR)
	{
		DEBUG_WARN("waveOutOpen failed: %d", result);
	}
}

static void rdpsnd_winmm_close(rdpsndDevicePlugin* device)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	if (winmm->out_handle)
	{
		DEBUG_SVC("close");
		rdpsnd_winmm_clear_datablocks(winmm, TRUE);

		if (waveOutClose(winmm->out_handle) != MMSYSERR_NOERROR)
		{
			DEBUG_WARN("waveOutClose error");
		}
		
		winmm->out_handle = NULL;
	}
}

static void rdpsnd_winmm_free(rdpsndDevicePlugin* device)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*)device;

	rdpsnd_winmm_close(device);
	freerdp_dsp_context_free(winmm->dsp_context);
	CloseHandle(winmm->event);
	free(winmm);
}

static BOOL rdpsnd_winmm_format_supported(rdpsndDevicePlugin* device, AUDIO_FORMAT* format)
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

	if (!winmm->out_handle)
		return dwVolume;

	waveOutGetVolume(winmm->out_handle, &dwVolume);

	return dwVolume;
}

static void rdpsnd_winmm_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (!winmm->out_handle)
		return;

	waveOutSetVolume(winmm->out_handle, value);
}

static void rdpsnd_winmm_play(rdpsndDevicePlugin* device, BYTE* data, int size)
{
	BYTE* src;
	MMRESULT result;
	rdpsndWinmmDatablock* last;
	rdpsndWinmmDatablock* datablock;
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	if (!winmm->out_handle)
		return;

	if (winmm->wformat == WAVE_FORMAT_ADPCM)
	{
		winmm->dsp_context->decode_ms_adpcm(winmm->dsp_context,
			data, size, winmm->format.nChannels, winmm->block_size);
		size = winmm->dsp_context->adpcm_size;
		src = winmm->dsp_context->adpcm_buffer;
	}
	else if (winmm->wformat == WAVE_FORMAT_DVI_ADPCM)
	{
		winmm->dsp_context->decode_ima_adpcm(winmm->dsp_context,
			data, size, winmm->format.nChannels, winmm->block_size);
		size = winmm->dsp_context->adpcm_size;
		src = winmm->dsp_context->adpcm_buffer;
	}
	else
	{
		src = data;
	}

	rdpsnd_winmm_clear_datablocks(winmm, FALSE);

	for (last = winmm->datablock_head; last && last->next; last = last->next)
	{

	}
	
	datablock = (rdpsndWinmmDatablock*) malloc(sizeof(rdpsndWinmmDatablock));
	ZeroMemory(datablock, sizeof(rdpsndWinmmDatablock));
	
	if (last)
		last->next = datablock;
	else
		winmm->datablock_head = datablock;
	
	datablock->header.dwBufferLength = size;
	datablock->header.lpData = (LPSTR) malloc(size);
	CopyMemory(datablock->header.lpData, src, size);

	result = waveOutPrepareHeader(winmm->out_handle, &datablock->header, sizeof(datablock->header));

	if (result != MMSYSERR_NOERROR)
	{
		DEBUG_WARN("waveOutPrepareHeader: %d", result);
		return;
	}
	
	ResetEvent(winmm->event);
	waveOutWrite(winmm->out_handle, &datablock->header, sizeof(datablock->header));
}

static void rdpsnd_winmm_start(rdpsndDevicePlugin* device)
{
	rdpsndWinmmPlugin* winmm = (rdpsndWinmmPlugin*) device;

	rdpsnd_winmm_clear_datablocks(winmm, FALSE);
}

static void rdpsnd_winmm_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
}

#ifdef STATIC_CHANNELS
#define freerdp_rdpsnd_client_subsystem_entry	winmm_freerdp_rdpsnd_client_subsystem_entry
#endif

int freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
{
	ADDIN_ARGV* args;
	rdpsndWinmmPlugin* winmm;

	winmm = (rdpsndWinmmPlugin*) malloc(sizeof(rdpsndWinmmPlugin));
	ZeroMemory(winmm, sizeof(rdpsndWinmmPlugin));

	winmm->device.Open = rdpsnd_winmm_open;
	winmm->device.FormatSupported = rdpsnd_winmm_format_supported;
	winmm->device.SetFormat = rdpsnd_winmm_set_format;
	winmm->device.GetVolume = rdpsnd_winmm_get_volume;
	winmm->device.SetVolume = rdpsnd_winmm_set_volume;
	winmm->device.Play = rdpsnd_winmm_play;
	winmm->device.Start = rdpsnd_winmm_start;
	winmm->device.Close = rdpsnd_winmm_close;
	winmm->device.Free = rdpsnd_winmm_free;

	args = pEntryPoints->args;
	rdpsnd_winmm_parse_addin_args((rdpsndDevicePlugin*) winmm, args);

	winmm->dsp_context = freerdp_dsp_context_new();
	winmm->event = CreateEvent(NULL, TRUE, FALSE, NULL);

	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) winmm);

	return 0;
}
