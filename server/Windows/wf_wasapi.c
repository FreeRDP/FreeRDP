
#include "wf_wasapi.h"
#include "wf_info.h"

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Audioclient.h>

//#define REFTIMES_PER_SEC  10000000
//#define REFTIMES_PER_MILLISEC  10000

#define REFTIMES_PER_SEC  100000
#define REFTIMES_PER_MILLISEC  100

//#define REFTIMES_PER_SEC  50000
//#define REFTIMES_PER_MILLISEC  50


DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C,
	    0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35,
	    0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1,0x78, 0xc2,0xf5,0x68,0xa7,0x03,0xb2);
DEFINE_GUID(IID_IAudioCaptureClient, 0xc8adbd64, 0xe71e, 0x48a0, 0xa4,0xde, 0x18,0x5c,0x39,0x5c,0xd3,0x17);

LPWSTR devStr = NULL;
wfPeerContext* latestPeer = NULL;

int wf_rdpsnd_set_latest_peer(wfPeerContext* peer)
{
	latestPeer = peer;
	return 0;
}


int wf_wasapi_activate(RdpsndServerContext* context)
{
	wchar_t * pattern = L"Stereo Mix";

	wf_wasapi_get_device_string(pattern, &devStr);

	if(devStr == NULL)
	{
		_tprintf(_T("Failed to match for output device! Disabling rdpsnd.\n"));
		return 1;
	}

	printf("RDPSND (WASAPI) Activated\n");

	CreateThread(NULL, 0, wf_rdpsnd_wasapi_thread, latestPeer, 0, NULL);

	return 0;
}

int wf_wasapi_get_device_string(LPWSTR pattern, LPWSTR * deviceStr)
{
	HRESULT hr;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pCollection = NULL;
	IMMDevice *pEndpoint = NULL;
	IPropertyStore *pProps = NULL;
	LPWSTR pwszID = NULL;
	unsigned int count, i;

	CoInitialize(NULL);
	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **) &pEnumerator);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to cocreate device enumerator\n"));
		exit(1);
	}

	hr = pEnumerator->lpVtbl->EnumAudioEndpoints(pEnumerator, eCapture, DEVICE_STATE_ACTIVE, &pCollection);
	if ( FAILED(hr) )
	{
		_tprintf(_T("Failed to create endpoint collection\n"));
		exit(1);
	}

	pCollection->lpVtbl->GetCount(pCollection, &count);
	_tprintf(_T("Num endpoints: %d\n"), count);

	if (count == 0)
	{
		_tprintf(_T("No endpoints!\n"));
		exit(1);
	}

	for (i = 0; i < count; ++i)
	{
		PROPVARIANT nameVar;
		PropVariantInit(&nameVar);

		hr = pCollection->lpVtbl->Item(pCollection, i, &pEndpoint);
		if ( FAILED(hr) )
		{
			_tprintf(_T("Failed to get endpoint %d\n"), i);
			exit(1);
		}


		hr = pEndpoint->lpVtbl->GetId(pEndpoint, &pwszID);
		if ( FAILED(hr) )
		{
			_tprintf(_T("Failed to get endpoint ID\n"));
			exit(1);
		}



		hr = pEndpoint->lpVtbl->OpenPropertyStore(pEndpoint, STGM_READ, &pProps);
		if ( FAILED(hr) )
		{
			_tprintf(_T("Failed to open property store\n"));
			exit(1);
		}


		hr = pProps->lpVtbl->GetValue(pProps, &PKEY_Device_FriendlyName, &nameVar);
		if ( FAILED(hr) )
		{
			_tprintf(_T("Failed to get device friendly name\n"));
			exit(1);
		}

		//do this a more reliable way
		if (wcscmp(pattern, nameVar.pwszVal) < 0)
		{
			unsigned int devStrLen;
			_tprintf(_T("Using sound ouput endpoint: [%s] (%s)\n"), nameVar.pwszVal, pwszID);
			//_tprintf(_T("matched %d characters\n"), wcscmp(pattern, nameVar.pwszVal));

			devStrLen = wcslen(pwszID);
			*deviceStr = (LPWSTR) malloc((devStrLen * 2) + 2);
			ZeroMemory(*deviceStr, (devStrLen * 2) + 2);
			wcscpy_s(*deviceStr, devStrLen+1, pwszID);
		}
		CoTaskMemFree(pwszID);
		pwszID = NULL;
		PropVariantClear(&nameVar);

		pProps->lpVtbl->Release(pProps);
		pProps = NULL;

		pEndpoint->lpVtbl->Release(pEndpoint);
		pEndpoint = NULL;

	}

	pCollection->lpVtbl->Release(pCollection);
	pCollection = NULL;

	pEnumerator->lpVtbl->Release(pEnumerator);
	pEnumerator = NULL;
	CoUninitialize();

	return 0;
}


DWORD WINAPI wf_rdpsnd_wasapi_thread(LPVOID lpParam)
{
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioCaptureClient *pCaptureClient = NULL;
	WAVEFORMATEX *pwfx = NULL;
	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	UINT32 packetLength = 0;
	UINT32 dCount = 0;
	BYTE *pData;

	wfPeerContext* context;
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	context = (wfPeerContext*)lpParam;

	CoInitialize(NULL);
	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **) &pEnumerator);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to cocreate device enumerator\n"));
		exit(1);
	}

	hr = pEnumerator->lpVtbl->GetDevice(pEnumerator, devStr, &pDevice);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to cocreate get device\n"));
		exit(1);
	}

	hr = pDevice->lpVtbl->Activate(pDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&pAudioClient);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to activate audio client\n"));
		exit(1);
	}

	hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient, &pwfx);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to get mix format\n"));
		exit(1);
	}


	pwfx->wFormatTag = wfi->agreed_format->wFormatTag;
	pwfx->nChannels = wfi->agreed_format->nChannels;
	pwfx->nSamplesPerSec = wfi->agreed_format->nSamplesPerSec;
	pwfx->nAvgBytesPerSec = wfi->agreed_format->nAvgBytesPerSec;
	pwfx->nBlockAlign = wfi->agreed_format->nBlockAlign;
	pwfx->wBitsPerSample = wfi->agreed_format->wBitsPerSample;
	pwfx->cbSize = wfi->agreed_format->cbSize;

	hr = pAudioClient->lpVtbl->Initialize(
		pAudioClient,
		AUDCLNT_SHAREMODE_SHARED,
		0,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to initialize the audio client\n"));
		exit(1);
	}

	hr = pAudioClient->lpVtbl->GetBufferSize(pAudioClient, &bufferFrameCount);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to get buffer size\n"));
		exit(1);
	}

	hr = pAudioClient->lpVtbl->GetService(pAudioClient, &IID_IAudioCaptureClient, (void **) &pCaptureClient);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to get the capture client\n"));
		exit(1);
	}


	hnsActualDuration = (UINT32)REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	hr = pAudioClient->lpVtbl->Start(pAudioClient);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to start capture\n"));
		exit(1);
	}

	dCount = 0;
	while(wfi->snd_stop == FALSE)
	{
		DWORD flags;


		Sleep(hnsActualDuration/REFTIMES_PER_MILLISEC/2);

		hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get packet length\n"));
			exit(1);
		}

		while(packetLength != 0)
		{
			hr = pCaptureClient->lpVtbl->GetBuffer(pCaptureClient, &pData, &numFramesAvailable, &flags, NULL, NULL);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to get buffer\n"));
				exit(1);
			}

			//Here we are writing the audio data
			//not sure if this flag is ever set by the system; msdn is not clear about it
			if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
				context->rdpsnd->SendSamples(context->rdpsnd, pData, packetLength);

			hr = pCaptureClient->lpVtbl->ReleaseBuffer(pCaptureClient, numFramesAvailable);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to release buffer\n"));
				exit(1);
			}

			hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
			if (FAILED(hr))
			{
				_tprintf(_T("Failed to get packet length\n"));
				exit(1);
			}
		}
	}

	pAudioClient->lpVtbl->Stop(pAudioClient);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to stop audio client\n"));
		exit(1);
	}

	CoTaskMemFree(pwfx);

	if (pEnumerator != NULL)
		pEnumerator->lpVtbl->Release(pEnumerator);

	if (pDevice != NULL)
		pDevice->lpVtbl->Release(pDevice);

	if (pAudioClient != NULL)
		pAudioClient->lpVtbl->Release(pAudioClient);

	if (pCaptureClient != NULL)
		pCaptureClient->lpVtbl->Release(pCaptureClient);

	CoUninitialize();

	return 0;
}