/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
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

#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "win_wasapi.h"

#define TAG SERVER_TAG("shadow.win")

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

/* GUIDs under names of our own: MinGW defines the system ones with
 * initguid.h, the Windows SDK in uuid.lib for some of them only */
static const CLSID win_wasapi_CLSID_MMDeviceEnumerator = {
	0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E }
};
static const IID win_wasapi_IID_IMMDeviceEnumerator = {
	0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 }
};
static const IID win_wasapi_IID_IAudioClient = {
	0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 }
};
static const IID win_wasapi_IID_IAudioCaptureClient = {
	0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 }
};

/* How the captured samples become 16-bit stereo */
typedef enum
{
	WIN_WASAPI_AS_IS, /* the audio engine converts (AUTOCONVERTPCM) */
	WIN_WASAPI_FLOAT32,
	WIN_WASAPI_INT16,
	WIN_WASAPI_INT24,
	WIN_WASAPI_INT32
} winWasapiSampleType;

struct win_wasapi_loopback
{
	IMMDeviceEnumerator* enumerator;
	IMMDevice* device;
	IAudioClient* client;
	IAudioCaptureClient* capture;
	winWasapiSampleType type;
	UINT32 channels;   /* in the captured frames */
	UINT32 frameBytes; /* of a captured frame */
};

static void win_wasapi_release(winWasapiLoopback* cap)
{
	WINPR_ASSERT(cap);

	if (cap->capture)
		cap->capture->lpVtbl->Release(cap->capture);
	if (cap->client)
		cap->client->lpVtbl->Release(cap->client);
	if (cap->device)
		cap->device->lpVtbl->Release(cap->device);
	if (cap->enumerator)
		cap->enumerator->lpVtbl->Release(cap->enumerator);

	const winWasapiLoopback empty = WINPR_C_ARRAY_INIT;
	*cap = empty;
}

/* The mix format, when the audio engine did not convert it for us: 48 kHz
 * only (there is no resampler), float or integer samples */
static BOOL win_wasapi_use_mix_format(winWasapiLoopback* cap, const WAVEFORMATEX* mix)
{
	WINPR_ASSERT(cap);
	WINPR_ASSERT(mix);

	UINT16 tag = mix->wFormatTag;
	if ((tag == WAVE_FORMAT_EXTENSIBLE) && (mix->cbSize >= 22))
	{
		/* the KSDATAFORMAT_SUBTYPE GUIDs start with the format tag */
		const WAVEFORMATEXTENSIBLE* ext = (const WAVEFORMATEXTENSIBLE*)mix;
		tag = (UINT16)ext->SubFormat.Data1;
	}

	if ((mix->nSamplesPerSec != WIN_WASAPI_RATE) || (mix->nChannels == 0))
		return FALSE;

	if ((tag == WAVE_FORMAT_IEEE_FLOAT) && (mix->wBitsPerSample == 32))
		cap->type = WIN_WASAPI_FLOAT32;
	else if ((tag == WAVE_FORMAT_PCM) && (mix->wBitsPerSample == 16))
		cap->type = WIN_WASAPI_INT16;
	else if ((tag == WAVE_FORMAT_PCM) && (mix->wBitsPerSample == 24))
		cap->type = WIN_WASAPI_INT24;
	else if ((tag == WAVE_FORMAT_PCM) && (mix->wBitsPerSample == 32))
		cap->type = WIN_WASAPI_INT32;
	else
		return FALSE;

	cap->channels = mix->nChannels;
	cap->frameBytes = mix->nBlockAlign;
	return TRUE;
}

static HRESULT win_wasapi_activate(winWasapiLoopback* cap)
{
	WINPR_ASSERT(cap);

	if (cap->client)
		cap->client->lpVtbl->Release(cap->client);
	cap->client = nullptr;
	return cap->device->lpVtbl->Activate(cap->device, &win_wasapi_IID_IAudioClient, CLSCTX_ALL,
	                                     nullptr, (void**)&cap->client);
}

static BOOL win_wasapi_start(winWasapiLoopback* cap, BOOL log)
{
	const DWORD level = log ? WLOG_WARN : WLOG_DEBUG;
	const REFERENCE_TIME bufferDuration = 2000000; /* 200 ms, in 100 ns units */
	WAVEFORMATEX* mix = nullptr;
	WAVEFORMATEX want = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(cap);

	HRESULT hr = CoCreateInstance(&win_wasapi_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
	                              &win_wasapi_IID_IMMDeviceEnumerator, (void**)&cap->enumerator);
	if (FAILED(hr))
	{
		WLog_Print(WLog_Get(TAG), level, "audio: no MMDeviceEnumerator (0x%08lX): no sound", hr);
		goto fail;
	}

	hr = cap->enumerator->lpVtbl->GetDefaultAudioEndpoint(cap->enumerator, eRender, eConsole,
	                                                      &cap->device);
	if (FAILED(hr))
	{
		WLog_Print(WLog_Get(TAG), level,
		           "audio: no render endpoint (0x%08lX): no sound until one appears", hr);
		goto fail;
	}

	hr = win_wasapi_activate(cap);
	if (FAILED(hr))
	{
		WLog_Print(WLog_Get(TAG), level, "audio: IMMDevice::Activate failed (0x%08lX)", hr);
		goto fail;
	}

	/* 16-bit stereo at 48 kHz from the audio engine, which converts the mix */
	want.wFormatTag = WAVE_FORMAT_PCM;
	want.nChannels = WIN_WASAPI_CHANNELS;
	want.nSamplesPerSec = WIN_WASAPI_RATE;
	want.wBitsPerSample = 16;
	want.nBlockAlign = WIN_WASAPI_FRAME_BYTES;
	want.nAvgBytesPerSec = WIN_WASAPI_RATE * WIN_WASAPI_FRAME_BYTES;
	hr = cap->client->lpVtbl->Initialize(cap->client, AUDCLNT_SHAREMODE_SHARED,
	                                     AUDCLNT_STREAMFLAGS_LOOPBACK |
	                                         AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
	                                         AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
	                                     bufferDuration, 0, &want, nullptr);
	if (SUCCEEDED(hr))
	{
		cap->type = WIN_WASAPI_AS_IS;
		cap->channels = WIN_WASAPI_CHANNELS;
		cap->frameBytes = WIN_WASAPI_FRAME_BYTES;
	}
	else
	{
		/* else the mix format, converted here */
		const HRESULT autoconvert = hr;
		hr = win_wasapi_activate(cap);
		if (SUCCEEDED(hr))
			hr = cap->client->lpVtbl->GetMixFormat(cap->client, &mix);
		if (FAILED(hr))
		{
			WLog_Print(WLog_Get(TAG), level, "audio: no mix format (0x%08lX)", hr);
			goto fail;
		}

		if (!win_wasapi_use_mix_format(cap, mix))
		{
			WLog_Print(WLog_Get(TAG), level,
			           "audio: the mix format (tag 0x%04" PRIX16 ", %" PRIu32 " Hz, %" PRIu16
			           " channels, %" PRIu16 " bits) is not one we convert, and the audio "
			           "engine refused to (0x%08lX): no sound",
			           mix->wFormatTag, (UINT32)mix->nSamplesPerSec, mix->nChannels,
			           mix->wBitsPerSample, autoconvert);
			goto fail;
		}

		hr = cap->client->lpVtbl->Initialize(cap->client, AUDCLNT_SHAREMODE_SHARED,
		                                     AUDCLNT_STREAMFLAGS_LOOPBACK, bufferDuration, 0, mix,
		                                     nullptr);
		if (FAILED(hr))
		{
			WLog_Print(WLog_Get(TAG), level, "audio: IAudioClient::Initialize failed (0x%08lX)",
			           hr);
			goto fail;
		}

		if (cap->channels > WIN_WASAPI_CHANNELS)
			WLog_INFO(TAG, "audio: the mix has %" PRIu32 " channels, the clients get the first two",
			          cap->channels);
	}

	hr = cap->client->lpVtbl->GetService(cap->client, &win_wasapi_IID_IAudioCaptureClient,
	                                     (void**)&cap->capture);
	if (FAILED(hr))
	{
		WLog_Print(WLog_Get(TAG), level, "audio: no IAudioCaptureClient (0x%08lX)", hr);
		goto fail;
	}

	hr = cap->client->lpVtbl->Start(cap->client);
	if (FAILED(hr))
	{
		WLog_Print(WLog_Get(TAG), level, "audio: IAudioClient::Start failed (0x%08lX)", hr);
		goto fail;
	}

	WLog_INFO(TAG, "audio: capturing what the default render endpoint plays (%s)",
	          (cap->type == WIN_WASAPI_AS_IS) ? "converted by the audio engine"
	                                          : "converted from the mix format");
	CoTaskMemFree(mix);
	return TRUE;

fail:
	CoTaskMemFree(mix);
	win_wasapi_release(cap);
	return FALSE;
}

static INT16 win_wasapi_sample(const winWasapiLoopback* cap, const BYTE* src)
{
	switch (cap->type)
	{
		case WIN_WASAPI_FLOAT32:
		{
			float f = 0.0f;
			memcpy(&f, src, sizeof(f));
			if (f >= 1.0f)
				return INT16_MAX;
			if (f <= -1.0f)
				return INT16_MIN;
			return (INT16)(f * 32767.0f);
		}
		case WIN_WASAPI_INT24:
			return (INT16)(src[1] | (src[2] << 8));
		case WIN_WASAPI_INT32:
			return (INT16)(src[2] | (src[3] << 8));
		case WIN_WASAPI_INT16:
		case WIN_WASAPI_AS_IS:
		default:
			return (INT16)(src[0] | (src[1] << 8));
	}
}

/* Appends frames to s as 16-bit stereo; silence when the engine says so */
static BOOL win_wasapi_append(const winWasapiLoopback* cap, wStream* s, const BYTE* data,
                              UINT32 frames, DWORD flags)
{
	WINPR_ASSERT(cap);

	const size_t size = 1ull * frames * WIN_WASAPI_FRAME_BYTES;
	if (!Stream_EnsureRemainingCapacity(s, size))
		return FALSE;

	if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
	{
		Stream_Zero(s, size);
		return TRUE;
	}

	if (cap->type == WIN_WASAPI_AS_IS)
	{
		Stream_Write(s, data, size);
		return TRUE;
	}

	const size_t sampleBytes = cap->frameBytes / cap->channels;
	for (UINT32 x = 0; x < frames; x++)
	{
		const BYTE* frame = &data[1ull * x * cap->frameBytes];
		const INT16 left = win_wasapi_sample(cap, frame);
		const INT16 right =
		    (cap->channels > 1) ? win_wasapi_sample(cap, &frame[sampleBytes]) : left;
		Stream_Write_INT16(s, left);
		Stream_Write_INT16(s, right);
	}
	return TRUE;
}

winWasapiLoopback* win_wasapi_loopback_open(BOOL log)
{
	winWasapiLoopback* loopback = (winWasapiLoopback*)calloc(1, sizeof(winWasapiLoopback));
	if (!loopback)
		return nullptr;

	if (!win_wasapi_start(loopback, log))
	{
		free(loopback);
		return nullptr;
	}
	return loopback;
}

void win_wasapi_loopback_close(winWasapiLoopback* loopback)
{
	if (!loopback)
		return;

	(void)loopback->client->lpVtbl->Stop(loopback->client);
	win_wasapi_release(loopback);
	free(loopback);
}

/* Loopback capture only has packets while something plays: the caller
 * polls, since an event never fires before Windows 10 1703 */
BOOL win_wasapi_loopback_read(winWasapiLoopback* loopback, wStream* s)
{
	WINPR_ASSERT(loopback);

	UINT32 packet = 0;
	HRESULT hr = loopback->capture->lpVtbl->GetNextPacketSize(loopback->capture, &packet);

	while (SUCCEEDED(hr) && (packet > 0))
	{
		BYTE* data = nullptr;
		UINT32 frames = 0;
		DWORD flags = 0;

		hr = loopback->capture->lpVtbl->GetBuffer(loopback->capture, &data, &frames, &flags,
		                                          nullptr, nullptr);
		if (FAILED(hr))
			break;

		const BOOL appended = win_wasapi_append(loopback, s, data, frames, flags);
		hr = loopback->capture->lpVtbl->ReleaseBuffer(loopback->capture, frames);
		if (!appended)
		{
			WLog_ERR(TAG, "audio: out of memory");
			return FALSE;
		}
		if (SUCCEEDED(hr))
			hr = loopback->capture->lpVtbl->GetNextPacketSize(loopback->capture, &packet);
	}

	if (FAILED(hr))
	{
		WLog_INFO(TAG, "audio: capture stopped (0x%08lX)", hr);
		return FALSE;
	}
	return TRUE;
}
