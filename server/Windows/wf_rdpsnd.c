/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/windows.h>

#define CINTERFACE
#include <mmsystem.h>
#include <dsound.h>

#include <freerdp/server/rdpsnd.h>

#include "wf_rdpsnd.h"
#include "wf_info.h"

/*
 * Here are some temp things that shall be moved
 *
 */
IDirectSoundCapture8* cap;
IDirectSoundCaptureBuffer8* capBuf;
DSCBUFFERDESC dscbd;
DWORD capturePos;

#define BYTESPERSEC 176400

//FIXME support multiple clients
wfPeerContext* latestPeer;

static const rdpsndFormat test_audio_formats[] =
{
	{ 0x11, 2, 22050, 1024, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 2 channels */
	{ 0x11, 1, 22050, 512, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 1 channels */
	{ 0x01, 2, 22050, 4, 16, 0, NULL }, /* PCM, 22050 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 22050, 2, 16, 0, NULL }, /* PCM, 22050 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 44100, 4, 16, 0, NULL }, /* PCM, 44100 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 44100, 2, 16, 0, NULL }, /* PCM, 44100 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 11025, 4, 16, 0, NULL }, /* PCM, 11025 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 11025, 2, 16, 0, NULL }, /* PCM, 11025 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 8000, 4, 16, 0, NULL }, /* PCM, 8000 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 8000, 2, 16, 0, NULL } /* PCM, 8000 Hz, 1 channels, 16 bits */
};

static void wf_peer_rdpsnd_activated(rdpsnd_server_context* context)
{
	HRESULT hr;
	
	LPDIRECTSOUNDCAPTUREBUFFER  pDSCB;
	WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, 44100, BYTESPERSEC, 4, 16, 0};


	printf("RDPSND Activated\n");

	hr = DirectSoundCaptureCreate8(NULL, &cap, NULL);

	if (FAILED(hr))
	{
		_tprintf(_T("Failed to create sound capture device\n"));
		return;
	}
	_tprintf(_T("Created sound capture device\n"));

	dscbd.dwSize = sizeof(DSCBUFFERDESC);
	dscbd.dwFlags = 0;
	dscbd.dwBufferBytes = BYTESPERSEC;
	dscbd.dwReserved = 0;
	dscbd.lpwfxFormat = &wfx;
	dscbd.dwFXCount = 0;
	dscbd.lpDSCFXDesc = NULL;

	hr = cap->lpVtbl->CreateCaptureBuffer(cap, &dscbd, &pDSCB, NULL);

	if (FAILED(hr))
	{
		_tprintf(_T("Failed to create capture buffer\n"));
	}
	_tprintf(_T("Created capture buffer"));

	hr = pDSCB->lpVtbl->QueryInterface(pDSCB, &IID_IDirectSoundCaptureBuffer8, (LPVOID*)&capBuf);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to QI capture buffer\n"));
	}
	_tprintf(_T("Created IDirectSoundCaptureBuffer8\n"));
	pDSCB->lpVtbl->Release(pDSCB); 

	context->SelectFormat(context, 4);
	context->SetVolume(context, 0x7FFF, 0x7FFF);
	capturePos = 0;

	CreateThread(NULL, 0, wf_rdpsnd_thread, latestPeer, 0, NULL);


}

int wf_rdpsnd_lock()
{
	DWORD dRes;
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	dRes = WaitForSingleObject(wfi->snd_mutex, INFINITE);

	switch (dRes)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return TRUE;
		break;

	case WAIT_TIMEOUT:
		return FALSE;
		break;

	case WAIT_FAILED:
		printf("wf_rdpsnd_lock failed with 0x%08X\n", GetLastError());
		return -1;
		break;
	}

	return -1;
}

int wf_rdpsnd_unlock()
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	if (ReleaseMutex(wfi->snd_mutex) == 0)
	{
		printf("wf_rdpsnd_unlock failed with 0x%08X\n", GetLastError());
		return -1;
	}

	return TRUE;
}

BOOL wf_peer_rdpsnd_init(wfPeerContext* context)
{
	wfInfo* wfi;
	
	wfi = wf_info_get_instance();

	wfi->snd_mutex = CreateMutex(NULL, FALSE, NULL);
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->data = context;

	context->rdpsnd->server_formats = test_audio_formats;
	context->rdpsnd->num_server_formats =
			sizeof(test_audio_formats) / sizeof(test_audio_formats[0]);

	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;

	context->rdpsnd->Activated = wf_peer_rdpsnd_activated;

	context->rdpsnd->Initialize(context->rdpsnd);

	latestPeer = context;
	wfi->snd_stop = FALSE;
	return TRUE;
}

DWORD WINAPI wf_rdpsnd_thread(LPVOID lpParam)
{
	HRESULT hr;
	DWORD beg, end;
	DWORD diff, rate;
	wfPeerContext* context;
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	context = (wfPeerContext*)lpParam;
	rate = 1000 / 5;

	_tprintf(_T("Trying to start capture\n"));
	hr = capBuf->lpVtbl->Start(capBuf, DSCBSTART_LOOPING);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to start capture\n"));
	}
	_tprintf(_T("Capture started\n"));

	while (1)
	{
		VOID* pbCaptureData  = NULL;
		DWORD dwCaptureLength;
		VOID* pbCaptureData2 = NULL;
		DWORD dwCaptureLength2;
		VOID* pbPlayData   = NULL;
		DWORD dwReadPos;
		LONG lLockSize;
		beg = GetTickCount();

		if (wf_rdpsnd_lock() > 0)
		{
			//check for main exit condition
			if (wfi->snd_stop == TRUE)
			{
				wf_rdpsnd_unlock();
				break;
			}

			hr = capBuf->lpVtbl->GetCurrentPosition(capBuf, NULL, &dwReadPos);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to get read pos\n"));
				wf_rdpsnd_unlock();
				break;
			}

			lLockSize = dwReadPos - capturePos;//dscbd.dwBufferBytes;
			if (lLockSize < 0) lLockSize += dscbd.dwBufferBytes;

			if (lLockSize == 0) 
			{
				wf_rdpsnd_unlock();
				continue;
			}

			
			hr = capBuf->lpVtbl->Lock(capBuf, capturePos, lLockSize, &pbCaptureData, &dwCaptureLength, &pbCaptureData2, &dwCaptureLength2, 0L);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to lock sound capture buffer\n"));
				wf_rdpsnd_unlock();
				break;
			}

			//fwrite(pbCaptureData, 1, dwCaptureLength, pFile);
			//fwrite(pbCaptureData2, 1, dwCaptureLength2, pFile);

			//FIXME: frames = bytes/(bytespersample * channels)
		
			context->rdpsnd->SendSamples(context->rdpsnd, pbCaptureData, dwCaptureLength/4);
			context->rdpsnd->SendSamples(context->rdpsnd, pbCaptureData2, dwCaptureLength2/4);


			hr = capBuf->lpVtbl->Unlock(capBuf, pbCaptureData, dwCaptureLength, pbCaptureData2, dwCaptureLength2);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to unlock sound capture buffer\n"));
				wf_rdpsnd_unlock();
				return 0;
			}

			//TODO keep track of location in buffer
			capturePos += dwCaptureLength;
			capturePos %= dscbd.dwBufferBytes;
			capturePos += dwCaptureLength2;
			capturePos %= dscbd.dwBufferBytes;

			wf_rdpsnd_unlock();
		}

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
		{
			Sleep(rate - diff);
		}
	}

	_tprintf(_T("Trying to stop sound capture\n"));
	hr = capBuf->lpVtbl->Stop(capBuf);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to stop capture\n"));
	}
	_tprintf(_T("Capture stopped\n"));


	return 0;
}
