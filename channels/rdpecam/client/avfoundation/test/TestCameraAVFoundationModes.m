/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation native mode cache tests.
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

#include "../modes.h"

/* These stand-ins provide the read-only format/range selectors without opening
 * a camera. Native AVCaptureDeviceFormat objects have no public initializer. */
@interface TestAVFoundationRange : NSObject
@property(nonatomic) CMTime minFrameDuration;
@property(nonatomic) CMTime maxFrameDuration;
@end
@implementation TestAVFoundationRange
@end

static unsigned liveFormats = 0;

@interface TestAVFoundationFormat : NSObject
@property(nonatomic) CMVideoFormatDescriptionRef formatDescription;
@property(nonatomic, copy) NSArray *videoSupportedFrameRateRanges;
@end
@implementation TestAVFoundationFormat
- (instancetype)init
{
	self = [super init];
	if (self)
		liveFormats++;
	return self;
}
- (void)dealloc
{
	liveFormats--;
	if (_formatDescription)
		CFRelease(_formatDescription);
	[_videoSupportedFrameRateRanges release];
	[super dealloc];
}
@end

static TestAVFoundationRange *test_range(CMTime duration)
{
	TestAVFoundationRange *range = [[[TestAVFoundationRange alloc] init] autorelease];
	range.minFrameDuration = duration;
	range.maxFrameDuration = duration;
	return range;
}

static AVCaptureDeviceFormat *test_format(int width, int height, NSArray *ranges)
{
	TestAVFoundationFormat *format = [[[TestAVFoundationFormat alloc] init] autorelease];
	CMVideoFormatDescriptionRef description = NULL;
	if (CMVideoFormatDescriptionCreate(NULL, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, width,
	                                   height, NULL, &description) != noErr)
		return nil;
	format.formatDescription = description;
	format.videoSupportedFrameRateRanges = ranges;
	return (AVCaptureDeviceFormat *)format;
}

static BOOL test_modes(void)
{
	BOOL success = FALSE;
	FreeRDPCameraAVFoundationModes *modes = nil;
	AVCaptureDeviceFormat *original = nil;
	const CMTime native30 = CMTimeMake(1000000, 30000030);
	const CMTime native15 = CMTimeMake(1000000, 15000015);
	const CMTime unusual30 = CMTimeMake(33333333, 1000000000);
	const CMTime nearby30 = CMTimeMake(1000000, 30000060);
	CAM_MEDIA_TYPE_DESCRIPTION types[8] = { 0 };
	size_t count = 0;
	@autoreleasepool
	{
		NSArray *rates = @[test_range(native30), test_range(native15)];
		original = test_format(640, 480, rates);
		AVCaptureDeviceFormat *duplicate = test_format(640, 480, rates);
		AVCaptureDeviceFormat *unusual = test_format(854, 480, @[test_range(unusual30)]);
		AVCaptureDeviceFormat *nearby = test_format(640, 480, @[test_range(nearby30)]);
		AVCaptureDeviceFormat *odd = test_format(641, 480, rates);
		AVCaptureDeviceFormat *oversized = test_format(3840, 2160, rates);
		if (!original || !duplicate || !unusual || !nearby || !odd || !oversized)
			return FALSE;
		NSMutableArray *formats =
		    [NSMutableArray arrayWithArray:@[original, duplicate, unusual, nearby, odd, oversized]];
		modes = [[FreeRDPCameraAVFoundationModes alloc] initWithFormats:formats];
		if (!modes)
			return FALSE;
		count = [modes copyMediaTypes:types capacity:ARRAYSIZE(types)];
		/* Reordering/removing the source formats must not change an advertised selection. */
		[formats removeObjectAtIndex:0];
		[(TestAVFoundationFormat *)original setVideoSupportedFrameRateRanges:@[]];
	}
	/* Only formats referenced by advertised entries should survive the source pool. */
	if ((count != 4) || (liveFormats != 3) || (types[0].Width != 640) || (types[0].Height != 480) ||
	    (types[0].FrameRateNumerator != 3000003) || (types[0].FrameRateDenominator != 100000) ||
	    (types[1].FrameRateDenominator != 200000) || (types[2].Width != 854) ||
	    (types[2].FrameRateNumerator != 1000000000) ||
	    (types[2].FrameRateDenominator != 33333333) || (types[3].FrameRateNumerator != 1500003) ||
	    (types[3].FrameRateDenominator != 50000))
	{
		fprintf(stderr, "Native mode enumeration, deduplication or retention failed\n");
		goto out;
	}
	const CMTime durations[] = { native30, native15, unusual30, nearby30 };
	for (size_t i = 0; i < count; i++)
	{
		CMTime actual = kCMTimeInvalid;
		AVCaptureDeviceFormat *format = [modes formatForMediaType:&types[i] frameDuration:&actual];
		if (!format || ((i < 2) && (format != original)) || (actual.value != durations[i].value) ||
		    (actual.timescale != durations[i].timescale) || (actual.flags != durations[i].flags) ||
		    (actual.epoch != durations[i].epoch))
		{
			fprintf(stderr, "Cached native format/duration changed for mode %zu\n", i);
			goto out;
		}
	}
	/* Repeated enumeration and a capacity-limited copy preserve the snapshot. */
	CAM_MEDIA_TYPE_DESCRIPTION again[8] = { 0 };
	if (([modes copyMediaTypes:again capacity:ARRAYSIZE(again)] != count) ||
	    (memcmp(types, again, count * sizeof(types[0])) != 0) ||
	    ([modes copyMediaTypes:again capacity:0] != 0) ||
	    ([modes copyMediaTypes:again capacity:1] != 1) ||
	    (memcmp(types, again, sizeof(types[0])) != 0))
	{
		fprintf(stderr, "Advertised mode snapshot changed\n");
		goto out;
	}
	CAM_MEDIA_TYPE_DESCRIPTION request = types[0];
	CMTime duration = kCMTimeInvalid;
	request.Flags = CAM_MEDIA_TYPE_DESCRIPTION_FLAG_DecodingRequired;
	if ([modes formatForMediaType:&request frameDuration:&duration] != original)
	{
		fprintf(stderr, "Shared encoder flags changed native mode lookup\n");
		goto out;
	}
	/* An unadvertised near-match must be rejected, never searched or approximated. */
	for (size_t i = 0; i < 5; i++)
	{
		request = types[0];
		switch (i)
		{
			case 0:
				request.FrameRateNumerator = 30;
				request.FrameRateDenominator = 1;
				break;
			case 1:
				request.Width += 2;
				break;
			case 2:
				request.Height += 2;
				break;
			case 3:
				request.PixelAspectRatioNumerator = 2;
				break;
			case 4:
				request.Format = CAM_MEDIA_FORMAT_H264;
				break;
		}
		if ([modes formatForMediaType:&request frameDuration:&duration] ||
		    CMTIME_IS_VALID(duration))
		{
			fprintf(stderr, "Unadvertised mode accepted in case %zu\n", i);
			goto out;
		}
	}
	success = TRUE;
out:
	[modes release];
	if (liveFormats != 0)
	{
		fprintf(stderr, "Cached native formats were not released\n");
		success = FALSE;
	}
	return success;
}

int main(int argc, char *argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	@autoreleasepool
	{
		return test_modes() ? 0 : -1;
	}
}
