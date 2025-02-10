/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, UVC H264 support
 *
 * See USB_Video_Payload_H 264_1 0.pdf for more details
 *
 * Credits:
 *     guvcview    http://guvcview.sourceforge.net
 *     Paulo Assis <pj.assis@gmail.com>
 *
 * Copyright 2025 Oleg Turovski <oleg2104@hotmail.com>
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

#ifndef UVC_H264_H
#define UVC_H264_H

#include <winpr/wtypes.h>

#include "../camera.h"
#include "camera_v4l.h"

/* UVC H.264 control selectors */
#define UVCX_VIDEO_CONFIG_PROBE 0x01
#define UVCX_VIDEO_CONFIG_COMMIT 0x02
#define UVCX_RATE_CONTROL_MODE 0x03
#define UVCX_TEMPORAL_SCALE_MODE 0x04
#define UVCX_SPATIAL_SCALE_MODE 0x05
#define UVCX_SNR_SCALE_MODE 0x06
#define UVCX_LTR_BUFFER_SIZE_CONTROL 0x07
#define UVCX_LTR_PICTURE_CONTROL 0x08
#define UVCX_PICTURE_TYPE_CONTROL 0x09
#define UVCX_VERSION 0x0A
#define UVCX_ENCODER_RESET 0x0B
#define UVCX_FRAMERATE_CONFIG 0x0C
#define UVCX_VIDEO_ADVANCE_CONFIG 0x0D
#define UVCX_BITRATE_LAYERS 0x0E
#define UVCX_QP_STEPS_LAYERS 0x0F

/* Video Class-Specific Request Codes */
#define UVC_RC_UNDEFINED 0x00
#define UVC_SET_CUR 0x01
#define UVC_GET_CUR 0x81
#define UVC_GET_MIN 0x82
#define UVC_GET_MAX 0x83
#define UVC_GET_RES 0x84
#define UVC_GET_LEN 0x85
#define UVC_GET_INFO 0x86
#define UVC_GET_DEF 0x87

/* bStreamMuxOption defines */
#define STREAMMUX_H264 (1 << 0) | (1 << 1)

/* wProfile defines */
#define PROFILE_BASELINE 0x4200
#define PROFILE_MAIN 0x4D00
#define PROFILE_HIGH 0x6400

/* bUsageType defines */
#define USAGETYPE_REALTIME 0x01

/* bRateControlMode defines */
#define RATECONTROL_CBR 0x01
#define RATECONTROL_VBR 0x02
#define RATECONTROL_CONST_QP 0x03

/* bEntropyCABAC defines */
#define ENTROPY_CABAC 0x01

/* bmHints defines */
#define BMHINTS_RESOLUTION 0x0001
#define BMHINTS_PROFILE 0x0002
#define BMHINTS_RATECONTROL 0x0004
#define BMHINTS_USAGE 0x0008
#define BMHINTS_SLICEMODE 0x0010
#define BMHINTS_SLICEUNITS 0x0020
#define BMHINTS_MVCVIEW 0x0040
#define BMHINTS_TEMPORAL 0x0080
#define BMHINTS_SNR 0x0100
#define BMHINTS_SPATIAL 0x0200
#define BMHINTS_SPATIAL_RATIO 0x0400
#define BMHINTS_FRAME_INTERVAL 0x0800
#define BMHINTS_LEAKY_BKT_SIZE 0x1000
#define BMHINTS_BITRATE 0x2000
#define BMHINTS_ENTROPY 0x4000
#define BMHINTS_IFRAMEPERIOD 0x8000

/* USB related defines */
#define USB_VIDEO_CONTROL 0x01
#define USB_VIDEO_CONTROL_INTERFACE 0x24
#define USB_VIDEO_CONTROL_XU_TYPE 0x06

#define MAX_DEVPATH_DEPTH 8
#define MAX_DEVPATH_STR_SIZE 32

#define WINPR_PACK_PUSH
#include <winpr/pack.h>

/* h264 probe commit struct (uvc 1.1) - packed */
typedef struct
{
	uint32_t dwFrameInterval;
	uint32_t dwBitRate;
	uint16_t bmHints;
	uint16_t wConfigurationIndex;
	uint16_t wWidth;
	uint16_t wHeight;
	uint16_t wSliceUnits;
	uint16_t wSliceMode;
	uint16_t wProfile;
	uint16_t wIFramePeriod;
	uint16_t wEstimatedVideoDelay;
	uint16_t wEstimatedMaxConfigDelay;
	uint8_t bUsageType;
	uint8_t bRateControlMode;
	uint8_t bTemporalScaleMode;
	uint8_t bSpatialScaleMode;
	uint8_t bSNRScaleMode;
	uint8_t bStreamMuxOption;
	uint8_t bStreamFormat;
	uint8_t bEntropyCABAC;
	uint8_t bTimestamp;
	uint8_t bNumOfReorderFrames;
	uint8_t bPreviewFlipped;
	uint8_t bView;
	uint8_t bReserved1;
	uint8_t bReserved2;
	uint8_t bStreamID;
	uint8_t bSpatialLayerRatio;
	uint16_t wLeakyBucketSize;

} uvcx_video_config_probe_commit_t;

/* encoder reset struct - packed */
typedef struct
{
	uint16_t wLayerID;

} uvcx_encoder_reset;

/* xu_descriptor struct - packed */
typedef struct
{
	int8_t bLength;
	int8_t bDescriptorType;
	int8_t bDescriptorSubType;
	int8_t bUnitID;
	uint8_t guidExtensionCode[16];

} xu_descriptor;

#define WINPR_PACK_POP
#include <winpr/pack.h>

uint8_t get_uvc_h624_unit_id(const char* deviceId);
BOOL set_h264_muxed_format(CamV4lStream* stream, const CAM_MEDIA_TYPE_DESCRIPTION* mediaType);

#endif /* UVC_H264_H */
