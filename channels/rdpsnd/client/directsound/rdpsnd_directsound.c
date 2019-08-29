/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Output Virtual Channel - DirectSound implementation
 *
 * Copyright 2019 Armin Novak <armin.novak@thincast.com>
 * Copyright 2019 Thincast Technologies GmbH
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

#include <winpr/crt.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>

#include <freerdp/types.h>
#include <freerdp/channels/log.h>

#include "rdpsnd_main.h"

#include <windows.h>
#include <mmreg.h>
#include <dsound.h>

typedef struct rdpsnd_directsound_plugin rdpsndDirectSoundPlugin;

struct rdpsnd_directsound_plugin
{
	rdpsndDevicePlugin device;

	size_t pos;
	LPDIRECTSOUND       dsobject;
	LPDIRECTSOUNDBUFFER dsbuffer[2];
	UINT64 bufferStart[2];
	size_t bufferSize;

	WAVEFORMATEX format;
	UINT32 volume;
	UINT32 latency;
};

static DWORD Win32FromHResult(HRESULT hr)
{
	return HRESULT_CODE(hr);
}

static const char* hresult_to_string(HRESULT hr)
{
	static char szBuffer[512] = { 0 };
	DWORD rc;
	DWORD err = Win32FromHResult(hr);

	rc = FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
				  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), szBuffer, sizeof( szBuffer ), NULL );

	return szBuffer;
}

static IDirectSoundBuffer* rdpsnd_create_buffer(IDirectSound* dsobject, const WAVEFORMATEX* format, size_t size)
{
	HRESULT hr;
	IDirectSoundBuffer* dsbuffer = NULL;
	DSBUFFERDESC dsbdesc = { 0 };
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /* Better position accuracy */
					  | DSBCAPS_GLOBALFOCUS         /* Allows background playing */
					  | DSBCAPS_CTRLVOLUME          /* Allows volume control */
					  | DSBCAPS_CTRLPOSITIONNOTIFY; /* Allow position notifications */
	dsbdesc.dwBufferBytes = size; /* buffer size */
	dsbdesc.lpwfxFormat = format;

	hr = IDirectSound_CreateSoundBuffer(dsobject, &dsbdesc,
										&dsbuffer, NULL);
	if (FAILED(hr) && (dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE))
	{
		/* Try without DSBCAPS_LOCHARDWARE */
		dsbdesc.dwFlags &= ~(DWORD)(DSBCAPS_LOCHARDWARE);
		hr = IDirectSound_CreateSoundBuffer(dsobject, &dsbdesc,
											&dsbuffer, NULL);
	}

	if (FAILED(hr))
	{
		WLog_WARN(TAG, "IDirectSound_CreateSoundBuffer() failed with %s", hresult_to_string(hr));
		return NULL;
	}

	return dsbuffer;
}

static void rdpsnd_release_buffers(rdpsndDirectSoundPlugin* directsound)
{
	size_t x;
	for (x=0; x<ARRAYSIZE(directsound->dsbuffer); x++)
	{
		IDirectSoundBuffer* dsbuffer = directsound->dsbuffer[x];
		if (dsbuffer)
		{
			IDirectSoundBuffer_Stop(dsbuffer);
			IDirectSoundBuffer_Release(dsbuffer);
		}
		directsound->dsbuffer[x] = NULL;
	}
	directsound->bufferSize = 0;
	directsound->pos = 0;
}

static BOOL rdpsnd_create_buffers(rdpsndDirectSoundPlugin* directsound, size_t size)
{
	size_t x;
	for (x=0; x<ARRAYSIZE(directsound->dsbuffer); x++)
	{
		directsound->dsbuffer[x] = rdpsnd_create_buffer(directsound->dsobject, &directsound->format, size);
		if (!directsound->dsbuffer[x])
			return FALSE;
	}
	directsound->bufferSize = size;
	directsound->pos = 0;
	return TRUE;
}

static void rdpsnd_directsound_start(rdpsndDevicePlugin* device)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!directsound)
	{
		WLog_WARN(TAG, "[%] directsound=%p",
				  __FUNCTION__, directsound);
		return;
	}
}

static BOOL rdpsnd_directsound_stop(rdpsndDevicePlugin* device)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!directsound)
	{
		WLog_WARN(TAG, "[%] directsound=%p",
				  __FUNCTION__, directsound);
		return FALSE;
	}

	return TRUE;
}

static BOOL rdpsnd_directsound_convert_format(const AUDIO_FORMAT* in, WAVEFORMATEX* out)
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

static BOOL rdpsnd_directsound_set_format(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                                    UINT32 latency)
{
    rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!rdpsnd_directsound_convert_format(format, &directsound->format))
		return FALSE;
	directsound->latency = latency;

	return TRUE;
}

static BOOL rdpsnd_directsound_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
                              UINT32 latency)
{
	HRESULT hr;
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!rdpsnd_directsound_set_format(device, format, latency))
		return FALSE;

	if (!directsound || !directsound->dsobject)
	{
		WLog_WARN(TAG, "[%] directsound=%p, directsound->dsobject=%p",
				  __FUNCTION__, directsound, directsound ? directsound->dsobject : NULL);
		return FALSE;
	}

	hr = IDirectSound_SetCooperativeLevel(directsound->dsobject, GetDesktopWindow(),
										  DSSCL_NORMAL);
	if(FAILED(hr))
	{
		WLog_WARN(TAG, "IDirectSound_SetCooperativeLevel() failed with %s", hresult_to_string(hr));
		return FALSE;
	}


	if (FAILED(hr))
	{
		WLog_WARN(TAG, "pAudioClient->Initialize() failed with %s", hresult_to_string(hr));
		goto fail;
	}

	return TRUE;
	fail:
		// TODO: Cleanup
		return FALSE;
}

static void rdpsnd_directsound_close(rdpsndDevicePlugin* device)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	rdpsnd_directsound_stop(device);
	if (directsound)
	{
		rdpsnd_release_buffers(directsound);
	}
}

static void rdpsnd_directsound_free(rdpsndDevicePlugin* device)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (directsound)
	{
		rdpsnd_directsound_close(device);
		if( directsound->dsobject != NULL )
		{
			IDirectSound_Release(directsound->dsobject);
			directsound->dsobject = NULL;
		}
		free(directsound);
	}
}

static BOOL rdpsnd_directsound_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format)
{
	WAVEFORMATEX out;

	if (rdpsnd_directsound_convert_format(format, &out))
		return TRUE;

	return FALSE;
}

static UINT32 rdpsnd_directsound_get_volume(rdpsndDevicePlugin* device)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!directsound)
	{
		WLog_WARN(TAG, "[%] directsound=%p",
				  __FUNCTION__, directsound);
		return 0;
	}

	return directsound->volume;
}

static BOOL rdpsnd_directsound_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!directsound)
	{
		WLog_WARN(TAG, "[%] directsound=%p",
				  __FUNCTION__, directsound);
		return FALSE;
	}
	directsound->volume = value;

	return TRUE;
}

static IDirectSoundBuffer* rdpsnd_fill_buffer(IDirectSoundBuffer* dsbuffer, const BYTE* data, size_t size)
{
	HRESULT hr;
	void *p_write_position;
	unsigned long l_bytes1;

	hr = IDirectSoundBuffer_Lock(
		dsbuffer,
		0,       /* Start offset */
		size,                /* Number of bytes */
		&p_write_position,    /* Address of lock start */
		&l_bytes1,            /* Count of bytes locked before wrap around */
		NULL,       /* Buffer address (if wrap around) */
		NULL,            /* Count of bytes after wrap around */
		0 );                  /* Flags: DSBLOCK_FROMWRITECURSOR is buggy */

	if (FAILED(hr))
	{
		WLog_WARN(TAG, "IDirectSoundBuffer_Lock() failed with %s", hresult_to_string(hr));
		return NULL;
	}

	if (l_bytes1 < size)
	{
		WLog_WARN(TAG, "IDirectSoundBuffer() too small %lu (%"PRIdz")", l_bytes1, size);
		return NULL;
	}
	memcpy(p_write_position, data, size);

	hr = IDirectSoundBuffer_Unlock(dsbuffer,
								   p_write_position, l_bytes1,
								   NULL, 0);
	if (FAILED(hr))
	{
		WLog_WARN(TAG, "IDirectSoundBuffer_Unlock() failed with %s", hresult_to_string(hr));
		return NULL;
	}

	return dsbuffer;
}


static BOOL rdpsnd_directsound_submit(rdpsndDevicePlugin* device, IDirectSoundBuffer* dsbuffer, size_t size)
{
	HRESULT hr;
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);

	if (!directsound || !dsbuffer)
	{
		WLog_WARN(TAG, "[%] directsound=%p, dsbuffer=%p",
				  __FUNCTION__, directsound, dsbuffer);
		return FALSE;
	}

	hr = IDirectSoundBuffer_Play(dsbuffer, 0, 0, 0);
	if (FAILED(hr))
	{
		WLog_WARN(TAG, "IDirectSoundBuffer_Play() %s", hresult_to_string(hr));
		return FALSE;
	}

	return TRUE;
}

static UINT rdpsnd_directsound_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size)
{
	DWORD status;
	UINT64 stop, diff;
	IDirectSoundBuffer* dsbuffer;
	rdpsndDirectSoundPlugin* directsound = (rdpsndDirectSoundPlugin*)(device);
	UINT64 start = GetTickCount64();

	if (!directsound)
	{
		WLog_WARN(TAG, "[%] directsound=%p", __FUNCTION__, directsound);
		return 0;
	}

	if (directsound->bufferSize != size)
	{
		rdpsnd_release_buffers(directsound);
		if (!rdpsnd_create_buffers(directsound, size))
			return 0;
	}

	dsbuffer = directsound->dsbuffer[directsound->pos];
	if (!dsbuffer)
		return 0;

	directsound->bufferStart[directsound->pos] = start;
	directsound->pos++;
	directsound->pos %= ARRAYSIZE(directsound->dsbuffer);
	start = directsound->bufferStart[directsound->pos];

	while(SUCCEEDED(IDirectSoundBuffer_GetStatus(dsbuffer, &status)) && (status == DSBSTATUS_PLAYING))
		Sleep(1);

	if (!rdpsnd_fill_buffer(dsbuffer, data, size))
		return 0;

	if (!rdpsnd_directsound_submit(device, dsbuffer, size))
		return 0;

	if (start == 0)
		return 0;

	stop = GetTickCount64();
	diff = stop - start + directsound->latency;
	return (diff > UINT_MAX) ? UINT_MAX : (UINT)diff;
}

static void rdpsnd_directsound_parse_addin_args(rdpsndDevicePlugin* device, ADDIN_ARGV* args)
{
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
#ifdef BUILTIN_CHANNELS
FREERDP_API UINT directsound_freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
#else
FREERDP_API UINT freerdp_rdpsnd_client_subsystem_entry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints)
#endif
{
	HRESULT hr;
	ADDIN_ARGV* args;
	rdpsndDirectSoundPlugin* directsound = calloc(1, sizeof(rdpsndDirectSoundPlugin));
	rdpsndDevicePlugin* device = (rdpsndDevicePlugin*)(directsound);

	if (!directsound)
		return CHANNEL_RC_NO_MEMORY;

	memset(directsound, 0, sizeof(rdpsndDirectSoundPlugin));
	directsound->device.Open = rdpsnd_directsound_open;
	directsound->device.FormatSupported = rdpsnd_directsound_format_supported;
	directsound->device.GetVolume = rdpsnd_directsound_get_volume;
	directsound->device.SetVolume = rdpsnd_directsound_set_volume;
	directsound->device.Start = rdpsnd_directsound_start;
	directsound->device.Play = rdpsnd_directsound_play;
	directsound->device.Close = rdpsnd_directsound_close;
	directsound->device.Free = rdpsnd_directsound_free;

	hr = DirectSoundCreate(NULL, &directsound->dsobject, NULL);
	if (FAILED(hr))
	{
		WLog_WARN(TAG, "DirectSoundCreate() failed with %s", hresult_to_string(hr));
		free(directsound);
		return ERROR_INTERNAL_ERROR;
	}

	args = pEntryPoints->args;
	rdpsnd_directsound_parse_addin_args(device, args);
	directsound->volume = 0xFFFFFFFF;
	pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, device);
	return CHANNEL_RC_OK;
}
