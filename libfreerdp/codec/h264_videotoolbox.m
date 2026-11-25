/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Apple VideoToolbox H.264 decoder and encoder
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
	BYTE *data;
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
	const BYTE *data;
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

WINPR_ATTR_MALLOC(free, 1)
static char *osstatus2str(OSStatus status)
{
	char *str = nullptr;
	size_t len = 0;
	(void)winpr_asprintf(&str, &len, "OSStatus %ld", status);
	return str;
}

/* Read only the RBSP identifiers needed to select the matching SPS/PPS. VideoToolbox
 * parses the complete parameter sets and slices. Keep emulation-prevention handling
 * here as the identifiers are Exp-Golomb coded, not necessarily byte aligned. */
WINPR_ATTR_NODISCARD
static BOOL videotoolbox_read_bit(H264_VT_BITS *bits, UINT32 *value)
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
static BOOL videotoolbox_read_ue(H264_VT_BITS *bits, UINT32 *value)
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
static size_t videotoolbox_start_code(const BYTE *data, size_t size, size_t *offset)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(offset);

	for (; (*offset < size) && ((size - *offset) >= 3); (*offset)++)
	{
		const BYTE *p = &data[*offset];
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
static BOOL videotoolbox_next_nal(const BYTE *data, size_t size, size_t *offset, const BYTE **nal,
                                  size_t *length)
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
static BOOL videotoolbox_save_parameter(H264_CONTEXT_VIDEOTOOLBOX *sys, const BYTE *nal,
                                        size_t length)
{
	WINPR_ASSERT(sys);
	WINPR_ASSERT(nal);

	H264_VT_BITS bits = { .data = nal + 1, .size = length - 1 };
	UINT32 id = 0;
	UINT32 spsId = 0;
	H264_VT_PARAMETER_SET *parameter = nullptr;
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
	BYTE *copy = malloc(length);
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

static void videotoolbox_release_image(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
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

static void videotoolbox_destroy_session(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
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

static void videotoolbox_output(WINPR_ATTR_UNUSED void *context, void *sourceFrame, OSStatus status,
                                WINPR_ATTR_UNUSED VTDecodeInfoFlags flags, CVImageBufferRef image,
                                WINPR_ATTR_UNUSED CMTime pts, WINPR_ATTR_UNUSED CMTime duration)
{
	WINPR_ASSERT(sourceFrame);
	/* context is unused; image may be NULL on failure or when a frame is dropped. */
	H264_VT_OUTPUT *output = sourceFrame;
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
static BOOL videotoolbox_ensure_session(H264_CONTEXT *h264, UINT32 ppsId)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
	WINPR_ASSERT(sys);

	if (sys->session && !sys->parametersChanged && (sys->ppsId == ppsId))
		return TRUE;
	const H264_VT_PARAMETER_SET *pps = &sys->pps[ppsId];
	const H264_VT_PARAMETER_SET *sps = &sys->sps[pps->spsId];
	if (!pps->data || !sps->data)
		return FALSE;
	const BYTE *parameters[] = { sps->data, pps->data };
	const size_t lengths[] = { sps->size, pps->size };
	CMVideoFormatDescriptionRef format = nullptr;
	OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
	    kCFAllocatorDefault, ARRAYSIZE(parameters), parameters, lengths, 4, &format);
	if (status != noErr)
	{
		char *str = osstatus2str(status);
		WLog_Print(h264->log, WLOG_WARN, "VideoToolbox H.264 format creation failed: %s", str);
		free(str);
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
	const void *keys[] = { kCVPixelBufferPixelFormatTypeKey };
	const void *values[] = { pixel };
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
		char *str = osstatus2str(status);
		WLog_Print(h264->log, WLOG_WARN, "VideoToolbox H.264 session creation failed: %s", str);
		free(str);
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
static BOOL videotoolbox_map_image(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
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
		BYTE *data = CVPixelBufferGetBaseAddressOfPlane(image, x);
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
static int videotoolbox_decompress(H264_CONTEXT *WINPR_RESTRICT h264,
                                   const BYTE *WINPR_RESTRICT data, UINT32 size)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(data || (size == 0));

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
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
		const BYTE *nal = nullptr;
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
	char *bytes = nullptr;
	status = CMBlockBufferGetDataPointer(block, 0, nullptr, nullptr, &bytes);
	if ((status != noErr) || !bytes)
		goto fail;
	offset = 0;
	size_t written = 0;
	while (offset < size)
	{
		const BYTE *nal = nullptr;
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
		char *str1 = osstatus2str(status);
		char *str2 = osstatus2str(output.status);
		WLog_Print(h264->log, WLOG_WARN,
		           "VideoToolbox H.264 decode failed: %s / %s, outputs=%" PRIu32, str1, str2,
		           output.count);
		free(str1);
		free(str2);
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

/* The encoder: VideoToolbox compresses NV12 into AVCC (length-prefixed NAL units, the
 * parameter sets kept in the format description); RDP's AVC420 carries an Annex B stream, so
 * the lengths become start codes and SPS/PPS go in front of every keyframe. */
typedef struct
{
	VTCompressionSessionRef session;
	UINT32 width;
	UINT32 height;
	UINT32 frameRate;
	UINT32 bitRate;
	UINT32 qp;
	H264_RATECONTROL_MODE rateControl;
	BOOL fullRange;
	INT64 frameIndex;
	BYTE *data;
	size_t size;
	size_t length;
	OSStatus status;
	BOOL failed;
	wLog *log;
} H264_CONTEXT_VT_ENCODER;

/* full range: FreeRDP's RGB to YUV conversion uses the whole 0..255 range
 * video range: FreeRDP's camera path supplies video-range YUV */
WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_full_range(const H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);
	switch (h264->UsageType)
	{
		case H264_CAMERA_VIDEO_REAL_TIME:
		case H264_CAMERA_VIDEO_NON_REAL_TIME:
			return FALSE;
		default:
			return TRUE;
	}
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_append(H264_CONTEXT_VT_ENCODER *enc, const void *data, size_t size)
{
	WINPR_ASSERT(enc);

	if (size > SIZE_MAX - enc->length)
		return FALSE;
	const size_t need = enc->length + size;
	if (need > enc->size)
	{
		size_t newSize = MAX(need, 4096);
		if (enc->size <= SIZE_MAX / 2)
			newSize = MAX(newSize, enc->size * 2);
		BYTE *tmp = realloc(enc->data, newSize);
		if (!tmp)
			return FALSE;
		enc->data = tmp;
		enc->size = newSize;
	}
	if (size > 0)
		memcpy(&enc->data[enc->length], data, size);
	enc->length = need;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_append_nal(H264_CONTEXT_VT_ENCODER *enc, const void *nal,
                                            size_t size)
{
	static const BYTE startCode[] = { 0x00, 0x00, 0x00, 0x01 };
	return videotoolbox_encoder_append(enc, startCode, sizeof(startCode)) &&
	       videotoolbox_encoder_append(enc, nal, size);
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_is_keyframe(CMSampleBufferRef sample)
{
	/* a sample without attachments is a sync sample */
	CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
	if (!attachments || (CFArrayGetCount(attachments) < 1))
		return TRUE;
	CFDictionaryRef dict = CFArrayGetValueAtIndex(attachments, 0);
	const void *notSync = nullptr;
	if (dict && CFDictionaryGetValueIfPresent(dict, kCMSampleAttachmentKey_NotSync, &notSync) &&
	    notSync && CFBooleanGetValue((CFBooleanRef)notSync))
		return FALSE;
	return TRUE;
}

static void videotoolbox_encoder_fail(H264_CONTEXT_VT_ENCODER *enc, OSStatus status,
                                      const char *what)
{
	WINPR_ASSERT(enc);
	enc->failed = TRUE;
	enc->status = status;
	char *str = osstatus2str(status);
	WLog_Print(enc->log, WLOG_ERROR, "VideoToolbox H.264 encoder: %s failed: %s", what, str);
	free(str);
}

static void videotoolbox_encoder_output(void *refcon, WINPR_ATTR_UNUSED void *frameRefcon,
                                        OSStatus status, VTEncodeInfoFlags flags,
                                        CMSampleBufferRef sample)
{
	H264_CONTEXT_VT_ENCODER *enc = refcon;
	WINPR_ASSERT(enc);

	if (status != noErr)
	{
		videotoolbox_encoder_fail(enc, status, "encoding a frame");
		return;
	}
	/* a dropped frame has no sample: nothing to send */
	if (!sample || ((flags & kVTEncodeInfo_FrameDropped) != 0))
		return;

	int nalHeaderLength = 4;
	CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);
	if (format)
	{
		size_t count = 0;
		OSStatus rc = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
		    format, 0, nullptr, nullptr, &count, &nalHeaderLength);
		if (rc != noErr)
		{
			videotoolbox_encoder_fail(enc, rc, "reading the parameter sets");
			return;
		}
		if (videotoolbox_encoder_is_keyframe(sample))
		{
			for (size_t x = 0; x < count; x++)
			{
				const uint8_t *set = nullptr;
				size_t setSize = 0;
				rc = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, x, &set, &setSize,
				                                                        nullptr, nullptr);
				if (rc != noErr)
				{
					videotoolbox_encoder_fail(enc, rc, "reading a parameter set");
					return;
				}
				if (!videotoolbox_encoder_append_nal(enc, set, setSize))
				{
					videotoolbox_encoder_fail(enc, kVTAllocationFailedErr,
					                          "copying a parameter set");
					return;
				}
			}
		}
	}
	if ((nalHeaderLength < 1) || (nalHeaderLength > 4))
	{
		videotoolbox_encoder_fail(enc, kVTParameterErr, "the NAL length size");
		return;
	}

	CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
	if (!block)
	{
		videotoolbox_encoder_fail(enc, kVTParameterErr, "getting the frame data");
		return;
	}

	size_t total = 0;
	size_t contiguous = 0;
	char *ptr = nullptr;
	BYTE *copy = nullptr;
	OSStatus rc = CMBlockBufferGetDataPointer(block, 0, &contiguous, &total, &ptr);
	if ((rc == noErr) && (contiguous < total))
	{
		/* not one piece: copy it */
		copy = malloc(total);
		if (!copy)
			rc = kVTAllocationFailedErr;
		else
			rc = CMBlockBufferCopyDataBytes(block, 0, total, copy);
		ptr = (char *)copy;
	}
	if (rc != noErr)
	{
		videotoolbox_encoder_fail(enc, rc, "reading the frame data");
		free(copy);
		return;
	}

	const BYTE *data = (const BYTE *)ptr;
	const size_t header = (size_t)nalHeaderLength;
	size_t offset = 0;
	while (total - offset > header)
	{
		size_t nalSize = 0;
		for (size_t x = 0; x < header; x++)
			nalSize = (nalSize << 8) | data[offset + x];
		offset += header;
		if (nalSize > total - offset)
		{
			videotoolbox_encoder_fail(enc, kVTParameterErr, "a NAL unit longer than the frame");
			break;
		}
		if (!videotoolbox_encoder_append_nal(enc, &data[offset], nalSize))
		{
			videotoolbox_encoder_fail(enc, kVTAllocationFailedErr, "copying a NAL unit");
			break;
		}
		offset += nalSize;
	}
	free(copy);
}

static void videotoolbox_encoder_destroy(H264_CONTEXT_VT_ENCODER *enc)
{
	WINPR_ASSERT(enc);

	if (enc->session)
	{
		(void)VTCompressionSessionCompleteFrames(enc->session, kCMTimeInvalid);
		VTCompressionSessionInvalidate(enc->session);
		CFRelease(enc->session);
		enc->session = nullptr;
	}
}

static BOOL videotoolbox_encoder_set(H264_CONTEXT_VT_ENCODER *enc, CFStringRef key, CFTypeRef value,
                                     const char *name)
{
	WINPR_ASSERT(enc);
	WINPR_ASSERT(name);

	if (!value)
		return FALSE;
	const OSStatus rc = VTSessionSetProperty(enc->session, key, value);
	if (rc != noErr)
	{
		char *str = osstatus2str(rc);
		WLog_Print(enc->log, WLOG_DEBUG, "VideoToolbox H.264 encoder: %s not set: %s", name, str);
		free(str);
		return FALSE;
	}
	return TRUE;
}

static BOOL videotoolbox_encoder_set_int(H264_CONTEXT_VT_ENCODER *enc, CFStringRef key,
                                         int32_t value, const char *name)
{
	CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
	const BOOL rc = videotoolbox_encoder_set(enc, key, number, name);
	if (number)
		CFRelease(number);
	return rc;
}

static BOOL videotoolbox_encoder_set_float(H264_CONTEXT_VT_ENCODER *enc, CFStringRef key,
                                           float value, const char *name)
{
	CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloat32Type, &value);
	const BOOL rc = videotoolbox_encoder_set(enc, key, number, name);
	if (number)
		CFRelease(number);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_set_qp(H264_CONTEXT_VT_ENCODER *enc, UINT32 qp)
{
	const int32_t value = (int32_t)MIN(qp, 51);
	if (__builtin_available(macOS 13.0, iOS 16.0, *))
	{
		/* a constant QP: the same lower and upper bound */
		if (videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_MinAllowedFrameQP, value,
		                                 "MinAllowedFrameQP") &&
		    videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_MaxAllowedFrameQP, value,
		                                 "MaxAllowedFrameQP"))
			return TRUE;
	}
	/* otherwise the 0..1 quality scale (1 best), from the QP scale (0..51, 0 best) */
	return videotoolbox_encoder_set_float(enc, kVTCompressionPropertyKey_Quality,
	                                      1.0f - (1.0f * (float)value / 51.0f), "Quality");
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_create(H264_CONTEXT *h264, H264_CONTEXT_VT_ENCODER *enc,
                                        UINT32 width, UINT32 height, UINT32 frameRate,
                                        UINT32 bitRate)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(enc);

	BOOL rc = FALSE;
	CFMutableDictionaryRef source = nullptr;
	CFMutableDictionaryRef surface = nullptr;
	CFNumberRef number = nullptr;

	videotoolbox_encoder_destroy(enc);
	if ((width > INT32_MAX) || (height > INT32_MAX) || (frameRate > INT32_MAX))
		return FALSE;

	source = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	                                   &kCFTypeDictionaryValueCallBacks);
	surface = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
	                                    &kCFTypeDictionaryValueCallBacks);
	const BOOL fullRange = videotoolbox_encoder_full_range(h264);
	const int32_t pixelFormat = fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
	                                      : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixelFormat);
	if (!source || !surface || !number)
		goto fail;
	CFDictionarySetValue(source, kCVPixelBufferPixelFormatTypeKey, number);
	/* IOSurface backed buffers go to the hardware encoder without a copy */
	CFDictionarySetValue(source, kCVPixelBufferIOSurfacePropertiesKey, surface);

	OSStatus status = VTCompressionSessionCreate(
	    kCFAllocatorDefault, (int32_t)width, (int32_t)height, kCMVideoCodecType_H264, nullptr,
	    source, kCFAllocatorDefault, videotoolbox_encoder_output, enc, &enc->session);
	if (status != noErr)
	{
		char *str = osstatus2str(status);
		WLog_Print(h264->log, WLOG_WARN,
		           "VideoToolbox H.264 encoder: VTCompressionSessionCreate %" PRIu32 "x%" PRIu32
		           " failed: %s",
		           width, height, str);
		free(str);
		enc->session = nullptr;
		goto fail;
	}

	/* every frame goes out at once: no frame reordering (B frames would hold frames back),
	 * real time scheduling */
	(void)videotoolbox_encoder_set(enc, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue,
	                               "RealTime");
	if (!videotoolbox_encoder_set(enc, kVTCompressionPropertyKey_AllowFrameReordering,
	                              kCFBooleanFalse, "AllowFrameReordering"))
		goto fail;
	/* Baseline, as OpenH264 sends: every RDP client decodes it */
	(void)videotoolbox_encoder_set(enc, kVTCompressionPropertyKey_ProfileLevel,
	                               kVTProfileLevel_H264_Baseline_AutoLevel, "ProfileLevel");
	(void)videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_MaxKeyFrameInterval, 30,
	                                   "MaxKeyFrameInterval");
	(void)videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_ExpectedFrameRate,
	                                   (int32_t)frameRate, "ExpectedFrameRate");

	switch (h264->RateControlMode)
	{
		case H264_RATECONTROL_CQP:
			if (!videotoolbox_encoder_set_qp(enc, h264->QP))
				goto fail;
			break;
		case H264_RATECONTROL_VBR:
		default:
			if (!videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_AverageBitRate,
			                                  (int32_t)MIN(bitRate, INT32_MAX), "AverageBitRate"))
				goto fail;
			break;
	}

	status = VTCompressionSessionPrepareToEncodeFrames(enc->session);
	if (status != noErr)
	{
		char *str = osstatus2str(status);
		WLog_Print(h264->log, WLOG_WARN,
		           "VideoToolbox H.264 encoder: VTCompressionSessionPrepareToEncodeFrames failed: "
		           "%s",
		           str);
		free(str);
		goto fail;
	}

	/* only logged: to yuv_ensure_buffer h264->hwAccel means NV12 YUV buffers, and this encoder
	 * takes the I420 buffers FreeRDP fills */
	BOOL hardware = FALSE;
	if (__builtin_available(macOS 10.9, iOS 17.4, tvOS 17.4, *))
	{
		CFTypeRef usingHardware = nullptr;
		if ((VTSessionCopyProperty(enc->session,
		                           kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
		                           kCFAllocatorDefault, (void *)&usingHardware) == noErr) &&
		    usingHardware)
		{
			hardware = (usingHardware == kCFBooleanTrue) ? TRUE : FALSE;
			CFRelease(usingHardware);
		}
	}

	enc->width = width;
	enc->height = height;
	enc->frameRate = frameRate;
	enc->bitRate = bitRate;
	enc->qp = h264->QP;
	enc->rateControl = h264->RateControlMode;
	enc->fullRange = fullRange;
	enc->frameIndex = 0;
	WLog_Print(
	    h264->log, WLOG_DEBUG,
	    "VideoToolbox H.264 encoder: %" PRIu32 "x%" PRIu32 " at %" PRIu32 " fps, %s, hardware %s",
	    width, height, frameRate,
	    (h264->RateControlMode == H264_RATECONTROL_CQP) ? "constant QP" : "variable bit rate",
	    hardware ? "yes" : "no");
	rc = TRUE;

fail:
	if (number)
		CFRelease(number);
	if (surface)
		CFRelease(surface);
	if (source)
		CFRelease(source);
	if (!rc)
		videotoolbox_encoder_destroy(enc);
	return rc;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_update(H264_CONTEXT *h264, H264_CONTEXT_VT_ENCODER *enc,
                                        UINT32 width, UINT32 height, UINT32 frameRate,
                                        UINT32 bitRate)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(enc);

	if (!enc->session || (enc->width != width) || (enc->height != height) ||
	    (enc->frameRate != frameRate) || (enc->rateControl != h264->RateControlMode) ||
	    (enc->fullRange != videotoolbox_encoder_full_range(h264)))
		return videotoolbox_encoder_create(h264, enc, width, height, frameRate, bitRate);

	/* bit rate and QP can change while the session runs; start over where they can't */
	if ((h264->RateControlMode != H264_RATECONTROL_CQP) && (enc->bitRate != bitRate))
	{
		if (!videotoolbox_encoder_set_int(enc, kVTCompressionPropertyKey_AverageBitRate,
		                                  (int32_t)MIN(bitRate, INT32_MAX), "AverageBitRate"))
			return videotoolbox_encoder_create(h264, enc, width, height, frameRate, bitRate);
		enc->bitRate = bitRate;
	}
	if ((h264->RateControlMode == H264_RATECONTROL_CQP) && (enc->qp != h264->QP))
	{
		if (!videotoolbox_encoder_set_qp(enc, h264->QP))
			return videotoolbox_encoder_create(h264, enc, width, height, frameRate, bitRate);
		enc->qp = h264->QP;
	}
	return TRUE;
}

/* I420 planes (FreeRDP's YUV buffers) into the NV12 picture the encoder takes */
WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_copy(CVPixelBufferRef image, const BYTE **WINPR_RESTRICT pSrcYuv,
                                      const UINT32 *WINPR_RESTRICT pStride, UINT32 width,
                                      UINT32 height)
{
	if (CVPixelBufferLockBaseAddress(image, 0) != kCVReturnSuccess)
		return FALSE;

	BOOL rc = FALSE;
	BYTE *luma = CVPixelBufferGetBaseAddressOfPlane(image, 0);
	BYTE *chroma = CVPixelBufferGetBaseAddressOfPlane(image, 1);
	const size_t lumaStride = CVPixelBufferGetBytesPerRowOfPlane(image, 0);
	const size_t chromaStride = CVPixelBufferGetBytesPerRowOfPlane(image, 1);
	if (!luma || !chroma || (lumaStride < width) || (chromaStride < width) ||
	    (CVPixelBufferGetHeightOfPlane(image, 0) < height) ||
	    (CVPixelBufferGetHeightOfPlane(image, 1) < height / 2))
		goto out;

	for (UINT32 y = 0; y < height; y++)
		memcpy(&luma[y * lumaStride], &pSrcYuv[0][1ull * y * pStride[0]], width);
	for (UINT32 y = 0; y < height / 2; y++)
	{
		const BYTE *u = &pSrcYuv[1][1ull * y * pStride[1]];
		const BYTE *v = &pSrcYuv[2][1ull * y * pStride[2]];
		BYTE *uv = &chroma[y * chromaStride];
		for (UINT32 x = 0; x < width / 2; x++)
		{
			uv[2 * x] = u[x];
			uv[2 * x + 1] = v[x];
		}
	}
	rc = TRUE;
out:
	(void)CVPixelBufferUnlockBaseAddress(image, 0);
	return rc;
}

WINPR_ATTR_NODISCARD
static int videotoolbox_compress(H264_CONTEXT *WINPR_RESTRICT h264,
                                 const BYTE **WINPR_RESTRICT pSrcYuv,
                                 const UINT32 *WINPR_RESTRICT pStride,
                                 BYTE **WINPR_RESTRICT ppDstData, UINT32 *WINPR_RESTRICT pDstSize)
{
	WINPR_ASSERT(h264);
	WINPR_ASSERT(pSrcYuv);
	WINPR_ASSERT(pStride);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	H264_CONTEXT_VT_ENCODER *enc = h264->pSystemData;
	if (!enc || !pSrcYuv[0] || !pSrcYuv[1] || !pSrcYuv[2])
		return -1;
	if ((h264->width == 0) || (h264->height == 0) || (h264->width > UINT16_MAX) ||
	    (h264->height > UINT16_MAX))
		return -1;

	/* 4:2:0 needs even sizes; the YUV buffers are padded (yuv_ensure_buffer), so one more
	 * column or row is readable */
	const UINT32 width = (h264->width + 1) & ~1u;
	const UINT32 height = (h264->height + 1) & ~1u;
	const UINT32 frameRate = (h264->FrameRate > 0) ? MIN(h264->FrameRate, 1000) : 30;
	const UINT32 bitRate = (h264->BitRate > 0) ? h264->BitRate : 1000000;

	if (!videotoolbox_encoder_update(h264, enc, width, height, frameRate, bitRate))
		return -1;

	CVPixelBufferRef image = nullptr;
	CVPixelBufferPoolRef pool = VTCompressionSessionGetPixelBufferPool(enc->session);
	const OSType pixelFormat = enc->fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
	                                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	const CVReturn cv = pool ? CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &image)
	                         : CVPixelBufferCreate(kCFAllocatorDefault, width, height, pixelFormat,
	                                               nullptr, &image);
	if ((cv != kCVReturnSuccess) || !image)
	{
		WLog_Print(h264->log, WLOG_ERROR, "VideoToolbox H.264 encoder: no picture buffer: %d",
		           (int)cv);
		return -1;
	}
	if (!videotoolbox_encoder_copy(image, pSrcYuv, pStride, width, height))
	{
		CVPixelBufferRelease(image);
		return -1;
	}

	enc->length = 0;
	enc->failed = FALSE;
	enc->status = noErr;
	const CMTime pts = CMTimeMake(enc->frameIndex++, (int32_t)frameRate);
	const CMTime duration = CMTimeMake(1, (int32_t)frameRate);
	OSStatus status = VTCompressionSessionEncodeFrame(enc->session, image, pts, duration, nullptr,
	                                                  nullptr, nullptr);
	CVPixelBufferRelease(image);
	if (status == noErr)
		/* the output callback has run for this frame when this returns */
		status = VTCompressionSessionCompleteFrames(enc->session, pts);
	if ((status != noErr) || enc->failed)
	{
		char *str1 = osstatus2str(status);
		char *str2 = osstatus2str(enc->status);
		WLog_Print(h264->log, WLOG_ERROR, "VideoToolbox H.264 encoder: frame failed: %s / %s", str1,
		           str2);
		free(str1);
		free(str2);
		/* the next frame starts a new session, with a keyframe */
		videotoolbox_encoder_destroy(enc);
		return -1;
	}

	if ((enc->length == 0) || (enc->length > UINT32_MAX))
		return 0;

	*ppDstData = enc->data;
	*pDstSize = (UINT32)enc->length;
	return 1;
}

static void videotoolbox_encoder_uninit(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VT_ENCODER *enc = h264->pSystemData;
	if (!enc)
		return;
	videotoolbox_encoder_destroy(enc);
	free(enc->data);
	free(enc);
	h264->pSystemData = nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL videotoolbox_encoder_init(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	H264_CONTEXT_VT_ENCODER *enc = calloc(1, sizeof(*enc));
	if (!enc)
		return FALSE;
	enc->log = h264->log;
	h264->pSystemData = enc;

	/* a session for the real size is made with the first frame; one small session now tells
	 * whether this system has an H.264 encoder at all, so that registration can go on to
	 * another backend if not */
	if (!videotoolbox_encoder_create(h264, enc, 64, 64, 30, 1000000))
	{
		videotoolbox_encoder_uninit(h264);
		return FALSE;
	}
	videotoolbox_encoder_destroy(enc);
	return TRUE;
}

static void videotoolbox_uninit(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	if (h264->Compressor)
	{
		videotoolbox_encoder_uninit(h264);
		return;
	}

	H264_CONTEXT_VIDEOTOOLBOX *sys = h264->pSystemData;
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
static BOOL videotoolbox_init(H264_CONTEXT *h264)
{
	WINPR_ASSERT(h264);

	if (h264->hevc || h264->hevc444)
		return FALSE;

	if (h264->Compressor)
		return videotoolbox_encoder_init(h264);

	H264_CONTEXT_VIDEOTOOLBOX *sys = calloc(1, sizeof(*sys));
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
	                                                      videotoolbox_decompress,
	                                                      videotoolbox_compress };
