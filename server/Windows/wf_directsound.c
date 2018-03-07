#include "wf_directsound.h"
#include "wf_interface.h"
#include "wf_info.h"
#include "wf_rdpsnd.h"


#include <winpr/windows.h>

#define INITGUID
#include <initguid.h>
#include <objbase.h>

#define CINTERFACE 1
#include <mmsystem.h>
#include <dsound.h>

#include <freerdp/log.h>
#define TAG SERVER_TAG("windows")

IDirectSoundCapture8* cap;
IDirectSoundCaptureBuffer8* capBuf;
DSCBUFFERDESC dscbd;
DWORD lastPos;
wfPeerContext* latestPeer;

int wf_rdpsnd_set_latest_peer(wfPeerContext* peer)
{
	latestPeer = peer;
	return 0;
}

int wf_directsound_activate(RdpsndServerContext* context)
{
	HRESULT hr;
	wfInfo* wfi;
	HANDLE hThread;
	
	LPDIRECTSOUNDCAPTUREBUFFER  pDSCB;

	wfi = wf_info_get_instance();
	if (!wfi)
	{
		WLog_ERR(TAG, "Failed to wfi instance");
		return 1;
	}
	WLog_DBG(TAG, "RDPSND (direct sound) Activated");
	hr = DirectSoundCaptureCreate8(NULL, &cap, NULL);

	if (FAILED(hr))
	{
		WLog_ERR(TAG, "Failed to create sound capture device");
		return 1;
	}

	WLog_INFO(TAG, "Created sound capture device");
	dscbd.dwSize = sizeof(DSCBUFFERDESC);
	dscbd.dwFlags = 0;
	dscbd.dwBufferBytes = wfi->agreed_format->nAvgBytesPerSec;
	dscbd.dwReserved = 0;
	dscbd.lpwfxFormat = wfi->agreed_format;
	dscbd.dwFXCount = 0;
	dscbd.lpDSCFXDesc = NULL;

	hr = cap->lpVtbl->CreateCaptureBuffer(cap, &dscbd, &pDSCB, NULL);

	if (FAILED(hr))
	{
		WLog_ERR(TAG, "Failed to create capture buffer");
	}

	WLog_INFO(TAG, "Created capture buffer");
	hr = pDSCB->lpVtbl->QueryInterface(pDSCB, &IID_IDirectSoundCaptureBuffer8, (LPVOID*)&capBuf);
	if (FAILED(hr))
	{
		WLog_ERR(TAG, "Failed to QI capture buffer");
	}
	WLog_INFO(TAG, "Created IDirectSoundCaptureBuffer8");
	pDSCB->lpVtbl->Release(pDSCB);
	lastPos = 0;

	if (!(hThread = CreateThread(NULL, 0, wf_rdpsnd_directsound_thread, latestPeer, 0, NULL)))
	{
		WLog_ERR(TAG, "Failed to create direct sound thread");
		return 1;
	}
	CloseHandle(hThread);

	return 0;
}

static DWORD WINAPI wf_rdpsnd_directsound_thread(LPVOID lpParam)
{
	HRESULT hr;
	DWORD beg = 0;
	DWORD end = 0;
	DWORD diff, rate;
	wfPeerContext* context;
	wfInfo* wfi;

	VOID* pbCaptureData  = NULL;
	DWORD dwCaptureLength = 0;
	VOID* pbCaptureData2 = NULL;
	DWORD dwCaptureLength2 = 0;
	VOID* pbPlayData   = NULL;
	DWORD dwReadPos = 0;
	LONG lLockSize = 0;

	wfi = wf_info_get_instance();
	if (!wfi)
	{
		WLog_ERR(TAG, "Failed get instance");
		return 1;
	}

	context = (wfPeerContext*)lpParam;
	rate = 1000 / 24;
	WLog_INFO(TAG, "Trying to start capture");
	hr = capBuf->lpVtbl->Start(capBuf, DSCBSTART_LOOPING);
	if (FAILED(hr))
	{
		WLog_ERR(TAG, "Failed to start capture");
	}
	WLog_INFO(TAG, "Capture started");

	while (1)
	{

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
		{
			Sleep(rate - diff);
		}

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
				WLog_ERR(TAG, "Failed to get read pos");
				wf_rdpsnd_unlock();
				break;
			}

			lLockSize = dwReadPos - lastPos;//dscbd.dwBufferBytes;
			if (lLockSize < 0) lLockSize += dscbd.dwBufferBytes;

			//WLog_DBG(TAG, "Last, read, lock = [%"PRIu32", %"PRIu32", %"PRId32"]\n", lastPos, dwReadPos, lLockSize);

			if (lLockSize == 0)
			{
				wf_rdpsnd_unlock();
				continue;
			}

			
			hr = capBuf->lpVtbl->Lock(capBuf, lastPos, lLockSize, &pbCaptureData, &dwCaptureLength, &pbCaptureData2, &dwCaptureLength2, 0L);
			if (FAILED(hr))
			{
				WLog_ERR(TAG, "Failed to lock sound capture buffer");
				wf_rdpsnd_unlock();
				break;
			}

			//fwrite(pbCaptureData, 1, dwCaptureLength, pFile);
			//fwrite(pbCaptureData2, 1, dwCaptureLength2, pFile);

			//FIXME: frames = bytes/(bytespersample * channels)
		
			context->rdpsnd->SendSamples(context->rdpsnd, pbCaptureData, dwCaptureLength/4, (UINT16)(beg & 0xffff));
			context->rdpsnd->SendSamples(context->rdpsnd, pbCaptureData2, dwCaptureLength2/4, (UINT16)(beg & 0xffff));


			hr = capBuf->lpVtbl->Unlock(capBuf, pbCaptureData, dwCaptureLength, pbCaptureData2, dwCaptureLength2);
			if (FAILED(hr))
			{
				WLog_ERR(TAG, "Failed to unlock sound capture buffer");
				wf_rdpsnd_unlock();
				return 0;
			}

			//TODO keep track of location in buffer
			lastPos += dwCaptureLength;
			lastPos %= dscbd.dwBufferBytes;
			lastPos += dwCaptureLength2;
			lastPos %= dscbd.dwBufferBytes;

			wf_rdpsnd_unlock();
		}

		
	}

	WLog_INFO(TAG, "Trying to stop sound capture");
	hr = capBuf->lpVtbl->Stop(capBuf);
	if (FAILED(hr))
	{
		WLog_ERR(TAG, "Failed to stop capture");
	}

	WLog_INFO(TAG, "Capture stopped");
	capBuf->lpVtbl->Release(capBuf);
	cap->lpVtbl->Release(cap);

	lastPos = 0;

	return 0;
}
