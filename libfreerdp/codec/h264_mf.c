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

#include <winpr/winpr.h>
#include <winpr/library.h>

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

static const GUID sCLSID_CMSH264DecoderMFT = {
	0x62CE7E72, 0x4C71, 0x4d20, { 0xB1, 0x5D, 0x45, 0x28, 0x31, 0xA8, 0x7D, 0x9D }
};
static const GUID sIID_IMFTransform = {
	0xbf94c121, 0x5b05, 0x4e6f, { 0x80, 0x00, 0xba, 0x59, 0x89, 0x61, 0x41, 0x4d }
};
static const GUID sMF_MT_MAJOR_TYPE = {
	0x48eba18e, 0xf8c9, 0x4687, { 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f }
};
static const GUID sMF_MT_FRAME_SIZE = {
	0x1652c33d, 0xd6b2, 0x4012, { 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d }
};
static const GUID sMF_MT_DEFAULT_STRIDE = {
	0x644b4e48, 0x1e02, 0x4516, { 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6 }
};
static const GUID sMF_MT_SUBTYPE = {
	0xf7e34c9a, 0x42e8, 0x4714, { 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5 }
};
static const GUID sMFMediaType_Video = {
	0x73646976, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 }
};
static const GUID sMFVideoFormat_H264 = {
	0x34363248, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};
static const GUID sMFVideoFormat_IYUV = {
	0x56555949, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};
static const GUID sIID_ICodecAPI = {
	0x901db4c7, 0x31ce, 0x41a2, { 0x85, 0xdc, 0x8f, 0xa0, 0xbf, 0x41, 0xb8, 0xda }
};
static const GUID sCODECAPI_AVLowLatencyMode = {
	0x9c27891a, 0xed7a, 0x40e1, { 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee }
};

/* encoder */
static const GUID sCLSID_CMSH264EncoderMFT = {
	0x6ca50344, 0x051a, 0x4ded, { 0x97, 0x79, 0xa4, 0x33, 0x05, 0x16, 0x5e, 0x35 }
};
static const GUID sMFVideoFormat_NV12 = {
	0x3231564e, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 }
};
static const GUID sMF_MT_FRAME_RATE = {
	0xc459a2e8, 0x3d2c, 0x4e44, { 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0 }
};
static const GUID sMF_MT_PIXEL_ASPECT_RATIO = {
	0xc6376a1e, 0x8d0a, 0x4027, { 0xbe, 0x45, 0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6 }
};
static const GUID sMF_MT_INTERLACE_MODE = {
	0xe2724bb8, 0xe676, 0x4806, { 0xb4, 0xb2, 0xa8, 0xd6, 0xef, 0xb4, 0x4c, 0xcd }
};
static const GUID sMF_MT_AVG_BITRATE = {
	0x20332624, 0xfb0d, 0x4d9e, { 0xbd, 0x0d, 0xcb, 0xf6, 0x78, 0x6c, 0x10, 0x2e }
};
static const GUID sMF_MT_MPEG2_PROFILE = {
	0xad76a80b, 0x2d5c, 0x4e0b, { 0xb3, 0x75, 0x64, 0xe5, 0x20, 0x13, 0x70, 0x36 }
};
static const GUID sCODECAPI_AVEncCommonRateControlMode = {
	0x1c0608e9, 0x370c, 0x4710, { 0x8a, 0x58, 0xcb, 0x61, 0x81, 0xc4, 0x24, 0x23 }
};
static const GUID sCODECAPI_AVEncCommonMeanBitRate = {
	0xf7222374, 0x2144, 0x4815, { 0xb5, 0x50, 0xa3, 0x7f, 0x8e, 0x12, 0xee, 0x52 }
};
static const GUID sCODECAPI_AVEncCommonQuality = {
	0xfcbf57a3, 0x7ea5, 0x4b0c, { 0x96, 0x44, 0x69, 0xb4, 0x0c, 0x39, 0xc3, 0x91 }
};
static const GUID sCODECAPI_AVEncVideoEncodeQP = {
	0x2cb5696b, 0x23fb, 0x4ce1, { 0xa0, 0xf9, 0xef, 0x5b, 0x90, 0xfd, 0x55, 0xca }
};
static const GUID sCODECAPI_AVEncMPVGOPSize = {
	0x95f31b26, 0x95a4, 0x41aa, { 0x93, 0x03, 0x24, 0x6a, 0x7f, 0xc6, 0xee, 0xf1 }
};

/* values from codecapi.h and mfobjects.h, spelled out like the GUIDs above */
#define FREERDP_MF_RC_CBR 0                /* eAVEncCommonRateControlMode_CBR */
#define FREERDP_MF_RC_QUALITY 3            /* eAVEncCommonRateControlMode_Quality */
#define FREERDP_MF_PROFILE_BASELINE 66     /* eAVEncH264VProfile_Base */
#define FREERDP_MF_INTERLACE_PROGRESSIVE 2 /* MFVideoInterlace_Progressive */

typedef HRESULT(__stdcall* pfnMFStartup)(ULONG Version, DWORD dwFlags);
typedef HRESULT(__stdcall* pfnMFShutdown)(void);
typedef HRESULT(__stdcall* pfnMFCreateSample)(IMFSample** ppIMFSample);
typedef HRESULT(__stdcall* pfnMFCreateMemoryBuffer)(DWORD cbMaxLength, IMFMediaBuffer** ppBuffer);
typedef HRESULT(__stdcall* pfnMFCreateMediaType)(IMFMediaType** ppMFType);

typedef struct
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
	UINT32 stride;
	BOOL bottomUp;

	/* encoder state: the media types are set on the first frame and again on a size change */
	BOOL encStreaming;
	UINT32 encWidth;
	UINT32 encHeight;
	UINT32 encFrameRate;
	UINT32 encBitRate;
	UINT32 encQP;
	H264_RATECONTROL_MODE encRateControl;
	DWORD encOutputFlags;
	DWORD encOutputSize;
	LONGLONG encSampleTime;
	BYTE* encData;
	size_t encDataSize;
	size_t encDataLength;
	UINT32 encHeld;
	BOOL encWarned;
} H264_CONTEXT_MF;

WINPR_ATTR_NODISCARD
static HRESULT mf_find_output_type(H264_CONTEXT_MF* sys, const GUID* guid,
                                   IMFMediaType** ppMediaType)
{
	DWORD idx = 0;
	GUID mediaGuid = WINPR_C_ARRAY_INIT;
	HRESULT hr = S_OK;
	IMFMediaType* pMediaType = nullptr;

	WINPR_ASSERT(sys);
	WINPR_ASSERT(sys->transform);
	WINPR_ASSERT(guid);
	WINPR_ASSERT(ppMediaType);

	while (1)
	{
		hr = sys->transform->lpVtbl->GetOutputAvailableType(sys->transform, 0, idx, &pMediaType);

		if (FAILED(hr))
			break;

		/* a type without a subtype can't be the one we look for, try the next */
		if (SUCCEEDED(pMediaType->lpVtbl->GetGUID(pMediaType, &sMF_MT_SUBTYPE, &mediaGuid)) &&
		    IsEqualGUID(&mediaGuid, guid))
		{
			*ppMediaType = pMediaType;
			return S_OK;
		}

		pMediaType->lpVtbl->Release(pMediaType);
		idx++;
	}

	return hr;
}

WINPR_ATTR_NODISCARD
static HRESULT mf_create_output_sample(H264_CONTEXT* h264, H264_CONTEXT_MF* sys)
{
	HRESULT hr = S_OK;
	MFT_OUTPUT_STREAM_INFO streamInfo;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);
	WINPR_ASSERT(sys->transform);

	if (sys->outputSample)
	{
		sys->outputSample->lpVtbl->Release(sys->outputSample);
		sys->outputSample = nullptr;
	}

	hr = sys->MFCreateSample(&sys->outputSample);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateSample failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = sys->transform->lpVtbl->GetOutputStreamInfo(sys->transform, 0, &streamInfo);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "GetOutputStreamInfo failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = sys->MFCreateMemoryBuffer(streamInfo.cbSize, &sys->outputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateMemoryBuffer failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = sys->outputSample->lpVtbl->AddBuffer(sys->outputSample, sys->outputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "AddBuffer failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	sys->outputBuffer->lpVtbl->Release(sys->outputBuffer);
error:
	return hr;
}

/* IYUV output: full size Y plane followed by the quarter size U and V planes */
WINPR_ATTR_NODISCARD
static size_t getSrcOffset(const H264_CONTEXT_MF* sys, UINT32 line, size_t plane)
{
	WINPR_ASSERT(sys);

	const size_t stride = (plane == 0) ? sys->stride : sys->stride / 2u;
	const size_t height = (plane == 0) ? sys->frameHeight : sys->frameHeight / 2u;
	size_t offset = 0;
	if (plane > 0)
		offset += (size_t)sys->stride * sys->frameHeight;
	if (plane > 1)
		offset += stride * height;

	WINPR_ASSERT(line < height);
	const size_t row = sys->bottomUp ? height - line - 1u : line;
	return offset + row * stride;
}

WINPR_ATTR_NODISCARD
static HRESULT mf_process_output(H264_CONTEXT_MF* sys)
{
	DWORD status = 0;
	MFT_OUTPUT_DATA_BUFFER buffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(sys);
	WINPR_ASSERT(sys->transform);

	buffer.pSample = sys->outputSample;
	const HRESULT hr =
	    sys->transform->lpVtbl->ProcessOutput(sys->transform, 0, 1, &buffer, &status);
	if (buffer.pEvents)
		buffer.pEvents->lpVtbl->Release(buffer.pEvents);
	return hr;
}

WINPR_ATTR_NODISCARD
static int mf_decompress(H264_CONTEXT* WINPR_RESTRICT h264, const BYTE* WINPR_RESTRICT pSrcData,
                         UINT32 SrcSize)
{
	int rc = -1;
	BYTE* pbBuffer = nullptr;
	DWORD cbMaxLength = 0;
	DWORD cbCurrentLength = 0;
	IMFSample* inputSample = nullptr;
	IMFMediaBuffer* inputBuffer = nullptr;
	IMFMediaBuffer* outputBuffer = nullptr;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcData);

	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*)h264->pSystemData;
	WINPR_ASSERT(sys);
	UINT32* iStride = h264->iStride;
	BYTE** pYUVData = h264->pYUVData;
	HRESULT hr = sys->MFCreateMemoryBuffer(SrcSize, &inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateMemoryBuffer failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = inputBuffer->lpVtbl->Lock(inputBuffer, &pbBuffer, &cbMaxLength, &cbCurrentLength);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	CopyMemory(pbBuffer, pSrcData, SrcSize);
	hr = inputBuffer->lpVtbl->SetCurrentLength(inputBuffer, SrcSize);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "SetCurrentLength failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = inputBuffer->lpVtbl->Unlock(inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = sys->MFCreateSample(&inputSample);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateSample failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = inputSample->lpVtbl->AddBuffer(inputSample, inputBuffer);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "AddBuffer failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = sys->transform->lpVtbl->ProcessInput(sys->transform, 0, inputSample, 0);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "ProcessInput failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = mf_create_output_sample(h264, sys);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08" PRIX32 "", hr);
		goto error;
	}

	hr = mf_process_output(sys);

	if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
	{
		UINT64 frameSize = 0;

		if (sys->outputType)
		{
			sys->outputType->lpVtbl->Release(sys->outputType);
			sys->outputType = nullptr;
		}

		hr = mf_find_output_type(sys, &sMFVideoFormat_IYUV, &sys->outputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_find_output_type failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetOutputType(sys->transform, 0, sys->outputType, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetOutputType failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = mf_create_output_sample(h264, sys);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08" PRIX32 "",
			           hr);
			goto error;
		}

		hr = sys->outputType->lpVtbl->GetUINT64(sys->outputType, &sMF_MT_FRAME_SIZE, &frameSize);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "GetUINT64(MF_MT_FRAME_SIZE) failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		sys->frameWidth = (UINT32)(frameSize >> 32);
		sys->frameHeight = (UINT32)frameSize;
		hr = sys->outputType->lpVtbl->GetUINT32(sys->outputType, &sMF_MT_DEFAULT_STRIDE,
		                                        &sys->stride);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "GetUINT32(MF_MT_DEFAULT_STRIDE) failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		const INT32 istride = (INT32)sys->stride;
		sys->bottomUp = (istride < 0);
		if (sys->bottomUp)
			sys->stride = istride * -1;

		if (!avc420_ensure_buffer(h264, sys->stride, sys->frameWidth, sys->frameHeight))
			goto error;

		/* the decoder still holds the frame, fetch it with the new output type */
		hr = mf_process_output(sys);
	}

	if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
		rc = 0;
	else if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "ProcessOutput failure: 0x%08" PRIX32 "", hr);
		goto error;
	}
	else
	{
		BYTE* buffer = nullptr;
		DWORD bufferCount = 0;
		DWORD cbMaxLength = 0;
		DWORD cbCurrentLength = 0;
		hr = sys->outputSample->lpVtbl->GetBufferCount(sys->outputSample, &bufferCount);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetBufferCount failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->outputSample->lpVtbl->GetBufferByIndex(sys->outputSample, 0, &outputBuffer);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "GetBufferByIndex failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = outputBuffer->lpVtbl->Lock(outputBuffer, &buffer, &cbMaxLength, &cbCurrentLength);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		if ((sys->stride == 0) || (cbCurrentLength / 3u / sys->stride < sys->frameHeight / 2u))
		{
			WLog_Print(h264->log, WLOG_ERROR, "output buffer too small: %" PRIu32 "",
			           cbCurrentLength);
			(void)outputBuffer->lpVtbl->Unlock(outputBuffer);
			goto error;
		}

		/* Copy data from decoder buffer to our YUV buffer.
		 * strides differ (the YUV buffer is always larger) so copy only data available from the
		 * decoder buffer but increment the YUV buffer with the YUV buffer strides.
		 */
		for (UINT32 x = 0; x < sys->frameHeight; x++)
		{
			const size_t srcOffset = getSrcOffset(sys, x, 0);
			const UINT32 dstOffset = (UINT32)iStride[0] * x;
			CopyMemory(&pYUVData[0][dstOffset], &buffer[srcOffset], sys->stride);
		}
		for (UINT32 x = 0; x < sys->frameHeight / 2; x++)
		{
			const size_t srcUOffset = getSrcOffset(sys, x, 1);
			const size_t srcVOffset = getSrcOffset(sys, x, 2);
			const UINT32 dstUOffset = (UINT32)iStride[1] * x;
			const UINT32 dstVOffset = (UINT32)iStride[2] * x;
			CopyMemory(&pYUVData[1][dstUOffset], &buffer[srcUOffset], sys->stride / 2u);
			CopyMemory(&pYUVData[2][dstVOffset], &buffer[srcVOffset], sys->stride / 2u);
		}

		hr = outputBuffer->lpVtbl->Unlock(outputBuffer);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		h264->YUVWidth = sys->frameWidth;
		h264->YUVHeight = sys->frameHeight;
		rc = 1;
	}

error:
	if (outputBuffer)
		outputBuffer->lpVtbl->Release(outputBuffer);
	if (inputSample)
		inputSample->lpVtbl->Release(inputSample);
	if (inputBuffer)
		inputBuffer->lpVtbl->Release(inputBuffer);
	if (rc < 0)
		WLog_Print(h264->log, WLOG_ERROR, "decompression failed");
	return rc;
}

WINPR_ATTR_NODISCARD
static HRESULT mf_encoder_set_value(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, const GUID* guid,
                                    const char* name, VARTYPE vt, UINT64 value)
{
	VARIANT var = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);
	WINPR_ASSERT(guid);
	WINPR_ASSERT(name);

	if (!sys->codecApi)
		return E_NOINTERFACE;

	var.n1.n2.vt = vt;
	if (vt == VT_UI8)
		var.n1.n2.n3.ullVal = value;
	else if (vt == VT_BOOL)
		var.n1.n2.n3.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
	else
		var.n1.n2.n3.ulVal = (ULONG)value;

	const HRESULT hr = sys->codecApi->lpVtbl->SetValue(sys->codecApi, guid, &var);
	if (FAILED(hr))
		WLog_Print(h264->log, WLOG_DEBUG, "ICodecAPI::SetValue(%s) failure: 0x%08" PRIX32 "", name,
		           (UINT32)hr);
	return hr;
}

/* Every ICodecAPI property below tunes the encoder on top of its media types, which carry what
 * the stream needs (size, frame rate, bit rate, profile). An encoder without a property keeps
 * its default and still produces a valid stream, so that is not an error: an encoder without
 * ICodecAPI (E_NOINTERFACE here), or Wine's, which answers E_NOTIMPL to every property. An
 * encoder that has the property (IsSupported) but refuses our value is an error. */
WINPR_ATTR_NODISCARD
static BOOL mf_encoder_check_value(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, const GUID* guid,
                                   const char* name, HRESULT hr)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);
	WINPR_ASSERT(guid);
	WINPR_ASSERT(name);

	if (SUCCEEDED(hr))
		return TRUE;

	if ((hr != E_NOTIMPL) && (hr != E_NOINTERFACE) && sys->codecApi &&
	    (sys->codecApi->lpVtbl->IsSupported(sys->codecApi, guid) == S_OK))
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "H264 encoder refuses ICodecAPI property %s: 0x%08" PRIX32 "", name, (UINT32)hr);
		return FALSE;
	}

	WLog_Print(h264->log, WLOG_DEBUG, "H264 encoder does not support %s, using its default", name);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_set_optional(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, const GUID* guid,
                                    const char* name, VARTYPE vt, UINT64 value)
{
	const HRESULT hr = mf_encoder_set_value(h264, sys, guid, name, vt, value);
	return mf_encoder_check_value(h264, sys, guid, name, hr);
}

static void mf_encoder_release(H264_CONTEXT_MF* sys)
{
	WINPR_ASSERT(sys);

	if (sys->outputSample)
	{
		sys->outputSample->lpVtbl->Release(sys->outputSample);
		sys->outputSample = nullptr;
	}
	if (sys->inputType)
	{
		sys->inputType->lpVtbl->Release(sys->inputType);
		sys->inputType = nullptr;
	}
	if (sys->outputType)
	{
		sys->outputType->lpVtbl->Release(sys->outputType);
		sys->outputType = nullptr;
	}
	if (sys->codecApi)
	{
		sys->codecApi->lpVtbl->Release(sys->codecApi);
		sys->codecApi = nullptr;
	}
	if (sys->transform)
	{
		sys->transform->lpVtbl->Release(sys->transform);
		sys->transform = nullptr;
	}
	sys->encStreaming = FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_create(H264_CONTEXT* h264, H264_CONTEXT_MF* sys)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);

	HRESULT hr = CoCreateInstance(&sCLSID_CMSH264EncoderMFT, nullptr, CLSCTX_INPROC_SERVER,
	                              &sIID_IMFTransform, (void**)&sys->transform);

	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_DEBUG,
		           "CoCreateInstance(CLSID_CMSH264EncoderMFT) failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		return FALSE;
	}

	/* rate control and latency are tuned through ICodecAPI; encode without it if it is missing */
	hr = sys->transform->lpVtbl->QueryInterface(sys->transform, &sIID_ICodecAPI,
	                                            (void**)&sys->codecApi);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_WARN,
		           "H264 encoder has no ICodecAPI (0x%08" PRIX32 "), using its defaults",
		           (UINT32)hr);
		sys->codecApi = nullptr;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static HRESULT mf_encoder_media_type(H264_CONTEXT_MF* sys, IMFMediaType** ppType,
                                     const GUID* subtype, UINT32 width, UINT32 height,
                                     UINT32 frameRate, UINT32 bitRate)
{
	IMFMediaType* type = nullptr;

	WINPR_ASSERT(sys);
	WINPR_ASSERT(sys->MFCreateMediaType);
	WINPR_ASSERT(ppType);
	WINPR_ASSERT(subtype);

	HRESULT hr = sys->MFCreateMediaType(&type);

	if (FAILED(hr))
		return hr;

	hr = type->lpVtbl->SetGUID(type, &sMF_MT_MAJOR_TYPE, &sMFMediaType_Video);
	if (SUCCEEDED(hr))
		hr = type->lpVtbl->SetGUID(type, &sMF_MT_SUBTYPE, subtype);
	if (SUCCEEDED(hr))
		hr = type->lpVtbl->SetUINT64(type, &sMF_MT_FRAME_SIZE, ((UINT64)width << 32) | height);
	if (SUCCEEDED(hr))
		hr = type->lpVtbl->SetUINT64(type, &sMF_MT_FRAME_RATE, ((UINT64)frameRate << 32) | 1);
	if (SUCCEEDED(hr))
		hr = type->lpVtbl->SetUINT64(type, &sMF_MT_PIXEL_ASPECT_RATIO, ((UINT64)1 << 32) | 1);
	if (SUCCEEDED(hr))
		hr =
		    type->lpVtbl->SetUINT32(type, &sMF_MT_INTERLACE_MODE, FREERDP_MF_INTERLACE_PROGRESSIVE);

	if (SUCCEEDED(hr) && IsEqualGUID(subtype, &sMFVideoFormat_H264))
	{
		hr = type->lpVtbl->SetUINT32(type, &sMF_MT_AVG_BITRATE, bitRate);
		/* constrained baseline: no B frames, CAVLC, so every FreeRDP client decoder can read it */
		if (SUCCEEDED(hr))
			hr = type->lpVtbl->SetUINT32(type, &sMF_MT_MPEG2_PROFILE, FREERDP_MF_PROFILE_BASELINE);
	}

	if (FAILED(hr))
	{
		type->lpVtbl->Release(type);
		return hr;
	}

	*ppType = type;
	return S_OK;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_rate_control(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, UINT32 bitRate)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);

	/* without it the encoder holds frames back (4 on Windows Server 2025); mf_compress copes
	 * with that, so the property is optional. It is a VT_BOOL; the encoder answers E_INVALIDARG
	 * to VT_UI4, which other MFTs (the decoder) take, so VT_UI4 is the second try */
	HRESULT hr = mf_encoder_set_value(h264, sys, &sCODECAPI_AVLowLatencyMode, "AVLowLatencyMode",
	                                  VT_BOOL, 1);
	if (FAILED(hr))
		hr = mf_encoder_set_value(h264, sys, &sCODECAPI_AVLowLatencyMode, "AVLowLatencyMode",
		                          VT_UI4, 1);
	if (!mf_encoder_check_value(h264, sys, &sCODECAPI_AVLowLatencyMode, "AVLowLatencyMode", hr))
		return FALSE;

	if (!mf_encoder_set_optional(h264, sys, &sCODECAPI_AVEncMPVGOPSize, "AVEncMPVGOPSize", VT_UI4,
	                             30))
		return FALSE;

	switch (h264->RateControlMode)
	{
		case H264_RATECONTROL_CQP:
		{
			if (!mf_encoder_set_optional(h264, sys, &sCODECAPI_AVEncCommonRateControlMode,
			                             "AVEncCommonRateControlMode", VT_UI4,
			                             FREERDP_MF_RC_QUALITY))
				return FALSE;
			/* AVEncVideoEncodeQP: the QP in the low 16 bits; older encoders only have the
			 * 0..100 quality scale, so when the QP is refused for any reason map it (0..51,
			 * lower is better) onto that scale; the quality scale's result decides */
			if (FAILED(mf_encoder_set_value(h264, sys, &sCODECAPI_AVEncVideoEncodeQP,
			                                "AVEncVideoEncodeQP", VT_UI8, h264->QP & 0xFFFF)))
			{
				const UINT32 qp = MIN(h264->QP, 51);
				if (!mf_encoder_set_optional(h264, sys, &sCODECAPI_AVEncCommonQuality,
				                             "AVEncCommonQuality", VT_UI4, 100 - (qp * 100 / 51)))
					return FALSE;
			}
		}
		break;

		case H264_RATECONTROL_VBR:
		default:
			/* a target bit rate, like OpenH264's RC_BITRATE_MODE; the bit rate is also on the
			 * output type (MF_MT_AVG_BITRATE), which every encoder reads */
			if (!mf_encoder_set_optional(h264, sys, &sCODECAPI_AVEncCommonRateControlMode,
			                             "AVEncCommonRateControlMode", VT_UI4, FREERDP_MF_RC_CBR))
				return FALSE;
			if (!mf_encoder_set_optional(h264, sys, &sCODECAPI_AVEncCommonMeanBitRate,
			                             "AVEncCommonMeanBitRate", VT_UI4, bitRate))
				return FALSE;
			break;
	}

	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_configure(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, UINT32 width,
                                 UINT32 height, UINT32 frameRate, UINT32 bitRate)
{
	MFT_OUTPUT_STREAM_INFO streamInfo = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);

	/* the types of a running encoder can't change, start a new one */
	if (sys->encStreaming || !sys->transform)
	{
		mf_encoder_release(sys);
		if (!mf_encoder_create(h264, sys))
			return FALSE;
	}

	/* rate control is read when the output type is set, so it goes first */
	if (!mf_encoder_rate_control(h264, sys, bitRate))
		goto fail;

	/* the H.264 encoder wants the output type before the input type */
	HRESULT hr = mf_encoder_media_type(sys, &sys->outputType, &sMFVideoFormat_H264, width, height,
	                                   frameRate, bitRate);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder output type failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	hr = sys->transform->lpVtbl->SetOutputType(sys->transform, 0, sys->outputType, 0);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder SetOutputType failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	hr = mf_encoder_media_type(sys, &sys->inputType, &sMFVideoFormat_NV12, width, height, frameRate,
	                           bitRate);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder input type failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	hr = sys->transform->lpVtbl->SetInputType(sys->transform, 0, sys->inputType, 0);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder SetInputType failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	hr = sys->transform->lpVtbl->GetOutputStreamInfo(sys->transform, 0, &streamInfo);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder GetOutputStreamInfo failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	sys->encOutputFlags = streamInfo.dwFlags;
	sys->encOutputSize = streamInfo.cbSize;
	if (sys->encOutputSize == 0)
		sys->encOutputSize = (DWORD)(3ull * width * height / 2); /* checked by mf_compress */

	if ((sys->encOutputFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0)
	{
		IMFMediaBuffer* buffer = nullptr;

		hr = sys->MFCreateSample(&sys->outputSample);
		if (SUCCEEDED(hr))
			hr = sys->MFCreateMemoryBuffer(sys->encOutputSize, &buffer);
		if (SUCCEEDED(hr))
			hr = sys->outputSample->lpVtbl->AddBuffer(sys->outputSample, buffer);
		if (buffer)
			buffer->lpVtbl->Release(buffer);
		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "encoder output sample failure: 0x%08" PRIX32 "",
			           (UINT32)hr);
			goto fail;
		}
	}

	hr = sys->transform->lpVtbl->ProcessMessage(sys->transform, MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
	                                            0);
	if (SUCCEEDED(hr))
		hr = sys->transform->lpVtbl->ProcessMessage(sys->transform,
		                                            MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder ProcessMessage failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto fail;
	}

	sys->encStreaming = TRUE;
	sys->encWidth = width;
	sys->encHeight = height;
	sys->encFrameRate = frameRate;
	sys->encBitRate = bitRate;
	sys->encQP = h264->QP;
	sys->encRateControl = h264->RateControlMode;
	sys->encSampleTime = 0;
	sys->encHeld = 0;
	WLog_Print(h264->log, WLOG_DEBUG,
	           "MediaFoundation H264 encoder: %" PRIu32 "x%" PRIu32 " %" PRIu32
	           " fps, output flags 0x%08" PRIx32 "",
	           width, height, frameRate, (UINT32)sys->encOutputFlags);
	return TRUE;

fail:
	mf_encoder_release(sys);
	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_append(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, IMFSample* sample)
{
	IMFMediaBuffer* buffer = nullptr;
	BYTE* data = nullptr;
	DWORD maxLength = 0;
	DWORD length = 0;
	BOOL rc = FALSE;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);
	WINPR_ASSERT(sample);

	HRESULT hr = sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "ConvertToContiguousBuffer failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		return FALSE;
	}

	hr = buffer->lpVtbl->Lock(buffer, &data, &maxLength, &length);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08" PRIX32 "", (UINT32)hr);
		goto out;
	}

	BOOL copied = FALSE;
	const size_t needed = sys->encDataLength + length;
	if (needed > sys->encDataSize)
	{
		BYTE* tmp = realloc(sys->encData, needed);
		if (tmp)
		{
			sys->encData = tmp;
			sys->encDataSize = needed;
		}
	}
	if (needed <= sys->encDataSize)
	{
		if (length > 0)
			memcpy(&sys->encData[sys->encDataLength], data, length);
		sys->encDataLength = needed;
		copied = TRUE;
	}

	/* one Unlock for both outcomes, so its result is checked on every path */
	hr = buffer->lpVtbl->Unlock(buffer);
	if (FAILED(hr))
		WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08" PRIX32 "", (UINT32)hr);
	else if (!copied)
		WLog_Print(h264->log, WLOG_ERROR, "encoder output: out of memory (%" PRIuz " bytes)",
		           needed);
	else
		rc = TRUE;

out:
	buffer->lpVtbl->Release(buffer);
	return rc;
}

/* collect everything the encoder has ready; MF_E_TRANSFORM_NEED_MORE_INPUT ends it */
WINPR_ATTR_NODISCARD
static HRESULT mf_encoder_drain(H264_CONTEXT* h264, H264_CONTEXT_MF* sys)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);
	WINPR_ASSERT(sys->transform);

	const BOOL provides = (sys->encOutputFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;

	for (size_t i = 0; i < 16; i++)
	{
		DWORD status = 0;
		MFT_OUTPUT_DATA_BUFFER output = WINPR_C_ARRAY_INIT;

		if (!provides)
		{
			/* the encoder writes into our sample: give it an empty buffer each time */
			IMFMediaBuffer* buffer = nullptr;

			WINPR_ASSERT(sys->outputSample);
			HRESULT hr = sys->outputSample->lpVtbl->GetBufferByIndex(sys->outputSample, 0, &buffer);
			if (SUCCEEDED(hr))
			{
				hr = buffer->lpVtbl->SetCurrentLength(buffer, 0);
				buffer->lpVtbl->Release(buffer);
			}
			if (FAILED(hr))
			{
				WLog_Print(h264->log, WLOG_ERROR,
				           "encoder output buffer reset failure: 0x%08" PRIX32 "", (UINT32)hr);
				return hr;
			}
			output.pSample = sys->outputSample;
		}

		const HRESULT hr =
		    sys->transform->lpVtbl->ProcessOutput(sys->transform, 0, 1, &output, &status);
		if (output.pEvents)
			output.pEvents->lpVtbl->Release(output.pEvents);

		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
			return S_OK;

		if (FAILED(hr))
		{
			if (provides && output.pSample)
				output.pSample->lpVtbl->Release(output.pSample);
			WLog_Print(h264->log, WLOG_ERROR, "encoder ProcessOutput failure: 0x%08" PRIX32 "",
			           (UINT32)hr);
			return hr;
		}

		const BOOL ok = output.pSample ? mf_encoder_append(h264, sys, output.pSample) : TRUE;
		if (provides && output.pSample)
			output.pSample->lpVtbl->Release(output.pSample);
		if (!ok)
			return E_FAIL; /* mf_encoder_append logged why */
	}

	return S_OK;
}

WINPR_ATTR_NODISCARD
static BOOL mf_encoder_update(H264_CONTEXT* h264, H264_CONTEXT_MF* sys, UINT32 width, UINT32 height,
                              UINT32 frameRate, UINT32 bitRate)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(sys);

	if (!sys->encStreaming || (sys->encWidth != width) || (sys->encHeight != height) ||
	    (sys->encFrameRate != frameRate) || (sys->encRateControl != h264->RateControlMode))
		return mf_encoder_configure(h264, sys, width, height, frameRate, bitRate);

	/* bit rate and QP can change while the encoder runs; start over where they can't */
	if ((h264->RateControlMode == H264_RATECONTROL_VBR) && (sys->encBitRate != bitRate))
	{
		if (FAILED(mf_encoder_set_value(h264, sys, &sCODECAPI_AVEncCommonMeanBitRate,
		                                "AVEncCommonMeanBitRate", VT_UI4, bitRate)))
			return mf_encoder_configure(h264, sys, width, height, frameRate, bitRate);
		sys->encBitRate = bitRate;
	}

	if ((h264->RateControlMode == H264_RATECONTROL_CQP) && (sys->encQP != h264->QP))
	{
		if (FAILED(mf_encoder_set_value(h264, sys, &sCODECAPI_AVEncVideoEncodeQP,
		                                "AVEncVideoEncodeQP", VT_UI8, h264->QP & 0xFFFF)))
			return mf_encoder_configure(h264, sys, width, height, frameRate, bitRate);
		sys->encQP = h264->QP;
	}

	return TRUE;
}

/* I420 planes as FreeRDP holds them -> one NV12 buffer of width x height */
static void mf_encoder_copy_nv12(BYTE* WINPR_RESTRICT dst, const BYTE** WINPR_RESTRICT pSrcYuv,
                                 const UINT32* WINPR_RESTRICT pStride, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(pSrcYuv);
	WINPR_ASSERT(pSrcYuv[0]);
	WINPR_ASSERT(pSrcYuv[1]);
	WINPR_ASSERT(pSrcYuv[2]);
	WINPR_ASSERT(pStride);

	for (UINT32 y = 0; y < height; y++)
		memcpy(&dst[1ull * y * width], &pSrcYuv[0][1ull * y * pStride[0]], width);

	BYTE* uv = &dst[1ull * width * height];
	for (UINT32 y = 0; y < height / 2; y++)
	{
		const BYTE* u = &pSrcYuv[1][1ull * y * pStride[1]];
		const BYTE* v = &pSrcYuv[2][1ull * y * pStride[2]];
		BYTE* line = &uv[1ull * y * width];

		for (UINT32 x = 0; x < width / 2; x++)
		{
			line[2 * x] = u[x];
			line[2 * x + 1] = v[x];
		}
	}
}

WINPR_ATTR_NODISCARD
static int mf_compress(H264_CONTEXT* WINPR_RESTRICT h264, const BYTE** WINPR_RESTRICT ppSrcYuv,
                       const UINT32* WINPR_RESTRICT pStride, BYTE** WINPR_RESTRICT ppDstData,
                       UINT32* WINPR_RESTRICT pDstSize)
{
	int rc = -1;
	BYTE* data = nullptr;
	DWORD maxLength = 0;
	DWORD curLength = 0;
	IMFSample* sample = nullptr;
	IMFMediaBuffer* buffer = nullptr;

	WINPR_ASSERT(h264);
	WINPR_ASSERT(ppSrcYuv);
	WINPR_ASSERT(pStride);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*)h264->pSystemData;
	if (!sys || !ppSrcYuv[0] || !ppSrcYuv[1] || !ppSrcYuv[2])
		return -1;

	if ((h264->width == 0) || (h264->height == 0) || (h264->width > UINT16_MAX) ||
	    (h264->height > UINT16_MAX))
		return -1;

	/* 4:2:0 needs even sizes; the YUV buffers are padded (yuv_ensure_buffer), so one more
	 * column or row is readable */
	const UINT32 width = (h264->width + 1) & ~1u;
	const UINT32 height = (h264->height + 1) & ~1u;
	const UINT32 frameRate = (h264->FrameRate > 0) ? h264->FrameRate : 30;
	const UINT32 bitRate = (h264->BitRate > 0) ? h264->BitRate : 1000000;

	/* one NV12 frame; the encoder's output buffer is sized the same way */
	const UINT64 size64 = 3ull * width * height / 2;
	if (size64 > UINT32_MAX)
		return -1;
	const DWORD size = (DWORD)size64;

	if (!mf_encoder_update(h264, sys, width, height, frameRate, bitRate))
		return -1;

	HRESULT hr = sys->MFCreateMemoryBuffer(size, &buffer);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "MFCreateMemoryBuffer failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto out;
	}

	hr = buffer->lpVtbl->Lock(buffer, &data, &maxLength, &curLength);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Lock failure: 0x%08" PRIX32 "", (UINT32)hr);
		goto out;
	}

	const BOOL fits = (maxLength >= size);
	if (fits)
		mf_encoder_copy_nv12(data, ppSrcYuv, pStride, width, height);

	/* one Unlock for both outcomes, so its result is checked on every path */
	hr = buffer->lpVtbl->Unlock(buffer);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "Unlock failure: 0x%08" PRIX32 "", (UINT32)hr);
		goto out;
	}
	if (!fits)
	{
		WLog_Print(h264->log, WLOG_ERROR,
		           "encoder input buffer too small: %" PRIu32 " < %" PRIu32 " bytes",
		           (UINT32)maxLength, (UINT32)size);
		goto out;
	}

	hr = buffer->lpVtbl->SetCurrentLength(buffer, size);
	if (SUCCEEDED(hr))
		hr = sys->MFCreateSample(&sample);
	if (SUCCEEDED(hr))
		hr = sample->lpVtbl->AddBuffer(sample, buffer);

	/* 100 ns units; the encoder's rate control works from these */
	const LONGLONG duration = 10000000LL / frameRate;
	if (SUCCEEDED(hr))
		hr = sample->lpVtbl->SetSampleTime(sample, sys->encSampleTime);
	if (SUCCEEDED(hr))
		hr = sample->lpVtbl->SetSampleDuration(sample, duration);
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder input sample failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto out;
	}
	sys->encSampleTime += duration;

	sys->encDataLength = 0;
	hr = sys->transform->lpVtbl->ProcessInput(sys->transform, 0, sample, 0);
	if (hr == MF_E_NOTACCEPTING)
	{
		/* output is waiting first: take it, then the input fits */
		hr = mf_encoder_drain(h264, sys);
		if (SUCCEEDED(hr))
			hr = sys->transform->lpVtbl->ProcessInput(sys->transform, 0, sample, 0);
	}
	if (FAILED(hr))
	{
		WLog_Print(h264->log, WLOG_ERROR, "encoder ProcessInput failure: 0x%08" PRIX32 "",
		           (UINT32)hr);
		goto out;
	}

	hr = mf_encoder_drain(h264, sys);
	if (FAILED(hr))
		goto out;

	if (sys->encDataLength > UINT32_MAX)
		goto out;

	if (sys->encDataLength == 0)
	{
		/* the encoder kept the frame (pipeline delay); nothing to send yet. Microsoft's
		 * encoder in low latency mode never does; an encoder that ignores the mode (look
		 * ahead, B frames) shows every change late, so say so once */
		sys->encHeld++;
		if ((sys->encHeld == 4) && !sys->encWarned)
		{
			WLog_Print(h264->log, WLOG_WARN,
			           "MediaFoundation H264 encoder holds frames back (low latency mode not "
			           "honoured), the client will see changes late");
			sys->encWarned = TRUE;
		}
		rc = 0;
		goto out;
	}

	sys->encHeld = 0;
	*ppDstData = sys->encData;
	*pDstSize = (UINT32)sys->encDataLength;
	rc = 1;

out:
	if (sample)
		sample->lpVtbl->Release(sample);
	if (buffer)
		buffer->lpVtbl->Release(buffer);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL mf_plat_loaded(H264_CONTEXT_MF* sys)
{
	WINPR_ASSERT(sys);
	return sys->MFStartup && sys->MFShutdown && sys->MFCreateSample && sys->MFCreateMemoryBuffer &&
	       sys->MFCreateMediaType;
}

static void mf_uninit(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*)h264->pSystemData;

	if (sys)
	{
		if (sys->transform)
		{
			sys->transform->lpVtbl->Release(sys->transform);
			sys->transform = nullptr;
		}

		if (sys->codecApi)
		{
			sys->codecApi->lpVtbl->Release(sys->codecApi);
			sys->codecApi = nullptr;
		}

		if (sys->inputType)
		{
			sys->inputType->lpVtbl->Release(sys->inputType);
			sys->inputType = nullptr;
		}

		if (sys->outputType)
		{
			sys->outputType->lpVtbl->Release(sys->outputType);
			sys->outputType = nullptr;
		}

		if (sys->outputSample)
		{
			sys->outputSample->lpVtbl->Release(sys->outputSample);
			sys->outputSample = nullptr;
		}

		if (sys->mfplat)
		{
			if (mf_plat_loaded(sys))
			{
				/* nothing to undo in a cleanup path, say so and go on */
				const HRESULT hr = sys->MFShutdown();
				if (FAILED(hr))
					WLog_Print(h264->log, WLOG_WARN, "MFShutdown failure: 0x%08" PRIX32 "",
					           (UINT32)hr);
			}

			FreeLibrary(sys->mfplat);
			sys->mfplat = nullptr;

			if (mf_plat_loaded(sys))
				CoUninitialize();
		}

		for (size_t x = 0; x < sizeof(h264->pYUVData) / sizeof(h264->pYUVData[0]); x++)
			winpr_aligned_free(h264->pYUVData[x]);

		memset(h264->pYUVData, 0, sizeof(h264->pYUVData));
		memset(h264->iStride, 0, sizeof(h264->iStride));

		free(sys->encData);
		free(sys);
		h264->pSystemData = nullptr;
	}
}

WINPR_ATTR_NODISCARD
static BOOL mf_init(H264_CONTEXT* h264)
{
	HRESULT hr;

	WINPR_ASSERT(h264);

	H264_CONTEXT_MF* sys = (H264_CONTEXT_MF*)calloc(1, sizeof(H264_CONTEXT_MF));

	if (!sys)
		goto error;

	h264->pSystemData = (void*)sys;
	/* http://decklink-sdk-delphi.googlecode.com/svn/trunk/Blackmagic%20DeckLink%20SDK%209.7/Win/Samples/Streaming/StreamingPreview/DecoderMF.cpp
	 */
	sys->mfplat = LoadLibraryA("mfplat.dll");

	if (!sys->mfplat)
		goto error;

	sys->MFStartup = GetProcAddressAs(sys->mfplat, "MFStartup", pfnMFStartup);
	sys->MFShutdown = GetProcAddressAs(sys->mfplat, "MFShutdown", pfnMFShutdown);
	sys->MFCreateSample = GetProcAddressAs(sys->mfplat, "MFCreateSample", pfnMFCreateSample);
	sys->MFCreateMemoryBuffer =
	    GetProcAddressAs(sys->mfplat, "MFCreateMemoryBuffer", pfnMFCreateMemoryBuffer);
	sys->MFCreateMediaType =
	    GetProcAddressAs(sys->mfplat, "MFCreateMediaType", pfnMFCreateMediaType);

	if (!mf_plat_loaded(sys))
		goto error;

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	if (h264->Compressor)
	{
		hr = sys->MFStartup(MF_VERSION, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "MFStartup failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		/* the media types need the frame size: they are set on the first frame (mf_compress).
		 * Creating the encoder now lets h264_context_init() try the next subsystem when this
		 * Windows has none (N editions, Server without the Media Foundation feature). */
		if (!mf_encoder_create(h264, sys))
		{
			WLog_Print(h264->log, WLOG_WARN, "MediaFoundation H264 encoder not available");
			goto error;
		}
	}
	else
	{
		VARIANT var = WINPR_C_ARRAY_INIT;
		hr = sys->MFStartup(MF_VERSION, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "MFStartup failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = CoCreateInstance(&sCLSID_CMSH264DecoderMFT, nullptr, CLSCTX_INPROC_SERVER,
		                      &sIID_IMFTransform, (void**)&sys->transform);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "CoCreateInstance(CLSID_CMSH264DecoderMFT) failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->QueryInterface(sys->transform, &sIID_ICodecAPI,
		                                            (void**)&sys->codecApi);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "QueryInterface(IID_ICodecAPI) failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		var.n1.n2.vt = VT_UI4;
		var.n1.n2.n3.ulVal = 1;
		hr = sys->codecApi->lpVtbl->SetValue(sys->codecApi, &sCODECAPI_AVLowLatencyMode, &var);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR,
			           "SetValue(CODECAPI_AVLowLatencyMode) failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->MFCreateMediaType(&sys->inputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "MFCreateMediaType failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->inputType->lpVtbl->SetGUID(sys->inputType, &sMF_MT_MAJOR_TYPE,
		                                     &sMFMediaType_Video);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetGUID(MF_MT_MAJOR_TYPE) failure: 0x%08" PRIX32 "",
			           hr);
			goto error;
		}

		hr = sys->inputType->lpVtbl->SetGUID(sys->inputType, &sMF_MT_SUBTYPE, &sMFVideoFormat_H264);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetGUID(MF_MT_SUBTYPE) failure: 0x%08" PRIX32 "",
			           hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetInputType(sys->transform, 0, sys->inputType, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetInputType failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = mf_find_output_type(sys, &sMFVideoFormat_IYUV, &sys->outputType);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_find_output_type failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = sys->transform->lpVtbl->SetOutputType(sys->transform, 0, sys->outputType, 0);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "SetOutputType failure: 0x%08" PRIX32 "", hr);
			goto error;
		}

		hr = mf_create_output_sample(h264, sys);

		if (FAILED(hr))
		{
			WLog_Print(h264->log, WLOG_ERROR, "mf_create_output_sample failure: 0x%08" PRIX32 "",
			           hr);
			goto error;
		}
	}

	return TRUE;
error:
	WLog_Print(h264->log, WLOG_ERROR, "mf_init failure");
	mf_uninit(h264);
	return FALSE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_MF = { "MediaFoundation", mf_init, mf_uninit,
	                                            mf_decompress, mf_compress };
