/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension
 *
 * Copyright 2018 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_CHANNEL_VIDEO_H
#define FREERDP_CHANNEL_VIDEO_H

#include <winpr/wtypes.h>
#include <freerdp/types.h>

#define VIDEO_CONTROL_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Video::Control::v08.01"
#define VIDEO_DATA_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Video::Data::v08.01"

/** @brief TSNM packet type */
enum
{
	TSMM_PACKET_TYPE_PRESENTATION_REQUEST	= 1,
	TSMM_PACKET_TYPE_PRESENTATION_RESPONSE	= 2,
	TSMM_PACKET_TYPE_CLIENT_NOTIFICATION	= 3,
	TSMM_PACKET_TYPE_VIDEO_DATA				= 4
};

/** @brief TSMM_PRESENTATION_REQUEST commands */
enum
{
	TSMM_START_PRESENTATION = 1,
	TSMM_STOP_PRESENTATION	= 2
};

/** @brief presentation request struct */
struct _TSMM_PRESENTATION_REQUEST
{
	BYTE PresentationId;
	BYTE Version;
	BYTE Command;
	BYTE FrameRate;
	UINT32 SourceWidth, SourceHeight;
	UINT32 ScaledWidth, ScaledHeight;
	UINT64 hnsTimestampOffset;
	UINT64 GeometryMappingId;
	BYTE VideoSubtypeId[16];
	UINT32 cbExtra;
	BYTE *pExtraData;
};
typedef struct _TSMM_PRESENTATION_REQUEST TSMM_PRESENTATION_REQUEST;

/** @brief response to a TSMM_PRESENTATION_REQUEST */
struct _TSMM_PRESENTATION_RESPONSE
{
	BYTE PresentationId;
};
typedef struct _TSMM_PRESENTATION_RESPONSE TSMM_PRESENTATION_RESPONSE;

/** @brief TSMM_VIDEO_DATA flags */
enum
{
	TSMM_VIDEO_DATA_FLAG_HAS_TIMESTAMPS	= 0x01,
	TSMM_VIDEO_DATA_FLAG_KEYFRAME		= 0x02,
	TSMM_VIDEO_DATA_FLAG_NEW_FRAMERATE	= 0x04
};

/** @brief a video data packet */
struct _TSMM_VIDEO_DATA
{
	BYTE PresentationId;
	BYTE Version;
	BYTE Flags;
	UINT64 hnsTimestamp;
	UINT64 hnsDuration;
	UINT16 CurrentPacketIndex;
	UINT16 PacketsInSample;
	UINT32 SampleNumber;
	UINT32 cbSample;
	BYTE *pSample;
};
typedef struct _TSMM_VIDEO_DATA TSMM_VIDEO_DATA;

/** @brief values for NotificationType in TSMM_CLIENT_NOTIFICATION */
enum
{
	TSMM_CLIENT_NOTIFICATION_TYPE_NETWORK_ERROR = 1,
	TSMM_CLIENT_NOTIFICATION_TYPE_FRAMERATE_OVERRIDE = 2
};

/** @brief struct used when NotificationType is FRAMERATE_OVERRIDE */
struct _TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE
{
	UINT32 Flags;
	UINT32 DesiredFrameRate;
};
typedef struct _TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE;

/** @brief a client to server notification struct */
struct _TSMM_CLIENT_NOTIFICATION
{
	BYTE PresentationId;
	BYTE NotificationType;
	TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE FramerateOverride;
};
typedef struct _TSMM_CLIENT_NOTIFICATION TSMM_CLIENT_NOTIFICATION;


#endif /* FREERDP_CHANNEL_VIDEO_H */
