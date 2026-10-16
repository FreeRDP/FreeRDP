/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation camera HAL for macOS and iOS
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

#import <AVFoundation/AVFoundation.h>
#import <VideoToolbox/VideoToolbox.h>
#include <stdatomic.h>
#include <winpr/assert.h>
#include "../camera.h"
#include "modes.h"
#include "nv12.h"

#define TAG CHANNELS_TAG("rdpecam-avfoundation.client")

#define CAM_AVFOUNDATION_MEDIA_FORMAT CAM_MEDIA_FORMAT_NV12

/* Prompt on first channel use; AVFoundation withholds camera frames until consent. */
WINPR_ATTR_NODISCARD
static BOOL cam_avfoundation_request_access(void)
{
	const AVAuthorizationStatus status =
	    [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
	if (status == AVAuthorizationStatusAuthorized)
		return TRUE;
	if (status == AVAuthorizationStatusNotDetermined)
	{
		NSString *usage =
		    [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NSCameraUsageDescription"];
		if (![usage isKindOfClass:[NSString class]] || (usage.length == 0))
		{
			WLog_ERR(
			    TAG,
			    "Camera access requires NSCameraUsageDescription in the application's Info.plist");
			return FALSE;
		}
		/* Coalesce requests without retaining HAL or channel pointers across the prompt. */
		static atomic_bool pending;
		bool expected = false;
		if (atomic_compare_exchange_strong(&pending, &expected, true))
		{
			WLog_INFO(TAG, "Requesting camera access");
			[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
			                         completionHandler:^(BOOL granted) {
				                         if (!granted)
					                         WLog_WARN(TAG, "Camera access denied by user");
				                         atomic_store(&pending, false);
			                         }];
		}
		return TRUE;
	}
	const char *reason = "unknown";
	switch (status)
	{
		case AVAuthorizationStatusNotDetermined:
			reason = "not determined";
			break;
		case AVAuthorizationStatusRestricted:
			reason = "restricted";
			break;
		case AVAuthorizationStatusDenied:
			reason = "denied";
			break;
		case AVAuthorizationStatusAuthorized:
			break;
	}
	WLog_WARN(TAG, "Camera access is %s", reason);
	return FALSE;
}

#if TARGET_OS_OSX && (__MAC_OS_X_VERSION_MIN_REQUIRED < 140000)
/* Describe the legacy API's lifetime so it is only used on systems before macOS 14. */
WINPR_ATTR_NODISCARD
static AVCaptureDeviceType cam_avfoundation_legacy_external_type(void)
    API_DEPRECATED_WITH_REPLACEMENT("AVCaptureDeviceTypeExternal", macos(10.15, 14.0));

static AVCaptureDeviceType cam_avfoundation_legacy_external_type(void)
{
	return AVCaptureDeviceTypeExternalUnknown;
}
#endif

WINPR_ATTR_NODISCARD
static BOOL cam_avfoundation_configure_camera(AVCaptureDevice *camera,
                                              AVCaptureDeviceFormat *format, CMTime duration)
{
	NSError *error = nil;
	if (![camera lockForConfiguration:&error])
	{
		WLog_ERR(TAG, "Unable to configure camera: %s", error.localizedDescription.UTF8String);
		return FALSE;
	}
	camera.activeFormat = format;
	camera.activeVideoMinFrameDuration = duration;
	camera.activeVideoMaxFrameDuration = duration;
	[camera unlockForConfiguration];
	return TRUE;
}

@interface FreeRDPCameraAVFoundationStream : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
  @public
	CameraDevice *_device;
	dispatch_queue_t _sampleQueue;
	dispatch_queue_t _controlQueue;
	size_t _streamIndex;
	ICamHalSampleCapturedCallback _callback;
	CFTypeRef _transfer; /* Pixel transfer is available on iOS 16 and later. */
	CVPixelBufferRef _rawImage;
	wStream *_rawSample;
	UINT32 _width;
	UINT32 _height;
	BOOL _captureLogged;
	BOOL _sampleLogged;
	atomic_bool _drained;
}
@property(nonatomic, retain) AVCaptureSession *session;
@property(nonatomic, retain) AVCaptureVideoDataOutput *output;
- (void)stop;
@end

static void cam_avfoundation_deliver(FreeRDPCameraAVFoundationStream *stream, const BYTE *data,
                                     size_t length)
{
	WINPR_ASSERT(stream);
	WINPR_ASSERT(data);
	if (!stream->_sampleLogged)
	{
		WLog_INFO(TAG, "First camera sample: format %u, %" PRIu32 "x%" PRIu32 ", %zu bytes",
		          CAM_AVFOUNDATION_MEDIA_FORMAT, stream->_width, stream->_height, length);
		stream->_sampleLogged = TRUE;
	}
	const UINT rc = stream->_callback(stream->_device, stream->_streamIndex, data, length);
	if (rc != CHANNEL_RC_OK)
	{
		atomic_store(&stream->_drained, true);
		if (rc != ECAM_SAMPLE_CAPTURE_DRAINED)
			WLog_WARN(TAG, "Sample callback failed: %" PRIu32, rc);
	}
}

WINPR_ATTR_NODISCARD
static CAM_ERROR_CODE cam_avfoundation_stream_start(FreeRDPCameraAVFoundationStream *stream,
                                                    AVCaptureDevice *camera,
                                                    const CAM_MEDIA_TYPE_DESCRIPTION *mediaType,
                                                    AVCaptureDeviceFormat *selected,
                                                    CMTime duration)
{
	WINPR_ASSERT(stream);
	WINPR_ASSERT(camera);
	WINPR_ASSERT(mediaType);
	WINPR_ASSERT(selected);
	/* Suppress startup samples until the cached native mode is applied. The protocol
	 * stream remains active during an idle capture restart. */
	atomic_init(&stream->_drained, true);
	stream->_width = mediaType->Width;
	stream->_height = mediaType->Height;
	WLog_INFO(TAG,
	          "Starting camera %s: format %u, %" PRIu32 "x%" PRIu32 ", %" PRIu32 "/%" PRIu32 " fps",
	          camera.localizedName.UTF8String, CAM_AVFOUNDATION_MEDIA_FORMAT, stream->_width,
	          stream->_height, mediaType->FrameRateNumerator, mediaType->FrameRateDenominator);
	stream->_sampleQueue =
	    dispatch_queue_create("org.freerdp.camera.samples", DISPATCH_QUEUE_SERIAL);
	stream->_controlQueue =
	    dispatch_queue_create("org.freerdp.camera.control", DISPATCH_QUEUE_SERIAL);
	stream.session = [[[AVCaptureSession alloc] init] autorelease];
	stream.output = [[[AVCaptureVideoDataOutput alloc] init] autorelease];
	if (!stream->_sampleQueue || !stream->_controlQueue || !stream.session || !stream.output)
		return CAM_ERROR_CODE_OutOfMemory;

	NSError *error = nil;
	AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
	if (!input)
	{
		WLog_ERR(TAG, "Unable to open camera: %s", error.localizedDescription.UTF8String);
		return CAM_ERROR_CODE_UnexpectedError;
	}
	[stream.session beginConfiguration];
#if !TARGET_OS_OSX
	stream.session.sessionPreset = AVCaptureSessionPresetInputPriority;
#endif
	if (![stream.session canAddInput:input])
	{
		[stream.session commitConfiguration];
		return CAM_ERROR_CODE_OperationNotSupported;
	}
	[stream.session addInput:input];
	if (![stream.session canAddOutput:stream.output])
	{
		[stream.session commitConfiguration];
		return CAM_ERROR_CODE_OperationNotSupported;
	}
	[stream.session addOutput:stream.output];
	const OSType pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	if (![stream.output.availableVideoCVPixelFormatTypes containsObject:@(pixelFormat)])
	{
		[stream.session commitConfiguration];
		return CAM_ERROR_CODE_InvalidMediaType;
	}
	stream.output.videoSettings = @{(NSString *)kCVPixelBufferPixelFormatTypeKey: @(pixelFormat)};
	stream.output.alwaysDiscardsLateVideoFrames = YES;
#if !TARGET_OS_OSX
	if (!cam_avfoundation_configure_camera(camera, selected, duration))
	{
		[stream.session commitConfiguration];
		return CAM_ERROR_CODE_UnexpectedError;
	}
#endif
	[stream.session commitConfiguration];

	stream->_rawSample = Stream_New(NULL, (size_t)stream->_width * stream->_height * 3 / 2);
	if (!stream->_rawSample)
		return CAM_ERROR_CODE_OutOfMemory;
	OSStatus status = CVPixelBufferCreate(NULL, stream->_width, stream->_height, pixelFormat, NULL,
	                                      &stream->_rawImage);
	if (status == noErr)
	{
		if (@available(iOS 16.0, *))
		{
			status = cam_avfoundation_pixel_transfer_create(&stream->_transfer);
		}
	}
	if (status != noErr)
	{
		WLog_ERR(TAG, "Unable to create NV12 output buffer: %d", (int)status);
		return CAM_ERROR_CODE_UnexpectedError;
	}
	[stream.output setSampleBufferDelegate:stream queue:stream->_sampleQueue];
	/* startRunning/stopRunning are blocking and must stay off the application's main queue. */
	dispatch_sync(stream->_controlQueue, ^{
		@autoreleasepool
		{
			[stream.session startRunning];
		}
	});
#if TARGET_OS_OSX
	/* macOS has no InputPriority preset and startRunning replaces the device's
	 * format/rate with the session preset. Apply the cached native selection after
	 * startup, before enabling sample delivery. */
	if (stream.session.running && !cam_avfoundation_configure_camera(camera, selected, duration))
		return CAM_ERROR_CODE_UnexpectedError;
#endif
	if (!stream.session.running)
		return CAM_ERROR_CODE_UnexpectedError;
	atomic_store(&stream->_drained, false);
	return CAM_ERROR_CODE_None;
}

@implementation FreeRDPCameraAVFoundationStream
- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sample
           fromConnection:(AVCaptureConnection *)connection
{
	@autoreleasepool
	{
		WINPR_ASSERT(output);
		WINPR_ASSERT(sample);
		WINPR_ASSERT(connection);
		/* StartStream returns before RDPECAM marks the stream active. */
		if (atomic_load(&_drained) || !_device->streams[_streamIndex].streaming)
			return;
		CVImageBufferRef image = CMSampleBufferGetImageBuffer(sample);
		if (!image)
			return;
		if (!_captureLogged)
		{
			WLog_INFO(TAG, "Camera capture input: %zux%zu, pixel format 0x%08" PRIx32,
			          CVPixelBufferGetWidth(image), CVPixelBufferGetHeight(image),
			          CVPixelBufferGetPixelFormatType(image));
			_captureLogged = TRUE;
		}
		CVPixelBufferRef raw = image;
		if ((CVPixelBufferGetWidth(image) != _width) || (CVPixelBufferGetHeight(image) != _height))
		{
			OSStatus status = kVTParameterErr;
			if (@available(iOS 16.0, *))
			{
				if (_transfer)
					status = VTPixelTransferSessionTransferImage(
					    (VTPixelTransferSessionRef)_transfer, image, _rawImage);
			}
			if (status != noErr)
			{
				WLog_WARN(TAG, "Unable to resize NV12 camera sample: %d", (int)status);
				return;
			}
			raw = _rawImage;
		}
		if (!cam_avfoundation_pixel_buffer_to_nv12(raw, _rawSample))
		{
			WLog_WARN(TAG, "Unable to pack native NV12 camera sample");
			return;
		}
		cam_avfoundation_deliver(self, Stream_Buffer(_rawSample), Stream_Length(_rawSample));
		if (atomic_load(&_drained))
		{
			AVCaptureSession *session = self.session;
			dispatch_async(_controlQueue, ^{
				@autoreleasepool
				{
					[session stopRunning];
				}
			});
		}
	}
}

- (void)stop
{
	[self.output setSampleBufferDelegate:nil queue:NULL];
	AVCaptureSession *session = self.session;
	if (_controlQueue)
		dispatch_sync(_controlQueue, ^{
			@autoreleasepool
			{
				[session stopRunning];
			}
		});
	/* Drain the delegate before freeing buffers; do not capture self during deallocation. */
	if (_sampleQueue)
		dispatch_sync(_sampleQueue, ^{
		              });
	if (_transfer)
	{
		if (@available(iOS 16.0, *))
			VTPixelTransferSessionInvalidate((VTPixelTransferSessionRef)_transfer);
		CFRelease(_transfer);
		_transfer = NULL;
	}
	if (_rawImage)
	{
		CFRelease(_rawImage);
		_rawImage = NULL;
	}
	Stream_Free(_rawSample, TRUE);
	_rawSample = NULL;
	_device = NULL;
}

- (void)dealloc
{
	[self stop];
	[_session release];
	[_output release];
	if (_sampleQueue)
		dispatch_release(_sampleQueue);
	if (_controlQueue)
		dispatch_release(_controlQueue);
	[super dealloc];
}
@end

@interface FreeRDPCameraAVFoundation : NSObject
@property(nonatomic, retain) NSMutableDictionary<NSString *, AVCaptureDevice *> *cameras;
@property(nonatomic, retain)
    NSMutableDictionary<NSString *, FreeRDPCameraAVFoundationStream *> *streams;
@property(nonatomic, retain)
    NSMutableDictionary<NSString *, FreeRDPCameraAVFoundationModes *> *modes;
@end
@implementation FreeRDPCameraAVFoundation
- (void)dealloc
{
	[_cameras release];
	[_streams release];
	[_modes release];
	[super dealloc];
}
@end

typedef struct
{
	ICamHal iface;
	void *context; /* Owned by the HAL until Free. */
} CamAVFoundationHal;

WINPR_ATTR_NODISCARD
static FreeRDPCameraAVFoundation *cam_avfoundation_context(ICamHal *hal)
{
	WINPR_ASSERT(hal);
	return (FreeRDPCameraAVFoundation *)((CamAVFoundationHal *)hal)->context;
}

WINPR_ATTR_NODISCARD
static UINT cam_avfoundation_enumerate(ICamHal *hal, ICamHalEnumCallback callback,
                                       CameraPlugin *ecam, GENERIC_CHANNEL_CALLBACK *channel)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(callback);
	WINPR_ASSERT(ecam);
	WINPR_ASSERT(channel);
	@autoreleasepool
	{
		/* Request permission when the server first enumerates cameras. */
		if (!cam_avfoundation_request_access())
			return 0;
		NSMutableArray<AVCaptureDeviceType> *types =
		    [NSMutableArray arrayWithObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
#if TARGET_OS_OSX
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 140000
		[types addObject:AVCaptureDeviceTypeExternal];
#else
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
		if (@available(macOS 14.0, *))
			[types addObject:AVCaptureDeviceTypeExternal];
		else
#endif
			[types addObject:cam_avfoundation_legacy_external_type()];
#endif
#else
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 170000
		if (@available(iOS 17.0, *))
			[types addObject:AVCaptureDeviceTypeExternal];
#endif
#endif
		AVCaptureDeviceDiscoverySession *discovery = [AVCaptureDeviceDiscoverySession
		    discoverySessionWithDeviceTypes:types
		                          mediaType:AVMediaTypeVideo
		                           position:AVCaptureDevicePositionUnspecified];
		FreeRDPCameraAVFoundation *context = cam_avfoundation_context(hal);
		UINT count = 0;
		for (AVCaptureDevice *camera in discovery.devices)
		{
			NSString *deviceId = nil;
			@synchronized(context)
			{
				for (NSString *key in context.cameras)
				{
					if ([context.cameras[key].uniqueID isEqualToString:camera.uniqueID])
					{
						deviceId = key;
						break;
					}
				}
				/* Apple's uniqueID can exceed the channel's 32-byte device ID buffer. */
				if (!deviceId)
					deviceId = [NSString
					    stringWithFormat:@"avfoundation-%lu", (unsigned long)context.cameras.count];
				context.cameras[deviceId] = camera;
				/* A new device enumeration refreshes the native mode snapshot. */
				[context.modes removeObjectForKey:deviceId];
			}
			if (callback(ecam, channel, deviceId.UTF8String, camera.localizedName.UTF8String) !=
			    CHANNEL_RC_OK)
				break;
			count++;
		}
		return count;
	}
}

WINPR_ATTR_NODISCARD
static BOOL cam_avfoundation_activate(ICamHal *hal, const char *deviceId, CAM_ERROR_CODE *error)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(error);
	@autoreleasepool
	{
		const BOOL found = cam_avfoundation_context(hal).cameras[@(deviceId)] != nil;
		*error = found ? CAM_ERROR_CODE_None : CAM_ERROR_CODE_ItemNotFound;
		return found;
	}
}

WINPR_ATTR_NODISCARD
static INT16 cam_avfoundation_media_types(ICamHal *hal, const char *deviceId, size_t streamIndex,
                                          const CAM_MEDIA_FORMAT_INFO *formats, size_t formatCount,
                                          CAM_MEDIA_TYPE_DESCRIPTION *mediaTypes,
                                          size_t *mediaCount)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(formats);
	WINPR_ASSERT(mediaTypes);
	WINPR_ASSERT(mediaCount);
	@autoreleasepool
	{
		const size_t capacity = *mediaCount;
		*mediaCount = 0;
		if (streamIndex != 0)
			return -1;
		INT16 selected = -1;
		/* Follow the shared format preference: encode NV12 as H.264 when offered,
		 * otherwise use NV12 passthrough. Encoder selection stays in the shared codec. */
		for (size_t i = 0; i < formatCount; i++)
		{
			if ((formats[i].inputFormat == CAM_AVFOUNDATION_MEDIA_FORMAT) &&
			    ((formats[i].outputFormat == CAM_MEDIA_FORMAT_H264) ||
			     (formats[i].outputFormat == CAM_AVFOUNDATION_MEDIA_FORMAT)))
			{
				selected = (INT16)i;
				break;
			}
		}
		if (selected < 0)
			return -1;
		FreeRDPCameraAVFoundation *context = cam_avfoundation_context(hal);
		FreeRDPCameraAVFoundationModes *modes = nil;
		NSString *key = @(deviceId);
		@synchronized(context)
		{
			modes = context.modes[key];
			AVCaptureDevice *camera = context.cameras[key];
			if (!modes && camera)
			{
				modes = [[[FreeRDPCameraAVFoundationModes alloc] initWithFormats:camera.formats]
				    autorelease];
				if (modes)
					context.modes[key] = modes;
			}
			/* Discovery may replace this snapshot after we release the lock. */
			modes = [[modes retain] autorelease];
		}
		if (!modes)
			return -1;
		*mediaCount = [modes copyMediaTypes:mediaTypes capacity:capacity];
		return selected;
	}
}

WINPR_ATTR_NODISCARD
static CAM_ERROR_CODE cam_avfoundation_stop(ICamHal *hal, const char *deviceId, size_t streamIndex)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	@autoreleasepool
	{
		if (streamIndex != 0)
			return CAM_ERROR_CODE_InvalidStreamNumber;
		FreeRDPCameraAVFoundation *context = cam_avfoundation_context(hal);
		NSString *key = @(deviceId);
		[context.streams[key] stop];
		[context.streams removeObjectForKey:key];
		return CAM_ERROR_CODE_None;
	}
}

WINPR_ATTR_NODISCARD
static BOOL cam_avfoundation_deactivate(ICamHal *hal, const char *deviceId, CAM_ERROR_CODE *error)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(deviceId);
	WINPR_ASSERT(error);
	*error = cam_avfoundation_stop(hal, deviceId, 0);
	return *error == CAM_ERROR_CODE_None;
}

WINPR_ATTR_NODISCARD
static CAM_ERROR_CODE cam_avfoundation_start(ICamHal *hal, CameraDevice *device, size_t streamIndex,
                                             const CAM_MEDIA_TYPE_DESCRIPTION *mediaType,
                                             ICamHalSampleCapturedCallback callback)
{
	WINPR_ASSERT(hal);
	WINPR_ASSERT(device);
	WINPR_ASSERT(mediaType);
	WINPR_ASSERT(callback);
	@autoreleasepool
	{
		if (streamIndex != 0)
			return CAM_ERROR_CODE_InvalidStreamNumber;
		if ((mediaType->Format != CAM_AVFOUNDATION_MEDIA_FORMAT) ||
		    !mediaType->FrameRateNumerator || !mediaType->FrameRateDenominator)
			return CAM_ERROR_CODE_InvalidMediaType;
		if (!cam_avfoundation_request_access())
			return CAM_ERROR_CODE_OperationNotSupported;
		FreeRDPCameraAVFoundation *context = cam_avfoundation_context(hal);
		NSString *key = @(device->deviceId);
		AVCaptureDevice *camera = nil;
		FreeRDPCameraAVFoundationModes *modes = nil;
		/* Hold the matching camera/snapshot through startup and the surrounding HAL pool. */
		@synchronized(context)
		{
			camera = [[context.cameras[key] retain] autorelease];
			modes = [[context.modes[key] retain] autorelease];
		}
		if (!camera || !camera.connected)
			return CAM_ERROR_CODE_ItemNotFound;
		CMTime duration = kCMTimeInvalid;
		AVCaptureDeviceFormat *selected = [modes formatForMediaType:mediaType
		                                              frameDuration:&duration];
		if (!selected)
			return CAM_ERROR_CODE_InvalidMediaType;
		/* Restart after request-driven idle release; drain the old delegate before replacing it. */
		const CAM_ERROR_CODE stopError = cam_avfoundation_stop(hal, device->deviceId, streamIndex);
		if (stopError != CAM_ERROR_CODE_None)
			return stopError;
		FreeRDPCameraAVFoundationStream *stream = [[FreeRDPCameraAVFoundationStream alloc] init];
		if (!stream)
			return CAM_ERROR_CODE_OutOfMemory;
		stream->_device = device;
		stream->_streamIndex = streamIndex;
		stream->_callback = callback;
		const CAM_ERROR_CODE error =
		    cam_avfoundation_stream_start(stream, camera, mediaType, selected, duration);
		if (error != CAM_ERROR_CODE_None)
		{
			[stream release];
			return error;
		}
		context.streams[key] = stream;
		[stream release];
		return CAM_ERROR_CODE_None;
	}
}

WINPR_ATTR_NODISCARD
static CAM_ERROR_CODE cam_avfoundation_free(ICamHal *hal)
{
	WINPR_ASSERT(hal);
	@autoreleasepool
	{
		CamAVFoundationHal *av = (CamAVFoundationHal *)hal;
		FreeRDPCameraAVFoundation *context = (FreeRDPCameraAVFoundation *)av->context;
		for (FreeRDPCameraAVFoundationStream *stream in context.streams.allValues)
			[stream stop];
		av->context = NULL;
		[context release];
		free(av);
		return CAM_ERROR_CODE_None;
	}
}

WINPR_ATTR_NODISCARD
FREERDP_ENTRY_POINT(UINT VCAPITYPE avfoundation_freerdp_rdpecam_client_subsystem_entry(
    PFREERDP_CAMERA_HAL_ENTRY_POINTS entryPoints))
{
	@autoreleasepool
	{
		WINPR_ASSERT(entryPoints);
		WINPR_ASSERT(entryPoints->plugin);
		WINPR_ASSERT(entryPoints->pRegisterCameraHal);
		CamAVFoundationHal *hal = calloc(1, sizeof(CamAVFoundationHal));
		if (!hal)
			return CHANNEL_RC_NO_MEMORY;
		FreeRDPCameraAVFoundation *context = [[FreeRDPCameraAVFoundation alloc] init];
		if (!context)
		{
			free(hal);
			return CHANNEL_RC_NO_MEMORY;
		}
		context.cameras = [NSMutableDictionary dictionary];
		context.streams = [NSMutableDictionary dictionary];
		context.modes = [NSMutableDictionary dictionary];
		hal->context = context;
		hal->iface.RequestDrivenCapture = TRUE;
		hal->iface.Enumerate = cam_avfoundation_enumerate;
		hal->iface.Activate = cam_avfoundation_activate;
		hal->iface.Deactivate = cam_avfoundation_deactivate;
		hal->iface.GetMediaTypeDescriptions = cam_avfoundation_media_types;
		hal->iface.StartStream = cam_avfoundation_start;
		hal->iface.StopStream = cam_avfoundation_stop;
		hal->iface.Free = cam_avfoundation_free;
		const UINT rc = entryPoints->pRegisterCameraHal(entryPoints->plugin, &hal->iface);
		if (rc != CHANNEL_RC_OK)
		{
			if (cam_avfoundation_free(&hal->iface) != CAM_ERROR_CODE_None)
				WLog_ERR(TAG, "Unable to release unregistered camera HAL");
		}
		return rc;
	}
}
