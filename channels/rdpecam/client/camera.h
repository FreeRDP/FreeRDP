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

#if defined(WITH_INPUT_FORMAT_MJPG) && !defined(WITH_SWSCALE_LOADING)
#include <libavcodec/avcodec.h>
#endif

#if defined(WITH_SWSCALE_LOADING)
#include "libfreerdp/codec/swscale.h"
#else
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#endif

#include <winpr/wlog.h>
#include <winpr/wtypes.h>

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

/* Special format addition for CAM_MEDIA_FORMAT enum formats
 * used to support H264 stream muxed in MJPG container stream.
 * The value picked not to overlap with enum values
 */
#define CAM_MEDIA_FORMAT_MJPG_H264 0x0401

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
	CRITICAL_SECTION lock;
	volatile LONG samplesRequested;
	wStream* pendingSample;
	volatile BOOL haveSample;
	wStream* sampleRespBuffer;

	H264_CONTEXT* h264;

#if defined(WITH_INPUT_FORMAT_MJPG) && !defined(WITH_SWSCALE_LOADING)
	AVCodecContext* avContext;
	AVPacket* avInputPkt;
	AVFrame* avOutFrame;
#endif

#if defined(WITH_INPUT_FORMAT_H264)
	size_t h264FrameMaxSize;
	BYTE* h264Frame;
#endif

	/* sws_scale */
	struct SwsContext* sws;

} CameraDeviceStream;

static inline CAM_MEDIA_FORMAT streamInputFormat(CameraDeviceStream* stream)
{
	return stream->formats.inputFormat;
}
static inline CAM_MEDIA_FORMAT streamOutputFormat(CameraDeviceStream* stream)
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

/** @brief interface to implement for the camera HAL*/
struct s_ICamHal
{
	/** callback to enumerate available camera calling callback for each found item
	 *
	 * @param ihal the hal interface
	 * @param callback the enum callback
	 * @param ecam the camera plugin
	 * @param hchannel the generic freerdp channel
	 * @return the number of found cameras
	 */
	UINT(*Enumerate)
	(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
	 GENERIC_CHANNEL_CALLBACK* hchannel);

	/**
	 * callback to activate a given camera device
	 * @param ihal the hal interface
	 * @param deviceId the name of the device
	 * @param errorCode a pointer to an error code set if the call failed
	 * @return if the operation was successful
	 * @since 3.18.0
	 */
	BOOL (*Activate)(ICamHal* ihal, const char* deviceId, UINT32* errorCode);

	/**
	 * callback to deactivate a given camera device
	 * @param ihal the hal interface
	 * @param deviceId the name of the device
	 * @param errorCode a pointer to an error code set if the call failed
	 * @return if the operation was successful
	 * @since 3.18.0
	 */
	BOOL (*Deactivate)(ICamHal* ihal, const char* deviceId, UINT32* errorCode);

	/**
	 * callback that returns the list of compatible media types given a set of supported formats
	 * @param ihal the hal interface
	 * @param deviceId the name of the device
	 * @param streamIndex stream index number
	 * @param supportedFormats a pointer to supported formats
	 * @param nSupportedFormats number of supported formats
	 * @param mediaTypes resulting media type descriptors
	 * @param nMediaTypes output number of media descriptors
	 * @return number of matched supported formats
	 */
	INT16(*GetMediaTypeDescriptions)
	(ICamHal* ihal, const char* deviceId, int streamIndex,
	 const CAM_MEDIA_FORMAT_INFO* supportedFormats, size_t nSupportedFormats,
	 CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes, size_t* nMediaTypes);

	/**
	 * callback to start a stream
	 * @param ihal the hal interface
	 * @param dev
	 * @param streamIndex stream index number
	 * @param mediaType
	 * @param callback
	 * @return 0 on success, a CAM_Error otherwise
	 */
	UINT(*StartStream)
	(ICamHal* ihal, CameraDevice* dev, int streamIndex, const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
	 ICamHalSampleCapturedCallback callback);

	/**
	 * callback to stop a stream
	 * @param ihal the hal interface
	 * @param deviceId the name of the device
	 * @param streamIndex stream index number
	 * @return 0 on success, a CAM_Error otherwise
	 */
	UINT (*StopStream)(ICamHal* ihal, const char* deviceId, int streamIndex);

	/**
	 * callback to free the ICamHal
	 * @param hal the hal interface
	 * @return 0 on success, a CAM_Error otherwise
	 */
	UINT (*Free)(ICamHal* ihal);
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
UINT32 h264_get_max_bitrate(UINT32 height);

#endif /* FREERDP_CLIENT_CAMERA_H */
