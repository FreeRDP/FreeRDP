/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
 * Copyright 2015 Vic Lee <llyzs.vic@gmail.com>
 * Copyright 2014 Armin Novak <armin.novak@gmail.com>
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

#include "wels/codec_def.h"
#include "wels/codec_api.h"
#include "wels/codec_ver.h"

#define TAG FREERDP_TAG("codec")

#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR < 3) || (OPENH264_MAJOR < 1)
#error "Unsupported OpenH264 version "OPENH264_MAJOR"."OPENH264_MINOR"."OPENH264_REVISION" detected!"
#elif (OPENH264_MAJOR > 1) || (OPENH264_MINOR > 6)
#warning "Untested OpenH264 version "OPENH264_MAJOR"."OPENH264_MINOR"."OPENH264_REVISION" detected!"
#endif

struct _H264_CONTEXT_OPENH264
{
	ISVCDecoder* pDecoder;
	ISVCEncoder* pEncoder;
	SEncParamExt EncParamExt;
};
typedef struct _H264_CONTEXT_OPENH264 H264_CONTEXT_OPENH264;

static BOOL g_openh264_trace_enabled = FALSE;

static void openh264_trace_callback(H264_CONTEXT* h264, int level,
                                    const char* message)
{
	WLog_INFO(TAG, "%d - %s", level, message);
}

static int openh264_decompress(H264_CONTEXT* h264, const BYTE* pSrcData,
                               UINT32 SrcSize)
{
	DECODING_STATE state;
	SBufferInfo sBufferInfo;
	SSysMEMBuffer* pSystemBuffer;
	H264_CONTEXT_OPENH264* sys = (H264_CONTEXT_OPENH264*) h264->pSystemData;
	UINT32* iStride = h264->iStride;
	BYTE** pYUVData = h264->pYUVData;

	if (!sys->pDecoder)
		return -2001;

	/*
	 * Decompress the image.  The RDP host only seems to send I420 format.
	 */
	pYUVData[0] = NULL;
	pYUVData[1] = NULL;
	pYUVData[2] = NULL;
	ZeroMemory(&sBufferInfo, sizeof(sBufferInfo));
	state = (*sys->pDecoder)->DecodeFrame2(sys->pDecoder, pSrcData, SrcSize,
	                                       pYUVData, &sBufferInfo);

	if (sBufferInfo.iBufferStatus != 1)
	{
		if (state == dsNoParamSets)
		{
			/* this happens on the first frame due to missing parameter sets */
			state = (*sys->pDecoder)->DecodeFrame2(sys->pDecoder, NULL, 0, pYUVData,
			                                       &sBufferInfo);
		}
		else if (state == dsErrorFree)
		{
			/* call DecodeFrame2 again to decode without delay */
			state = (*sys->pDecoder)->DecodeFrame2(sys->pDecoder, NULL, 0, pYUVData,
			                                       &sBufferInfo);
		}
		else
		{
			WLog_WARN(TAG, "DecodeFrame2 state: 0x%04X iBufferStatus: %d", state,
			          sBufferInfo.iBufferStatus);
			return -2002;
		}
	}

	pSystemBuffer = &sBufferInfo.UsrData.sSystemBuffer;
	iStride[0] = pSystemBuffer->iStride[0];
	iStride[1] = pSystemBuffer->iStride[1];
	iStride[2] = pSystemBuffer->iStride[1];

	if (sBufferInfo.iBufferStatus != 1)
	{
		WLog_WARN(TAG, "DecodeFrame2 iBufferStatus: %d", sBufferInfo.iBufferStatus);
		return 0;
	}

	if (state != dsErrorFree)
	{
		WLog_WARN(TAG, "DecodeFrame2 state: 0x%02X", state);
		return -2003;
	}

#if 0
	WLog_INFO(TAG,
	          "h264_decompress: state=%u, pYUVData=[%p,%p,%p], bufferStatus=%d, width=%d, height=%d, format=%d, stride=[%d,%d]",
	          state, (void*) pYUVData[0], (void*) pYUVData[1], (void*) pYUVData[2], sBufferInfo.iBufferStatus,
	          pSystemBuffer->iWidth, pSystemBuffer->iHeight, pSystemBuffer->iFormat,
	          pSystemBuffer->iStride[0], pSystemBuffer->iStride[1]);
#endif

	if (pSystemBuffer->iFormat != videoFormatI420)
		return -2004;

	if (!pYUVData[0] || !pYUVData[1] || !pYUVData[2])
		return -2005;

	return 1;
}

static int openh264_compress(H264_CONTEXT* h264, BYTE** ppDstData,
                             UINT32* pDstSize)
{
	int i, j;
	int status;
	SFrameBSInfo info;
	SSourcePicture pic;
	SBitrateInfo bitrate;
	H264_CONTEXT_OPENH264* sys;
	BYTE** pYUVData = h264->pYUVData;
	UINT32* iStride = h264->iStride;
	sys = &((H264_CONTEXT_OPENH264*) h264->pSystemData)[0];

	if (!sys->pEncoder)
		return -1;

	if (!pYUVData[0] || !pYUVData[1] || !pYUVData[2])
		return -1;

	if ((sys->EncParamExt.iPicWidth != h264->width)
	    || (sys->EncParamExt.iPicHeight != h264->height))
	{
		status = (*sys->pEncoder)->GetDefaultParams(sys->pEncoder, &sys->EncParamExt);

		if (status < 0)
		{
			WLog_ERR(TAG, "Failed to get OpenH264 default parameters (status=%d)", status);
			return status;
		}

		sys->EncParamExt.iUsageType = SCREEN_CONTENT_REAL_TIME;
		sys->EncParamExt.iPicWidth = h264->width;
		sys->EncParamExt.iPicHeight = h264->height;
		sys->EncParamExt.fMaxFrameRate = h264->FrameRate;
		sys->EncParamExt.iMaxBitrate = UNSPECIFIED_BIT_RATE;
		sys->EncParamExt.bEnableDenoise = 0;
		sys->EncParamExt.bEnableLongTermReference = 0;
		sys->EncParamExt.bEnableFrameSkip = 0;
		sys->EncParamExt.iSpatialLayerNum = 1;
		sys->EncParamExt.iMultipleThreadIdc = h264->NumberOfThreads;
		sys->EncParamExt.sSpatialLayers[0].fFrameRate = h264->FrameRate;
		sys->EncParamExt.sSpatialLayers[0].iVideoWidth = sys->EncParamExt.iPicWidth;
		sys->EncParamExt.sSpatialLayers[0].iVideoHeight = sys->EncParamExt.iPicHeight;
		sys->EncParamExt.sSpatialLayers[0].iMaxSpatialBitrate =
		    sys->EncParamExt.iMaxBitrate;

		switch (h264->RateControlMode)
		{
			case H264_RATECONTROL_VBR:
				sys->EncParamExt.iRCMode = RC_BITRATE_MODE;
				sys->EncParamExt.iTargetBitrate = h264->BitRate;
				sys->EncParamExt.sSpatialLayers[0].iSpatialBitrate =
				    sys->EncParamExt.iTargetBitrate;
				break;

			case H264_RATECONTROL_CQP:
				sys->EncParamExt.iRCMode = RC_OFF_MODE;
				sys->EncParamExt.sSpatialLayers[0].iDLayerQp = h264->QP;
				break;
		}

		if (sys->EncParamExt.iMultipleThreadIdc > 1)
		{
#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR <= 5)
			sys->EncParamExt.sSpatialLayers[0].sSliceCfg.uiSliceMode = SM_AUTO_SLICE;
#else
			sys->EncParamExt.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_FIXEDSLCNUM_SLICE;
#endif
		}

		status = (*sys->pEncoder)->InitializeExt(sys->pEncoder, &sys->EncParamExt);

		if (status < 0)
		{
			WLog_ERR(TAG, "Failed to initialize OpenH264 encoder (status=%d)", status);
			return status;
		}

		status = (*sys->pEncoder)->GetOption(sys->pEncoder,
		                                     ENCODER_OPTION_SVC_ENCODE_PARAM_EXT,
		                                     &sys->EncParamExt);

		if (status < 0)
		{
			WLog_ERR(TAG, "Failed to get initial OpenH264 encoder parameters (status=%d)",
			         status);
			return status;
		}
	}
	else
	{
		switch (h264->RateControlMode)
		{
			case H264_RATECONTROL_VBR:
				if (sys->EncParamExt.iTargetBitrate != h264->BitRate)
				{
					sys->EncParamExt.iTargetBitrate = h264->BitRate;
					bitrate.iLayer = SPATIAL_LAYER_ALL;
					bitrate.iBitrate = h264->BitRate;
					status = (*sys->pEncoder)->SetOption(sys->pEncoder, ENCODER_OPTION_BITRATE,
					                                     &bitrate);

					if (status < 0)
					{
						WLog_ERR(TAG, "Failed to set encoder bitrate (status=%d)", status);
						return status;
					}
				}

				if (sys->EncParamExt.fMaxFrameRate != h264->FrameRate)
				{
					sys->EncParamExt.fMaxFrameRate = h264->FrameRate;
					status = (*sys->pEncoder)->SetOption(sys->pEncoder, ENCODER_OPTION_FRAME_RATE,
					                                     &sys->EncParamExt.fMaxFrameRate);

					if (status < 0)
					{
						WLog_ERR(TAG, "Failed to set encoder framerate (status=%d)", status);
						return status;
					}
				}

				break;

			case H264_RATECONTROL_CQP:
				if (sys->EncParamExt.sSpatialLayers[0].iDLayerQp != h264->QP)
				{
					sys->EncParamExt.sSpatialLayers[0].iDLayerQp = h264->QP;
					status = (*sys->pEncoder)->SetOption(sys->pEncoder,
					                                     ENCODER_OPTION_SVC_ENCODE_PARAM_EXT,
					                                     &sys->EncParamExt);

					if (status < 0)
					{
						WLog_ERR(TAG, "Failed to set encoder parameters (status=%d)", status);
						return status;
					}
				}

				break;
		}
	}

	memset(&info, 0, sizeof(SFrameBSInfo));
	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = h264->width;
	pic.iPicHeight = h264->height;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = iStride[0];
	pic.iStride[1] = iStride[1];
	pic.iStride[2] = iStride[2];
	pic.pData[0] = pYUVData[0];
	pic.pData[1] = pYUVData[1];
	pic.pData[2] = pYUVData[2];
	status = (*sys->pEncoder)->EncodeFrame(sys->pEncoder, &pic, &info);

	if (status < 0)
	{
		WLog_ERR(TAG, "Failed to encode frame (status=%d)", status);
		return status;
	}

	*ppDstData = info.sLayerInfo[0].pBsBuf;
	*pDstSize = 0;

	for (i = 0; i < info.iLayerNum; i++)
	{
		for (j = 0; j < info.sLayerInfo[i].iNalCount; j++)
		{
			*pDstSize += info.sLayerInfo[i].pNalLengthInByte[j];
		}
	}

	return 1;
}

static void openh264_uninit(H264_CONTEXT* h264)
{
	UINT32 x;
	H264_CONTEXT_OPENH264* sysContexts = (H264_CONTEXT_OPENH264*) h264->pSystemData;

	if (sysContexts)
	{
		for (x = 0; x < h264->numSystemData; x++)
		{
			H264_CONTEXT_OPENH264* sys = &sysContexts[x];

			if (sys->pDecoder)
			{
				(*sys->pDecoder)->Uninitialize(sys->pDecoder);
				WelsDestroyDecoder(sys->pDecoder);
				sys->pDecoder = NULL;
			}

			if (sys->pEncoder)
			{
				(*sys->pEncoder)->Uninitialize(sys->pEncoder);
				WelsDestroySVCEncoder(sys->pEncoder);
				sys->pEncoder = NULL;
			}
		}

		free(h264->pSystemData);
		h264->pSystemData = NULL;
	}
}

static BOOL openh264_init(H264_CONTEXT* h264)
{
	UINT32 x;
	long status;
	SDecodingParam sDecParam;
	H264_CONTEXT_OPENH264* sysContexts;
	static int traceLevel = WELS_LOG_DEBUG;
#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR <= 5)
	static EVideoFormatType videoFormat = videoFormatI420;
#endif
	static WelsTraceCallback traceCallback = (WelsTraceCallback)
	        openh264_trace_callback;
	h264->numSystemData = 1;
	sysContexts = (H264_CONTEXT_OPENH264*) calloc(h264->numSystemData,
	              sizeof(H264_CONTEXT_OPENH264));

	if (!sysContexts)
		goto EXCEPTION;

	h264->pSystemData = (void*) sysContexts;

	for (x = 0; x < h264->numSystemData; x++)
	{
		H264_CONTEXT_OPENH264* sys = &sysContexts[x];

		if (h264->Compressor)
		{
			WelsCreateSVCEncoder(&sys->pEncoder);

			if (!sys->pEncoder)
			{
				WLog_ERR(TAG, "Failed to create OpenH264 encoder");
				goto EXCEPTION;
			}
		}
		else
		{
			WelsCreateDecoder(&sys->pDecoder);

			if (!sys->pDecoder)
			{
				WLog_ERR(TAG, "Failed to create OpenH264 decoder");
				goto EXCEPTION;
			}

			ZeroMemory(&sDecParam, sizeof(sDecParam));
#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR <= 5)
			sDecParam.eOutputColorFormat  = videoFormatI420;
#endif
			sDecParam.eEcActiveIdc = ERROR_CON_FRAME_COPY;
			sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
			status = (*sys->pDecoder)->Initialize(sys->pDecoder, &sDecParam);

			if (status != 0)
			{
				WLog_ERR(TAG, "Failed to initialize OpenH264 decoder (status=%ld)",
				         status);
				goto EXCEPTION;
			}

#if (OPENH264_MAJOR == 1) && (OPENH264_MINOR <= 5)
			status = (*sys->pDecoder)->SetOption(
			             sys->pDecoder, DECODER_OPTION_DATAFORMAT,
			             &videoFormat);
#endif

			if (status != 0)
			{
				WLog_ERR(TAG,
				         "Failed to set data format option on OpenH264 decoder (status=%ld)",
				         status);
				goto EXCEPTION;
			}

			if (g_openh264_trace_enabled)
			{
				status = (*sys->pDecoder)->SetOption(
				             sys->pDecoder, DECODER_OPTION_TRACE_LEVEL,
				             &traceLevel);

				if (status != 0)
				{
					WLog_ERR(TAG,
					         "Failed to set trace level option on OpenH264 decoder (status=%ld)",
					         status);
					goto EXCEPTION;
				}

				status = (*sys->pDecoder)->SetOption(
				             sys->pDecoder, DECODER_OPTION_TRACE_CALLBACK,
				             &traceCallback);

				if (status != 0)
				{
					WLog_ERR(TAG,
					         "Failed to set trace callback option on OpenH264 decoder (status=%ld)",
					         status);
					goto EXCEPTION;
				}

				status = (*sys->pDecoder)->SetOption(
				             sys->pDecoder,
				             DECODER_OPTION_TRACE_CALLBACK_CONTEXT,
				             &h264);

				if (status != 0)
				{
					WLog_ERR(TAG,
					         "Failed to set trace callback context option on OpenH264 decoder (status=%ld)",
					         status);
					goto EXCEPTION;
				}
			}
		}
	}

	return TRUE;
EXCEPTION:
	openh264_uninit(h264);
	return FALSE;
}

H264_CONTEXT_SUBSYSTEM g_Subsystem_OpenH264 =
{
	"OpenH264",
	openh264_init,
	openh264_uninit,
	openh264_decompress,
	openh264_compress
};
