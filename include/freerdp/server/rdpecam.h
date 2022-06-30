/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Capture Virtual Channel Extension
 *
 * Copyright 2022 Pascal Nowack <Pascal.Nowack@gmx.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_CAMERA_DEVICE_SERVER_CAMERA_DEVICE_H
#define FREERDP_CHANNEL_CAMERA_DEVICE_SERVER_CAMERA_DEVICE_H

#include <freerdp/channels/rdpecam.h>
#include <freerdp/channels/wtsvc.h>

typedef struct camera_device_server_context CameraDeviceServerContext;

typedef UINT (*psCameraDeviceServerOpen)(CameraDeviceServerContext* context);
typedef UINT (*psCameraDeviceServerClose)(CameraDeviceServerContext* context);

typedef BOOL (*psCameraDeviceServerChannelIdAssigned)(CameraDeviceServerContext* context,
                                                      UINT32 channelId);

typedef UINT (*psCameraDeviceServerInitialize)(CameraDeviceServerContext* context,
                                               BOOL externalThread);
typedef UINT (*psCameraDeviceServerPoll)(CameraDeviceServerContext* context);
typedef BOOL (*psCameraDeviceServerChannelHandle)(CameraDeviceServerContext* context,
                                                  HANDLE* handle);

typedef UINT (*psCameraDeviceServerSuccessResponse)(CameraDeviceServerContext* context,
                                                    const CAM_SUCCESS_RESPONSE* successResponse);
typedef UINT (*psCameraDeviceServerErrorResponse)(CameraDeviceServerContext* context,
                                                  const CAM_ERROR_RESPONSE* errorResponse);

typedef UINT (*psCameraDeviceServerActivateDeviceRequest)(
    CameraDeviceServerContext* context, const CAM_ACTIVATE_DEVICE_REQUEST* activateDeviceRequest);
typedef UINT (*psCameraDeviceServerDeactivateDeviceRequest)(
    CameraDeviceServerContext* context,
    const CAM_DEACTIVATE_DEVICE_REQUEST* deactivateDeviceRequest);

typedef UINT (*psCameraDeviceServerStreamListRequest)(
    CameraDeviceServerContext* context, const CAM_STREAM_LIST_REQUEST* streamListRequest);
typedef UINT (*psCameraDeviceServerStreamListResponse)(
    CameraDeviceServerContext* context, const CAM_STREAM_LIST_RESPONSE* streamListResponse);

typedef UINT (*psCameraDeviceServerMediaTypeListRequest)(
    CameraDeviceServerContext* context, const CAM_MEDIA_TYPE_LIST_REQUEST* mediaTypeListRequest);
typedef UINT (*psCameraDeviceServerMediaTypeListResponse)(
    CameraDeviceServerContext* context, const CAM_MEDIA_TYPE_LIST_RESPONSE* mediaTypeListResponse);

typedef UINT (*psCameraDeviceServerCurrentMediaTypeRequest)(
    CameraDeviceServerContext* context,
    const CAM_CURRENT_MEDIA_TYPE_REQUEST* currentMediaTypeRequest);
typedef UINT (*psCameraDeviceServerCurrentMediaTypeResponse)(
    CameraDeviceServerContext* context,
    const CAM_CURRENT_MEDIA_TYPE_RESPONSE* currentMediaTypeResponse);

typedef UINT (*psCameraDeviceServerStartStreamsRequest)(
    CameraDeviceServerContext* context, const CAM_START_STREAMS_REQUEST* startStreamsRequest);
typedef UINT (*psCameraDeviceServerStopStreamsRequest)(
    CameraDeviceServerContext* context, const CAM_STOP_STREAMS_REQUEST* stopStreamsRequest);

typedef UINT (*psCameraDeviceServerSampleRequest)(CameraDeviceServerContext* context,
                                                  const CAM_SAMPLE_REQUEST* sampleRequest);
typedef UINT (*psCameraDeviceServerSampleResponse)(CameraDeviceServerContext* context,
                                                   const CAM_SAMPLE_RESPONSE* sampleResponse);
typedef UINT (*psCameraDeviceServerSampleErrorResponse)(
    CameraDeviceServerContext* context, const CAM_SAMPLE_ERROR_RESPONSE* sampleErrorResponse);

typedef UINT (*psCameraDeviceServerPropertyListRequest)(
    CameraDeviceServerContext* context, const CAM_PROPERTY_LIST_REQUEST* propertyListRequest);
typedef UINT (*psCameraDeviceServerPropertyListResponse)(
    CameraDeviceServerContext* context, const CAM_PROPERTY_LIST_RESPONSE* propertyListResponse);

typedef UINT (*psCameraDeviceServerPropertyValueRequest)(
    CameraDeviceServerContext* context, const CAM_PROPERTY_VALUE_REQUEST* propertyValueRequest);
typedef UINT (*psCameraDeviceServerPropertyValueResponse)(
    CameraDeviceServerContext* context, const CAM_PROPERTY_VALUE_RESPONSE* propertyValueResponse);

typedef UINT (*psCameraDeviceServerSetPropertyValueRequest)(
    CameraDeviceServerContext* context,
    const CAM_SET_PROPERTY_VALUE_REQUEST* setPropertyValueRequest);

struct camera_device_server_context
{
	HANDLE vcm;

	/* Server self-defined pointer. */
	void* userdata;

	/**
	 * Name of the virtual channel. Pointer owned by the CameraDeviceServerContext,
	 * meaning camera_device_server_context_free() takes care of freeing the pointer.
	 *
	 * Server implementations should sanitize the virtual channel name for invalid
	 * names, like names for other known channels
	 * ("ECHO", "AUDIO_PLAYBACK_DVC", etc.)
	 */
	char* virtualChannelName;

	/**
	 * Protocol version to be used. Every sent server to client PDU has the
	 * version value in the Header set to the following value.
	 */
	BYTE protocolVersion;

	/*** APIs called by the server. ***/

	/**
	 * Optional: Set thread handling.
	 * When externalThread=TRUE, the application is responsible to call
	 * Poll() periodically to process channel events.
	 *
	 * Defaults to externalThread=FALSE
	 */
	psCameraDeviceServerInitialize Initialize;

	/**
	 * Open the camera device channel.
	 */
	psCameraDeviceServerOpen Open;

	/**
	 * Close the camera device channel.
	 */
	psCameraDeviceServerClose Close;

	/**
	 * Poll
	 * When externalThread=TRUE, call Poll() periodically from your main loop.
	 * If externalThread=FALSE do not call.
	 */
	psCameraDeviceServerPoll Poll;

	/**
	 * Retrieve the channel handle for use in conjunction with Poll().
	 * If externalThread=FALSE do not call.
	 */
	psCameraDeviceServerChannelHandle ChannelHandle;

	/**
	 * For the following server to client PDUs,
	 * the message header does not have to be set.
	 */

	/**
	 * Send a Activate Device Request PDU.
	 */
	psCameraDeviceServerActivateDeviceRequest ActivateDeviceRequest;

	/**
	 * Send a Deactivate Device Request PDU.
	 */
	psCameraDeviceServerDeactivateDeviceRequest DeactivateDeviceRequest;

	/**
	 * Send a Stream List Request PDU.
	 */
	psCameraDeviceServerStreamListRequest StreamListRequest;

	/**
	 * Send a Media Type List Request PDU.
	 */
	psCameraDeviceServerMediaTypeListRequest MediaTypeListRequest;

	/**
	 * Send a Current Media Type Request PDU.
	 */
	psCameraDeviceServerCurrentMediaTypeRequest CurrentMediaTypeRequest;

	/**
	 * Send a Start Streams Request PDU.
	 */
	psCameraDeviceServerStartStreamsRequest StartStreamsRequest;

	/**
	 * Send a Stop Streams Request PDU.
	 */
	psCameraDeviceServerStopStreamsRequest StopStreamsRequest;

	/**
	 * Send a Sample Request PDU.
	 */
	psCameraDeviceServerSampleRequest SampleRequest;

	/**
	 * Send a Property List Request PDU.
	 */
	psCameraDeviceServerPropertyListRequest PropertyListRequest;

	/**
	 * Send a Property Value Request PDU.
	 */
	psCameraDeviceServerPropertyValueRequest PropertyValueRequest;

	/**
	 * Send a Set Property Value Request PDU.
	 */
	psCameraDeviceServerSetPropertyValueRequest SetPropertyValueRequest;

	/*** Callbacks registered by the server. ***/

	/**
	 * Callback, when the channel got its id assigned.
	 */
	psCameraDeviceServerChannelIdAssigned ChannelIdAssigned;

	/**
	 * Callback for the Success Response PDU.
	 */
	psCameraDeviceServerSuccessResponse SuccessResponse;

	/**
	 * Callback for the Error Response PDU.
	 */
	psCameraDeviceServerErrorResponse ErrorResponse;

	/**
	 * Callback for the Stream List Response PDU.
	 */
	psCameraDeviceServerStreamListResponse StreamListResponse;

	/**
	 * Callback for the Media Type List Response PDU.
	 */
	psCameraDeviceServerMediaTypeListResponse MediaTypeListResponse;

	/**
	 * Callback for the Current Media Type Response PDU.
	 */
	psCameraDeviceServerCurrentMediaTypeResponse CurrentMediaTypeResponse;

	/**
	 * Callback for the Sample Response PDU.
	 */
	psCameraDeviceServerSampleResponse SampleResponse;

	/**
	 * Callback for the Sample Error Response PDU.
	 */
	psCameraDeviceServerSampleErrorResponse SampleErrorResponse;

	/**
	 * Callback for the Property List Response PDU.
	 */
	psCameraDeviceServerPropertyListResponse PropertyListResponse;

	/**
	 * Callback for the Property Value Response PDU.
	 */
	psCameraDeviceServerPropertyValueResponse PropertyValueResponse;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API CameraDeviceServerContext* camera_device_server_context_new(HANDLE vcm);
	FREERDP_API void camera_device_server_context_free(CameraDeviceServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_CAMERA_DEVICE_SERVER_CAMERA_DEVICE_H */
