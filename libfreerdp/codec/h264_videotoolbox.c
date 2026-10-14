/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Apple VideoToolbox H.264 decoder
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

#include <freerdp/config.h>

#include <stdlib.h>
#include <string.h>

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include <winpr/assert.h>
#include <freerdp/log.h>

#include "h264.h"

typedef struct
{
	BYTE* data;
	size_t size;
	UINT32 spsId;
} H264_VT_PARAMETER_SET;

typedef struct
{
	VTDecompressionSessionRef session;
	CMVideoFormatDescriptionRef format;
	CVPixelBufferRef image;
	BOOL imageLocked;
	BOOL parametersChanged;
	BOOL needsIdr;
	UINT32 ppsId;
	OSType pixelFormat;
	H264_VT_PARAMETER_SET sps[32];
	H264_VT_PARAMETER_SET pps[256];
} H264_CONTEXT_VIDEOTOOLBOX;

typedef struct
{
	const BYTE* data;
	size_t size;
	size_t offset;
	UINT32 bits;
	UINT32 byte;
	UINT32 zeros;
} H264_VT_BITS;

typedef struct
{
	CVPixelBufferRef image;
	OSStatus status;
	UINT32 count;
} H264_VT_OUTPUT;

/* Read only the RBSP identifiers needed to select the matching SPS/PPS. VideoToolbox
 * parses the complete parameter sets and slices. Keep emulation-prevention handling
 * here as the identifiers are Exp-Golomb coded, not necessarily byte aligned. */
WINPR_ATTR_NODISCARD
static BOOL videotoolbox_read_bit(H264_VT_BITS* bits, UINT32* value)
{
	WINPR_ASSERT(bits);
	WINPR_ASSERT(value);
	WINPR_ASSERT(bits->data);

	if (bits->bits == 0)
	{
		if (bits->offset >= bits->size)
			return FALSE;
		UINT32 byte = bits->data[bits->offset++];
		if ((bits->zeros == 2) && (byte == 3))
		{
			if ((bits->offset >= bits->size) || (bits->data[bits->offset] > 3))
				return FALSE;
			byte = bits->data[bits->offset++];
			bits->zeros = 0;
		}
		bits->zeros = (byte == 0) ? MIN(bits->zeros + 1, 2) : 0;
		bits->byte = byte;
		bits->bits = 8;
	}
	*value = (bits->byte >> --bits->bits) & 1;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_read_ue(H264_VT_BITS* bits, UINT32* value)
{
	WINPR_ASSERT(bits);
	WINPR_ASSERT(value);

	UINT32 bit = 0;
	UINT32 zeros = 0;
	while (TRUE)
	{
		if (!videotoolbox_read_bit(bits, &bit))
			return FALSE;
		if (bit != 0)
			break;
		if (++zeros > 31)
			return FALSE;
	}
	UINT32 result = 1;
	for (UINT32 x = 0; x < zeros; x++)
	{
		if (!videotoolbox_read_bit(bits, &bit))
			return FALSE;
		result = (result << 1) | bit;
	}
	*value = result - 1;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static size_t videotoolbox_start_code(const BYTE* data, size_t size, size_t* offset)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(offset);

	for (; (*offset < size) && ((size - *offset) >= 3); (*offset)++)
	{
		const BYTE* p = &data[*offset];
		if ((p[0] != 0) || (p[1] != 0))
			continue;
		if (p[2] == 1)
			return 3;
		if (((size - *offset) >= 4) && (p[2] == 0) && (p[3] == 1))
			return 4;
	}
	*offset = size;
	return 0;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_next_nal(const BYTE* data, size_t size, size_t* offset, const BYTE** nal,
                                  size_t* length)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(offset);
	WINPR_ASSERT(nal);
	WINPR_ASSERT(length);

	const size_t previous = *offset;
	const size_t prefix = videotoolbox_start_code(data, size, offset);
	if (prefix == 0)
		return FALSE;
	/* Annex B permits leading_zero_8bits, but not arbitrary leading bytes. */
	for (size_t x = previous; x < *offset; x++)
	{
		if (data[x] != 0)
			return FALSE;
	}
	*offset += prefix;
	const size_t start = *offset;
	(void)videotoolbox_start_code(data, size, offset);
	size_t end = *offset;
	while ((end > start) && (data[end - 1] == 0))
		end--;
	if ((end == start) || ((data[start] & 0x80) != 0))
		return FALSE;
	*nal = &data[start];
	*length = end - start;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_save_parameter(H264_CONTEXT_VIDEOTOOLBOX* sys, const BYTE* nal,
                                        size_t length)
{
	WINPR_ASSERT(sys);
	WINPR_ASSERT(nal);

	H264_VT_BITS bits = { .data = nal + 1, .size = length - 1 };
	UINT32 id = 0;
	UINT32 spsId = 0;
	H264_VT_PARAMETER_SET* parameter = nullptr;
	if ((nal[0] & 0x1F) == 7)
	{
		/* profile_idc, constraint flags and level_idc precede seq_parameter_set_id. */
		for (UINT32 x = 0; x < 24; x++)
		{
			if (!videotoolbox_read_bit(&bits, &id))
				return FALSE;
		}
		if (!videotoolbox_read_ue(&bits, &id) || (id >= ARRAYSIZE(sys->sps)))
			return FALSE;
		parameter = &sys->sps[id];
	}
	else
	{
		if (!videotoolbox_read_ue(&bits, &id) || (id >= ARRAYSIZE(sys->pps)) ||
		    !videotoolbox_read_ue(&bits, &spsId) || (spsId >= ARRAYSIZE(sys->sps)))
			return FALSE;
		parameter = &sys->pps[id];
	}
	if ((parameter->size == length) && (memcmp(parameter->data, nal, length) == 0))
		return TRUE;
	BYTE* copy = malloc(length);
	if (!copy)
		return FALSE;
	memcpy(copy, nal, length);
	free(parameter->data);
	parameter->data = copy;
	parameter->size = length;
	parameter->spsId = spsId;
	sys->parametersChanged = TRUE;
	return TRUE;
}

static void videotoolbox_release_image(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->image)
	{
		if (sys->imageLocked)
			CVPixelBufferUnlockBaseAddress(sys->image, kCVPixelBufferLock_ReadOnly);
		CVPixelBufferRelease(sys->image);
		sys->image = nullptr;
	}
	sys->imageLocked = FALSE;
	for (size_t x = 0; x < 3; x++)
	{
		h264->pYUVData[x] = nullptr;
		h264->iStride[x] = 0;
	}
	h264->YUVWidth = 0;
	h264->YUVHeight = 0;
}

static void videotoolbox_destroy_session(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->session)
	{
		VTDecompressionSessionInvalidate(sys->session);
		CFRelease(sys->session);
		sys->session = nullptr;
	}
	if (sys->format)
	{
		CFRelease(sys->format);
		sys->format = nullptr;
	}
	sys->needsIdr = TRUE;
	h264->hwAccel = FALSE;
}

static void videotoolbox_output(WINPR_ATTR_UNUSED void* context, void* sourceFrame, OSStatus status,
                                WINPR_ATTR_UNUSED VTDecodeInfoFlags flags, CVImageBufferRef image,
                                WINPR_ATTR_UNUSED CMTime pts, WINPR_ATTR_UNUSED CMTime duration)
{
	WINPR_ASSERT(sourceFrame);
	/* context is unused; image may be NULL on failure or when a frame is dropped. */
	H264_VT_OUTPUT* output = sourceFrame;
	output->count++;
	output->status = status;
	if (image)
	{
		if (output->image)
			CVPixelBufferRelease(output->image);
		output->image = CVPixelBufferRetain(image);
	}
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_ensure_session(H264_CONTEXT* h264, UINT32 ppsId)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->session && !sys->parametersChanged && (sys->ppsId == ppsId))
		return TRUE;
	const H264_VT_PARAMETER_SET* pps = &sys->pps[ppsId];
	const H264_VT_PARAMETER_SET* sps = &sys->sps[pps->spsId];
	if (!pps->data || !sps->data)
		return FALSE;
	const BYTE* parameters[] = { sps->data, pps->data };
	const size_t lengths[] = { sps->size, pps->size };
	CMVideoFormatDescriptionRef format = nullptr;
	OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
	    kCFAllocatorDefault, ARRAYSIZE(parameters), parameters, lengths, 4, &format);
	if (status != noErr)
	{
		WLog_Print(h264->log, WLOG_WARN, "VideoToolbox H.264 format creation failed: %d", status);
		return FALSE;
	}
	/* Preserve sample values, including auxiliary AVC444 chroma encoded in luma.
	 * Do not ask VideoToolbox to scale, convert to RGB, or change the video range. */
	const BOOL fullRange =
	    CMFormatDescriptionGetExtension(format, kCMFormatDescriptionExtension_FullRangeVideo) ==
	    kCFBooleanTrue;
	const OSType pixelFormat = fullRange ? kCVPixelFormatType_420YpCbCr8PlanarFullRange
	                                     : kCVPixelFormatType_420YpCbCr8Planar;
	if (sys->session && (sys->pixelFormat == pixelFormat) &&
	    VTDecompressionSessionCanAcceptFormatDescription(sys->session, format))
	{
		CFRelease(sys->format);
		sys->format = format;
		sys->parametersChanged = FALSE;
		sys->ppsId = ppsId;
		return TRUE;
	}

	videotoolbox_destroy_session(h264);
	CFNumberRef pixel = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixelFormat);
	if (!pixel)
	{
		CFRelease(format);
		return FALSE;
	}
	const void* keys[] = { kCVPixelBufferPixelFormatTypeKey };
	const void* values[] = { pixel };
	CFDictionaryRef attributes =
	    CFDictionaryCreate(kCFAllocatorDefault, keys, values, ARRAYSIZE(keys),
	                       &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFRelease(pixel);
	if (!attributes)
	{
		CFRelease(format);
		return FALSE;
	}
	const VTDecompressionOutputCallbackRecord callback = { videotoolbox_output, nullptr };
	/* A NULL decoder specification leaves hardware/software selection to Apple.
	 * SoftwareGdi and H264_CONTEXT_OPTION_HW_ACCEL deliberately do not control it. */
	status = VTDecompressionSessionCreate(kCFAllocatorDefault, format, nullptr, attributes,
	                                      &callback, &sys->session);
	CFRelease(attributes);
	if (status != noErr)
	{
		WLog_Print(h264->log, WLOG_WARN, "VideoToolbox H.264 session creation failed: %d", status);
		CFRelease(format);
		return FALSE;
	}
	sys->format = format;
	sys->pixelFormat = pixelFormat;
	sys->ppsId = ppsId;
	sys->parametersChanged = FALSE;
	/* RealTime is a scheduling hint, not permission to drop RDP subframes. */
	(void)VTSessionSetProperty(sys->session, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
	if (__builtin_available(macOS 10.9, iOS 17.0, tvOS 17.0, *))
	{
		CFTypeRef hardware = nullptr;
		status = VTSessionCopyProperty(
		    sys->session, kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder,
		    kCFAllocatorDefault, &hardware);
		if (status == noErr)
		{
			h264->hwAccel = (hardware == kCFBooleanTrue);
			WLog_Print(h264->log, WLOG_DEBUG, "VideoToolbox H.264 decoder: %s",
			           h264->hwAccel ? "hardware" : "software");
		}
		if (hardware)
			CFRelease(hardware);
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_map_image(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	CVPixelBufferRef image = sys->image;
	WINPR_ASSERT(image);

	const size_t width = CVPixelBufferGetWidth(image);
	const size_t height = CVPixelBufferGetHeight(image);
	if ((width == 0) || (height == 0) || (width > UINT32_MAX) || (height > UINT32_MAX) ||
	    (CVPixelBufferGetPixelFormatType(image) != sys->pixelFormat) ||
	    (CVPixelBufferGetPlaneCount(image) != 3))
		return FALSE;
	if (CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
		return FALSE;
	sys->imageLocked = TRUE;
	for (size_t x = 0; x < 3; x++)
	{
		const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(image, x);
		const size_t planeWidth = (x == 0) ? width : (width + 1) / 2;
		const size_t planeHeight = (x == 0) ? height : (height + 1) / 2;
		BYTE* data = CVPixelBufferGetBaseAddressOfPlane(image, x);
		if (!data || (stride < planeWidth) || (stride > UINT32_MAX) ||
		    (CVPixelBufferGetWidthOfPlane(image, x) < planeWidth) ||
		    (CVPixelBufferGetHeightOfPlane(image, x) < planeHeight))
			return FALSE;
		h264->pYUVData[x] = data;
		h264->iStride[x] = (UINT32)stride;
	}
	h264->YUVWidth = (UINT32)width;
	h264->YUVHeight = (UINT32)height;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static int videotoolbox_decompress(H264_CONTEXT* WINPR_RESTRICT h264,
                                   const BYTE* WINPR_RESTRICT data, UINT32 size)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(data || (size == 0));

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	CMBlockBufferRef block = nullptr;
	CMSampleBufferRef sample = nullptr;
	H264_VT_OUTPUT output = { 0 };
	int result = -1;
	size_t sampleSize = 0;
	size_t offset = 0;
	UINT32 ppsId = UINT32_MAX;
	BOOL idr = FALSE;
	videotoolbox_release_image(h264);
	if (size == 0)
		return 0;
	while (offset < size)
	{
		const BYTE* nal = nullptr;
		size_t length = 0;
		if (!videotoolbox_next_nal(data, size, &offset, &nal, &length))
			goto fail;
		const BYTE type = nal[0] & 0x1F;
		if ((type == 7) || (type == 8))
		{
			if (!videotoolbox_save_parameter(sys, nal, length))
				goto fail;
			continue;
		}
		if ((type == 1) || (type == 5))
		{
			H264_VT_BITS bits = { .data = nal + 1, .size = length - 1 };
			UINT32 value = 0;
			UINT32 slicePps = 0;
			if (!videotoolbox_read_ue(&bits, &value) || !videotoolbox_read_ue(&bits, &value) ||
			    !videotoolbox_read_ue(&bits, &slicePps) || (slicePps >= ARRAYSIZE(sys->pps)))
				goto fail;
			if ((ppsId != UINT32_MAX) && ((ppsId != slicePps) || (idr != (type == 5))))
				goto fail;
			ppsId = slicePps;
			idr = (type == 5);
		}
		else if ((type == 9) || (type == 10) || (type == 11) || (type == 12))
			continue;
		else if (type != 6) /* Only ordinary AVC slices and SEI are submitted. */
			goto fail;
		if ((length > UINT32_MAX - 4) || (sampleSize > UINT32_MAX - length - 4))
			goto fail;
		sampleSize += length + 4;
	}
	/* Parameter sets may arrive separately from the first picture (MS-RDPEVOR). */
	if (ppsId == UINT32_MAX)
		return 0;
	if (!videotoolbox_ensure_session(h264, ppsId))
		goto fail;
	if (sys->needsIdr && !idr)
	{
		WLog_Print(h264->log, WLOG_WARN, "VideoToolbox H.264 decoder requires an IDR picture");
		goto fail;
	}
	OSStatus status = CMBlockBufferCreateWithMemoryBlock(
	    kCFAllocatorDefault, nullptr, sampleSize, kCFAllocatorDefault, nullptr, 0, sampleSize,
	    kCMBlockBufferAssureMemoryNowFlag, &block);
	if (status != noErr)
		goto fail;
	char* bytes = nullptr;
	status = CMBlockBufferGetDataPointer(block, 0, nullptr, nullptr, &bytes);
	if ((status != noErr) || !bytes)
		goto fail;
	offset = 0;
	size_t written = 0;
	while (offset < size)
	{
		const BYTE* nal = nullptr;
		size_t length = 0;
		if (!videotoolbox_next_nal(data, size, &offset, &nal, &length))
			goto fail;
		const BYTE type = nal[0] & 0x1F;
		if ((type != 1) && (type != 5) && (type != 6))
			continue;
		bytes[written++] = (char)((length >> 24) & 0xFF);
		bytes[written++] = (char)((length >> 16) & 0xFF);
		bytes[written++] = (char)((length >> 8) & 0xFF);
		bytes[written++] = (char)(length & 0xFF);
		memcpy(&bytes[written], nal, length);
		written += length;
	}
	WINPR_ASSERT(written == sampleSize);
	status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, sys->format, 1, 0, nullptr, 1,
	                                   &sampleSize, &sample);
	if (status != noErr)
		goto fail;
	/* Neither asynchronous nor temporal processing is enabled: the callback completes
	 * before this returns. AVC444 feeds main/auxiliary pictures through the SAME session,
	 * and FreeRDP must consume each output before submitting the following subframe. */
	status = VTDecompressionSessionDecodeFrame(sys->session, sample, 0, &output, nullptr);
	if ((status != noErr) || (output.status != noErr) || (output.count != 1) || !output.image)
	{
		WLog_Print(h264->log, WLOG_WARN,
		           "VideoToolbox H.264 decode failed: %d / %d, outputs=%" PRIu32, status,
		           output.status, output.count);
		goto fail;
	}
	sys->image = output.image;
	output.image = nullptr;
	if (!videotoolbox_map_image(h264))
		goto fail;
	sys->needsIdr = FALSE;
	result = 1;
fail:
	if (output.image)
		CVPixelBufferRelease(output.image);
	if (sample)
		CFRelease(sample);
	if (block)
		CFRelease(block);
	if (result < 0)
	{
		/* A skipped/dropped reference picture makes subsequent P pictures unsafe. Keep
		 * cached parameter sets, but require an IDR rather than displaying stale pixels. */
		videotoolbox_release_image(h264);
		videotoolbox_destroy_session(h264);
	}
	return result;
}

static void videotoolbox_uninit(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX* sys = h264->pSystemData;
	if (!sys)
		return;
	videotoolbox_release_image(h264);
	videotoolbox_destroy_session(h264);
	for (size_t x = 0; x < ARRAYSIZE(sys->sps); x++)
		free(sys->sps[x].data);
	for (size_t x = 0; x < ARRAYSIZE(sys->pps); x++)
		free(sys->pps[x].data);
	free(sys);
	h264->pSystemData = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_init(H264_CONTEXT* h264)
{
	WINPR_ASSERT(h264);

	/* Let registration continue to an encoder-capable backend for compressor contexts. */
	if (h264->Compressor)
		return FALSE;
	H264_CONTEXT_VIDEOTOOLBOX* sys = calloc(1, sizeof(*sys));
	if (!sys)
		return FALSE;
	sys->needsIdr = TRUE;
	h264->pSystemData = sys;
	h264->hwAccel = FALSE;
	/* The actual session is created lazily when the stream supplies SPS/PPS. */
	return TRUE;
}

const H264_CONTEXT_SUBSYSTEM g_Subsystem_VideoToolbox = { "VideoToolbox", videotoolbox_init,
	                                                      videotoolbox_uninit,
	                                                      videotoolbox_decompress, nullptr };
