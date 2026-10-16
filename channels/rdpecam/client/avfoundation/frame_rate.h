/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation frame-rate negotiation.
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

#ifndef FREERDP_RDPECAM_AVFOUNDATION_FRAME_RATE_H
#define FREERDP_RDPECAM_AVFOUNDATION_FRAME_RATE_H

#include <math.h>
#include <CoreMedia/CoreMedia.h>
#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include "capture_policy.h"

WINPR_ATTR_NODISCARD
static inline BOOL cam_avfoundation_frame_duration(UINT32 numerator, UINT32 denominator,
                                                   CMTime minimum, CMTime maximum, CMTime* duration)
{
	WINPR_ASSERT(duration);
	if (!numerator || (numerator > INT32_MAX) || !denominator || !CMTIME_IS_NUMERIC(minimum) ||
	    !CMTIME_IS_NUMERIC(maximum) || (minimum.value <= 0) || (maximum.value <= 0) ||
	    (minimum.timescale <= 0) || (maximum.timescale <= 0) || minimum.epoch || maximum.epoch ||
	    (CMTimeCompare(minimum, maximum) > 0))
		return FALSE;
	const CMTime requested = CMTimeMake(denominator, (CMTimeScale)numerator);
	CMTime supported = requested;
	if (CMTimeCompare(requested, minimum) < 0)
		supported = minimum;
	else if (CMTimeCompare(requested, maximum) > 0)
		supported = maximum;

	/* Some drivers report nominal 30/15 fps as 30.000030/15.000015 fps.
	 * Clamp small endpoint rounding to a native duration using the capture policy's
	 * tolerance, without merging distinct rates such as 29.97 and 30. */
	const double seconds = CMTimeGetSeconds(requested);
	if (fabs(CMTimeGetSeconds(supported) - seconds) >
	    seconds * CAM_AVFOUNDATION_FRAME_DURATION_TOLERANCE)
		return FALSE;
	*duration = supported;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static inline BOOL cam_avfoundation_frame_rate(CMTime duration, UINT32* numerator,
                                               UINT32* denominator)
{
	WINPR_ASSERT(numerator);
	WINPR_ASSERT(denominator);
	if (!CMTIME_IS_NUMERIC(duration) || (duration.value <= 0) || (duration.timescale <= 0) ||
	    duration.epoch)
		return FALSE;
	/* Reduce the inverse duration to fit the wire fields without rounding the native rate. */
	UINT64 num = (UINT32)duration.timescale;
	UINT64 den = (UINT64)duration.value;
	UINT64 a = num;
	UINT64 b = den;
	while (b)
	{
		const UINT64 remainder = a % b;
		a = b;
		b = remainder;
	}
	num /= a;
	den /= a;
	if ((num > UINT32_MAX) || (den > UINT32_MAX))
		return FALSE;
	*numerator = (UINT32)num;
	*denominator = (UINT32)den;
	return TRUE;
}

#endif
