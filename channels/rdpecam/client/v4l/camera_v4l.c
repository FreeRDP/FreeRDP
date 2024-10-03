/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, V4L Interface
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

/* v4l includes */
#include <linux/videodev2.h>

#include "camera.h"
#include <winpr/handle.h>

#define TAG CHANNELS_TAG("rdpecam-v4l.client")

#define CAM_V4L2_BUFFERS_COUNT 4
#define CAM_V4L2_CAPTURE_THREAD_SLEEP_MS 1000

#define CAM_V4L2_FRAMERATE_NUMERATOR_DEFAULT 30
#define CAM_V4L2_FRAMERATE_DENOMINATOR_DEFAULT 1

typedef struct
{
	void* start;
	size_t length;

} CamV4lBuffer;

typedef struct
{
	CRITICAL_SECTION lock;

	/* members used to call the callback */
	CameraDevice* dev;
	int streamIndex;
	ICamHalSampleCapturedCallback sampleCallback;

	BOOL streaming;
	int fd;
	size_t nBuffers;
	CamV4lBuffer* buffers;
	HANDLE captureThread;

} CamV4lStream;

typedef struct
{
	ICamHal iHal;

	wHashTable* streams; /* Index: deviceId, Value: CamV4lStream */

} CamV4lHal;

static void cam_v4l_stream_free(void* obj);
static void cam_v4l_stream_close_device(CamV4lStream* stream);
static UINT cam_v4l_stream_stop(CamV4lStream* stream);

/**
 * Function description
 *
 * @return NULL-terminated fourcc string
 */
static const char* cam_v4l_get_fourcc_str(unsigned int fourcc, char* buffer, size_t size)
{
	if (size < 5)
		return NULL;

	buffer[0] = (char)(fourcc & 0xFF);
	buffer[1] = (char)((fourcc >> 8) & 0xFF);
	buffer[2] = (char)((fourcc >> 16) & 0xFF);
	buffer[3] = (char)((fourcc >> 24) & 0xFF);
	buffer[4] = '\0';
	return buffer;
}

/**
 * Function description
 *
 * @return one of V4L2_PIX_FMT
 */
static UINT32 ecamToV4L2PixFormat(CAM_MEDIA_FORMAT ecamFormat)
{
	switch (ecamFormat)
	{
		case CAM_MEDIA_FORMAT_H264:
			return V4L2_PIX_FMT_H264;
		case CAM_MEDIA_FORMAT_MJPG:
			return V4L2_PIX_FMT_MJPEG;
		case CAM_MEDIA_FORMAT_YUY2:
			return V4L2_PIX_FMT_YUYV;
		case CAM_MEDIA_FORMAT_NV12:
			return V4L2_PIX_FMT_NV12;
		case CAM_MEDIA_FORMAT_I420:
			return V4L2_PIX_FMT_YUV420;
		case CAM_MEDIA_FORMAT_RGB24:
			return V4L2_PIX_FMT_RGB24;
		case CAM_MEDIA_FORMAT_RGB32:
			return V4L2_PIX_FMT_RGB32;
		default:
			WLog_ERR(TAG, "Unsupported CAM_MEDIA_FORMAT %d", ecamFormat);
			return 0;
	}
}

/**
 * Function description
 *
 * @return TRUE or FALSE
 */
static BOOL cam_v4l_format_supported(int fd, UINT32 format)
{
	struct v4l2_fmtdesc fmtdesc = { 0 };
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	for (fmtdesc.index = 0; ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0; fmtdesc.index++)
	{
		if (fmtdesc.pixelformat == format)
			return TRUE;
	}
	return FALSE;
}

/**
 * Function description
 *
 * @return file descriptor
 */
static int cam_v4l_open_device(const char* deviceId, int flags)
{
	char device[20] = { 0 };
	int fd = -1;
	struct v4l2_capability cap = { 0 };

	if (!deviceId)
		return -1;

	if (0 == strncmp(deviceId, "/dev/video", 10))
		return open(deviceId, flags);

	for (UINT n = 0; n < 64; n++)
	{
		(void)_snprintf(device, sizeof(device), "/dev/video%d", n);
		if ((fd = open(device, flags)) == -1)
			continue;

		/* query device capabilities and make sure this is a video capture device */
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE))
		{
			close(fd);
			continue;
		}

		if (cap.bus_info[0] != 0 && 0 == strcmp((const char*)cap.bus_info, deviceId))
			return fd;

		close(fd);
	}

	return fd;
}

/**
 * Function description
 *
 * @return -1 if error, otherwise index of supportedFormats array and mediaTypes/nMediaTypes filled
 * in
 */
static INT16 cam_v4l_get_media_type_descriptions(ICamHal* hal, const char* deviceId,
                                                 int streamIndex,
                                                 const CAM_MEDIA_FORMAT_INFO* supportedFormats,
                                                 size_t nSupportedFormats,
                                                 CAM_MEDIA_TYPE_DESCRIPTION* mediaTypes,
                                                 size_t* nMediaTypes)
{
	size_t maxMediaTypes = *nMediaTypes;
	size_t nTypes = 0;
	BOOL formatFound = FALSE;

	int fd = cam_v4l_open_device(deviceId, O_RDONLY);
	if (fd == -1)
	{
		WLog_ERR(TAG, "Unable to open device %s", deviceId);
		return -1;
	}

	size_t formatIndex = 0;
	for (; formatIndex < nSupportedFormats; formatIndex++)
	{
		UINT32 pixelFormat = ecamToV4L2PixFormat(supportedFormats[formatIndex].inputFormat);
		WINPR_ASSERT(pixelFormat != 0);
		struct v4l2_frmsizeenum frmsize = { 0 };

		if (!cam_v4l_format_supported(fd, pixelFormat))
			continue;

		frmsize.pixel_format = pixelFormat;
		for (frmsize.index = 0; ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) == 0; frmsize.index++)
		{
			struct v4l2_frmivalenum frmival = { 0 };

			if (frmsize.type != V4L2_FRMSIZE_TYPE_DISCRETE)
				break; /* don't support size types other than discrete */

			formatFound = TRUE;
			mediaTypes->Width = frmsize.discrete.width;
			mediaTypes->Height = frmsize.discrete.height;
			mediaTypes->Format = supportedFormats[formatIndex].inputFormat;

			/* query frame rate (1st is highest fps supported) */
			frmival.index = 0;
			frmival.pixel_format = pixelFormat;
			frmival.width = frmsize.discrete.width;
			frmival.height = frmsize.discrete.height;
			if (ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) == 0 &&
			    frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE)
			{
				/* inverse of a fraction */
				mediaTypes->FrameRateNumerator = frmival.discrete.denominator;
				mediaTypes->FrameRateDenominator = frmival.discrete.numerator;
			}
			else
			{
				WLog_DBG(TAG, "VIDIOC_ENUM_FRAMEINTERVALS failed, using default framerate");
				mediaTypes->FrameRateNumerator = CAM_V4L2_FRAMERATE_NUMERATOR_DEFAULT;
				mediaTypes->FrameRateDenominator = CAM_V4L2_FRAMERATE_DENOMINATOR_DEFAULT;
			}

			mediaTypes->PixelAspectRatioNumerator = mediaTypes->PixelAspectRatioDenominator = 1;

			char fourccstr[5] = { 0 };
			WLog_DBG(TAG, "Camera format: %s, width: %u, height: %u, fps: %u/%u",
			         cam_v4l_get_fourcc_str(pixelFormat, fourccstr, ARRAYSIZE(fourccstr)),
			         mediaTypes->Width, mediaTypes->Height, mediaTypes->FrameRateNumerator,
			         mediaTypes->FrameRateDenominator);

			mediaTypes++;
			nTypes++;

			if (nTypes == maxMediaTypes)
			{
				WLog_ERR(TAG, "Media types reached buffer maximum %" PRIu32 "", maxMediaTypes);
				goto error;
			}
		}

		if (formatFound)
		{
			/* we are interested in 1st supported format only, with all supported sizes */
			break;
		}
	}

error:

	*nMediaTypes = nTypes;
	close(fd);
	if (formatIndex > INT16_MAX)
		return -1;
	return (INT16)formatIndex;
}

/**
 * Function description
 *
 * @return number of video capture devices
 */
static UINT cam_v4l_enumerate(ICamHal* ihal, ICamHalEnumCallback callback, CameraPlugin* ecam,
                              GENERIC_CHANNEL_CALLBACK* hchannel)
{
	UINT count = 0;

	for (UINT n = 0; n < 64; n++)
	{
		char device[20] = { 0 };
		struct v4l2_capability cap = { 0 };
		(void)_snprintf(device, sizeof(device), "/dev/video%d", n);
		int fd = open(device, O_RDONLY);
		if (fd == -1)
			continue;

		/* query device capabilities and make sure this is a video capture device */
		if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0 || !(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE))
		{
			close(fd);
			continue;
		}
		count++;

		const char* deviceName = (char*)cap.card;
		const char* deviceId = device;
		if (cap.bus_info[0] != 0) /* may not be available in all drivers */
			deviceId = (char*)cap.bus_info;

		IFCALL(callback, ecam, hchannel, deviceId, deviceName);

		close(fd);
	}

	return count;
}

static void cam_v4l_stream_free_buffers(CamV4lStream* stream)
{
	if (!stream || !stream->buffers)
		return;

	/* unmap buffers */
	for (size_t i = 0; i < stream->nBuffers; i++)
	{
		if (stream->buffers[i].length && stream->buffers[i].start != MAP_FAILED)
		{
			munmap(stream->buffers[i].start, stream->buffers[i].length);
		}
	}

	free(stream->buffers);
	stream->buffers = NULL;
	stream->nBuffers = 0;
}

/**
 * Function description
 *
 * @return 0 on failure, otherwise allocated buffer size
 */
static size_t cam_v4l_stream_alloc_buffers(CamV4lStream* stream)
{
	struct v4l2_requestbuffers rbuffer = { 0 };

	rbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rbuffer.memory = V4L2_MEMORY_MMAP;
	rbuffer.count = CAM_V4L2_BUFFERS_COUNT;

	if (ioctl(stream->fd, VIDIOC_REQBUFS, &rbuffer) < 0 || rbuffer.count == 0)
	{
		WLog_ERR(TAG, "Failure in VIDIOC_REQBUFS, errno %d, count %d", errno, rbuffer.count);
		return 0;
	}

	stream->nBuffers = rbuffer.count;

	/* Map the buffers */
	stream->buffers = (CamV4lBuffer*)calloc(rbuffer.count, sizeof(CamV4lBuffer));
	if (!stream->buffers)
	{
		WLog_ERR(TAG, "Failure in calloc");
		return 0;
	}

	for (unsigned int i = 0; i < rbuffer.count; i++)
	{
		struct v4l2_buffer buffer = { 0 };
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;

		if (ioctl(stream->fd, VIDIOC_QUERYBUF, &buffer) < 0)
		{
			WLog_ERR(TAG, "Failure in VIDIOC_QUERYBUF, errno %d", errno);
			cam_v4l_stream_free_buffers(stream);
			return 0;
		}

		stream->buffers[i].start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
		                                stream->fd, buffer.m.offset);

		if (MAP_FAILED == stream->buffers[i].start)
		{
			WLog_ERR(TAG, "Failure in mmap, errno %d", errno);
			cam_v4l_stream_free_buffers(stream);
			return 0;
		}

		stream->buffers[i].length = buffer.length;

		WLog_DBG(TAG, "Buffer %d mapped, size: %d", i, buffer.length);

		if (ioctl(stream->fd, VIDIOC_QBUF, &buffer) < 0)
		{
			WLog_ERR(TAG, "Failure in VIDIOC_QBUF, errno %d", errno);
			cam_v4l_stream_free_buffers(stream);
			return 0;
		}
	}

	return stream->buffers[0].length;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cam_v4l_stream_capture_thread(void* param)
{
	CamV4lStream* stream = (CamV4lStream*)param;

	int fd = stream->fd;

	do
	{
		int retVal = 0;
		struct pollfd pfd = { 0 };

		pfd.fd = fd;
		pfd.events = POLLIN;

		retVal = poll(&pfd, 1, CAM_V4L2_CAPTURE_THREAD_SLEEP_MS);

		if (retVal == 0)
		{
			/* poll timed out */
			continue;
		}
		else if (retVal < 0)
		{
			WLog_DBG(TAG, "Failure in poll, errno %d", errno);
			Sleep(CAM_V4L2_CAPTURE_THREAD_SLEEP_MS); /* trying to recover */
			continue;
		}
		else if (!(pfd.revents & POLLIN))
		{
			WLog_DBG(TAG, "poll reported non-read event %d", pfd.revents);
			Sleep(CAM_V4L2_CAPTURE_THREAD_SLEEP_MS); /* also trying to recover */
			continue;
		}

		EnterCriticalSection(&stream->lock);
		if (stream->streaming)
		{
			struct v4l2_buffer buf = { 0 };
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			/* dequeue buffers until empty */
			while (ioctl(fd, VIDIOC_DQBUF, &buf) != -1)
			{
				stream->sampleCallback(stream->dev, stream->streamIndex,
				                       stream->buffers[buf.index].start, buf.bytesused);

				/* enqueue buffer back */
				if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
				{
					WLog_ERR(TAG, "Failure in VIDIOC_QBUF, errno %d", errno);
				}
			}
		}
		LeaveCriticalSection(&stream->lock);

	} while (stream->streaming);

	return CHANNEL_RC_OK;
}

void cam_v4l_stream_close_device(CamV4lStream* stream)
{
	if (stream->fd != -1)
	{
		close(stream->fd);
		stream->fd = -1;
	}
}

/**
 * Function description
 *
 * @return Null on failure, otherwise pointer to new CamV4lStream
 */
static CamV4lStream* cam_v4l_stream_create(CameraDevice* dev, int streamIndex,
                                           ICamHalSampleCapturedCallback callback)
{
	CamV4lStream* stream = calloc(1, sizeof(CamV4lStream));

	if (!stream)
	{
		WLog_ERR(TAG, "Failure in calloc");
		return NULL;
	}
	stream->dev = dev;
	stream->streamIndex = streamIndex;
	stream->sampleCallback = callback;

	stream->fd = -1;

	if (!InitializeCriticalSectionEx(&stream->lock, 0, 0))
	{
		WLog_ERR(TAG, "Failure in calloc");
		free(stream);
		return NULL;
	}

	return stream;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cam_v4l_stream_stop(CamV4lStream* stream)
{
	if (!stream || !stream->streaming)
		return CHANNEL_RC_OK;

	stream->streaming = FALSE; /* this will terminate capture thread */

	if (stream->captureThread)
	{
		(void)WaitForSingleObject(stream->captureThread, INFINITE);
		(void)CloseHandle(stream->captureThread);
		stream->captureThread = NULL;
	}

	EnterCriticalSection(&stream->lock);

	/* stop streaming */
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(stream->fd, VIDIOC_STREAMOFF, &type) < 0)
	{
		WLog_ERR(TAG, "Failure in VIDIOC_STREAMOFF, errno %d", errno);
	}

	cam_v4l_stream_free_buffers(stream);
	cam_v4l_stream_close_device(stream);

	LeaveCriticalSection(&stream->lock);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise CAM_ERROR_CODE
 */
static UINT cam_v4l_stream_start(ICamHal* ihal, CameraDevice* dev, int streamIndex,
                                 const CAM_MEDIA_TYPE_DESCRIPTION* mediaType,
                                 ICamHalSampleCapturedCallback callback)
{
	CamV4lHal* hal = (CamV4lHal*)ihal;

	CamV4lStream* stream = (CamV4lStream*)HashTable_GetItemValue(hal->streams, dev->deviceId);

	if (!stream)
	{
		stream = cam_v4l_stream_create(dev, streamIndex, callback);
		if (!stream)
			return CAM_ERROR_CODE_OutOfMemory;

		if (!HashTable_Insert(hal->streams, dev->deviceId, stream))
		{
			cam_v4l_stream_free(stream);
			return CAM_ERROR_CODE_UnexpectedError;
		}
	}

	if (stream->streaming)
	{
		WLog_ERR(TAG, "Streaming already in progress, device %s, streamIndex %d", dev->deviceId,
		         streamIndex);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	if ((stream->fd = cam_v4l_open_device(dev->deviceId, O_RDWR | O_NONBLOCK)) == -1)
	{
		WLog_ERR(TAG, "Unable to open device %s", dev->deviceId);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	struct v4l2_format video_fmt = { 0 };
	video_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	video_fmt.fmt.pix.sizeimage = 0;
	video_fmt.fmt.pix.width = mediaType->Width;
	video_fmt.fmt.pix.height = mediaType->Height;
	UINT32 pixelFormat = ecamToV4L2PixFormat(mediaType->Format);
	if (pixelFormat == 0)
	{
		cam_v4l_stream_close_device(stream);
		return CAM_ERROR_CODE_InvalidMediaType;
	}
	video_fmt.fmt.pix.pixelformat = pixelFormat;

	/* set format and frame size */
	if (ioctl(stream->fd, VIDIOC_S_FMT, &video_fmt) < 0)
	{
		WLog_ERR(TAG, "Failure in VIDIOC_S_FMT, errno %d", errno);
		cam_v4l_stream_close_device(stream);
		return CAM_ERROR_CODE_InvalidMediaType;
	}

	/* trying to set frame rate, if driver supports it */
	struct v4l2_streamparm sp1 = { 0 };
	struct v4l2_streamparm sp2 = { 0 };
	sp1.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(stream->fd, VIDIOC_G_PARM, &sp1) < 0 ||
	    !(sp1.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
	{
		WLog_INFO(TAG, "Driver doesn't support setting framerate");
	}
	else
	{
		sp2.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		/* inverse of a fraction */
		sp2.parm.capture.timeperframe.numerator = mediaType->FrameRateDenominator;
		sp2.parm.capture.timeperframe.denominator = mediaType->FrameRateNumerator;

		if (ioctl(stream->fd, VIDIOC_S_PARM, &sp2) < 0)
		{
			WLog_INFO(TAG, "Failed to set the framerate, errno %d", errno);
		}
	}

	size_t maxSample = cam_v4l_stream_alloc_buffers(stream);
	if (maxSample == 0)
	{
		WLog_ERR(TAG, "Failure to allocate video buffers");
		cam_v4l_stream_close_device(stream);
		return CAM_ERROR_CODE_OutOfMemory;
	}

	stream->streaming = TRUE;

	/* start streaming */
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(stream->fd, VIDIOC_STREAMON, &type) < 0)
	{
		WLog_ERR(TAG, "Failure in VIDIOC_STREAMON, errno %d", errno);
		cam_v4l_stream_stop(stream);
		return CAM_ERROR_CODE_UnexpectedError;
	}

	stream->captureThread = CreateThread(NULL, 0, cam_v4l_stream_capture_thread, stream, 0, NULL);
	if (!stream->captureThread)
	{
		WLog_ERR(TAG, "CreateThread failure");
		cam_v4l_stream_stop(stream);
		return CAM_ERROR_CODE_OutOfMemory;
	}

	char fourccstr[5] = { 0 };
	WLog_INFO(TAG, "Camera format: %s, width: %u, height: %u, fps: %u/%u",
	          cam_v4l_get_fourcc_str(pixelFormat, fourccstr, ARRAYSIZE(fourccstr)),
	          mediaType->Width, mediaType->Height, mediaType->FrameRateNumerator,
	          mediaType->FrameRateDenominator);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cam_v4l_stream_stop_by_device_id(ICamHal* ihal, const char* deviceId, int streamIndex)
{
	CamV4lHal* hal = (CamV4lHal*)ihal;

	CamV4lStream* stream = (CamV4lStream*)HashTable_GetItemValue(hal->streams, deviceId);

	if (!stream)
		return CHANNEL_RC_OK;

	return cam_v4l_stream_stop(stream);
}

/**
 * Function description
 *
 * OBJECT_FREE_FN for streams hash table value
 *
 */
void cam_v4l_stream_free(void* obj)
{
	CamV4lStream* stream = (CamV4lStream*)obj;
	if (!stream)
		return;

	cam_v4l_stream_stop(stream);

	DeleteCriticalSection(&stream->lock);
	free(stream);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cam_v4l_free(ICamHal* ihal)
{
	CamV4lHal* hal = (CamV4lHal*)ihal;

	if (hal == NULL)
		return ERROR_INVALID_PARAMETER;

	HashTable_Free(hal->streams);

	free(hal);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE v4l_freerdp_rdpecam_client_subsystem_entry(
    PFREERDP_CAMERA_HAL_ENTRY_POINTS pEntryPoints))
{
	UINT ret = CHANNEL_RC_OK;
	WINPR_ASSERT(pEntryPoints);

	CamV4lHal* hal = (CamV4lHal*)calloc(1, sizeof(CamV4lHal));

	if (hal == NULL)
		return CHANNEL_RC_NO_MEMORY;

	hal->iHal.Enumerate = cam_v4l_enumerate;
	hal->iHal.GetMediaTypeDescriptions = cam_v4l_get_media_type_descriptions;
	hal->iHal.StartStream = cam_v4l_stream_start;
	hal->iHal.StopStream = cam_v4l_stream_stop_by_device_id;
	hal->iHal.Free = cam_v4l_free;

	hal->streams = HashTable_New(FALSE);
	if (!hal->streams)
	{
		ret = CHANNEL_RC_NO_MEMORY;
		goto error;
	}

	HashTable_SetupForStringData(hal->streams, FALSE);

	wObject* obj = HashTable_ValueObject(hal->streams);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = cam_v4l_stream_free;

	if ((ret = pEntryPoints->pRegisterCameraHal(pEntryPoints->plugin, &hal->iHal)))
	{
		WLog_ERR(TAG, "RegisterCameraHal failed with error %" PRIu32 "", ret);
		goto error;
	}

	return ret;

error:
	cam_v4l_free(&hal->iHal);
	return ret;
}
