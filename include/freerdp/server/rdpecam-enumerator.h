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

#ifndef FREERDP_CHANNEL_CAM_DEV_ENUM_SERVER_CAM_DEV_ENUM_H
#define FREERDP_CHANNEL_CAM_DEV_ENUM_SERVER_CAM_DEV_ENUM_H

#include <freerdp/channels/rdpecam.h>
#include <freerdp/channels/wtsvc.h>

typedef struct _cam_dev_enum_server_context CamDevEnumServerContext;

typedef UINT (*psCamDevEnumServerServerOpen)(CamDevEnumServerContext* context);
typedef UINT (*psCamDevEnumServerServerClose)(CamDevEnumServerContext* context);

typedef BOOL (*psCamDevEnumServerServerChannelIdAssigned)(CamDevEnumServerContext* context,
                                                          UINT32 channelId);

typedef UINT (*psCamDevEnumServerServerInitialize)(CamDevEnumServerContext* context,
                                                   BOOL externalThread);
typedef UINT (*psCamDevEnumServerServerPoll)(CamDevEnumServerContext* context);
typedef BOOL (*psCamDevEnumServerServerChannelHandle)(CamDevEnumServerContext* context,
                                                      HANDLE* handle);

typedef UINT (*psCamDevEnumServerServerSelectVersionRequest)(
    CamDevEnumServerContext* context, const CAM_SELECT_VERSION_REQUEST* selectVersionRequest);
typedef UINT (*psCamDevEnumServerServerSelectVersionResponse)(
    CamDevEnumServerContext* context, const CAM_SELECT_VERSION_RESPONSE* selectVersionResponse);

typedef UINT (*psCamDevEnumServerServerDeviceAddedNotification)(
    CamDevEnumServerContext* context, const CAM_DEVICE_ADDED_NOTIFICATION* deviceAddedNotification);
typedef UINT (*psCamDevEnumServerServerDeviceRemovedNotification)(
    CamDevEnumServerContext* context,
    const CAM_DEVICE_REMOVED_NOTIFICATION* deviceRemovedNotification);

struct _cam_dev_enum_server_context
{
	HANDLE vcm;

	/* Server self-defined pointer. */
	void* userdata;

	/*** APIs called by the server. ***/

	/**
	 * Optional: Set thread handling.
	 * When externalThread=TRUE, the application is responsible to call
	 * Poll() periodically to process channel events.
	 *
	 * Defaults to externalThread=FALSE
	 */
	psCamDevEnumServerServerInitialize Initialize;

	/**
	 * Open the camera device enumerator channel.
	 */
	psCamDevEnumServerServerOpen Open;

	/**
	 * Close the camera device enumerator channel.
	 */
	psCamDevEnumServerServerClose Close;

	/**
	 * Poll
	 * When externalThread=TRUE, call Poll() periodically from your main loop.
	 * If externalThread=FALSE do not call.
	 */
	psCamDevEnumServerServerPoll Poll;

	/**
	 * Retrieve the channel handle for use in conjunction with Poll().
	 * If externalThread=FALSE do not call.
	 */
	psCamDevEnumServerServerChannelHandle ChannelHandle;

	/*
	 * Send a Select Version Response PDU.
	 */
	psCamDevEnumServerServerSelectVersionResponse SelectVersionResponse;

	/*** Callbacks registered by the server. ***/

	/**
	 * Callback, when the channel got its id assigned.
	 */
	psCamDevEnumServerServerChannelIdAssigned ChannelIdAssigned;

	/**
	 * Callback for the Select Version Request PDU.
	 */
	psCamDevEnumServerServerSelectVersionRequest SelectVersionRequest;

	/**
	 * Callback for the Device Added Notification PDU.
	 */
	psCamDevEnumServerServerDeviceAddedNotification DeviceAddedNotification;

	/**
	 * Callback for the Device Removed Notification PDU.
	 */
	psCamDevEnumServerServerDeviceRemovedNotification DeviceRemovedNotification;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API CamDevEnumServerContext* cam_dev_enum_server_context_new(HANDLE vcm);
	FREERDP_API void cam_dev_enum_server_context_free(CamDevEnumServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_CAM_DEV_ENUM_SERVER_CAM_DEV_ENUM_H */
