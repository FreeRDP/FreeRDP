/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, main header file
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

#ifndef FREERDP_CLIENT_CAMERA_H
#define FREERDP_CLIENT_CAMERA_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(WITH_INPUT_FORMAT_MJPG)
#include <libavcodec/avcodec.h>
#endif

#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/client/channels.h>
#include <freerdp/channels/log.h>
#include <freerdp/channels/rdpecam.h>
#include <freerdp/codecs.h>
#include <freerdp/primitives.h>

#define ECAM_PROTO_VERSION 0x02
/* currently supporting 1 stream per device */
#define ECAM_DEVICE_MAX_STREAMS 1
#define ECAM_MAX_MEDIA_TYPE_DESCRIPTORS 256

/* Allow to send up to that many unsolicited samples.
 * For example, to support 30 fps with 250 ms round trip
 * ECAM_MAX_SAMPLE_CREDITS has to be at least 8.
 */
#define ECAM_MAX_SAMPLE_CREDITS 8

/* Having this hardcoded allows to preallocate and reuse buffer
 * for sample responses. Excessive size is to make sure any sample
 * will fit in, even with highest resolution.
 */
#define ECAM_SAMPLE_RESPONSE_BUFFER_SIZE (1024ULL * 4050ULL)

typedef struct s_ICamHal ICamHal;

typedef struct
{
	IWTSPlugin iface;
	IWTSListener* listener;
	GENERIC_LISTENER_CALLBACK* hlistener;

	/* HAL interface */
	ICamHal* ihal;
	char* subsystem;

	BOOL initialized;
	BOOL attached;

	UINT32 version;
	wHashTable* devices;

} CameraPlugin;

typedef struct
{
	CAM_MEDIA_FORMAT inputFormat;  /* camera side */
	CAM_MEDIA_FORMAT outputFormat; /* network side */

} CAM_MEDIA_FORMAT_INFO;

typedef struct
{
	BOOL streaming;
	CAM_MEDIA_FORMAT_INFO formats;
	CAM_MEDIA_TYPE_DESCRIPTION currMediaType;

	GENERIC_CHANNEL_CALLBACK* hSampleReqChannel;
	INT nSampleCredits;
	wStream* sampleRespBuffer;

	H264_CONTEXT* h264;

#if defined(WITH_INPUT_FORMAT_MJPG)
	AVCodecContext* avContext;
	AVPacket* avInputPkt;
	AVFrame* avOutFrame;
#endif

	/* sws_scale */
	struct SwsContext* sws;

} CameraDeviceStream;

static INLINE CAM_MEDIA_FORMAT streamInputFormat(CameraDeviceStream* stream)
{
	return stream->formats.inputFormat;
}
static INLINE CAM_MEDIA_FORMAT streamOutputFormat(CameraDeviceStream* stream)
{
	return stream->formats.outputFormat;
}

typedef struct
{
	IWTSListener* listener;
	GENERIC_LISTENER_CALLBACK* hlistener;
	CameraPlugin* ecam;
	ICamHal* ihal; /* HAL interface, same as used by CameraPlugin */
	char deviceId[32];
	CameraDeviceStream streams[ECAM_DEVICE_MAX_STREAMS];

} CameraDevice;

/**
 * Subsystem (Hardware Abstraction Layer, HAL) Interface
 */

typedef UINT (*ICamHalEnumCallback)(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                    const char* deviceId, const char* deviceName);

/* may run in context of different thread */
typedef UINT (*ICamHalSampleCapturedCallback)(CameraDevice* dev, int streamIndex,
                                              const BYTE* sample, size_t size);

struct s_ICamHal
{
	UINT(*Enumerate)
	(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
	 GENERIC_CHANNEL_CALLBACK* hchannel);
	INT16(*GetMediaTypeDescriptions)
	(ICamHal* ihal, const char* deviceId, int streamIndex,
	 const CAM_MEDIA_FORMAT_INFO* supportedFormats, size_t nSupportedFormats,
	 CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes, size_t* nMediaTypes);
	UINT(*StartStream)
	(ICamHal* ihal, CameraDevice* dev, int streamIndex, const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
	 ICamHalSampleCapturedCallback callback);
	UINT (*StopStream)(ICamHal* ihal, const char* deviceId, int streamIndex);
	UINT (*Free)(ICamHal* hal);
};

typedef UINT (*PREGISTERCAMERAHAL)(IWTSPlugin* plugin, ICamHal* hal);

typedef struct
{
	IWTSPlugin* plugin;
	PREGISTERCAMERAHAL pRegisterCameraHal;
	CameraPlugin* ecam;
	const ADDIN_ARGV* args;

} FREERDP_CAMERA_HAL_ENTRY_POINTS;

typedef FREERDP_CAMERA_HAL_ENTRY_POINTS* PFREERDP_CAMERA_HAL_ENTRY_POINTS;

/* entry point called by addin manager */
typedef UINT(VCAPITYPE* PFREERDP_CAMERA_HAL_ENTRY)(PFREERDP_CAMERA_HAL_ENTRY_POINTS pEntryPoints);

/* common functions */
UINT ecam_channel_send_generic_msg(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                   CAM_MSG_ID msg);
UINT ecam_channel_send_error_response(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                      CAM_ERROR_CODE code);
UINT ecam_channel_write(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel, CAM_MSG_ID msg,
                        wStream* out, BOOL freeStream);

/* ecam device interface */
void ecam_dev_destroy(CameraDevice* dev);

WINPR_ATTR_MALLOC(ecam_dev_destroy, 1)
CameraDevice* ecam_dev_create(CameraPlugin* ecam, const char* deviceId, const char* deviceName);

/* video encoding interface */
BOOL ecam_encoder_context_init(CameraDeviceStream* stream);
BOOL ecam_encoder_context_free(CameraDeviceStream* stream);
BOOL ecam_encoder_compress(CameraDeviceStream* stream, const BYTE* srcData, size_t srcSize,
                           BYTE** ppDstData, size_t* pDstSize);

#endif /* FREERDP_CLIENT_CAMERA_H */
