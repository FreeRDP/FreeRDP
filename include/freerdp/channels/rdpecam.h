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
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_RDPECAM_H
#define FREERDP_CHANNEL_RDPECAM_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

#define RDPECAM_DVC_CHANNEL_NAME "rdpecam"
#define RDPECAM_CONTROL_DVC_CHANNEL_NAME "RDCamera_Device_Enumerator"

typedef enum
{
	CAM_MSG_ID_SuccessResponse = 0x01,
	CAM_MSG_ID_ErrorResponse = 0x02,
	CAM_MSG_ID_SelectVersionRequest = 0x03,
	CAM_MSG_ID_SelectVersionResponse = 0x04,
	CAM_MSG_ID_DeviceAddedNotification = 0x05,
	CAM_MSG_ID_DeviceRemovedNotification = 0x06,
	CAM_MSG_ID_ActivateDeviceRequest = 0x07,
	CAM_MSG_ID_DeactivateDeviceRequest = 0x08,
	CAM_MSG_ID_StreamListRequest = 0x09,
	CAM_MSG_ID_StreamListResponse = 0x0A,
	CAM_MSG_ID_MediaTypeListRequest = 0x0B,
	CAM_MSG_ID_MediaTypeListResponse = 0x0C,
	CAM_MSG_ID_CurrentMediaTypeRequest = 0x0D,
	CAM_MSG_ID_CurrentMediaTypeResponse = 0x0E,
	CAM_MSG_ID_StartStreamsRequest = 0x0F,
	CAM_MSG_ID_StopStreamsRequest = 0x10,
	CAM_MSG_ID_SampleRequest = 0x11,
	CAM_MSG_ID_SampleResponse = 0x12,
	CAM_MSG_ID_SampleErrorResponse = 0x13,
	CAM_MSG_ID_PropertyListRequest = 0x14,
	CAM_MSG_ID_PropertyListResponse = 0x15,
	CAM_MSG_ID_PropertyValueRequest = 0x16,
	CAM_MSG_ID_PropertyValueResponse = 0x17,
	CAM_MSG_ID_SetPropertyValueRequest = 0x18,
} CAM_MSG_ID;

#define CAM_HEADER_SIZE 2

typedef struct
{
	BYTE Version;
	CAM_MSG_ID MessageId;
} CAM_SHARED_MSG_HEADER;

/* Messages Exchanged on the Device Enumeration Channel (2.2.2) */

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_SELECT_VERSION_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_SELECT_VERSION_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	WCHAR* DeviceName;
	char* VirtualChannelName;
} CAM_DEVICE_ADDED_NOTIFICATION;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	char* VirtualChannelName;
} CAM_DEVICE_REMOVED_NOTIFICATION;

/* Messages Exchanged on Device Channels (2.2.3) */

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_SUCCESS_RESPONSE;

typedef enum
{
	CAM_ERROR_CODE_UnexpectedError = 0x00000001,
	CAM_ERROR_CODE_InvalidMessage = 0x00000002,
	CAM_ERROR_CODE_NotInitialized = 0x00000003,
	CAM_ERROR_CODE_InvalidRequest = 0x00000004,
	CAM_ERROR_CODE_InvalidStreamNumber = 0x00000005,
	CAM_ERROR_CODE_InvalidMediaType = 0x00000006,
	CAM_ERROR_CODE_OutOfMemory = 0x00000007,
	CAM_ERROR_CODE_ItemNotFound = 0x00000008,
	CAM_ERROR_CODE_SetNotFound = 0x00000009,
	CAM_ERROR_CODE_OperationNotSupported = 0x0000000A,
} CAM_ERROR_CODE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	CAM_ERROR_CODE ErrorCode;
} CAM_ERROR_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_ACTIVATE_DEVICE_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_DEACTIVATE_DEVICE_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_STREAM_LIST_REQUEST;

typedef enum
{
	CAM_STREAM_FRAME_SOURCE_TYPE_Color = 0x0001,
	CAM_STREAM_FRAME_SOURCE_TYPE_Infrared = 0x0002,
	CAM_STREAM_FRAME_SOURCE_TYPE_Custom = 0x0008,
} CAM_STREAM_FRAME_SOURCE_TYPES;

typedef enum
{
	CAM_STREAM_CATEGORY_Capture = 0x01,
} CAM_STREAM_CATEGORY;

typedef struct
{
	CAM_STREAM_FRAME_SOURCE_TYPES FrameSourceTypes;
	CAM_STREAM_CATEGORY StreamCategory;
	BYTE Selected;
	BYTE CanBeShared;
} CAM_STREAM_DESCRIPTION;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE N_Descriptions;
	CAM_STREAM_DESCRIPTION StreamDescriptions[255];
} CAM_STREAM_LIST_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE StreamIndex;
} CAM_MEDIA_TYPE_LIST_REQUEST;

typedef enum
{
	CAM_MEDIA_FORMAT_H264 = 0x01,
	CAM_MEDIA_FORMAT_MJPG = 0x02,
	CAM_MEDIA_FORMAT_YUY2 = 0x03,
	CAM_MEDIA_FORMAT_NV12 = 0x04,
	CAM_MEDIA_FORMAT_I420 = 0x05,
	CAM_MEDIA_FORMAT_RGB24 = 0x06,
	CAM_MEDIA_FORMAT_RGB32 = 0x07,
} CAM_MEDIA_FORMAT;

typedef enum
{
	CAM_MEDIA_TYPE_DESCRIPTION_FLAG_DecodingRequired = 0x01,
	CAM_MEDIA_TYPE_DESCRIPTION_FLAG_BottomUpImage = 0x02,
} CAM_MEDIA_TYPE_DESCRIPTION_FLAGS;

typedef struct
{
	CAM_MEDIA_FORMAT Format;
	UINT32 Width;
	UINT32 Height;
	UINT32 FrameRateNumerator;
	UINT32 FrameRateDenominator;
	UINT32 PixelAspectRatioNumerator;
	UINT32 PixelAspectRatioDenominator;
	CAM_MEDIA_TYPE_DESCRIPTION_FLAGS Flags;
} CAM_MEDIA_TYPE_DESCRIPTION;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	size_t N_Descriptions;
	CAM_MEDIA_TYPE_DESCRIPTION* MediaTypeDescriptions;
} CAM_MEDIA_TYPE_LIST_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE StreamIndex;
} CAM_CURRENT_MEDIA_TYPE_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	CAM_MEDIA_TYPE_DESCRIPTION MediaTypeDescription;
} CAM_CURRENT_MEDIA_TYPE_RESPONSE;

typedef struct
{
	BYTE StreamIndex;
	CAM_MEDIA_TYPE_DESCRIPTION MediaTypeDescription;
} CAM_START_STREAM_INFO;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE N_Infos;
	CAM_START_STREAM_INFO StartStreamsInfo[255];
} CAM_START_STREAMS_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_STOP_STREAMS_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE StreamIndex;
} CAM_SAMPLE_REQUEST;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE StreamIndex;
	size_t SampleSize;
	BYTE* Sample;
} CAM_SAMPLE_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	BYTE StreamIndex;
	CAM_ERROR_CODE ErrorCode;
} CAM_SAMPLE_ERROR_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
} CAM_PROPERTY_LIST_REQUEST;

typedef enum
{
	CAM_PROPERTY_SET_CameraControl = 0x01,
	CAM_PROPERTY_SET_VideoProcAmp = 0x02,
} CAM_PROPERTY_SET;

/* CameraControl properties */
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Exposure 0x01
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Focus 0x02
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Pan 0x03
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Roll 0x04
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Tilt 0x05
#define CAM_PROPERTY_ID_CAMERA_CONTROL_Zoom 0x06

/* VideoProcAmp properties */
#define CAM_PROPERTY_ID_VIDEO_PROC_AMP_BacklightCompensation 0x01
#define CAM_PROPERTY_ID_VIDEO_PROC_AMP_Brightness 0x02
#define CAM_PROPERTY_ID_VIDEO_PROC_AMP_Contrast 0x03
#define CAM_PROPERTY_ID_VIDEO_PROC_AMP_Hue 0x04
#define CAM_PROPERTY_ID_VIDEO_PROC_AMP_WhiteBalance 0x05

typedef enum
{
	CAM_PROPERTY_CAPABILITY_Manual = 0x01,
	CAM_PROPERTY_CAPABILITY_Auto = 0x02,
} CAM_PROPERTY_CAPABILITIES;

typedef struct
{
	CAM_PROPERTY_SET PropertySet;
	BYTE PropertyId;
	CAM_PROPERTY_CAPABILITIES Capabilities;
	INT32 MinValue;
	INT32 MaxValue;
	INT32 Step;
	INT32 DefaultValue;
} CAM_PROPERTY_DESCRIPTION;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	size_t N_Properties;
	CAM_PROPERTY_DESCRIPTION* Properties;
} CAM_PROPERTY_LIST_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	CAM_PROPERTY_SET PropertySet;
	BYTE PropertyId;
} CAM_PROPERTY_VALUE_REQUEST;

typedef enum
{
	CAM_PROPERTY_MODE_Manual = 0x01,
	CAM_PROPERTY_MODE_Auto = 0x02,
} CAM_PROPERTY_MODE;

typedef struct
{
	CAM_PROPERTY_MODE Mode;
	INT32 Value;
} CAM_PROPERTY_VALUE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	CAM_PROPERTY_VALUE PropertyValue;
} CAM_PROPERTY_VALUE_RESPONSE;

typedef struct
{
	CAM_SHARED_MSG_HEADER Header;
	CAM_PROPERTY_SET PropertySet;
	BYTE PropertyId;
	CAM_PROPERTY_VALUE PropertyValue;
} CAM_SET_PROPERTY_VALUE_REQUEST;

#endif /* FREERDP_CHANNEL_RDPECAM_H */
