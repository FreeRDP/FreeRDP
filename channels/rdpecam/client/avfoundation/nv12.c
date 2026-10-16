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
#include "capture_policy.h"
#include <winpr/assert.h>
#include <VideoToolbox/VideoToolbox.h>

OSStatus cam_avfoundation_pixel_transfer_create(CFTypeRef* transfer)
{
	WINPR_ASSERT(transfer);
	*transfer = NULL;
	VTPixelTransferSessionRef session = NULL;
	OSStatus status = VTPixelTransferSessionCreate(NULL, &session);
	if (status != noErr)
		return status;
	const int one = 1;
	CFNumberRef spacing = CFNumberCreate(NULL, kCFNumberIntType, &one);
	CFDictionaryRef aspect = NULL;
	if (spacing)
	{
		const void* keys[] = { kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
			                   kCVImageBufferPixelAspectRatioVerticalSpacingKey };
		const void* values[] = { spacing, spacing };
		aspect = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
		                            &kCFTypeDictionaryValueCallBacks);
		CFRelease(spacing);
	}
	if (!aspect)
		status = kVTAllocationFailedErr;
	else
	{
		/* RDPECAM advertises square pixels and transmits no CoreVideo attachments.
		 * Fit the complete camera view without relying on pixel aspect metadata. */
		status = VTSessionSetProperty(session, kVTPixelTransferPropertyKey_ScalingMode,
		                              kVTScalingMode_Letterbox);
		if (status == noErr)
			status = VTSessionSetProperty(
			    session, kVTPixelTransferPropertyKey_DestinationPixelAspectRatio, aspect);
		CFRelease(aspect);
	}
	if (status != noErr)
	{
		CFRelease(session);
		return status;
	}
	*transfer = session;
	return status;
}

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
	if (!width || !height || (width % 2) || (height % 2) || (width > CAM_AVFOUNDATION_MAX_WIDTH) ||
	    (height > CAM_AVFOUNDATION_MAX_HEIGHT))
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
