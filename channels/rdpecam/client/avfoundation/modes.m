/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation native mode cache.
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

#include "modes.h"
#include "capture_policy.h"
#include "frame_rate.h"

@interface FreeRDPCameraAVFoundationMode : NSObject
@property(nonatomic) CAM_MEDIA_TYPE_DESCRIPTION mediaType;
@property(nonatomic, retain) AVCaptureDeviceFormat *format;
@property(nonatomic) CMTime frameDuration;
@end

@implementation FreeRDPCameraAVFoundationMode
- (void)dealloc
{
	[_format release];
	[super dealloc];
}
@end

/* Do not hash the C struct's padding. Flags describe the shared encoder's output,
 * so they do not participate in selecting the native NV12 capture mode. */
static NSString *cam_avfoundation_mode_key(const CAM_MEDIA_TYPE_DESCRIPTION *type)
{
	WINPR_ASSERT(type);
	return [NSString stringWithFormat:@"%u:%u:%u:%u:%u:%u:%u", type->Format, type->Width,
	                                  type->Height, type->FrameRateNumerator,
	                                  type->FrameRateDenominator, type->PixelAspectRatioNumerator,
	                                  type->PixelAspectRatioDenominator];
}

@implementation FreeRDPCameraAVFoundationModes
{
	/* Preserve enumeration order independently of the descriptor hash table. */
	NSArray<FreeRDPCameraAVFoundationMode *> *_entries;
	NSDictionary<NSString *, FreeRDPCameraAVFoundationMode *> *_lookup;
}

- (instancetype)initWithFormats:(NSArray<AVCaptureDeviceFormat *> *)formats
{
	self = [super init];
	if (!self)
		return nil;
	NSMutableArray<FreeRDPCameraAVFoundationMode *> *entries = [NSMutableArray array];
	NSMutableDictionary<NSString *, FreeRDPCameraAVFoundationMode *> *lookup =
	    [NSMutableDictionary dictionary];
	for (AVCaptureDeviceFormat *format in formats)
	{
		const CMVideoDimensions size =
		    CMVideoFormatDescriptionGetDimensions(format.formatDescription);
		if ((size.width < CAM_AVFOUNDATION_MIN_WIDTH) ||
		    (size.height < CAM_AVFOUNDATION_MIN_HEIGHT) ||
		    (size.width > CAM_AVFOUNDATION_MAX_WIDTH) ||
		    (size.height > CAM_AVFOUNDATION_MAX_HEIGHT) || (size.width % 2) || (size.height % 2))
			continue;
		const UINT32 rates[] = CAM_AVFOUNDATION_FRAME_RATES;
		for (size_t r = 0; r < ARRAYSIZE(rates); r++)
		{
			CMTime duration = kCMTimeInvalid;
			for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges)
			{
				if (cam_avfoundation_frame_duration(rates[r], 1, range.minFrameDuration,
				                                    range.maxFrameDuration, &duration))
					break;
			}
			CAM_MEDIA_TYPE_DESCRIPTION type = {
				CAM_MEDIA_FORMAT_NV12, (UINT32)size.width, (UINT32)size.height, 0, 0, 1, 1, 0
			};
			if (!cam_avfoundation_frame_rate(duration, &type.FrameRateNumerator,
			                                 &type.FrameRateDenominator))
				continue;
			NSString *key = cam_avfoundation_mode_key(&type);
			if (lookup[key] || (entries.count >= ECAM_MAX_MEDIA_TYPE_DESCRIPTORS))
				continue;
			FreeRDPCameraAVFoundationMode *entry = [[FreeRDPCameraAVFoundationMode alloc] init];
			if (!entry)
			{
				[self release];
				return nil;
			}
			entry.mediaType = type;
			entry.format = format;
			/* Keep the native clock representation, not a duration rebuilt from the wire rate. */
			entry.frameDuration = duration;
			[entries addObject:entry];
			lookup[key] = entry;
			[entry release];
		}
	}
	_entries = [entries copy];
	_lookup = [lookup copy];
	if (!_entries || !_lookup)
	{
		[self release];
		return nil;
	}
	return self;
}

- (size_t)copyMediaTypes:(CAM_MEDIA_TYPE_DESCRIPTION *)mediaTypes capacity:(size_t)capacity
{
	WINPR_ASSERT(mediaTypes);
	size_t count = 0;
	for (FreeRDPCameraAVFoundationMode *entry in _entries)
	{
		if (count >= capacity)
			break;
		mediaTypes[count++] = entry.mediaType;
	}
	return count;
}

- (AVCaptureDeviceFormat *)formatForMediaType:(const CAM_MEDIA_TYPE_DESCRIPTION *)mediaType
                                frameDuration:(CMTime *)frameDuration
{
	WINPR_ASSERT(frameDuration);
	FreeRDPCameraAVFoundationMode *entry = _lookup[cam_avfoundation_mode_key(mediaType)];
	*frameDuration = entry ? entry.frameDuration : kCMTimeInvalid;
	return entry.format;
}

- (void)dealloc
{
	[_entries release];
	[_lookup release];
	[super dealloc];
}
@end
