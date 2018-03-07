/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#include <freerdp/log.h>
#include <freerdp/codec/h264.h>

#include <ks.h>
#include <codecapi.h>

#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <mftransform.h>

#include "h264.h"

#define TAG FREERDP_TAG("codec")

static const GUID sCLSID_CMSH264DecoderMFT = { 0x62CE7E72, 0x4C71, 0x4d20, { 0xB1, 0x5D,
            0x45, 0x28, 0x31, 0xA8, 0x7D, 0x9D } };
static const GUID sCLSID_VideoProcessorMFT = { 0x88753b26, 0x5b24, 0x49bd, { 0xb2, 0xe7,
            0x0c, 0x44, 0x5c, 0x78, 0xc9, 0x82 } };
static const GUID sIID_IMFTransform = { 0xbf94c121, 0x5b05, 0x4e6f, { 0x80, 0x00, 0xba,
            0x59, 0x89, 0x61, 0x41, 0x4d } };
static const GUID sMF_MT_MAJOR_TYPE = { 0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a,
            0x74, 0xc9, 0xf9, 0x6a, 0x8f } };
static const GUID sMF_MT_FRAME_SIZE = { 0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72,
            0x03, 0x08, 0x49, 0xa3, 0x7d } };
static const GUID sMF_MT_DEFAULT_STRIDE = { 0x644b4e48, 0x1e02, 0x4516, { 0xb0, 0xeb, 0xc0,
            0x1c, 0xa9, 0xd4, 0x9a, 0xc6 } };
static const GUID sMF_MT_SUBTYPE = { 0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29,
            0xd7, 0x2c, 0x35, 0xe5 } };
static const GUID sMF_XVP_DISABLE_FRC = { 0x2c0afa19, 0x7a97, 0x4d5a, { 0x9e, 0xe8, 0x16,
            0xd4, 0xfc, 0x51, 0x8d, 0x8c } };
static const GUID sMFMediaType_Video = { 0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00,
            0xAA, 0x00, 0x38, 0x9B, 0x71 } };
static const GUID sMFVideoFormat_RGB32 = { 22, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA,
            0x00, 0x38, 0x9B, 0x71 } };
static const GUID sMFVideoFormat_ARGB32 = { 21, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA,
            0x00, 0x38, 0x9B, 0x71 } };
static const GUID sMFVideoFormat_H264 = { 0x34363248, 0x0000, 0x0010, { 0x80, 0x00, 0x00,
            0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID sMFVideoFormat_IYUV = { 0x56555949, 0x0000, 0x0010, { 0x80, 0x00, 0x00,
            0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID sIID_ICodecAPI = { 0x901db4c7, 0x31ce, 0x41a2, { 0x85, 0xdc, 0x8f, 0xa0,
            0xbf, 0x41, 0xb8, 0xda } };
static const GUID sCODECAPI_AVLowLatencyMode = { 0x9c27891a, 0xed7a, 0x40e1, { 0x88, 0xe8,
            0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee } };
static const GUID sCODECAPI_AVDecVideoMaxCodedWidth = { 0x5ae557b8, 0x77af, 0x41f5, { 0x9f,
            0xa6, 0x4d, 0xb2, 0xfe, 0x1d, 0x4b, 0xca } };
static const GUID sCODECAPI_AVDecVideoMaxCodedHeight = { 0x7262a16a, 0xd2dc, 0x4e75, { 0x9b,
            0xa8, 0x65, 0xc0, 0xc6, 0xd3, 0x2b, 0x13 } };

#ifndef __IMFDXGIDeviceManager_FWD_DEFINED__
#define __IMFDXGIDeviceManager_FWD_DEFINED__
typedef interface IMFDXGIDeviceManager IMFDXGIDeviceManager;
#endif /* __IMFDXGIDeviceManager_FWD_DEFINED__ */

#ifndef __IMFDXGIDeviceManager_INTERFACE_DEFINED__
#define __IMFDXGIDeviceManager_INTERFACE_DEFINED__

typedef struct IMFDXGIDeviceManagerVtbl
{
	HRESULT(STDMETHODCALLTYPE* QueryInterface)(IMFDXGIDeviceManager* This,
	        REFIID riid, void** ppvObject);
	ULONG(STDMETHODCALLTYPE* AddRef)(IMFDXGIDeviceManager* This);
	ULONG(STDMETHODCALLTYPE* Release)(IMFDXGIDeviceManager* This);
	HRESULT(STDMETHODCALLTYPE* CloseDeviceHandle)(IMFDXGIDeviceManager* This,
	        HANDLE hDevice);
	HRESULT(STDMETHODCALLTYPE* GetVideoService)(IMFDXGIDeviceManager* This,
	        HANDLE hDevice, REFIID riid, void** ppService);
	HRESULT(STDMETHODCALLTYPE* LockDevice)(IMFDXGIDeviceManager* This,
	                                       HANDLE hDevice, REFIID riid, void** ppUnkDevice, BOOL fBlock);
	HRESULT(STDMETHODCALLTYPE* OpenDeviceHandle)(IMFDXGIDeviceManager* This,
	        HANDLE* phDevice);
	HRESULT(STDMETHODCALLTYPE* ResetDevice)(IMFDXGIDeviceManager* This,
	                                        IUnknown* pUnkDevice, UINT resetToken);
	HRESULT(STDMETHODCALLTYPE* TestDevice)(IMFDXGIDeviceManager* This,
	                                       HANDLE hDevice);
	HRESULT(STDMETHODCALLTYPE* UnlockDevice)(IMFDXGIDeviceManager* This,
	        HANDLE hDevice, BOOL fSaveState);
}
IMFDXGIDeviceManagerVtbl;

interface IMFDXGIDeviceManager
{
	CONST_VTBL struct IMFDXGIDeviceManagerVtbl* lpVtbl;
};

#endif /* __IMFDXGIDeviceManager_INTERFACE_DEFINED__ */

typedef HRESULT(__stdcall* pfnMFStartup)(ULONG Version, DWORD dwFlags);
typedef HRESULT(__stdcall* pfnMFShutdown)(void);
typedef HRESULT(__stdcall* pfnMFCreateSample)(IMFSample** ppIMFSample);
typedef HRESULT(__stdcall* pfnMFCreateMemoryBuffer)(DWORD cbMaxLength,
        IMFMediaBuffer** ppBuffer);
typedef HRESULT(__stdcall* pfnMFCreateMediaType)(IMFMediaType** ppMFType);

struct _H264_CONTEXT_MF
{
	ICodecAPI* codecApi;
	IMFTransform* transform;
	IMFMediaType* inputType;
	IMFMediaType* outputType;
	IMFSample* sample;
	UINT32 frameWidth;
	UINT32 frameHeight;
	IMFSample* outputSample;
	IMFMediaBuffer* outputBuffer;
	HMODULE mfplat;
	pfnMFStartup MFStartup;
	pfnMFShutdown MFShutdown;
	pfnMFCreateSample MFCreateSample;
	pfnMFCreateMemoryBuffer MFCreateMemoryBuffer;
	pfnMFCreateMediaType MFCreateMediaType;
};
typedef struct _H264_CONTEXT_MF H264_CONTEXT_MF;

static HRESULT mf_find_output_type(H264_CONTEXT_MF* sys, const GUID* guid,
                                   IMFMediaType** ppMediaType)
{
	DWORD idx = 0;
	GUID mediaGuid;
	HRESULT hr = S_OK;
	IMFMediaType* pMediaType = NULL;

	while (1)
	{
		hr = sys->transform->lpVtbl->GetOutputAvailableType(sys->transform, 0, idx,
		        &pMediaType);

		if (FAILED(hr))
			break;

		pMediaType->lpVtbl->GetGUID(pMediaType, &sMF_MT_SUBTYPE, &mediaGuid);

		if (IsEqualGUID(&mediaGuid, guid))
		{
			*ppMediaType = pMediaType;
			return S_OK;
		}

		pMediaType->lpVtbl->Release(pMediaType);
		idx++;
	}

	return hr;
}

static HRESULT mf_create_output_sample(H264_CONTEXT* h264, H264_CONTEXT_MF* sys)
{
	HRESULT hr = S_OK;
	MFT_OUTPUT_STREAM_INFO streamInfo;

	if (sys->outputSample)
	{
		sys->outputSample->lpVtbl->Release(sys->outputSample);
		sys->outputSample = NULL;
	}

	hr = sys->MFCreateSample(&sys->outputSample);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateSample failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = sys->transform->lpVtbl->GetOutputStreamInfo(sys->transform, 0,
	        &streamInfo);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "GetOutputStreamInfo failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = sys->MFCreateMemoryBuffer(streamInfo.cbSize, &sys->outputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateMemoryBuffer failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	sys->outputSample->lpVtbl->AddBuffer(sys->outputSample, sys->outputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "AddBuffer failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	sys->outputBuffer->lpVtbl->Release(sys->outputBuffer);
error:
	return hr;
}

static int mf_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize)
{
	HRESULT hr;
	BYTE* pbBuffer = NULL;
	DWORD cbMaxLength = 0;
	DWORD cbCurrentLength = 0;
	DWORD outputStatus = 0;
	IMFSample* inputSample = NULL;
	IMFMediaBuffer* inputBuffer = NULL;
	IMFMediaBuffer* outputBuffer = NULL;
	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*) h264->pSystemData;
	INT32* iStride = h264->iStride;
	BYTE** pYUVData = h264->pYUVData;
	hr = sys->MFCreateMemoryBuffer(SrcSize, &inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateMemoryBuffer failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = inputBuffer->lpVtbl->Lock(inputBuffer, &pbBuffer, &cbMaxLength,
	                               &cbCurrentLength);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	CopyMemory(pbBuffer, pSrcData, SrcSize);
	hr = inputBuffer->lpVtbl->SetCurrentLength(inputBuffer, SrcSize);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "SetCurrentLength failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = inputBuffer->lpVtbl->Unlock(inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = sys->MFCreateSample(&inputSample);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateSample failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	inputSample->lpVtbl->AddBuffer(inputSample, inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "AddBuffer failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	inputBuffer->lpVtbl->Release(inputBuffer);
	hr = sys->transform->lpVtbl->ProcessInput(sys->transform, 0, inputSample, 0);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "ProcessInput failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	hr = mf_create_output_sample(h264, sys);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08"PRIX32"", hr);
		goto error;
	}

	outputDataBuffer.dwStreamID = 0;
	outputDataBuffer.dwStatus = 0;
	outputDataBuffer.pEvents = NULL;
	outputDataBuffer.pSample = sys->outputSample;
	hr = sys->transform->lpVtbl->ProcessOutput(sys->transform, 0, 1,
	        &outputDataBuffer, &outputStatus);

	if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
	{
		UINT32 stride = 0;
		UINT64 frameSize = 0;

		if (sys->outputType)
		{
			sys->outputType->lpVtbl->Release(sys->outputType);
			sys->outputType = NULL;
		}

		hr = mf_find_output_type(sys, &sMFVideoFormat_IYUV, &sys->outputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_find_output_type failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetOutputType(sys->transform, 0, sys->outputType,
		        0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetOutputType failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = mf_create_output_sample(h264, sys);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->outputType->lpVtbl->GetUINT64(sys->outputType, &sMF_MT_FRAME_SIZE,
		                                        &frameSize);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetUINT64(MF_MT_FRAME_SIZE) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		sys->frameWidth = (UINT32)(frameSize >> 32);
		sys->frameHeight = (UINT32) frameSize;
		hr = sys->outputType->lpVtbl->GetUINT32(sys->outputType, &sMF_MT_DEFAULT_STRIDE,
		                                        &stride);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetUINT32(MF_MT_DEFAULT_STRIDE) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		if (!avc420_ensure_buffer(h264, stride, sys->frameWidth, sys->frameHeight))
			goto error;
	}
	else if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
	{
	}
	else if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "ProcessOutput failure: 0x%08"PRIX32"", hr);
		goto error;
	}
	else
	{
		int offset = 0;
		BYTE* buffer = NULL;
		DWORD bufferCount = 0;
		DWORD cbMaxLength = 0;
		DWORD cbCurrentLength = 0;
		hr = sys->outputSample->lpVtbl->GetBufferCount(sys->outputSample, &bufferCount);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetBufferCount failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->outputSample->lpVtbl->GetBufferByIndex(sys->outputSample, 0,
		        &outputBuffer);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetBufferByIndex failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = outputBuffer->lpVtbl->Lock(outputBuffer, &buffer, &cbMaxLength,
		                                &cbCurrentLength);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		CopyMemory(pYUVData[0], &buffer[offset], iStride[0] * sys->frameHeight);
		offset += iStride[0] * sys->frameHeight;
		CopyMemory(pYUVData[1], &buffer[offset], iStride[1] * (sys->frameHeight / 2));
		offset += iStride[1] * (sys->frameHeight / 2);
		CopyMemory(pYUVData[2], &buffer[offset], iStride[2] * (sys->frameHeight / 2));
		offset += iStride[2] * (sys->frameHeight / 2);
		hr = outputBuffer->lpVtbl->Unlock(outputBuffer);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		outputBuffer->lpVtbl->Release(outputBuffer);
	}

	inputSample->lpVtbl->Release(inputSample);
	return 1;
error:
	fprintf(stderr, "mf_decompress error\n");
	return -1;
}

static int mf_compress(H264_CONTEXT* h264, const BYTE** ppSrcYuv, const UINT32* pStride,
                       BYTE** ppDstData, UINT32* pDstSize)
{
	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*) h264->pSystemData;
	return 1;
}

static BOOL mf_plat_loaded(H264_CONTEXT_MF* sys)
{
	return sys->MFStartup && sys->MFShutdown && sys->MFCreateSample
		&& sys->MFCreateMemoryBuffer && sys->MFCreateMediaType;
}

static void mf_uninit(H264_CONTEXT* h264)
{
	UINT32 x;
	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*) h264->pSystemData;

	if (sys)
	{
		if (sys->transform)
		{
			sys->transform->lpVtbl->Release(sys->transform);
			sys->transform = NULL;
		}

		if (sys->codecApi)
		{
			sys->codecApi->lpVtbl->Release(sys->codecApi);
			sys->codecApi = NULL;
		}

		if (sys->inputType)
		{
			sys->inputType->lpVtbl->Release(sys->inputType);
			sys->inputType = NULL;
		}

		if (sys->outputType)
		{
			sys->outputType->lpVtbl->Release(sys->outputType);
			sys->outputType = NULL;
		}

		if (sys->outputSample)
		{
			sys->outputSample->lpVtbl->Release(sys->outputSample);
			sys->outputSample = NULL;
		}

		if (sys->mfplat)
		{
			if (mf_plat_loaded(sys))
				sys->MFShutdown();

			FreeLibrary(sys->mfplat);
			sys->mfplat = NULL;

			if (mf_plat_loaded(sys))
				CoUninitialize();
		}

		for (x = 0; x < sizeof(h264->pYUVData) / sizeof(h264->pYUVData[0]); x++)
			_aligned_free(h264->pYUVData[x]);

		memset(h264->pYUVData, 0, sizeof(h264->pYUVData));
		memset(h264->iStride, 0, sizeof(h264->iStride));
		
		free(sys);
		h264->pSystemData = NULL;
	}
}

static BOOL mf_init(H264_CONTEXT* h264)
{
	HRESULT hr;
	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*) calloc(1, sizeof(H264_CONTEXT_MF));

	if (!sys)
		goto error;

	h264->pSystemData = (void*) sys;
	/* http://decklink-sdk-delphi.googlecode.com/svn/trunk/Blackmagic%20DeckLink%20SDK%209.7/Win/Samples/Streaming/StreamingPreview/DecoderMF.cpp */
	sys->mfplat = LoadLibraryA("mfplat.dll");

	if (!sys->mfplat)
		goto error;

	sys->MFStartup = (pfnMFStartup) GetProcAddress(sys->mfplat, "MFStartup");
	sys->MFShutdown = (pfnMFShutdown) GetProcAddress(sys->mfplat, "MFShutdown");
	sys->MFCreateSample = (pfnMFCreateSample) GetProcAddress(sys->mfplat,
	                      "MFCreateSample");
	sys->MFCreateMemoryBuffer = (pfnMFCreateMemoryBuffer) GetProcAddress(
	                                sys->mfplat, "MFCreateMemoryBuffer");
	sys->MFCreateMediaType = (pfnMFCreateMediaType) GetProcAddress(sys->mfplat,
	                         "MFCreateMediaType");

	if (!mf_plat_loaded(sys))
		goto error;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	if (h264->Compressor)
	{
	}
	else
	{
		VARIANT var = { 0 };
		hr = sys->MFStartup(MF_VERSION, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "MFStartup failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = CoCreateInstance(&sCLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,
		                      &sIID_IMFTransform, (void**) &sys->transform);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "CoCreateInstance(CLSID_CMSH264DecoderMFT) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->QueryInterface(sys->transform, &sIID_ICodecAPI,
		        (void**) &sys->codecApi);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "QueryInterface(IID_ICodecAPI) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		var.vt = VT_UI4;
		var.ulVal = 1;
		hr = sys->codecApi->lpVtbl->SetValue(sys->codecApi, &sCODECAPI_AVLowLatencyMode,
		                                     &var);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetValue(CODECAPI_AVLowLatencyMode) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->MFCreateMediaType(&sys->inputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "MFCreateMediaType failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->inputType->lpVtbl->SetGUID(sys->inputType, &sMF_MT_MAJOR_TYPE,
		                                     &sMFMediaType_Video);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetGUID(MF_MT_MAJOR_TYPE) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->inputType->lpVtbl->SetGUID(sys->inputType, &sMF_MT_SUBTYPE,
		                                     &sMFVideoFormat_H264);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetGUID(MF_MT_SUBTYPE) failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetInputType(sys->transform, 0, sys->inputType, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetInputType failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = mf_find_output_type(sys, &sMFVideoFormat_IYUV, &sys->outputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_find_output_type failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetOutputType(sys->transform, 0, sys->outputType,
		        0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetOutputType failure: 0x%08"PRIX32"", hr);
			goto error;
		}

		hr = mf_create_output_sample(h264, sys);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08"PRIX32"", hr);
			goto error;
		}
	}

	return TRUE;
error:
	WLog_Print(h264->log, WLOG_ERROR, "mf_init failure");
	mf_uninit(h264);
	return FALSE;
}

H264_CONTEXT_SUBSYSTEM g_Subsystem_MF =
{
	"MediaFoundation",
	mf_init,
	mf_uninit,
	mf_decompress,
	mf_compress
};

