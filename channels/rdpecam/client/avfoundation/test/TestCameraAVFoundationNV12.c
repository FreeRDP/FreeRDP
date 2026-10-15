/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM native NV12 packing and media flag tests.
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
#include <string.h>
#include <winpr/assert.h>
#include <freerdp/codec/color.h>
#include "../nv12.h"
#include "../../camera.h"
#include "../../../common/rdpecam-utils.h"

/* Exercise the same shared encoder used for NV12 capture when H.264 output is offered. */
WINPR_ATTR_NODISCARD
static BOOL test_h264_encoding(void)
{
	if (!freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT_NV12, FREERDP_VIDEO_FORMAT_H264))
		return TRUE;

	enum
	{
		width = 64,
		height = 48,
		stride = width * 4
	};
	BOOL result = FALSE;
	CameraDeviceStream stream = { 0 };
	wStream* sample = Stream_New(NULL, 1);
	H264_CONTEXT* decoder = h264_context_new(FALSE);
	BYTE raw[width * height * 3 / 2] = { 0 };
	BYTE rgb[stride * height] = { 0 };
	const RECTANGLE_16 region = { 0, 0, width, height };
	stream.formats.inputFormat = CAM_MEDIA_FORMAT_NV12;
	stream.formats.outputFormat = CAM_MEDIA_FORMAT_H264;
	stream.currMediaType.Width = width;
	stream.currMediaType.Height = height;
	stream.currMediaType.FrameRateNumerator = 30;
	stream.currMediaType.FrameRateDenominator = 1;
	if (!sample || !decoder || !ecam_encoder_context_init(&stream) ||
	    !h264_context_reset(decoder, width, height))
		goto out;

	memset(raw + width * height, 128, sizeof(raw) - width * height);
	for (size_t frame = 0; frame < 3; frame++)
	{
		memset(raw, (int)(96 + frame * 16), width * height);
		memset(rgb, 0xa5, sizeof(rgb));
		Stream_ResetPosition(sample);
		Stream_Write_UINT8(sample, 0x5a);
		if (!ecam_encoder_compress(&stream, raw, sizeof(raw), sample) ||
		    (Stream_GetPosition(sample) <= 1) || (Stream_Buffer(sample)[0] != 0x5a))
			goto out;
		const size_t size = Stream_GetPosition(sample) - 1;
		if ((size > UINT32_MAX) ||
		    (avc420_decompress(decoder, Stream_Buffer(sample) + 1, (UINT32)size, rgb,
		                       PIXEL_FORMAT_BGRA32, stride, width, height, &region, 1) < 0))
			goto out;
		/* The decoded neutral-gray frames must change with their source luminance. */
		const BYTE* pixel = rgb + (height / 2) * stride + (width / 2) * 4;
		const int expected = (int)((80 + frame * 16) * 255 / 219);
		for (size_t channel = 0; channel < 3; channel++)
		{
			const int difference = (int)pixel[channel] - expected;
			if ((difference < -8) || (difference > 8))
				goto out;
		}
	}
	result = TRUE;
out:
	if (!ecam_encoder_context_free(&stream))
		result = FALSE;
	h264_context_free(decoder);
	Stream_Free(sample, TRUE);
	if (!result)
		fprintf(stderr, "NV12 camera H.264 encoding or decoding failed\n");
	return result;
}

WINPR_ATTR_NODISCARD
int TestCameraAVFoundationNV12(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_ASSERT(argv);
	WINPR_UNUSED(argv);
	CVPixelBufferRef image = NULL;
	CameraDeviceStream stream = { 0 };
	wStream* sample = Stream_New(NULL, 1);
	int result = -1;
	if (!sample || CVPixelBufferCreate(NULL, 8, 4, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
	                                   NULL, &image) != kCVReturnSuccess)
		goto out;
	if (CVPixelBufferLockBaseAddress(image, 0) != kCVReturnSuccess)
		goto out;
	BYTE expected[48] = { 0 };
	size_t offset = 0;
	for (size_t plane = 0; plane < 2; plane++)
	{
		BYTE* data = CVPixelBufferGetBaseAddressOfPlane(image, plane);
		const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(image, plane);
		for (size_t row = 0; row < CVPixelBufferGetHeightOfPlane(image, plane); row++)
		{
			/* Distinct rows surrounded by padding that must not reach the wire. */
			memset(data + row * stride, 0xff, stride);
			memset(data + row * stride, (int)(plane * 16 + row), 8);
			memset(expected + offset, (int)(plane * 16 + row), 8);
			offset += 8;
		}
	}
	CVPixelBufferUnlockBaseAddress(image, 0);
	if (!cam_avfoundation_pixel_buffer_to_nv12(image, sample) ||
	    (Stream_Length(sample) != sizeof(expected)) ||
	    memcmp(Stream_Buffer(sample), expected, sizeof(expected)) != 0)
		goto out;
	/* Transport the packed bytes with no software encoder, including in a
	 * VideoToolbox decoder-only build. Preserve any existing packet header. */
	stream.formats.inputFormat = CAM_MEDIA_FORMAT_NV12;
	stream.formats.outputFormat = CAM_MEDIA_FORMAT_NV12;
	stream.currMediaType.Width = 8;
	stream.currMediaType.Height = 4;
	stream.currMediaType.FrameRateNumerator = 30;
	stream.currMediaType.FrameRateDenominator = 1;
	Stream_ResetPosition(sample);
	Stream_Write_UINT8(sample, 0x5a);
	if (!ecam_encoder_context_init(&stream) ||
	    !ecam_encoder_compress(&stream, expected, sizeof(expected), sample) ||
	    (Stream_GetPosition(sample) != sizeof(expected) + 1) ||
	    (Stream_Buffer(sample)[0] != 0x5a) ||
	    memcmp(Stream_Buffer(sample) + 1, expected, sizeof(expected)) != 0)
		goto out;
	/* Accept raw top-down samples without changing validation of existing single flags. */
	for (UINT8 flags = 0; flags < 3; flags++)
	{
		if (!rdpecam_valid_MediaTypeDescriptionFlags_(flags, WLog_Get("camera.test"), __FILE__,
		                                              __func__, __LINE__))
			goto out;
	}
	CVPixelBufferRef invalid = NULL;
	if (CVPixelBufferCreate(NULL, 8, 4, kCVPixelFormatType_32ARGB, NULL, &invalid) !=
	    kCVReturnSuccess)
		goto out;
	const BOOL accepted = cam_avfoundation_pixel_buffer_to_nv12(invalid, sample);
	CFRelease(invalid);
	if (rdpecam_valid_MediaTypeDescriptionFlags_(4, WLog_Get("camera.test"), __FILE__, __func__,
	                                             __LINE__) ||
	    accepted)
		goto out;
	if (!test_h264_encoding())
		goto out;
	result = 0;
out:
	if (!ecam_encoder_context_free(&stream))
		result = -1;
	if (image)
		CFRelease(image);
	Stream_Free(sample, TRUE);
	if (result != 0)
		fprintf(stderr, "NV12 camera sample packing or flags validation failed\n");
	return result;
}
