/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, Android NDK Camera2 HAL
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraCaptureSession.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include "../camera.h"

#define TAG CHANNELS_TAG("rdpecam-android.client")

#define ANDROID_CAMERA_PREFIX "android-camera-"
#define ANDROID_CAMERA_PREFIX_LEN (sizeof(ANDROID_CAMERA_PREFIX) - 1)

#define CAM_ANDROID_FPS 30

typedef struct
{
	ICamHal iHal;
	ACameraManager* manager;
	wHashTable* streams; /* key: deviceId, value: CamAndroidStream */
} CamAndroidHal;

typedef struct
{
	size_t streamIndex;

	ACameraDevice* device;
	ACameraCaptureSession* session;
	ACaptureSessionOutputContainer* outputContainer;
	ACaptureSessionOutput* sessionOutput;
	ACameraOutputTarget* outputTarget;
	ACaptureRequest* captureRequest;
	AImageReader* imageReader;
	ANativeWindow* window;

	ICamHalSampleCapturedCallback sampleCallback;
	CameraDevice* dev;
	BOOL streaming;
	BOOL deviceError;
	CRITICAL_SECTION lock;

	BYTE* nv12Buffer;
	size_t nv12BufferSize;
} CamAndroidStream;

static const char* cam_android_extract_camera_id(const char* deviceId)
{
	WINPR_ASSERT(deviceId);

	if (strncmp(deviceId, ANDROID_CAMERA_PREFIX, ANDROID_CAMERA_PREFIX_LEN) != 0)
		return NULL;
	return deviceId + ANDROID_CAMERA_PREFIX_LEN;
}

/* Tear down the capture session and its request objects. The ImageReader and device are kept. */
static void cam_android_close_session(CamAndroidStream* stream)
{
	WINPR_ASSERT(stream);

	if (stream->session)
	{
		ACameraCaptureSession_stopRepeating(stream->session);
		ACameraCaptureSession_close(stream->session);
		stream->session = NULL;
	}
	if (stream->captureRequest)
	{
		if (stream->outputTarget)
			ACaptureRequest_removeTarget(stream->captureRequest, stream->outputTarget);
		ACaptureRequest_free(stream->captureRequest);
		stream->captureRequest = NULL;
	}
	if (stream->outputTarget)
	{
		ACameraOutputTarget_free(stream->outputTarget);
		stream->outputTarget = NULL;
	}
	if (stream->outputContainer)
	{
		ACaptureSessionOutputContainer_free(stream->outputContainer);
		stream->outputContainer = NULL;
	}
	if (stream->sessionOutput)
	{
		ACaptureSessionOutput_free(stream->sessionOutput);
		stream->sessionOutput = NULL;
	}
}

/* Pack a YUV_420_888 frame into NV12 (handles NV12, NV21 and planar I420 layouts). */
static void cam_android_yuv420_888_to_nv12(BYTE* dst, int width, int height, const uint8_t* yData,
                                           int yRowStride, const uint8_t* uData, int uRowStride,
                                           int uPixelStride, const uint8_t* vData, int vRowStride,
                                           int vPixelStride)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(yData);
	WINPR_ASSERT(uData);
	WINPR_ASSERT(vData);

	const size_t ySize = (size_t)width * height;

	/* Y plane */
	if (yRowStride == width)
		memcpy(dst, yData, ySize);
	else
	{
		for (int row = 0; row < height; row++)
			memcpy(dst + (size_t)row * width, yData + (size_t)row * yRowStride, (size_t)width);
	}

	/* UV planes -> interleaved NV12 */
	BYTE* uv = dst + ySize;
	const int uvHeight = height / 2;
	const int uvWidth = width / 2;
	const size_t uvRowBytes = (size_t)(uvWidth * 2);

	if (uPixelStride == 2 && vPixelStride == 2)
	{
		/* Semi-planar: NV12 (U first) or NV21 (V first). */
		if (uData < vData)
		{
			/* NV12: copy the UV block directly. */
			if (uRowStride == (int)uvRowBytes)
				memcpy(uv, uData, (size_t)uvHeight * uvRowBytes);
			else
			{
				for (int row = 0; row < uvHeight; row++)
					memcpy(uv + (size_t)row * uvRowBytes, uData + (size_t)row * uRowStride,
					       uvRowBytes);
			}
		}
		else
		{
			/* NV21: swap U and V bytes. */
			for (int row = 0; row < uvHeight; row++)
			{
				const uint8_t* u = uData + (size_t)row * uRowStride;
				const uint8_t* v = vData + (size_t)row * vRowStride;
				BYTE* d = uv + (size_t)row * uvRowBytes;
				for (int col = 0; col < uvWidth; col++, u += 2, v += 2, d += 2)
				{
					d[0] = u[0];
					d[1] = v[0];
				}
			}
		}
	}
	else
	{
		/* Planar I420: interleave U and V. */
		for (int row = 0; row < uvHeight; row++)
		{
			const uint8_t* uRow = uData + (size_t)row * uRowStride;
			const uint8_t* vRow = vData + (size_t)row * vRowStride;
			BYTE* dstRow = uv + (size_t)row * uvRowBytes;
			for (int col = 0; col < uvWidth; col++)
			{
				dstRow[col * 2] = uRow[col * uPixelStride];
				dstRow[col * 2 + 1] = vRow[col * vPixelStride];
			}
		}
	}
}

static void cam_android_deliver_frame(CamAndroidStream* stream, AImage* image)
{
	WINPR_ASSERT(stream);
	WINPR_ASSERT(image);

	int32_t width = 0;
	int32_t height = 0;
	if (AImage_getWidth(image, &width) != AMEDIA_OK ||
	    AImage_getHeight(image, &height) != AMEDIA_OK)
		return;

	const size_t ySize = (size_t)(width * height);
	const size_t nv12Size = ySize + ySize / 2;

	if (stream->nv12BufferSize < nv12Size)
	{
		BYTE* tmp = (BYTE*)realloc(stream->nv12Buffer, nv12Size);
		if (!tmp)
			return;
		stream->nv12Buffer = tmp;
		stream->nv12BufferSize = nv12Size;
	}

	/* Fetch the planes; the length out-param is required but unused. */
	int dataLength = 0;
	uint8_t* yData = NULL;
	uint8_t* uData = NULL;
	uint8_t* vData = NULL;
	if (AImage_getPlaneData(image, 0, &yData, &dataLength) != AMEDIA_OK ||
	    AImage_getPlaneData(image, 1, &uData, &dataLength) != AMEDIA_OK ||
	    AImage_getPlaneData(image, 2, &vData, &dataLength) != AMEDIA_OK)
		return;

	int yRowStride = 0;
	int uRowStride = 0;
	int uPixelStride = 0;
	int vRowStride = 0;
	int vPixelStride = 0;
	AImage_getPlaneRowStride(image, 0, &yRowStride);
	AImage_getPlaneRowStride(image, 1, &uRowStride);
	AImage_getPlanePixelStride(image, 1, &uPixelStride);
	AImage_getPlaneRowStride(image, 2, &vRowStride);
	AImage_getPlanePixelStride(image, 2, &vPixelStride);

	cam_android_yuv420_888_to_nv12(stream->nv12Buffer, width, height, yData, yRowStride, uData,
	                               uRowStride, uPixelStride, vData, vRowStride, vPixelStride);

	stream->sampleCallback(stream->dev, stream->streamIndex, stream->nv12Buffer, nv12Size);
}

static void cam_android_on_image_available(void* context, AImageReader* reader)
{
	CamAndroidStream* stream = (CamAndroidStream*)context;
	WINPR_ASSERT(stream);
	WINPR_ASSERT(reader);

	EnterCriticalSection(&stream->lock);
	const BOOL streaming = stream->streaming;
	LeaveCriticalSection(&stream->lock);

	if (!streaming)
		return;

	AImage* image = NULL;
	if (AImageReader_acquireLatestImage(reader, &image) == AMEDIA_OK && image)
	{
		cam_android_deliver_frame(stream, image);
		AImage_delete(image);
	}
}

/* Mark the device broken so it is reopened on next use. */
static void cam_android_mark_device_error(CamAndroidStream* stream)
{
	if (!stream)
		return;
	EnterCriticalSection(&stream->lock);
	stream->deviceError = TRUE;
	stream->streaming = FALSE;
	LeaveCriticalSection(&stream->lock);
}

static void cam_android_device_disconnected(void* context, ACameraDevice* device)
{
	WINPR_UNUSED(device);
	WLog_WARN(TAG, "Camera device disconnected");
	cam_android_mark_device_error((CamAndroidStream*)context);
}

static void cam_android_device_error(void* context, ACameraDevice* device, int error)
{
	WINPR_UNUSED(device);
	WLog_ERR(TAG, "Camera device error %d", error);
	cam_android_mark_device_error((CamAndroidStream*)context);
}

static void cam_android_session_active(void* context, ACameraCaptureSession* session)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(session);
	WLog_DBG(TAG, "Capture session active");
}

static void cam_android_session_closed(void* context, ACameraCaptureSession* session)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(session);
	WLog_DBG(TAG, "Capture session closed");
}

static void cam_android_session_ready(void* context, ACameraCaptureSession* session)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(session);
	WLog_DBG(TAG, "Capture session ready");
}

/* TRUE if the camera is BACKWARD_COMPATIBLE. Depth/IR/sub-sensor cameras lack this and cannot
 * configure a normal YUV session, so they must not be offered to the server. */
static BOOL cam_android_is_backward_compatible(ACameraMetadata* characteristics)
{
	WINPR_ASSERT(characteristics);

	ACameraMetadata_const_entry caps;
	if (ACameraMetadata_getConstEntry(characteristics, ACAMERA_REQUEST_AVAILABLE_CAPABILITIES,
	                                  &caps) != ACAMERA_OK)
		return FALSE;

	for (uint32_t i = 0; i < caps.count; i++)
	{
		if (caps.data.u8[i] == ACAMERA_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE)
			return TRUE;
	}
	return FALSE;
}

static UINT cam_android_enumerate(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
                                  GENERIC_CHANNEL_CALLBACK* hchannel)
{
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);

	UINT count = 0;

	/* NV12 capture needs FFmpeg/swscale to be encoded; without it, offer no cameras. */
	if (!freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT_NV12,
	                                        FREERDP_VIDEO_FORMAT_YUV420P))
	{
		WLog_WARN(TAG, "Camera redirection unavailable: video conversion (FFmpeg/swscale) missing");
		return 0;
	}

	ACameraIdList* idList = NULL;
	if (ACameraManager_getCameraIdList(hal->manager, &idList) != ACAMERA_OK || !idList)
		return 0;

	/* Offer only the primary camera per facing direction: auxiliary back cameras (ultrawide/tele/
	 * macro/depth) advertise resolutions but fail to configure a YUV session and freeze the
	 * channel. Indexed by ACAMERA_LENS_FACING_* (FRONT=0, BACK=1, EXTERNAL=2). */
	BOOL seenFacing[ACAMERA_LENS_FACING_EXTERNAL + 1] = WINPR_C_ARRAY_INIT;

	for (int i = 0; i < idList->numCameras; i++)
	{
		const char* cameraId = idList->cameraIds[i];

		ACameraMetadata* characteristics = NULL;
		if (ACameraManager_getCameraCharacteristics(hal->manager, cameraId, &characteristics) !=
		    ACAMERA_OK)
			continue;

		/* Skip depth/IR/sub-sensor cameras that can't configure a normal capture session. */
		if (!cam_android_is_backward_compatible(characteristics))
		{
			WLog_DBG(TAG, "Skipping camera %s: not BACKWARD_COMPATIBLE", cameraId);
			ACameraMetadata_free(characteristics);
			continue;
		}

		ACameraMetadata_const_entry facingEntry;
		const char* facingName = "Camera";
		uint8_t facing = ACAMERA_LENS_FACING_EXTERNAL;
		if (ACameraMetadata_getConstEntry(characteristics, ACAMERA_LENS_FACING, &facingEntry) ==
		    ACAMERA_OK)
		{
			facing = facingEntry.data.u8[0];
			switch (facing)
			{
				case ACAMERA_LENS_FACING_FRONT:
					facingName = "Front Camera";
					break;
				case ACAMERA_LENS_FACING_BACK:
					facingName = "Back Camera";
					break;
				default:
					facingName = "External Camera";
					break;
			}
		}

		/* Bounds-guard seenFacing[] against an out-of-range facing value. */
		if (facing <= ACAMERA_LENS_FACING_EXTERNAL)
		{
			if (seenFacing[facing])
			{
				WLog_DBG(TAG, "Skipping camera %s: already have a %s", cameraId, facingName);
				ACameraMetadata_free(characteristics);
				continue;
			}
			seenFacing[facing] = TRUE;
		}

		char deviceId[64];
		(void)_snprintf(deviceId, sizeof(deviceId), ANDROID_CAMERA_PREFIX "%s", cameraId);

		IFCALL(callback, ecam, hchannel, deviceId, facingName);
		count++;

		ACameraMetadata_free(characteristics);
	}

	ACameraManager_deleteCameraIdList(idList);
	return count;
}

/* Stop streaming and fully release the camera. Order matters: close the session and device before
 * the ImageReader, or the camera service fails to unconfigure and leaves the device running. */
static void cam_android_close_device(CamAndroidStream* stream)
{
	WINPR_ASSERT(stream);

	EnterCriticalSection(&stream->lock);
	stream->streaming = FALSE;
	LeaveCriticalSection(&stream->lock);

	if (stream->imageReader)
		AImageReader_setImageListener(stream->imageReader, NULL);

	cam_android_close_session(stream);

	if (stream->device)
	{
		ACameraDevice_close(stream->device);
		stream->device = NULL;
	}

	if (stream->imageReader)
	{
		AImageReader_delete(stream->imageReader);
		stream->imageReader = NULL;
		stream->window = NULL;
	}
	free(stream->nv12Buffer);
	stream->nv12Buffer = NULL;
	stream->nv12BufferSize = 0;
	stream->deviceError = FALSE;
}

/* Close every open camera except keepDeviceId. Phones share camera hardware, so holding more than
 * one open makes the next openCamera fail with CAMERA_DISCONNECTED and freeze the channel. */
static void cam_android_close_other_devices(CamAndroidHal* hal, const char* keepDeviceId)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(keepDeviceId);

	ULONG_PTR* keys = NULL;
	const size_t count = HashTable_GetKeys(hal->streams, &keys);
	for (size_t i = 0; i < count; i++)
	{
		const char* id = (const char*)keys[i];
		if (strcmp(id, keepDeviceId) == 0)
			continue;

		CamAndroidStream* other = (CamAndroidStream*)HashTable_GetItemValue(hal->streams, id);
		if (!other || !other->device)
			continue;

		WLog_DBG(TAG, "Closing camera %s to free resources for %s", id, keepDeviceId);
		cam_android_close_device(other);
	}
	free(keys);
}

static CamAndroidStream* cam_android_get_or_create_stream(CamAndroidHal* hal, const char* deviceId,
                                                          CAM_ERROR_CODE* errorCode)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(errorCode);

	CamAndroidStream* stream = (CamAndroidStream*)HashTable_GetItemValue(hal->streams, deviceId);
	if (stream)
		return stream;

	stream = (CamAndroidStream*)calloc(1, sizeof(CamAndroidStream));
	if (!stream)
	{
		*errorCode = CAM_ERROR_CODE_OutOfMemory;
		return NULL;
	}
	if (!InitializeCriticalSectionEx(&stream->lock, 0, 0))
	{
		free(stream);
		*errorCode = CAM_ERROR_CODE_UnexpectedError;
		return NULL;
	}
	if (!HashTable_Insert(hal->streams, deviceId, stream))
	{
		DeleteCriticalSection(&stream->lock);
		free(stream);
		*errorCode = CAM_ERROR_CODE_UnexpectedError;
		return NULL;
	}
	return stream;
}

/* Open the camera, closing any other open one first (phones keep only one open). Deferred from
 * Activate to StartStream so media-type probing doesn't thrash the hardware. */
static BOOL cam_android_open_device(CamAndroidHal* hal, CamAndroidStream* stream,
                                    const char* deviceId, const char* cameraId,
                                    CAM_ERROR_CODE* errorCode)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(stream);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(cameraId);
	WINPR_ASSERT(errorCode);

	/* Reopen a device left broken by a previous error. */
	EnterCriticalSection(&stream->lock);
	const BOOL hadError = stream->deviceError;
	LeaveCriticalSection(&stream->lock);
	if (stream->device && hadError)
		cam_android_close_device(stream);

	if (stream->device)
		return TRUE; /* already open */

	cam_android_close_other_devices(hal, deviceId);

	ACameraDevice_StateCallbacks deviceCallbacks = {
		.context = stream,
		.onDisconnected = cam_android_device_disconnected,
		.onError = cam_android_device_error,
	};

	camera_status_t status =
	    ACameraManager_openCamera(hal->manager, cameraId, &deviceCallbacks, &stream->device);
	if (status != ACAMERA_OK)
	{
		WLog_ERR(TAG, "ACameraManager_openCamera failed: %d", status);
		*errorCode = CAM_ERROR_CODE_InvalidRequest;
		return FALSE;
	}

	WLog_DBG(TAG, "Opened camera %s", cameraId);
	return TRUE;
}

static BOOL cam_android_activate(ICamHal* ihal, const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(errorCode);

	*errorCode = CAM_ERROR_CODE_None;

	const char* cameraId = cam_android_extract_camera_id(deviceId);
	if (!cameraId)
	{
		*errorCode = CAM_ERROR_CODE_InvalidRequest;
		return FALSE;
	}

	/* Hardware is opened lazily in StartStream, not here. */
	return cam_android_get_or_create_stream(hal, deviceId, errorCode) != NULL;
}

static BOOL cam_android_deactivate(ICamHal* ihal, const char* deviceId, CAM_ERROR_CODE* errorCode)
{
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(errorCode);

	*errorCode = CAM_ERROR_CODE_None;

	CamAndroidStream* stream = (CamAndroidStream*)HashTable_GetItemValue(hal->streams, deviceId);
	if (!stream || !stream->device)
		return TRUE;

	cam_android_close_device(stream);
	return TRUE;
}

static INT16 cam_android_get_media_type_descriptions(ICamHal* ihal, const char* deviceId,
                                                     size_t streamIndex,
                                                     const CAM_MEDIA_FORMAT_INFO* supportedFormats,
                                                     size_t nSupportedFormats,
                                                     CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes,
                                                     size_t* nMediaTypes)
{
	WINPR_UNUSED(streamIndex);
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(supportedFormats || (nSupportedFormats == 0));
	WINPR_ASSERT(mediaTypes);
	WINPR_ASSERT(nMediaTypes);

	size_t maxMediaTypes = *nMediaTypes;
	*nMediaTypes = 0;

	const char* cameraId = cam_android_extract_camera_id(deviceId);
	if (!cameraId)
		return -1;

	/* Find the NV12 format index. */
	INT16 formatIndex = -1;
	for (size_t i = 0; i < nSupportedFormats; i++)
	{
		if (supportedFormats[i].inputFormat == CAM_MEDIA_FORMAT_NV12)
		{
			formatIndex = (INT16)i;
			break;
		}
	}
	if (formatIndex < 0)
	{
		WLog_WARN(TAG, "Server does not offer NV12 format");
		return -1;
	}

	ACameraMetadata* characteristics = NULL;
	if (ACameraManager_getCameraCharacteristics(hal->manager, cameraId, &characteristics) !=
	    ACAMERA_OK)
		return -1;

	/* Stream configs: [format, width, height, isInput] tuples. */
	ACameraMetadata_const_entry entry;
	if (ACameraMetadata_getConstEntry(
	        characteristics, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry) != ACAMERA_OK)
	{
		ACameraMetadata_free(characteristics);
		return -1;
	}

	size_t nTypes = 0;
	for (uint32_t i = 0; i + 3 < entry.count; i += 4)
	{
		const int32_t fmt = entry.data.i32[i];
		const int32_t w = entry.data.i32[i + 1];
		const int32_t h = entry.data.i32[i + 2];
		const int32_t isInput = entry.data.i32[i + 3];

		if (isInput != 0)
			continue;
		if (fmt != AIMAGE_FORMAT_YUV_420_888)
			continue;
		/* Skip resolutions larger than 1080p to avoid saturating the RDP link */
		if (w > 1920 || h > 1080)
			continue;

		mediaTypes[nTypes].Format = CAM_MEDIA_FORMAT_NV12;
		mediaTypes[nTypes].Width = (UINT32)w;
		mediaTypes[nTypes].Height = (UINT32)h;
		mediaTypes[nTypes].FrameRateNumerator = CAM_ANDROID_FPS;
		mediaTypes[nTypes].FrameRateDenominator = 1;
		mediaTypes[nTypes].PixelAspectRatioNumerator = 1;
		mediaTypes[nTypes].PixelAspectRatioDenominator = 1;

		WLog_DBG(TAG, "Camera format NV12 %dx%d @ %dfps", w, h, CAM_ANDROID_FPS);

		if (++nTypes >= maxMediaTypes)
			break;
	}

	*nMediaTypes = nTypes;
	ACameraMetadata_free(characteristics);

	if (nTypes == 0)
	{
		WLog_ERR(TAG, "No supported YUV resolutions found for camera %s", cameraId);
		return -1;
	}

	return formatIndex;
}

static CAM_ERROR_CODE cam_android_start_stream(ICamHal* ihal, CameraDevice* dev, size_t streamIndex,
                                               const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
                                               ICamHalSampleCapturedCallback callback)
{
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(dev);
	WINPR_ASSERT(mediaType);
	WINPR_ASSERT(callback);

	const char* deviceId = dev->deviceId;

	const char* cameraId = cam_android_extract_camera_id(deviceId);
	if (!cameraId)
		return CAM_ERROR_CODE_InvalidRequest;

	CAM_ERROR_CODE errorCode = CAM_ERROR_CODE_None;
	CamAndroidStream* stream = cam_android_get_or_create_stream(hal, deviceId, &errorCode);
	if (!stream)
		return errorCode;

	if (!cam_android_open_device(hal, stream, deviceId, cameraId, &errorCode))
		return errorCode;

	stream->dev = dev;
	stream->streamIndex = streamIndex;
	stream->sampleCallback = callback;

	const int32_t width = (int32_t)mediaType->Width;
	const int32_t height = (int32_t)mediaType->Height;

	if (AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 4, &stream->imageReader) !=
	    AMEDIA_OK)
	{
		WLog_ERR(TAG, "AImageReader_new failed");
		return CAM_ERROR_CODE_UnexpectedError;
	}

	AImageReader_ImageListener listener = {
		.context = stream,
		.onImageAvailable = cam_android_on_image_available,
	};
	AImageReader_setImageListener(stream->imageReader, &listener);

	if (AImageReader_getWindow(stream->imageReader, &stream->window) != AMEDIA_OK)
	{
		WLog_ERR(TAG, "AImageReader_getWindow failed");
		goto error;
	}

	ACaptureSessionOutput_create(stream->window, &stream->sessionOutput);
	ACaptureSessionOutputContainer_create(&stream->outputContainer);
	ACaptureSessionOutputContainer_add(stream->outputContainer, stream->sessionOutput);
	ACameraOutputTarget_create(stream->window, &stream->outputTarget);
	ACameraDevice_createCaptureRequest(stream->device, TEMPLATE_RECORD, &stream->captureRequest);
	ACaptureRequest_addTarget(stream->captureRequest, stream->outputTarget);

	ACameraCaptureSession_stateCallbacks sessionCallbacks = {
		.context = stream,
		.onActive = cam_android_session_active,
		.onClosed = cam_android_session_closed,
		.onReady = cam_android_session_ready,
	};

	if (ACameraDevice_createCaptureSession(stream->device, stream->outputContainer,
	                                       &sessionCallbacks, &stream->session) != ACAMERA_OK)
	{
		WLog_ERR(TAG, "ACameraDevice_createCaptureSession failed");
		goto error;
	}

	if (ACameraCaptureSession_setRepeatingRequest(stream->session, NULL, 1, &stream->captureRequest,
	                                              NULL) != ACAMERA_OK)
	{
		WLog_ERR(TAG, "ACameraCaptureSession_setRepeatingRequest failed");
		goto error;
	}

	EnterCriticalSection(&stream->lock);
	stream->streaming = TRUE;
	LeaveCriticalSection(&stream->lock);

	WLog_DBG(TAG, "Camera stream started: %dx%d", width, height);
	return CAM_ERROR_CODE_None;

error:
	cam_android_close_device(stream);
	return CAM_ERROR_CODE_UnexpectedError;
}

static CAM_ERROR_CODE cam_android_stop_stream(ICamHal* ihal, const char* deviceId,
                                              size_t streamIndex)
{
	WINPR_UNUSED(streamIndex);
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);

	CamAndroidStream* stream = (CamAndroidStream*)HashTable_GetItemValue(hal->streams, deviceId);
	if (!stream)
		return CAM_ERROR_CODE_None;

	cam_android_close_device(stream);

	WLog_DBG(TAG, "Camera stream stopped for %s", deviceId);
	return CAM_ERROR_CODE_None;
}

static void cam_android_stream_free(void* obj)
{
	CamAndroidStream* stream = (CamAndroidStream*)obj;
	if (!stream)
		return;

	cam_android_close_device(stream);
	DeleteCriticalSection(&stream->lock);
	free(stream);
}

static CAM_ERROR_CODE cam_android_free(ICamHal* ihal)
{
	CamAndroidHal* hal = (CamAndroidHal*)ihal;
	if (!hal)
		return CAM_ERROR_CODE_NotInitialized;

	HashTable_Free(hal->streams);
	if (hal->manager)
		ACameraManager_delete(hal->manager);
	free(hal);
	return CAM_ERROR_CODE_None;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE android_freerdp_rdpecam_client_subsystem_entry(
    PFREERDP_CAMERA_HAL_ENTRY_POINTS pEntryPoints))
{
	WINPR_ASSERT(pEntryPoints);

	CamAndroidHal* hal = (CamAndroidHal*)calloc(1, sizeof(CamAndroidHal));
	if (!hal)
		return CHANNEL_RC_NO_MEMORY;

	hal->manager = ACameraManager_create();
	if (!hal->manager)
	{
		free(hal);
		return ERROR_INTERNAL_ERROR;
	}

	hal->streams = HashTable_New(FALSE);
	if (!hal->streams)
		goto error;

	if (!HashTable_SetupForStringData(hal->streams, FALSE))
		goto error;

	wObject* obj = HashTable_ValueObject(hal->streams);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = cam_android_stream_free;

	hal->iHal.Enumerate = cam_android_enumerate;
	hal->iHal.Activate = cam_android_activate;
	hal->iHal.Deactivate = cam_android_deactivate;
	hal->iHal.GetMediaTypeDescriptions = cam_android_get_media_type_descriptions;
	hal->iHal.StartStream = cam_android_start_stream;
	hal->iHal.StopStream = cam_android_stop_stream;
	hal->iHal.Free = cam_android_free;

	UINT ret = pEntryPoints->pRegisterCameraHal(pEntryPoints->plugin, &hal->iHal);
	if (ret != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "RegisterCameraHal failed: %" PRIu32, ret);
		goto error;
	}

	return ret;

error:
	cam_android_free(&hal->iHal);
	return ERROR_INTERNAL_ERROR;
}
