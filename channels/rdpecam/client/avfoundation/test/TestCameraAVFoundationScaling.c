/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM AVFoundation aspect ratio and orientation tests.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <VideoToolbox/VideoToolbox.h>
#include "../nv12.h"

WINPR_ATTR_NODISCARD
static BOOL test_scaling(size_t width, size_t height, size_t outWidth, size_t outHeight,
                         int pixelAspect)
{
	BOOL result = FALSE;
	CVPixelBufferRef source = NULL;
	CVPixelBufferRef destination = NULL;
	CFTypeRef transfer = NULL;
	wStream* packed = Stream_New(NULL, 1);
	const OSType format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	if (!packed ||
	    (CVPixelBufferCreate(NULL, width, height, format, NULL, &source) != kCVReturnSuccess) ||
	    (CVPixelBufferCreate(NULL, outWidth, outHeight, format, NULL, &destination) !=
	     kCVReturnSuccess))
		goto out;
	CFNumberRef horizontal = CFNumberCreate(NULL, kCFNumberIntType, &pixelAspect);
	const int one = 1;
	CFNumberRef vertical = CFNumberCreate(NULL, kCFNumberIntType, &one);
	const void* keys[] = { kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
		                   kCVImageBufferPixelAspectRatioVerticalSpacingKey };
	const void* values[] = { horizontal, vertical };
	CFDictionaryRef aspect = CFDictionaryCreate(
	    NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CVBufferSetAttachment(source, kCVImageBufferPixelAspectRatioKey, aspect,
	                      kCVAttachmentMode_ShouldPropagate);
	CFRelease(aspect);
	CFRelease(horizontal);
	CFRelease(vertical);
	if (CVPixelBufferLockBaseAddress(source, 0) != kCVReturnSuccess)
		goto out;
	BYTE* luma = CVPixelBufferGetBaseAddressOfPlane(source, 0);
	const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(source, 0);
	for (size_t y = 0; y < height; y++)
	{
		memset(luma + y * stride, 64, width);
		for (size_t x = 0; x < width; x++)
		{
			const double dx = ((double)x + 0.5 - (double)width / 2) * pixelAspect;
			const double dy = (double)y + 0.5 - (double)height / 2;
			/* A physical circle must remain circular after scaling. */
			if (dx * dx + dy * dy < 24 * 24)
				luma[y * stride + x] = 224;
			/* Distinct corner markers detect any horizontal or vertical flip. */
			if ((y >= height / 6 - 12) && (y < height / 6 + 12))
			{
				if ((x >= width / 12) && (x < width / 12 + width / 4))
					luma[y * stride + x] = 112;
				if ((x >= width - width / 12 - width / 4) && (x < width - width / 12))
					luma[y * stride + x] = 176;
			}
		}
	}
	BYTE* chroma = CVPixelBufferGetBaseAddressOfPlane(source, 1);
	const size_t chromaStride = CVPixelBufferGetBytesPerRowOfPlane(source, 1);
	for (size_t y = 0; y < height / 2; y++)
		memset(chroma + y * chromaStride, 128, width);
	CVPixelBufferUnlockBaseAddress(source, 0);
	if ((cam_avfoundation_pixel_transfer_create(&transfer) != noErr) ||
	    (VTPixelTransferSessionTransferImage((VTPixelTransferSessionRef)transfer, source,
	                                         destination) != noErr) ||
	    !cam_avfoundation_pixel_buffer_to_nv12(destination, packed) ||
	    (Stream_Length(packed) != outWidth * outHeight * 3 / 2))
		goto out;
	const BYTE* data = Stream_Buffer(packed);
	size_t minX = outWidth;
	size_t minY = outHeight;
	size_t maxX = 0;
	size_t maxY = 0;
	for (size_t y = 0; y < outHeight; y++)
	{
		for (size_t x = 0; x < outWidth; x++)
		{
			if (data[y * outWidth + x] <= 200)
				continue;
			if (x < minX)
				minX = x;
			if (x > maxX)
				maxX = x;
			if (y < minY)
				minY = y;
			if (y > maxY)
				maxY = y;
		}
	}
	if ((minX >= outWidth) || (minY >= outHeight) ||
	    (abs((int)(maxX - minX) - (int)(maxY - minY)) > 2))
	{
		fprintf(stderr, "Circle distorted: %zux%zu -> %zux%zu, PAR %d:1, bounds %zu,%zu-%zu,%zu\n",
		        width, height, outWidth, outHeight, pixelAspect, minX, minY, maxX, maxY);
		goto out;
	}
	const double scale =
	    fmin((double)outWidth / ((double)width * pixelAspect), (double)outHeight / height);
	const double paddingX = ((double)outWidth - width * pixelAspect * scale) / 2;
	const double paddingY = ((double)outHeight - height * scale) / 2;
	const size_t markerY = (size_t)(paddingY + (height / 6) * scale);
	const size_t left = (size_t)(paddingX + (width / 12 + width / 8) * pixelAspect * scale);
	const size_t right =
	    (size_t)(paddingX + (width - width / 12 - width / 8) * pixelAspect * scale);
	const int leftValue = data[markerY * outWidth + left];
	const int rightValue = data[markerY * outWidth + right];
	if ((abs(leftValue - 112) > 16) || (abs(rightValue - 176) > 16))
	{
		fprintf(
		    stderr,
		    "Corner markers moved or flipped: %zux%zu -> %zux%zu, PAR %d:1, left=%d, right=%d\n",
		    width, height, outWidth, outHeight, pixelAspect, leftValue, rightValue);
		goto out;
	}
	if (((paddingX > 1) || (paddingY > 1)) && (data[0] > 24))
	{
		fprintf(stderr, "Letterbox padding is not black\n");
		goto out;
	}
	result = TRUE;
out:
	if (transfer)
		CFRelease(transfer);
	if (source)
		CFRelease(source);
	if (destination)
		CFRelease(destination);
	Stream_Free(packed, TRUE);
	return result;
}

int TestCameraAVFoundationScaling(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	return (test_scaling(192, 108, 64, 48, 1) && test_scaling(108, 192, 64, 48, 1) &&
	        test_scaling(192, 108, 64, 36, 1) && test_scaling(192, 108, 64, 48, 2))
	           ? 0
	           : -1;
}
