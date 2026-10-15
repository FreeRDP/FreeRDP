/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation native NV12 samples.
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

#include "nv12.h"
#include <winpr/assert.h>

BOOL cam_avfoundation_pixel_buffer_to_nv12(CVPixelBufferRef image, wStream* output)
{
	WINPR_ASSERT(image);
	WINPR_ASSERT(output);
	if ((CVPixelBufferGetPixelFormatType(image) !=
	     kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) ||
	    (CVPixelBufferGetPlaneCount(image) != 2))
		return FALSE;
	const size_t width = CVPixelBufferGetWidth(image);
	const size_t height = CVPixelBufferGetHeight(image);
	if (!width || !height || (width % 2) || (height % 2) || (width > 1920) || (height > 1080))
		return FALSE;
	if ((CVPixelBufferGetHeightOfPlane(image, 0) != height) ||
	    (CVPixelBufferGetHeightOfPlane(image, 1) != height / 2) ||
	    (CVPixelBufferGetBytesPerRowOfPlane(image, 0) < width) ||
	    (CVPixelBufferGetBytesPerRowOfPlane(image, 1) < width))
		return FALSE;
	Stream_ResetPosition(output);
	if (!Stream_EnsureRemainingCapacity(output, width * height * 3 / 2) ||
	    (CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess))
		return FALSE;
	for (size_t plane = 0; plane < 2; plane++)
	{
		const BYTE* data = CVPixelBufferGetBaseAddressOfPlane(image, plane);
		const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(image, plane);
		if (!data)
		{
			CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
			return FALSE;
		}
		for (size_t row = 0; row < CVPixelBufferGetHeightOfPlane(image, plane); row++)
			Stream_Write(output, data + row * stride, width);
	}
	CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
	Stream_SealLength(output);
	return TRUE;
}
