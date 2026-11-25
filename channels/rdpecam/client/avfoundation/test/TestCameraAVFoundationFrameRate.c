/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation frame-rate negotiation tests.
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

#include <stdio.h>
#include "../frame_rate.h"

WINPR_ATTR_NODISCARD
int TestCameraAVFoundationFrameRate(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	const CMTime faceTime30 = CMTimeMake(1000000, 30000030);
	const CMTime faceTime15 = CMTimeMake(1000000, 15000015);
	const CMTime ntsc = CMTimeMake(1001, 30000);
	const struct
	{
		const char* name;
		UINT32 numerator;
		UINT32 denominator;
		CMTime minimum;
		CMTime maximum;
		BOOL accepted;
		CMTime expected;
	} tests[] = {
		{ "FaceTime 30 fps", 30, 1, faceTime30, faceTime30, TRUE, faceTime30 },
		{ "FaceTime 15 fps", 15, 1, faceTime15, faceTime15, TRUE, faceTime15 },
		{ "nanosecond camera clock", 30, 1, CMTimeMake(33333333, 1000000000),
		  CMTimeMake(33333333, 1000000000), TRUE, CMTimeMake(33333333, 1000000000) },
		{ "rounded lower rate", 30, 1, CMTimeMake(1000000, 29999970), CMTimeMake(1000000, 29999970),
		  TRUE, CMTimeMake(1000000, 29999970) },
		{ "continuous range", 30, 1, CMTimeMake(1, 60), CMTimeMake(1, 15), TRUE,
		  CMTimeMake(1, 30) },
		{ "fractional rate", 30000, 1001, CMTimeMake(1, 60), CMTimeMake(1, 15), TRUE, ntsc },
		{ "distinct NTSC rate", 30, 1, ntsc, ntsc, FALSE, kCMTimeInvalid },
		{ "unsupported rate", 60, 1, faceTime30, faceTime30, FALSE, kCMTimeInvalid },
		{ "zero numerator", 0, 1, faceTime30, faceTime30, FALSE, kCMTimeInvalid },
		{ "zero denominator", 30, 0, faceTime30, faceTime30, FALSE, kCMTimeInvalid },
		{ "overflowing timescale", UINT32_MAX, 1, faceTime30, faceTime30, FALSE, kCMTimeInvalid },
		{ "invalid range", 30, 1, kCMTimeInvalid, faceTime30, FALSE, kCMTimeInvalid },
		{ "infinite range", 30, 1, faceTime30, kCMTimePositiveInfinity, FALSE, kCMTimeInvalid },
		{ "zero duration", 30, 1, kCMTimeZero, faceTime30, FALSE, kCMTimeInvalid },
		{ "reversed range", 30, 1, CMTimeMake(1, 15), CMTimeMake(1, 60), FALSE, kCMTimeInvalid },
	};
	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		CMTime actual = kCMTimeInvalid;
		const BOOL accepted = cam_avfoundation_frame_duration(
		    tests[i].numerator, tests[i].denominator, tests[i].minimum, tests[i].maximum, &actual);
		if ((accepted != tests[i].accepted) ||
		    (accepted && (CMTimeCompare(actual, tests[i].expected) != 0)))
		{
			fprintf(stderr, "Frame duration case failed: %s\n", tests[i].name);
			return -1;
		}
		if (!accepted)
			continue;

		/* Advertised fractional rates must describe the same supported native duration. */
		UINT32 numerator = 0;
		UINT32 denominator = 0;
		CMTime selected = kCMTimeInvalid;
		if (!cam_avfoundation_frame_rate(actual, &numerator, &denominator) ||
		    !cam_avfoundation_frame_duration(numerator, denominator, tests[i].minimum,
		                                     tests[i].maximum, &selected) ||
		    (CMTimeCompare(selected, actual) != 0))
		{
			fprintf(stderr, "Frame rate round trip failed: %s\n", tests[i].name);
			return -1;
		}
	}

	UINT32 numerator = 0;
	UINT32 denominator = 0;
	if (!cam_avfoundation_frame_rate(faceTime30, &numerator, &denominator) ||
	    (numerator != 3000003) || (denominator != 100000) ||
	    cam_avfoundation_frame_rate(kCMTimeInvalid, &numerator, &denominator) ||
	    cam_avfoundation_frame_rate(CMTimeMake(INT64_MAX, 1), &numerator, &denominator))
	{
		fprintf(stderr, "Native frame rate serialization failed\n");
		return -1;
	}
	return 0;
}
