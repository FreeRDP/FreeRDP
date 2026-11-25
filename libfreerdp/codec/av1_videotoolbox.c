/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV1 decoding with Apple VideoToolbox
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

/* Apple silicon from M3 and A17 Pro on decodes AV1 in hardware; VideoToolbox has no
 * software AV1 decoder, so older machines keep dav1d (av1_vt_new() returns nullptr).
 *
 * VideoToolbox takes AV1 the way ISO BMFF carries it: an av1C record (AV1 codec ISO media
 * file format binding, 2.3) with the sequence header in the format description, and each
 * sample a temporal unit without its temporal delimiter. The decoded YUV planes go back to
 * av1.c, which converts them to RGB exactly as it does dav1d's pictures. */

#include <freerdp/config.h>

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include "av1_videotoolbox.h"

/* kCMVideoCodecType_AV1, spelled out for SDKs older than macOS 13 */
#define AV1_VT_CODEC_TYPE ((CMVideoCodecType)0x61763031) /* 'av01' */

enum
{
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME_HEADER = 3,
	OBU_TILE_GROUP = 4,
	OBU_FRAME = 6,
	OBU_PADDING = 15
};

struct S_AV1_VT
{
	wLog* log;
	VTDecompressionSessionRef session;
	CMVideoFormatDescriptionRef format;
	BYTE* seq; /* the sequence header OBU the session was made for */
	size_t seqSize;
	BOOL i444;
	BOOL decoded; /* a frame of the session's stream came out */

	BYTE* sample; /* the temporal unit without temporal delimiters and padding */
	size_t sampleCapacity;

	OSStatus outStatus;         /* of the last output callback */
	CVImageBufferRef pending;   /* the output of the last decode */
	CVPixelBufferRef handedOut; /* locked, its planes are in the caller's AV1_VT_PICTURE */
	BYTE* chroma[2];            /* Cb and Cr of a bi-planar picture, apart */
	size_t chromaSize;
};

/* --- sequence header (AV1 specification 5.5) --- */

typedef struct
{
	const BYTE* p;
	size_t size;
	size_t bit;
	BOOL overrun;
} AV1_VT_BITS;

static UINT32 av1_vt_bits(AV1_VT_BITS* b, unsigned count)
{
	UINT32 v = 0;
	for (unsigned i = 0; i < count; i++)
	{
		if ((b->bit >> 3) >= b->size)
		{
			b->overrun = TRUE;
			return 0;
		}
		v = (v << 1) | ((b->p[b->bit >> 3] >> (7 - (b->bit & 7))) & 1);
		b->bit++;
	}
	return v;
}

static void av1_vt_uvlc(AV1_VT_BITS* b)
{
	unsigned leadingZeros = 0;
	while (!b->overrun && (av1_vt_bits(b, 1) == 0))
	{
		if (++leadingZeros >= 32)
		{
			b->overrun = TRUE;
			return;
		}
	}
	(void)av1_vt_bits(b, leadingZeros);
}

typedef struct
{
	UINT32 profile;
	UINT32 level0;
	UINT32 tier0;
	UINT32 highBitdepth;
	UINT32 twelveBit;
	UINT32 mono;
	UINT32 subsamplingX;
	UINT32 subsamplingY;
	UINT32 chromaSamplePosition;
	UINT32 fullRange;
	UINT32 width;
	UINT32 height;
} AV1_VT_SEQ;

static BOOL av1_vt_parse_sequence_header(const BYTE* p, size_t size, AV1_VT_SEQ* s)
{
	AV1_VT_BITS b = { .p = p, .size = size };
	memset(s, 0, sizeof(*s));

	s->profile = av1_vt_bits(&b, 3);
	(void)av1_vt_bits(&b, 1); /* still_picture */
	const UINT32 reduced = av1_vt_bits(&b, 1);
	if (reduced)
		s->level0 = av1_vt_bits(&b, 5);
	else
	{
		UINT32 decoderModelInfo = 0;
		UINT32 bufferDelayLength = 0;
		if (av1_vt_bits(&b, 1)) /* timing_info_present_flag */
		{
			(void)av1_vt_bits(&b, 32); /* num_units_in_display_tick */
			(void)av1_vt_bits(&b, 32); /* time_scale */
			if (av1_vt_bits(&b, 1))    /* equal_picture_interval */
				av1_vt_uvlc(&b);
			decoderModelInfo = av1_vt_bits(&b, 1);
			if (decoderModelInfo)
			{
				bufferDelayLength = av1_vt_bits(&b, 5) + 1;
				(void)av1_vt_bits(&b, 32); /* num_units_in_decoding_tick */
				(void)av1_vt_bits(&b, 10); /* buffer_removal_time_length, frame_presentation */
			}
		}
		const UINT32 initialDisplayDelay = av1_vt_bits(&b, 1);
		const UINT32 operatingPoints = av1_vt_bits(&b, 5) + 1;
		for (UINT32 i = 0; i < operatingPoints; i++)
		{
			(void)av1_vt_bits(&b, 12); /* operating_point_idc */
			const UINT32 level = av1_vt_bits(&b, 5);
			const UINT32 tier = (level > 7) ? av1_vt_bits(&b, 1) : 0;
			if (i == 0)
			{
				s->level0 = level;
				s->tier0 = tier;
			}
			if (decoderModelInfo && av1_vt_bits(&b, 1))
			{
				(void)av1_vt_bits(&b, bufferDelayLength); /* decoder_buffer_delay */
				(void)av1_vt_bits(&b, bufferDelayLength); /* encoder_buffer_delay */
				(void)av1_vt_bits(&b, 1);                 /* low_delay_mode_flag */
			}
			if (initialDisplayDelay && av1_vt_bits(&b, 1))
				(void)av1_vt_bits(&b, 4);
		}
	}

	const UINT32 widthBits = av1_vt_bits(&b, 4) + 1;
	const UINT32 heightBits = av1_vt_bits(&b, 4) + 1;
	s->width = av1_vt_bits(&b, widthBits) + 1;
	s->height = av1_vt_bits(&b, heightBits) + 1;
	if (!reduced && av1_vt_bits(&b, 1)) /* frame_id_numbers_present_flag */
		(void)av1_vt_bits(&b, 7);
	(void)av1_vt_bits(&b, 3); /* 128x128 superblock, filter intra, intra edge filter */
	if (!reduced)
	{
		(void)av1_vt_bits(&b, 4); /* interintra, masked compound, warped motion, dual filter */
		const UINT32 orderHint = av1_vt_bits(&b, 1);
		if (orderHint)
			(void)av1_vt_bits(&b, 2); /* jnt_comp, ref_frame_mvs */
		UINT32 forceScreenContentTools = 2;
		if (!av1_vt_bits(&b, 1)) /* seq_choose_screen_content_tools */
			forceScreenContentTools = av1_vt_bits(&b, 1);
		if (forceScreenContentTools > 0)
		{
			if (!av1_vt_bits(&b, 1)) /* seq_choose_integer_mv */
				(void)av1_vt_bits(&b, 1);
		}
		if (orderHint)
			(void)av1_vt_bits(&b, 3);
	}
	(void)av1_vt_bits(&b, 3); /* superres, cdef, restoration */

	/* color_config() */
	s->highBitdepth = av1_vt_bits(&b, 1);
	if ((s->profile == 2) && s->highBitdepth)
		s->twelveBit = av1_vt_bits(&b, 1);
	s->mono = (s->profile == 1) ? 0 : av1_vt_bits(&b, 1);
	UINT32 primaries = 2;
	UINT32 transfer = 2;
	UINT32 matrix = 2;
	if (av1_vt_bits(&b, 1)) /* color_description_present_flag */
	{
		primaries = av1_vt_bits(&b, 8);
		transfer = av1_vt_bits(&b, 8);
		matrix = av1_vt_bits(&b, 8);
	}
	if (s->mono)
	{
		s->fullRange = av1_vt_bits(&b, 1);
		s->subsamplingX = s->subsamplingY = 1;
	}
	else if ((primaries == 1) && (transfer == 13) && (matrix == 0)) /* sRGB */
		s->fullRange = 1;
	else
	{
		s->fullRange = av1_vt_bits(&b, 1);
		if (s->profile == 0)
			s->subsamplingX = s->subsamplingY = 1;
		else if (s->profile == 2)
		{
			if (s->twelveBit)
			{
				s->subsamplingX = av1_vt_bits(&b, 1);
				s->subsamplingY = s->subsamplingX ? av1_vt_bits(&b, 1) : 0;
			}
			else
				s->subsamplingX = 1;
		}
		if (s->subsamplingX && s->subsamplingY)
			s->chromaSamplePosition = av1_vt_bits(&b, 2);
	}
	return !b.overrun;
}

/* --- the session --- */

static void av1_vt_release_picture(AV1_VT* vt)
{
	if (vt->handedOut)
	{
		CVPixelBufferUnlockBaseAddress(vt->handedOut, kCVPixelBufferLock_ReadOnly);
		CVPixelBufferRelease(vt->handedOut);
		vt->handedOut = nullptr;
	}
	if (vt->pending)
	{
		CVPixelBufferRelease(vt->pending);
		vt->pending = nullptr;
	}
}

static void av1_vt_close(AV1_VT* vt)
{
	av1_vt_release_picture(vt);
	if (vt->session)
	{
		VTDecompressionSessionInvalidate(vt->session);
		CFRelease(vt->session);
		vt->session = nullptr;
	}
	if (vt->format)
	{
		CFRelease(vt->format);
		vt->format = nullptr;
	}
	free(vt->seq);
	vt->seq = nullptr;
	vt->seqSize = 0;
	vt->decoded = FALSE;
}

static void av1_vt_output(void* decompressionOutputRefCon,
                          WINPR_ATTR_UNUSED void* sourceFrameRefCon, OSStatus status,
                          WINPR_ATTR_UNUSED VTDecodeInfoFlags infoFlags,
                          CVImageBufferRef imageBuffer,
                          WINPR_ATTR_UNUSED CMTime presentationTimeStamp,
                          WINPR_ATTR_UNUSED CMTime presentationDuration)
{
	AV1_VT* vt = decompressionOutputRefCon;
	WINPR_ASSERT(vt);
	vt->outStatus = status;
	if ((status != noErr) || !imageBuffer)
		return;
	if (vt->pending)
		CVPixelBufferRelease(vt->pending);
	vt->pending = CVPixelBufferRetain(imageBuffer);
}

static CFDictionaryRef av1_vt_dictionary(const void* key, const void* value)
{
	const void* keys[] = { key };
	const void* values[] = { value };
	return CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
	                          &kCFTypeDictionaryValueCallBacks);
}

/* Return: TRUE, AV1_VT_UNSUPPORTED, AV1_VT_INVALID (nothing changed) or -1 */
static INT32 av1_vt_open(AV1_VT* vt, const BYTE* obu, size_t obuSize, const BYTE* payload,
                         size_t payloadSize)
{
	AV1_VT_SEQ s = WINPR_C_ARRAY_INIT;
	if (!av1_vt_parse_sequence_header(payload, payloadSize, &s))
	{
		WLog_Print(vt->log, WLOG_WARN, "AV1 sequence header too short");
		return AV1_VT_INVALID;
	}

	/* a new stream: the old session is done */
	av1_vt_close(vt);

	/* What av1.c converts: 8 bit 4:2:0 and 4:4:4 */
	const BOOL i420 = (s.subsamplingX == 1) && (s.subsamplingY == 1);
	const BOOL i444 = (s.subsamplingX == 0) && (s.subsamplingY == 0);
	if (s.highBitdepth || s.mono || (!i420 && !i444))
	{
		WLog_Print(vt->log, WLOG_DEBUG,
		           "AV1 profile %" PRIu32 " high bit depth %" PRIu32 " mono %" PRIu32
		           " subsampling %" PRIu32 "x%" PRIu32 ": not for VideoToolbox",
		           s.profile, s.highBitdepth, s.mono, s.subsamplingX, s.subsamplingY);
		return AV1_VT_UNSUPPORTED;
	}

	/* av1C: marker 1, version 1; seq_profile, seq_level_idx_0; seq_tier_0, high_bitdepth,
	 * twelve_bit, monochrome, chroma_subsampling_x/y, chroma_sample_position;
	 * no initial_presentation_delay; then the sequence header OBU. */
	BYTE* av1c = malloc(4 + obuSize);
	if (!av1c)
		return -1;
	av1c[0] = 0x81;
	av1c[1] = (BYTE)((s.profile << 5) | (s.level0 & 0x1f));
	av1c[2] = (BYTE)((s.tier0 << 7) | (s.highBitdepth << 6) | (s.twelveBit << 5) | (s.mono << 4) |
	                 (s.subsamplingX << 3) | (s.subsamplingY << 2) | (s.chromaSamplePosition & 3));
	av1c[3] = 0;
	memcpy(&av1c[4], obu, obuSize);

	INT32 rc = AV1_VT_UNSUPPORTED;
	CFDataRef record =
	    CFDataCreate(kCFAllocatorDefault, av1c, WINPR_ASSERTING_INT_CAST(CFIndex, 4 + obuSize));
	free(av1c);
	CFDictionaryRef atoms = record ? av1_vt_dictionary(CFSTR("av1C"), record) : nullptr;
	CFDictionaryRef extensions =
	    atoms ? av1_vt_dictionary(kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
	                              atoms)
	          : nullptr;

	/* The decoder's own bi-planar layout and the stream's range: no conversion in
	 * VideoToolbox, the samples come out as decoded. */
	OSType pixelFormat = 0;
	if (i444)
		pixelFormat = s.fullRange ? kCVPixelFormatType_444YpCbCr8BiPlanarFullRange
		                          : kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange;
	else
		pixelFormat = s.fullRange ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
		                          : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	SInt32 pf = WINPR_ASSERTING_INT_CAST(SInt32, pixelFormat);
	CFNumberRef pfNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pf);
	CFDictionaryRef attributes =
	    pfNumber ? av1_vt_dictionary(kCVPixelBufferPixelFormatTypeKey, pfNumber) : nullptr;
	CFDictionaryRef specification = av1_vt_dictionary(
	    kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder, kCFBooleanTrue);

	if (!extensions || !attributes || !specification)
	{
		rc = -1;
		goto out;
	}

	OSStatus status = CMVideoFormatDescriptionCreate(
	    kCFAllocatorDefault, AV1_VT_CODEC_TYPE, WINPR_ASSERTING_INT_CAST(int32_t, s.width),
	    WINPR_ASSERTING_INT_CAST(int32_t, s.height), extensions, &vt->format);
	if (status != noErr)
	{
		WLog_Print(vt->log, WLOG_WARN, "CMVideoFormatDescriptionCreate: %d", (int)status);
		goto out;
	}

	const VTDecompressionOutputCallbackRecord callback = { .decompressionOutputCallback =
		                                                       av1_vt_output,
		                                                   .decompressionOutputRefCon = vt };
	status = VTDecompressionSessionCreate(kCFAllocatorDefault, vt->format, specification,
	                                      attributes, &callback, &vt->session);
	if (status != noErr)
	{
		WLog_Print(vt->log, WLOG_INFO,
		           "VideoToolbox can't decode AV1 profile %" PRIu32 " %s %" PRIu32 "x%" PRIu32
		           " (%d): software decoder",
		           s.profile, i444 ? "4:4:4" : "4:2:0", s.width, s.height, (int)status);
		goto out;
	}
	(void)VTSessionSetProperty(vt->session, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);

	vt->seq = malloc(obuSize);
	if (!vt->seq)
	{
		rc = -1;
		goto out;
	}
	memcpy(vt->seq, obu, obuSize);
	vt->seqSize = obuSize;
	vt->i444 = i444;
	WLog_Print(vt->log, WLOG_DEBUG,
	           "VideoToolbox AV1 session: profile %" PRIu32 " level %" PRIu32 " %s %" PRIu32
	           "x%" PRIu32 " %s range",
	           s.profile, s.level0, i444 ? "4:4:4" : "4:2:0", s.width, s.height,
	           s.fullRange ? "full" : "video");
	rc = TRUE;

out:
	if (specification)
		CFRelease(specification);
	if (attributes)
		CFRelease(attributes);
	if (pfNumber)
		CFRelease(pfNumber);
	if (extensions)
		CFRelease(extensions);
	if (atoms)
		CFRelease(atoms);
	if (record)
		CFRelease(record);
	if (rc != TRUE)
		av1_vt_close(vt);
	return rc;
}

static BOOL av1_vt_leb128(const BYTE* p, size_t size, size_t* pos, UINT64* value)
{
	*value = 0;
	for (unsigned i = 0; i < 8; i++)
	{
		if (*pos >= size)
			return FALSE;
		const BYTE c = p[(*pos)++];
		*value |= (UINT64)(c & 0x7f) << (7 * i);
		if ((c & 0x80) == 0)
			return TRUE;
	}
	return FALSE;
}

static BOOL av1_vt_reserve(AV1_VT* vt, size_t size)
{
	if (size <= vt->sampleCapacity)
		return TRUE;
	BYTE* p = realloc(vt->sample, size);
	if (!p)
		return FALSE;
	vt->sample = p;
	vt->sampleCapacity = size;
	return TRUE;
}

/* Hand the decoded picture to the caller: lock it, split a bi-planar CbCr plane. */
static INT32 av1_vt_picture(AV1_VT* vt, AV1_VT_PICTURE* pic)
{
	CVPixelBufferRef buffer = vt->pending;
	vt->pending = nullptr;
	if (CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
	{
		CVPixelBufferRelease(buffer);
		return -1;
	}
	vt->handedOut = buffer;

	const size_t planes = CVPixelBufferGetPlaneCount(buffer);
	pic->width = WINPR_ASSERTING_INT_CAST(UINT32, CVPixelBufferGetWidth(buffer));
	pic->height = WINPR_ASSERTING_INT_CAST(UINT32, CVPixelBufferGetHeight(buffer));
	pic->i444 = vt->i444;
	pic->planes[0] = CVPixelBufferGetBaseAddressOfPlane(buffer, 0);
	pic->strides[0] =
	    WINPR_ASSERTING_INT_CAST(UINT32, CVPixelBufferGetBytesPerRowOfPlane(buffer, 0));

	if (planes == 3)
	{
		for (size_t i = 1; i < 3; i++)
		{
			pic->planes[i] = CVPixelBufferGetBaseAddressOfPlane(buffer, i);
			pic->strides[i] =
			    WINPR_ASSERTING_INT_CAST(UINT32, CVPixelBufferGetBytesPerRowOfPlane(buffer, i));
		}
		return TRUE;
	}
	if (planes != 2)
	{
		WLog_Print(vt->log, WLOG_ERROR, "VideoToolbox picture with %" PRIuz " planes", planes);
		return -1;
	}

	const size_t cw = CVPixelBufferGetWidthOfPlane(buffer, 1);
	const size_t ch = CVPixelBufferGetHeightOfPlane(buffer, 1);
	const size_t stride = CVPixelBufferGetBytesPerRowOfPlane(buffer, 1);
	const BYTE* cbcr = CVPixelBufferGetBaseAddressOfPlane(buffer, 1);
	if (!cbcr || (stride < 2 * cw))
		return -1;
	if (cw * ch > vt->chromaSize)
	{
		for (size_t i = 0; i < 2; i++)
		{
			winpr_aligned_free(vt->chroma[i]);
			vt->chroma[i] = winpr_aligned_malloc(cw * ch, 64);
		}
		vt->chromaSize = (vt->chroma[0] && vt->chroma[1]) ? cw * ch : 0;
		if (vt->chromaSize == 0)
			return -1;
	}
	for (size_t y = 0; y < ch; y++)
	{
		const BYTE* src = &cbcr[y * stride];
		BYTE* cb = &vt->chroma[0][y * cw];
		BYTE* cr = &vt->chroma[1][y * cw];
		for (size_t x = 0; x < cw; x++)
		{
			cb[x] = src[2 * x];
			cr[x] = src[2 * x + 1];
		}
	}
	pic->planes[1] = vt->chroma[0];
	pic->planes[2] = vt->chroma[1];
	pic->strides[1] = pic->strides[2] = WINPR_ASSERTING_INT_CAST(UINT32, cw);
	return TRUE;
}

INT32 av1_vt_decode(AV1_VT* vt, const BYTE* pSrcData, UINT32 SrcSize, AV1_VT_PICTURE* pic)
{
	WINPR_ASSERT(vt);
	WINPR_ASSERT(pSrcData || (SrcSize == 0));
	WINPR_ASSERT(pic);

	av1_vt_release_picture(vt);

	/* First check the whole temporal unit, so that bad input changes nothing, and see whether
	 * it carries a frame at all (a temporal unit may hold a sequence header only). */
	BOOL hasFrame = FALSE;
	for (size_t pos = 0; pos < SrcSize;)
	{
		const size_t start = pos;
		const BYTE header = pSrcData[pos++];
		const unsigned type = (header >> 3) & 0x0f;
		if (header & 0x04) /* obu_extension_flag */
			pos++;
		UINT64 size = 0;
		if (((header & 0x02) == 0) || !av1_vt_leb128(pSrcData, SrcSize, &pos, &size) ||
		    (size > SrcSize - pos))
		{
			WLog_Print(vt->log, WLOG_WARN, "AV1 OBU at %" PRIuz " of %" PRIu32 " is malformed",
			           start, SrcSize);
			return AV1_VT_INVALID;
		}
		pos += WINPR_ASSERTING_INT_CAST(size_t, size);
		if ((type == OBU_FRAME_HEADER) || (type == OBU_TILE_GROUP) || (type == OBU_FRAME))
			hasFrame = TRUE;
	}

	if (!av1_vt_reserve(vt, SrcSize))
		return -1;

	/* Keep every OBU but temporal delimiters and padding; a changed sequence header starts a
	 * new stream, so a new session. */
	size_t sampleSize = 0;
	size_t pos = 0;
	while (pos < SrcSize)
	{
		const size_t start = pos;
		const BYTE header = pSrcData[pos++];
		const unsigned type = (header >> 3) & 0x0f;
		if (header & 0x04) /* obu_extension_flag */
			pos++;
		UINT64 size = 0;
		if (!av1_vt_leb128(pSrcData, SrcSize, &pos, &size)) /* checked above */
			return AV1_VT_INVALID;
		const size_t payload = pos;
		pos += WINPR_ASSERTING_INT_CAST(size_t, size);

		if ((type == OBU_TEMPORAL_DELIMITER) || (type == OBU_PADDING))
			continue;
		if (type == OBU_SEQUENCE_HEADER)
		{
			const size_t obuSize = pos - start;
			if (!vt->session || (obuSize != vt->seqSize) ||
			    (memcmp(vt->seq, &pSrcData[start], obuSize) != 0))
			{
				const INT32 rc = av1_vt_open(vt, &pSrcData[start], obuSize, &pSrcData[payload],
				                             WINPR_ASSERTING_INT_CAST(size_t, size));
				if (rc != TRUE)
					return rc;
			}
		}
		memcpy(&vt->sample[sampleSize], &pSrcData[start], pos - start);
		sampleSize += pos - start;
	}

	if (!vt->session)
	{
		WLog_Print(vt->log, WLOG_WARN, "AV1 stream without a sequence header");
		return AV1_VT_INVALID;
	}
	if (!hasFrame)
		return FALSE;

	CMBlockBufferRef block = nullptr;
	CMSampleBufferRef sample = nullptr;
	OSStatus status =
	    CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, vt->sample, sampleSize,
	                                       kCFAllocatorNull, nullptr, 0, sampleSize, 0, &block);
	if (status == noErr)
	{
		const size_t sizes[] = { sampleSize };
		status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, vt->format, 1, 0, nullptr, 1,
		                                   sizes, &sample);
	}
	if (status != noErr)
	{
		if (block)
			CFRelease(block);
		WLog_Print(vt->log, WLOG_ERROR, "AV1 sample buffer: %d", (int)status);
		return -1;
	}

	/* Synchronous: the output callback runs before the call returns (and the wait makes sure). */
	vt->outStatus = noErr;
	VTDecodeInfoFlags info = 0;
	status = VTDecompressionSessionDecodeFrame(vt->session, sample, 0, nullptr, &info);
	if (status == noErr)
		status = VTDecompressionSessionWaitForAsynchronousFrames(vt->session);
	CFRelease(sample);
	CFRelease(block);
	if (status == noErr)
		status = vt->outStatus;

	if (status != noErr)
	{
		av1_vt_release_picture(vt);
		if (!vt->decoded)
		{
			/* The first frame of the stream: a software decoder can take over from here. */
			WLog_Print(vt->log, WLOG_INFO,
			           "VideoToolbox can't decode this AV1 stream (%d): software decoder",
			           (int)status);
			av1_vt_close(vt);
			return AV1_VT_UNSUPPORTED;
		}
		WLog_Print(vt->log, WLOG_WARN, "VideoToolbox AV1 decode: %d", (int)status);
		return -1;
	}

	if (!vt->pending)
		return FALSE;
	vt->decoded = TRUE;
	return av1_vt_picture(vt, pic);
}

void av1_vt_reset(AV1_VT* vt)
{
	if (vt)
		av1_vt_close(vt);
}

AV1_VT* av1_vt_new(wLog* log)
{
	if (__builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, *))
	{
		if (!VTIsHardwareDecodeSupported(AV1_VT_CODEC_TYPE))
			return nullptr;
	}
	else
		return nullptr;

	AV1_VT* vt = calloc(1, sizeof(AV1_VT));
	if (!vt)
		return nullptr;
	vt->log = log;
	return vt;
}

void av1_vt_free(AV1_VT* vt)
{
	if (!vt)
		return;
	av1_vt_close(vt);
	for (size_t i = 0; i < 2; i++)
		winpr_aligned_free(vt->chroma[i]);
	free(vt->sample);
	free(vt);
}
