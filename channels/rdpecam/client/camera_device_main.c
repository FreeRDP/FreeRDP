/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, Device Channels
 *
 * Copyright 2024 Oleg Turovski <oleg2104@hotmail.com>
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

#include <winpr/assert.h>

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-device.client")

/* supported formats in preference order:
 * H264, MJPG, I420 (used as input for H264 encoder), other YUV based, RGB based
 */
static const CAM_MEDIA_FORMAT_INFO supportedFormats[] = {
/* inputFormat, outputFormat */
#if defined(WITH_INPUT_FORMAT_H264)
	{ CAM_MEDIA_FORMAT_H264, CAM_MEDIA_FORMAT_H264 }, /* passthrough */
#endif
#if defined(WITH_INPUT_FORMAT_MJPG)
	{ CAM_MEDIA_FORMAT_MJPG, CAM_MEDIA_FORMAT_H264 },
#endif
	{ CAM_MEDIA_FORMAT_I420, CAM_MEDIA_FORMAT_H264 },
	{ CAM_MEDIA_FORMAT_YUY2, CAM_MEDIA_FORMAT_H264 },
	{ CAM_MEDIA_FORMAT_NV12, CAM_MEDIA_FORMAT_H264 },
	{ CAM_MEDIA_FORMAT_RGB24, CAM_MEDIA_FORMAT_H264 },
	{ CAM_MEDIA_FORMAT_RGB32, CAM_MEDIA_FORMAT_H264 },
};
static const size_t nSupportedFormats = ARRAYSIZE(supportedFormats);

static void ecam_dev_write_media_type(wStream* s, CAM_MEDIA_TYPE_DESCRIPTION* mediaType)
{
	WINPR_ASSERT(mediaType);

	Stream_Write_UINT8(s, mediaType->Format);
	Stream_Write_UINT32(s, mediaType->Width);
	Stream_Write_UINT32(s, mediaType->Height);
	Stream_Write_UINT32(s, mediaType->FrameRateNumerator);
	Stream_Write_UINT32(s, mediaType->FrameRateDenominator);
	Stream_Write_UINT32(s, mediaType->PixelAspectRatioNumerator);
	Stream_Write_UINT32(s, mediaType->PixelAspectRatioDenominator);
	Stream_Write_UINT8(s, mediaType->Flags);
}

static BOOL ecam_dev_read_media_type(wStream* s, CAM_MEDIA_TYPE_DESCRIPTION* mediaType)
{
	WINPR_ASSERT(mediaType);

	Stream_Read_UINT8(s, mediaType->Format);
	Stream_Read_UINT32(s, mediaType->Width);
	Stream_Read_UINT32(s, mediaType->Height);
	Stream_Read_UINT32(s, mediaType->FrameRateNumerator);
	Stream_Read_UINT32(s, mediaType->FrameRateDenominator);
	Stream_Read_UINT32(s, mediaType->PixelAspectRatioNumerator);
	Stream_Read_UINT32(s, mediaType->PixelAspectRatioDenominator);
	Stream_Read_UINT8(s, mediaType->Flags);
	return TRUE;
}

static void ecam_dev_print_media_type(CAM_MEDIA_TYPE_DESCRIPTION* mediaType)
{
	WINPR_ASSERT(mediaType);

	WLog_DBG(TAG, "Format: %d, width: %d, height: %d, fps: %d, flags: %d", mediaType->Format,
	         mediaType->Width, mediaType->Height, mediaType->FrameRateNumerator, mediaType->Flags);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_send_sample_response(CameraDevice* dev, size_t streamIndex, const BYTE* sample,
                                          size_t size)
{
	WINPR_ASSERT(dev);

	CameraDeviceStream* stream = &dev->streams[streamIndex];
	CAM_MSG_ID msg = CAM_MSG_ID_SampleResponse;

	Stream_SetPosition(stream->sampleRespBuffer, 0);

	Stream_Write_UINT8(stream->sampleRespBuffer, dev->ecam->version);
	Stream_Write_UINT8(stream->sampleRespBuffer, msg);
	Stream_Write_UINT8(stream->sampleRespBuffer, streamIndex);

	Stream_Write(stream->sampleRespBuffer, sample, size);

	/* channel write is protected by critical section in dvcman_write_channel */
	return ecam_channel_write(dev->ecam, stream->hSampleReqChannel, msg, stream->sampleRespBuffer,
	                          FALSE /* don't free stream */);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_sample_captured_callback(CameraDevice* dev, int streamIndex,
                                              const BYTE* sample, size_t size)
{
	BYTE* encodedSample = NULL;
	size_t encodedSize = 0;

	WINPR_ASSERT(dev);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
		return ERROR_INVALID_INDEX;

	CameraDeviceStream* stream = &dev->streams[streamIndex];

	if (!stream->streaming)
		return CHANNEL_RC_OK;

	if (streamInputFormat(stream) != streamOutputFormat(stream))
	{
		if (!ecam_encoder_compress(stream, sample, size, &encodedSample, &encodedSize))
		{
			WLog_DBG(TAG, "Frame drop or error in ecam_encoder_compress");
			return CHANNEL_RC_OK;
		}

		if (!stream->streaming)
			return CHANNEL_RC_OK;
	}
	else /* passthrough */
	{
		encodedSample = WINPR_CAST_CONST_PTR_AWAY(sample, BYTE*);
		encodedSize = size;
	}

	if (stream->nSampleCredits == 0)
	{
		WLog_DBG(TAG, "Skip sample: no credits left");
		return CHANNEL_RC_OK;
	}
	stream->nSampleCredits--;

	return ecam_dev_send_sample_response(dev, streamIndex, encodedSample, encodedSize);
}

static void ecam_dev_stop_stream(CameraDevice* dev, size_t streamIndex)
{
	WINPR_ASSERT(dev);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
		return;

	CameraDeviceStream* stream = &dev->streams[streamIndex];

	if (stream->streaming)
	{
		stream->streaming = FALSE;
		dev->ihal->StopStream(dev->ihal, dev->deviceId, 0);
	}

	if (stream->sampleRespBuffer)
	{
		Stream_Free(stream->sampleRespBuffer, TRUE);
		stream->sampleRespBuffer = NULL;
	}

	ecam_encoder_context_free(stream);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_stop_streams_request(CameraDevice* dev,
                                                  GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	WINPR_ASSERT(dev);
	WINPR_UNUSED(s);

	for (size_t i = 0; i < ECAM_DEVICE_MAX_STREAMS; i++)
		ecam_dev_stop_stream(dev, i);

	return ecam_channel_send_generic_msg(dev->ecam, hchannel, CAM_MSG_ID_SuccessResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_start_streams_request(CameraDevice* dev,
                                                   GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	BYTE streamIndex = 0;
	CAM_MEDIA_TYPE_DESCRIPTION mediaType = { 0 };

	WINPR_ASSERT(dev);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1 + 26))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, streamIndex);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
	{
		WLog_ERR(TAG, "Incorrect streamIndex %" PRIuz, streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_InvalidStreamNumber);
		return ERROR_INVALID_INDEX;
	}

	if (!ecam_dev_read_media_type(s, &mediaType))
	{
		WLog_ERR(TAG, "Unable to read MEDIA_TYPE_DESCRIPTION");
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_InvalidMessage);
		return ERROR_INVALID_DATA;
	}

	ecam_dev_print_media_type(&mediaType);

	CameraDeviceStream* stream = &dev->streams[streamIndex];

	if (stream->streaming)
	{
		WLog_ERR(TAG, "Streaming already in progress, device %s, streamIndex %d", dev->deviceId,
		         streamIndex);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	/* saving  media type description for CurrentMediaTypeRequest,
	 * to be done before calling ecam_encoder_context_init
	 */
	stream->currMediaType = mediaType;

	/* initialize encoder, if input and output formats differ */
	if (streamInputFormat(stream) != streamOutputFormat(stream) &&
	    !ecam_encoder_context_init(stream))
	{
		WLog_ERR(TAG, "stream_ecam_encoder_init failed");
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_UnexpectedError);
		return ERROR_INVALID_DATA;
	}

	stream->sampleRespBuffer = Stream_New(NULL, ECAM_SAMPLE_RESPONSE_BUFFER_SIZE);
	if (!stream->sampleRespBuffer)
	{
		WLog_ERR(TAG, "Stream_New failed");
		ecam_dev_stop_stream(dev, streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_OutOfMemory);
		return ERROR_INVALID_DATA;
	}

	/* replacing outputFormat with inputFormat in mediaType before starting stream */
	mediaType.Format = streamInputFormat(stream);

	stream->nSampleCredits = 0;

	UINT error = dev->ihal->StartStream(dev->ihal, dev, streamIndex, &mediaType,
	                                    ecam_dev_sample_captured_callback);
	if (error)
	{
		WLog_ERR(TAG, "StartStream failure");
		ecam_dev_stop_stream(dev, streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, error);
		return ERROR_INVALID_DATA;
	}

	stream->streaming = TRUE;
	return ecam_channel_send_generic_msg(dev->ecam, hchannel, CAM_MSG_ID_SuccessResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_property_list_request(CameraDevice* dev,
                                                   GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	WINPR_ASSERT(dev);
	// TODO: supported properties implementation

	return ecam_channel_send_generic_msg(dev->ecam, hchannel, CAM_MSG_ID_PropertyListResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_send_current_media_type_response(CameraDevice* dev,
                                                      GENERIC_CHANNEL_CALLBACK* hchannel,
                                                      CAM_MEDIA_TYPE_DESCRIPTION* mediaType)
{
	CAM_MSG_ID msg = CAM_MSG_ID_CurrentMediaTypeResponse;

	WINPR_ASSERT(dev);

	wStream* s = Stream_New(NULL, CAM_HEADER_SIZE + sizeof(CAM_MEDIA_TYPE_DESCRIPTION));
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, dev->ecam->version);
	Stream_Write_UINT8(s, msg);

	ecam_dev_write_media_type(s, mediaType);

	return ecam_channel_write(dev->ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_sample_request(CameraDevice* dev, GENERIC_CHANNEL_CALLBACK* hchannel,
                                            wStream* s)
{
	BYTE streamIndex = 0;

	WINPR_ASSERT(dev);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, streamIndex);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
	{
		WLog_ERR(TAG, "Incorrect streamIndex %d", streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_InvalidStreamNumber);
		return ERROR_INVALID_INDEX;
	}

	CameraDeviceStream* stream = &dev->streams[streamIndex];

	/* need to save channel because responses are asynchronous and coming from capture thread */
	if (stream->hSampleReqChannel != hchannel)
		stream->hSampleReqChannel = hchannel;

	/* allow to send that many unsolicited samples */
	stream->nSampleCredits = ECAM_MAX_SAMPLE_CREDITS;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_current_media_type_request(CameraDevice* dev,
                                                        GENERIC_CHANNEL_CALLBACK* hchannel,
                                                        wStream* s)
{
	BYTE streamIndex = 0;

	WINPR_ASSERT(dev);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, streamIndex);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
	{
		WLog_ERR(TAG, "Incorrect streamIndex %d", streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_InvalidStreamNumber);
		return ERROR_INVALID_INDEX;
	}

	CameraDeviceStream* stream = &dev->streams[streamIndex];

	if (stream->currMediaType.Format == 0)
	{
		WLog_ERR(TAG, "Current media type unknown for streamIndex %d", streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_NotInitialized);
		return ERROR_DEVICE_REINITIALIZATION_NEEDED;
	}

	return ecam_dev_send_current_media_type_response(dev, hchannel, &stream->currMediaType);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_send_media_type_list_response(CameraDevice* dev,
                                                   GENERIC_CHANNEL_CALLBACK* hchannel,
                                                   CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes,
                                                   size_t nMediaTypes)
{
	CAM_MSG_ID msg = CAM_MSG_ID_MediaTypeListResponse;

	WINPR_ASSERT(dev);

	wStream* s = Stream_New(NULL, CAM_HEADER_SIZE + ECAM_MAX_MEDIA_TYPE_DESCRIPTORS *
	                                                    sizeof(CAM_MEDIA_TYPE_DESCRIPTION));
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, dev->ecam->version);
	Stream_Write_UINT8(s, msg);

	for (size_t i = 0; i < nMediaTypes; i++, mediaTypes++)
	{
		ecam_dev_write_media_type(s, mediaTypes);
	}

	return ecam_channel_write(dev->ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_media_type_list_request(CameraDevice* dev,
                                                     GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	BYTE streamIndex = 0;
	CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes = NULL;
	size_t nMediaTypes = ECAM_MAX_MEDIA_TYPE_DESCRIPTORS;

	WINPR_ASSERT(dev);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, streamIndex);

	if (streamIndex >= ECAM_DEVICE_MAX_STREAMS)
	{
		WLog_ERR(TAG, "Incorrect streamIndex %d", streamIndex);
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_InvalidStreamNumber);
		return ERROR_INVALID_INDEX;
	}
	CameraDeviceStream* stream = &dev->streams[streamIndex];

	mediaTypes =
	    (CAM_MEDIA_TYPE_DESCRIPTION*)calloc(nMediaTypes, sizeof(CAM_MEDIA_TYPE_DESCRIPTION));
	if (!mediaTypes)
	{
		WLog_ERR(TAG, "calloc failed");
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_OutOfMemory);
		return CHANNEL_RC_NO_MEMORY;
	}

	INT16 formatIndex =
	    dev->ihal->GetMediaTypeDescriptions(dev->ihal, dev->deviceId, streamIndex, supportedFormats,
	                                        nSupportedFormats, mediaTypes, &nMediaTypes);
	if (formatIndex == -1 || nMediaTypes == 0)
	{
		WLog_ERR(TAG, "Camera doesn't support any compatible video formats");
		ecam_channel_send_error_response(dev->ecam, hchannel, CAM_ERROR_CODE_ItemNotFound);
		error = ERROR_DEVICE_FEATURE_NOT_SUPPORTED;
		goto error;
	}

	stream->formats = supportedFormats[formatIndex];

	/* replacing inputFormat with outputFormat in mediaTypes before sending response */
	for (size_t i = 0; i < nMediaTypes; i++)
	{
		mediaTypes[i].Format = streamOutputFormat(stream);
		mediaTypes[i].Flags = CAM_MEDIA_TYPE_DESCRIPTION_FLAG_DecodingRequired;
	}

	if (stream->currMediaType.Format == 0)
	{
		/* saving 1st media type description for CurrentMediaTypeRequest */
		stream->currMediaType = mediaTypes[0];
	}

	error = ecam_dev_send_media_type_list_response(dev, hchannel, mediaTypes, nMediaTypes);

error:
	free(mediaTypes);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_send_stream_list_response(CameraDevice* dev,
                                               GENERIC_CHANNEL_CALLBACK* hchannel)
{
	CAM_MSG_ID msg = CAM_MSG_ID_StreamListResponse;

	WINPR_ASSERT(dev);

	wStream* s = Stream_New(NULL, CAM_HEADER_SIZE + sizeof(CAM_STREAM_DESCRIPTION));
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, dev->ecam->version);
	Stream_Write_UINT8(s, msg);

	/* single stream description */
	Stream_Write_UINT16(s, CAM_STREAM_FRAME_SOURCE_TYPE_Color);
	Stream_Write_UINT8(s, CAM_STREAM_CATEGORY_Capture);
	Stream_Write_UINT8(s, TRUE /* Selected */);
	Stream_Write_UINT8(s, FALSE /* CanBeShared */);

	return ecam_channel_write(dev->ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_stream_list_request(CameraDevice* dev,
                                                 GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	return ecam_dev_send_stream_list_response(dev, hchannel);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_activate_device_request(CameraDevice* dev,
                                                     GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s)
{
	WINPR_ASSERT(dev);

	/* TODO: TBD if this is required */
	return ecam_channel_send_generic_msg(dev->ecam, hchannel, CAM_MSG_ID_SuccessResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_process_deactivate_device_request(CameraDevice* dev,
                                                       GENERIC_CHANNEL_CALLBACK* hchannel,
                                                       wStream* s)
{
	WINPR_ASSERT(dev);
	WINPR_UNUSED(s);

	for (size_t i = 0; i < ECAM_DEVICE_MAX_STREAMS; i++)
		ecam_dev_stop_stream(dev, i);

	return ecam_channel_send_generic_msg(dev->ecam, hchannel, CAM_MSG_ID_SuccessResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	UINT error = CHANNEL_RC_OK;
	BYTE version = 0;
	BYTE messageId = 0;
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	if (!hchannel || !data)
		return ERROR_INVALID_PARAMETER;

	CameraDevice* dev = (CameraDevice*)hchannel->plugin;

	if (!dev)
		return ERROR_INTERNAL_ERROR;

	if (!Stream_CheckAndLogRequiredCapacity(TAG, data, CAM_HEADER_SIZE))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(data, version);
	Stream_Read_UINT8(data, messageId);
	WLog_DBG(TAG, "ChannelId=%d, MessageId=0x%02" PRIx8 ", Version=%d",
	         hchannel->channel_mgr->GetChannelId(hchannel->channel), messageId, version);

	switch (messageId)
	{
		case CAM_MSG_ID_ActivateDeviceRequest:
			error = ecam_dev_process_activate_device_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_DeactivateDeviceRequest:
			error = ecam_dev_process_deactivate_device_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_StreamListRequest:
			error = ecam_dev_process_stream_list_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_MediaTypeListRequest:
			error = ecam_dev_process_media_type_list_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_CurrentMediaTypeRequest:
			error = ecam_dev_process_current_media_type_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_PropertyListRequest:
			error = ecam_dev_process_property_list_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_StartStreamsRequest:
			error = ecam_dev_process_start_streams_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_StopStreamsRequest:
			error = ecam_dev_process_stop_streams_request(dev, hchannel, data);
			break;

		case CAM_MSG_ID_SampleRequest:
			error = ecam_dev_process_sample_request(dev, hchannel, data);
			break;

		default:
			WLog_WARN(TAG, "unknown MessageId=0x%02" PRIx8 "", messageId);
			error = ERROR_INVALID_DATA;
			ecam_channel_send_error_response(dev->ecam, hchannel,
			                                 CAM_ERROR_CODE_OperationNotSupported);
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(hchannel);

	CameraDevice* dev = (CameraDevice*)hchannel->plugin;
	WINPR_ASSERT(dev);

	WLog_DBG(TAG, "entered");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(hchannel);

	CameraDevice* dev = (CameraDevice*)hchannel->plugin;
	WINPR_ASSERT(dev);

	WLog_DBG(TAG, "entered");

	/* make sure this channel is not used for sample responses */
	for (size_t i = 0; i < ECAM_DEVICE_MAX_STREAMS; i++)
		if (dev->streams[i].hSampleReqChannel == hchannel)
			dev->streams[i].hSampleReqChannel = NULL;

	free(hchannel);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_dev_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                               IWTSVirtualChannel* pChannel, BYTE* Data,
                                               BOOL* pbAccept,
                                               IWTSVirtualChannelCallback** ppCallback)
{
	GENERIC_LISTENER_CALLBACK* hlistener = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;

	if (!hlistener || !hlistener->plugin)
		return ERROR_INTERNAL_ERROR;

	WLog_DBG(TAG, "entered");
	GENERIC_CHANNEL_CALLBACK* hchannel =
	    (GENERIC_CHANNEL_CALLBACK*)calloc(1, sizeof(GENERIC_CHANNEL_CALLBACK));

	if (!hchannel)
	{
		WLog_ERR(TAG, "calloc failed");
		return CHANNEL_RC_NO_MEMORY;
	}

	hchannel->iface.OnDataReceived = ecam_dev_on_data_received;
	hchannel->iface.OnOpen = ecam_dev_on_open;
	hchannel->iface.OnClose = ecam_dev_on_close;
	hchannel->plugin = hlistener->plugin;
	hchannel->channel_mgr = hlistener->channel_mgr;
	hchannel->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*)hchannel;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return CameraDevice pointer or NULL in case of error
 */
CameraDevice* ecam_dev_create(CameraPlugin* ecam, const char* deviceId, const char* deviceName)
{
	WINPR_ASSERT(ecam);
	WINPR_ASSERT(ecam->hlistener);

	IWTSVirtualChannelManager* pChannelMgr = ecam->hlistener->channel_mgr;
	WINPR_ASSERT(pChannelMgr);

	WLog_DBG(TAG, "entered for %s", deviceId);

	CameraDevice* dev = (CameraDevice*)calloc(1, sizeof(CameraDevice));

	if (!dev)
	{
		WLog_ERR(TAG, "calloc failed");
		return NULL;
	}

	dev->ecam = ecam;
	dev->ihal = ecam->ihal;
	strncpy(dev->deviceId, deviceId, sizeof(dev->deviceId) - 1);
	dev->hlistener = (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));

	if (!dev->hlistener)
	{
		free(dev);
		WLog_ERR(TAG, "calloc failed");
		return NULL;
	}

	dev->hlistener->iface.OnNewChannelConnection = ecam_dev_on_new_channel_connection;
	dev->hlistener->plugin = (IWTSPlugin*)dev;
	dev->hlistener->channel_mgr = pChannelMgr;
	if (CHANNEL_RC_OK != pChannelMgr->CreateListener(pChannelMgr, deviceId, 0,
	                                                 &dev->hlistener->iface, &dev->listener))
	{
		free(dev->hlistener);
		free(dev);
		WLog_ERR(TAG, "CreateListener failed");
		return NULL;
	}

	return dev;
}

/**
 * Function description
 *
 * OBJECT_FREE_FN for devices hash table value
 *
 */
void ecam_dev_destroy(CameraDevice* dev)
{
	if (!dev)
		return;

	WLog_DBG(TAG, "entered for %s", dev->deviceId);

	if (dev->hlistener)
	{
		IWTSVirtualChannelManager* mgr = dev->hlistener->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, dev->listener);
	}

	free(dev->hlistener);

	for (int i = 0; i < ECAM_DEVICE_MAX_STREAMS; i++)
		ecam_dev_stop_stream(dev, i);

	free(dev);
}
